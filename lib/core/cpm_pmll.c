/*
 * File: lib/core/cpm_pmll.c
 * Description: PMLL (Persistent Memory Lock Library) implementation
 * Author: Dr. Q Josef Kurk Edwards
 */

#include "cpm_pmll.h"
#include "cpm_promise.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

// --- PMEM Context Implementation ---

PMEMContextHandle* pmem_context_create(const char* pool_path, 
                                      size_t pool_size, 
                                      bool create_if_missing) {
    if (!pool_path) {
        return NULL;
    }
    
    PMEMContextHandle* ctx = malloc(sizeof(PMEMContextHandle));
    if (!ctx) {
        return NULL;
    }
    
    memset(ctx, 0, sizeof(PMEMContextHandle));
    ctx->pool_path = strdup(pool_path);
    ctx->pool_size = pool_size;
    
    // Check if pool exists
    struct stat st;
    bool pool_exists = (stat(pool_path, &st) == 0);
    
    if (!pool_exists && !create_if_missing) {
        free((void*)ctx->pool_path);
        free(ctx);
        return NULL;
    }
    
    // Create or open PMEM pool
    if (pool_exists) {
        ctx->pool = pmemobj_open(pool_path, "cpm_pool");
    } else {
        // Create new pool
        ctx->pool = pmemobj_create(pool_path, "cpm_pool", pool_size, 0666);
    }
    
    if (!ctx->pool) {
        free((void*)ctx->pool_path);
        free(ctx);
        return NULL;
    }
    
    // Get root object
    ctx->root_oid = pmemobj_root(ctx->pool, sizeof(uint64_t));
    if (OID_IS_NULL(ctx->root_oid)) {
        pmemobj_close(ctx->pool);
        free((void*)ctx->pool_path);
        free(ctx);
        return NULL;
    }
    
    // Map pool for direct access
    ctx->mapped_addr = pmemobj_direct(ctx->root_oid);
    
    return ctx;
}

PMEMContextHandle* pmem_context_open(const char* pool_path) {
    return pmem_context_create(pool_path, 0, false);
}

void pmem_context_close(PMEMContextHandle* ctx) {
    if (!ctx) {
        return;
    }
    
    if (ctx->pool) {
        pmemobj_close(ctx->pool);
    }
    
    free((void*)ctx->pool_path);
    free(ctx);
}

PMEMoid pmem_context_alloc(PMEMContextHandle* ctx, size_t size) {
    if (!ctx || !ctx->pool) {
        return OID_NULL;
    }
    
    PMEMoid oid;
    int ret = pmemobj_alloc(ctx->pool, &oid, size, 1, NULL, NULL);
    if (ret != 0) {
        return OID_NULL;
    }
    
    return oid;
}

void pmem_context_free(PMEMContextHandle* ctx, PMEMoid oid) {
    if (!ctx || !ctx->pool || OID_IS_NULL(oid)) {
        return;
    }
    
    pmemobj_free(&oid);
}

void pmem_context_persist(void* addr, size_t size) {
    if (addr && size > 0) {
        pmem_persist(addr, size);
    }
}

// --- PMLL Lock Implementation ---

CPM_Result pmll_lock_init(PMLL_Lock* lock,
                         const char* resource_id,
                         bool is_persistent,
                         PMEMContextHandle* pmem_ctx) {
    if (!lock || !resource_id) {
        return CPM_RESULT_ERROR_INVALID_ARGS;
    }
    
    // Initialize pthread mutex
    if (pthread_mutex_init(&lock->mutex, NULL) != 0) {
        return CPM_RESULT_ERROR_PMLL_INIT;
    }
    
    lock->resource_id = strdup(resource_id);
    lock->is_persistent = is_persistent;
    lock->lock_id = (uint64_t)time(NULL) + (uint64_t)getpid();
    
    if (is_persistent && pmem_ctx) {
        // Allocate persistent storage for lock state
        lock->pmem_oid = pmem_context_alloc(pmem_ctx, sizeof(uint64_t));
        if (OID_IS_NULL(lock->pmem_oid)) {
            pthread_mutex_destroy(&lock->mutex);
            free((void*)lock->resource_id);
            return CPM_RESULT_ERROR_PMDK_OPERATION;
        }
        
        // Initialize persistent lock state
        uint64_t* persistent_state = pmemobj_direct(lock->pmem_oid);
        *persistent_state = 0;  // Unlocked state
        pmem_context_persist(persistent_state, sizeof(uint64_t));
    } else {
        lock->pmem_oid = OID_NULL;
    }
    
    return CPM_RESULT_SUCCESS;
}

CPM_Result pmll_lock_acquire(PMLL_Lock* lock, uint32_t timeout_ms) {
    if (!lock) {
        return CPM_RESULT_ERROR_INVALID_ARGS;
    }
    
    int result;
    if (timeout_ms == 0) {
        // Blocking acquire
        result = pthread_mutex_lock(&lock->mutex);
    } else {
        // Timed acquire
        struct timespec abs_timeout;
        clock_gettime(CLOCK_REALTIME, &abs_timeout);
        abs_timeout.tv_sec += timeout_ms / 1000;
        abs_timeout.tv_nsec += (timeout_ms % 1000) * 1000000;
        if (abs_timeout.tv_nsec >= 1000000000) {
            abs_timeout.tv_sec += 1;
            abs_timeout.tv_nsec -= 1000000000;
        }
        
        result = pthread_mutex_timedlock(&lock->mutex, &abs_timeout);
    }
    
    if (result != 0) {
        return (result == ETIMEDOUT) ? CPM_RESULT_ERROR_UNKNOWN : CPM_RESULT_ERROR_PMLL_INIT;
    }
    
    // Update persistent state if needed
    if (lock->is_persistent && !OID_IS_NULL(lock->pmem_oid)) {
        uint64_t* persistent_state = pmemobj_direct(lock->pmem_oid);
        *persistent_state = lock->lock_id;
        pmem_context_persist(persistent_state, sizeof(uint64_t));
    }
    
    return CPM_RESULT_SUCCESS;
}

CPM_Result pmll_lock_release(PMLL_Lock* lock) {
    if (!lock) {
        return CPM_RESULT_ERROR_INVALID_ARGS;
    }
    
    // Update persistent state if needed
    if (lock->is_persistent && !OID_IS_NULL(lock->pmem_oid)) {
        uint64_t* persistent_state = pmemobj_direct(lock->pmem_oid);
        *persistent_state = 0;  // Unlocked state
        pmem_context_persist(persistent_state, sizeof(uint64_t));
    }
    
    int result = pthread_mutex_unlock(&lock->mutex);
    return (result == 0) ? CPM_RESULT_SUCCESS : CPM_RESULT_ERROR_PMLL_INIT;
}

void pmll_lock_destroy(PMLL_Lock* lock) {
    if (!lock) {
        return;
    }
    
    pthread_mutex_destroy(&lock->mutex);
    free((void*)lock->resource_id);
    
    // Note: Persistent memory is not freed here as it might be needed for recovery
    // It should be managed by the application or PMEM context cleanup
}

// --- PMLL Hardened Queue Implementation ---

PMLL_HardenedResourceQueue* pmll_queue_create(const char* resource_id,
                                              bool persistent_queue,
                                              PMEMContextHandle* pmem_ctx) {
    if (!resource_id) {
        return NULL;
    }
    
    PMLL_HardenedResourceQueue* hq = malloc(sizeof(PMLL_HardenedResourceQueue));
    if (!hq) {
        return NULL;
    }
    
    memset(hq, 0, sizeof(PMLL_HardenedResourceQueue));
    hq->resource_id = strdup(resource_id);
    hq->persistent_queue = persistent_queue;
    hq->pmem_context = pmem_ctx;
    hq->max_queue_depth = 1000;  // Default limit
    hq->timeout_ms = 30000;      // 30 second default timeout
    
    // Initialize queue lock
    CPM_Result lock_result = pmll_lock_init(&hq->queue_lock, resource_id, 
                                           persistent_queue, pmem_ctx);
    if (lock_result != CPM_RESULT_SUCCESS) {
        free((void*)hq->resource_id);
        free(hq);
        return NULL;
    }
    
    // Create initial resolved promise to start the chain
    hq->operation_queue_promise = promise_resolve_value(NULL);
    if (!hq->operation_queue_promise) {
        pmll_lock_destroy(&hq->queue_lock);
        free((void*)hq->resource_id);
        free(hq);
        return NULL;
    }
    
    return hq;
}

// Wrapper structure for hardened operations
typedef struct {
    on_fulfilled_callback user_op_fn;
    on_rejected_callback user_error_fn;
    void* user_op_data;
    PromiseDeferred* specific_deferred;
    PMLL_HardenedResourceQueue* queue;
} HardenedOpWrapperData;

static PromiseValue hardened_operation_wrapper(PromiseValue prev_result, void* user_data) {
    HardenedOpWrapperData* wrapper = (HardenedOpWrapperData*)user_data;
    PMLL_HardenedResourceQueue* hq = wrapper->queue;
    
    // Acquire queue statistics lock
    pmll_lock_acquire(&hq->queue_lock, hq->timeout_ms);
    hq->operations_pending++;
    pmll_lock_release(&hq->queue_lock);
    
    PromiseValue op_result = NULL;
    bool operation_succeeded = false;
    
    // Execute the user's operation function
    if (wrapper->user_op_fn) {
        try {
            op_result = wrapper->user_op_fn(prev_result, wrapper->user_op_data);
            operation_succeeded = true;
        } catch (...) {
            // C doesn't have exceptions, but this shows the concept
            operation_succeeded = false;
        }
    }
    
    // Update statistics
    pmll_lock_acquire(&hq->queue_lock, hq->timeout_ms);
    hq->operations_pending--;
    if (operation_succeeded) {
        hq->operations_completed++;
        // Resolve the specific deferred for this operation
        if (wrapper->specific_deferred) {
            promise_defer_resolve(wrapper->specific_deferred, op_result);
        }
    } else {
        hq->operations_failed++;
        // Reject the specific deferred for this operation
        if (wrapper->specific_deferred) {
            promise_defer_reject(wrapper->specific_deferred, "Operation failed");
        }
    }
    pmll_lock_release(&hq->queue_lock);
    
    // Free wrapper data
    free(wrapper);
    
    // Return result to continue the queue chain
    return op_result;
}

static PromiseValue hardened_error_wrapper(PromiseValue reason, void* user_data) {
    HardenedOpWrapperData* wrapper = (HardenedOpWrapperData*)user_data;
    PMLL_HardenedResourceQueue* hq = wrapper->queue;
    
    PromiseValue error_result = NULL;
    
    // Execute error handler if provided
    if (wrapper->user_error_fn) {
        error_result = wrapper->user_error_fn(reason, wrapper->user_op_data);
    }
    
    // Update statistics
    pmll_lock_acquire(&hq->queue_lock, hq->timeout_ms);
    hq->operations_pending--;
    hq->operations_failed++;
    pmll_lock_release(&hq->queue_lock);
    
    // Reject the specific deferred
    if (wrapper->specific_deferred) {
        promise_defer_reject(wrapper->specific_deferred, reason);
    }
    
    free(wrapper);
    return error_result;
}

Promise* pmll_execute_hardened_operation(PMLL_HardenedResourceQueue* hq,
                                        on_fulfilled_callback operation_fn,
                                        on_rejected_callback error_fn,
                                        void* user_data) {
    if (!hq || !operation_fn) {
        return promise_reject_reason("Invalid arguments to hardened operation");
    }
    
    // Check queue depth limit
    if (hq->operations_pending >= hq->max_queue_depth) {
        return promise_reject_reason("Queue depth limit exceeded");
    }
    
    // Acquire queue lock to modify the operation chain
    pmll_lock_acquire(&hq->queue_lock, hq->timeout_ms);
    
    // Create a deferred for this specific operation's outcome
    PromiseDeferred* operation_specific_deferred = promise_defer_create();
    if (!operation_specific_deferred) {
        pmll_lock_release(&hq->queue_lock);
        return promise_reject_reason("Failed to create operation deferred");
    }
    
    // Create wrapper data
    HardenedOpWrapperData* wrapper_data = malloc(sizeof(HardenedOpWrapperData));
    if (!wrapper_data) {
        promise_defer_free(operation_specific_deferred);
        pmll_lock_release(&hq->queue_lock);
        return promise_reject_reason("Memory allocation failed");
    }
    
    wrapper_data->user_op_fn = operation_fn;
    wrapper_data->user_error_fn = error_fn;
    wrapper_data->user_op_data = user_data;
    wrapper_data->specific_deferred = operation_specific_deferred;
    wrapper_data->queue = hq;
    
    // Chain this operation onto the existing queue
    Promise* new_tail = promise_then(hq->operation_queue_promise,
                                    hardened_operation_wrapper,
                                    hardened_error_wrapper,
                                    wrapper_data);
    
    if (!new_tail) {
        free(wrapper_data);
        promise_defer_free(operation_specific_deferred);
        pmll_lock_release(&hq->queue_lock);
        return promise_reject_reason("Failed to chain operation");
    }
    
    // Update queue tail
    hq->operation_queue_promise = new_tail;
    
    pmll_lock_release(&hq->queue_lock);
    
    // Return the promise for this specific operation
    return promise_defer_get_promise(operation_specific_deferred);
}

void pmll_queue_get_stats(PMLL_HardenedResourceQueue* hq,
                         uint64_t* completed,
                         uint64_t* failed,
                         uint64_t* pending) {
    if (!hq) {
        return;
    }
    
    pmll_lock_acquire(&hq->queue_lock, hq->timeout_ms);
    
    if (completed) *completed = hq->operations_completed;
    if (failed) *failed = hq->operations_failed;
    if (pending) *pending = hq->operations_pending;
    
    pmll_lock_release(&hq->queue_lock);
}

CPM_Result pmll_queue_wait_complete(PMLL_HardenedResourceQueue* hq, uint32_t timeout_ms) {
    if (!hq) {
        return CPM_RESULT_ERROR_INVALID_ARGS;
    }
    
    uint64_t start_time = (uint64_t)time(NULL) * 1000;
    uint64_t end_time = start_time + timeout_ms;
    
    while (true) {
        uint64_t pending;
        pmll_queue_get_stats(hq, NULL, NULL, &pending);
        
        if (pending == 0) {
            return CPM_RESULT_SUCCESS;
        }
        
        uint64_t current_time = (uint64_t)time(NULL) * 1000;
        if (timeout_ms > 0 && current_time >= end_time) {
            return CPM_RESULT_ERROR_UNKNOWN;  // Timeout
        }
        
        // Sleep for a short time before checking again
        usleep(1000);  // 1ms
    }
}

void pmll_queue_free(PMLL_HardenedResourceQueue* hq) {
    if (!hq) {
        return;
    }
    
    // Wait for pending operations to complete (with timeout)
    pmll_queue_wait_complete(hq, 10000);  // 10 second timeout
    
    // Clean up
    pmll_lock_destroy(&hq->queue_lock);
    promise_free(hq->operation_queue_promise);
    free((void*)hq->resource_id);
    free(hq);
}

// --- PMLL Transaction Support ---

PMLL_Transaction* pmll_transaction_begin(PMEMContextHandle* pmem_ctx) {
    if (!pmem_ctx || !pmem_ctx->pool) {
        return NULL;
    }
    
    PMLL_Transaction* tx = malloc(sizeof(PMLL_Transaction));
    if (!tx) {
        return NULL;
    }
    
    tx->pmem_ctx = pmem_ctx;
    tx->is_active = true;
    tx->tx_id = (uint64_t)time(NULL) + (uint64_t)getpid();
    
    // Allocate persistent storage for transaction state
    tx->tx_oid = pmem_context_alloc(pmem_ctx, sizeof(uint64_t));
    if (OID_IS_NULL(tx->tx_oid)) {
        free(tx);
        return NULL;
    }
    
    // Initialize transaction state
    uint64_t* tx_state = pmemobj_direct(tx->tx_oid);
    *tx_state = tx->tx_id;
    pmem_context_persist(tx_state, sizeof(uint64_t));
    
    return tx;
}

CPM_Result pmll_transaction_commit(PMLL_Transaction* tx) {
    if (!tx || !tx->is_active) {
        return CPM_RESULT_ERROR_INVALID_ARGS;
    }
    
    // Mark transaction as committed in persistent memory
    uint64_t* tx_state = pmemobj_direct(tx->tx_oid);
    *tx_state = 0;  // Committed state
    pmem_context_persist(tx_state, sizeof(uint64_t));
    
    tx->is_active = false;
    return CPM_RESULT_SUCCESS;
}

void pmll_transaction_abort(PMLL_Transaction* tx) {
    if (!tx || !tx->is_active) {
        return;
    }
    
    // Mark transaction as aborted
    uint64_t* tx_state = pmemobj_direct(tx->tx_oid);
    *tx_state = UINT64_MAX;  // Aborted state
    pmem_context_persist(tx_state, sizeof(uint64_t));
    
    tx->is_active = false;
}

void pmll_transaction_free(PMLL_Transaction* tx) {
    if (!tx) {
        return;
    }
    
    if (tx->is_active) {
        pmll_transaction_abort(tx);
    }
    
    // Free persistent transaction storage
    pmem_context_free(tx->pmem_ctx, tx->tx_oid);
    free(tx);
}

// --- PMLL Utilities ---

static bool pmll_initialized = false;

CPM_Result pmll_init(void) {
    if (pmll_initialized) {
        return CPM_RESULT_SUCCESS;
    }
    
    // Check if PMDK is available
    if (!pmll_is_pmdk_available()) {
        return CPM_RESULT_ERROR_PMDK_OPERATION;
    }
    
    pmll_initialized = true;
    return CPM_RESULT_SUCCESS;
}

void pmll_shutdown(void) {
    pmll_initialized = false;
}

bool pmll_is_pmdk_available(void) {
    // Try to check PMDK availability
    // This could be enhanced to check for specific PMDK features
    return access("/usr/include/libpmem.h", R_OK) == 0 ||
           access("/usr/local/include/libpmem.h", R_OK) == 0;
}

void pmll_get_version(int* major, int* minor, int* patch) {
    if (major) *major = 0;
    if (minor) *minor = 1;
    if (patch) *patch = 0;
}
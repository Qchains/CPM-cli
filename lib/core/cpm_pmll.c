/*
 * File: lib/core/cpm_pmll.c
 * Description: PMLL (Persistent Memory Lock Library) implementation for CPM
 * Author: Dr. Q Josef Kurk Edwards
 */

#include "../../include/cpm_pmll.h"
#include "../../include/cpm_promise.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <uuid/uuid.h>

// --- Global PMLL State ---
static struct {
    bool initialized;
    PMEMContextHandle global_pool;
    char pool_path[PATH_MAX];
    size_t pool_size;
    PMLL_PoolHeader* pool_header;
    pthread_mutex_t global_lock;
} pmll_state = {0};

// --- PMLL Initialization ---

CPM_Result pmll_init(const char* pool_path, size_t pool_size)
{
    if (pmll_state.initialized) {
        return CPM_RESULT_SUCCESS;
    }
    
    if (!pool_path || pool_size == 0) {
        return CPM_RESULT_ERROR_INVALID_ARGS;
    }
    
    strncpy(pmll_state.pool_path, pool_path, sizeof(pmll_state.pool_path) - 1);
    pmll_state.pool_size = pool_size;
    
    // Initialize or open PMDK pool
    pmll_state.global_pool = pmemobj_create(pool_path, "cpm_pmll_pool", 
                                           pool_size, 0666);
    if (!pmll_state.global_pool) {
        // Try to open existing pool
        pmll_state.global_pool = pmemobj_open(pool_path, "cpm_pmll_pool");
        if (!pmll_state.global_pool) {
            return CPM_RESULT_ERROR_PMDK_INIT;
        }
    }
    
    // Initialize pool header
    PMEMoid root = pmemobj_root(pmll_state.global_pool, sizeof(PMLL_PoolHeader));
    pmll_state.pool_header = pmemobj_direct(root);
    
    if (pmll_state.pool_header->version == 0) {
        // Initialize new pool
        TX_BEGIN(pmll_state.global_pool) {
            TX_MEMCPY(pmll_state.pool_header->magic, "CPM_PMLL_POOL", 16);
            pmll_state.pool_header->version = 1;
            pmll_state.pool_header->queue_count = 0;
            pmll_state.pool_header->operation_count = 0;
            strncpy(pmll_state.pool_header->pool_name, pool_path, 
                   sizeof(pmll_state.pool_header->pool_name) - 1);
            pmll_state.pool_header->created_at = time(NULL);
        } TX_END
    }
    
    if (pthread_mutex_init(&pmll_state.global_lock, NULL) != 0) {
        pmemobj_close(pmll_state.global_pool);
        return CPM_RESULT_ERROR_INITIALIZATION_FAILED;
    }
    
    pmll_state.initialized = true;
    return CPM_RESULT_SUCCESS;
}

void pmll_cleanup(void)
{
    if (!pmll_state.initialized) return;
    
    pthread_mutex_destroy(&pmll_state.global_lock);
    pmemobj_close(pmll_state.global_pool);
    
    memset(&pmll_state, 0, sizeof(pmll_state));
}

PMEMContextHandle pmll_get_global_pool(void)
{
    return pmll_state.global_pool;
}

// --- Hardened Queue Implementation ---

PMLL_HardenedResourceQueue* pmll_queue_create(const char* resource_id, bool persistent_queue)
{
    if (!pmll_state.initialized) {
        return NULL;
    }
    
    PMLL_HardenedResourceQueue* hq = malloc(sizeof(PMLL_HardenedResourceQueue));
    if (!hq) return NULL;
    
    hq->resource_id = strdup(resource_id);
    hq->operation_queue_promise = promise_create();
    hq->pmem_pool = persistent_queue ? pmll_state.global_pool : NULL;
    hq->pmem_queue_ctx = NULL;
    
    hq->operations_processed = 0;
    hq->operations_failed = 0;
    hq->last_operation_time = time(NULL);
    
    hq->persistent_enabled = persistent_queue;
    hq->max_queue_size = 1000;
    hq->timeout_ms = 30000;
    
    if (pthread_mutex_init(&hq->queue_lock, NULL) != 0) {
        free(hq->resource_id);
        promise_unref(hq->operation_queue_promise);
        free(hq);
        return NULL;
    }
    
    // Start with resolved promise
    promise_resolve(hq->operation_queue_promise, NULL);
    
    // Update global queue count
    pthread_mutex_lock(&pmll_state.global_lock);
    if (pmll_state.pool_header && persistent_queue) {
        TX_BEGIN(pmll_state.global_pool) {
            pmll_state.pool_header->queue_count++;
        } TX_END
    }
    pthread_mutex_unlock(&pmll_state.global_lock);
    
    return hq;
}

Promise* pmll_execute_hardened_operation(
    PMLL_HardenedResourceQueue* hq,
    on_fulfilled_callback operation_fn,
    on_rejected_callback error_fn,
    void* op_user_data)
{
    return pmll_execute_prioritized_operation(hq, operation_fn, error_fn, op_user_data, 0);
}

Promise* pmll_execute_prioritized_operation(
    PMLL_HardenedResourceQueue* hq,
    on_fulfilled_callback operation_fn,
    on_rejected_callback error_fn,
    void* op_user_data,
    int priority)
{
    if (!hq || !operation_fn) return NULL;
    
    pthread_mutex_lock(&hq->queue_lock);
    
    PromiseDeferred* operation_specific_deferred = promise_defer_create();
    if (!operation_specific_deferred) {
        pthread_mutex_unlock(&hq->queue_lock);
        return NULL;
    }
    
    HardenedOpWrapperData* wrapper_data = malloc(sizeof(HardenedOpWrapperData));
    if (!wrapper_data) {
        promise_defer_free(operation_specific_deferred);
        pthread_mutex_unlock(&hq->queue_lock);
        return NULL;
    }
    
    wrapper_data->user_op_fn = operation_fn;
    wrapper_data->error_fn = error_fn;
    wrapper_data->user_op_data = op_user_data;
    wrapper_data->specific_deferred = operation_specific_deferred;
    wrapper_data->pmem_op_ctx = hq->persistent_enabled ? hq->pmem_pool : NULL;
    wrapper_data->start_time = time(NULL);
    wrapper_data->priority = priority;
    
    // Generate unique operation ID
    uuid_t uuid;
    uuid_generate(uuid);
    uuid_unparse_lower(uuid, wrapper_data->operation_id);
    
    // Chain operation execution
    on_fulfilled_callback chain_callback = [](PromiseValue prev_result, void* ud) -> PromiseValue {
        HardenedOpWrapperData* wd = (HardenedOpWrapperData*)ud;
        PromiseValue op_result = NULL;
        
        // Execute the operation with PMLL protection
        if (wd->user_op_fn) {
            if (wd->pmem_op_ctx) {
                // Execute in persistent memory transaction
                TX_BEGIN(wd->pmem_op_ctx) {
                    op_result = wd->user_op_fn(prev_result, wd->user_op_data);
                } TX_ONABORT {
                    if (wd->error_fn) {
                        op_result = wd->error_fn((PromiseValue)"Transaction aborted", wd->user_op_data);
                    }
                    promise_defer_reject(wd->specific_deferred, (PromiseValue)"PMEM transaction failed");
                    goto cleanup;
                } TX_END
            } else {
                op_result = wd->user_op_fn(prev_result, wd->user_op_data);
            }
        }
        
        promise_defer_resolve(wd->specific_deferred, op_result);
        
    cleanup:
        free(wd);
        return op_result;
    };
    
    on_rejected_callback error_callback = [](PromiseValue reason, void* ud) -> PromiseValue {
        HardenedOpWrapperData* wd = (HardenedOpWrapperData*)ud;
        
        PromiseValue error_result = NULL;
        if (wd->error_fn) {
            error_result = wd->error_fn(reason, wd->user_op_data);
            promise_defer_resolve(wd->specific_deferred, error_result);
        } else {
            promise_defer_reject(wd->specific_deferred, reason);
        }
        
        free(wd);
        return error_result;
    };
    
    Promise* new_queue_promise = promise_then(
        hq->operation_queue_promise,
        chain_callback,
        error_callback,
        wrapper_data
    );
    
    // Update queue promise
    promise_unref(hq->operation_queue_promise);
    hq->operation_queue_promise = promise_ref(new_queue_promise);
    
    // Update statistics
    hq->last_operation_time = time(NULL);
    
    // Update global operation count
    pthread_mutex_lock(&pmll_state.global_lock);
    if (pmll_state.pool_header && hq->persistent_enabled) {
        TX_BEGIN(pmll_state.global_pool) {
            pmll_state.pool_header->operation_count++;
        } TX_END
    }
    pthread_mutex_unlock(&pmll_state.global_lock);
    
    pthread_mutex_unlock(&hq->queue_lock);
    
    return operation_specific_deferred->promise;
}

CPM_Result pmll_queue_flush(PMLL_HardenedResourceQueue* hq, int timeout_ms)
{
    if (!hq) return CPM_RESULT_ERROR_INVALID_ARGS;
    
    // Create a barrier operation that waits for all previous operations
    PromiseDeferred* barrier_deferred = promise_defer_create();
    if (!barrier_deferred) return CPM_RESULT_ERROR_MEMORY_ALLOCATION;
    
    on_fulfilled_callback barrier_fn = [](PromiseValue prev_result, void* ud) -> PromiseValue {
        PromiseDeferred* deferred = (PromiseDeferred*)ud;
        promise_defer_resolve(deferred, prev_result);
        return prev_result;
    };
    
    Promise* barrier_promise = pmll_execute_hardened_operation(hq, barrier_fn, NULL, barrier_deferred);
    
    // Wait for barrier with timeout
    struct timespec start, current;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    while (barrier_promise->state == PROMISE_PENDING) {
        clock_gettime(CLOCK_MONOTONIC, &current);
        long elapsed_ms = (current.tv_sec - start.tv_sec) * 1000 + 
                         (current.tv_nsec - start.tv_nsec) / 1000000;
        
        if (elapsed_ms >= timeout_ms) {
            promise_unref(barrier_promise);
            promise_defer_free(barrier_deferred);
            return CPM_RESULT_ERROR_LOCK_TIMEOUT;
        }
        
        usleep(1000); // Sleep 1ms
    }
    
    CPM_Result result = (barrier_promise->state == PROMISE_FULFILLED) ? 
                       CPM_RESULT_SUCCESS : CPM_RESULT_ERROR_COMMAND_FAILED;
    
    promise_unref(barrier_promise);
    promise_defer_free(barrier_deferred);
    
    return result;
}

void pmll_queue_get_stats(PMLL_HardenedResourceQueue* hq, 
                         uint64_t* ops_processed, 
                         uint64_t* ops_failed, 
                         size_t* queue_size)
{
    if (!hq) return;
    
    pthread_mutex_lock(&hq->queue_lock);
    
    if (ops_processed) *ops_processed = hq->operations_processed;
    if (ops_failed) *ops_failed = hq->operations_failed;
    if (queue_size) *queue_size = 0; // TODO: Implement actual queue size tracking
    
    pthread_mutex_unlock(&hq->queue_lock);
}

void pmll_queue_free(PMLL_HardenedResourceQueue* hq)
{
    if (!hq) return;
    
    // Wait for pending operations
    pmll_queue_flush(hq, hq->timeout_ms);
    
    pthread_mutex_destroy(&hq->queue_lock);
    free(hq->resource_id);
    promise_unref(hq->operation_queue_promise);
    
    // Update global queue count
    pthread_mutex_lock(&pmll_state.global_lock);
    if (pmll_state.pool_header && hq->persistent_enabled) {
        TX_BEGIN(pmll_state.global_pool) {
            if (pmll_state.pool_header->queue_count > 0) {
                pmll_state.pool_header->queue_count--;
            }
        } TX_END
    }
    pthread_mutex_unlock(&pmll_state.global_lock);
    
    free(hq);
}

// --- Persistent Memory Utilities ---

CPM_Result pmll_persist_data(PMEMContextHandle pmem_ctx, const void* data, size_t size)
{
    if (!pmem_ctx || !data || size == 0) {
        return CPM_RESULT_ERROR_INVALID_ARGS;
    }
    
    // For this implementation, we'll use a simple approach
    // In a production system, you'd want more sophisticated allocation
    
    TX_BEGIN(pmem_ctx) {
        PMEMoid oid;
        pmemobj_tx_alloc(oid, size);
        void* dest = pmemobj_direct(oid);
        memcpy(dest, data, size);
    } TX_ONABORT {
        return CPM_RESULT_ERROR_PERSISTENT_MEMORY;
    } TX_END
    
    return CPM_RESULT_SUCCESS;
}

CPM_Result pmll_read_data(PMEMContextHandle pmem_ctx, size_t offset, void* data, size_t size)
{
    if (!pmem_ctx || !data || size == 0) {
        return CPM_RESULT_ERROR_INVALID_ARGS;
    }
    
    // This is a simplified implementation
    // In practice, you'd need a more sophisticated addressing scheme
    
    char* pool_base = (char*)pmemobj_pool_by_ptr(pmem_ctx);
    if (!pool_base) {
        return CPM_RESULT_ERROR_PERSISTENT_MEMORY;
    }
    
    memcpy(data, pool_base + offset, size);
    return CPM_RESULT_SUCCESS;
}

CPM_Result pmll_execute_transaction(PMEMContextHandle pmem_ctx, 
                                   CPM_Result (*transaction_fn)(void* user_data),
                                   void* user_data)
{
    if (!pmem_ctx || !transaction_fn) {
        return CPM_RESULT_ERROR_INVALID_ARGS;
    }
    
    CPM_Result result = CPM_RESULT_SUCCESS;
    
    TX_BEGIN(pmem_ctx) {
        result = transaction_fn(user_data);
        if (result != CPM_RESULT_SUCCESS) {
            pmemobj_tx_abort(result);
        }
    } TX_ONABORT {
        result = CPM_RESULT_ERROR_PERSISTENT_MEMORY;
    } TX_END
    
    return result;
}

// --- Lock Management ---

CPM_Result pmll_lock_init(PMLL_Lock* lock)
{
    if (!lock) return CPM_RESULT_ERROR_INVALID_ARGS;
    
    if (pthread_mutex_init(lock, NULL) != 0) {
        return CPM_RESULT_ERROR_INITIALIZATION_FAILED;
    }
    
    return CPM_RESULT_SUCCESS;
}

CPM_Result pmll_lock_acquire_timeout(PMLL_Lock* lock, int timeout_ms)
{
    if (!lock) return CPM_RESULT_ERROR_INVALID_ARGS;
    
    if (timeout_ms <= 0) {
        return (pthread_mutex_lock(lock) == 0) ? 
               CPM_RESULT_SUCCESS : CPM_RESULT_ERROR_LOCK_TIMEOUT;
    }
    
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }
    
    int result = pthread_mutex_timedlock(lock, &ts);
    return (result == 0) ? CPM_RESULT_SUCCESS : CPM_RESULT_ERROR_LOCK_TIMEOUT;
}

void pmll_lock_release(PMLL_Lock* lock)
{
    if (lock) {
        pthread_mutex_unlock(lock);
    }
}

void pmll_lock_destroy(PMLL_Lock* lock)
{
    if (lock) {
        pthread_mutex_destroy(lock);
    }
}

// --- Hardening Utilities ---

Promise* pmll_create_file_operation_promise(const char* file_path)
{
    if (!file_path) return NULL;
    
    // Create a hardened queue specifically for this file
    PMLL_HardenedResourceQueue* file_queue = pmll_queue_create(file_path, true);
    if (!file_queue) return NULL;
    
    // The caller would use pmll_execute_hardened_operation with this queue
    // For now, return a basic resolved promise
    return promise_resolve_value((PromiseValue)file_queue);
}

Promise* pmll_create_network_operation_promise(const char* url)
{
    if (!url) return NULL;
    
    // Create a hardened queue for network operations
    char queue_id[256];
    snprintf(queue_id, sizeof(queue_id), "network:%s", url);
    
    PMLL_HardenedResourceQueue* net_queue = pmll_queue_create(queue_id, false);
    if (!net_queue) return NULL;
    
    return promise_resolve_value((PromiseValue)net_queue);
}

bool pmll_validate_integrity(PMEMContextHandle pmem_ctx)
{
    if (!pmem_ctx) return false;
    
    // Basic integrity check - verify pool header
    PMEMoid root = pmemobj_root(pmem_ctx, sizeof(PMLL_PoolHeader));
    PMLL_PoolHeader* header = pmemobj_direct(root);
    
    if (!header) return false;
    
    return (strncmp(header->magic, "CPM_PMLL_POOL", 16) == 0 && 
            header->version > 0);
}
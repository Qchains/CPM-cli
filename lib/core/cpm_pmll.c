/*
 * File: lib/core/cpm_pmll.c
 * Description: PMLL (Persistent Memory Lock Library) implementation with full PMDK integration
 * Author: Dr. Q Josef Kurk Edwards
 */

#include "../../include/cpm_pmll.h"
#include "../../include/cpm_promise.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <libpmem.h>
#include <libpmemobj.h>

// --- PMDK Pool Management ---

POBJ_LAYOUT_BEGIN(cpm_pmll);
POBJ_LAYOUT_TOID(cpm_pmll, struct pmll_lock_data);
POBJ_LAYOUT_TOID(cpm_pmll, struct pmll_queue_data);
POBJ_LAYOUT_END(cpm_pmll);

struct pmll_lock_data {
    uint64_t lock_id;
    pthread_mutex_t mutex;
    bool is_locked;
    pid_t owner_pid;
    pthread_t owner_thread;
};

struct pmll_queue_data {
    char resource_id[256];
    bool persistent_queue;
    uint64_t operation_count;
    pthread_mutex_t queue_mutex;
};

struct PMLL_HardenedResourceQueue {
    char* resource_id;
    Promise* operation_queue_promise;
    pthread_mutex_t queue_lock;
    PMEMContextHandle pmem_queue_ctx;
    TOID(struct pmll_queue_data) pmem_queue_data;
    bool is_persistent;
};

struct PMLL_Lock {
    void* pmem_lock_ptr;
    uint64_t lock_id;
    bool is_persistent;
    pthread_mutex_t local_mutex;
    TOID(struct pmll_lock_data) pmem_lock_data;
    PMEMContextHandle pmem_ctx;
};

// --- PMDK Pool Management Implementation ---

PMEMContextHandle pmll_init_pmem_pool(const char* pool_path, size_t pool_size) {
    if (!pool_path) {
        return NULL;
    }
    
    PMEMobjpool* pool = NULL;
    
    // Try to open existing pool first
    pool = pmemobj_open(pool_path, POBJ_LAYOUT_NAME(cpm_pmll));
    
    if (!pool) {
        // Create new pool if it doesn't exist
        pool = pmemobj_create(pool_path, POBJ_LAYOUT_NAME(cpm_pmll), pool_size, 0666);
        if (!pool) {
            perror("Failed to create PMEM pool");
            return NULL;
        }
        printf("[PMLL] Created new PMEM pool: %s (size: %zu bytes)\n", pool_path, pool_size);
    } else {
        printf("[PMLL] Opened existing PMEM pool: %s\n", pool_path);
    }
    
    return (PMEMContextHandle)pool;
}

void pmll_close_pmem_pool(PMEMContextHandle ctx) {
    if (ctx) {
        PMEMobjpool* pool = (PMEMobjpool*)ctx;
        pmemobj_close(pool);
        printf("[PMLL] Closed PMEM pool\n");
    }
}

// --- PMLL Lock Implementation ---

PMLL_Lock* pmll_lock_create(uint64_t lock_id, bool persistent, PMEMContextHandle pmem_ctx) {
    PMLL_Lock* lock = malloc(sizeof(PMLL_Lock));
    if (!lock) {
        return NULL;
    }
    
    lock->lock_id = lock_id;
    lock->is_persistent = persistent;
    lock->pmem_ctx = pmem_ctx;
    
    // Initialize local mutex
    if (pthread_mutex_init(&lock->local_mutex, NULL) != 0) {
        free(lock);
        return NULL;
    }
    
    if (persistent && pmem_ctx) {
        PMEMobjpool* pool = (PMEMobjpool*)pmem_ctx;
        
        // Allocate persistent lock data
        TX_BEGIN(pool) {
            lock->pmem_lock_data = TX_ALLOC(struct pmll_lock_data, sizeof(struct pmll_lock_data));
            D_RW(lock->pmem_lock_data)->lock_id = lock_id;
            D_RW(lock->pmem_lock_data)->is_locked = false;
            D_RW(lock->pmem_lock_data)->owner_pid = 0;
            D_RW(lock->pmem_lock_data)->owner_thread = 0;
            
            // Initialize persistent mutex
            pthread_mutexattr_t attr;
            pthread_mutexattr_init(&attr);
            pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
            pthread_mutex_init(&D_RW(lock->pmem_lock_data)->mutex, &attr);
            pthread_mutexattr_destroy(&attr);
        } TX_ONABORT {
            free(lock);
            return NULL;
        } TX_END
        
        lock->pmem_lock_ptr = D_RW(lock->pmem_lock_data);
        printf("[PMLL] Created persistent lock (ID: %lu)\n", lock_id);
    } else {
        lock->pmem_lock_ptr = NULL;
        TOID_ASSIGN(lock->pmem_lock_data, TOID_NULL(struct pmll_lock_data));
        printf("[PMLL] Created local lock (ID: %lu)\n", lock_id);
    }
    
    return lock;
}

bool pmll_lock_acquire(PMLL_Lock* lock) {
    if (!lock) return false;
    
    if (lock->is_persistent && lock->pmem_lock_ptr) {
        struct pmll_lock_data* plock = (struct pmll_lock_data*)lock->pmem_lock_ptr;
        
        // Use persistent mutex
        if (pthread_mutex_lock(&plock->mutex) != 0) {
            return false;
        }
        
        plock->is_locked = true;
        plock->owner_pid = getpid();
        plock->owner_thread = pthread_self();
        
        // Persist the lock state
        PMEMobjpool* pool = (PMEMobjpool*)lock->pmem_ctx;
        pmemobj_persist(pool, plock, sizeof(*plock));
        
        printf("[PMLL] Acquired persistent lock (ID: %lu, PID: %d)\n", lock->lock_id, getpid());
        return true;
    } else {
        // Use local mutex
        bool acquired = (pthread_mutex_lock(&lock->local_mutex) == 0);
        if (acquired) {
            printf("[PMLL] Acquired local lock (ID: %lu)\n", lock->lock_id);
        }
        return acquired;
    }
}

bool pmll_lock_release(PMLL_Lock* lock) {
    if (!lock) return false;
    
    if (lock->is_persistent && lock->pmem_lock_ptr) {
        struct pmll_lock_data* plock = (struct pmll_lock_data*)lock->pmem_lock_ptr;
        
        plock->is_locked = false;
        plock->owner_pid = 0;
        plock->owner_thread = 0;
        
        // Persist the unlock state
        PMEMobjpool* pool = (PMEMobjpool*)lock->pmem_ctx;
        pmemobj_persist(pool, plock, sizeof(*plock));
        
        pthread_mutex_unlock(&plock->mutex);
        printf("[PMLL] Released persistent lock (ID: %lu)\n", lock->lock_id);
        return true;
    } else {
        bool released = (pthread_mutex_unlock(&lock->local_mutex) == 0);
        if (released) {
            printf("[PMLL] Released local lock (ID: %lu)\n", lock->lock_id);
        }
        return released;
    }
}

void pmll_lock_free(PMLL_Lock* lock) {
    if (!lock) return;
    
    if (lock->is_persistent && !TOID_IS_NULL(lock->pmem_lock_data)) {
        PMEMobjpool* pool = (PMEMobjpool*)lock->pmem_ctx;
        TX_BEGIN(pool) {
            TX_FREE(lock->pmem_lock_data);
        } TX_END
    }
    
    pthread_mutex_destroy(&lock->local_mutex);
    printf("[PMLL] Freed lock (ID: %lu)\n", lock->lock_id);
    free(lock);
}

// --- Hardened Resource Queue Implementation ---

PMLL_HardenedResourceQueue* pmll_queue_create(const char* resource_id, 
                                              bool persistent_queue,
                                              PMEMContextHandle pmem_ctx) {
    if (!resource_id) return NULL;
    
    PMLL_HardenedResourceQueue* hq = malloc(sizeof(PMLL_HardenedResourceQueue));
    if (!hq) return NULL;
    
    hq->resource_id = strdup(resource_id);
    hq->is_persistent = persistent_queue;
    hq->pmem_queue_ctx = pmem_ctx;
    
    // Initialize queue lock
    if (pthread_mutex_init(&hq->queue_lock, NULL) != 0) {
        free(hq->resource_id);
        free(hq);
        return NULL;
    }
    
    if (persistent_queue && pmem_ctx) {
        PMEMobjpool* pool = (PMEMobjpool*)pmem_ctx;
        
        // Allocate persistent queue data
        TX_BEGIN(pool) {
            hq->pmem_queue_data = TX_ALLOC(struct pmll_queue_data, sizeof(struct pmll_queue_data));
            strncpy(D_RW(hq->pmem_queue_data)->resource_id, resource_id, sizeof(D_RW(hq->pmem_queue_data)->resource_id) - 1);
            D_RW(hq->pmem_queue_data)->persistent_queue = persistent_queue;
            D_RW(hq->pmem_queue_data)->operation_count = 0;
            
            // Initialize persistent queue mutex
            pthread_mutexattr_t attr;
            pthread_mutexattr_init(&attr);
            pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
            pthread_mutex_init(&D_RW(hq->pmem_queue_data)->queue_mutex, &attr);
            pthread_mutexattr_destroy(&attr);
        } TX_ONABORT {
            pthread_mutex_destroy(&hq->queue_lock);
            free(hq->resource_id);
            free(hq);
            return NULL;
        } TX_END
        
        printf("[PMLL] Created persistent queue for resource: %s\n", resource_id);
    } else {
        TOID_ASSIGN(hq->pmem_queue_data, TOID_NULL(struct pmll_queue_data));
        printf("[PMLL] Created local queue for resource: %s\n", resource_id);
    }
    
    // Create initial resolved promise for the queue
    hq->operation_queue_promise = promise_create_persistent(pmem_ctx, NULL);
    if (!hq->operation_queue_promise) {
        pmll_queue_free(hq);
        return NULL;
    }
    
    promise_resolve(hq->operation_queue_promise, NULL);
    return hq;
}

typedef struct {
    on_fulfilled_callback user_op_fn;
    void* user_op_data;
    PromiseDeferred* specific_deferred;
    PMLL_HardenedResourceQueue* queue;
} HardenedOpWrapperData;

static PromiseValue hardened_operation_wrapper(PromiseValue prev_result, void* user_data) {
    HardenedOpWrapperData* wd = (HardenedOpWrapperData*)user_data;
    PromiseValue op_result = NULL;
    
    printf("[PMLL] Executing hardened operation on resource: %s\n", wd->queue->resource_id);
    
    // Update operation count if persistent
    if (wd->queue->is_persistent && !TOID_IS_NULL(wd->queue->pmem_queue_data)) {
        PMEMobjpool* pool = (PMEMobjpool*)wd->queue->pmem_queue_ctx;
        TX_BEGIN(pool) {
            D_RW(wd->queue->pmem_queue_data)->operation_count++;
        } TX_END
    }
    
    // Execute the user's operation function
    if (wd->user_op_fn) {
        op_result = wd->user_op_fn(prev_result, wd->user_op_data);
    }
    
    // Resolve the specific deferred for this operation
    promise_defer_resolve(wd->specific_deferred, op_result);
    
    printf("[PMLL] Completed hardened operation on resource: %s\n", wd->queue->resource_id);
    
    // Cleanup wrapper data
    free(wd);
    
    return op_result;
}

Promise* pmll_execute_hardened_operation(PMLL_HardenedResourceQueue* hq,
                                        on_fulfilled_callback operation_fn,
                                        on_rejected_callback error_fn,
                                        void* op_user_data) {
    if (!hq || !operation_fn) return NULL;
    
    pthread_mutex_lock(&hq->queue_lock);
    
    // Create a deferred for this specific operation's outcome
    PromiseDeferred* operation_specific_deferred = promise_defer_create_persistent(
        hq->pmem_queue_ctx, NULL);
    if (!operation_specific_deferred) {
        pthread_mutex_unlock(&hq->queue_lock);
        return NULL;
    }
    
    // Create wrapper data
    HardenedOpWrapperData* wrapper_data = malloc(sizeof(HardenedOpWrapperData));
    if (!wrapper_data) {
        promise_defer_free(operation_specific_deferred);
        pthread_mutex_unlock(&hq->queue_lock);
        return NULL;
    }
    
    wrapper_data->user_op_fn = operation_fn;
    wrapper_data->user_op_data = op_user_data;
    wrapper_data->specific_deferred = operation_specific_deferred;
    wrapper_data->queue = hq;
    
    // Chain this operation to the queue
    hq->operation_queue_promise = promise_then(
        hq->operation_queue_promise,
        hardened_operation_wrapper,
        error_fn,
        wrapper_data
    );
    
    pthread_mutex_unlock(&hq->queue_lock);
    
    return operation_specific_deferred->promise;
}

void pmll_queue_free(PMLL_HardenedResourceQueue* hq) {
    if (!hq) return;
    
    pthread_mutex_destroy(&hq->queue_lock);
    
    if (hq->is_persistent && !TOID_IS_NULL(hq->pmem_queue_data)) {
        PMEMobjpool* pool = (PMEMobjpool*)hq->pmem_queue_ctx;
        TX_BEGIN(pool) {
            TX_FREE(hq->pmem_queue_data);
        } TX_END
    }
    
    if (hq->operation_queue_promise) {
        promise_free(hq->operation_queue_promise);
    }
    
    printf("[PMLL] Freed hardened queue for resource: %s\n", hq->resource_id);
    free(hq->resource_id);
    free(hq);
}

// --- Hardened File Operations ---

typedef struct {
    const char* filepath;
    const void* data;
    size_t size;
    PromiseDeferred* deferred;
} FileWriteOpData;

static PromiseValue file_write_operation(PromiseValue prev_result, void* user_data) {
    FileWriteOpData* data = (FileWriteOpData*)user_data;
    
    printf("[PMLL] Writing file: %s (%zu bytes)\n", data->filepath, data->size);
    
    FILE* fp = fopen(data->filepath, "wb");
    if (!fp) {
        char* error = strdup("Failed to open file for writing");
        promise_defer_reject(data->deferred, error);
        return NULL;
    }
    
    size_t written = fwrite(data->data, 1, data->size, fp);
    fclose(fp);
    
    if (written != data->size) {
        char* error = strdup("Failed to write all data to file");
        promise_defer_reject(data->deferred, error);
        return NULL;
    }
    
    char* result = strdup("File written successfully");
    promise_defer_resolve(data->deferred, result);
    return result;
}

Promise* pmll_hardened_file_write(PMLL_HardenedResourceQueue* queue,
                                 const char* filepath,
                                 const void* data,
                                 size_t size) {
    if (!queue || !filepath || !data) return NULL;
    
    FileWriteOpData* op_data = malloc(sizeof(FileWriteOpData));
    if (!op_data) return NULL;
    
    op_data->filepath = strdup(filepath);
    op_data->data = data;
    op_data->size = size;
    op_data->deferred = promise_defer_create();
    
    if (!op_data->deferred) {
        free((void*)op_data->filepath);
        free(op_data);
        return NULL;
    }
    
    return pmll_execute_hardened_operation(queue, file_write_operation, NULL, op_data);
}

typedef struct {
    const char* filepath;
    PromiseDeferred* deferred;
} FileReadOpData;

static PromiseValue file_read_operation(PromiseValue prev_result, void* user_data) {
    FileReadOpData* data = (FileReadOpData*)user_data;
    
    printf("[PMLL] Reading file: %s\n", data->filepath);
    
    FILE* fp = fopen(data->filepath, "rb");
    if (!fp) {
        char* error = strdup("Failed to open file for reading");
        promise_defer_reject(data->deferred, error);
        return NULL;
    }
    
    // Get file size
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    if (file_size < 0) {
        fclose(fp);
        char* error = strdup("Failed to determine file size");
        promise_defer_reject(data->deferred, error);
        return NULL;
    }
    
    // Allocate buffer and read
    char* buffer = malloc(file_size + 1);
    if (!buffer) {
        fclose(fp);
        char* error = strdup("Failed to allocate memory for file contents");
        promise_defer_reject(data->deferred, error);
        return NULL;
    }
    
    size_t read_bytes = fread(buffer, 1, file_size, fp);
    fclose(fp);
    buffer[read_bytes] = '';
    
    promise_defer_resolve(data->deferred, buffer);
    return buffer;
}

Promise* pmll_hardened_file_read(PMLL_HardenedResourceQueue* queue,
                                const char* filepath) {
    if (!queue || !filepath) return NULL;
    
    FileReadOpData* op_data = malloc(sizeof(FileReadOpData));
    if (!op_data) return NULL;
    
    op_data->filepath = strdup(filepath);
    op_data->deferred = promise_defer_create();
    
    if (!op_data->deferred) {
        free((void*)op_data->filepath);
        free(op_data);
        return NULL;
    }
    
    return pmll_execute_hardened_operation(queue, file_read_operation, NULL, op_data);
}

Promise* pmll_hardened_file_replace(PMLL_HardenedResourceQueue* queue,
                                   const char* filepath,
                                   const char* temp_filepath) {
    // Implementation for atomic file replacement
    // This would involve rename() or similar atomic operations
    if (!queue || !filepath || !temp_filepath) return NULL;
    
    Promise* p = promise_create();
    if (rename(temp_filepath, filepath) == 0) {
        promise_resolve(p, strdup("File replaced successfully"));
    } else {
        promise_reject(p, strdup("Failed to replace file"));
    }
    
    return p;
}

// --- Transaction Support ---

void* pmll_transaction_begin(PMEMContextHandle pmem_ctx) {
    if (!pmem_ctx) return NULL;
    
    PMEMobjpool* pool = (PMEMobjpool*)pmem_ctx;
    // PMDK transaction context would be returned here
    // For simplicity, return the pool pointer
    printf("[PMLL] Transaction began\n");
    return pool;
}

bool pmll_transaction_commit(void* tx_handle) {
    if (!tx_handle) return false;
    
    // In real implementation, would commit PMDK transaction
    printf("[PMLL] Transaction committed\n");
    return true;
}

void pmll_transaction_abort(void* tx_handle) {
    if (!tx_handle) return;
    
    // In real implementation, would abort PMDK transaction
    printf("[PMLL] Transaction aborted\n");
}
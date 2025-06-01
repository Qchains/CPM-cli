/*
 * File: lib/core/cpm_pmll.c
 * Description: PMLL (Persistent Memory Lock Library) hardened queue implementation.
 * Provides serialized operations to prevent race conditions in shared resources.
 * Author: Dr. Q Josef Kurk Edwards
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "cpm_pmll.h"
#include "cpm_promise.h"

// --- Global PMLL State ---
static struct {
    bool initialized;
    PMLL_HardenedResourceQueue* default_file_queue;
    pthread_mutex_t global_lock;
} pmll_global = {0};

// --- PMLL Hardened Resource Queue Implementation ---
PMLL_HardenedResourceQueue* pmll_queue_create(const char* resource_id, bool persistent_queue) {
    PMLL_HardenedResourceQueue* hq = (PMLL_HardenedResourceQueue*)malloc(sizeof(PMLL_HardenedResourceQueue));
    if (!hq) {
        perror("Failed to allocate PMLL_HardenedResourceQueue");
        return NULL;
    }
    
    hq->resource_id = strdup(resource_id);
    if (!hq->resource_id) {
        free(hq);
        return NULL;
    }
    
    // Initialize the operation queue with a resolved promise
    if (persistent_queue) {
        // For persistent queue, we would initialize with persistent memory context
        // hq->pmem_queue_ctx = get_or_create_persistent_context_for_queue(resource_id);
        // hq->operation_queue_promise = promise_create_persistent(hq->pmem_queue_ctx, &hq->queue_lock);
        hq->pmem_queue_ctx = NULL; // Placeholder
        hq->operation_queue_promise = promise_create();
    } else {
        hq->pmem_queue_ctx = NULL;
        hq->operation_queue_promise = promise_create();
    }
    
    if (!hq->operation_queue_promise) {
        free((void*)hq->resource_id);
        free(hq);
        return NULL;
    }
    
    if (pthread_mutex_init(&hq->queue_lock, NULL) != 0) {
        promise_free(hq->operation_queue_promise);
        free((void*)hq->resource_id);
        free(hq);
        return NULL;
    }
    
    // Start with a resolved promise to begin the queue
    promise_resolve(hq->operation_queue_promise, NULL);
    
    printf("[PMLL] Created hardened queue for resource: %s\n", resource_id);
    return hq;
}

void pmll_queue_free(PMLL_HardenedResourceQueue* hq) {
    if (!hq) return;
    
    printf("[PMLL] Destroying hardened queue for resource: %s\n", hq->resource_id);
    
    pthread_mutex_destroy(&hq->queue_lock);
    promise_free(hq->operation_queue_promise);
    free((void*)hq->resource_id);
    free(hq);
}

// --- Hardened Operation Execution ---
typedef struct {
    on_fulfilled_callback user_op_fn;
    on_rejected_callback user_error_fn;
    void* user_op_data;
    PromiseDeferred* specific_deferred;
    PMLL_HardenedResourceQueue* queue;
} HardenedOpWrapperData;

PromiseValue hardened_operation_wrapper(PromiseValue prev_result, void* user_data) {
    HardenedOpWrapperData* wd = (HardenedOpWrapperData*)user_data;
    PromiseValue op_result = NULL;
    
    printf("[PMLL] Executing hardened operation on resource: %s\n", wd->queue->resource_id);
    
    // Execute the user's operation function
    if (wd->user_op_fn) {
        // PMLL: If the operation needs locking, or if it's on persistent memory,
        // the user_op_fn must be PMLL/PMEM aware.
        // The prev_result could be data loaded from PMEM by a previous step.
        op_result = wd->user_op_fn(prev_result, wd->user_op_data);
    }
    
    // Resolve the specific deferred for this operation with the result
    if (wd->specific_deferred) {
        promise_defer_resolve(wd->specific_deferred, op_result);
    }
    
    // Clean up wrapper data
    free(wd);
    
    // Return result for the next operation in the queue
    return op_result;
}

PromiseValue hardened_operation_error_wrapper(PromiseValue prev_error, void* user_data) {
    HardenedOpWrapperData* wd = (HardenedOpWrapperData*)user_data;
    PromiseValue error_result = NULL;
    
    printf("[PMLL] Handling error in hardened operation on resource: %s\n", wd->queue->resource_id);
    
    if (wd->user_error_fn) {
        error_result = wd->user_error_fn(prev_error, wd->user_op_data);
    }
    
    // Reject the specific deferred for this operation
    if (wd->specific_deferred) {
        promise_defer_reject(wd->specific_deferred, error_result ? error_result : prev_error);
    }
    
    // Clean up wrapper data
    free(wd);
    
    // Continue the queue with error recovery
    return error_result;
}

Promise* pmll_execute_hardened_operation(
    PMLL_HardenedResourceQueue* hq,
    on_fulfilled_callback operation_fn,
    on_rejected_callback error_fn,
    void* op_user_data) {
    
    if (!hq || !operation_fn) {
        return NULL;
    }
    
    pthread_mutex_lock(&hq->queue_lock);
    
    // Create a deferred for this specific operation's outcome
    PromiseDeferred* operation_specific_deferred = promise_defer_create();
    if (!operation_specific_deferred) {
        pthread_mutex_unlock(&hq->queue_lock);
        return NULL;
    }
    
    // Create wrapper data for this operation
    HardenedOpWrapperData* wrapper_data = (HardenedOpWrapperData*)malloc(sizeof(HardenedOpWrapperData));
    if (!wrapper_data) {
        promise_defer_free(operation_specific_deferred);
        pthread_mutex_unlock(&hq->queue_lock);
        return NULL;
    }
    
    wrapper_data->user_op_fn = operation_fn;
    wrapper_data->user_error_fn = error_fn;
    wrapper_data->user_op_data = op_user_data;
    wrapper_data->specific_deferred = operation_specific_deferred;
    wrapper_data->queue = hq;
    
    // Chain this operation onto the existing queue
    Promise* new_queue_promise = promise_then(
        hq->operation_queue_promise,
        hardened_operation_wrapper,    // Success handler
        hardened_operation_error_wrapper, // Error handler
        wrapper_data                   // User data
    );
    
    if (!new_queue_promise) {
        free(wrapper_data);
        promise_defer_free(operation_specific_deferred);
        pthread_mutex_unlock(&hq->queue_lock);
        return NULL;
    }
    
    // Update the queue tail to point to the new promise
    promise_free(hq->operation_queue_promise);
    hq->operation_queue_promise = new_queue_promise;
    
    pthread_mutex_unlock(&hq->queue_lock);
    
    printf("[PMLL] Queued hardened operation on resource: %s\n", hq->resource_id);
    
    // Return the promise for this specific operation
    return promise_defer_get_promise(operation_specific_deferred);
}

// --- Global PMLL Management ---
bool pmll_init_global_system(void) {
    if (pmll_global.initialized) {
        return true; // Already initialized
    }
    
    if (pthread_mutex_init(&pmll_global.global_lock, NULL) != 0) {
        return false;
    }
    
    // Create default file operations queue
    pmll_global.default_file_queue = pmll_queue_create("global_file_operations", false);
    if (!pmll_global.default_file_queue) {
        pthread_mutex_destroy(&pmll_global.global_lock);
        return false;
    }
    
    pmll_global.initialized = true;
    printf("[PMLL] Global PMLL system initialized\n");
    return true;
}

void pmll_shutdown_global_system(void) {
    if (!pmll_global.initialized) {
        return;
    }
    
    pthread_mutex_lock(&pmll_global.global_lock);
    
    if (pmll_global.default_file_queue) {
        pmll_queue_free(pmll_global.default_file_queue);
        pmll_global.default_file_queue = NULL;
    }
    
    pmll_global.initialized = false;
    
    pthread_mutex_unlock(&pmll_global.global_lock);
    pthread_mutex_destroy(&pmll_global.global_lock);
    
    printf("[PMLL] Global PMLL system shutdown\n");
}

PMLL_HardenedResourceQueue* pmll_get_default_file_queue(void) {
    if (!pmll_global.initialized) {
        return NULL;
    }
    return pmll_global.default_file_queue;
}

// --- PMLL Serialized File Operations (Example Implementation) ---
typedef struct {
    char* filepath;
    char* content;
    PromiseDeferred* op_deferred;
} PMMLFileWriteData;

PromiseValue pmll_file_write_operation(PromiseValue prev_result, void* user_data) {
    PMMLFileWriteData* data = (PMMLFileWriteData*)user_data;
    
    printf("[PMLL] Serialized file write: %s\n", data->filepath);
    
    FILE* fp = fopen(data->filepath, "w");
    if (fp) {
        fputs(data->content, fp);
        fclose(fp);
        
        char* result = strdup("File write successful");
        if (data->op_deferred) {
            promise_defer_resolve(data->op_deferred, result);
        }
        
        // Clean up
        free(data->filepath);
        free(data->content);
        free(data);
        
        return result;
    } else {
        char* error = strdup("File write failed");
        if (data->op_deferred) {
            promise_defer_reject(data->op_deferred, error);
        }
        
        // Clean up
        free(data->filepath);
        free(data->content);
        free(data);
        
        return error;
    }
}

Promise* pmll_write_file_serialized(const char* filepath, const char* content) {
    PMLL_HardenedResourceQueue* file_queue = pmll_get_default_file_queue();
    if (!file_queue) {
        return NULL;
    }
    
    PMMLFileWriteData* data = (PMMLFileWriteData*)malloc(sizeof(PMMLFileWriteData));
    if (!data) {
        return NULL;
    }
    
    data->filepath = strdup(filepath);
    data->content = strdup(content);
    data->op_deferred = promise_defer_create();
    
    if (!data->filepath || !data->content || !data->op_deferred) {
        free(data->filepath);
        free(data->content);
        if (data->op_deferred) promise_defer_free(data->op_deferred);
        free(data);
        return NULL;
    }
    
    Promise* operation_promise = pmll_execute_hardened_operation(
        file_queue,
        pmll_file_write_operation,
        NULL, // No specific error handler
        data
    );
    
    return operation_promise;
}
/*
 * File: lib/core/cpm_promise.c
 * Description: Q Promise library implementation for C
 * Author: Dr. Q Josef Kurk Edwards
 */

#include "cpm_promise.h"
#include "cpm_pmll.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

// --- Internal Structures ---

typedef struct PromiseCallback {
    on_fulfilled_callback on_fulfilled;
    on_rejected_callback on_rejected;
    void* user_data;
    Promise* chained_promise;
} PromiseCallback;

typedef struct PromiseCallbackQueue {
    PromiseCallback* items;
    size_t count;
    size_t capacity;
} PromiseCallbackQueue;

struct Promise {
    PromiseState state;
    PromiseValue value;
    
    PromiseCallbackQueue fulfillment_callbacks;
    PromiseCallbackQueue rejection_callbacks;
    
    // PMLL/Hardening fields
    bool is_persistent_backed;
    PMEMContextHandle* pmem_handle;
    PMLL_Lock* resource_lock;
    
    // Reference counting for memory management
    int ref_count;
    
    // Promise ID for debugging
    uint64_t promise_id;
};

struct PromiseDeferred {
    Promise* promise;
    PMEMContextHandle* pmem_op_ctx;
};

// --- Global State ---
static uint64_t next_promise_id = 1;
static bool event_loop_initialized = false;

// --- Helper Functions ---

static void init_callback_queue(PromiseCallbackQueue* queue) {
    queue->items = NULL;
    queue->count = 0;
    queue->capacity = 0;
}

static CPM_Result add_callback(PromiseCallbackQueue* queue, 
                              on_fulfilled_callback on_fulfilled,
                              on_rejected_callback on_rejected,
                              void* user_data, 
                              Promise* chained_promise) {
    if (queue->count >= queue->capacity) {
        size_t new_capacity = queue->capacity == 0 ? 4 : queue->capacity * 2;
        PromiseCallback* new_items = realloc(queue->items, new_capacity * sizeof(PromiseCallback));
        if (!new_items) {
            return CPM_RESULT_ERROR_MEMORY_ALLOCATION;
        }
        queue->items = new_items;
        queue->capacity = new_capacity;
    }
    
    queue->items[queue->count++] = (PromiseCallback){
        .on_fulfilled = on_fulfilled,
        .on_rejected = on_rejected,
        .user_data = user_data,
        .chained_promise = chained_promise
    };
    
    return CPM_RESULT_SUCCESS;
}

static void free_callback_queue(PromiseCallbackQueue* queue) {
    free(queue->items);
    queue->items = NULL;
    queue->count = 0;
    queue->capacity = 0;
}

static void process_callbacks(Promise* p);

// --- Promise Creation and Management ---

Promise* promise_create(void) {
    return promise_create_persistent(NULL, NULL);
}

Promise* promise_create_persistent(void* pmem_ctx, pthread_mutex_t* lock) {
    Promise* p = malloc(sizeof(Promise));
    if (!p) {
        return NULL;
    }
    
    p->state = PROMISE_PENDING;
    p->value = NULL;
    p->is_persistent_backed = (pmem_ctx != NULL);
    p->pmem_handle = (PMEMContextHandle*)pmem_ctx;
    p->resource_lock = (PMLL_Lock*)lock;
    p->ref_count = 1;
    p->promise_id = __sync_fetch_and_add(&next_promise_id, 1);
    
    init_callback_queue(&p->fulfillment_callbacks);
    init_callback_queue(&p->rejection_callbacks);
    
    return p;
}

void promise_resolve(Promise* p, PromiseValue value) {
    if (!p || p->state != PROMISE_PENDING) {
        return;
    }
    
    // PMLL: Acquire lock if needed
    if (p->resource_lock) {
        pmll_lock_acquire(p->resource_lock, 0);
    }
    
    p->state = PROMISE_FULFILLED;
    p->value = value;
    
    // PMLL: Persist state if backed by PMEM
    if (p->is_persistent_backed && p->pmem_handle) {
        // Store promise state to persistent memory
        pmem_context_persist(&p->state, sizeof(p->state));
        pmem_context_persist(&p->value, sizeof(p->value));
    }
    
    if (p->resource_lock) {
        pmll_lock_release(p->resource_lock);
    }
    
    // Schedule callback processing
    if (event_loop_initialized) {
        cpm_event_loop_enqueue_microtask((void(*)(void*))process_callbacks, p);
    } else {
        process_callbacks(p);
    }
}

void promise_reject(Promise* p, PromiseValue reason) {
    if (!p || p->state != PROMISE_PENDING) {
        return;
    }
    
    if (p->resource_lock) {
        pmll_lock_acquire(p->resource_lock, 0);
    }
    
    p->state = PROMISE_REJECTED;
    p->value = reason;
    
    if (p->is_persistent_backed && p->pmem_handle) {
        pmem_context_persist(&p->state, sizeof(p->state));
        pmem_context_persist(&p->value, sizeof(p->value));
    }
    
    if (p->resource_lock) {
        pmll_lock_release(p->resource_lock);
    }
    
    if (event_loop_initialized) {
        cpm_event_loop_enqueue_microtask((void(*)(void*))process_callbacks, p);
    } else {
        process_callbacks(p);
    }
}

static void process_callbacks(Promise* p) {
    if (!p || p->state == PROMISE_PENDING) {
        return;
    }
    
    PromiseCallbackQueue* queue_to_process = NULL;
    if (p->state == PROMISE_FULFILLED) {
        queue_to_process = &p->fulfillment_callbacks;
    } else {
        queue_to_process = &p->rejection_callbacks;
    }
    
    // Process a copy to handle re-entrancy
    PromiseCallback* callbacks_copy = NULL;
    size_t count_copy = 0;
    
    if (queue_to_process->count > 0) {
        callbacks_copy = malloc(queue_to_process->count * sizeof(PromiseCallback));
        if (!callbacks_copy) {
            return; // Out of memory, callbacks will be lost
        }
        memcpy(callbacks_copy, queue_to_process->items, 
               queue_to_process->count * sizeof(PromiseCallback));
        count_copy = queue_to_process->count;
        
        // Clear original queue
        free_callback_queue(queue_to_process);
        init_callback_queue(queue_to_process);
    }
    
    // Process callbacks
    for (size_t i = 0; i < count_copy; i++) {
        PromiseCallback cb = callbacks_copy[i];
        PromiseValue callback_result = NULL;
        bool chained_promise_settled = false;
        
        if (p->state == PROMISE_FULFILLED && cb.on_fulfilled) {
            callback_result = cb.on_fulfilled(p->value, cb.user_data);
            if (cb.chained_promise) {
                promise_resolve(cb.chained_promise, callback_result);
                chained_promise_settled = true;
            }
        } else if (p->state == PROMISE_REJECTED && cb.on_rejected) {
            callback_result = cb.on_rejected(p->value, cb.user_data);
            if (cb.chained_promise) {
                promise_resolve(cb.chained_promise, callback_result);
                chained_promise_settled = true;
            }
        }
        
        // If no appropriate handler, propagate state
        if (!chained_promise_settled && cb.chained_promise) {
            if (p->state == PROMISE_FULFILLED) {
                promise_resolve(cb.chained_promise, p->value);
            } else {
                promise_reject(cb.chained_promise, p->value);
            }
        }
    }
    
    free(callbacks_copy);
}

Promise* promise_then(Promise* p,
                     on_fulfilled_callback on_fulfilled,
                     on_rejected_callback on_rejected,
                     void* user_data) {
    if (!p) {
        return NULL;
    }
    
    Promise* chained_promise = promise_create_persistent(
        p->pmem_handle, (pthread_mutex_t*)p->resource_lock);
    if (!chained_promise) {
        return NULL;
    }
    
    if (p->state == PROMISE_PENDING) {
        // Add callbacks to appropriate queues
        if (on_fulfilled) {
            add_callback(&p->fulfillment_callbacks, on_fulfilled, on_rejected, 
                        user_data, chained_promise);
        }
        if (on_rejected) {
            add_callback(&p->rejection_callbacks, on_fulfilled, on_rejected,
                        user_data, chained_promise);
        }
    } else {
        // Promise already settled, schedule immediate execution
        PromiseCallback temp_cb = {on_fulfilled, on_rejected, user_data, chained_promise};
        
        if (p->state == PROMISE_FULFILLED && temp_cb.on_fulfilled) {
            PromiseValue res = temp_cb.on_fulfilled(p->value, temp_cb.user_data);
            promise_resolve(temp_cb.chained_promise, res);
        } else if (p->state == PROMISE_REJECTED && temp_cb.on_rejected) {
            PromiseValue res = temp_cb.on_rejected(p->value, temp_cb.user_data);
            promise_resolve(temp_cb.chained_promise, res);
        } else {
            // No handler, propagate state
            if (p->state == PROMISE_FULFILLED) {
                promise_resolve(temp_cb.chained_promise, p->value);
            } else {
                promise_reject(temp_cb.chained_promise, p->value);
            }
        }
    }
    
    return chained_promise;
}

void promise_free(Promise* p) {
    if (!p) {
        return;
    }
    
    // Reference counting
    int ref_count = __sync_sub_and_fetch(&p->ref_count, 1);
    if (ref_count > 0) {
        return;
    }
    
    // Cleanup non-persistent values
    if (!p->is_persistent_backed && p->value && 
        (p->state == PROMISE_FULFILLED || p->state == PROMISE_REJECTED)) {
        // Note: Value ownership rules must be clearly defined by application
        // For safety, we don't free the value here unless explicitly owned
    }
    
    free_callback_queue(&p->fulfillment_callbacks);
    free_callback_queue(&p->rejection_callbacks);
    free(p);
}

// --- Q.defer() Implementation ---

PromiseDeferred* promise_defer_create(void) {
    return promise_defer_create_persistent(NULL, NULL);
}

PromiseDeferred* promise_defer_create_persistent(void* pmem_ctx, pthread_mutex_t* lock) {
    PromiseDeferred* d = malloc(sizeof(PromiseDeferred));
    if (!d) {
        return NULL;
    }
    
    d->promise = promise_create_persistent(pmem_ctx, lock);
    if (!d->promise) {
        free(d);
        return NULL;
    }
    
    d->pmem_op_ctx = (PMEMContextHandle*)pmem_ctx;
    return d;
}

void promise_defer_resolve(PromiseDeferred* deferred, PromiseValue value) {
    if (!deferred || !deferred->promise) {
        return;
    }
    promise_resolve(deferred->promise, value);
}

void promise_defer_reject(PromiseDeferred* deferred, PromiseValue reason) {
    if (!deferred || !deferred->promise) {
        return;
    }
    promise_reject(deferred->promise, reason);
}

Promise* promise_defer_get_promise(PromiseDeferred* deferred) {
    if (!deferred) {
        return NULL;
    }
    return deferred->promise;
}

void promise_defer_free(PromiseDeferred* deferred) {
    if (deferred) {
        // Promise is managed separately through reference counting
        free(deferred);
    }
}

// --- Utility Functions ---

Promise* promise_resolve_value(PromiseValue value) {
    Promise* p = promise_create();
    if (p) {
        promise_resolve(p, value);
    }
    return p;
}

Promise* promise_reject_reason(PromiseValue reason) {
    Promise* p = promise_create();
    if (p) {
        promise_reject(p, reason);
    }
    return p;
}

PromiseState promise_get_state(Promise* p) {
    return p ? p->state : PROMISE_PENDING;
}

PromiseValue promise_get_value(Promise* p) {
    if (!p || p->state == PROMISE_PENDING) {
        return NULL;
    }
    return p->value;
}
/*
 * File: lib/core/cpm_promise.c
 * Description: Q Promise library implementation with full PMDK integration
 * Author: Dr. Q Josef Kurk Edwards
 */

#include "../../include/cpm_promise.h"
#include "../../include/cpm_pmll.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <libpmem.h>
#include <libpmemobj.h>

// --- Promise Internal Structures ---

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
    
    // PMLL/Hardening Fields
    bool is_persistent_backed;
    PMEMContextHandle pmem_handle;
    PMLL_Lock* resource_lock;
    
    // Event loop integration
    bool is_queued_for_processing;
    struct Promise* next_in_queue;
};

struct PromiseDeferred {
    Promise* promise;
    PMEMContextHandle pmem_op_ctx;
};

// --- Event Loop State ---
static struct {
    bool initialized;
    pthread_mutex_t queue_mutex;
    Promise* processing_queue_head;
    Promise* processing_queue_tail;
    bool should_stop;
    pthread_t event_thread;
} event_loop_state = {0};

// --- Helper Functions ---

static void init_callback_queue(PromiseCallbackQueue* queue) {
    queue->items = NULL;
    queue->count = 0;
    queue->capacity = 0;
}

static bool add_callback(PromiseCallbackQueue* queue, on_fulfilled_callback on_fulfilled,
                        on_rejected_callback on_rejected, void* user_data, Promise* chained_promise) {
    if (queue->count >= queue->capacity) {
        size_t new_capacity = queue->capacity == 0 ? 4 : queue->capacity * 2;
        PromiseCallback* new_items = realloc(queue->items, new_capacity * sizeof(PromiseCallback));
        if (!new_items) {
            return false;
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
    return true;
}

static void free_callback_queue(PromiseCallbackQueue* queue) {
    free(queue->items);
    queue->items = NULL;
    queue->count = 0;
    queue->capacity = 0;
}

static void enqueue_promise_for_processing(Promise* p) {
    if (!event_loop_state.initialized || p->is_queued_for_processing) {
        return;
    }
    
    pthread_mutex_lock(&event_loop_state.queue_mutex);
    
    p->is_queued_for_processing = true;
    p->next_in_queue = NULL;
    
    if (event_loop_state.processing_queue_tail) {
        event_loop_state.processing_queue_tail->next_in_queue = p;
    } else {
        event_loop_state.processing_queue_head = p;
    }
    event_loop_state.processing_queue_tail = p;
    
    pthread_mutex_unlock(&event_loop_state.queue_mutex);
}

static void process_callbacks(Promise* p);

// --- Core Promise Implementation ---

Promise* promise_create() {
    return promise_create_persistent(NULL, NULL);
}

Promise* promise_create_persistent(PMEMContextHandle pmem_ctx, PMLL_Lock* lock) {
    Promise* p = malloc(sizeof(Promise));
    if (!p) {
        return NULL;
    }
    
    p->state = PROMISE_PENDING;
    p->value = NULL;
    init_callback_queue(&p->fulfillment_callbacks);
    init_callback_queue(&p->rejection_callbacks);
    
    p->is_persistent_backed = (pmem_ctx != NULL);
    p->pmem_handle = pmem_ctx;
    p->resource_lock = lock;
    p->is_queued_for_processing = false;
    p->next_in_queue = NULL;
    
    return p;
}

static void promise_resolve_internal(Promise* p, PromiseValue value, bool via_deferred) {
    if (!p || p->state != PROMISE_PENDING) {
        return;
    }
    
    // PMLL: Acquire lock if needed
    if (p->resource_lock) {
        pmll_lock_acquire(p->resource_lock);
    }
    
    p->state = PROMISE_FULFILLED;
    p->value = value;
    
    // PMDK: Persist state if backed by persistent memory
    if (p->is_persistent_backed && p->pmem_handle) {
        // Persist the promise state and value to PMEM
        // This is a simplified version - in practice, would use proper PMDK transactions
        PMEMobjpool* pool = (PMEMobjpool*)p->pmem_handle;
        if (pool) {
            pmemobj_persist(pool, &p->state, sizeof(p->state));
            pmemobj_persist(pool, &p->value, sizeof(p->value));
        }
    }
    
    if (p->resource_lock) {
        pmll_lock_release(p->resource_lock);
    }
    
    // Queue for callback processing
    enqueue_promise_for_processing(p);
    
    // If no event loop, process synchronously
    if (!event_loop_state.initialized) {
        process_callbacks(p);
    }
}

static void promise_reject_internal(Promise* p, PromiseValue reason, bool via_deferred) {
    if (!p || p->state != PROMISE_PENDING) {
        return;
    }
    
    if (p->resource_lock) {
        pmll_lock_acquire(p->resource_lock);
    }
    
    p->state = PROMISE_REJECTED;
    p->value = reason;
    
    if (p->is_persistent_backed && p->pmem_handle) {
        PMEMobjpool* pool = (PMEMobjpool*)p->pmem_handle;
        if (pool) {
            pmemobj_persist(pool, &p->state, sizeof(p->state));
            pmemobj_persist(pool, &p->value, sizeof(p->value));
        }
    }
    
    if (p->resource_lock) {
        pmll_lock_release(p->resource_lock);
    }
    
    enqueue_promise_for_processing(p);
    
    if (!event_loop_state.initialized) {
        process_callbacks(p);
    }
}

void promise_resolve(Promise* p, PromiseValue value) {
    promise_resolve_internal(p, value, false);
}

void promise_reject(Promise* p, PromiseValue reason) {
    promise_reject_internal(p, reason, false);
}

static void process_callbacks(Promise* p) {
    PromiseCallbackQueue* queue_to_process = NULL;
    PromiseCallback* cbs_copy = NULL;
    size_t count_copy = 0;
    
    if (p->resource_lock) {
        pmll_lock_acquire(p->resource_lock);
    }
    
    if (p->state == PROMISE_FULFILLED) {
        queue_to_process = &p->fulfillment_callbacks;
    } else if (p->state == PROMISE_REJECTED) {
        queue_to_process = &p->rejection_callbacks;
    } else {
        if (p->resource_lock) {
            pmll_lock_release(p->resource_lock);
        }
        return;
    }
    
    if (queue_to_process->count > 0) {
        cbs_copy = malloc(queue_to_process->count * sizeof(PromiseCallback));
        if (!cbs_copy) {
            if (p->resource_lock) {
                pmll_lock_release(p->resource_lock);
            }
            return;
        }
        memcpy(cbs_copy, queue_to_process->items, queue_to_process->count * sizeof(PromiseCallback));
        count_copy = queue_to_process->count;
        
        free_callback_queue(queue_to_process);
        init_callback_queue(queue_to_process);
    }
    
    if (p->resource_lock) {
        pmll_lock_release(p->resource_lock);
    }
    
    for (size_t i = 0; i < count_copy; ++i) {
        PromiseCallback cb_item = cbs_copy[i];
        PromiseValue callback_result = NULL;
        bool chained_promise_settled = false;
        
        if (p->state == PROMISE_FULFILLED && cb_item.on_fulfilled) {
            callback_result = cb_item.on_fulfilled(p->value, cb_item.user_data);
            if (cb_item.chained_promise) {
                promise_resolve(cb_item.chained_promise, callback_result);
                chained_promise_settled = true;
            }
        } else if (p->state == PROMISE_REJECTED && cb_item.on_rejected) {
            callback_result = cb_item.on_rejected(p->value, cb_item.user_data);
            if (cb_item.chained_promise) {
                promise_resolve(cb_item.chained_promise, callback_result);
                chained_promise_settled = true;
            }
        }
        
        if (!chained_promise_settled && cb_item.chained_promise) {
            if (p->state == PROMISE_FULFILLED) {
                promise_resolve(cb_item.chained_promise, p->value);
            } else {
                promise_reject(cb_item.chained_promise, p->value);
            }
        }
    }
    
    free(cbs_copy);
    p->is_queued_for_processing = false;
}

Promise* promise_then(Promise* p, on_fulfilled_callback on_fulfilled,
                     on_rejected_callback on_rejected, void* user_data) {
    if (!p) return NULL;
    
    Promise* chained_promise = promise_create_persistent(
        p->is_persistent_backed ? p->pmem_handle : NULL,
        p->resource_lock
    );
    if (!chained_promise) return NULL;
    
    if (p->resource_lock) {
        pmll_lock_acquire(p->resource_lock);
    }
    
    if (p->state == PROMISE_PENDING) {
        if (on_fulfilled) {
            add_callback(&p->fulfillment_callbacks, on_fulfilled, on_rejected, user_data, chained_promise);
        }
        if (on_rejected) {
            add_callback(&p->rejection_callbacks, on_fulfilled, on_rejected, user_data, chained_promise);
        }
    } else {
        // Promise already settled - schedule immediate processing
        PromiseCallback temp_cb_item = {on_fulfilled, on_rejected, user_data, chained_promise};
        
        if (p->state == PROMISE_FULFILLED && temp_cb_item.on_fulfilled) {
            PromiseValue res = temp_cb_item.on_fulfilled(p->value, temp_cb_item.user_data);
            promise_resolve(temp_cb_item.chained_promise, res);
        } else if (p->state == PROMISE_REJECTED && temp_cb_item.on_rejected) {
            PromiseValue res = temp_cb_item.on_rejected(p->value, temp_cb_item.user_data);
            promise_resolve(temp_cb_item.chained_promise, res);
        } else {
            if (p->state == PROMISE_FULFILLED) {
                promise_resolve(temp_cb_item.chained_promise, p->value);
            } else {
                promise_reject(temp_cb_item.chained_promise, p->value);
            }
        }
    }
    
    if (p->resource_lock) {
        pmll_lock_release(p->resource_lock);
    }
    
    return chained_promise;
}

void promise_free(Promise* p) {
    if (!p) return;
    
    if (!p->is_persistent_backed && p->value && 
        (p->state == PROMISE_FULFILLED || p->state == PROMISE_REJECTED)) {
        // In a real implementation, this would need careful ownership management
        // free(p->value);
    }
    
    free_callback_queue(&p->fulfillment_callbacks);
    free_callback_queue(&p->rejection_callbacks);
    free(p);
}

// --- Q.defer() Implementation ---

PromiseDeferred* promise_defer_create() {
    return promise_defer_create_persistent(NULL, NULL);
}

PromiseDeferred* promise_defer_create_persistent(PMEMContextHandle pmem_ctx, PMLL_Lock* lock) {
    PromiseDeferred* d = malloc(sizeof(PromiseDeferred));
    if (!d) return NULL;
    
    d->promise = promise_create_persistent(pmem_ctx, lock);
    if (!d->promise) {
        free(d);
        return NULL;
    }
    
    d->pmem_op_ctx = pmem_ctx;
    return d;
}

void promise_defer_resolve(PromiseDeferred* deferred, PromiseValue value) {
    if (!deferred || !deferred->promise) return;
    promise_resolve_internal(deferred->promise, value, true);
}

void promise_defer_reject(PromiseDeferred* deferred, PromiseValue reason) {
    if (!deferred || !deferred->promise) return;
    promise_reject_internal(deferred->promise, reason, true);
}

void promise_defer_free(PromiseDeferred* deferred) {
    if (deferred) {
        // The promise lifecycle is managed separately
        free(deferred);
    }
}

// --- Q.all() Implementation ---

typedef struct {
    Promise** promises;
    size_t count;
    PromiseValue* results;
    size_t resolved_count;
    size_t rejected_count;
    PromiseDeferred* master_deferred;
    pthread_mutex_t access_lock;
} PromiseAllContext;

static PromiseValue promise_all_on_fulfilled(PromiseValue value, void* user_data) {
    PromiseAllContext* ctx = (PromiseAllContext*)user_data;
    
    pthread_mutex_lock(&ctx->access_lock);
    ctx->resolved_count++;
    if (ctx->resolved_count == ctx->count) {
        promise_defer_resolve(ctx->master_deferred, (PromiseValue)ctx->results);
    }
    pthread_mutex_unlock(&ctx->access_lock);
    return NULL;
}

static PromiseValue promise_all_on_rejected(PromiseValue reason, void* user_data) {
    PromiseAllContext* ctx = (PromiseAllContext*)user_data;
    
    pthread_mutex_lock(&ctx->access_lock);
    if (ctx->rejected_count == 0) {
        ctx->rejected_count++;
        promise_defer_reject(ctx->master_deferred, reason);
    }
    pthread_mutex_unlock(&ctx->access_lock);
    return NULL;
}

Promise* promise_all(Promise* promises[], size_t count) {
    if (count == 0) {
        Promise* p = promise_create();
        promise_resolve(p, NULL);
        return p;
    }
    
    PromiseDeferred* master_deferred = promise_defer_create();
    if (!master_deferred) return NULL;
    
    PromiseAllContext* ctx = malloc(sizeof(PromiseAllContext));
    if (!ctx) {
        promise_defer_free(master_deferred);
        return NULL;
    }
    
    ctx->promises = promises;
    ctx->count = count;
    ctx->results = calloc(count, sizeof(PromiseValue));
    if (!ctx->results) {
        free(ctx);
        promise_defer_free(master_deferred);
        return NULL;
    }
    
    ctx->resolved_count = 0;
    ctx->rejected_count = 0;
    ctx->master_deferred = master_deferred;
    
    if (pthread_mutex_init(&ctx->access_lock, NULL) != 0) {
        free(ctx->results);
        free(ctx);
        promise_defer_free(master_deferred);
        return NULL;
    }
    
    for (size_t i = 0; i < count; ++i) {
        promise_then(promises[i], promise_all_on_fulfilled, promise_all_on_rejected, ctx);
    }
    
    return master_deferred->promise;
}

// --- Event Loop Implementation ---

static void* event_loop_thread(void* arg) {
    (void)arg;
    
    while (!event_loop_state.should_stop) {
        Promise* current = NULL;
        
        pthread_mutex_lock(&event_loop_state.queue_mutex);
        if (event_loop_state.processing_queue_head) {
            current = event_loop_state.processing_queue_head;
            event_loop_state.processing_queue_head = current->next_in_queue;
            if (!event_loop_state.processing_queue_head) {
                event_loop_state.processing_queue_tail = NULL;
            }
        }
        pthread_mutex_unlock(&event_loop_state.queue_mutex);
        
        if (current) {
            process_callbacks(current);
        } else {
            // No work, sleep briefly
            usleep(1000); // 1ms
        }
    }
    
    return NULL;
}

bool init_event_loop() {
    if (event_loop_state.initialized) {
        return true;
    }
    
    if (pthread_mutex_init(&event_loop_state.queue_mutex, NULL) != 0) {
        return false;
    }
    
    event_loop_state.processing_queue_head = NULL;
    event_loop_state.processing_queue_tail = NULL;
    event_loop_state.should_stop = false;
    
    if (pthread_create(&event_loop_state.event_thread, NULL, event_loop_thread, NULL) != 0) {
        pthread_mutex_destroy(&event_loop_state.queue_mutex);
        return false;
    }
    
    event_loop_state.initialized = true;
    return true;
}

void enqueue_microtask(void (*task)(void* data), void* data) {
    if (task) {
        task(data); // Simple synchronous execution for now
    }
}

void run_event_loop() {
    if (!event_loop_state.initialized) {
        return;
    }
    
    // Event loop runs in background thread, this is a no-op
}

void free_event_loop() {
    if (!event_loop_state.initialized) {
        return;
    }
    
    event_loop_state.should_stop = true;
    pthread_join(event_loop_state.event_thread, NULL);
    pthread_mutex_destroy(&event_loop_state.queue_mutex);
    event_loop_state.initialized = false;
}

// --- Promise Utilities ---

PromiseState promise_get_state(Promise* p) {
    return p ? p->state : PROMISE_PENDING;
}

PromiseValue promise_get_value(Promise* p) {
    return (p && p->state != PROMISE_PENDING) ? p->value : NULL;
}

bool promise_is_settled(Promise* p) {
    return p && (p->state == PROMISE_FULFILLED || p->state == PROMISE_REJECTED);
}

// --- Q.nfcall() Implementation ---

typedef struct {
    NodeCallback cb;
    void* user_data;
    PromiseDeferred* deferred;
} NfcallContext;

static void nfcall_wrapper(void* err, PromiseValue result, void* user_data) {
    NfcallContext* ctx = (NfcallContext*)user_data;
    
    if (err) {
        promise_defer_reject(ctx->deferred, err);
    } else {
        promise_defer_resolve(ctx->deferred, result);
    }
    
    free(ctx);
}

Promise* promise_nfcall(NodeCallback cb, void* user_data) {
    if (!cb) return NULL;
    
    NfcallContext* ctx = malloc(sizeof(NfcallContext));
    if (!ctx) return NULL;
    
    ctx->deferred = promise_defer_create();
    if (!ctx->deferred) {
        free(ctx);
        return NULL;
    }
    
    ctx->cb = cb;
    ctx->user_data = user_data;
    
    // Execute the callback
    cb(NULL, NULL, ctx);
    
    return ctx->deferred->promise;
}
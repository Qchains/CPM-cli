/*
 * File: lib/core/cpm_promise.c
 * Description: Q Promise library implementation for CPM.
 * Provides JavaScript-style promise-based asynchronous programming in C.
 * Author: Dr. Q Josef Kurk Edwards
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include "cpm_promise.h"

// --- Promise Callback Structure ---
typedef struct {
    on_fulfilled_callback on_fulfilled;
    on_rejected_callback on_rejected;
    void* user_data;
    Promise* chained_promise;
} PromiseCallback;

// --- Dynamic Callback Queue ---
typedef struct {
    PromiseCallback* items;
    size_t count;
    size_t capacity;
} PromiseCallbackQueue;

// --- Promise Structure ---
struct Promise {
    PromiseState state;
    PromiseValue value;
    
    PromiseCallbackQueue fulfillment_callbacks;
    PromiseCallbackQueue rejection_callbacks;
    
    // PMLL/Hardening Fields
    bool is_persistent_backed;
    PMEMContextHandle pmem_handle;
    PMLL_Lock* resource_lock;
};

// --- Deferred Structure ---
struct PromiseDeferred {
    Promise* promise;
};

// --- Event Loop Structure ---
typedef struct MicrotaskNode {
    void (*task)(void* data);
    void* data;
    struct MicrotaskNode* next;
} MicrotaskNode;

static struct {
    MicrotaskNode* head;
    MicrotaskNode* tail;
    pthread_mutex_t mutex;
    bool initialized;
} event_loop = {0};

// --- Helper Functions for Callback Queues ---
void init_callback_queue(PromiseCallbackQueue* queue) {
    queue->items = NULL;
    queue->count = 0;
    queue->capacity = 0;
}

void add_callback(PromiseCallbackQueue* queue, on_fulfilled_callback on_fulfilled, 
                 on_rejected_callback on_rejected, void* user_data, Promise* chained_promise) {
    if (queue->count >= queue->capacity) {
        queue->capacity = queue->capacity == 0 ? 4 : queue->capacity * 2;
        queue->items = (PromiseCallback*)realloc(queue->items, queue->capacity * sizeof(PromiseCallback));
        if (!queue->items) {
            perror("Failed to allocate memory for callback queue");
            return;
        }
    }
    queue->items[queue->count++] = (PromiseCallback){on_fulfilled, on_rejected, user_data, chained_promise};
}

void free_callback_queue(PromiseCallbackQueue* queue) {
    free(queue->items);
    queue->items = NULL;
    queue->count = 0;
    queue->capacity = 0;
}

// --- Forward Declarations ---
void process_callbacks(Promise* p);
void promise_resolve_internal(Promise* p, PromiseValue value, bool via_deferred);
void promise_reject_internal(Promise* p, PromiseValue reason, bool via_deferred);

// --- Promise Creation ---
Promise* promise_create_internal(bool is_persistent, PMEMContextHandle pmem_ctx, PMLL_Lock* lock) {
    Promise* p = (Promise*)malloc(sizeof(Promise));
    if (!p) {
        perror("Failed to allocate memory for Promise");
        return NULL;
    }
    p->state = PROMISE_PENDING;
    p->value = NULL;
    init_callback_queue(&p->fulfillment_callbacks);
    init_callback_queue(&p->rejection_callbacks);
    
    p->is_persistent_backed = is_persistent;
    p->pmem_handle = pmem_ctx;
    p->resource_lock = lock;
    return p;
}

Promise* promise_create(void) {
    return promise_create_internal(false, NULL, NULL);
}

Promise* promise_create_persistent(PMEMContextHandle pmem_ctx, PMLL_Lock* lock) {
    return promise_create_internal(true, pmem_ctx, lock);
}

void promise_free(Promise* p) {
    if (!p) return;
    
    free_callback_queue(&p->fulfillment_callbacks);
    free_callback_queue(&p->rejection_callbacks);
    free(p);
}

// --- Promise Settlement ---
void promise_resolve(Promise* p, PromiseValue value) {
    promise_resolve_internal(p, value, false);
}

void promise_reject(Promise* p, PromiseValue reason) {
    promise_reject_internal(p, reason, false);
}

void promise_resolve_internal(Promise* p, PromiseValue value, bool via_deferred) {
    if (!p || p->state != PROMISE_PENDING) {
        return;
    }
    
    if (p->resource_lock) pthread_mutex_lock(p->resource_lock);
    
    p->state = PROMISE_FULFILLED;
    p->value = value;
    
    if (p->is_persistent_backed) {
        printf("[PMLL] Conceptual: Persisted fulfillment value for promise tied to handle %p\n", p->pmem_handle);
    }
    
    if (p->resource_lock) pthread_mutex_unlock(p->resource_lock);
    
    process_callbacks(p);
}

void promise_reject_internal(Promise* p, PromiseValue reason, bool via_deferred) {
    if (!p || p->state != PROMISE_PENDING) {
        return;
    }
    
    if (p->resource_lock) pthread_mutex_lock(p->resource_lock);
    
    p->state = PROMISE_REJECTED;
    p->value = reason;
    
    if (p->is_persistent_backed) {
        printf("[PMLL] Conceptual: Persisted rejection reason for promise tied to handle %p\n", p->pmem_handle);
    }
    
    if (p->resource_lock) pthread_mutex_unlock(p->resource_lock);
    
    process_callbacks(p);
}

// --- Promise Callback Processing ---
void process_callbacks(Promise* p) {
    PromiseCallbackQueue* queue_to_process = NULL;
    PromiseCallback* cbs_copy = NULL;
    size_t count_copy = 0;
    
    if (p->resource_lock) pthread_mutex_lock(p->resource_lock);
    
    if (p->state == PROMISE_FULFILLED) {
        queue_to_process = &p->fulfillment_callbacks;
    } else if (p->state == PROMISE_REJECTED) {
        queue_to_process = &p->rejection_callbacks;
    } else {
        if (p->resource_lock) pthread_mutex_unlock(p->resource_lock);
        return;
    }
    
    if (queue_to_process->count > 0) {
        cbs_copy = (PromiseCallback*)malloc(queue_to_process->count * sizeof(PromiseCallback));
        if (!cbs_copy) {
            if (p->resource_lock) pthread_mutex_unlock(p->resource_lock);
            return;
        }
        memcpy(cbs_copy, queue_to_process->items, queue_to_process->count * sizeof(PromiseCallback));
        count_copy = queue_to_process->count;
        
        free_callback_queue(queue_to_process);
        init_callback_queue(queue_to_process);
    }
    
    if (p->resource_lock) pthread_mutex_unlock(p->resource_lock);
    
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
}

// --- Promise Chaining ---
Promise* promise_then(Promise* p, on_fulfilled_callback on_fulfilled, 
                     on_rejected_callback on_rejected, void* user_data) {
    if (!p) return NULL;
    
    Promise* chained_promise = promise_create_internal(
        p->is_persistent_backed,
        p->pmem_handle,
        p->resource_lock
    );
    if (!chained_promise) return NULL;
    
    if (p->resource_lock) pthread_mutex_lock(p->resource_lock);
    
    if (p->state == PROMISE_PENDING) {
        if (on_fulfilled) add_callback(&p->fulfillment_callbacks, on_fulfilled, on_rejected, user_data, chained_promise);
        if (on_rejected) add_callback(&p->rejection_callbacks, on_fulfilled, on_rejected, user_data, chained_promise);
    } else {
        // Promise already settled, schedule callback
        PromiseCallback temp_cb_item = {on_fulfilled, on_rejected, user_data, chained_promise};
        
        if (p->state == PROMISE_FULFILLED && temp_cb_item.on_fulfilled) {
            PromiseValue res = temp_cb_item.on_fulfilled(p->value, temp_cb_item.user_data);
            promise_resolve(temp_cb_item.chained_promise, res);
        } else if (p->state == PROMISE_REJECTED && temp_cb_item.on_rejected) {
            PromiseValue res = temp_cb_item.on_rejected(p->value, temp_cb_item.user_data);
            promise_resolve(temp_cb_item.chained_promise, res);
        } else {
            if (p->state == PROMISE_FULFILLED) promise_resolve(temp_cb_item.chained_promise, p->value);
            else promise_reject(temp_cb_item.chained_promise, p->value);
        }
    }
    
    if (p->resource_lock) pthread_mutex_unlock(p->resource_lock);
    return chained_promise;
}

// --- Q.defer() Implementation ---
PromiseDeferred* promise_defer_create(void) {
    PromiseDeferred* d = (PromiseDeferred*)malloc(sizeof(PromiseDeferred));
    if (!d) {
        perror("Failed to allocate memory for PromiseDeferred");
        return NULL;
    }
    d->promise = promise_create();
    if (!d->promise) {
        free(d);
        return NULL;
    }
    return d;
}

PromiseDeferred* promise_defer_create_persistent(PMEMContextHandle pmem_ctx, PMLL_Lock* lock) {
    PromiseDeferred* d = (PromiseDeferred*)malloc(sizeof(PromiseDeferred));
    if (!d) return NULL;
    d->promise = promise_create_persistent(pmem_ctx, lock);
    if (!d->promise) {
        free(d);
        return NULL;
    }
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
    PMLL_Lock* access_lock;
} PromiseAllContext;

PromiseValue promise_all_on_fulfilled(PromiseValue value, void* user_data) {
    PromiseAllContext* ctx = (PromiseAllContext*)user_data;
    
    pthread_mutex_lock(ctx->access_lock);
    ctx->resolved_count++;
    if (ctx->resolved_count == ctx->count) {
        promise_defer_resolve(ctx->master_deferred, (PromiseValue)ctx->results);
    }
    pthread_mutex_unlock(ctx->access_lock);
    return NULL;
}

PromiseValue promise_all_on_rejected(PromiseValue reason, void* user_data) {
    PromiseAllContext* ctx = (PromiseAllContext*)user_data;
    pthread_mutex_lock(ctx->access_lock);
    if (ctx->rejected_count == 0) {
        ctx->rejected_count++;
        promise_defer_reject(ctx->master_deferred, reason);
    }
    pthread_mutex_unlock(ctx->access_lock);
    return NULL;
}

Promise* promise_all(Promise* promises[], size_t count) {
    if (count == 0) {
        Promise* p = promise_create();
        promise_resolve(p, (PromiseValue)NULL);
        return p;
    }
    
    PromiseDeferred* master_deferred = promise_defer_create();
    if (!master_deferred) return NULL;
    
    PromiseAllContext* ctx = (PromiseAllContext*)malloc(sizeof(PromiseAllContext));
    if (!ctx) { 
        promise_defer_free(master_deferred); 
        return NULL; 
    }
    
    ctx->promises = promises;
    ctx->count = count;
    ctx->results = (PromiseValue*)calloc(count, sizeof(PromiseValue));
    if (!ctx->results) { 
        free(ctx); 
        promise_defer_free(master_deferred); 
        return NULL; 
    }
    ctx->resolved_count = 0;
    ctx->rejected_count = 0;
    ctx->master_deferred = master_deferred;
    
    ctx->access_lock = (PMLL_Lock*)malloc(sizeof(PMLL_Lock));
    if (!ctx->access_lock || pthread_mutex_init(ctx->access_lock, NULL) != 0) {
        free(ctx->results); 
        free(ctx); 
        promise_defer_free(master_deferred);
        if(ctx->access_lock) free(ctx->access_lock);
        return NULL;
    }
    
    for (size_t i = 0; i < count; ++i) {
        void* callback_context_for_promise_i = ctx;
        promise_then(promises[i], promise_all_on_fulfilled, promise_all_on_rejected, callback_context_for_promise_i);
    }
    
    return master_deferred->promise;
}

// --- Event Loop Implementation ---
void init_event_loop(void) {
    if (!event_loop.initialized) {
        event_loop.head = NULL;
        event_loop.tail = NULL;
        pthread_mutex_init(&event_loop.mutex, NULL);
        event_loop.initialized = true;
    }
}

void enqueue_microtask(void (*task)(void* data), void* data) {
    if (!event_loop.initialized) return;
    
    MicrotaskNode* node = (MicrotaskNode*)malloc(sizeof(MicrotaskNode));
    if (!node) return;
    
    node->task = task;
    node->data = data;
    node->next = NULL;
    
    pthread_mutex_lock(&event_loop.mutex);
    if (event_loop.tail) {
        event_loop.tail->next = node;
    } else {
        event_loop.head = node;
    }
    event_loop.tail = node;
    pthread_mutex_unlock(&event_loop.mutex);
}

void run_event_loop(void) {
    if (!event_loop.initialized) return;
    
    while (true) {
        pthread_mutex_lock(&event_loop.mutex);
        MicrotaskNode* node = event_loop.head;
        if (node) {
            event_loop.head = node->next;
            if (!event_loop.head) {
                event_loop.tail = NULL;
            }
        }
        pthread_mutex_unlock(&event_loop.mutex);
        
        if (!node) break;
        
        node->task(node->data);
        free(node);
    }
}

void free_event_loop(void) {
    if (!event_loop.initialized) return;
    
    pthread_mutex_lock(&event_loop.mutex);
    MicrotaskNode* current = event_loop.head;
    while (current) {
        MicrotaskNode* next = current->next;
        free(current);
        current = next;
    }
    event_loop.head = NULL;
    event_loop.tail = NULL;
    pthread_mutex_unlock(&event_loop.mutex);
    
    pthread_mutex_destroy(&event_loop.mutex);
    event_loop.initialized = false;
}

// --- Promise State Access ---
PromiseState promise_get_state(const Promise* p) {
    if (!p) return PROMISE_PENDING;
    return p->state;
}

PromiseValue promise_get_value(const Promise* p) {
    if (!p) return NULL;
    return p->value;
}
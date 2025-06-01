/*
 * File: lib/core/cpm_promise.c
 * Description: Q Promise library implementation for CPM
 * Author: Dr. Q Josef Kurk Edwards
 */

#include "../../include/cpm_promise.h"
#include "../../include/cpm_pmll.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/time.h>

// --- Global Event Loop State ---
static struct {
    bool initialized;
    bool running;
    pthread_t event_thread;
    pthread_mutex_t task_queue_mutex;
    pthread_cond_t task_queue_cond;
    
    struct task_node {
        void (*task)(void* data);
        void* data;
        struct task_node* next;
    } *task_queue_head, *task_queue_tail;
    
} event_loop = {0};

// --- Callback Queue Management ---

static void init_callback_queue(PromiseCallbackQueue* queue)
{
    queue->items = NULL;
    queue->count = 0;
    queue->capacity = 0;
    pthread_mutex_init(&queue->mutex, NULL);
}

static void add_callback(PromiseCallbackQueue* queue, on_fulfilled_callback on_fulfilled,
                        on_rejected_callback on_rejected, void* user_data, Promise* chained_promise)
{
    pthread_mutex_lock(&queue->mutex);
    
    if (queue->count >= queue->capacity) {
        queue->capacity = queue->capacity == 0 ? 4 : queue->capacity * 2;
        queue->items = realloc(queue->items, queue->capacity * sizeof(PromiseCallback));
        if (!queue->items) {
            pthread_mutex_unlock(&queue->mutex);
            return;
        }
    }
    
    queue->items[queue->count++] = (PromiseCallback){
        .on_fulfilled = on_fulfilled,
        .on_rejected = on_rejected,
        .user_data = user_data,
        .chained_promise = chained_promise
    };
    
    pthread_mutex_unlock(&queue->mutex);
}

static void free_callback_queue(PromiseCallbackQueue* queue)
{
    pthread_mutex_lock(&queue->mutex);
    free(queue->items);
    queue->items = NULL;
    queue->count = 0;
    queue->capacity = 0;
    pthread_mutex_unlock(&queue->mutex);
    pthread_mutex_destroy(&queue->mutex);
}

// --- Promise Reference Counting ---

Promise* promise_ref(Promise* p)
{
    if (!p) return NULL;
    
    pthread_mutex_lock(&p->ref_mutex);
    p->ref_count++;
    pthread_mutex_unlock(&p->ref_mutex);
    
    return p;
}

void promise_unref(Promise* p)
{
    if (!p) return;
    
    pthread_mutex_lock(&p->ref_mutex);
    p->ref_count--;
    bool should_free = (p->ref_count <= 0);
    pthread_mutex_unlock(&p->ref_mutex);
    
    if (should_free) {
        promise_free(p);
    }
}

// --- Core Promise API Implementation ---

Promise* promise_create(void)
{
    return promise_create_persistent(NULL, NULL);
}

Promise* promise_create_persistent(void* pmem_ctx, pthread_mutex_t* lock)
{
    Promise* p = malloc(sizeof(Promise));
    if (!p) return NULL;
    
    p->state = PROMISE_PENDING;
    p->value = NULL;
    
    init_callback_queue(&p->fulfillment_callbacks);
    init_callback_queue(&p->rejection_callbacks);
    
    p->is_persistent_backed = (pmem_ctx != NULL);
    p->pmem_handle = pmem_ctx;
    p->resource_lock = lock;
    
    p->ref_count = 1;
    pthread_mutex_init(&p->ref_mutex, NULL);
    
    p->creation_site = __FILE__;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    p->creation_time = tv.tv_sec * 1000000ULL + tv.tv_usec;
    
    return p;
}

// Forward declaration
static void process_callbacks(Promise* p);

void promise_resolve(Promise* p, PromiseValue value)
{
    if (!p || p->state != PROMISE_PENDING) return;
    
    if (p->resource_lock) {
        pthread_mutex_lock(p->resource_lock);
    }
    
    p->state = PROMISE_FULFILLED;
    p->value = value;
    
    if (p->is_persistent_backed && p->pmem_handle) {
        pmll_persist_data(p->pmem_handle, &value, sizeof(value));
    }
    
    if (p->resource_lock) {
        pthread_mutex_unlock(p->resource_lock);
    }
    
    process_callbacks(p);
}

void promise_reject(Promise* p, PromiseValue reason)
{
    if (!p || p->state != PROMISE_PENDING) return;
    
    if (p->resource_lock) {
        pthread_mutex_lock(p->resource_lock);
    }
    
    p->state = PROMISE_REJECTED;
    p->value = reason;
    
    if (p->is_persistent_backed && p->pmem_handle) {
        pmll_persist_data(p->pmem_handle, &reason, sizeof(reason));
    }
    
    if (p->resource_lock) {
        pthread_mutex_unlock(p->resource_lock);
    }
    
    process_callbacks(p);
}

static void process_callbacks(Promise* p)
{
    PromiseCallbackQueue* queue_to_process = NULL;
    PromiseCallback* cbs_copy = NULL;
    size_t count_copy = 0;
    
    if (p->state == PROMISE_FULFILLED) {
        queue_to_process = &p->fulfillment_callbacks;
    } else if (p->state == PROMISE_REJECTED) {
        queue_to_process = &p->rejection_callbacks;
    } else {
        return;
    }
    
    pthread_mutex_lock(&queue_to_process->mutex);
    if (queue_to_process->count > 0) {
        cbs_copy = malloc(queue_to_process->count * sizeof(PromiseCallback));
        if (!cbs_copy) {
            pthread_mutex_unlock(&queue_to_process->mutex);
            return;
        }
        
        memcpy(cbs_copy, queue_to_process->items, 
               queue_to_process->count * sizeof(PromiseCallback));
        count_copy = queue_to_process->count;
        
        // Clear the queue
        free(queue_to_process->items);
        queue_to_process->items = NULL;
        queue_to_process->count = 0;
        queue_to_process->capacity = 0;
    }
    pthread_mutex_unlock(&queue_to_process->mutex);
    
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

Promise* promise_then(Promise* p, on_fulfilled_callback on_fulfilled,
                     on_rejected_callback on_rejected, void* user_data)
{
    if (!p) return NULL;
    
    Promise* chained_promise = promise_create_persistent(
        p->is_persistent_backed ? p->pmem_handle : NULL,
        p->resource_lock
    );
    if (!chained_promise) return NULL;
    
    if (p->state == PROMISE_PENDING) {
        if (on_fulfilled) {
            add_callback(&p->fulfillment_callbacks, on_fulfilled, on_rejected, 
                        user_data, chained_promise);
        }
        if (on_rejected) {
            add_callback(&p->rejection_callbacks, on_fulfilled, on_rejected, 
                        user_data, chained_promise);
        }
    } else {
        // Already settled, schedule callback via event loop
        struct settled_callback_data {
            Promise* promise;
            Promise* chained_promise;
            on_fulfilled_callback on_fulfilled;
            on_rejected_callback on_rejected;
            void* user_data;
        };
        
        struct settled_callback_data* data = malloc(sizeof(*data));
        data->promise = promise_ref(p);
        data->chained_promise = promise_ref(chained_promise);
        data->on_fulfilled = on_fulfilled;
        data->on_rejected = on_rejected;
        data->user_data = user_data;
        
        promise_enqueue_microtask([](void* task_data) {
            struct settled_callback_data* d = task_data;
            PromiseValue result = NULL;
            
            if (d->promise->state == PROMISE_FULFILLED && d->on_fulfilled) {
                result = d->on_fulfilled(d->promise->value, d->user_data);
                promise_resolve(d->chained_promise, result);
            } else if (d->promise->state == PROMISE_REJECTED && d->on_rejected) {
                result = d->on_rejected(d->promise->value, d->user_data);
                promise_resolve(d->chained_promise, result);
            } else {
                if (d->promise->state == PROMISE_FULFILLED) {
                    promise_resolve(d->chained_promise, d->promise->value);
                } else {
                    promise_reject(d->chained_promise, d->promise->value);
                }
            }
            
            promise_unref(d->promise);
            promise_unref(d->chained_promise);
            free(d);
        }, data);
    }
    
    return chained_promise;
}

Promise* promise_catch(Promise* p, on_rejected_callback on_rejected, void* user_data)
{
    return promise_then(p, NULL, on_rejected, user_data);
}

void promise_free(Promise* p)
{
    if (!p) return;
    
    free_callback_queue(&p->fulfillment_callbacks);
    free_callback_queue(&p->rejection_callbacks);
    
    pthread_mutex_destroy(&p->ref_mutex);
    
    free(p);
}

// --- Q.defer() Implementation ---

PromiseDeferred* promise_defer_create(void)
{
    return promise_defer_create_persistent(NULL, NULL);
}

PromiseDeferred* promise_defer_create_persistent(void* pmem_ctx, pthread_mutex_t* lock)
{
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

void promise_defer_resolve(PromiseDeferred* deferred, PromiseValue value)
{
    if (!deferred || !deferred->promise) return;
    promise_resolve(deferred->promise, value);
}

void promise_defer_reject(PromiseDeferred* deferred, PromiseValue reason)
{
    if (!deferred || !deferred->promise) return;
    promise_reject(deferred->promise, reason);
}

void promise_defer_free(PromiseDeferred* deferred)
{
    if (deferred) {
        // The promise will be freed by its own reference counting
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

static PromiseValue promise_all_on_fulfilled(PromiseValue value, void* user_data)
{
    PromiseAllContext* ctx = user_data;
    
    pthread_mutex_lock(&ctx->access_lock);
    ctx->resolved_count++;
    
    if (ctx->resolved_count == ctx->count) {
        promise_defer_resolve(ctx->master_deferred, ctx->results);
    }
    
    pthread_mutex_unlock(&ctx->access_lock);
    return NULL;
}

static PromiseValue promise_all_on_rejected(PromiseValue reason, void* user_data)
{
    PromiseAllContext* ctx = user_data;
    
    pthread_mutex_lock(&ctx->access_lock);
    if (ctx->rejected_count == 0) {
        ctx->rejected_count++;
        promise_defer_reject(ctx->master_deferred, reason);
    }
    pthread_mutex_unlock(&ctx->access_lock);
    return NULL;
}

Promise* promise_all(Promise* promises[], size_t count)
{
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

static void* event_loop_thread(void* arg)
{
    (void)arg;
    
    while (event_loop.running) {
        pthread_mutex_lock(&event_loop.task_queue_mutex);
        
        while (!event_loop.task_queue_head && event_loop.running) {
            pthread_cond_wait(&event_loop.task_queue_cond, &event_loop.task_queue_mutex);
        }
        
        if (!event_loop.running) {
            pthread_mutex_unlock(&event_loop.task_queue_mutex);
            break;
        }
        
        struct task_node* task = event_loop.task_queue_head;
        event_loop.task_queue_head = task->next;
        if (!event_loop.task_queue_head) {
            event_loop.task_queue_tail = NULL;
        }
        
        pthread_mutex_unlock(&event_loop.task_queue_mutex);
        
        if (task->task) {
            task->task(task->data);
        }
        free(task);
    }
    
    return NULL;
}

CPM_Result promise_event_loop_init(void)
{
    if (event_loop.initialized) {
        return CPM_RESULT_SUCCESS;
    }
    
    event_loop.task_queue_head = NULL;
    event_loop.task_queue_tail = NULL;
    event_loop.running = false;
    
    if (pthread_mutex_init(&event_loop.task_queue_mutex, NULL) != 0) {
        return CPM_RESULT_ERROR_INITIALIZATION_FAILED;
    }
    
    if (pthread_cond_init(&event_loop.task_queue_cond, NULL) != 0) {
        pthread_mutex_destroy(&event_loop.task_queue_mutex);
        return CPM_RESULT_ERROR_INITIALIZATION_FAILED;
    }
    
    event_loop.initialized = true;
    return CPM_RESULT_SUCCESS;
}

void promise_event_loop_run(void)
{
    if (!event_loop.initialized || event_loop.running) return;
    
    event_loop.running = true;
    pthread_create(&event_loop.event_thread, NULL, event_loop_thread, NULL);
}

void promise_event_loop_stop(void)
{
    if (!event_loop.running) return;
    
    event_loop.running = false;
    pthread_cond_broadcast(&event_loop.task_queue_cond);
    pthread_join(event_loop.event_thread, NULL);
}

void promise_event_loop_cleanup(void)
{
    if (!event_loop.initialized) return;
    
    promise_event_loop_stop();
    
    // Clean up remaining tasks
    pthread_mutex_lock(&event_loop.task_queue_mutex);
    while (event_loop.task_queue_head) {
        struct task_node* task = event_loop.task_queue_head;
        event_loop.task_queue_head = task->next;
        free(task);
    }
    pthread_mutex_unlock(&event_loop.task_queue_mutex);
    
    pthread_mutex_destroy(&event_loop.task_queue_mutex);
    pthread_cond_destroy(&event_loop.task_queue_cond);
    
    event_loop.initialized = false;
}

void promise_enqueue_microtask(void (*task)(void* data), void* data)
{
    if (!event_loop.initialized || !task) return;
    
    struct task_node* node = malloc(sizeof(struct task_node));
    if (!node) return;
    
    node->task = task;
    node->data = data;
    node->next = NULL;
    
    pthread_mutex_lock(&event_loop.task_queue_mutex);
    
    if (!event_loop.task_queue_tail) {
        event_loop.task_queue_head = event_loop.task_queue_tail = node;
    } else {
        event_loop.task_queue_tail->next = node;
        event_loop.task_queue_tail = node;
    }
    
    pthread_cond_signal(&event_loop.task_queue_cond);
    pthread_mutex_unlock(&event_loop.task_queue_mutex);
}

// --- Utility Functions ---

Promise* promise_resolve_value(PromiseValue value)
{
    Promise* p = promise_create();
    promise_resolve(p, value);
    return p;
}

Promise* promise_reject_reason(PromiseValue reason)
{
    Promise* p = promise_create();
    promise_reject(p, reason);
    return p;
}

Promise* promise_delay(int ms)
{
    PromiseDeferred* deferred = promise_defer_create();
    if (!deferred) return NULL;
    
    struct delay_data {
        PromiseDeferred* deferred;
        int ms;
    };
    
    struct delay_data* data = malloc(sizeof(*data));
    data->deferred = deferred;
    data->ms = ms;
    
    promise_enqueue_microtask([](void* task_data) {
        struct delay_data* d = task_data;
        struct timespec ts = {d->ms / 1000, (d->ms % 1000) * 1000000};
        nanosleep(&ts, NULL);
        promise_defer_resolve(d->deferred, NULL);
        free(d);
    }, data);
    
    return deferred->promise;
}
/*
 * File: lib/core/cpm_promise_all.c
 * Description: Q.all() and Q.allSettled() implementation
 * Author: Dr. Q Josef Kurk Edwards
 */

#include "cpm_promise.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

// --- Promise.all() Implementation ---

typedef struct {
    Promise** promises;
    size_t count;
    PromiseValue* results;
    size_t resolved_count;
    size_t rejected_count;
    PromiseDeferred* master_deferred;
    pthread_mutex_t access_lock;
    bool any_rejected;
} PromiseAllContext;

typedef struct {
    PromiseAllContext* ctx;
    size_t index;
} PromiseAllCallbackContext;

static PromiseValue promise_all_on_fulfilled(PromiseValue value, void* user_data) {
    PromiseAllCallbackContext* cb_ctx = (PromiseAllCallbackContext*)user_data;
    PromiseAllContext* ctx = cb_ctx->ctx;
    size_t index = cb_ctx->index;
    
    pthread_mutex_lock(&ctx->access_lock);
    
    if (!ctx->any_rejected) {
        ctx->results[index] = value;
        ctx->resolved_count++;
        
        if (ctx->resolved_count == ctx->count) {
            // All promises fulfilled
            promise_defer_resolve(ctx->master_deferred, (PromiseValue)ctx->results);
        }
    }
    
    pthread_mutex_unlock(&ctx->access_lock);
    return NULL;
}

static PromiseValue promise_all_on_rejected(PromiseValue reason, void* user_data) {
    PromiseAllCallbackContext* cb_ctx = (PromiseAllCallbackContext*)user_data;
    PromiseAllContext* ctx = cb_ctx->ctx;
    
    pthread_mutex_lock(&ctx->access_lock);
    
    if (ctx->rejected_count == 0) {
        ctx->rejected_count++;
        ctx->any_rejected = true;
        promise_defer_reject(ctx->master_deferred, reason);
    }
    
    pthread_mutex_unlock(&ctx->access_lock);
    return NULL;
}

Promise* promise_all(Promise* promises[], size_t count) {
    if (count == 0) {
        // Return immediately resolved promise with empty array
        PromiseValue* empty_results = malloc(sizeof(PromiseValue));
        *empty_results = NULL;
        return promise_resolve_value((PromiseValue)empty_results);
    }
    
    PromiseDeferred* master_deferred = promise_defer_create();
    if (!master_deferred) {
        return NULL;
    }
    
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
    ctx->any_rejected = false;
    
    if (pthread_mutex_init(&ctx->access_lock, NULL) != 0) {
        free(ctx->results);
        free(ctx);
        promise_defer_free(master_deferred);
        return NULL;
    }
    
    // Create callback contexts for each promise
    PromiseAllCallbackContext* callback_contexts = malloc(count * sizeof(PromiseAllCallbackContext));
    if (!callback_contexts) {
        pthread_mutex_destroy(&ctx->access_lock);
        free(ctx->results);
        free(ctx);
        promise_defer_free(master_deferred);
        return NULL;
    }
    
    // Attach handlers to all promises
    for (size_t i = 0; i < count; i++) {
        callback_contexts[i].ctx = ctx;
        callback_contexts[i].index = i;
        
        promise_then(promises[i], 
                    promise_all_on_fulfilled, 
                    promise_all_on_rejected,
                    &callback_contexts[i]);
    }
    
    Promise* result_promise = promise_defer_get_promise(master_deferred);
    
    // TODO: Proper cleanup of context when promise settles
    // This would typically be done with a finally-like callback
    
    return result_promise;
}

// --- Promise.allSettled() Implementation ---

typedef enum {
    SETTLEMENT_FULFILLED,
    SETTLEMENT_REJECTED
} SettlementStatus;

typedef struct {
    SettlementStatus status;
    PromiseValue value;  // value if fulfilled, reason if rejected
} SettlementResult;

typedef struct {
    Promise** promises;
    size_t count;
    SettlementResult* results;
    size_t settled_count;
    PromiseDeferred* master_deferred;
    pthread_mutex_t access_lock;
} PromiseAllSettledContext;

typedef struct {
    PromiseAllSettledContext* ctx;
    size_t index;
} PromiseAllSettledCallbackContext;

static PromiseValue promise_all_settled_on_fulfilled(PromiseValue value, void* user_data) {
    PromiseAllSettledCallbackContext* cb_ctx = (PromiseAllSettledCallbackContext*)user_data;
    PromiseAllSettledContext* ctx = cb_ctx->ctx;
    size_t index = cb_ctx->index;
    
    pthread_mutex_lock(&ctx->access_lock);
    
    ctx->results[index].status = SETTLEMENT_FULFILLED;
    ctx->results[index].value = value;
    ctx->settled_count++;
    
    if (ctx->settled_count == ctx->count) {
        // All promises settled
        promise_defer_resolve(ctx->master_deferred, (PromiseValue)ctx->results);
    }
    
    pthread_mutex_unlock(&ctx->access_lock);
    return NULL;
}

static PromiseValue promise_all_settled_on_rejected(PromiseValue reason, void* user_data) {
    PromiseAllSettledCallbackContext* cb_ctx = (PromiseAllSettledCallbackContext*)user_data;
    PromiseAllSettledContext* ctx = cb_ctx->ctx;
    size_t index = cb_ctx->index;
    
    pthread_mutex_lock(&ctx->access_lock);
    
    ctx->results[index].status = SETTLEMENT_REJECTED;
    ctx->results[index].value = reason;
    ctx->settled_count++;
    
    if (ctx->settled_count == ctx->count) {
        // All promises settled
        promise_defer_resolve(ctx->master_deferred, (PromiseValue)ctx->results);
    }
    
    pthread_mutex_unlock(&ctx->access_lock);
    return NULL;
}

Promise* promise_all_settled(Promise* promises[], size_t count) {
    if (count == 0) {
        SettlementResult* empty_results = malloc(sizeof(SettlementResult));
        return promise_resolve_value((PromiseValue)empty_results);
    }
    
    PromiseDeferred* master_deferred = promise_defer_create();
    if (!master_deferred) {
        return NULL;
    }
    
    PromiseAllSettledContext* ctx = malloc(sizeof(PromiseAllSettledContext));
    if (!ctx) {
        promise_defer_free(master_deferred);
        return NULL;
    }
    
    ctx->promises = promises;
    ctx->count = count;
    ctx->results = calloc(count, sizeof(SettlementResult));
    if (!ctx->results) {
        free(ctx);
        promise_defer_free(master_deferred);
        return NULL;
    }
    
    ctx->settled_count = 0;
    ctx->master_deferred = master_deferred;
    
    if (pthread_mutex_init(&ctx->access_lock, NULL) != 0) {
        free(ctx->results);
        free(ctx);
        promise_defer_free(master_deferred);
        return NULL;
    }
    
    PromiseAllSettledCallbackContext* callback_contexts = 
        malloc(count * sizeof(PromiseAllSettledCallbackContext));
    if (!callback_contexts) {
        pthread_mutex_destroy(&ctx->access_lock);
        free(ctx->results);
        free(ctx);
        promise_defer_free(master_deferred);
        return NULL;
    }
    
    for (size_t i = 0; i < count; i++) {
        callback_contexts[i].ctx = ctx;
        callback_contexts[i].index = i;
        
        promise_then(promises[i],
                    promise_all_settled_on_fulfilled,
                    promise_all_settled_on_rejected,
                    &callback_contexts[i]);
    }
    
    return promise_defer_get_promise(master_deferred);
}

// --- Q.nfcall() Implementation ---

typedef struct {
    NodeCallback callback;
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
    if (!cb) {
        return promise_reject_reason("Invalid callback");
    }
    
    PromiseDeferred* deferred = promise_defer_create();
    if (!deferred) {
        return NULL;
    }
    
    NfcallContext* ctx = malloc(sizeof(NfcallContext));
    if (!ctx) {
        promise_defer_free(deferred);
        return NULL;
    }
    
    ctx->callback = cb;
    ctx->user_data = user_data;
    ctx->deferred = deferred;
    
    // Execute callback with our wrapper
    cb(NULL, NULL, ctx);  // Simplified - actual implementation would schedule this
    
    return promise_defer_get_promise(deferred);
}
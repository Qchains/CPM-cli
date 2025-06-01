#include "cpm.h"

QPromise* q_promise_new(void) {
    QPromise* promise = malloc(sizeof(QPromise));
    if (!promise) return NULL;
    
    promise->state = Q_PENDING;
    promise->result = malloc(sizeof(QPromiseResult));
    if (!promise->result) {
        free(promise);
        return NULL;
    }
    
    promise->result->data = NULL;
    promise->result->error = NULL;
    promise->result->state = Q_PENDING;
    promise->then_callback = NULL;
    promise->catch_callback = NULL;
    
    // Initialize thread synchronization
    if (pthread_mutex_init(&promise->mutex, NULL) != 0) {
        free(promise->result);
        free(promise);
        return NULL;
    }
    
    if (pthread_cond_init(&promise->condition, NULL) != 0) {
        pthread_mutex_destroy(&promise->mutex);
        free(promise->result);
        free(promise);
        return NULL;
    }
    
    return promise;
}

void q_promise_resolve(QPromise* promise, void* data) {
    if (!promise || promise->state != Q_PENDING) return;
    
    pthread_mutex_lock(&promise->mutex);
    
    promise->state = Q_FULFILLED;
    promise->result->state = Q_FULFILLED;
    promise->result->data = data;
    
    // Execute then callback if registered
    if (promise->then_callback) {
        promise->then_callback(promise->result);
    }
    
    pthread_cond_broadcast(&promise->condition);
    pthread_mutex_unlock(&promise->mutex);
}

void q_promise_reject(QPromise* promise, const char* error) {
    if (!promise || promise->state != Q_PENDING) return;
    
    pthread_mutex_lock(&promise->mutex);
    
    promise->state = Q_REJECTED;
    promise->result->state = Q_REJECTED;
    
    // Copy error message
    if (error) {
        size_t error_len = strlen(error) + 1;
        promise->result->error = malloc(error_len);
        if (promise->result->error) {
            strcpy(promise->result->error, error);
        }
    }
    
    // Execute catch callback if registered
    if (promise->catch_callback) {
        promise->catch_callback(promise->result);
    }
    
    pthread_cond_broadcast(&promise->condition);
    pthread_mutex_unlock(&promise->mutex);
}

QPromise* q_promise_then(QPromise* promise, QPromiseThen callback) {
    if (!promise || !callback) return promise;
    
    pthread_mutex_lock(&promise->mutex);
    promise->then_callback = callback;
    
    // If already resolved, execute immediately
    if (promise->state == Q_FULFILLED) {
        callback(promise->result);
    }
    
    pthread_mutex_unlock(&promise->mutex);
    return promise;
}

QPromise* q_promise_catch(QPromise* promise, QPromiseThen callback) {
    if (!promise || !callback) return promise;
    
    pthread_mutex_lock(&promise->mutex);
    promise->catch_callback = callback;
    
    // If already rejected, execute immediately
    if (promise->state == Q_REJECTED) {
        callback(promise->result);
    }
    
    pthread_mutex_unlock(&promise->mutex);
    return promise;
}

void q_promise_wait(QPromise* promise) {
    if (!promise) return;
    
    pthread_mutex_lock(&promise->mutex);
    
    while (promise->state == Q_PENDING) {
        pthread_cond_wait(&promise->condition, &promise->mutex);
    }
    
    pthread_mutex_unlock(&promise->mutex);
}

void q_promise_free(QPromise* promise) {
    if (!promise) return;
    
    pthread_mutex_lock(&promise->mutex);
    
    if (promise->result) {
        if (promise->result->error) {
            free(promise->result->error);
        }
        free(promise->result);
    }
    
    pthread_mutex_unlock(&promise->mutex);
    pthread_mutex_destroy(&promise->mutex);
    pthread_cond_destroy(&promise->condition);
    
    free(promise);
}

// Q Promise utility functions for chaining
QPromise* q_all(QPromise** promises, size_t count) {
    if (!promises || count == 0) return NULL;
    
    QPromise* all_promise = q_promise_new();
    if (!all_promise) return NULL;
    
    // Create a structure to track completion
    typedef struct {
        size_t completed;
        size_t total;
        void** results;
        bool has_error;
        QPromise* main_promise;
        pthread_mutex_t mutex;
    } AllContext;
    
    AllContext* ctx = malloc(sizeof(AllContext));
    if (!ctx) {
        q_promise_free(all_promise);
        return NULL;
    }
    
    ctx->completed = 0;
    ctx->total = count;
    ctx->results = calloc(count, sizeof(void*));
    ctx->has_error = false;
    ctx->main_promise = all_promise;
    pthread_mutex_init(&ctx->mutex, NULL);
    
    // Set up completion callbacks for each promise
    for (size_t i = 0; i < count; i++) {
        // This would require more complex callback handling
        // For now, we'll implement a simplified version
        q_promise_wait(promises[i]);
        
        pthread_mutex_lock(&ctx->mutex);
        if (promises[i]->state == Q_REJECTED) {
            if (!ctx->has_error) {
                ctx->has_error = true;
                q_promise_reject(all_promise, "One or more promises failed");
            }
        } else {
            ctx->results[i] = promises[i]->result->data;
            ctx->completed++;
        }
        pthread_mutex_unlock(&ctx->mutex);
    }
    
    pthread_mutex_lock(&ctx->mutex);
    if (!ctx->has_error && ctx->completed == ctx->total) {
        q_promise_resolve(all_promise, ctx->results);
    }
    pthread_mutex_unlock(&ctx->mutex);
    
    pthread_mutex_destroy(&ctx->mutex);
    free(ctx);
    
    return all_promise;
}

// Async/await style wrapper
typedef struct {
    QPromise* promise;
    void* result;
    char* error;
} AwaitResult;

AwaitResult q_await(QPromise* promise) {
    AwaitResult result = {0};
    if (!promise) return result;
    
    q_promise_wait(promise);
    
    result.promise = promise;
    if (promise->state == Q_FULFILLED) {
        result.result = promise->result->data;
    } else if (promise->state == Q_REJECTED) {
        result.error = promise->result->error;
    }
    
    return result;
}
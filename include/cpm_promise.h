/*
 * File: include/cpm_promise.h
 * Description: Q Promise library API for CPM - bringing JavaScript-style 
 * promise-based asynchronous programming to C.
 * Author: Dr. Q Josef Kurk Edwards
 */

#ifndef CPM_PROMISE_H
#define CPM_PROMISE_H

#include <stdbool.h>
#include <pthread.h>
#include <stddef.h>

// --- Forward Declarations ---
typedef struct Promise Promise;
typedef struct PromiseDeferred PromiseDeferred;

// --- Promise States ---
typedef enum {
    PROMISE_PENDING,
    PROMISE_FULFILLED,
    PROMISE_REJECTED
} PromiseState;

// --- Generic Value/Reason Type ---
typedef void* PromiseValue;

// --- Callback Function Types ---
typedef PromiseValue (*on_fulfilled_callback)(PromiseValue value, void* user_data);
typedef PromiseValue (*on_rejected_callback)(PromiseValue reason, void* user_data);

// --- Node.js-style callback wrapping ---
typedef void (*NodeCallback)(void* err, PromiseValue result, void* user_data);

// --- PMLL Types (Persistent Memory Lock Library) ---
typedef void* PMEMContextHandle;
typedef pthread_mutex_t PMLL_Lock;

// --- Promise API ---
Promise* promise_create(void);
Promise* promise_create_persistent(PMEMContextHandle pmem_ctx, PMLL_Lock* lock);
void promise_resolve(Promise* p, PromiseValue value);
void promise_reject(Promise* p, PromiseValue reason);
Promise* promise_then(Promise* p, on_fulfilled_callback on_fulfilled, 
                     on_rejected_callback on_rejected, void* user_data);
void promise_free(Promise* p);

// --- Q.defer() API ---
PromiseDeferred* promise_defer_create(void);
PromiseDeferred* promise_defer_create_persistent(PMEMContextHandle pmem_ctx, PMLL_Lock* lock);
void promise_defer_resolve(PromiseDeferred* deferred, PromiseValue value);
void promise_defer_reject(PromiseDeferred* deferred, PromiseValue reason);
void promise_defer_free(PromiseDeferred* deferred);

// --- Q.all() API ---
Promise* promise_all(Promise* promises[], size_t count);

// --- Promise State Access ---
PromiseState promise_get_state(const Promise* p);
PromiseValue promise_get_value(const Promise* p);

// --- Deferred Promise Access ---
Promise* promise_defer_get_promise(PromiseDeferred* deferred);

// --- Event Loop Simulation (for async behavior) ---
void init_event_loop(void);
void enqueue_microtask(void (*task)(void* data), void* data);
void run_event_loop(void);
void free_event_loop(void);

#endif // CPM_PROMISE_H
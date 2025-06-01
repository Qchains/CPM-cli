/*
 * File: include/cpm_promise.h
 * Description: Q Promise library API for C - Public interface
 * Author: Dr. Q Josef Kurk Edwards
 */

#ifndef CPM_PROMISE_H
#define CPM_PROMISE_H

#include "cpm_types.h"
#include <stdbool.h>
#include <pthread.h>

// --- Core Promise API ---

/**
 * @brief Creates a new promise in the PENDING state
 * @return Pointer to newly created Promise or NULL on failure
 */
Promise* promise_create(void);

/**
 * @brief Creates a promise backed by persistent memory (PMDK integration)
 * @param pmem_ctx Persistent memory context handle
 * @param lock PMLL lock for thread safety
 * @return Pointer to newly created Promise or NULL on failure
 */
Promise* promise_create_persistent(void* pmem_ctx, pthread_mutex_t* lock);

/**
 * @brief Resolves a promise with a given value
 * @param p Promise to resolve
 * @param value Value to resolve with
 */
void promise_resolve(Promise* p, PromiseValue value);

/**
 * @brief Rejects a promise with a given reason
 * @param p Promise to reject  
 * @param reason Rejection reason
 */
void promise_reject(Promise* p, PromiseValue reason);

/**
 * @brief Attaches fulfillment and rejection handlers to a promise
 * @param p Promise to attach handlers to
 * @param on_fulfilled Fulfillment callback (can be NULL)
 * @param on_rejected Rejection callback (can be NULL)
 * @param user_data User data passed to callbacks
 * @return New promise representing the result of the callbacks
 */
Promise* promise_then(Promise* p, 
                     on_fulfilled_callback on_fulfilled,
                     on_rejected_callback on_rejected, 
                     void* user_data);

/**
 * @brief Frees a promise and its resources
 * @param p Promise to free
 */
void promise_free(Promise* p);

// --- Q.defer() Pattern API ---

/**
 * @brief Creates a deferred object with an associated promise
 * @return Pointer to PromiseDeferred or NULL on failure
 */
PromiseDeferred* promise_defer_create(void);

/**
 * @brief Creates a deferred object backed by persistent memory
 * @param pmem_ctx Persistent memory context
 * @param lock PMLL lock for thread safety
 * @return Pointer to PromiseDeferred or NULL on failure
 */
PromiseDeferred* promise_defer_create_persistent(void* pmem_ctx, pthread_mutex_t* lock);

/**
 * @brief Resolves the promise associated with a deferred object
 * @param deferred Deferred object
 * @param value Value to resolve with
 */
void promise_defer_resolve(PromiseDeferred* deferred, PromiseValue value);

/**
 * @brief Rejects the promise associated with a deferred object
 * @param deferred Deferred object
 * @param reason Rejection reason
 */
void promise_defer_reject(PromiseDeferred* deferred, PromiseValue reason);

/**
 * @brief Gets the promise from a deferred object
 * @param deferred Deferred object
 * @return Associated promise
 */
Promise* promise_defer_get_promise(PromiseDeferred* deferred);

/**
 * @brief Frees a deferred object
 * @param deferred Deferred object to free
 */
void promise_defer_free(PromiseDeferred* deferred);

// --- Q.all() API ---

/**
 * @brief Creates a promise that resolves when all input promises resolve
 * @param promises Array of promises
 * @param count Number of promises in array
 * @return Promise that resolves with array of results or rejects on first rejection
 */
Promise* promise_all(Promise* promises[], size_t count);

/**
 * @brief Creates a promise that resolves when all input promises settle (resolve or reject)
 * @param promises Array of promises
 * @param count Number of promises in array
 * @return Promise that resolves with array of settlement results
 */
Promise* promise_all_settled(Promise* promises[], size_t count);

// --- Q.nfcall() API (Node.js-style callback wrapping) ---

/**
 * @brief Node.js style callback function type
 * @param err Error object (NULL if no error)
 * @param result Result value
 * @param user_data User context data
 */
typedef void (*NodeCallback)(void* err, PromiseValue result, void* user_data);

/**
 * @brief Wraps a Node.js style callback function in a promise
 * @param cb Callback function
 * @param user_data User data for callback
 * @return Promise that will be resolved/rejected based on callback result
 */
Promise* promise_nfcall(NodeCallback cb, void* user_data);

// --- Promise Utilities ---

/**
 * @brief Creates a promise that is already resolved with a value
 * @param value Value to resolve with
 * @return Resolved promise
 */
Promise* promise_resolve_value(PromiseValue value);

/**
 * @brief Creates a promise that is already rejected with a reason
 * @param reason Rejection reason
 * @return Rejected promise
 */
Promise* promise_reject_reason(PromiseValue reason);

/**
 * @brief Gets the current state of a promise
 * @param p Promise to query
 * @return Current promise state
 */
PromiseState promise_get_state(Promise* p);

/**
 * @brief Gets the value/reason from a settled promise
 * @param p Promise to query (must be settled)
 * @return Value if fulfilled, reason if rejected, NULL if pending
 */
PromiseValue promise_get_value(Promise* p);

// --- Event Loop Integration ---

/**
 * @brief Initializes the promise event loop
 */
void cpm_event_loop_init(void);

/**
 * @brief Runs the event loop for a single iteration
 * @return true if there are pending operations, false if idle
 */
bool cpm_event_loop_run_once(void);

/**
 * @brief Runs the event loop until all promises are settled
 */
void cpm_event_loop_run_until_complete(void);

/**
 * @brief Enqueues a microtask to be executed on the next event loop tick
 * @param task Task function to execute
 * @param data Data to pass to task
 */
void cpm_event_loop_enqueue_microtask(void (*task)(void* data), void* data);

/**
 * @brief Shuts down the event loop and frees resources
 */
void cpm_event_loop_shutdown(void);

#endif // CPM_PROMISE_H
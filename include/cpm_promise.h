/*
 * File: include/cpm_promise.h
 * Description: Q Promise library API for C - Public interface
 * Implements JavaScript Q-style promises with full PMDK integration
 * Author: Dr. Q Josef Kurk Edwards
 */

#ifndef CPM_PROMISE_H
#define CPM_PROMISE_H

#include "cpm_types.h"
#include <pthread.h>

// --- Core Promise API ---

/**
 * @brief Creates a new promise in the PENDING state.
 * @return Pointer to the newly created Promise, or NULL on failure.
 */
Promise* promise_create(void);

/**
 * @brief Creates a new promise backed by persistent memory.
 * @param pmem_ctx Persistent memory context handle
 * @param lock Optional PMLL lock for resource synchronization
 * @return Pointer to the newly created Promise, or NULL on failure.
 */
Promise* promise_create_persistent(PMEMContextHandle pmem_ctx, PMLL_Lock* lock);

/**
 * @brief Resolves a promise with a given value.
 * @param p The promise to resolve
 * @param value The fulfillment value
 */
void promise_resolve(Promise* p, PromiseValue value);

/**
 * @brief Rejects a promise with a given reason.
 * @param p The promise to reject
 * @param reason The rejection reason
 */
void promise_reject(Promise* p, PromiseValue reason);

/**
 * @brief Attaches fulfillment and rejection handlers to a promise.
 * @param p The promise to attach handlers to
 * @param on_fulfilled Callback for fulfillment (can be NULL)
 * @param on_rejected Callback for rejection (can be NULL)
 * @param user_data User data passed to callbacks
 * @return A new Promise for chaining
 */
Promise* promise_then(Promise* p, on_fulfilled_callback on_fulfilled, 
                     on_rejected_callback on_rejected, void* user_data);

/**
 * @brief Frees a promise and its resources.
 * @param p The promise to free
 */
void promise_free(Promise* p);

// --- Q.defer() API ---

/**
 * @brief Creates a new deferred object.
 * @return Pointer to PromiseDeferred, or NULL on failure.
 */
PromiseDeferred* promise_defer_create(void);

/**
 * @brief Creates a new deferred object backed by persistent memory.
 * @param pmem_ctx Persistent memory context
 * @param lock Optional PMLL lock
 * @return Pointer to PromiseDeferred, or NULL on failure.
 */
PromiseDeferred* promise_defer_create_persistent(PMEMContextHandle pmem_ctx, PMLL_Lock* lock);

/**
 * @brief Resolves the promise associated with this deferred.
 * @param deferred The deferred object
 * @param value The fulfillment value
 */
void promise_defer_resolve(PromiseDeferred* deferred, PromiseValue value);

/**
 * @brief Rejects the promise associated with this deferred.
 * @param deferred The deferred object
 * @param reason The rejection reason
 */
void promise_defer_reject(PromiseDeferred* deferred, PromiseValue reason);

/**
 * @brief Frees a deferred object.
 * @param deferred The deferred to free
 */
void promise_defer_free(PromiseDeferred* deferred);

// --- Q.all() API ---

/**
 * @brief Creates a promise that resolves when all input promises resolve.
 * @param promises Array of promises to wait for
 * @param count Number of promises in the array
 * @return Promise that resolves with array of results or rejects with first error
 */
Promise* promise_all(Promise* promises[], size_t count);

// --- Q.nfcall() API (Node.js-style callback wrapping) ---
typedef void (*NodeCallback)(void* err, PromiseValue result, void* user_data);

/**
 * @brief Wraps a Node.js-style callback function in a promise.
 * @param cb The callback function
 * @param user_data User data for the callback
 * @return Promise that resolves/rejects based on callback result
 */
Promise* promise_nfcall(NodeCallback cb, void* user_data);

// --- Event Loop API ---

/**
 * @brief Initializes the promise event loop.
 * @return true on success, false on failure
 */
bool init_event_loop(void);

/**
 * @brief Enqueues a microtask for execution.
 * @param task Function to execute
 * @param data Data to pass to the function
 */
void enqueue_microtask(void (*task)(void* data), void* data);

/**
 * @brief Runs the event loop until all promises are settled.
 */
void run_event_loop(void);

/**
 * @brief Frees event loop resources.
 */
void free_event_loop(void);

// --- Promise Utilities ---

/**
 * @brief Gets the current state of a promise.
 * @param p The promise
 * @return The promise state
 */
PromiseState promise_get_state(Promise* p);

/**
 * @brief Gets the value/reason of a settled promise.
 * @param p The promise
 * @return The promise value/reason, or NULL if pending
 */
PromiseValue promise_get_value(Promise* p);

/**
 * @brief Checks if a promise is settled (fulfilled or rejected).
 * @param p The promise
 * @return true if settled, false if pending
 */
bool promise_is_settled(Promise* p);

#endif // CPM_PROMISE_H
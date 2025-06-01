/*
 * File: include/cpm_promise.h
 * Description: Q Promise library API for CPM - Complete implementation of JavaScript-style promises in C
 * Author: Dr. Q Josef Kurk Edwards
 */

#ifndef CPM_PROMISE_H
#define CPM_PROMISE_H

#include "cpm_types.h"
#include <pthread.h>
#include <stdbool.h>

// --- Promise States ---
typedef enum {
    PROMISE_PENDING,
    PROMISE_FULFILLED,
    PROMISE_REJECTED
} PromiseState;

// --- Promise Callback Structure ---
typedef struct {
    on_fulfilled_callback on_fulfilled;
    on_rejected_callback on_rejected;
    void* user_data;
    Promise* chained_promise;
} PromiseCallback;

// --- Promise Callback Queue ---
typedef struct {
    PromiseCallback* items;
    size_t count;
    size_t capacity;
    pthread_mutex_t mutex;
} PromiseCallbackQueue;

// --- Promise Structure ---
struct Promise {
    PromiseState state;
    PromiseValue value;
    
    PromiseCallbackQueue fulfillment_callbacks;
    PromiseCallbackQueue rejection_callbacks;
    
    // PMLL/PMDK integration
    bool is_persistent_backed;
    void* pmem_handle;
    pthread_mutex_t* resource_lock;
    
    // Reference counting for memory management
    int ref_count;
    pthread_mutex_t ref_mutex;
    
    // Debug information
    const char* creation_site;
    uint64_t creation_time;
};

// --- Q.defer() Pattern ---
struct PromiseDeferred {
    Promise* promise;
    void* pmem_op_ctx;
};

// --- Core Promise API ---

/**
 * @brief Creates a new promise in the PENDING state
 * @return Pointer to newly created Promise or NULL on failure
 */
Promise* promise_create(void);

/**
 * @brief Creates a promise backed by persistent memory
 * @param pmem_ctx Persistent memory context
 * @param lock Optional lock for synchronization
 * @return Pointer to newly created Promise or NULL on failure
 */
Promise* promise_create_persistent(void* pmem_ctx, pthread_mutex_t* lock);

/**
 * @brief Resolves a promise with a value
 * @param p Promise to resolve
 * @param value Value to resolve with
 */
void promise_resolve(Promise* p, PromiseValue value);

/**
 * @brief Rejects a promise with a reason
 * @param p Promise to reject
 * @param reason Reason for rejection
 */
void promise_reject(Promise* p, PromiseValue reason);

/**
 * @brief Attaches fulfillment and rejection handlers
 * @param p Promise to attach handlers to
 * @param on_fulfilled Fulfillment callback (can be NULL)
 * @param on_rejected Rejection callback (can be NULL)
 * @param user_data User data passed to callbacks
 * @return New promise for chaining
 */
Promise* promise_then(Promise* p, on_fulfilled_callback on_fulfilled, 
                     on_rejected_callback on_rejected, void* user_data);

/**
 * @brief Attaches only a rejection handler
 * @param p Promise to attach handler to
 * @param on_rejected Rejection callback
 * @param user_data User data passed to callback
 * @return New promise for chaining
 */
Promise* promise_catch(Promise* p, on_rejected_callback on_rejected, void* user_data);

/**
 * @brief Increases promise reference count
 * @param p Promise to reference
 * @return Same promise pointer
 */
Promise* promise_ref(Promise* p);

/**
 * @brief Decreases reference count and frees if zero
 * @param p Promise to unreference
 */
void promise_unref(Promise* p);

/**
 * @brief Force free a promise (use with caution)
 * @param p Promise to free
 */
void promise_free(Promise* p);

// --- Q.defer() API ---

/**
 * @brief Creates a deferred object with an associated promise
 * @return Pointer to PromiseDeferred or NULL on failure
 */
PromiseDeferred* promise_defer_create(void);

/**
 * @brief Creates a persistent deferred object
 * @param pmem_ctx Persistent memory context
 * @param lock Optional lock for synchronization
 * @return Pointer to PromiseDeferred or NULL on failure
 */
PromiseDeferred* promise_defer_create_persistent(void* pmem_ctx, pthread_mutex_t* lock);

/**
 * @brief Resolves the deferred's promise
 * @param deferred Deferred object
 * @param value Value to resolve with
 */
void promise_defer_resolve(PromiseDeferred* deferred, PromiseValue value);

/**
 * @brief Rejects the deferred's promise
 * @param deferred Deferred object
 * @param reason Reason for rejection
 */
void promise_defer_reject(PromiseDeferred* deferred, PromiseValue reason);

/**
 * @brief Frees a deferred object
 * @param deferred Deferred object to free
 */
void promise_defer_free(PromiseDeferred* deferred);

// --- Q.all() API ---

/**
 * @brief Waits for all promises to resolve or any to reject
 * @param promises Array of promise pointers
 * @param count Number of promises in array
 * @return Promise that resolves with array of results or rejects with first error
 */
Promise* promise_all(Promise* promises[], size_t count);

/**
 * @brief Waits for all promises to settle (resolve or reject)
 * @param promises Array of promise pointers
 * @param count Number of promises in array
 * @return Promise that resolves with array of results
 */
Promise* promise_all_settled(Promise* promises[], size_t count);

// --- Q.nfcall() API ---

/**
 * @brief Wraps a Node.js-style callback function as a promise
 * @param cb Callback function
 * @param user_data User data passed to callback
 * @return Promise that resolves/rejects based on callback result
 */
Promise* promise_nfcall(NodeCallback cb, void* user_data);

// --- Utility Functions ---

/**
 * @brief Creates a resolved promise
 * @param value Value to resolve with
 * @return Resolved promise
 */
Promise* promise_resolve_value(PromiseValue value);

/**
 * @brief Creates a rejected promise
 * @param reason Reason for rejection
 * @return Rejected promise
 */
Promise* promise_reject_reason(PromiseValue reason);

/**
 * @brief Delays execution for specified milliseconds
 * @param ms Milliseconds to delay
 * @return Promise that resolves after delay
 */
Promise* promise_delay(int ms);

/**
 * @brief Times out a promise after specified milliseconds
 * @param p Promise to timeout
 * @param ms Timeout in milliseconds
 * @return Promise that rejects if timeout occurs
 */
Promise* promise_timeout(Promise* p, int ms);

// --- Event Loop API ---

/**
 * @brief Initializes the promise event loop
 * @return CPM_RESULT_SUCCESS on success
 */
CPM_Result promise_event_loop_init(void);

/**
 * @brief Runs the event loop
 */
void promise_event_loop_run(void);

/**
 * @brief Stops the event loop
 */
void promise_event_loop_stop(void);

/**
 * @brief Cleans up event loop resources
 */
void promise_event_loop_cleanup(void);

/**
 * @brief Enqueues a microtask
 * @param task Task function to execute
 * @param data Data passed to task
 */
void promise_enqueue_microtask(void (*task)(void* data), void* data);

#endif // CPM_PROMISE_H
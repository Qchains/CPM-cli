/*
 * qpromise.h - Q Promises Library for C
 *
 * This header defines the public API for a C-based implementation of promises,
 * inspired by the Q library in JavaScript. It aims to provide asynchronous
 * operation management, including deferred execution, chaining, and error handling.
 *
 * The API includes:
 * - Core Promise creation and manipulation (resolve, reject, then).
 * - Q.defer() equivalent for creating deferred objects.
 * - Q.all() for synchronizing multiple promises.
 * - Q.nfcall() for wrapping Node.js-style callbacks.
 * - Experimental PMLL (Persistent Memory) hardened queue for resilient operations.
 * - A basic event loop simulation for managing asynchronous tasks.
 */
#ifndef QPROMISE_H
#define QPROMISE_H

#include <stdbool.h> // For bool type
#include <stddef.h>  // For size_t
#include <pthread.h> // For mutex in persistent/concurrent scenarios

// --- Core Types ---

// Forward declaration of the main Promise structure.
typedef struct Promise Promise;

// Forward declaration for the Deferred object, used with Q.defer() style.
typedef struct PromiseDeferred PromiseDeferred;

// Forward declaration for a hardened resource queue, potentially using persistent memory.
// PMLL = Persistent Memory Lexicon/Library (hypothetical or specific project context)
typedef struct PMLL_HardenedResourceQueue PMLL_HardenedResourceQueue;

// Represents the state of a Promise.
typedef enum {
    PROMISE_PENDING,  // Initial state, operation not yet completed.
    PROMISE_FULFILLED, // Operation completed successfully.
    PROMISE_REJECTED   // Operation failed.
} PromiseState;

// Generic type for the value of a fulfilled promise or the reason for a rejection.
// In a more complex system, this might be a tagged union or a more structured error type.
typedef void* PromiseValue;

// Callback function type for when a promise is fulfilled.
// Args:
//   value: The fulfillment value of the parent promise.
//   user_data: Arbitrary user-provided data passed during 'then'.
// Returns:
//   A PromiseValue. This can be:
//   1. A direct value: The chained promise (from 'then') will be fulfilled with this value.
//   2. A new Promise*: The chained promise will adopt the state and value of this new promise.
//   3. NULL (or a special marker): Could indicate no further value, or an error if not handled.
typedef PromiseValue (*on_fulfilled_callback)(PromiseValue value, void* user_data);

// Callback function type for when a promise is rejected.
// Args:
//   reason: The rejection reason of the parent promise.
//   user_data: Arbitrary user-provided data passed during 'then'.
// Returns:
//   A PromiseValue. This can be:
//   1. A direct value: The chained promise will be fulfilled with this value (recovery).
//   2. A new Promise*: The chained promise will adopt the state and value of this new promise.
//   3. If the callback itself wants to propagate or signal a new error, it might
//      return a special error marker or a new promise that is already rejected.
//      Alternatively, it could re-throw by returning a specific error-coded PromiseValue.
typedef PromiseValue (*on_rejected_callback)(PromiseValue reason, void* user_data);


// --- Promise API ---

// Creates a new promise in the PENDING state.
// This is for standard, in-memory promises.
Promise* promise_create(void);

// Creates a new promise, potentially in persistent memory.
// Args:
//   pmem_ctx: Context for persistent memory allocation/management.
//   lock: A mutex for synchronizing access if the promise is shared or persistent.
// This is an advanced feature for durability and inter-process/session state.
Promise* promise_create_persistent(void* pmem_ctx, pthread_mutex_t* lock);

// Resolves a promise with a given value.
// If the promise is already settled, this is a no-op.
// Transitions state from PENDING to FULFILLED.
void promise_resolve(Promise* p, PromiseValue value);

// Rejects a promise with a given reason.
// If the promise is already settled, this is a no-op.
// Transitions state from PENDING to REJECTED.
void promise_reject(Promise* p, PromiseValue reason);

// Attaches fulfillment and rejection handlers to a promise.
// Returns a new promise that is resolved or rejected based on the outcome of the callbacks.
// Args:
//   p: The parent promise.
//   on_fulfilled: Callback for successful resolution. Can be NULL.
//   on_rejected: Callback for rejection. Can be NULL.
//   user_data: Arbitrary data to be passed to the callbacks.
Promise* promise_then(Promise* p, on_fulfilled_callback on_fulfilled, on_rejected_callback on_rejected, void* user_data);

// Frees the memory associated with a promise.
// Important: This should handle freeing internal callback queues and other resources.
// If the promise is persistent, this might involve deallocation in persistent memory.
void promise_free(Promise* p);


// --- Q.defer() API ---
// Provides a way to create a promise and manually control its resolution or rejection.

// Structure for a deferred object. Contains the promise it controls.
struct PromiseDeferred {
    Promise* promise;
    // Potentially a lock if the deferred object itself needs to be thread-safe
    // for resolve/reject operations from different threads.
    // pthread_mutex_t* lock; // Example
};

// Creates a new deferred object (and its associated promise).
PromiseDeferred* promise_defer_create(void);

// Creates a new deferred object, potentially with persistent backing for its promise.
PromiseDeferred* promise_defer_create_persistent(void* pmem_ctx, pthread_mutex_t* lock);

// Resolves the promise associated with the deferred object.
void promise_defer_resolve(PromiseDeferred* deferred, PromiseValue value);

// Rejects the promise associated with the deferred object.
void promise_defer_reject(PromiseDeferred* deferred, PromiseValue reason);

// Frees the deferred object. Note: This typically also implies freeing the
// associated promise if it's not intended to outlive the deferred controller.
// Careful lifetime management is needed.
void promise_defer_free(PromiseDeferred* deferred);


// --- Q.all() API ---
// Creates a promise that fulfills when all promises in an array are fulfilled,
// or rejects if any promise in the array rejects.

// Args:
//   promises: An array of Promise pointers.
//   count: The number of promises in the array.
// Returns:
//   A new promise that aggregates the results. The fulfillment value will typically
//   be an array of the fulfillment values from the input promises, in order.
Promise* promise_all(Promise* promises[], size_t count);


// --- Q.nfcall() API (Node.js-style callback wrapping) ---
// Wraps a function that uses the Node.js (error, result) callback pattern
// into a function that returns a promise.

// Node.js-style callback signature.
// Args:
//   err: Error object/value (or NULL/0 if success).
//   result: Result value (if success).
//   user_data: Context data.
typedef void (*NodeCallback)(void* err, PromiseValue result, void* user_data);

// Calls the Node.js-style callback function and returns a promise
// that resolves or rejects based on the callback's arguments.
// Args:
//   cb: The Node.js-style callback function to wrap.
//   user_data: User data to pass to the NodeCallback.
//   ... (potentially other arguments to pass to the wrapped function itself,
//        which would require varargs or a more complex wrapper).
// For simplicity, this example assumes 'cb' is called with pre-set arguments
// or 'user_data' is used to convey them. A more complete `nfcall` would
// need to handle forwarding arguments to the target function.
Promise* promise_nfcall(NodeCallback cb, void* user_data /*, ...args for the function itself */);


// --- PMLL Hardened Queue API ---
// Experimental API for operations that need to be resilient, potentially
// involving persistent state or transactional execution.

// Creates a hardened resource queue.
// Args:
//   resource_id: A unique identifier for the resource this queue manages.
//   persistent_queue: If true, the queue's state might be backed by persistent memory.
PMLL_HardenedResourceQueue* pmll_queue_create(const char* resource_id, bool persistent_queue);

// Executes an operation through the hardened queue.
// This implies that the operation might be retried, logged, or handled
// transactionally depending on the queue's implementation.
// Args:
//   hq: The hardened queue instance.
//   operation_fn: The function to execute (similar to on_fulfilled).
//   error_fn: Handler for errors during the operation (similar to on_rejected).
//   op_user_data: User data for the operation and error functions.
// Returns:
//   A promise that settles with the outcome of the hardened operation.
Promise* pmll_execute_hardened_operation(
    PMLL_HardenedResourceQueue* hq,
    on_fulfilled_callback operation_fn, // Or a more specific operation signature
    on_rejected_callback error_fn,     // Or a more specific error signature
    void* op_user_data);

// Frees the hardened resource queue and any associated resources.
void pmll_queue_free(PMLL_HardenedResourceQueue* hq);


// --- Event Loop Simulation (for async behavior) ---
// Promises inherently imply asynchronous behavior. In C, this usually means
// integrating with an existing event loop (libuv, libevent, etc.) or simulating one.
// This is a very basic simulation for demonstration.

// Initializes the event loop (e.g., creates a task queue).
void init_event_loop(void);

// Enqueues a "microtask" to be run by the event loop.
// Promise resolutions and rejections often schedule their callbacks as microtasks.
// Args:
//   task: Function pointer to the task to execute.
//   data: Data to pass to the task function.
void enqueue_microtask(void (*task)(void* data), void* data);

// Runs the event loop until all tasks are processed.
// In a real application, this would be more sophisticated, possibly running
// indefinitely or until a specific stop condition.
void run_event_loop(void);

// Frees any resources associated with the event loop.
void free_event_loop(void);

#endif // QPROMISE_H

// --- Implementation (qpromise.c) ---
#include <stdio.h>   // For printf (debugging), NULL
#include <stdlib.h>  // For malloc, free
#include <string.h>  // For memcpy, memset (potentially)
#include <stdbool.h> // For boolean types
// #include "qpromise.h" // Already included via the combined file structure for this example

// Forward declaration (already in .h but good practice in .c if not including .h directly)
// typedef struct Promise Promise; // Not needed if qpromise.h is included

// --- Core Promise Structures ---

// Enum for Promise States (mirroring JS promise states)
// typedef enum { PROMISE_PENDING, PROMISE_FULFILLED, PROMISE_REJECTED } PromiseState; // Defined in .h

// Generic void pointer for promise value/reason.
// typedef void* PromiseValue; // Defined in .h

// Callback function pointer types
// typedef PromiseValue (*on_fulfilled_callback)(PromiseValue value, void* user_data); // Defined in .h
// typedef PromiseValue (*on_rejected_callback)(PromiseValue reason, void* user_data); // Defined in .h

// Structure to hold a callback and its associated chained promise.
// When a promise settles, its callbacks are invoked, and they affect their chained_promise.
typedef struct {
    on_fulfilled_callback on_fulfilled; // Function to call on fulfillment. NULL if not set.
    on_rejected_callback on_rejected;   // Function to call on rejection. NULL if not set.
    void* user_data;                    // User-specific context for the callback.
    Promise* chained_promise;           // The promise returned by .then() or .catch().
                                        // This promise will be resolved/rejected based on
                                        // the outcome of on_fulfilled/on_rejected.
} PromiseCallbackEntry; // Renamed for clarity from PromiseCallback

// Simple dynamic array for storing callback entries.
// This queue is processed when the promise settles.
typedef struct {
    PromiseCallbackEntry* items; // Dynamically allocated array of callback entries.
    size_t count;                // Number of callback entries currently in the queue.
    size_t capacity;             // Allocated capacity of the items array.
    // For PMLL/persistent scenarios, this queue might also need to be in persistent memory,
    // or its contents carefully managed with respect to transactions if the promise is persistent.
    // pthread_mutex_t* queue_lock; // Optional: if callbacks can be added from multiple threads
                                 // after promise creation but before settlement.
} PromiseCallbackQueue;

// The Promise structure itself
struct Promise {
    PromiseState state; // Current state of the promise (PENDING, FULFILLED, REJECTED).
    PromiseValue value; // Stores the fulfillment value or rejection reason once settled.
                        // If state is PENDING, this is typically NULL or undefined.

    PromiseCallbackQueue fulfillment_callbacks; // Queue of callbacks to execute on fulfillment.
    PromiseCallbackQueue rejection_callbacks;   // Queue of callbacks to execute on rejection.

    pthread_mutex_t lock; // Mutex for thread-safe access to promise state and callback queues.
                          // Essential if promises are resolved/rejected or 'then'ed from
                          // multiple threads. For single-threaded event loops, this might be
                          // optional or a lighter-weight synchronization mechanism could be used.

    bool is_persistent;   // Flag to indicate if this promise is backed by persistent memory.
    void* pmem_ctx;       // Context for persistent memory operations (e.g., a pmemobj_pool*).
                          // If is_persistent is true, 'value' might be a persistent pointer,
                          // and callback queues might also need special handling.

    // Reference counting could be useful for managing promise lifetime, especially
    // in complex chaining scenarios or when promises are shared.
    // int ref_count;
    // pthread_mutex_t ref_count_lock;
};

// --- Helper Functions for Callback Queues (Conceptual moving towards Real) ---

// Initializes a callback queue.
// Real-world: Allocate initial capacity, set count to 0.
static void callback_queue_init(PromiseCallbackQueue* queue, size_t initial_capacity) {
    // Defensive coding: check for NULL queue pointer.
    if (!queue) return;

    queue->count = 0;
    if (initial_capacity > 0) {
        queue->items = (PromiseCallbackEntry*)malloc(initial_capacity * sizeof(PromiseCallbackEntry));
        if (!queue->items) {
            // Allocation failure: a critical error.
            // In a real system, this should be handled robustly (e.g., log, signal error).
            // For this sketch, we'll set capacity to 0 to indicate failure.
            perror("Failed to allocate callback queue items");
            queue->capacity = 0;
            // Potentially, the promise creation itself should fail if its queues can't be initialized.
        } else {
            queue->capacity = initial_capacity;
        }
    } else {
        queue->items = NULL;
        queue->capacity = 0;
    }
    // If this queue were for a persistent promise, 'items' might be allocated in pmem
    // using pmemobj_alloc or similar, and 'queue_lock' might be a persistent mutex.
}

// Adds a callback entry to the queue.
// Real-world: Handle resizing if capacity is reached.
static bool callback_queue_add(PromiseCallbackQueue* queue, PromiseCallbackEntry entry) {
    // Defensive coding: check for NULL queue or uninitialized items (if capacity is 0 but items is NULL).
    if (!queue) return false;

    if (queue->count >= queue->capacity) {
        // Resize needed. Double the capacity, or a more sophisticated growth strategy.
        size_t new_capacity = (queue->capacity == 0) ? 4 : queue->capacity * 2; // Start with 4, then double
        PromiseCallbackEntry* new_items = (PromiseCallbackEntry*)realloc(queue->items, new_capacity * sizeof(PromiseCallbackEntry));
        if (!new_items) {
            perror("Failed to resize callback queue");
            // Failed to add. The caller needs to handle this (e.g., the 'then' operation fails).
            return false;
        }
        queue->items = new_items;
        queue->capacity = new_capacity;
    }

    queue->items[queue->count++] = entry;
    return true;
    // For PMLL/persistent queues:
    // If 'items' is in pmem, realloc might not be directly applicable.
    // A persistent memory allocator would be used (e.g., pmemobj_realloc or manual copy to new larger pmem block).
    // The addition itself might need to be part of a transaction if the promise is persistent
    // to ensure consistency (pmemobj_tx_add_range).
}

// Frees the resources used by a callback queue.
// Real-world: Free the 'items' array.
static void callback_queue_free(PromiseCallbackQueue* queue) {
    if (!queue) return;

    // Important: This frees the queue's internal buffer.
    // It does NOT free the chained_promise within each PromiseCallbackEntry.
    // The lifetime of chained_promises is managed separately (e.g., when they are resolved/rejected and freed).
    if (queue->items) {
        // If using PMLL for the queue items, this would be pmemobj_free(&queue->items_oid) or similar.
        free(queue->items);
        queue->items = NULL;
    }
    queue->count = 0;
    queue->capacity = 0;
}

// --- Forward declarations for internal promise functions ---
static void promise_settle(Promise* p, PromiseState state, PromiseValue value);
static void schedule_callback_execution(Promise* p);
static void execute_callbacks(void* data); // Task for event loop


// --- Promise API Implementation ---

Promise* promise_create_internal(bool is_persistent_promise, void* pmem_ctx_param, pthread_mutex_t* lock_param) {
    Promise* p = NULL;

    // For persistent promises, allocation would use pmemobj_alloc or similar.
    // For this sketch, we'll use malloc and simulate the distinction.
    if (is_persistent_promise && pmem_ctx_param) {
        // p = pmemobj_tx_alloc(sizeof(Promise), TYPE_PROMISE_STRUCT_ID); // Example PMDK
        // if (!p) { perror("Failed to allocate persistent promise"); return NULL; }
        // For now, simulate with malloc and set flags.
        p = (Promise*)malloc(sizeof(Promise));
        if (!p) { perror("Failed to allocate promise (simulating persistent)"); return NULL; }
        p->is_persistent = true;
        p->pmem_ctx = pmem_ctx_param;
        // If a lock is provided for a persistent promise, it's likely for managing
        // concurrent access to the persistent state itself.
        // The internal 'lock' field of the Promise struct is for its own state integrity.
        // This needs careful design: is lock_param for the pmem region or for this promise instance?
        // Assuming lock_param is for this instance if it's to be managed externally.
        // Or, the promise initializes its own persistent mutex if pmem_ctx supports it.
    } else {
        p = (Promise*)malloc(sizeof(Promise));
        if (!p) { perror("Failed to allocate promise"); return NULL; }
        p->is_persistent = false;
        p->pmem_ctx = NULL;
    }

    p->state = PROMISE_PENDING;
    p->value = NULL; // No value/reason when pending.

    // Initialize callback queues.
    // Initial capacity can be tuned. 0 means it allocates on first 'then'.
    callback_queue_init(&p->fulfillment_callbacks, 2);
    callback_queue_init(&p->rejection_callbacks, 2);

    // Initialize the mutex for the promise.
    // For persistent promises, this mutex itself might need to be persistent (e.g., PMDK POBJ_MUTEX_INIT).
    if (pthread_mutex_init(&p->lock, NULL) != 0) {
        perror("Failed to initialize promise mutex");
        // If persistent, pmemobj_free(&p_oid);
        free(p);
        return NULL;
    }
    
    // p->ref_count = 1; // If using reference counting
    // pthread_mutex_init(&p->ref_count_lock, NULL); // If using reference counting

    return p;
}

Promise* promise_create(void) {
    // This is the standard, in-memory promise.
    return promise_create_internal(false, NULL, NULL);
}

Promise* promise_create_persistent(void* pmem_ctx, pthread_mutex_t* lock) {
    // This is the advanced persistent promise.
    // The 'lock' parameter here might be for the overall persistent context or
    // an externally managed lock for this specific promise instance if it's stored
    // in a shared persistent region. The promise's internal lock handles its own data.
    if (!pmem_ctx) {
        // fprintf(stderr, "Persistent context (pmem_ctx) is required for persistent promise.\n");
        return NULL; // Or fallback to non-persistent if allowed by design.
    }
    return promise_create_internal(true, pmem_ctx, lock);
}

// Internal function to actually settle the promise and trigger callbacks.
// This function MUST be called with the promise's lock held.
static void promise_settle_locked(Promise* p, PromiseState new_state, PromiseValue new_value) {
    if (p->state != PROMISE_PENDING) {
        // Promise already settled. Per spec, further resolves/rejects are ignored.
        // If new_value is a promise itself (e.g. promise_resolve(p, another_promise)),
        // and it's dynamically allocated, it might need to be freed here if not adopted.
        // However, typical Q/Promises+ spec doesn't involve freeing values passed to resolve/reject
        // by the resolve/reject functions themselves; caller manages that.
        return;
    }

    p->state = new_state;
    p->value = new_value; // Ownership of value is passed to the promise.

    // For persistent promises, the state and value update need to be made persistent.
    if (p->is_persistent && p->pmem_ctx) {
        // pmemobj_tx_add_range_direct(p, sizeof(PromiseState) + sizeof(PromiseValue)); // Or specific fields
        // pmemobj_persist(p->pmem_ctx, p, sizeof(Promise)); // Or finer-grained flush
        // This is a simplification; actual PMDK usage involves transactions (pmemobj_tx_begin, commit/abort).
        // For example:
        // TX_BEGIN(p->pmem_ctx) {
        //     pmemobj_tx_add_range_direct(p, offsetof(struct Promise, state), sizeof(p->state) + sizeof(p->value) + ...);
        //     p->state = new_state;
        //     p->value = new_value; // If value is also persistent, its pmem representation is assigned.
        // } TX_ONABORT {
        //     fprintf(stderr, "Transaction aborted while settling persistent promise.\n");
        // } TX_END
    }

    // Schedule the execution of callbacks. This is often done via an event loop or microtask queue
    // to ensure asynchronous behavior (callbacks don't run immediately on the caller's stack).
    schedule_callback_execution(p);
}


void promise_resolve(Promise* p, PromiseValue value) {
    if (!p) return;

    // Special case: If 'value' is itself a promise (promiseA.resolve(promiseB)),
    // then promiseA should adopt the state of promiseB. This is part of the Promises/A+ spec (thenables).
    // This C implementation currently doesn't automatically handle `value` being a `Promise*`.
    // A full implementation would check if `value` is a "thenable" (another promise)
    // and if so, chain to it: `return promise_then(value, ...)` essentially.
    // For this sketch, we assume `value` is a final fulfillment value.
    // if (is_promise(value)) { /* complex adoption logic */ }

    pthread_mutex_lock(&p->lock);
    promise_settle_locked(p, PROMISE_FULFILLED, value);
    pthread_mutex_unlock(&p->lock);
}

void promise_reject(Promise* p, PromiseValue reason) {
    if (!p) return;

    pthread_mutex_lock(&p->lock);
    promise_settle_locked(p, PROMISE_REJECTED, reason);
    pthread_mutex_unlock(&p->lock);
}


Promise* promise_then(Promise* p, on_fulfilled_callback on_fulfilled, on_rejected_callback on_rejected, void* user_data) {
    if (!p) return NULL;

    // Create the new promise that will be returned by 'then'.
    // This chained promise's persistence should ideally match the parent,
    // or be explicitly managed. For simplicity, new chained promises are in-memory.
    Promise* chained_promise = promise_create_internal(p->is_persistent, p->pmem_ctx, NULL); // or just promise_create()
    if (!chained_promise) {
        // Failed to create chained promise, cannot proceed.
        return NULL;
    }

    PromiseCallbackEntry entry;
    entry.on_fulfilled = on_fulfilled;
    entry.on_rejected = on_rejected;
    entry.user_data = user_data;
    entry.chained_promise = chained_promise;

    pthread_mutex_lock(&p->lock);

    bool added = false;
    if (p->state == PROMISE_PENDING) {
        // If parent is pending, enqueue the callbacks.
        // The choice of queue depends on whether we have fulfillment/rejection handlers.
        // A single queue is often used, and the handler type is checked during execution.
        // Here, we use separate queues as sketched.
        // A 'then' call always effectively registers for both, even if one handler is NULL.
        // The resolution logic will pick the right one.
        // For simplicity, let's assume if on_fulfilled is provided, it goes to fulfillment_callbacks,
        // and if on_rejected is provided, it goes to rejection_callbacks.
        // A more robust approach might be a single callback queue where each entry
        // stores both, and the execution logic picks the correct one.
        // Or, always add to both and let the NULL check handle it.
        // Let's refine: add to fulfillment_callbacks if on_fulfilled is not NULL,
        // and to rejection_callbacks if on_rejected is not NULL.
        // This is slightly different from JS where a single "reaction record" is typically made.
        // For this C sketch, adding to respective queues is simpler.
        
        // A more common model: a single list of "reactions"
        // For this sketch, we'll add to both if handlers exist.
        // This means a chained promise could be affected by either path if both handlers are given.
        // This is fine, as only one path (fulfill or reject) will be taken by the parent.
        added = callback_queue_add(&p->fulfillment_callbacks, entry); // Add the same entry
        if(added) { // only try to add to second queue if first one succeeded.
             added = callback_queue_add(&p->rejection_callbacks, entry);
        }


    } else {
        // If parent is already settled, schedule immediate execution of the relevant callback.
        // This still needs to be asynchronous to maintain consistency.
        // We directly prepare data for a single callback execution.
        // This is a simplified path; typically, even for already settled promises,
        // the callback execution is deferred to a microtask.
        // For this sketch, we'll use the same schedule_callback_execution mechanism.
        // The execute_callbacks function will then iterate through the (now non-empty) queues.
        // This means we still add to the queue, and then schedule.
        if (p->state == PROMISE_FULFILLED && on_fulfilled) {
            added = callback_queue_add(&p->fulfillment_callbacks, entry);
        } else if (p->state == PROMISE_REJECTED && on_rejected) {
            added = callback_queue_add(&p->rejection_callbacks, entry);
        } else if (p->state == PROMISE_FULFILLED && !on_fulfilled) {
            // No fulfillment handler, but parent fulfilled. Propagate fulfillment to chained promise.
            promise_resolve(chained_promise, p->value); // Directly resolve/reject
            added = true; // conceptually
        } else if (p->state == PROMISE_REJECTED && !on_rejected) {
            // No rejection handler, but parent rejected. Propagate rejection to chained promise.
            promise_reject(chained_promise, p->value); // Directly resolve/reject
            added = true; // conceptually
        }
        
        if (added && p->state != PROMISE_PENDING) {
             // If already settled and callback was relevant (or propagation needed),
             // ensure processing is scheduled.
             schedule_callback_execution(p); // This will pick up the newly added callback for the settled promise
        }
    }
    
    pthread_mutex_unlock(&p->lock);

    if (!added && p->state == PROMISE_PENDING) {
        // If adding callback failed (e.g., out of memory for queue resize)
        // The chained_promise should probably be rejected.
        promise_reject(chained_promise, (void*)"Failed to attach callback"); // Or a proper error value
        // promise_free(chained_promise); // Or let it be freed by its eventual consumer.
        return chained_promise; // Or NULL if we decide 'then' itself failed.
    }


    return chained_promise;
}

void promise_free(Promise* p) {
    if (!p) return;

    // Potential complexities:
    // 1. Reference counting: Decrement ref_count. If 0, then proceed to free.
    //    pthread_mutex_lock(&p->ref_count_lock);
    //    p->ref_count--;
    //    if (p->ref_count > 0) {
    //        pthread_mutex_unlock(&p->ref_count_lock);
    //        return;
    //    }
    //    pthread_mutex_unlock(&p->ref_count_lock);
    //    pthread_mutex_destroy(&p->ref_count_lock);


    // 2. Chained promises: Freeing a promise should not break unresolved chained promises.
    //    The `value` (if it's a pointer to dynamically allocated memory) needs careful ownership rules.
    //    Typically, the promise "owns" its resolved value/reason if it was heap-allocated for it.
    //    If the user provided a pointer, they manage its lifetime unless documented otherwise.
    //    For this sketch, we assume `value` is either not owned or its freeing is handled elsewhere/by type.

    pthread_mutex_destroy(&p->lock);

    // Free the callback queues.
    // This frees the arrays of PromiseCallbackEntry, but not the chained_promise pointers themselves.
    // Chained promises are independent entities and should be freed when they are no longer needed.
    callback_queue_free(&p->fulfillment_callbacks);
    callback_queue_free(&p->rejection_callbacks);

    // If the promise value itself is dynamically allocated and owned by this promise, free it.
    // This requires a convention, e.g., if p->value was malloc'd by promise_resolve/reject.
    // if (p->state != PROMISE_PENDING && p->value_is_owned) { free(p->value); }


    // For persistent promises, deallocation involves pmemobj_free or similar.
    if (p->is_persistent && p->pmem_ctx) {
        // TX_BEGIN(p->pmem_ctx) {
        //     pmemobj_tx_free(pmemobj_oid(p)); // Free the promise struct itself from pmem
        // } TX_ONABORT {
        //     fprintf(stderr, "Transaction aborted while freeing persistent promise.\n");
        // } TX_END
        // For this simulation:
        free(p);
    } else {
        free(p);
    }
}


// --- Event Loop Simulation and Callback Execution ---
// This is a very basic event loop for demonstration.
// A real system would use libuv, libevent, or integrate into an existing application loop.

typedef struct Microtask {
    void (*task_func)(void* data);
    void* task_data;
    struct Microtask* next;
} Microtask;

static Microtask* microtask_queue_head = NULL;
static Microtask* microtask_queue_tail = NULL;
static pthread_mutex_t event_loop_lock = PTHREAD_MUTEX_INITIALIZER;
// static pthread_cond_t event_loop_cond = PTHREAD_COND_INITIALIZER; // For a blocking run_event_loop

void init_event_loop(void) {
    // Already initialized statically or could do dynamic init here.
    // Ensure queue is empty.
    microtask_queue_head = NULL;
    microtask_queue_tail = NULL;
}

void enqueue_microtask(void (*task)(void* data), void* task_data) {
    Microtask* new_task = (Microtask*)malloc(sizeof(Microtask));
    if (!new_task) {
        perror("Failed to allocate microtask");
        // This is a critical failure. In a robust system, might try to recover or log.
        return;
    }
    new_task->task_func = task;
    new_task->task_data = task_data;
    new_task->next = NULL;

    pthread_mutex_lock(&event_loop_lock);
    if (microtask_queue_tail) {
        microtask_queue_tail->next = new_task;
        microtask_queue_tail = new_task;
    } else {
        microtask_queue_head = new_task;
        microtask_queue_tail = new_task;
    }
    // pthread_cond_signal(&event_loop_cond); // Signal if run_event_loop is waiting
    pthread_mutex_unlock(&event_loop_lock);
}

// This function is called when a promise settles to schedule the processing of its callbacks.
static void schedule_callback_execution(Promise* p) {
    // The 'data' for the microtask is the promise itself.
    // The `execute_callbacks` function will then use this promise to find and run its callbacks.
    // We might need to increment a ref count for 'p' if 'execute_callbacks' could outlive
    // the current scope that holds 'p', though typically microtasks are short-lived.
    // For now, assume 'p' remains valid.
    enqueue_microtask(execute_callbacks, p);
}

// This is the function executed by the event loop for a settled promise.
static void execute_callbacks(void* data) {
    Promise* p = (Promise*)data;
    if (!p) return;

    pthread_mutex_lock(&p->lock);

    PromiseCallbackQueue* queue_to_process = NULL;
    if (p->state == PROMISE_FULFILLED) {
        queue_to_process = &p->fulfillment_callbacks;
    } else if (p->state == PROMISE_REJECTED) {
        queue_to_process = &p->rejection_callbacks;
    } else {
        // Should not happen if scheduled correctly (only on settlement)
        pthread_mutex_unlock(&p->lock);
        return;
    }

    // Iterate over a copy of the callbacks or carefully manage the queue.
    // Processing callbacks might add new callbacks to other promises,
    // so the list should be stable during iteration.
    // For simplicity, we iterate and clear. A more robust system might copy.
    // This also means callbacks added *after* settlement but *before* this execution
    // will be processed.
    
    // Create a temporary copy of the relevant callback entries to avoid issues
    // if a callback itself tries to call .then() on the same promise (though unlikely/problematic).
    size_t count = queue_to_process->count;
    PromiseCallbackEntry* entries_to_run = NULL;
    if (count > 0) {
        entries_to_run = (PromiseCallbackEntry*)malloc(count * sizeof(PromiseCallbackEntry));
        if (entries_to_run) {
            memcpy(entries_to_run, queue_to_process->items, count * sizeof(PromiseCallbackEntry));
            // Clear the original queue as these are now being processed.
            // Or, if callbacks are one-shot, remove them after processing.
            // For this model, once a promise is settled, its callback queues are processed once.
            queue_to_process->count = 0; // Effectively clears it for future (though should not be reused for same settlement)
        } else {
            perror("Failed to allocate temporary callback array for execution");
            // Critical: cannot execute callbacks. Chained promises will not settle.
            // Potentially reject all chained promises from this queue.
            for (size_t i = 0; i < queue_to_process->count; ++i) {
                 if (queue_to_process->items[i].chained_promise) {
                    promise_reject(queue_to_process->items[i].chained_promise, (void*)"Internal error during callback processing");
                 }
            }
            queue_to_process->count = 0;
            pthread_mutex_unlock(&p->lock);
            return;
        }
    }


    pthread_mutex_unlock(&p->lock); // Unlock before calling user code (callbacks)

    for (size_t i = 0; i < count; ++i) {
        PromiseCallbackEntry* current_entry = &entries_to_run[i];
        Promise* chained_promise = current_entry->chained_promise;
        PromiseValue callback_result = NULL;
        bool callback_executed = false;

        if (p->state == PROMISE_FULFILLED && current_entry->on_fulfilled) {
            // Try-catch equivalent is harder in C. If on_fulfilled crashes, the loop breaks.
            // A robust system might use setjmp/longjmp or run callbacks in separate threads/processes (heavy).
            callback_result = current_entry->on_fulfilled(p->value, current_entry->user_data);
            callback_executed = true;
        } else if (p->state == PROMISE_REJECTED && current_entry->on_rejected) {
            callback_result = current_entry->on_rejected(p->value, current_entry->user_data);
            callback_executed = true;
        }

        if (chained_promise) {
            if (callback_executed) {
                // If the callback returned another promise, the chained_promise should adopt its state.
                // This is the "Promise Resolution Procedure".
                // For simplicity here, we assume callback_result is a direct value or NULL.
                // A full implementation would check if callback_result is a "thenable" (another promise).
                // if (is_promise(callback_result)) {
                //    promise_then(callback_result, resolve_chained_passthrough, reject_chained_passthrough, chained_promise);
                // } else {
                //    promise_resolve(chained_promise, callback_result);
                // }
                // Simplified: directly resolve the chained promise with the callback's result.
                // If callback_result is intended to be a rejection, it needs a special marker or type.
                // For now, assume on_fulfilled's result fulfills, on_rejected's result also fulfills (recovery).
                // To propagate rejection from on_rejected, it would need to return a new rejected promise or a special error value.
                promise_resolve(chained_promise, callback_result);
            } else {
                // No appropriate callback was executed (e.g., on_fulfilled was NULL for a fulfilled promise).
                // Propagate the original promise's state and value.
                if (p->state == PROMISE_FULFILLED) {
                    promise_resolve(chained_promise, p->value);
                } else { // PROMISE_REJECTED
                    promise_reject(chained_promise, p->value);
                }
            }
        }
        // If callback_result was dynamically allocated by the callback, its ownership needs to be clear.
        // If chained_promise takes ownership (e.g. via promise_resolve), then it's fine.
    }
    if (entries_to_run) {
        free(entries_to_run);
    }
    // Note: The original promise 'p' is not freed here. Its lifetime is independent.
    // Chained promises are now resolved/rejected and will be freed by their consumers or when they are no longer referenced.
}


void run_event_loop(void) {
    // Simple run-once model: process all tasks currently in the queue.
    // A real event loop would often block and wait for new tasks if the queue is empty (using a condition variable).
    while (true) {
        Microtask* task_to_run = NULL;

        pthread_mutex_lock(&event_loop_lock);
        if (microtask_queue_head) {
            task_to_run = microtask_queue_head;
            microtask_queue_head = microtask_queue_head->next;
            if (!microtask_queue_head) {
                microtask_queue_tail = NULL; // Queue is now empty
            }
        }
        pthread_mutex_unlock(&event_loop_lock);

        if (task_to_run) {
            task_to_run->task_func(task_to_run->task_data);
            free(task_to_run);
        } else {
            // No more tasks in the queue.
            break;
        }
    }
}

void free_event_loop(void) {
    // Free any remaining tasks in the queue.
    Microtask* current = microtask_queue_head;
    while (current) {
        Microtask* next = current->next;
        // Potentially, if task_data was allocated and owned by the event loop, free it.
        // Here, task_data (the Promise*) is not owned by the event loop tasks.
        free(current);
        current = next;
    }
    microtask_queue_head = NULL;
    microtask_queue_tail = NULL;
    // pthread_mutex_destroy(&event_loop_lock); // If dynamically allocated
    // pthread_cond_destroy(&event_loop_cond); // If dynamically allocated
}

// --- Q.defer() API Implementation ---
PromiseDeferred* promise_defer_create_internal(bool is_persistent, void* pmem_ctx, pthread_mutex_t* lock) {
    PromiseDeferred* deferred = (PromiseDeferred*)malloc(sizeof(PromiseDeferred));
    if (!deferred) {
        perror("Failed to allocate deferred object");
        return NULL;
    }

    // The promise associated with the deferred object.
    // Its persistence should match the deferred request.
    if (is_persistent) {
        deferred->promise = promise_create_persistent(pmem_ctx, lock);
    } else {
        deferred->promise = promise_create();
    }

    if (!deferred->promise) {
        free(deferred);
        return NULL;
    }
    // If 'deferred' itself needs to be persistent, its allocation would also use pmem.
    return deferred;
}


PromiseDeferred* promise_defer_create(void) {
    return promise_defer_create_internal(false, NULL, NULL);
}

PromiseDeferred* promise_defer_create_persistent(void* pmem_ctx, pthread_mutex_t* lock) {
     if (!pmem_ctx) return NULL;
    return promise_defer_create_internal(true, pmem_ctx, lock);
}

void promise_defer_resolve(PromiseDeferred* deferred, PromiseValue value) {
    if (!deferred || !deferred->promise) return;
    promise_resolve(deferred->promise, value);
}

void promise_defer_reject(PromiseDeferred* deferred, PromiseValue reason) {
    if (!deferred || !deferred->promise) return;
    promise_reject(deferred->promise, reason);
}

void promise_defer_free(PromiseDeferred* deferred) {
    if (!deferred) return;
    // The crucial question: does freeing the deferred object also free its promise?
    // Typically, yes, unless the promise has been "detached" or returned elsewhere
    // and its lifetime is now managed independently.
    // For a simple defer, the promise is intrinsically linked.
    // If promise_free handles ref counting or checks if it's part of a deferred,
    // it might be okay. Otherwise, explicit free here.
    if (deferred->promise) {
        promise_free(deferred->promise); // This assumes deferred owns the promise.
        deferred->promise = NULL;
    }
    free(deferred);
}


// --- Q.all() API Implementation (Sketch) ---
typedef struct {
    Promise* all_promise; // The aggregate promise returned by promise_all
    PromiseValue* results_array; // Array to store results from individual promises
    size_t total_promises;
    size_t fulfilled_count;
    size_t rejected_count; // Only one rejection is enough, but good to track
    pthread_mutex_t lock; // To protect shared counts and results_array
    bool is_persistent_context; // if results_array needs to be in pmem
    void* pmem_ctx;
} PromiseAllContext;

// Callback for individual promises in promise_all
static PromiseValue promise_all_on_fulfilled(PromiseValue value, void* user_data_packed) {
    // user_data_packed contains { PromiseAllContext* context, size_t index }
    // This requires a small struct to be passed as user_data or careful casting.
    // For simplicity, let's assume user_data is PromiseAllContext* and index is implicitly managed
    // or passed via a more complex structure.
    // A simpler way: create a unique fulfillment handler for each promise in the 'all' array.
    // Each handler knows its index.

    // This is a placeholder. A full implementation would be more complex.
    // It needs to:
    // 1. Lock the PromiseAllContext.
    // 2. Store 'value' in context->results_array[index].
    // 3. Increment context->fulfilled_count.
    // 4. If context->fulfilled_count == context->total_promises, resolve context->all_promise with results_array.
    // 5. Unlock.
    // 6. Return NULL (or value, though it's not directly used by a chained promise here).
    PromiseAllContext* context = (PromiseAllContext*) user_data_packed; // Simplified
    // int index = ... ; // This needs to be passed correctly.

    // pthread_mutex_lock(&context->lock);
    // context->results_array[index] = value; // Value ownership needs care
    // context->fulfilled_count++;
    // if (context->fulfilled_count == context->total_promises) {
    //    promise_resolve(context->all_promise, context->results_array); // results_array becomes owned by all_promise
    // }
    // pthread_mutex_unlock(&context->lock);
    return NULL;
}

static PromiseValue promise_all_on_rejected(PromiseValue reason, void* user_data) {
    PromiseAllContext* context = (PromiseAllContext*)user_data;
    // pthread_mutex_lock(&context->lock);
    // if (context->all_promise->state == PROMISE_PENDING) { // Only reject once
    //    promise_reject(context->all_promise, reason); // Reason ownership needs care
    //    context->rejected_count++;
    // }
    // pthread_mutex_unlock(&context->lock);
    // If reason was allocated, and all_promise now owns it, fine. Otherwise, potential leak/double free.
    return NULL; // Indicates this handler doesn't try to recover for the 'all' promise.
}


Promise* promise_all(Promise* promises[], size_t count) {
    if (count == 0) {
        // If the array is empty, Q resolves with an empty array.
        Promise* p = promise_create();
        PromiseValue* empty_array = (PromiseValue*)malloc(0); // Or a static empty array marker
        promise_resolve(p, empty_array);
        return p;
    }

    // PromiseAllContext* context = (PromiseAllContext*)malloc(sizeof(PromiseAllContext));
    // // ... initialize context ...
    // context->all_promise = promise_create(); // Or persistent if source promises are
    // context->results_array = (PromiseValue*)malloc(count * sizeof(PromiseValue));
    // // ... error check mallocs ...
    // pthread_mutex_init(&context->lock, NULL);

    // for (size_t i = 0; i < count; ++i) {
    //    // Create unique user_data for each if index is needed, or pass context and index.
    //    // For simplicity, assume promise_all_on_fulfilled can get its index or context is enough.
    //    promise_then(promises[i], promise_all_on_fulfilled, promise_all_on_rejected, context);
    // }
    // return context->all_promise;
    // TODO: Full implementation is non-trivial due to concurrent updates and managing the results array.
    // Memory management for results_array and individual results also needs care.
    // If any promise in the input `promises` array is persistent, the `all_promise` and its
    // `results_array` might also need to be persistent.
    fprintf(stderr, "promise_all is not fully implemented in this sketch.\n");
    return promise_create(); // Return a pending promise as a placeholder
}


// --- Q.nfcall() API Implementation (Sketch) ---
typedef struct {
    PromiseDeferred* deferred;
    // Any other context needed by the NodeCallback wrapper
} NFCALL_Context;

// This is the actual callback that will be passed to the Node.js-style function
static void nfcall_trampoline_callback(void* err, PromiseValue result, void* user_data) {
    NFCALL_Context* context = (NFCALL_Context*)user_data;
    if (!context || !context->deferred) return;

    if (err) {
        promise_defer_reject(context->deferred, err); // 'err' becomes the rejection reason
    } else {
        promise_defer_resolve(context->deferred, result); // 'result' becomes the fulfillment value
    }
    // The NFCALL_Context might need to be freed here if it was allocated per call.
    // free(context); // If context was dynamically allocated for this specific nfcall.
}

Promise* promise_nfcall(NodeCallback node_fn_to_wrap, void* user_data_for_node_fn /*, ...args for node_fn_to_wrap */) {
    // This sketch assumes node_fn_to_wrap takes its arguments directly or via user_data_for_node_fn.
    // A full nfcall would use varargs to capture arguments for node_fn_to_wrap.

    PromiseDeferred* deferred = promise_defer_create(); // Or persistent if context demands
    if (!deferred) return NULL; // Or return a pre-rejected promise

    // The context for our trampoline.
    // This might be allocated on the stack if nfcall blocks until node_fn_to_wrap calls back (synchronous nfcall),
    // or heap-allocated if node_fn_to_wrap is asynchronous. Assuming async.
    NFCALL_Context* trampoline_context = (NFCALL_Context*)malloc(sizeof(NFCALL_Context));
    if (!trampoline_context) {
        promise_defer_reject(deferred, (void*)"Failed to allocate nfcall context");
        promise_defer_free(deferred); // Free the deferred as we can't proceed.
        return NULL; // Or return the rejected promise from deferred.promise.
    }
    trampoline_context->deferred = deferred;

    // Call the Node.js-style function, passing our trampoline and its context.
    // The `user_data_for_node_fn` is the original user_data intended for the wrapped Node function.
    // The `trampoline_context` is for our nfcall_trampoline_callback.
    // A typical Node function signature is `void func(arg1, arg2, ..., callback_fn, callback_user_data)`
    // This sketch is simplified. A real nfcall needs to handle how arguments are passed to `node_fn_to_wrap`.
    // For example, if node_fn_to_wrap is `void my_node_func(int a, int b, NodeCallback cb, void* cb_data)`
    // then nfcall would be `promise_nfcall(my_node_func, cb_data_for_my_node_func, a, b)`.
    
    // Simplified call:
    // node_fn_to_wrap(some_error_ptr, some_result_ptr, nfcall_trampoline_callback, trampoline_context);
    // This implies node_fn_to_wrap is called immediately and its callback is also immediate.
    // More realistically, user_data_for_node_fn would be passed to node_fn_to_wrap,
    // and node_fn_to_wrap itself would be invoked.
    // Example: if node_fn_to_wrap is like fs.readFile(path, options, callback)
    // node_fn_to_wrap(path_arg, options_arg, nfcall_trampoline_callback, trampoline_context);

    // This part is highly dependent on how `node_fn_to_wrap` is actually invoked with its own arguments.
    // The current `NodeCallback` signature `void (NodeCallback)(void* err, PromiseValue result, void* user_data)`
    // suggests `node_fn_to_wrap` itself IS the function that will eventually call our trampoline.
    // So, we call `node_fn_to_wrap` and it will, at some point, call `nfcall_trampoline_callback`.
    
    // Let's assume node_fn_to_wrap is a function pointer that we call, and it will invoke our trampoline.
    // The `user_data_for_node_fn` is passed as the user_data TO `node_fn_to_wrap` when it calls our trampoline.
    // This seems a bit mixed up. Let's clarify:
    // `NodeCallback cb` in `promise_nfcall` is the function like `fs.readFile`.
    // `promise_nfcall` needs to *call* `cb`, providing `nfcall_trampoline_callback` as *its* callback.
    
    // This means `NodeCallback` should be redefined or `promise_nfcall` needs more args.
    // Let's assume `NodeCallback` is `typedef void (*GenericNodeFunction)(..., ActualNodeCallback cb, void* cb_user_data);`
    // This is getting too complex for a sketch without varargs.
    //
    // Simpler interpretation: `cb` is a function that takes `(err, result, user_data)`
    // and `promise_nfcall` sets up a context where `cb` is called, and `cb`'s results
    // are used to settle the promise. This is more like `Q.nfapply`.
    //
    // For Q.nfcall(fn, ...args):
    // `fn` is `void actual_node_function(arg1, arg2, ..., NodeCallback cb_for_actual, void* user_data_for_cb_for_actual)`
    // `promise_nfcall` would call `actual_node_function(arg1, arg2, ..., nfcall_trampoline_callback, trampoline_context)`
    //
    // The current signature `Promise* promise_nfcall(NodeCallback cb, void* user_data)`
    // implies `cb` is ALREADY the final callback. This is not how nfcall usually works.
    // It should take the function-to-be-called-with-a-node-style-callback.
    //
    // Re-interpreting based on header: `NodeCallback cb` *is* the function that *we provide*
    // to some other operation, and `user_data` is for that `cb`.
    // This is confusing. Let's assume the header's `Q.nfcall()` means:
    // "I have a function `do_async_op(params, node_style_callback, user_data_for_callback)`
    //  Wrap the call to `do_async_op`."
    //
    //  `Promise* promise_nfcall(do_async_op_ptr_type do_async_op, params_for_op, user_data_for_trampoline)`
    // This structure is not matching.
    //
    // Sticking to the provided header's `promise_nfcall(NodeCallback cb, void* user_data)`:
    // This implies `cb` is something that will be invoked (e.g., by an external event)
    // and `user_data` is what will be passed to it.
    // And we want a promise that reflects `cb`'s invocation.
    // This is more like "promisifying an event handler registration".
    // This is likely not the standard Q.nfcall.
    //
    // Assuming a more standard `nfcall` where we *call* a node-style function:
    // The `NodeCallback` in the header is the `fs.readFile` like function.
    // This means `promise_nfcall` needs to take varargs for the arguments to `NodeCallback`.
    // `typedef void (NodeFunctionToBeCalled)(arg1, arg2, ..., NodeStyleCallback actual_cb, void* actual_cb_data);`
    // `Promise* promise_nfcall(NodeFunctionToBeCalled* node_func, ...args_for_node_func...)`
    //
    // Given the current simple signature, this function is hard to implement meaningfully
    // as standard `nfcall`. It might be intended for a different pattern.
    // For now, returning a pending promise and noting this requires clarification.
    fprintf(stderr, "promise_nfcall requires a clearer signature or varargs for full Node.js style wrapping. Returning pending promise.\n");
    // If it were to proceed:
    // some_external_system_calls_this(cb, user_data, nfcall_trampoline_callback, trampoline_context);
    // where `cb` is the user's function, `user_data` is for it.
    // This is still not quite right.

    // Let's assume `cb` is a function that *we* call, and it expects a node-style callback.
    // This is still not `nfcall`. `nfcall` is `Q.denodeify(nodeStyleFunction)(...args)`.
    //
    // Final attempt at interpreting the header's `promise_nfcall(NodeCallback cb, void* user_data)`:
    // `cb` is a function pointer of type `void (ActualNodeFunction)(void* err, PromiseValue result, void* user_data_for_actual_node_function)`
    // `user_data` is the `user_data_for_actual_node_function`.
    // `promise_nfcall` will execute `cb` (perhaps with some preset err/result for testing, or this is wrong).
    // This interpretation doesn't make sense for wrapping an existing node-style function.
    //
    // The most plausible interpretation for a simple sketch without varargs:
    // `cb` is a function that takes `(NodeCallback actual_final_cb, void* context_for_actual_final_cb)`
    // And `user_data` is for `cb` itself.
    // `cb(nfcall_trampoline_callback, trampoline_context);`
    // This is what `Q.nfcall(someFunctionThatTakesACallback, arg1, arg2)` would do.
    // The `NodeCallback cb` in the signature of `promise_nfcall` would be `someFunctionThatTakesACallback`.
    // Its arguments `arg1, arg2` are missing.
    //
    // This function as defined is difficult to make robust without varargs or a clearer convention.
    // For the purpose of this sketch, we'll assume it's a placeholder or simplified context.
    // The `trampoline_context` would be freed inside `nfcall_trampoline_callback` after the deferred is settled.
    return deferred->promise;
}


// --- PMLL Hardened Queue API (Sketch) ---
struct PMLL_HardenedResourceQueue {
    const char* resource_id;
    bool persistent_queue_flag;
    void* pmem_pool; // If persistent
    // Internal queue for operations, logs, etc.
    // pthread_mutex_t lock;
    // ... other queue-specific data ...
};

PMLL_HardenedResourceQueue* pmll_queue_create(const char* resource_id, bool persistent_queue) {
    // PMLL_HardenedResourceQueue* hq = (PMLL_HardenedResourceQueue*)malloc(sizeof(PMLL_HardenedResourceQueue));
    // ... initialize ...
    // If persistent, allocate hq in pmem, initialize pmem_pool, persistent locks, etc.
    fprintf(stderr, "pmll_queue_create is not fully implemented in this sketch.\n");
    return NULL;
}

Promise* pmll_execute_hardened_operation(
    PMLL_HardenedResourceQueue* hq,
    on_fulfilled_callback operation_fn,
    on_rejected_callback error_fn,
    void* op_user_data) {
    // 1. Create a deferred object for the promise to be returned.
    // 2. Package the operation (op_fn, error_fn, user_data) into a task.
    // 3. Enqueue this task into the hq's internal queue (potentially persistent).
    // 4. The hq's worker mechanism would pick up tasks, execute operation_fn.
    //    - On success, resolve the deferred with result.
    //    - On failure, call error_fn. If error_fn recovers, resolve. Else, reject.
    //    - Handle retries, logging, transactions as per hq's design.
    // This is a complex component.
    fprintf(stderr, "pmll_execute_hardened_operation is not fully implemented.\n");
    PromiseDeferred* deferred = promise_defer_create(); // Or persistent if hq is
    if (!deferred) return NULL; // Or return pre-rejected
    // For now, immediately reject as it's not implemented.
    promise_defer_reject(deferred, (void*)"Hardened operation not implemented");
    return deferred->promise;
}

void pmll_queue_free(PMLL_HardenedResourceQueue* hq) {
    // Free resources, close pmem pool if open, etc.
    // free(hq);
    fprintf(stderr, "pmll_queue_free is not fully implemented.\n");
}

/*
Further Elaboration Points for Real-World Implementation:

1.  Error Handling and Propagation:
    * Robust error checking for all allocations (malloc, realloc, pmem equivalents).
    * Define specific error types/codes for PromiseValue when rejecting, rather than just `void*`.
        This could be a struct `ErrorValue { int code; char* message; }`.
    * Consider how `on_rejected` handlers can "throw" new errors (i.e., reject their chained promise
        with a new reason). This might involve returning a specially recognized error structure
        or a new, readily rejected promise from the callback.

2.  Type Safety for PromiseValue:
    * `void*` is highly flexible but unsafe. For specific applications, using tagged unions
        or a common base struct for promise values could improve type safety.
    * If values are dynamically allocated, clear ownership rules are essential: who allocates, who frees?
        Typically, if a promise callback allocates a value it returns, the promise system might take
        ownership if that value is used to resolve a chained promise.

3.  Thread Safety and Concurrency:
    * The provided `pthread_mutex_t lock` in `struct Promise` is a good start. Ensure all
        accesses to `state`, `value`, and callback queues are protected by this lock.
    * `promise_settle_locked` is a good pattern.
    * The event loop (`enqueue_microtask`, `run_event_loop`) also needs to be thread-safe if tasks
        can be enqueued from multiple threads. `event_loop_lock` handles this.
    * Consider if `user_data` passed to callbacks needs protection if accessed by multiple callbacks
        concurrently (though typically a promise settles once, so its callbacks run sequentially,
        but different promises' callbacks might run interleaved by the event loop).

4.  Promise Resolution Procedure (for `promise_then` and `promise_resolve(p, other_promise)`):
    * A critical part of Promises/A+ is how `then` handles return values from callbacks, especially
        if a callback returns another promise (a "thenable"). The chained promise must adopt the
        state of the returned promise.
    * `promise_resolve(p, x)`: If `x` is a promise, `p` should adopt `x`'s state. This involves
        calling `promise_then` on `x` to link `p`'s resolution to `x`'s settlement.
    * This logic can be complex, involving checks for `x == p` (type error), and careful handling
        to avoid infinite recursion if `x` is a thenable that resolves to itself.

5.  Persistent Memory (PMLL) Integration:
    * Allocator: Use PMDK's `pmemobj_alloc`, `pmemobj_tx_alloc` for all promise structures,
        callback queues, and potentially values if they need to be persistent.
    * Pointers: Convert between `PMEMoid` (PMDK's persistent pointer) and `void*` using
        `pmemobj_direct()`. All persistent pointers within structs must be `PMEMoid`.
    * Transactions: Wrap state modifications (e.g., in `promise_settle_locked`, `callback_queue_add`
        for persistent queues) in PMDK transactions (`TX_BEGIN`, `TX_ADD`, `TX_COMMIT`, `TX_END`)
        to ensure atomicity and consistency in case of crashes.
    * Root Object: A persistent promise system needs a root object in the pmem pool to locate
        existing promises after a restart.
    * Mutexes: Use persistent mutexes (`pthread_mutex_t` stored in pmem, initialized with
        `POBJ_MUTEX_INIT` or similar, if using PMDK's helpers) for persistent data.

6.  Event Loop Integration:
    * The sketched event loop is basic. In a real application, integrate with an existing
        event loop like libuv (used by Node.js), libevent, or a custom application-specific loop.
    * This involves posting the `execute_callbacks` function (or a wrapper) to that event loop's
        task queue.
    * Consider different task priorities (e.g., microtasks for promise jobs vs. macrotasks for I/O).

7.  Q API Completeness:
    * `Q.allSettled()`: Similar to `all`, but waits for all promises to settle (fulfill or reject)
        and returns an array of result objects describing the outcome of each.
    * `Q.race()`: Fulfills or rejects as soon as one of the promises in an iterable fulfills or rejects.
    * `Q.any()`: Fulfills as soon as one of the promises fulfills. Rejects if all reject.
    * `Q.delay()`: Returns a promise that fulfills after a specified delay.
    * `Q.timeout()`: Rejects a promise if it doesn't settle within a given time.
    * `finally(callback)` or `fin(callback)`: Registers a callback to be called when a promise
        is settled (either fulfilled or rejected).

8.  Memory Management:
    * Reference counting for promises can simplify lifetime management, especially in complex graphs
        of chained promises. `promise_free` would decrement, and free only when count reaches zero.
    * Clear rules for `PromiseValue` ownership: if a promise is fulfilled with dynamically allocated
        data, who is responsible for freeing it? Does the promise take ownership?
    * Callback data (`user_data`): The promise library generally shouldn't manage the lifetime of
        `user_data`; the user who provides it is responsible.

9.  Testing:
    * Thorough unit tests are crucial, covering all states, transitions, callback behaviors,
        error conditions, and asynchronous interactions.
    * The Promises/A+ compliance test suite can be adapted if aiming for that level of compatibility.

10. Documentation:
    * Clear API documentation explaining function behaviors, ownership rules, thread safety
        guarantees, and how to integrate with an event loop.
*/

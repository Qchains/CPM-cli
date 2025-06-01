# Q Promises in C

## Overview

CPM implements a complete Q-style promises library in C, bringing asynchronous programming patterns from JavaScript to C development.

## Core Concepts

### Promise States

```c
typedef enum {
    PROMISE_PENDING,    // Promise is not yet settled
    PROMISE_FULFILLED,  // Promise resolved successfully
    PROMISE_REJECTED    // Promise was rejected with an error
} PromiseState;
```

### Basic Usage

```c
#include <cpm_promise.h>

// Create a promise
Promise* p = promise_create();

// Attach handlers
Promise* result = promise_then(p, 
    my_success_handler,    // Called on fulfillment
    my_error_handler,      // Called on rejection
    user_data             // Context data
);

// Resolve the promise
promise_resolve(p, some_value);
```

### Callback Signatures

```c
typedef PromiseValue (*on_fulfilled_callback)(PromiseValue value, void* user_data);
typedef PromiseValue (*on_rejected_callback)(PromiseValue reason, void* user_data);
```

## Advanced Features

### Q.defer() Pattern

```c
// Create a deferred object
PromiseDeferred* deferred = promise_defer_create();

// Get the associated promise
Promise* promise = promise_defer_get_promise(deferred);

// Later, resolve or reject
promise_defer_resolve(deferred, result);
// or
promise_defer_reject(deferred, error);
```

### Q.all() - Promise Aggregation

```c
Promise* promises[] = {promise1, promise2, promise3};
Promise* all_promise = promise_all(promises, 3);

// Resolves when all promises resolve, rejects on first rejection
promise_then(all_promise, handle_all_success, handle_any_failure, NULL);
```

### Promise Chaining

```c
Promise* chain = promise_then(initial_promise, step1_handler, error_handler, NULL);
chain = promise_then(chain, step2_handler, error_handler, NULL);
chain = promise_then(chain, final_handler, error_handler, NULL);
```

## Memory Management

### Ownership Rules

1. **Promise Creation**: Caller owns the returned promise
2. **Promise Values**: Clear ownership transfer rules
3. **Chained Promises**: Automatic cleanup when chain completes
4. **User Data**: Caller manages lifecycle

### Example with Cleanup

```c
PromiseValue my_handler(PromiseValue value, void* user_data) {
    char* input = (char*)value;
    char* output = malloc(strlen(input) + 10);
    sprintf(output, "Processed: %s", input);
    
    // Caller takes ownership of input
    free(input);
    
    // Return new value (ownership transfers to next handler)
    return output;
}
```

## PMLL Integration

### Persistent Promises

```c
// Create promise backed by persistent memory
Promise* persistent_promise = promise_create_persistent(pmem_ctx, &lock);

// Automatic persistence of state changes
promise_resolve(persistent_promise, value);  // State persisted to PMEM
```

### Hardened Operations

```c
// Create hardened queue for file operations
PMLL_HardenedResourceQueue* queue = pmll_queue_create("file_ops", true, pmem_ctx);

// Execute operation with race condition protection
Promise* result = pmll_execute_hardened_operation(queue, my_file_op, error_handler, data);
```

## Error Handling

### Rejection Propagation

```c
PromiseValue error_handler(PromiseValue reason, void* user_data) {
    char* error_msg = (char*)reason;
    printf("Error: %s\n", error_msg);
    
    // Option 1: Handle error and recover
    return recovery_value;
    
    // Option 2: Re-throw error
    // return promise_reject_reason(reason);
}
```

### Exception Safety

- All promise operations are exception-safe
- Memory leaks prevented through careful ownership management
- RAII-style cleanup where possible

## Performance Considerations

### Event Loop Integration

```c
// Initialize event loop
cpm_event_loop_init();

// Process pending promises
while (cpm_event_loop_run_once()) {
    // Handle other work
}

// Cleanup
cpm_event_loop_shutdown();
```

### Memory Pooling

- Promise objects pooled for efficiency
- Callback structures reused
- Configurable pool sizes

## Best Practices

1. **Always handle rejections** - Unhandled rejections can cause resource leaks
2. **Clear ownership** - Document who owns promise values
3. **Use PMLL for shared resources** - Prevent race conditions
4. **Proper cleanup** - Free promises when no longer needed
5. **Error propagation** - Let errors bubble up the chain appropriately
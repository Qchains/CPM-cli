# PMLL Hardening Design

## Overview

PMLL (Persistent Memory Lock Library) provides race condition prevention and data integrity guarantees for CPM operations through serialized operation queues and persistent memory integration.

## Core Concepts

### Hardened Resource Queues

Each shared resource (files, registries, caches) gets its own serialized operation queue:

```c
PMLL_HardenedResourceQueue* file_queue = pmll_queue_create("package_cache", true, pmem_ctx);
```

### Operation Serialization

All operations on a resource are serialized through promise chains:

```c
Promise* op1 = pmll_execute_hardened_operation(queue, download_op, error_handler, data1);
Promise* op2 = pmll_execute_hardened_operation(queue, extract_op, error_handler, data2);
Promise* op3 = pmll_execute_hardened_operation(queue, install_op, error_handler, data3);

// Operations execute in order: op1 -> op2 -> op3
```

## PMDK Integration

### Persistent Memory Pools

```c
PMEMContextHandle* ctx = pmem_context_create("/mnt/pmem/cpm_pool", 1GB, true);
```

### Persistent State

Critical state is stored in persistent memory:
- Package installation status
- Dependency resolution results
- Build artifacts metadata
- Configuration changes

### Crash Recovery

```c
// On startup, recover persistent state
PMEMContextHandle* ctx = pmem_context_open("/mnt/pmem/cpm_pool");
if (ctx) {
    // Recover incomplete operations
    pmll_queue_recover_operations(queue, ctx);
}
```

## Race Condition Prevention

### File System Operations

```c
typedef struct {
    const char* file_path;
    const char* content;
    PromiseDeferred* completion;
} WriteFileOp;

PromiseValue write_file_operation(PromiseValue prev_result, void* user_data) {
    WriteFileOp* op = (WriteFileOp*)user_data;
    
    // This runs exclusively - no other file ops can interfere
    FILE* f = fopen(op->file_path, "w");
    if (f) {
        fprintf(f, "%s", op->content);
        fclose(f);
        promise_defer_resolve(op->completion, "success");
    } else {
        promise_defer_reject(op->completion, "file_error");
    }
    
    return "write_completed";
}
```

### Network Operations

```c
// All downloads for a package are serialized
Promise* download1 = pmll_execute_hardened_operation(download_queue, 
                                                    download_package_op, 
                                                    NULL, pkg1);
Promise* download2 = pmll_execute_hardened_operation(download_queue,
                                                    download_package_op,
                                                    NULL, pkg2);
```

## Locking Mechanisms

### PMLL Locks

```c
PMLL_Lock cache_lock;
pmll_lock_init(&cache_lock, "package_cache", true, pmem_ctx);

// Acquire with timeout
if (pmll_lock_acquire(&cache_lock, 5000) == CPM_RESULT_SUCCESS) {
    // Critical section - modify cache
    pmll_lock_release(&cache_lock);
}
```

### Lock Hierarchies

To prevent deadlocks, locks are acquired in fixed order:
1. Global registry lock
2. Package-specific locks  
3. File system locks
4. Network locks

## Transaction Support

### Atomic Operations

```c
PMLL_Transaction* tx = pmll_transaction_begin(pmem_ctx);

// All operations within transaction are atomic
pmem_write(tx, &package_state, new_state);
pmem_write(tx, &dependency_graph, new_graph);

if (validation_success) {
    pmll_transaction_commit(tx);
} else {
    pmll_transaction_abort(tx);  // All changes rolled back
}
```

### Rollback Scenarios

1. **Package installation failure** - Rollback partial installs
2. **Dependency conflicts** - Restore previous state
3. **Build failures** - Clean up build artifacts
4. **Network timeouts** - Retry with backoff

## Performance Optimization

### Batching Operations

```c
// Batch multiple file operations
BatchOperation batch[] = {
    {WRITE_FILE, "file1.txt", data1},
    {WRITE_FILE, "file2.txt", data2},
    {DELETE_FILE, "old_file.txt", NULL}
};

Promise* batch_result = pmll_execute_batch_operation(queue, batch, 3);
```

### Parallel Queues

Different resource types use separate queues for parallelism:

```c
PMLL_HardenedResourceQueue* file_queue = pmll_queue_create("files", true, ctx);
PMLL_HardenedResourceQueue* net_queue = pmll_queue_create("network", false, NULL);

// These can run in parallel
Promise* file_op = pmll_execute_hardened_operation(file_queue, file_work, NULL, data);
Promise* net_op = pmll_execute_hardened_operation(net_queue, net_work, NULL, data);
```

## Error Recovery

### Operation Replay

Failed operations are automatically retried with exponential backoff:

```c
typedef struct {
    int retry_count;
    int max_retries;
    int base_delay_ms;
    PromiseDeferred* final_result;
} RetryContext;
```

### Consistency Checks

```c
// Verify package installation integrity
Promise* verify = pmll_execute_hardened_operation(verify_queue,
                                                 verify_package_integrity,
                                                 corruption_handler,
                                                 package);
```

## Monitoring and Diagnostics

### Queue Statistics

```c
uint64_t completed, failed, pending;
pmll_queue_get_stats(queue, &completed, &failed, &pending);

printf("Queue stats: %lu completed, %lu failed, %lu pending\n", 
       completed, failed, pending);
```

### Performance Metrics

- Operation latency tracking
- Lock contention detection
- Persistent memory usage monitoring
- Queue depth analysis

## Configuration

### PMLL Settings

```c
CPM_Config config = {
    .pmll_enabled = true,
    .pmll_pool_size = 16,
    .promise_pool_size = 64,
    .timeout_ms = 30000,
    .max_retries = 3
};
```

### Persistent Memory Configuration

- Pool size allocation
- Recovery policies
- Consistency check frequency
- Backup strategies

## Security Considerations

- **Signature verification** before persistent storage
- **Checksum validation** for all operations
- **Access control** for sensitive operations
- **Audit logging** of all hardened operations
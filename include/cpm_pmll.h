/*
 * File: include/cpm_pmll.h
 * Description: PMLL (Persistent Memory Lock Library) API for CPM
 * Provides hardened queues for serializing operations and preventing race conditions
 * Author: Dr. Q Josef Kurk Edwards
 */

#ifndef CPM_PMLL_H
#define CPM_PMLL_H

#include "cpm_types.h"
#include "cpm_promise.h"
#include <pthread.h>
#include <libpmem.h>
#include <libpmemobj.h>

// --- PMDK Integration ---
typedef PMEMobjpool* PMEMContextHandle;
typedef pthread_mutex_t PMLL_Lock;

// --- Persistent Memory Pool Structure ---
typedef struct {
    char magic[16];           // Magic string for validation
    uint64_t version;         // Pool version
    uint64_t queue_count;     // Number of active queues
    uint64_t operation_count; // Total operations processed
    char pool_name[256];      // Pool identifier
    time_t created_at;        // Creation timestamp
} PMLL_PoolHeader;

// --- Hardened Resource Queue ---
struct PMLL_HardenedResourceQueue {
    char* resource_id;
    Promise* operation_queue_promise;
    PMLL_Lock queue_lock;
    PMEMContextHandle pmem_pool;
    void* pmem_queue_ctx;
    
    // Statistics
    uint64_t operations_processed;
    uint64_t operations_failed;
    time_t last_operation_time;
    
    // Configuration
    bool persistent_enabled;
    size_t max_queue_size;
    int timeout_ms;
};

// --- Operation Context ---
typedef struct {
    on_fulfilled_callback user_op_fn;
    on_rejected_callback error_fn;
    void* user_op_data;
    PromiseDeferred* specific_deferred;
    PMEMContextHandle pmem_op_ctx;
    
    // Operation metadata
    char operation_id[64];
    time_t start_time;
    int priority;
} HardenedOpWrapperData;

// --- PMLL Initialization ---

/**
 * @brief Initializes the PMLL subsystem
 * @param pool_path Path to persistent memory pool
 * @param pool_size Size of memory pool in bytes
 * @return CPM_RESULT_SUCCESS on success
 */
CPM_Result pmll_init(const char* pool_path, size_t pool_size);

/**
 * @brief Cleans up PMLL resources
 */
void pmll_cleanup(void);

/**
 * @brief Gets the global persistent memory pool
 * @return Persistent memory pool handle
 */
PMEMContextHandle pmll_get_global_pool(void);

// --- Hardened Queue Management ---

/**
 * @brief Creates a new hardened resource queue
 * @param resource_id Unique identifier for the resource
 * @param persistent_queue Whether to use persistent memory
 * @return Pointer to hardened queue or NULL on failure
 */
PMLL_HardenedResourceQueue* pmll_queue_create(const char* resource_id, bool persistent_queue);

/**
 * @brief Executes an operation on the hardened queue
 * @param hq Hardened queue
 * @param operation_fn Operation function to execute
 * @param error_fn Error handler function
 * @param op_user_data User data for operation
 * @return Promise that resolves with operation result
 */
Promise* pmll_execute_hardened_operation(
    PMLL_HardenedResourceQueue* hq,
    on_fulfilled_callback operation_fn,
    on_rejected_callback error_fn,
    void* op_user_data
);

/**
 * @brief Executes an operation with priority
 * @param hq Hardened queue
 * @param operation_fn Operation function
 * @param error_fn Error handler
 * @param op_user_data User data
 * @param priority Operation priority (higher = more urgent)
 * @return Promise that resolves with operation result
 */
Promise* pmll_execute_prioritized_operation(
    PMLL_HardenedResourceQueue* hq,
    on_fulfilled_callback operation_fn,
    on_rejected_callback error_fn,
    void* op_user_data,
    int priority
);

/**
 * @brief Waits for all pending operations to complete
 * @param hq Hardened queue
 * @param timeout_ms Timeout in milliseconds
 * @return CPM_RESULT_SUCCESS if all operations completed
 */
CPM_Result pmll_queue_flush(PMLL_HardenedResourceQueue* hq, int timeout_ms);

/**
 * @brief Gets queue statistics
 * @param hq Hardened queue
 * @param ops_processed Output: operations processed
 * @param ops_failed Output: operations failed
 * @param queue_size Output: current queue size
 */
void pmll_queue_get_stats(PMLL_HardenedResourceQueue* hq, 
                         uint64_t* ops_processed, 
                         uint64_t* ops_failed, 
                         size_t* queue_size);

/**
 * @brief Frees a hardened queue
 * @param hq Hardened queue to free
 */
void pmll_queue_free(PMLL_HardenedResourceQueue* hq);

// --- Persistent Memory Utilities ---

/**
 * @brief Persists data to persistent memory
 * @param pmem_ctx Persistent memory context
 * @param data Data to persist
 * @param size Size of data in bytes
 * @return CPM_RESULT_SUCCESS on success
 */
CPM_Result pmll_persist_data(PMEMContextHandle pmem_ctx, const void* data, size_t size);

/**
 * @brief Reads data from persistent memory
 * @param pmem_ctx Persistent memory context
 * @param offset Offset in persistent memory
 * @param data Output buffer
 * @param size Size to read
 * @return CPM_RESULT_SUCCESS on success
 */
CPM_Result pmll_read_data(PMEMContextHandle pmem_ctx, size_t offset, void* data, size_t size);

/**
 * @brief Creates a persistent memory transaction
 * @param pmem_ctx Persistent memory context
 * @param transaction_fn Function to execute in transaction
 * @param user_data User data passed to function
 * @return CPM_RESULT_SUCCESS on success
 */
CPM_Result pmll_execute_transaction(PMEMContextHandle pmem_ctx, 
                                   CPM_Result (*transaction_fn)(void* user_data),
                                   void* user_data);

// --- Lock Management ---

/**
 * @brief Initializes a PMLL lock
 * @param lock Lock to initialize
 * @return CPM_RESULT_SUCCESS on success
 */
CPM_Result pmll_lock_init(PMLL_Lock* lock);

/**
 * @brief Acquires a PMLL lock with timeout
 * @param lock Lock to acquire
 * @param timeout_ms Timeout in milliseconds
 * @return CPM_RESULT_SUCCESS if acquired
 */
CPM_Result pmll_lock_acquire_timeout(PMLL_Lock* lock, int timeout_ms);

/**
 * @brief Releases a PMLL lock
 * @param lock Lock to release
 */
void pmll_lock_release(PMLL_Lock* lock);

/**
 * @brief Destroys a PMLL lock
 * @param lock Lock to destroy
 */
void pmll_lock_destroy(PMLL_Lock* lock);

// --- Hardening Utilities ---

/**
 * @brief Creates a hardened promise for file operations
 * @param file_path Path to file
 * @return Promise for file operation
 */
Promise* pmll_create_file_operation_promise(const char* file_path);

/**
 * @brief Creates a hardened promise for network operations
 * @param url URL for network operation
 * @return Promise for network operation
 */
Promise* pmll_create_network_operation_promise(const char* url);

/**
 * @brief Validates persistent memory integrity
 * @param pmem_ctx Persistent memory context
 * @return true if integrity check passes
 */
bool pmll_validate_integrity(PMEMContextHandle pmem_ctx);

#endif // CPM_PMLL_H
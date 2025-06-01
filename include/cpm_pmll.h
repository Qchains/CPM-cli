/*
 * File: include/cpm_pmll.h  
 * Description: PMLL (Persistent Memory Lock Library) hardened queue API
 * Author: Dr. Q Josef Kurk Edwards
 */

#ifndef CPM_PMLL_H
#define CPM_PMLL_H

#include "cpm_types.h"
#include "cpm_promise.h"
#include <pthread.h>
#include <libpmem.h>
#include <libpmemobj.h>

// --- PMDK Integration Types ---

/**
 * @brief PMEM context handle for persistent memory operations
 */
typedef struct {
    PMEMobjpool* pool;          // PMDK object pool
    PMEMoid root_oid;           // Root object ID
    void* mapped_addr;          // Mapped memory address
    size_t pool_size;           // Pool size in bytes
    const char* pool_path;      // Path to pool file
} PMEMContextHandle;

/**
 * @brief PMLL lock structure for serializing access to shared resources
 */
typedef struct {
    pthread_mutex_t mutex;      // Thread synchronization
    PMEMoid pmem_oid;          // Persistent memory object ID
    uint64_t lock_id;          // Unique lock identifier
    const char* resource_id;   // Resource being protected
    bool is_persistent;        // Whether lock state is persistent
} PMLL_Lock;

/**
 * @brief PMLL hardened resource queue for serializing operations
 */
typedef struct PMLL_HardenedResourceQueue {
    const char* resource_id;                    // Resource identifier
    Promise* operation_queue_promise;          // Current tail of operation chain
    PMLL_Lock queue_lock;                      // Lock for queue modifications
    PMEMContextHandle* pmem_context;           // Persistent memory context
    
    // Statistics
    uint64_t operations_completed;
    uint64_t operations_failed;
    uint64_t operations_pending;
    
    // Configuration
    bool persistent_queue;                     // Whether queue state is persistent
    size_t max_queue_depth;                   // Maximum pending operations
    uint32_t timeout_ms;                      // Operation timeout
} PMLL_HardenedResourceQueue;

// --- PMEM Context Management ---

/**
 * @brief Creates a new PMEM context for persistent memory operations
 * @param pool_path Path to persistent memory pool file
 * @param pool_size Size of pool in bytes (0 for existing pool)
 * @param create_if_missing Create pool if it doesn't exist
 * @return PMEM context handle or NULL on failure
 */
PMEMContextHandle* pmem_context_create(const char* pool_path, 
                                      size_t pool_size, 
                                      bool create_if_missing);

/**
 * @brief Opens an existing PMEM context
 * @param pool_path Path to existing pool file
 * @return PMEM context handle or NULL on failure
 */
PMEMContextHandle* pmem_context_open(const char* pool_path);

/**
 * @brief Closes and frees a PMEM context
 * @param ctx Context to close
 */
void pmem_context_close(PMEMContextHandle* ctx);

/**
 * @brief Allocates persistent memory from context
 * @param ctx PMEM context
 * @param size Bytes to allocate
 * @return Persistent memory object ID
 */
PMEMoid pmem_context_alloc(PMEMContextHandle* ctx, size_t size);

/**
 * @brief Frees persistent memory object
 * @param ctx PMEM context
 * @param oid Object ID to free
 */
void pmem_context_free(PMEMContextHandle* ctx, PMEMoid oid);

/**
 * @brief Persists data to persistent memory
 * @param addr Address to persist
 * @param size Number of bytes to persist
 */
void pmem_context_persist(void* addr, size_t size);

// --- PMLL Lock Management ---

/**
 * @brief Initializes a PMLL lock
 * @param lock Lock structure to initialize
 * @param resource_id Resource being protected
 * @param is_persistent Whether lock state should be persistent
 * @param pmem_ctx PMEM context (required if persistent)
 * @return CPM_RESULT_SUCCESS on success
 */
CPM_Result pmll_lock_init(PMLL_Lock* lock, 
                         const char* resource_id,
                         bool is_persistent,
                         PMEMContextHandle* pmem_ctx);

/**
 * @brief Acquires a PMLL lock
 * @param lock Lock to acquire
 * @param timeout_ms Timeout in milliseconds (0 for blocking)
 * @return CPM_RESULT_SUCCESS on success
 */
CPM_Result pmll_lock_acquire(PMLL_Lock* lock, uint32_t timeout_ms);

/**
 * @brief Releases a PMLL lock
 * @param lock Lock to release
 * @return CPM_RESULT_SUCCESS on success
 */
CPM_Result pmll_lock_release(PMLL_Lock* lock);

/**
 * @brief Destroys a PMLL lock
 * @param lock Lock to destroy
 */
void pmll_lock_destroy(PMLL_Lock* lock);

// --- PMLL Hardened Queue Management ---

/**
 * @brief Creates a new hardened resource queue
 * @param resource_id Unique identifier for the resource
 * @param persistent_queue Whether queue state should be persistent
 * @param pmem_ctx PMEM context (required if persistent)
 * @return Queue handle or NULL on failure
 */
PMLL_HardenedResourceQueue* pmll_queue_create(const char* resource_id,
                                              bool persistent_queue,
                                              PMEMContextHandle* pmem_ctx);

/**
 * @brief Executes an operation on the hardened queue
 * @param hq Hardened queue
 * @param operation_fn Function to execute
 * @param error_fn Error handler (can be NULL)
 * @param user_data Data passed to operation function
 * @return Promise that resolves with operation result
 */
Promise* pmll_execute_hardened_operation(PMLL_HardenedResourceQueue* hq,
                                        on_fulfilled_callback operation_fn,
                                        on_rejected_callback error_fn,
                                        void* user_data);

/**
 * @brief Gets queue statistics
 * @param hq Hardened queue
 * @param completed Output for completed operations count
 * @param failed Output for failed operations count  
 * @param pending Output for pending operations count
 */
void pmll_queue_get_stats(PMLL_HardenedResourceQueue* hq,
                         uint64_t* completed,
                         uint64_t* failed,
                         uint64_t* pending);

/**
 * @brief Waits for all pending operations to complete
 * @param hq Hardened queue
 * @param timeout_ms Timeout in milliseconds (0 for infinite)
 * @return CPM_RESULT_SUCCESS if all operations completed
 */
CPM_Result pmll_queue_wait_complete(PMLL_HardenedResourceQueue* hq, uint32_t timeout_ms);

/**
 * @brief Shuts down and frees a hardened queue
 * @param hq Queue to free
 */
void pmll_queue_free(PMLL_HardenedResourceQueue* hq);

// --- PMLL Transaction Support ---

/**
 * @brief Transaction handle for atomic operations
 */
typedef struct {
    PMEMContextHandle* pmem_ctx;
    PMEMoid tx_oid;
    bool is_active;
    uint64_t tx_id;
} PMLL_Transaction;

/**
 * @brief Begins a new PMLL transaction
 * @param pmem_ctx PMEM context
 * @return Transaction handle or NULL on failure
 */
PMLL_Transaction* pmll_transaction_begin(PMEMContextHandle* pmem_ctx);

/**
 * @brief Commits a PMLL transaction
 * @param tx Transaction to commit
 * @return CPM_RESULT_SUCCESS on success
 */
CPM_Result pmll_transaction_commit(PMLL_Transaction* tx);

/**
 * @brief Aborts a PMLL transaction
 * @param tx Transaction to abort
 */
void pmll_transaction_abort(PMLL_Transaction* tx);

/**
 * @brief Frees a transaction handle
 * @param tx Transaction to free
 */
void pmll_transaction_free(PMLL_Transaction* tx);

// --- PMLL Utilities ---

/**
 * @brief Initializes PMLL subsystem
 * @return CPM_RESULT_SUCCESS on success
 */
CPM_Result pmll_init(void);

/**
 * @brief Shuts down PMLL subsystem
 */
void pmll_shutdown(void);

/**
 * @brief Checks if PMDK is available on system
 * @return true if PMDK is available and functional
 */
bool pmll_is_pmdk_available(void);

/**
 * @brief Gets PMLL version information
 * @param major Output for major version
 * @param minor Output for minor version  
 * @param patch Output for patch version
 */
void pmll_get_version(int* major, int* minor, int* patch);

#endif // CPM_PMLL_H
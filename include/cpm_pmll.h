/*
 * File: include/cpm_pmll.h
 * Description: PMLL (Persistent Memory Lock Library) hardened queue API
 * Provides serialized operations using Q promises for race condition prevention
 * Full PMDK integration for persistent memory operations
 * Author: Dr. Q Josef Kurk Edwards
 */

#ifndef CPM_PMLL_H
#define CPM_PMLL_H

#include "cpm_types.h"
#include "cpm_promise.h"
#include <libpmem.h>
#include <libpmemobj.h>

// --- PMDK Pool Management ---

/**
 * @brief Initializes PMDK and creates/opens persistent memory pool.
 * @param pool_path Path to the persistent memory pool file
 * @param pool_size Size of the pool (only used if creating new pool)
 * @return PMEMContextHandle on success, NULL on failure
 */
PMEMContextHandle pmll_init_pmem_pool(const char* pool_path, size_t pool_size);

/**
 * @brief Closes and cleans up PMDK pool.
 * @param ctx The PMEM context handle to close
 */
void pmll_close_pmem_pool(PMEMContextHandle ctx);

// --- PMLL Hardened Resource Queue ---

/**
 * @brief Creates a hardened resource queue for serializing operations.
 * @param resource_id Unique identifier for the resource
 * @param persistent_queue Whether to use persistent memory backing
 * @param pmem_ctx Optional persistent memory context (required if persistent_queue is true)
 * @return Pointer to PMLL_HardenedResourceQueue, or NULL on failure
 */
PMLL_HardenedResourceQueue* pmll_queue_create(const char* resource_id, 
                                              bool persistent_queue,
                                              PMEMContextHandle pmem_ctx);

/**
 * @brief Executes an operation serially on the resource managed by this queue.
 * @param hq The hardened resource queue
 * @param operation_fn The work function to execute
 * @param error_fn Optional error handler
 * @param op_user_data Data specific to this operation
 * @return Promise that resolves with the operation result
 */
Promise* pmll_execute_hardened_operation(PMLL_HardenedResourceQueue* hq,
                                        on_fulfilled_callback operation_fn,
                                        on_rejected_callback error_fn,
                                        void* op_user_data);

/**
 * @brief Frees a hardened resource queue and waits for pending operations.
 * @param hq The queue to free
 */
void pmll_queue_free(PMLL_HardenedResourceQueue* hq);

// --- PMLL Lock Management ---

/**
 * @brief Creates a new PMLL lock.
 * @param lock_id Unique identifier for the lock
 * @param persistent Whether to use persistent memory backing
 * @param pmem_ctx Optional persistent memory context
 * @return Pointer to PMLL_Lock, or NULL on failure
 */
PMLL_Lock* pmll_lock_create(uint64_t lock_id, bool persistent, PMEMContextHandle pmem_ctx);

/**
 * @brief Acquires a PMLL lock.
 * @param lock The lock to acquire
 * @return true on success, false on failure
 */
bool pmll_lock_acquire(PMLL_Lock* lock);

/**
 * @brief Releases a PMLL lock.
 * @param lock The lock to release
 * @return true on success, false on failure
 */
bool pmll_lock_release(PMLL_Lock* lock);

/**
 * @brief Frees a PMLL lock.
 * @param lock The lock to free
 */
void pmll_lock_free(PMLL_Lock* lock);

// --- Hardened File Operations ---

/**
 * @brief Hardened file write operation using PMLL serialization.
 * @param queue The resource queue for the file
 * @param filepath Path to the file
 * @param data Data to write
 * @param size Size of data
 * @return Promise that resolves when write completes
 */
Promise* pmll_hardened_file_write(PMLL_HardenedResourceQueue* queue,
                                 const char* filepath,
                                 const void* data,
                                 size_t size);

/**
 * @brief Hardened file read operation using PMLL serialization.
 * @param queue The resource queue for the file
 * @param filepath Path to the file
 * @return Promise that resolves with file contents
 */
Promise* pmll_hardened_file_read(PMLL_HardenedResourceQueue* queue,
                                const char* filepath);

/**
 * @brief Hardened atomic file replacement using PMLL serialization.
 * @param queue The resource queue for the file
 * @param filepath Path to the target file
 * @param temp_filepath Path to the temporary file
 * @return Promise that resolves when replacement completes
 */
Promise* pmll_hardened_file_replace(PMLL_HardenedResourceQueue* queue,
                                   const char* filepath,
                                   const char* temp_filepath);

// --- Transaction Support ---

/**
 * @brief Begins a PMLL transaction for multiple operations.
 * @param pmem_ctx Persistent memory context
 * @return Transaction handle, or NULL on failure
 */
void* pmll_transaction_begin(PMEMContextHandle pmem_ctx);

/**
 * @brief Commits a PMLL transaction.
 * @param tx_handle Transaction handle
 * @return true on success, false on failure
 */
bool pmll_transaction_commit(void* tx_handle);

/**
 * @brief Aborts a PMLL transaction.
 * @param tx_handle Transaction handle
 */
void pmll_transaction_abort(void* tx_handle);

#endif // CPM_PMLL_H
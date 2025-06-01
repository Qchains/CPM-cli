/*
 * File: include/cpm_types.h
 * Description: Common type definitions for CPM (C Package Manager)
 * Author: Dr. Q Josef Kurk Edwards
 */

#ifndef CPM_TYPES_H
#define CPM_TYPES_H

#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

// --- Forward Declarations ---
typedef struct Promise Promise;
typedef struct PromiseDeferred PromiseDeferred;
typedef struct PMLL_HardenedResourceQueue PMLL_HardenedResourceQueue;
typedef struct Package Package;

// --- Basic Result Types for CPM Operations ---
typedef enum {
    CPM_RESULT_SUCCESS = 0,
    CPM_RESULT_ERROR_UNKNOWN = 1,
    CPM_RESULT_ERROR_INVALID_ARGS = 2,
    CPM_RESULT_ERROR_NOT_INITIALIZED = 3,
    CPM_RESULT_ERROR_ALREADY_INITIALIZED = 4,
    CPM_RESULT_ERROR_INITIALIZATION_FAILED = 5,
    CPM_RESULT_ERROR_TERMINATION_FAILED = 6,
    CPM_RESULT_ERROR_UNKNOWN_COMMAND = 7,
    CPM_RESULT_ERROR_COMMAND_FAILED = 8,
    CPM_RESULT_ERROR_MEMORY_ALLOCATION = 9,
    CPM_RESULT_ERROR_FILE_OPERATION = 10,
    CPM_RESULT_ERROR_NETWORK = 11,
    CPM_RESULT_ERROR_PACKAGE_PARSE = 12,
    CPM_RESULT_ERROR_DEPENDENCY_RESOLUTION = 13,
    CPM_RESULT_ERROR_SCRIPT_EXECUTION = 14,
    CPM_RESULT_ERROR_PMLL_INIT = 15,
    CPM_RESULT_ERROR_PMDK_INIT = 16,
    CPM_RESULT_ERROR_PROMISE_CREATE = 17,
    CPM_RESULT_ERROR_PROMISE_CHAIN = 18
} CPM_Result;

// --- Logging Levels ---
#define CPM_LOG_NONE  0
#define CPM_LOG_ERROR 1
#define CPM_LOG_WARN  2
#define CPM_LOG_INFO  3
#define CPM_LOG_DEBUG 4
#define CPM_LOG_TRACE 5

// --- Promise Types ---
typedef enum {
    PROMISE_PENDING,
    PROMISE_FULFILLED,
    PROMISE_REJECTED
} PromiseState;

// Generic void pointer for promise value/reason
typedef void* PromiseValue;

// Callback function pointer types
typedef PromiseValue (*on_fulfilled_callback)(PromiseValue value, void* user_data);
typedef PromiseValue (*on_rejected_callback)(PromiseValue reason, void* user_data);

// --- PMDK/PMLL Types ---
// PMDK persistent memory context handle
typedef void* PMEMContextHandle;

// PMLL Lock type for synchronization
typedef struct {
    void* pmem_lock_ptr;  // Pointer to persistent memory lock
    uint64_t lock_id;     // Unique lock identifier
    bool is_persistent;   // Whether this lock is backed by persistent memory
} PMLL_Lock;

// --- CPM Configuration ---
typedef struct {
    const char* working_directory;
    const char* modules_directory;
    const char* registry_url;
    const char* log_file_path;
    FILE* log_stream;
    int log_level;
    bool enable_pmem;           // Enable persistent memory features
    const char* pmem_pool_path; // Path to PMEM pool file
    size_t pmem_pool_size;      // Size of PMEM pool
} CPM_Config;

#endif // CPM_TYPES_H
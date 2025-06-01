/*
 * File: include/cpm_types.h
 * Description: Common type definitions and constants for CPM
 * Author: Dr. Q Josef Kurk Edwards
 */

#ifndef CPM_TYPES_H
#define CPM_TYPES_H

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

// --- Forward Declarations ---
typedef struct Promise Promise;
typedef struct PromiseDeferred PromiseDeferred;
typedef struct PMLL_HardenedResourceQueue PMLL_HardenedResourceQueue;
typedef struct Package Package;

// --- Result Types ---
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
    CPM_RESULT_ERROR_PROMISE_CREATION = 16,
    CPM_RESULT_ERROR_PMDK_OPERATION = 17
} CPM_Result;

// --- Logging Levels ---
#define CPM_LOG_NONE  0
#define CPM_LOG_ERROR 1
#define CPM_LOG_WARN  2
#define CPM_LOG_INFO  3
#define CPM_LOG_DEBUG 4
#define CPM_LOG_TRACE 5

// --- Promise Value Type ---
typedef void* PromiseValue;

// --- Promise States ---
typedef enum {
    PROMISE_PENDING,
    PROMISE_FULFILLED,
    PROMISE_REJECTED
} PromiseState;

// --- Promise Callback Types ---
typedef PromiseValue (*on_fulfilled_callback)(PromiseValue value, void* user_data);
typedef PromiseValue (*on_rejected_callback)(PromiseValue reason, void* user_data);

// --- Package Structure ---
typedef struct Package {
    char* name;
    char* version;
    char* description;
    char* author;
    char* license;
    
    // Dependencies
    char** dependencies;
    size_t dep_count;
    
    // Scripts
    char** scripts;
    size_t script_count;
    
    // Build configuration
    char* build_script;
    char* install_script;
    char* test_script;
    
    // Metadata
    char* homepage;
    char* repository;
    char* bugs_url;
    
    // CPM specific
    char* include_path;
    char* lib_path;
    char* bin_path;
} Package;

// --- CPM Global Configuration ---
typedef struct {
    const char* working_directory;
    const char* modules_directory;
    const char* registry_url;
    const char* log_file_path;
    FILE* log_stream;
    int log_level;
    
    // PMLL configuration
    bool pmll_enabled;
    size_t pmll_pool_size;
    
    // Promise configuration
    size_t promise_pool_size;
    
    // Network configuration
    int timeout_ms;
    int max_retries;
    
    // Security configuration
    bool verify_signatures;
    bool verify_checksums;
    
    // Cache configuration
    const char* cache_dir;
    size_t cache_max_age;
} CPM_Config;

// --- Version Information ---
#define CPM_VERSION_MAJOR 0
#define CPM_VERSION_MINOR 1
#define CPM_VERSION_PATCH 0
#define CPM_VERSION_SUFFIX "alpha"
#define CPM_VERSION_STRING "0.1.0-alpha"

// --- Constants ---
#define CPM_MAX_PATH 4096
#define CPM_MAX_NAME 256
#define CPM_MAX_VERSION 64
#define CPM_MAX_DEPS 1024
#define CPM_MAX_SCRIPTS 64

#endif // CPM_TYPES_H
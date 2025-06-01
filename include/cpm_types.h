/*
 * File: include/cpm_types.h
 * Description: Core type definitions for CPM (C Package Manager)
 * Author: Dr. Q Josef Kurk Edwards
 */

#ifndef CPM_TYPES_H
#define CPM_TYPES_H

#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

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
    CPM_RESULT_ERROR_PMDK_INIT = 16,
    CPM_RESULT_ERROR_PERSISTENT_MEMORY = 17,
    CPM_RESULT_ERROR_PROMISE_CHAIN = 18,
    CPM_RESULT_ERROR_LOCK_TIMEOUT = 19,
    CPM_RESULT_ERROR_REGISTRY_UNAVAILABLE = 20
} CPM_Result;

// --- Logging Levels ---
#define CPM_LOG_NONE  0
#define CPM_LOG_ERROR 1
#define CPM_LOG_WARN  2
#define CPM_LOG_INFO  3
#define CPM_LOG_DEBUG 4
#define CPM_LOG_TRACE 5

// --- Package Structure ---
typedef struct Package {
    char* name;
    char* version;
    char* description;
    char* author;
    char* license;
    char* homepage;
    char* repository;
    
    // Dependencies
    char** dependencies;
    size_t dep_count;
    
    // Scripts
    char** scripts;
    size_t script_count;
    
    // Build information
    char* build_script;
    char* install_script;
    
    // File lists
    char** source_files;
    size_t source_count;
    char** header_files;
    size_t header_count;
    
    // Metadata
    time_t created_at;
    time_t updated_at;
    uint64_t size;
    char* checksum;
} Package;

// --- CPM Configuration ---
typedef struct {
    const char* working_directory;
    const char* modules_directory;
    const char* registry_url;
    const char* log_file_path;
    FILE* log_stream;
    int log_level;
    
    // PMLL/PMDK settings
    bool pmll_enabled;
    const char* pmem_pool_path;
    size_t pmem_pool_size;
    
    // Network settings
    int timeout_ms;
    int max_retries;
    
    // Security settings
    bool verify_signatures;
    bool verify_checksums;
    
    // Performance settings
    size_t promise_pool_size;
    size_t max_concurrent_downloads;
} CPM_Config;

// --- Forward Declarations ---
typedef struct Promise Promise;
typedef struct PromiseDeferred PromiseDeferred;
typedef struct PMLL_HardenedResourceQueue PMLL_HardenedResourceQueue;

// --- Promise Value Type ---
typedef void* PromiseValue;

// --- Callback Types ---
typedef PromiseValue (*on_fulfilled_callback)(PromiseValue value, void* user_data);
typedef PromiseValue (*on_rejected_callback)(PromiseValue reason, void* user_data);
typedef void (*NodeCallback)(void* err, PromiseValue result, void* user_data);

// --- Utility Macros ---
#define CPM_UNUSED(x) ((void)(x))
#define CPM_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

// --- String Utilities ---
char* cpm_strdup(const char* str);
void cpm_free_string_array(char** array, size_t count);
char** cpm_split_string(const char* str, const char* delimiter, size_t* count);

// --- Memory Management ---
void* cpm_malloc(size_t size);
void* cpm_calloc(size_t nmemb, size_t size);
void* cpm_realloc(void* ptr, size_t size);
void cpm_free(void* ptr);

#endif // CPM_TYPES_H
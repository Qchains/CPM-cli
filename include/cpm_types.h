/*
 * File: include/cpm_types.h
 * Description: Common type definitions for CPM (C Package Manager).
 * Contains result codes, logging levels, and shared data structures.
 * Author: Dr. Q Josef Kurk Edwards
 */

#ifndef CPM_TYPES_H
#define CPM_TYPES_H

#include <stddef.h> // For size_t
#include <stdbool.h> // For bool

// --- Result Codes ---
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
    CPM_RESULT_ERROR_PMLL_INIT = 15
} CPM_Result;

// --- Logging Levels ---
#define CPM_LOG_NONE  0
#define CPM_LOG_ERROR 1
#define CPM_LOG_WARN  2
#define CPM_LOG_INFO  3
#define CPM_LOG_DEBUG 4
#define CPM_LOG_TRACE 5

#endif // CPM_TYPES_H
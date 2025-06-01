/*
 * File: include/cpm.h
 * Description: Main public API header for CPM (C Package Manager) operations.
 * This header defines the core functions for initializing the CPM environment,
 * executing commands, and cleaning up resources. It's used by the
 * CPM CLI and potentially by other tools linking against CPM as a library.
 * Author: Dr. Q Josef Kurk Edwards
 */

#ifndef CPM_H
#define CPM_H

#include <stdio.h> // For FILE* etc.
#include <stdbool.h> // For bool
#include <stddef.h>  // For size_t
#include "cpm_types.h"   // Common type definitions (e.g., CPM_Result, Package)
#include "cpm_promise.h" // Q Promise library API
#include "cpm_package.h" // Package structure and parsing functions
#include "cpm_pmll.h"    // PMLL hardened queue for file operations

// --- CPM Global Configuration ---
typedef struct {
    const char* working_directory;
    const char* modules_directory; // e.g., "cpm_modules"
    const char* registry_url;      // e.g., "https://registry.cpm.example.org"
    const char* log_file_path;     // Path to log file, if not stderr/stdout
    FILE* log_stream;              // Defaults to stderr
    int log_level;                 // e.g., CPM_LOG_INFO
} CPM_Config;

// --- Core CPM Lifecycle Functions ---

/**
 * @brief Initializes the CPM environment.
 *
 * Sets up global configurations, logging, PMLL queues, etc.
 * Must be called before any other CPM operations.
 *
 * @param config A pointer to a CPM_Config struct with initial settings.
 * If NULL, default settings will be used.
 * @return CPM_RESULT_SUCCESS on success, or an error code on failure.
 */
CPM_Result cpm_initialize(const CPM_Config* config);

/**
 * @brief Executes a CPM command.
 *
 * Main dispatcher for commands like "install", "publish", "run", etc.
 *
 * @param command The name of the command to execute.
 * @param argc The number of arguments for the command.
 * @param argv An array of argument strings for the command.
 * @return CPM_RESULT_SUCCESS on successful command execution, or an error code.
 */
CPM_Result cpm_execute_command(const char* command, int argc, char* argv[]);

/**
 * @brief Cleans up CPM resources.
 *
 * Frees global resources, ensures pending operations are handled.
 * Should be called before program exit.
 */
void cpm_terminate(void);

// --- Command Handler Function Prototypes ---
CPM_Result cpm_handle_install_command(int argc, char* argv[], const CPM_Config* config);
CPM_Result cpm_handle_publish_command(int argc, char* argv[], const CPM_Config* config);
CPM_Result cpm_handle_search_command(int argc, char* argv[], const CPM_Config* config);
CPM_Result cpm_handle_run_script_command(int argc, char* argv[], const CPM_Config* config);
CPM_Result cpm_handle_init_command(int argc, char* argv[], const CPM_Config* config);
CPM_Result cpm_handle_help_command(int argc, char* argv[], const CPM_Config* config);

#endif // CPM_H
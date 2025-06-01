/*
 * File: cpm.c (Root directory)
 * Description: Main entry point for the CPM (C Package Manager) command-line interface.
 * This file contains the main() function, parses command-line arguments,
 * initializes the CPM environment, dispatches commands, and handles cleanup.
 * Author: Dr. Q Josef Kurk Edwards
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>     // For va_list, va_start, va_end
#include <time.h>       // For logging timestamp
#include "include/cpm.h" // Main CPM public API
#include "include/cpm_config.h" // Configuration management

// Global CPM configuration instance
static CPM_Config* global_cpm_config = NULL;
static bool cpm_is_initialized = false;

// --- Simple Logging Implementation ---
void cpm_log_message_impl(int level, const char* file, int line, const char* format, ...) {
    (void)file; // Suppress unused parameter warning
    (void)line; // Suppress unused parameter warning
    
    if (!global_cpm_config) return;
    if (global_cpm_config->quiet && level > CPM_LOG_ERROR) return;
    if (!global_cpm_config->verbose && level > CPM_LOG_INFO) return;

    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
    fflush(stderr);
}

// Macro to simplify logging calls
#define cpm_log(level, ...) cpm_log_message_impl(level, __FILE__, __LINE__, __VA_ARGS__)

// --- Core CPM Lifecycle Function Implementations ---

CPM_Result cpm_initialize(const CPM_Config* user_config) {
    if (cpm_is_initialized) {
        fprintf(stderr, "CPM already initialized.\n");
        return CPM_RESULT_ERROR_ALREADY_INITIALIZED;
    }

    // Load configuration
    if (user_config) {
        // Make a copy of the provided config
        global_cpm_config = cpm_config_load();
        if (!global_cpm_config) {
            fprintf(stderr, "Failed to load configuration\n");
            return CPM_RESULT_ERROR_INITIALIZATION_FAILED;
        }
        // Override with user settings would go here
    } else {
        global_cpm_config = cpm_config_load();
        if (!global_cpm_config) {
            fprintf(stderr, "Failed to load configuration\n");
            return CPM_RESULT_ERROR_INITIALIZATION_FAILED;
        }
    }

    // Initialize PMLL system
    if (!pmll_init_global_system()) {
        fprintf(stderr, "Failed to initialize PMLL system.\n");
        cpm_config_free(global_cpm_config);
        global_cpm_config = NULL;
        return CPM_RESULT_ERROR_INITIALIZATION_FAILED;
    }

    // Initialize promise subsystem's event loop
    init_event_loop();

    cpm_is_initialized = true;
    return CPM_RESULT_SUCCESS;
}

CPM_Result cpm_execute_command(const char* command, int argc, char* argv[]) {
    if (!cpm_is_initialized) {
        fprintf(stderr, "Fatal Error: CPM not initialized. Call cpm_initialize() first.\n");
        return CPM_RESULT_ERROR_NOT_INITIALIZED;
    }
    if (!command || command[0] == '\0') {
        fprintf(stderr, "No command provided.\n");
        cpm_handle_help_command(0, NULL, global_cpm_config);
        return CPM_RESULT_ERROR_INVALID_ARGS;
    }

    if (strcmp(command, "install") == 0) {
        return cpm_handle_install_command(argc, argv, global_cpm_config);
    } else if (strcmp(command, "publish") == 0) {
        return cpm_handle_publish_command(argc, argv, global_cpm_config);
    } else if (strcmp(command, "search") == 0) {
        return cpm_handle_search_command(argc, argv, global_cpm_config);
    } else if (strcmp(command, "run") == 0 || strcmp(command, "run-script") == 0) {
        return cpm_handle_run_script_command(argc, argv, global_cpm_config);
    } else if (strcmp(command, "init") == 0) {
        return cpm_handle_init_command(argc, argv, global_cpm_config);
    } else if (strcmp(command, "help") == 0 || strcmp(command, "--help") == 0 || strcmp(command, "-h") == 0) {
        return cpm_handle_help_command(argc, argv, global_cpm_config);
    } else {
        fprintf(stderr, "Unknown command: %s\n", command);
        cpm_handle_help_command(0, NULL, global_cpm_config);
        return CPM_RESULT_ERROR_UNKNOWN_COMMAND;
    }
}

void cpm_terminate(void) {
    if (!cpm_is_initialized) {
        return;
    }

    // Terminate PMLL system
    pmll_shutdown_global_system();

    // Free promise subsystem's event loop resources
    free_event_loop();

    // Free configuration
    if (global_cpm_config) {
        cpm_config_free(global_cpm_config);
        global_cpm_config = NULL;
    }
    
    cpm_is_initialized = false;
}

// --- Main Function ---
int main(int argc, char* argv[]) {
    CPM_Result result;
    
    // Initialize with NULL config to use defaults
    result = cpm_initialize(NULL);
    if (result != CPM_RESULT_SUCCESS) {
        fprintf(stderr, "Critical error: CPM failed to initialize (code: %d).\n", result);
        return EXIT_FAILURE;
    }

    // Apply command line arguments to config
    if (global_cpm_config) {
        cpm_config_apply_command_line_args(global_cpm_config, argc, argv);
    }

    if (argc <= 1) {
        cpm_handle_help_command(0, NULL, global_cpm_config);
        cpm_terminate();
        return EXIT_SUCCESS;
    }

    const char* command = argv[1];
    int cmd_argc = argc - 2;
    char** cmd_argv = argv + 2;

    result = cpm_execute_command(command, cmd_argc, cmd_argv);

    cpm_terminate();

    return (result == CPM_RESULT_SUCCESS) ? EXIT_SUCCESS : EXIT_FAILURE;
}
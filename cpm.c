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

    printf("[PMLL] Initializing CPM package manager...\n");

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
        cpm_log(CPM_LOG_ERROR, "No command provided.");
        cpm_handle_help_command(0, NULL, &global_cpm_config);
        return CPM_RESULT_ERROR_INVALID_ARGS;
    }

    cpm_log(CPM_LOG_INFO, "Executing command: \"%s\"", command);
    for(int i=0; i < argc; ++i) {
        cpm_log(CPM_LOG_DEBUG, "  arg[%d]: \"%s\"", i, argv[i]);
    }

    if (strcmp(command, "install") == 0) {
        return cpm_handle_install_command(argc, argv, &global_cpm_config);
    } else if (strcmp(command, "publish") == 0) {
        return cpm_handle_publish_command(argc, argv, &global_cpm_config);
    } else if (strcmp(command, "search") == 0) {
        return cpm_handle_search_command(argc, argv, &global_cpm_config);
    } else if (strcmp(command, "run") == 0 || strcmp(command, "run-script") == 0) {
        return cpm_handle_run_script_command(argc, argv, &global_cpm_config);
    } else if (strcmp(command, "init") == 0) {
        return cpm_handle_init_command(argc, argv, &global_cpm_config);
    } else if (strcmp(command, "help") == 0 || strcmp(command, "--help") == 0 || strcmp(command, "-h") == 0) {
        return cpm_handle_help_command(argc, argv, &global_cpm_config);
    } else {
        cpm_log(CPM_LOG_ERROR, "Unknown command: %s", command);
        cpm_handle_help_command(0, NULL, &global_cpm_config);
        return CPM_RESULT_ERROR_UNKNOWN_COMMAND;
    }
}

void cpm_terminate(void) {
    if (!cpm_is_initialized) {
        return;
    }
    cpm_log(CPM_LOG_INFO, "CPM Terminating...");

    // Terminate PMLL system
    pmll_shutdown_global_system();

    // Free promise subsystem's event loop resources
    free_event_loop();

    cpm_log(CPM_LOG_INFO, "CPM Termination complete.");
    
    // Close log file if it was opened and not stderr/stdout
    if (global_cpm_config.log_file_path && global_cpm_config.log_stream != stderr && global_cpm_config.log_stream != stdout) {
        fclose(global_cpm_config.log_stream);
        global_cpm_config.log_stream = stderr;
    }
    cpm_is_initialized = false;
}

// --- Main Function ---
int main(int argc, char* argv[]) {
    CPM_Result result;
    CPM_Config current_run_config;
    cpm_set_default_config(&current_run_config);

    // Basic CLI Argument Parsing for Global Options
    int command_arg_offset = 1;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0) {
            current_run_config.log_level = CPM_LOG_DEBUG;
            command_arg_offset = i + 1;
        } else if (strcmp(argv[i], "--trace") == 0) {
            current_run_config.log_level = CPM_LOG_TRACE;
            command_arg_offset = i + 1;
        } else if (strcmp(argv[i], "--log-file") == 0) {
            if (i + 1 < argc) {
                current_run_config.log_file_path = argv[i+1];
                command_arg_offset = i + 2;
                i++; // consume value
            } else {
                fprintf(stderr, "Error: --log-file requires an argument.\n");
                return EXIT_FAILURE;
            }
        } else {
            command_arg_offset = i;
            break;
        }
    }
    
    result = cpm_initialize(&current_run_config);
    if (result != CPM_RESULT_SUCCESS) {
        fprintf(global_cpm_config.log_stream ? global_cpm_config.log_stream : stderr,
                "Critical error: CPM failed to initialize (code: %d).\n", result);
        return EXIT_FAILURE;
    }

    if (argc <= command_arg_offset) {
        cpm_handle_help_command(0, NULL, &global_cpm_config);
        cpm_terminate();
        return EXIT_SUCCESS;
    }

    const char* command = argv[command_arg_offset];
    int cmd_argc = argc - (command_arg_offset + 1);
    char** cmd_argv = argv + (command_arg_offset + 1);

    result = cpm_execute_command(command, cmd_argc, cmd_argv);

    cpm_terminate();

    return (result == CPM_RESULT_SUCCESS) ? EXIT_SUCCESS : EXIT_FAILURE;
}
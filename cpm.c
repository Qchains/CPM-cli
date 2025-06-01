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

// Global CPM configuration instance
static CPM_Config global_cpm_config;
static bool cpm_is_initialized = false;

// --- Simple Logging Implementation ---
void cpm_log_message_impl(int level, const char* file, int line, const char* format, ...) {
    if (level > global_cpm_config.log_level) {
        return;
    }

    time_t now = time(NULL);
    char time_buf[32];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", localtime(&now));

    FILE* out_stream = global_cpm_config.log_stream ? global_cpm_config.log_stream : stderr;

    fprintf(out_stream, "[%s] ", time_buf);
    switch (level) {
        case CPM_LOG_ERROR: fprintf(out_stream, "ERROR "); break;
        case CPM_LOG_WARN:  fprintf(out_stream, "WARN  "); break;
        case CPM_LOG_INFO:  fprintf(out_stream, "INFO  "); break;
        case CPM_LOG_DEBUG: fprintf(out_stream, "DEBUG "); break;
        case CPM_LOG_TRACE: fprintf(out_stream, "TRACE "); break;
    }

    va_list args;
    va_start(args, format);
    vfprintf(out_stream, format, args);
    va_end(args);
    fprintf(out_stream, "\n");
    fflush(out_stream);
}

// Macro to simplify logging calls
#define cpm_log(level, ...) cpm_log_message_impl(level, __FILE__, __LINE__, __VA_ARGS__)

// --- Helper function to set default configuration ---
void cpm_set_default_config(CPM_Config* config) {
    if (!config) return;
    config->working_directory = "."; // Current directory
    config->modules_directory = "cpm_modules";
    config->registry_url = "https://registry.cpm.example.org"; // Placeholder
    config->log_file_path = NULL; // No file logging by default
    config->log_stream = stderr;  // Default to stderr
    config->log_level = CPM_LOG_INFO; // Default to Info
}

// --- Core CPM Lifecycle Function Implementations ---

CPM_Result cpm_initialize(const CPM_Config* user_config) {
    if (cpm_is_initialized) {
        cpm_log(CPM_LOG_WARN, "CPM already initialized.");
        return CPM_RESULT_ERROR_ALREADY_INITIALIZED;
    }

    // Set default config first, then override with user_config if provided
    cpm_set_default_config(&global_cpm_config);
    if (user_config) {
        if (user_config->working_directory) global_cpm_config.working_directory = user_config->working_directory;
        if (user_config->modules_directory) global_cpm_config.modules_directory = user_config->modules_directory;
        if (user_config->registry_url) global_cpm_config.registry_url = user_config->registry_url;
        if (user_config->log_file_path) global_cpm_config.log_file_path = user_config->log_file_path;
        global_cpm_config.log_level = user_config->log_level;
        if (user_config->log_stream) global_cpm_config.log_stream = user_config->log_stream;
    }
    
    // If a log file path is provided, try to open it
    if (global_cpm_config.log_file_path) {
        FILE* lf = fopen(global_cpm_config.log_file_path, "a"); // Append mode
        if (lf) {
            global_cpm_config.log_stream = lf;
        } else {
            cpm_log(CPM_LOG_ERROR, "Failed to open log file: %s. Defaulting to stderr.", global_cpm_config.log_file_path);
            global_cpm_config.log_stream = stderr;
        }
    }

    cpm_log(CPM_LOG_INFO, "CPM Initializing (v0.1.0-alpha)...");
    cpm_log(CPM_LOG_DEBUG, "Working Directory: %s", global_cpm_config.working_directory);
    cpm_log(CPM_LOG_DEBUG, "Modules Directory: %s", global_cpm_config.modules_directory);
    cpm_log(CPM_LOG_DEBUG, "Registry URL: %s", global_cpm_config.registry_url);
    cpm_log(CPM_LOG_DEBUG, "Log Level: %d", global_cpm_config.log_level);

    // Initialize PMLL system
    if (!pmll_init_global_system()) {
        cpm_log(CPM_LOG_ERROR, "Failed to initialize PMLL system.");
        return CPM_RESULT_ERROR_INITIALIZATION_FAILED;
    }

    // Initialize promise subsystem's event loop
    init_event_loop();

    cpm_is_initialized = true;
    cpm_log(CPM_LOG_INFO, "CPM Initialization complete.");
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
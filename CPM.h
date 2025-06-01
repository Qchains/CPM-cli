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
#include "cpm_types.h"   // Common type definitions (e.g., CPM_Result, Package)
#include "cpm_promise.h" // Q Promise library API
#include "cpm_package.h" // Package structure and parsing functions
#include "cpm_pmll.h"    // PMLL hardened queue for file operations

// --- CPM Global Configuration (Conceptual) ---
// In a real application, this might be a struct passed around or accessed via getters.
typedef struct {
    const char* working_directory;
    const char* modules_directory; // e.g., "cpm_modules"
    const char* registry_url;
    FILE* log_file;
    int log_level; // 0: None, 1: Error, 2: Warn, 3: Info, 4: Debug
    // Add other global settings as needed
} CPM_Config;

// --- Core CPM Lifecycle Functions ---

/**
 * @brief Initializes the CPM environment.
 *
 * This function should be called once at the beginning of the program.
 * It sets up global configurations, initializes subsystems like logging,
 * the PMLL file operations queue, and parses any global CPM configuration files (e.g., .cpmrc).
 *
 * @param config A pointer to a CPM_Config struct with initial settings.
 * If NULL, default settings will be used.
 * @return CPM_RESULT_SUCCESS on success, or an error code on failure.
 */
CPM_Result cpm_initialize(const CPM_Config* config);

/**
 * @brief Executes a CPM command.
 *
 * This is the main dispatcher for all CPM commands (install, publish, etc.).
 * It takes the command name and its arguments, then routes to the appropriate
 * command handler.
 *
 * @param command The name of the command to execute (e.g., "install", "publish").
 * @param argc The number of arguments for the command (similar to main's argc).
 * @param argv An array of argument strings for the command (similar to main's argv).
 * argv[0] is typically the command name itself if parsed from a full CLI string.
 * @return CPM_RESULT_SUCCESS on successful command execution, or an error code.
 */
CPM_Result cpm_execute_command(const char* command, int argc, char* argv[]);

/**
 * @brief Cleans up CPM resources.
 *
 * This function should be called once before the program exits.
 * It frees any globally allocated memory, closes log files,
 * ensures any pending PMLL operations are completed or handled gracefully,
 * and performs other necessary cleanup tasks.
 */
void cpm_terminate(void);


// --- Command Handler Function Prototypes (to be implemented in lib/commands/*.c) ---
// These are examples; the actual cpm_execute_command would call these.
// They are declared here to illustrate how cpm.c (main) might interact with them.
// In a full build, these would typically be in their own headers (e.g., commands/install.h)
// and cpm.c would include those specific headers or a general commands.h.

CPM_Result cpm_handle_install_command(int argc, char* argv[]);
CPM_Result cpm_handle_publish_command(int argc, char* argv[]);
CPM_Result cpm_handle_search_command(int argc, char* argv[]);
CPM_Result cpm_handle_run_script_command(int argc, char* argv[]);
CPM_Result cpm_handle_init_command(int argc, char* argv[]);
CPM_Result cpm_handle_help_command(int argc, char* argv[]);
// ... other command handlers

#endif // CPM_H

// =====================================================================================

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
#include "include/cpm.h"      // Main CPM public API
#include "include/cpm_log.h"  // For cpm_log_message (assuming it exists)

// Global CPM configuration instance
static CPM_Config global_cpm_config;
static bool cpm_initialized = false;

// --- Helper function to set default configuration ---
void cpm_set_default_config(CPM_Config* config) {
    if (!config) return;
    config->working_directory = "."; // Current directory
    config->modules_directory = "cpm_modules";
    config->registry_url = "https://registry.cpm.example.org"; // Placeholder
    config->log_file = stderr; // Default to stderr
    config->log_level = 3;     // Default to Info
}


// --- Core CPM Lifecycle Function Implementations ---

CPM_Result cpm_initialize(const CPM_Config* config) {
    if (cpm_initialized) {
        cpm_log_message(CPM_LOG_WARN, "CPM already initialized.");
        return CPM_RESULT_SUCCESS; // Or an error indicating already initialized
    }

    if (config) {
        global_cpm_config = *config; // Copy user-provided config
    } else {
        cpm_set_default_config(&global_cpm_config);
    }

    // Initialize logging subsystem (conceptual)
    // cpm_log_init(global_cpm_config.log_file, global_cpm_config.log_level);
    cpm_log_message(CPM_LOG_INFO, "CPM Initializing...");
    cpm_log_message(CPM_LOG_DEBUG, "Working Directory: %s", global_cpm_config.working_directory);
    cpm_log_message(CPM_LOG_DEBUG, "Modules Directory: %s", global_cpm_config.modules_directory);

    // Initialize PMLL hardened queue for file operations (conceptual)
    // if (!pmll_init_default_queue()) {
    //     cpm_log_message(CPM_LOG_ERROR, "Failed to initialize PMLL file operations queue.");
    //     return CPM_RESULT_ERROR_PMLL_INIT;
    // }

    // Initialize promise subsystem's event loop (if applicable and not auto-managed)
    // init_event_loop(); // From your conceptual QPROMISE_H

    // Load global .cpmrc configuration if it exists
    // cpm_config_load_global(&global_cpm_config);


    cpm_initialized = true;
    cpm_log_message(CPM_LOG_INFO, "CPM Initialization complete.");
    return CPM_RESULT_SUCCESS;
}

CPM_Result cpm_execute_command(const char* command, int argc, char* argv[]) {
    if (!cpm_initialized) {
        fprintf(stderr, "Error: CPM not initialized. Call cpm_initialize() first.\n");
        return CPM_RESULT_ERROR_NOT_INITIALIZED;
    }
    if (!command || command[0] == '\0') {
        cpm_log_message(CPM_LOG_ERROR, "No command provided.");
        cpm_handle_help_command(0, NULL); // Show help if no command
        return CPM_RESULT_ERROR_INVALID_ARGS;
    }

    cpm_log_message(CPM_LOG_INFO, "Executing command: %s", command);

    // Command dispatching
    // In a real application, argv would be passed, and each handler would parse its specific options.
    // For simplicity, we pass argc and argv directly.
    if (strcmp(command, "install") == 0) {
        return cpm_handle_install_command(argc, argv);
    } else if (strcmp(command, "publish") == 0) {
        return cpm_handle_publish_command(argc, argv);
    } else if (strcmp(command, "search") == 0) {
        return cpm_handle_search_command(argc, argv);
    } else if (strcmp(command, "run") == 0 || strcmp(command, "run-script") == 0) {
        return cpm_handle_run_script_command(argc, argv);
    } else if (strcmp(command, "init") == 0) {
        return cpm_handle_init_command(argc, argv);
    } else if (strcmp(command, "help") == 0 || strcmp(command, "--help") == 0 || strcmp(command, "-h") == 0) {
        return cpm_handle_help_command(argc, argv);
    }
    // ... other commands
    else {
        cpm_log_message(CPM_LOG_ERROR, "Unknown command: %s", command);
        cpm_handle_help_command(0, NULL); // Show help for unknown command
        return CPM_RESULT_ERROR_UNKNOWN_COMMAND;
    }
}

void cpm_terminate(void) {
    if (!cpm_initialized) {
        return; // Nothing to terminate
    }
    cpm_log_message(CPM_LOG_INFO, "CPM Terminating...");

    // Terminate PMLL queue (wait for pending operations, cleanup)
    // pmll_shutdown_default_queue();

    // Free promise subsystem's event loop resources
    // free_event_loop(); // From your conceptual QPROMISE_H

    // Close log file if it's not stderr/stdout
    // cpm_log_shutdown();

    cpm_initialized = false;
    cpm_log_message(CPM_LOG_INFO, "CPM Termination complete.");

    // Note: If global_cpm_config holds dynamically allocated strings, free them here.
}


// --- Main Function ---
int main(int argc, char* argv[]) {
    CPM_Result result;

    // Initialize CPM with default configuration
    // A real CLI might parse global options like --config, --verbose here
    // and pass them to cpm_initialize.
    result = cpm_initialize(NULL);
    if (result != CPM_RESULT_SUCCESS) {
        fprintf(stderr, "Critical error: CPM failed to initialize (code: %d).\n", result);
        return EXIT_FAILURE;
    }

    if (argc < 2) {
        // No command provided, show general help or usage
        cpm_handle_help_command(0, NULL);
        cpm_terminate();
        return EXIT_SUCCESS; // Or EXIT_FAILURE if no command is an error
    }

    const char* command = argv[1];
    // Adjust argc and argv for the command handler if needed
    // e.g., cmd_argc = argc - 1; cmd_argv = argv + 1;
    // For now, cpm_execute_command handles it.
    result = cpm_execute_command(command, argc - 1, argv + 1); // Pass command-specific args

    cpm_terminate();

    return (result == CPM_RESULT_SUCCESS) ? EXIT_SUCCESS : EXIT_FAILURE;
}


// --- Stub implementations for command handlers (to be moved to lib/commands/*.c) ---
// These are placeholders to make cpm.c compilable and demonstrate the structure.

CPM_Result cpm_handle_install_command(int argc, char* argv[]) {
    cpm_log_message(CPM_LOG_INFO, "Handler: install command");
    if (argc < 1) {
        cpm_log_message(CPM_LOG_ERROR, "install: Missing package name(s).");
        return CPM_RESULT_ERROR_INVALID_ARGS;
    }
    for (int i = 0; i < argc; ++i) {
        cpm_log_message(CPM_LOG_INFO, "Attempting to install: %s", argv[i]);
        // Actual install logic using promises and PMLL would go here.
        // Example: Promise* install_promise = cpm_core_install_package(argv[i]);
        //          promise_wait(install_promise); // Simplified blocking wait for demo
        //          promise_free(install_promise);
    }
    return CPM_RESULT_SUCCESS;
}

CPM_Result cpm_handle_publish_command(int argc, char* argv[]) {
    cpm_log_message(CPM_LOG_INFO, "Handler: publish command");
    // Actual publish logic
    return CPM_RESULT_SUCCESS;
}

CPM_Result cpm_handle_search_command(int argc, char* argv[]) {
    cpm_log_message(CPM_LOG_INFO, "Handler: search command");
    // Actual search logic
    return CPM_RESULT_SUCCESS;
}

CPM_Result cpm_handle_run_script_command(int argc, char* argv[]) {
    cpm_log_message(CPM_LOG_INFO, "Handler: run-script command");
    if (argc < 1) {
        cpm_log_message(CPM_LOG_ERROR, "run-script: Missing script name.");
        return CPM_RESULT_ERROR_INVALID_ARGS;
    }
    cpm_log_message(CPM_LOG_INFO, "Attempting to run script: %s", argv[0]);
    // Actual script running logic
    return CPM_RESULT_SUCCESS;
}

CPM_Result cpm_handle_init_command(int argc, char* argv[]) {
    cpm_log_message(CPM_LOG_INFO, "Handler: init command");
    // Actual init logic to create cpm_package.spec
    return CPM_RESULT_SUCCESS;
}

CPM_Result cpm_handle_help_command(int argc, char* argv[]) {
    // If argc > 0, argv[0] could be a command to get specific help for.
    if (argc > 0 && argv[0] != NULL) {
        printf("CPM Help for command: %s\n", argv[0]);
        printf("  (Detailed help for %s would be shown here.)\n\n", argv[0]);
    } else {
        printf("CPM - C Package Manager (Conceptual)\n");
        printf("Usage: cpm <command> [options]\n\n");
        printf("Available commands:\n");
        printf("  install <pkg...>   Install C package(s)\n");
        printf("  publish            Publish a C package\n");
        printf("  search <term>      Search for C packages\n");
        printf("  run <script>       Run a script defined in cpm_package.spec\n");
        printf("  init               Initialize a new C package (create cpm_package.spec)\n");
        printf("  help [command]     Show help\n");
        // ... other commands
        printf("\nRun 'cpm help <command>' for more information on a specific command.\n");
    }
    return CPM_RESULT_SUCCESS;
}

// Placeholder for cpm_types.h content (would be in include/cpm_types.h)
// typedef enum { CPM_RESULT_SUCCESS = 0, CPM_RESULT_ERROR_UNKNOWN_COMMAND = 1, /* ... other errors */ } CPM_Result;
// typedef struct Package Package; // Defined in cpm_package.h

// Placeholder for cpm_log.h content (would be in include/cpm_log.h or lib/core/cpm_log.h)
// #define CPM_LOG_ERROR 1
// #define CPM_LOG_WARN  2
// #define CPM_LOG_INFO  3
// #define CPM_LOG_DEBUG 4
// void cpm_log_message(int level, const char* format, ...);


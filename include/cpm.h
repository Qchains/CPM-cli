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

#include "cpm_types.h"
#include "cpm_promise.h"
#include "cpm_package.h"
#include "cpm_pmll.h"
#include <stdio.h>

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

/**
 * @brief Gets CPM version information
 * @return Version string
 */
const char* cpm_get_version(void);

/**
 * @brief Gets CPM configuration
 * @return Current configuration (read-only)
 */
const CPM_Config* cpm_get_config(void);

// --- Command Handler Function Prototypes ---
// These are implemented in lib/commands/*.c

CPM_Result cpm_handle_install_command(int argc, char* argv[], const CPM_Config* config);
CPM_Result cpm_handle_publish_command(int argc, char* argv[], const CPM_Config* config);
CPM_Result cpm_handle_search_command(int argc, char* argv[], const CPM_Config* config);
CPM_Result cpm_handle_run_script_command(int argc, char* argv[], const CPM_Config* config);
CPM_Result cpm_handle_init_command(int argc, char* argv[], const CPM_Config* config);
CPM_Result cpm_handle_help_command(int argc, char* argv[], const CPM_Config* config);
CPM_Result cpm_handle_audit_command(int argc, char* argv[], const CPM_Config* config);
CPM_Result cpm_handle_cache_command(int argc, char* argv[], const CPM_Config* config);
CPM_Result cpm_handle_config_command(int argc, char* argv[], const CPM_Config* config);
CPM_Result cpm_handle_doctor_command(int argc, char* argv[], const CPM_Config* config);
CPM_Result cpm_handle_ls_command(int argc, char* argv[], const CPM_Config* config);
CPM_Result cpm_handle_outdated_command(int argc, char* argv[], const CPM_Config* config);
CPM_Result cpm_handle_uninstall_command(int argc, char* argv[], const CPM_Config* config);
CPM_Result cpm_handle_update_command(int argc, char* argv[], const CPM_Config* config);
CPM_Result cpm_handle_version_command(int argc, char* argv[], const CPM_Config* config);
CPM_Result cpm_handle_whoami_command(int argc, char* argv[], const CPM_Config* config);

// --- Logging Functions ---
void cpm_log_message(int level, const char* format, ...);
void cpm_log_set_level(int level);
int cpm_log_get_level(void);

// --- Configuration Functions ---
CPM_Result cpm_config_load_file(const char* filepath, CPM_Config* config);
CPM_Result cpm_config_save_file(const char* filepath, const CPM_Config* config);
CPM_Result cpm_config_load_global(CPM_Config* config);

#endif // CPM_H
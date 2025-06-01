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

// --- Core CPM Lifecycle Functions ---

/**
 * @brief Initializes the CPM environment
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
 * @brief Executes a CPM command
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
 * @brief Cleans up CPM resources
 *
 * Frees global resources, ensures pending operations are handled.
 * Should be called before program exit.
 */
void cpm_terminate(void);

/**
 * @brief Gets current CPM configuration
 * @return Pointer to current configuration
 */
const CPM_Config* cpm_get_config(void);

/**
 * @brief Gets CPM version string
 * @return Version string
 */
const char* cpm_get_version(void);

// --- Command Handler Function Prototypes ---

CPM_Result cpm_handle_install_command(int argc, char* argv[], const CPM_Config* config);
CPM_Result cpm_handle_publish_command(int argc, char* argv[], const CPM_Config* config);
CPM_Result cpm_handle_search_command(int argc, char* argv[], const CPM_Config* config);
CPM_Result cpm_handle_run_script_command(int argc, char* argv[], const CPM_Config* config);
CPM_Result cpm_handle_init_command(int argc, char* argv[], const CPM_Config* config);
CPM_Result cpm_handle_help_command(int argc, char* argv[], const CPM_Config* config);
CPM_Result cpm_handle_cache_command(int argc, char* argv[], const CPM_Config* config);
CPM_Result cpm_handle_update_command(int argc, char* argv[], const CPM_Config* config);
CPM_Result cpm_handle_uninstall_command(int argc, char* argv[], const CPM_Config* config);
CPM_Result cpm_handle_list_command(int argc, char* argv[], const CPM_Config* config);
CPM_Result cpm_handle_version_command(int argc, char* argv[], const CPM_Config* config);
CPM_Result cpm_handle_config_command(int argc, char* argv[], const CPM_Config* config);
CPM_Result cpm_handle_login_command(int argc, char* argv[], const CPM_Config* config);
CPM_Result cpm_handle_logout_command(int argc, char* argv[], const CPM_Config* config);
CPM_Result cpm_handle_whoami_command(int argc, char* argv[], const CPM_Config* config);
CPM_Result cpm_handle_audit_command(int argc, char* argv[], const CPM_Config* config);
CPM_Result cpm_handle_doctor_command(int argc, char* argv[], const CPM_Config* config);

#endif // CPM_H
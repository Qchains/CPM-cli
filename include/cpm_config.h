/*
 * File: cpm_config.h
 * Description: Configuration management for CPM - handles .cpmrc files and user settings
 * Author: Dr. Q Josef Kurk Edwards
 */

#ifndef CPM_CONFIG_H
#define CPM_CONFIG_H

#include <stdbool.h>
#include "cpm_types.h"

// --- Configuration Structure ---
typedef struct CPM_Config {
    // Registry settings
    char* registry_url;
    char* auth_token;
    bool verify_ssl;
    
    // Local settings
    char* cache_dir;
    char* global_modules_dir;
    char* temp_dir;
    
    // Build settings
    char* default_compiler;
    char* default_cflags;
    char* default_ldflags;
    
    // Behavior settings
    bool verbose;
    bool quiet;
    bool force;
    bool save_exact;
    int timeout_seconds;
    
    // User information
    char* author_name;
    char* author_email;
    char* default_license;
    
    // Advanced settings
    int max_concurrent_downloads;
    bool use_package_lock;
    bool auto_install_deps;
} CPM_Config;

// --- Configuration Loading ---
CPM_Config* cpm_config_create_default(void);
CPM_Config* cpm_config_load(void);
CPM_Config* cpm_config_load_from_file(const char* config_file);
void cpm_config_free(CPM_Config* config);

// --- Configuration Saving ---
CPM_Result cpm_config_save(const CPM_Config* config);
CPM_Result cpm_config_save_to_file(const CPM_Config* config, const char* config_file);

// --- Configuration Access ---
const char* cpm_config_get_string(const CPM_Config* config, const char* key);
bool cpm_config_get_bool(const CPM_Config* config, const char* key);
int cpm_config_get_int(const CPM_Config* config, const char* key);

// --- Configuration Modification ---
CPM_Result cpm_config_set_string(CPM_Config* config, const char* key, const char* value);
CPM_Result cpm_config_set_bool(CPM_Config* config, const char* key, bool value);
CPM_Result cpm_config_set_int(CPM_Config* config, const char* key, int value);

// --- Configuration Utilities ---
char* cpm_config_get_global_config_path(void);
char* cpm_config_get_local_config_path(void);
char* cpm_config_get_cache_dir(const CPM_Config* config);
char* cpm_config_get_modules_dir(const CPM_Config* config);

// --- Environment Variable Support ---
CPM_Config* cpm_config_merge_with_env(const CPM_Config* base_config);
void cpm_config_apply_command_line_args(CPM_Config* config, int argc, char* argv[]);

#endif // CPM_CONFIG_H
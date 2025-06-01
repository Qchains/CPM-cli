/*
 * File: cpm_config.c
 * Description: Configuration management implementation for CPM
 * Author: Dr. Q Josef Kurk Edwards
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pwd.h>
#include "cpm_config.h"

// --- Internal Helper Functions ---
static char* strdup_safe(const char* str) {
    if (!str) return NULL;
    return strdup(str);
}

static char* get_home_directory(void) {
    char* home = getenv("HOME");
    if (home) return strdup(home);
    
    struct passwd* pw = getpwuid(getuid());
    if (pw && pw->pw_dir) return strdup(pw->pw_dir);
    
    return strdup("/tmp");
}

static bool file_exists(const char* path) {
    return access(path, F_OK) == 0;
}

static char* join_path(const char* dir, const char* file) {
    if (!dir || !file) return NULL;
    
    size_t dir_len = strlen(dir);
    size_t file_len = strlen(file);
    size_t total_len = dir_len + file_len + 2; // +1 for '/', +1 for '\0'
    
    char* result = malloc(total_len);
    if (!result) return NULL;
    
    snprintf(result, total_len, "%s/%s", dir, file);
    return result;
}

// --- Configuration Creation ---
CPM_Config* cpm_config_create_default(void) {
    CPM_Config* config = calloc(1, sizeof(CPM_Config));
    if (!config) return NULL;
    
    // Registry settings
    config->registry_url = strdup("http://localhost:8080");
    config->verify_ssl = true;
    
    // Local settings
    char* home = get_home_directory();
    config->cache_dir = join_path(home, ".cpm/cache");
    config->global_modules_dir = join_path(home, ".cpm/global");
    config->temp_dir = strdup("/tmp/cpm");
    free(home);
    
    // Build settings
    config->default_compiler = strdup("gcc");
    config->default_cflags = strdup("-Wall -Wextra -std=c11 -O2");
    config->default_ldflags = strdup("");
    
    // Behavior settings
    config->verbose = false;
    config->quiet = false;
    config->force = false;
    config->save_exact = false;
    config->timeout_seconds = 30;
    
    // User information
    config->author_name = strdup("");
    config->author_email = strdup("");
    config->default_license = strdup("MIT");
    
    // Advanced settings
    config->max_concurrent_downloads = 4;
    config->use_package_lock = true;
    config->auto_install_deps = true;
    
    return config;
}

void cpm_config_free(CPM_Config* config) {
    if (!config) return;
    
    free(config->registry_url);
    free(config->auth_token);
    free(config->cache_dir);
    free(config->global_modules_dir);
    free(config->temp_dir);
    free(config->default_compiler);
    free(config->default_cflags);
    free(config->default_ldflags);
    free(config->author_name);
    free(config->author_email);
    free(config->default_license);
    free(config);
}

// --- Configuration File Parsing ---
static void parse_config_line(CPM_Config* config, const char* line) {
    if (!config || !line) return;
    
    // Skip comments and empty lines
    const char* trimmed = line;
    while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
    if (*trimmed == '#' || *trimmed == '\0' || *trimmed == '\n') return;
    
    // Find equals sign
    const char* equals = strchr(trimmed, '=');
    if (!equals) return;
    
    // Extract key and value
    size_t key_len = equals - trimmed;
    char* key = malloc(key_len + 1);
    if (!key) return;
    
    strncpy(key, trimmed, key_len);
    key[key_len] = '\0';
    
    // Trim key
    char* key_end = key + key_len - 1;
    while (key_end > key && (*key_end == ' ' || *key_end == '\t')) *key_end-- = '\0';
    
    // Get value
    const char* value = equals + 1;
    while (*value == ' ' || *value == '\t') value++;
    
    // Remove trailing newline and whitespace
    char* value_copy = strdup(value);
    if (!value_copy) {
        free(key);
        return;
    }
    
    char* value_end = value_copy + strlen(value_copy) - 1;
    while (value_end > value_copy && (*value_end == '\n' || *value_end == '\r' || *value_end == ' ' || *value_end == '\t')) {
        *value_end-- = '\0';
    }
    
    // Set configuration value
    if (strcmp(key, "registry") == 0) {
        free(config->registry_url);
        config->registry_url = strdup(value_copy);
    } else if (strcmp(key, "auth_token") == 0) {
        free(config->auth_token);
        config->auth_token = strdup(value_copy);
    } else if (strcmp(key, "verify_ssl") == 0) {
        config->verify_ssl = (strcmp(value_copy, "true") == 0 || strcmp(value_copy, "1") == 0);
    } else if (strcmp(key, "cache_dir") == 0) {
        free(config->cache_dir);
        config->cache_dir = strdup(value_copy);
    } else if (strcmp(key, "global_modules_dir") == 0) {
        free(config->global_modules_dir);
        config->global_modules_dir = strdup(value_copy);
    } else if (strcmp(key, "temp_dir") == 0) {
        free(config->temp_dir);
        config->temp_dir = strdup(value_copy);
    } else if (strcmp(key, "default_compiler") == 0) {
        free(config->default_compiler);
        config->default_compiler = strdup(value_copy);
    } else if (strcmp(key, "default_cflags") == 0) {
        free(config->default_cflags);
        config->default_cflags = strdup(value_copy);
    } else if (strcmp(key, "default_ldflags") == 0) {
        free(config->default_ldflags);
        config->default_ldflags = strdup(value_copy);
    } else if (strcmp(key, "verbose") == 0) {
        config->verbose = (strcmp(value_copy, "true") == 0 || strcmp(value_copy, "1") == 0);
    } else if (strcmp(key, "quiet") == 0) {
        config->quiet = (strcmp(value_copy, "true") == 0 || strcmp(value_copy, "1") == 0);
    } else if (strcmp(key, "force") == 0) {
        config->force = (strcmp(value_copy, "true") == 0 || strcmp(value_copy, "1") == 0);
    } else if (strcmp(key, "save_exact") == 0) {
        config->save_exact = (strcmp(value_copy, "true") == 0 || strcmp(value_copy, "1") == 0);
    } else if (strcmp(key, "timeout_seconds") == 0) {
        config->timeout_seconds = atoi(value_copy);
    } else if (strcmp(key, "author_name") == 0) {
        free(config->author_name);
        config->author_name = strdup(value_copy);
    } else if (strcmp(key, "author_email") == 0) {
        free(config->author_email);
        config->author_email = strdup(value_copy);
    } else if (strcmp(key, "default_license") == 0) {
        free(config->default_license);
        config->default_license = strdup(value_copy);
    } else if (strcmp(key, "max_concurrent_downloads") == 0) {
        config->max_concurrent_downloads = atoi(value_copy);
    } else if (strcmp(key, "use_package_lock") == 0) {
        config->use_package_lock = (strcmp(value_copy, "true") == 0 || strcmp(value_copy, "1") == 0);
    } else if (strcmp(key, "auto_install_deps") == 0) {
        config->auto_install_deps = (strcmp(value_copy, "true") == 0 || strcmp(value_copy, "1") == 0);
    }
    
    free(key);
    free(value_copy);
}

CPM_Config* cpm_config_load_from_file(const char* config_file) {
    if (!config_file || !file_exists(config_file)) {
        return cpm_config_create_default();
    }
    
    CPM_Config* config = cpm_config_create_default();
    if (!config) return NULL;
    
    FILE* f = fopen(config_file, "r");
    if (!f) return config;
    
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        parse_config_line(config, line);
    }
    
    fclose(f);
    return config;
}

// --- Configuration Loading with Hierarchy ---
CPM_Config* cpm_config_load(void) {
    // Load global config first
    char* global_config_path = cpm_config_get_global_config_path();
    CPM_Config* config = cpm_config_load_from_file(global_config_path);
    free(global_config_path);
    
    if (!config) return NULL;
    
    // Override with local config
    char* local_config_path = cpm_config_get_local_config_path();
    if (local_config_path && file_exists(local_config_path)) {
        FILE* f = fopen(local_config_path, "r");
        if (f) {
            char line[1024];
            while (fgets(line, sizeof(line), f)) {
                parse_config_line(config, line);
            }
            fclose(f);
        }
    }
    free(local_config_path);
    
    // Apply environment variables
    CPM_Config* final_config = cpm_config_merge_with_env(config);
    cpm_config_free(config);
    
    return final_config;
}

// --- Configuration Saving ---
CPM_Result cpm_config_save_to_file(const CPM_Config* config, const char* config_file) {
    if (!config || !config_file) return CPM_RESULT_ERROR_INVALID_ARGS;
    
    FILE* f = fopen(config_file, "w");
    if (!f) return CPM_RESULT_ERROR_FILE_OPERATION;
    
    fprintf(f, "# CPM Configuration File\n");
    fprintf(f, "# Generated automatically - edit with caution\n\n");
    
    fprintf(f, "# Registry settings\n");
    fprintf(f, "registry=%s\n", config->registry_url ? config->registry_url : "");
    if (config->auth_token && strlen(config->auth_token) > 0) {
        fprintf(f, "auth_token=%s\n", config->auth_token);
    }
    fprintf(f, "verify_ssl=%s\n", config->verify_ssl ? "true" : "false");
    fprintf(f, "\n");
    
    fprintf(f, "# Local settings\n");
    fprintf(f, "cache_dir=%s\n", config->cache_dir ? config->cache_dir : "");
    fprintf(f, "global_modules_dir=%s\n", config->global_modules_dir ? config->global_modules_dir : "");
    fprintf(f, "temp_dir=%s\n", config->temp_dir ? config->temp_dir : "");
    fprintf(f, "\n");
    
    fprintf(f, "# Build settings\n");
    fprintf(f, "default_compiler=%s\n", config->default_compiler ? config->default_compiler : "");
    fprintf(f, "default_cflags=%s\n", config->default_cflags ? config->default_cflags : "");
    fprintf(f, "default_ldflags=%s\n", config->default_ldflags ? config->default_ldflags : "");
    fprintf(f, "\n");
    
    fprintf(f, "# Behavior settings\n");
    fprintf(f, "verbose=%s\n", config->verbose ? "true" : "false");
    fprintf(f, "quiet=%s\n", config->quiet ? "true" : "false");
    fprintf(f, "force=%s\n", config->force ? "true" : "false");
    fprintf(f, "save_exact=%s\n", config->save_exact ? "true" : "false");
    fprintf(f, "timeout_seconds=%d\n", config->timeout_seconds);
    fprintf(f, "\n");
    
    fprintf(f, "# User information\n");
    fprintf(f, "author_name=%s\n", config->author_name ? config->author_name : "");
    fprintf(f, "author_email=%s\n", config->author_email ? config->author_email : "");
    fprintf(f, "default_license=%s\n", config->default_license ? config->default_license : "");
    fprintf(f, "\n");
    
    fprintf(f, "# Advanced settings\n");
    fprintf(f, "max_concurrent_downloads=%d\n", config->max_concurrent_downloads);
    fprintf(f, "use_package_lock=%s\n", config->use_package_lock ? "true" : "false");
    fprintf(f, "auto_install_deps=%s\n", config->auto_install_deps ? "true" : "false");
    
    fclose(f);
    return CPM_RESULT_SUCCESS;
}

CPM_Result cpm_config_save(const CPM_Config* config) {
    char* global_config_path = cpm_config_get_global_config_path();
    if (!global_config_path) return CPM_RESULT_ERROR_FILE_OPERATION;
    
    // Ensure directory exists
    char* dir_end = strrchr(global_config_path, '/');
    if (dir_end) {
        *dir_end = '\0';
        mkdir(global_config_path, 0755);
        *dir_end = '/';
    }
    
    CPM_Result result = cpm_config_save_to_file(config, global_config_path);
    free(global_config_path);
    return result;
}

// --- Configuration Path Utilities ---
char* cpm_config_get_global_config_path(void) {
    char* home = get_home_directory();
    if (!home) return NULL;
    
    char* config_path = join_path(home, ".cpmrc");
    free(home);
    return config_path;
}

char* cpm_config_get_local_config_path(void) {
    return strdup("./.cpmrc");
}

char* cpm_config_get_cache_dir(const CPM_Config* config) {
    if (!config) return NULL;
    return strdup(config->cache_dir ? config->cache_dir : "/tmp/cpm/cache");
}

char* cpm_config_get_modules_dir(const CPM_Config* config) {
    if (!config) return NULL;
    return strdup(config->global_modules_dir ? config->global_modules_dir : "/usr/local/lib/cpm");
}

// --- Environment Variable Support ---
CPM_Config* cpm_config_merge_with_env(const CPM_Config* base_config) {
    if (!base_config) return NULL;
    
    CPM_Config* config = calloc(1, sizeof(CPM_Config));
    if (!config) return NULL;
    
    // Copy base config
    memcpy(config, base_config, sizeof(CPM_Config));
    
    // Duplicate strings
    config->registry_url = strdup_safe(base_config->registry_url);
    config->auth_token = strdup_safe(base_config->auth_token);
    config->cache_dir = strdup_safe(base_config->cache_dir);
    config->global_modules_dir = strdup_safe(base_config->global_modules_dir);
    config->temp_dir = strdup_safe(base_config->temp_dir);
    config->default_compiler = strdup_safe(base_config->default_compiler);
    config->default_cflags = strdup_safe(base_config->default_cflags);
    config->default_ldflags = strdup_safe(base_config->default_ldflags);
    config->author_name = strdup_safe(base_config->author_name);
    config->author_email = strdup_safe(base_config->author_email);
    config->default_license = strdup_safe(base_config->default_license);
    
    // Override with environment variables
    char* env_value;
    
    if ((env_value = getenv("CPM_REGISTRY")) != NULL) {
        free(config->registry_url);
        config->registry_url = strdup(env_value);
    }
    
    if ((env_value = getenv("CPM_AUTH_TOKEN")) != NULL) {
        free(config->auth_token);
        config->auth_token = strdup(env_value);
    }
    
    if ((env_value = getenv("CPM_CACHE_DIR")) != NULL) {
        free(config->cache_dir);
        config->cache_dir = strdup(env_value);
    }
    
    if ((env_value = getenv("CPM_VERBOSE")) != NULL) {
        config->verbose = (strcmp(env_value, "true") == 0 || strcmp(env_value, "1") == 0);
    }
    
    if ((env_value = getenv("CPM_QUIET")) != NULL) {
        config->quiet = (strcmp(env_value, "true") == 0 || strcmp(env_value, "1") == 0);
    }
    
    return config;
}

// --- Command Line Argument Support ---
void cpm_config_apply_command_line_args(CPM_Config* config, int argc, char* argv[]) {
    if (!config || !argv) return;
    
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--verbose") == 0) {
            config->verbose = true;
        } else if (strcmp(argv[i], "--quiet") == 0) {
            config->quiet = true;
        } else if (strcmp(argv[i], "--force") == 0) {
            config->force = true;
        } else if (strncmp(argv[i], "--registry=", 11) == 0) {
            free(config->registry_url);
            config->registry_url = strdup(argv[i] + 11);
        } else if (strncmp(argv[i], "--timeout=", 10) == 0) {
            config->timeout_seconds = atoi(argv[i] + 10);
        }
    }
}
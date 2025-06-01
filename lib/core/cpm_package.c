/*
 * File: lib/core/cpm_package.c
 * Description: Package structure and parsing functions for CPM.
 * Handles cpm_package.spec files and package metadata.
 * Author: Dr. Q Josef Kurk Edwards
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "cpm_package.h"
#include "cpm_promise.h"
#include "cpm_pmll.h"

// --- Package Creation and Destruction ---
Package* cpm_create_package(void) {
    Package* pkg = (Package*)malloc(sizeof(Package));
    if (!pkg) {
        return NULL;
    }
    
    memset(pkg, 0, sizeof(Package));
    return pkg;
}

void cpm_free_package(Package* pkg) {
    if (!pkg) return;
    
    free(pkg->name);
    free(pkg->version);
    free(pkg->description);
    free(pkg->author);
    free(pkg->license);
    
    // Free dependencies
    for (size_t i = 0; i < pkg->dep_count; i++) {
        free(pkg->dependencies[i]);
    }
    free(pkg->dependencies);
    
    // Free scripts
    for (size_t i = 0; i < pkg->script_count; i++) {
        free(pkg->scripts[i]);
    }
    free(pkg->scripts);
    
    free(pkg->build_command);
    free(pkg->install_command);
    free(pkg->test_command);
    free(pkg);
}

// --- Simple JSON-like Parser for cpm_package.spec ---
// This is a simplified parser - in a real implementation, you'd use a proper JSON library

char* extract_json_string_value(const char* json, const char* key) {
    char search_pattern[256];
    snprintf(search_pattern, sizeof(search_pattern), "\"%s\":", key);
    
    char* key_pos = strstr(json, search_pattern);
    if (!key_pos) {
        return NULL;
    }
    
    // Move to after the key
    key_pos += strlen(search_pattern);
    
    // Skip whitespace
    while (*key_pos == ' ' || *key_pos == '\t' || *key_pos == '\n') {
        key_pos++;
    }
    
    // Expect opening quote
    if (*key_pos != '"') {
        return NULL;
    }
    key_pos++; // Skip opening quote
    
    // Find closing quote
    char* end_pos = strchr(key_pos, '"');
    if (!end_pos) {
        return NULL;
    }
    
    // Extract value
    size_t value_len = end_pos - key_pos;
    char* value = (char*)malloc(value_len + 1);
    if (!value) {
        return NULL;
    }
    
    strncpy(value, key_pos, value_len);
    value[value_len] = '\0';
    
    return value;
}

char** extract_json_array_values(const char* json, const char* key, size_t* count) {
    *count = 0;
    
    char search_pattern[256];
    snprintf(search_pattern, sizeof(search_pattern), "\"%s\":", key);
    
    char* key_pos = strstr(json, search_pattern);
    if (!key_pos) {
        return NULL;
    }
    
    // Move to after the key
    key_pos += strlen(search_pattern);
    
    // Skip whitespace
    while (*key_pos == ' ' || *key_pos == '\t' || *key_pos == '\n') {
        key_pos++;
    }
    
    // Expect opening bracket
    if (*key_pos != '[') {
        return NULL;
    }
    key_pos++;
    
    // Find closing bracket
    char* end_pos = strchr(key_pos, ']');
    if (!end_pos) {
        return NULL;
    }
    
    // Simple array parsing - split by commas and extract quoted strings
    char** values = NULL;
    size_t capacity = 0;
    
    char* current = key_pos;
    while (current < end_pos) {
        // Skip whitespace
        while (*current == ' ' || *current == '\t' || *current == '\n' || *current == ',') {
            current++;
        }
        
        if (current >= end_pos || *current != '"') {
            break;
        }
        
        current++; // Skip opening quote
        char* value_end = strchr(current, '"');
        if (!value_end || value_end > end_pos) {
            break;
        }
        
        // Extract value
        size_t value_len = value_end - current;
        char* value = (char*)malloc(value_len + 1);
        if (!value) {
            break;
        }
        
        strncpy(value, current, value_len);
        value[value_len] = '\0';
        
        // Add to array
        if (*count >= capacity) {
            capacity = capacity == 0 ? 4 : capacity * 2;
            values = (char**)realloc(values, capacity * sizeof(char*));
            if (!values) {
                free(value);
                break;
            }
        }
        
        values[*count] = value;
        (*count)++;
        
        current = value_end + 1;
    }
    
    return values;
}

Package* cpm_parse_package_file(const char* filepath) {
    FILE* fp = fopen(filepath, "r");
    if (!fp) {
        printf("[CPM] Could not open package file: %s\n", filepath);
        return NULL;
    }
    
    // Read entire file
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    char* content = (char*)malloc(file_size + 1);
    if (!content) {
        fclose(fp);
        return NULL;
    }
    
    fread(content, 1, file_size, fp);
    content[file_size] = '\0';
    fclose(fp);
    
    Package* pkg = cpm_create_package();
    if (!pkg) {
        free(content);
        return NULL;
    }
    
    // Parse JSON-like content
    pkg->name = extract_json_string_value(content, "name");
    pkg->version = extract_json_string_value(content, "version");
    pkg->description = extract_json_string_value(content, "description");
    pkg->author = extract_json_string_value(content, "author");
    pkg->license = extract_json_string_value(content, "license");
    pkg->build_command = extract_json_string_value(content, "build");
    pkg->install_command = extract_json_string_value(content, "install");
    pkg->test_command = extract_json_string_value(content, "test");
    
    // Parse dependencies array
    pkg->dependencies = extract_json_array_values(content, "dependencies", &pkg->dep_count);
    
    // Parse scripts array
    pkg->scripts = extract_json_array_values(content, "scripts", &pkg->script_count);
    
    free(content);
    
    printf("[CPM] Parsed package: %s@%s\n", 
           pkg->name ? pkg->name : "unknown", 
           pkg->version ? pkg->version : "unknown");
    
    return pkg;
}

CPM_Result cpm_save_package_file(const Package* pkg, const char* filepath) {
    if (!pkg || !filepath) {
        return CPM_RESULT_ERROR_INVALID_ARGS;
    }
    
    FILE* fp = fopen(filepath, "w");
    if (!fp) {
        return CPM_RESULT_ERROR_FILE_OPERATION;
    }
    
    fprintf(fp, "{\n");
    
    if (pkg->name) {
        fprintf(fp, "  \"name\": \"%s\",\n", pkg->name);
    }
    if (pkg->version) {
        fprintf(fp, "  \"version\": \"%s\",\n", pkg->version);
    }
    if (pkg->description) {
        fprintf(fp, "  \"description\": \"%s\",\n", pkg->description);
    }
    if (pkg->author) {
        fprintf(fp, "  \"author\": \"%s\",\n", pkg->author);
    }
    if (pkg->license) {
        fprintf(fp, "  \"license\": \"%s\",\n", pkg->license);
    }
    
    // Dependencies
    if (pkg->dependencies && pkg->dep_count > 0) {
        fprintf(fp, "  \"dependencies\": [\n");
        for (size_t i = 0; i < pkg->dep_count; i++) {
            fprintf(fp, "    \"%s\"%s\n", pkg->dependencies[i], 
                   (i < pkg->dep_count - 1) ? "," : "");
        }
        fprintf(fp, "  ],\n");
    }
    
    // Scripts
    if (pkg->scripts && pkg->script_count > 0) {
        fprintf(fp, "  \"scripts\": [\n");
        for (size_t i = 0; i < pkg->script_count; i++) {
            fprintf(fp, "    \"%s\"%s\n", pkg->scripts[i], 
                   (i < pkg->script_count - 1) ? "," : "");
        }
        fprintf(fp, "  ],\n");
    }
    
    if (pkg->build_command) {
        fprintf(fp, "  \"build\": \"%s\",\n", pkg->build_command);
    }
    if (pkg->install_command) {
        fprintf(fp, "  \"install\": \"%s\",\n", pkg->install_command);
    }
    if (pkg->test_command) {
        fprintf(fp, "  \"test\": \"%s\"\n", pkg->test_command);
    }
    
    fprintf(fp, "}\n");
    fclose(fp);
    
    printf("[CPM] Saved package file: %s\n", filepath);
    return CPM_RESULT_SUCCESS;
}

// --- Package Validation ---
bool cpm_package_validate(const Package* pkg) {
    if (!pkg) return false;
    
    // Must have name and version
    if (!pkg->name || !pkg->version) {
        return false;
    }
    
    // Name must not be empty
    if (strlen(pkg->name) == 0) {
        return false;
    }
    
    // Version must not be empty
    if (strlen(pkg->version) == 0) {
        return false;
    }
    
    return true;
}

bool cpm_package_spec_exists(const char* directory) {
    char spec_path[512];
    snprintf(spec_path, sizeof(spec_path), "%s/cpm_package.spec", directory);
    
    struct stat st;
    return (stat(spec_path, &st) == 0);
}

// --- Asynchronous Package Operations ---
typedef struct {
    Package* pkg;
    char* install_dir;
    PromiseDeferred* deferred;
} PackageInstallData;

PromiseValue package_install_operation(PromiseValue prev_result, void* user_data) {
    PackageInstallData* data = (PackageInstallData*)user_data;
    
    printf("[CPM] Installing package %s@%s to %s\n", 
           data->pkg->name, data->pkg->version, data->install_dir);
    
    // Create package directory
    char pkg_dir[512];
    snprintf(pkg_dir, sizeof(pkg_dir), "%s/%s", data->install_dir, data->pkg->name);
    
    if (mkdir(pkg_dir, 0755) == -1) {
        char* error = strdup("Failed to create package directory");
        promise_defer_reject(data->deferred, error);
        free(data->install_dir);
        free(data);
        return error;
    }
    
    // Save package spec
    char spec_path[512];
    snprintf(spec_path, sizeof(spec_path), "%s/cpm_package.spec", pkg_dir);
    
    if (cpm_save_package_file(data->pkg, spec_path) != CPM_RESULT_SUCCESS) {
        char* error = strdup("Failed to save package spec");
        promise_defer_reject(data->deferred, error);
        free(data->install_dir);
        free(data);
        return error;
    }
    
    // Execute install command if present
    if (data->pkg->install_command) {
        char install_cmd[1024];
        snprintf(install_cmd, sizeof(install_cmd), "cd %s && %s", pkg_dir, data->pkg->install_command);
        
        int result = system(install_cmd);
        if (result != 0) {
            char* error = strdup("Package install command failed");
            promise_defer_reject(data->deferred, error);
            free(data->install_dir);
            free(data);
            return error;
        }
    }
    
    char* success = strdup("Package installed successfully");
    promise_defer_resolve(data->deferred, success);
    
    free(data->install_dir);
    free(data);
    return success;
}

Promise* cpm_package_install_async(const Package* pkg, const char* install_dir) {
    if (!pkg || !install_dir) {
        return NULL;
    }
    
    PackageInstallData* data = (PackageInstallData*)malloc(sizeof(PackageInstallData));
    if (!data) {
        return NULL;
    }
    
    data->pkg = (Package*)pkg; // Cast away const for this operation
    data->install_dir = strdup(install_dir);
    data->deferred = promise_defer_create();
    
    if (!data->install_dir || !data->deferred) {
        free(data->install_dir);
        if (data->deferred) promise_defer_free(data->deferred);
        free(data);
        return NULL;
    }
    
    PMLL_HardenedResourceQueue* file_queue = pmll_get_default_file_queue();
    if (!file_queue) {
        free(data->install_dir);
        promise_defer_free(data->deferred);
        free(data);
        return NULL;
    }
    
    return pmll_execute_hardened_operation(
        file_queue,
        package_install_operation,
        NULL,
        data
    );
}

// --- Package Resolution (Mock Implementation) ---
Package* cpm_package_resolve_remote(const char* package_spec, const char* registry_url) {
    printf("[CPM] Resolving package: %s from registry: %s\n", package_spec, registry_url);
    
    // Mock implementation - in real system would make HTTP requests
    Package* pkg = cpm_create_package();
    if (!pkg) return NULL;
    
    // Parse package_spec (name@version format)
    char* at_pos = strchr(package_spec, '@');
    if (at_pos) {
        size_t name_len = at_pos - package_spec;
        pkg->name = (char*)malloc(name_len + 1);
        if (pkg->name) {
            strncpy(pkg->name, package_spec, name_len);
            pkg->name[name_len] = '\0';
        }
        pkg->version = strdup(at_pos + 1);
    } else {
        pkg->name = strdup(package_spec);
        pkg->version = strdup("latest");
    }
    
    // Mock metadata
    pkg->description = strdup("Mock package from remote registry");
    pkg->author = strdup("Unknown Author");
    pkg->license = strdup("MIT");
    
    return pkg;
}

typedef struct {
    Package* pkg;
    char* package_dir;
    PromiseDeferred* deferred;
} PackageBuildData;

PromiseValue package_build_operation(PromiseValue prev_result, void* user_data) {
    PackageBuildData* data = (PackageBuildData*)user_data;
    
    printf("[CPM] Building package %s in %s\n", data->pkg->name, data->package_dir);
    
    if (data->pkg->build_command) {
        char build_cmd[1024];
        snprintf(build_cmd, sizeof(build_cmd), "cd %s && %s", data->package_dir, data->pkg->build_command);
        
        int result = system(build_cmd);
        if (result != 0) {
            char* error = strdup("Package build command failed");
            promise_defer_reject(data->deferred, error);
            free(data->package_dir);
            free(data);
            return error;
        }
    }
    
    char* success = strdup("Package built successfully");
    promise_defer_resolve(data->deferred, success);
    
    free(data->package_dir);
    free(data);
    return success;
}

Promise* cpm_package_build_async(const Package* pkg, const char* package_dir) {
    if (!pkg || !package_dir) {
        return NULL;
    }
    
    PackageBuildData* data = (PackageBuildData*)malloc(sizeof(PackageBuildData));
    if (!data) {
        return NULL;
    }
    
    data->pkg = (Package*)pkg;
    data->package_dir = strdup(package_dir);
    data->deferred = promise_defer_create();
    
    if (!data->package_dir || !data->deferred) {
        free(data->package_dir);
        if (data->deferred) promise_defer_free(data->deferred);
        free(data);
        return NULL;
    }
    
    PMLL_HardenedResourceQueue* file_queue = pmll_get_default_file_queue();
    if (!file_queue) {
        free(data->package_dir);
        promise_defer_free(data->deferred);
        free(data);
        return NULL;
    }
    
    return pmll_execute_hardened_operation(
        file_queue,
        package_build_operation,
        NULL,
        data
    );
}
/*
 * File: lib/core/cpm_package.c
 * Description: Package structure and parsing implementation for CPM
 * Author: Dr. Q Josef Kurk Edwards
 */

#include "../../include/cpm_package.h"
#include "../../include/cpm_promise.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

// --- Package Creation and Destruction ---

Package* cpm_package_create(void) {
    Package* pkg = malloc(sizeof(Package));
    if (!pkg) return NULL;
    
    memset(pkg, 0, sizeof(Package));
    
    // Initialize dynamic arrays
    pkg->dep_capacity = 4;
    pkg->dependencies = malloc(pkg->dep_capacity * sizeof(char*));
    
    pkg->dev_dep_capacity = 4;
    pkg->dev_dependencies = malloc(pkg->dev_dep_capacity * sizeof(char*));
    
    pkg->script_capacity = 4;
    pkg->scripts = malloc(pkg->script_capacity * sizeof(char*));
    
    pkg->include_path_count = 0;
    pkg->include_paths = malloc(4 * sizeof(char*));
    
    pkg->library_path_count = 0;
    pkg->library_paths = malloc(4 * sizeof(char*));
    
    pkg->linked_library_count = 0;
    pkg->linked_libraries = malloc(4 * sizeof(char*));
    
    if (!pkg->dependencies || !pkg->dev_dependencies || !pkg->scripts ||
        !pkg->include_paths || !pkg->library_paths || !pkg->linked_libraries) {
        cpm_package_free(pkg);
        return NULL;
    }
    
    return pkg;
}

void cpm_package_free(Package* pkg) {
    if (!pkg) return;
    
    // Free basic strings
    free(pkg->name);
    free(pkg->version);
    free(pkg->description);
    free(pkg->author);
    free(pkg->license);
    free(pkg->homepage);
    free(pkg->repository_url);
    free(pkg->build_command);
    free(pkg->test_command);
    free(pkg->install_command);
    free(pkg->pmem_pool_size);
    
    // Free dependency arrays
    if (pkg->dependencies) {
        for (size_t i = 0; i < pkg->dep_count; i++) {
            free(pkg->dependencies[i]);
        }
        free(pkg->dependencies);
    }
    
    if (pkg->dev_dependencies) {
        for (size_t i = 0; i < pkg->dev_dep_count; i++) {
            free(pkg->dev_dependencies[i]);
        }
        free(pkg->dev_dependencies);
    }
    
    // Free script arrays
    if (pkg->scripts) {
        for (size_t i = 0; i < pkg->script_count; i++) {
            free(pkg->scripts[i]);
        }
        free(pkg->scripts);
    }
    
    // Free include paths
    if (pkg->include_paths) {
        for (size_t i = 0; i < pkg->include_path_count; i++) {
            free(pkg->include_paths[i]);
        }
        free(pkg->include_paths);
    }
    
    // Free library paths
    if (pkg->library_paths) {
        for (size_t i = 0; i < pkg->library_path_count; i++) {
            free(pkg->library_paths[i]);
        }
        free(pkg->library_paths);
    }
    
    // Free linked libraries
    if (pkg->linked_libraries) {
        for (size_t i = 0; i < pkg->linked_library_count; i++) {
            free(pkg->linked_libraries[i]);
        }
        free(pkg->linked_libraries);
    }
    
    free(pkg);
}

// --- Package Parsing ---

Package* cpm_parse_package_file(const char* filepath) {
    if (!filepath) return NULL;
    
    FILE* fp = fopen(filepath, "r");
    if (!fp) {
        printf("[CPM] Failed to open package file: %s\n", filepath);
        return NULL;
    }
    
    // Read entire file
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    if (file_size <= 0) {
        fclose(fp);
        return NULL;
    }
    
    char* json_data = malloc(file_size + 1);
    if (!json_data) {
        fclose(fp);
        return NULL;
    }
    
    size_t read_size = fread(json_data, 1, file_size, fp);
    fclose(fp);
    json_data[read_size] = '';
    
    Package* pkg = cpm_parse_package_json(json_data);
    free(json_data);
    
    return pkg;
}

static void parse_string_array(cJSON* json_array, char*** array, size_t* count, size_t* capacity) {
    if (!cJSON_IsArray(json_array)) return;
    
    int array_size = cJSON_GetArraySize(json_array);
    
    // Ensure capacity
    if (*capacity < (size_t)array_size) {
        *capacity = array_size;
        *array = realloc(*array, *capacity * sizeof(char*));
    }
    
    *count = 0;
    cJSON* item = NULL;
    cJSON_ArrayForEach(item, json_array) {
        if (cJSON_IsString(item)) {
            (*array)[*count] = strdup(cJSON_GetStringValue(item));
            (*count)++;
        }
    }
}

Package* cpm_parse_package_json(const char* json_data) {
    if (!json_data) return NULL;
    
    cJSON* json = cJSON_Parse(json_data);
    if (!json) {
        printf("[CPM] Failed to parse JSON package data\n");
        return NULL;
    }
    
    Package* pkg = cpm_package_create();
    if (!pkg) {
        cJSON_Delete(json);
        return NULL;
    }
    
    // Parse basic fields
    cJSON* name = cJSON_GetObjectItem(json, "name");
    if (cJSON_IsString(name)) {
        pkg->name = strdup(cJSON_GetStringValue(name));
    }
    
    cJSON* version = cJSON_GetObjectItem(json, "version");
    if (cJSON_IsString(version)) {
        pkg->version = strdup(cJSON_GetStringValue(version));
    }
    
    cJSON* description = cJSON_GetObjectItem(json, "description");
    if (cJSON_IsString(description)) {
        pkg->description = strdup(cJSON_GetStringValue(description));
    }
    
    cJSON* author = cJSON_GetObjectItem(json, "author");
    if (cJSON_IsString(author)) {
        pkg->author = strdup(cJSON_GetStringValue(author));
    }
    
    cJSON* license = cJSON_GetObjectItem(json, "license");
    if (cJSON_IsString(license)) {
        pkg->license = strdup(cJSON_GetStringValue(license));
    }
    
    cJSON* homepage = cJSON_GetObjectItem(json, "homepage");
    if (cJSON_IsString(homepage)) {
        pkg->homepage = strdup(cJSON_GetStringValue(homepage));
    }
    
    cJSON* repository = cJSON_GetObjectItem(json, "repository");
    if (cJSON_IsString(repository)) {
        pkg->repository_url = strdup(cJSON_GetStringValue(repository));
    }
    
    // Parse build commands
    cJSON* build_command = cJSON_GetObjectItem(json, "build_command");
    if (cJSON_IsString(build_command)) {
        pkg->build_command = strdup(cJSON_GetStringValue(build_command));
    }
    
    cJSON* test_command = cJSON_GetObjectItem(json, "test_command");
    if (cJSON_IsString(test_command)) {
        pkg->test_command = strdup(cJSON_GetStringValue(test_command));
    }
    
    cJSON* install_command = cJSON_GetObjectItem(json, "install_command");
    if (cJSON_IsString(install_command)) {
        pkg->install_command = strdup(cJSON_GetStringValue(install_command));
    }
    
    // Parse arrays
    cJSON* dependencies = cJSON_GetObjectItem(json, "dependencies");
    if (dependencies) {
        parse_string_array(dependencies, &pkg->dependencies, &pkg->dep_count, &pkg->dep_capacity);
    }
    
    cJSON* dev_dependencies = cJSON_GetObjectItem(json, "dev_dependencies");
    if (dev_dependencies) {
        parse_string_array(dev_dependencies, &pkg->dev_dependencies, &pkg->dev_dep_count, &pkg->dev_dep_capacity);
    }
    
    cJSON* scripts = cJSON_GetObjectItem(json, "scripts");
    if (scripts) {
        parse_string_array(scripts, &pkg->scripts, &pkg->script_count, &pkg->script_capacity);
    }
    
    cJSON* include_paths = cJSON_GetObjectItem(json, "include_paths");
    if (include_paths) {
        parse_string_array(include_paths, &pkg->include_paths, &pkg->include_path_count, &pkg->include_path_count);
    }
    
    cJSON* library_paths = cJSON_GetObjectItem(json, "library_paths");
    if (library_paths) {
        parse_string_array(library_paths, &pkg->library_paths, &pkg->library_path_count, &pkg->library_path_count);
    }
    
    cJSON* linked_libraries = cJSON_GetObjectItem(json, "linked_libraries");
    if (linked_libraries) {
        parse_string_array(linked_libraries, &pkg->linked_libraries, &pkg->linked_library_count, &pkg->linked_library_count);
    }
    
    // Parse PMEM flags
    cJSON* is_persistent = cJSON_GetObjectItem(json, "is_persistent");
    if (cJSON_IsBool(is_persistent)) {
        pkg->is_persistent = cJSON_IsTrue(is_persistent);
    }
    
    cJSON* requires_pmem = cJSON_GetObjectItem(json, "requires_pmem");
    if (cJSON_IsBool(requires_pmem)) {
        pkg->requires_pmem = cJSON_IsTrue(requires_pmem);
    }
    
    cJSON* pmem_pool_size = cJSON_GetObjectItem(json, "pmem_pool_size");
    if (cJSON_IsString(pmem_pool_size)) {
        pkg->pmem_pool_size = strdup(cJSON_GetStringValue(pmem_pool_size));
    }
    
    cJSON_Delete(json);
    return pkg;
}

// --- Package Manipulation ---

static CPM_Result resize_array(char*** array, size_t* count, size_t* capacity) {
    if (*count >= *capacity) {
        size_t new_capacity = (*capacity == 0) ? 4 : (*capacity * 2);
        char** new_array = realloc(*array, new_capacity * sizeof(char*));
        if (!new_array) {
            return CPM_RESULT_ERROR_MEMORY_ALLOCATION;
        }
        *array = new_array;
        *capacity = new_capacity;
    }
    return CPM_RESULT_SUCCESS;
}

CPM_Result cpm_package_add_dependency(Package* pkg, const char* dependency) {
    if (!pkg || !dependency) return CPM_RESULT_ERROR_INVALID_ARGS;
    
    CPM_Result result = resize_array(&pkg->dependencies, &pkg->dep_count, &pkg->dep_capacity);
    if (result != CPM_RESULT_SUCCESS) return result;
    
    pkg->dependencies[pkg->dep_count] = strdup(dependency);
    if (!pkg->dependencies[pkg->dep_count]) {
        return CPM_RESULT_ERROR_MEMORY_ALLOCATION;
    }
    
    pkg->dep_count++;
    return CPM_RESULT_SUCCESS;
}

CPM_Result cpm_package_add_dev_dependency(Package* pkg, const char* dev_dependency) {
    if (!pkg || !dev_dependency) return CPM_RESULT_ERROR_INVALID_ARGS;
    
    CPM_Result result = resize_array(&pkg->dev_dependencies, &pkg->dev_dep_count, &pkg->dev_dep_capacity);
    if (result != CPM_RESULT_SUCCESS) return result;
    
    pkg->dev_dependencies[pkg->dev_dep_count] = strdup(dev_dependency);
    if (!pkg->dev_dependencies[pkg->dev_dep_count]) {
        return CPM_RESULT_ERROR_MEMORY_ALLOCATION;
    }
    
    pkg->dev_dep_count++;
    return CPM_RESULT_SUCCESS;
}

CPM_Result cpm_package_add_script(Package* pkg, const char* script) {
    if (!pkg || !script) return CPM_RESULT_ERROR_INVALID_ARGS;
    
    CPM_Result result = resize_array(&pkg->scripts, &pkg->script_count, &pkg->script_capacity);
    if (result != CPM_RESULT_SUCCESS) return result;
    
    pkg->scripts[pkg->script_count] = strdup(script);
    if (!pkg->scripts[pkg->script_count]) {
        return CPM_RESULT_ERROR_MEMORY_ALLOCATION;
    }
    
    pkg->script_count++;
    return CPM_RESULT_SUCCESS;
}

CPM_Result cpm_package_add_include_path(Package* pkg, const char* include_path) {
    if (!pkg || !include_path) return CPM_RESULT_ERROR_INVALID_ARGS;
    
    // Simple resize for include paths
    size_t capacity = pkg->include_path_count + 1;
    char** new_array = realloc(pkg->include_paths, capacity * sizeof(char*));
    if (!new_array) {
        return CPM_RESULT_ERROR_MEMORY_ALLOCATION;
    }
    
    pkg->include_paths = new_array;
    pkg->include_paths[pkg->include_path_count] = strdup(include_path);
    if (!pkg->include_paths[pkg->include_path_count]) {
        return CPM_RESULT_ERROR_MEMORY_ALLOCATION;
    }
    
    pkg->include_path_count++;
    return CPM_RESULT_SUCCESS;
}

CPM_Result cpm_package_add_library_path(Package* pkg, const char* library_path) {
    if (!pkg || !library_path) return CPM_RESULT_ERROR_INVALID_ARGS;
    
    size_t capacity = pkg->library_path_count + 1;
    char** new_array = realloc(pkg->library_paths, capacity * sizeof(char*));
    if (!new_array) {
        return CPM_RESULT_ERROR_MEMORY_ALLOCATION;
    }
    
    pkg->library_paths = new_array;
    pkg->library_paths[pkg->library_path_count] = strdup(library_path);
    if (!pkg->library_paths[pkg->library_path_count]) {
        return CPM_RESULT_ERROR_MEMORY_ALLOCATION;
    }
    
    pkg->library_path_count++;
    return CPM_RESULT_SUCCESS;
}

CPM_Result cpm_package_add_linked_library(Package* pkg, const char* library) {
    if (!pkg || !library) return CPM_RESULT_ERROR_INVALID_ARGS;
    
    size_t capacity = pkg->linked_library_count + 1;
    char** new_array = realloc(pkg->linked_libraries, capacity * sizeof(char*));
    if (!new_array) {
        return CPM_RESULT_ERROR_MEMORY_ALLOCATION;
    }
    
    pkg->linked_libraries = new_array;
    pkg->linked_libraries[pkg->linked_library_count] = strdup(library);
    if (!pkg->linked_libraries[pkg->linked_library_count]) {
        return CPM_RESULT_ERROR_MEMORY_ALLOCATION;
    }
    
    pkg->linked_library_count++;
    return CPM_RESULT_SUCCESS;
}

// --- Package Validation ---

CPM_Result cpm_package_validate(const Package* pkg) {
    if (!pkg) return CPM_RESULT_ERROR_INVALID_ARGS;
    
    if (!pkg->name || strlen(pkg->name) == 0) {
        printf("[CPM] Package validation failed: missing name\n");
        return CPM_RESULT_ERROR_PACKAGE_PARSE;
    }
    
    if (!pkg->version || strlen(pkg->version) == 0) {
        printf("[CPM] Package validation failed: missing version\n");
        return CPM_RESULT_ERROR_PACKAGE_PARSE;
    }
    
    // Basic version format check (x.y.z)
    int major, minor, patch;
    if (sscanf(pkg->version, "%d.%d.%d", &major, &minor, &patch) != 3) {
        printf("[CPM] Package validation failed: invalid version format: %s\n", pkg->version);
        return CPM_RESULT_ERROR_PACKAGE_PARSE;
    }
    
    return CPM_RESULT_SUCCESS;
}

bool cpm_package_satisfies_version(const Package* pkg, const char* version_spec) {
    if (!pkg || !pkg->version || !version_spec) return false;
    
    // Simple version comparison (would need more sophisticated parsing in reality)
    if (strncmp(version_spec, ">=", 2) == 0) {
        const char* required_version = version_spec + 2;
        return strcmp(pkg->version, required_version) >= 0;
    } else if (strncmp(version_spec, "<=", 2) == 0) {
        const char* required_version = version_spec + 2;
        return strcmp(pkg->version, required_version) <= 0;
    } else if (strncmp(version_spec, ">", 1) == 0) {
        const char* required_version = version_spec + 1;
        return strcmp(pkg->version, required_version) > 0;
    } else if (strncmp(version_spec, "<", 1) == 0) {
        const char* required_version = version_spec + 1;
        return strcmp(pkg->version, required_version) < 0;
    } else {
        // Exact match
        return strcmp(pkg->version, version_spec) == 0;
    }
}

// --- Package Serialization ---

static void add_string_array_to_json(cJSON* json, const char* key, char** array, size_t count) {
    if (count == 0) return;
    
    cJSON* json_array = cJSON_CreateArray();
    for (size_t i = 0; i < count; i++) {
        cJSON_AddItemToArray(json_array, cJSON_CreateString(array[i]));
    }
    cJSON_AddItemToObject(json, key, json_array);
}

char* cpm_package_to_json(const Package* pkg) {
    if (!pkg) return NULL;
    
    cJSON* json = cJSON_CreateObject();
    
    // Add basic fields
    if (pkg->name) cJSON_AddStringToObject(json, "name", pkg->name);
    if (pkg->version) cJSON_AddStringToObject(json, "version", pkg->version);
    if (pkg->description) cJSON_AddStringToObject(json, "description", pkg->description);
    if (pkg->author) cJSON_AddStringToObject(json, "author", pkg->author);
    if (pkg->license) cJSON_AddStringToObject(json, "license", pkg->license);
    if (pkg->homepage) cJSON_AddStringToObject(json, "homepage", pkg->homepage);
    if (pkg->repository_url) cJSON_AddStringToObject(json, "repository", pkg->repository_url);
    
    // Add build commands
    if (pkg->build_command) cJSON_AddStringToObject(json, "build_command", pkg->build_command);
    if (pkg->test_command) cJSON_AddStringToObject(json, "test_command", pkg->test_command);
    if (pkg->install_command) cJSON_AddStringToObject(json, "install_command", pkg->install_command);
    
    // Add arrays
    add_string_array_to_json(json, "dependencies", pkg->dependencies, pkg->dep_count);
    add_string_array_to_json(json, "dev_dependencies", pkg->dev_dependencies, pkg->dev_dep_count);
    add_string_array_to_json(json, "scripts", pkg->scripts, pkg->script_count);
    add_string_array_to_json(json, "include_paths", pkg->include_paths, pkg->include_path_count);
    add_string_array_to_json(json, "library_paths", pkg->library_paths, pkg->library_path_count);
    add_string_array_to_json(json, "linked_libraries", pkg->linked_libraries, pkg->linked_library_count);
    
    // Add PMEM flags
    if (pkg->is_persistent) cJSON_AddBoolToObject(json, "is_persistent", true);
    if (pkg->requires_pmem) cJSON_AddBoolToObject(json, "requires_pmem", true);
    if (pkg->pmem_pool_size) cJSON_AddStringToObject(json, "pmem_pool_size", pkg->pmem_pool_size);
    
    char* json_string = cJSON_Print(json);
    cJSON_Delete(json);
    
    return json_string;
}

CPM_Result cpm_package_write_to_file(const Package* pkg, const char* filepath) {
    if (!pkg || !filepath) return CPM_RESULT_ERROR_INVALID_ARGS;
    
    char* json_data = cpm_package_to_json(pkg);
    if (!json_data) return CPM_RESULT_ERROR_PACKAGE_PARSE;
    
    FILE* fp = fopen(filepath, "w");
    if (!fp) {
        free(json_data);
        return CPM_RESULT_ERROR_FILE_OPERATION;
    }
    
    size_t json_len = strlen(json_data);
    size_t written = fwrite(json_data, 1, json_len, fp);
    fclose(fp);
    free(json_data);
    
    return (written == json_len) ? CPM_RESULT_SUCCESS : CPM_RESULT_ERROR_FILE_OPERATION;
}

// --- Package Query Functions ---

const char* cpm_package_find_script(const Package* pkg, const char* script_name) {
    if (!pkg || !script_name) return NULL;
    
    for (size_t i = 0; i < pkg->script_count; i++) {
        const char* script = pkg->scripts[i];
        const char* colon = strchr(script, ':');
        if (colon) {
            size_t name_len = colon - script;
            if (strncmp(script, script_name, name_len) == 0 && script_name[name_len] == '') {
                // Return the command part (after colon and any spaces)
                const char* command = colon + 1;
                while (*command == ' ') command++;
                return command;
            }
        }
    }
    
    return NULL;
}

const char** cpm_package_get_dependencies(const Package* pkg, size_t* count) {
    if (!pkg || !count) return NULL;
    
    *count = pkg->dep_count;
    return (const char**)pkg->dependencies;
}

const char** cpm_package_get_scripts(const Package* pkg, size_t* count) {
    if (!pkg || !count) return NULL;
    
    *count = pkg->script_count;
    return (const char**)pkg->scripts;
}
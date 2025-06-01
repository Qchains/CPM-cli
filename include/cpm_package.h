/*
 * File: include/cpm_package.h  
 * Description: Package structure and parsing functions for CPM
 * Author: Dr. Q Josef Kurk Edwards
 */

#ifndef CPM_PACKAGE_H
#define CPM_PACKAGE_H

#include "cpm_types.h"

// --- Package Structure ---
typedef struct Package {
    char* name;
    char* version;
    char* description;
    char* author;
    char* license;
    char* homepage;
    char* repository_url;
    
    // Dependencies
    char** dependencies;
    size_t dep_count;
    size_t dep_capacity;
    
    // Development dependencies
    char** dev_dependencies;
    size_t dev_dep_count;
    size_t dev_dep_capacity;
    
    // Scripts
    char** scripts;
    size_t script_count;
    size_t script_capacity;
    
    // Build configuration
    char* build_command;
    char* test_command;
    char* install_command;
    
    // Include paths and library paths for C projects
    char** include_paths;
    size_t include_path_count;
    
    char** library_paths;
    size_t library_path_count;
    
    // Linked libraries
    char** linked_libraries;
    size_t linked_library_count;
    
    // Package metadata
    bool is_persistent;     // Uses persistent memory
    bool requires_pmem;     // Requires PMEM support
    char* pmem_pool_size;   // Required PMEM pool size
    
} Package;

// --- Package Parsing ---

/**
 * @brief Parses a cpm_package.spec file.
 * @param filepath Path to the package specification file
 * @return Pointer to Package structure, or NULL on failure
 */
Package* cpm_parse_package_file(const char* filepath);

/**
 * @brief Parses package data from a JSON string.
 * @param json_data JSON string containing package data
 * @return Pointer to Package structure, or NULL on failure
 */
Package* cpm_parse_package_json(const char* json_data);

/**
 * @brief Creates a new empty package structure.
 * @return Pointer to Package structure, or NULL on failure
 */
Package* cpm_package_create(void);

/**
 * @brief Frees a package structure and all its data.
 * @param pkg The package to free
 */
void cpm_package_free(Package* pkg);

// --- Package Manipulation ---

/**
 * @brief Adds a dependency to a package.
 * @param pkg The package
 * @param dependency The dependency string (e.g., "libfoo@1.0.0")
 * @return CPM_RESULT_SUCCESS on success, error code on failure
 */
CPM_Result cpm_package_add_dependency(Package* pkg, const char* dependency);

/**
 * @brief Adds a development dependency to a package.
 * @param pkg The package
 * @param dev_dependency The dev dependency string
 * @return CPM_RESULT_SUCCESS on success, error code on failure
 */
CPM_Result cpm_package_add_dev_dependency(Package* pkg, const char* dev_dependency);

/**
 * @brief Adds a script to a package.
 * @param pkg The package
 * @param script The script string (e.g., "test: make test")
 * @return CPM_RESULT_SUCCESS on success, error code on failure
 */
CPM_Result cpm_package_add_script(Package* pkg, const char* script);

/**
 * @brief Adds an include path to a package.
 * @param pkg The package
 * @param include_path The include path
 * @return CPM_RESULT_SUCCESS on success, error code on failure
 */
CPM_Result cpm_package_add_include_path(Package* pkg, const char* include_path);

/**
 * @brief Adds a library path to a package.
 * @param pkg The package
 * @param library_path The library path
 * @return CPM_RESULT_SUCCESS on success, error code on failure
 */
CPM_Result cpm_package_add_library_path(Package* pkg, const char* library_path);

/**
 * @brief Adds a linked library to a package.
 * @param pkg The package
 * @param library The library name
 * @return CPM_RESULT_SUCCESS on success, error code on failure
 */
CPM_Result cpm_package_add_linked_library(Package* pkg, const char* library);

// --- Package Validation ---

/**
 * @brief Validates a package structure.
 * @param pkg The package to validate
 * @return CPM_RESULT_SUCCESS if valid, error code otherwise
 */
CPM_Result cpm_package_validate(const Package* pkg);

/**
 * @brief Checks if a package satisfies version requirements.
 * @param pkg The package
 * @param version_spec The version specification (e.g., ">=1.0.0")
 * @return true if satisfied, false otherwise
 */
bool cpm_package_satisfies_version(const Package* pkg, const char* version_spec);

// --- Package Serialization ---

/**
 * @brief Serializes a package to JSON format.
 * @param pkg The package to serialize
 * @return JSON string (caller must free), or NULL on failure
 */
char* cpm_package_to_json(const Package* pkg);

/**
 * @brief Writes a package to a cpm_package.spec file.
 * @param pkg The package to write
 * @param filepath The output file path
 * @return CPM_RESULT_SUCCESS on success, error code on failure
 */
CPM_Result cpm_package_write_to_file(const Package* pkg, const char* filepath);

// --- Package Query Functions ---

/**
 * @brief Finds a script by name in a package.
 * @param pkg The package
 * @param script_name The name of the script
 * @return The script command, or NULL if not found
 */
const char* cpm_package_find_script(const Package* pkg, const char* script_name);

/**
 * @brief Gets all dependencies of a package.
 * @param pkg The package
 * @param count Output parameter for dependency count
 * @return Array of dependency strings (do not free)
 */
const char** cpm_package_get_dependencies(const Package* pkg, size_t* count);

/**
 * @brief Gets all scripts of a package.
 * @param pkg The package
 * @param count Output parameter for script count
 * @return Array of script strings (do not free)
 */
const char** cpm_package_get_scripts(const Package* pkg, size_t* count);

#endif // CPM_PACKAGE_H
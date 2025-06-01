/*
 * File: include/cpm_package.h
 * Description: Package management structures and functions for CPM
 * Author: Dr. Q Josef Kurk Edwards
 */

#ifndef CPM_PACKAGE_H
#define CPM_PACKAGE_H

#include "cpm_types.h"
#include "cpm_promise.h"
#include <cjson/cjson.h>

// --- Package Parsing ---

/**
 * @brief Parses a package specification file
 * @param filepath Path to cpm_package.spec file
 * @return Parsed package structure or NULL on failure
 */
Package* cpm_parse_package_file(const char* filepath);

/**
 * @brief Parses package from JSON string
 * @param json_str JSON string containing package specification
 * @return Parsed package structure or NULL on failure
 */
Package* cpm_parse_package_json(const char* json_str);

/**
 * @brief Parses package from cJSON object
 * @param json cJSON object containing package specification
 * @return Parsed package structure or NULL on failure
 */
Package* cpm_parse_package_cjson(const cJSON* json);

/**
 * @brief Writes package to specification file
 * @param pkg Package to write
 * @param filepath Output file path
 * @return CPM_RESULT_SUCCESS on success
 */
CPM_Result cpm_write_package_file(const Package* pkg, const char* filepath);

/**
 * @brief Converts package to JSON string
 * @param pkg Package to convert
 * @return JSON string (must be freed) or NULL on failure
 */
char* cpm_package_to_json(const Package* pkg);

/**
 * @brief Frees a package structure
 * @param pkg Package to free
 */
void cpm_free_package(Package* pkg);

// --- Package Operations ---

/**
 * @brief Downloads a package from registry
 * @param name Package name
 * @param version Package version (NULL for latest)
 * @param registry_url Registry URL
 * @return Promise that resolves with downloaded package path
 */
Promise* cpm_download_package(const char* name, const char* version, const char* registry_url);

/**
 * @brief Installs a package
 * @param name Package name
 * @param version Package version (NULL for latest)
 * @param config CPM configuration
 * @return Promise that resolves when installation is complete
 */
Promise* cpm_install_package(const char* name, const char* version, const CPM_Config* config);

/**
 * @brief Resolves package dependencies
 * @param pkg Package with dependencies to resolve
 * @param config CPM configuration
 * @return Promise that resolves when all dependencies are resolved
 */
Promise* cpm_resolve_dependencies(const Package* pkg, const CPM_Config* config);

/**
 * @brief Publishes a package to registry
 * @param pkg Package to publish
 * @param registry_url Registry URL
 * @param auth_token Authentication token
 * @return Promise that resolves when package is published
 */
Promise* cpm_publish_package(const Package* pkg, const char* registry_url, const char* auth_token);

/**
 * @brief Searches for packages in registry
 * @param query Search query
 * @param registry_url Registry URL
 * @return Promise that resolves with search results
 */
Promise* cpm_search_packages(const char* query, const char* registry_url);

// --- Package Building ---

/**
 * @brief Builds a package from source
 * @param pkg Package to build
 * @param build_dir Build directory
 * @return Promise that resolves when build is complete
 */
Promise* cpm_build_package(const Package* pkg, const char* build_dir);

/**
 * @brief Runs package tests
 * @param pkg Package to test
 * @param test_dir Test directory
 * @return Promise that resolves with test results
 */
Promise* cpm_test_package(const Package* pkg, const char* test_dir);

/**
 * @brief Packages source into distributable archive
 * @param pkg Package to pack
 * @param output_path Output archive path
 * @return Promise that resolves when packing is complete
 */
Promise* cpm_pack_package(const Package* pkg, const char* output_path);

// --- Script Execution ---

/**
 * @brief Runs a package script
 * @param pkg Package containing script
 * @param script_name Name of script to run
 * @param args Arguments to pass to script
 * @param config CPM configuration
 * @return Promise that resolves with script exit code
 */
Promise* cpm_run_script(const Package* pkg, const char* script_name, 
                       const char** args, const CPM_Config* config);

/**
 * @brief Lists available scripts in package
 * @param pkg Package to examine
 * @param script_names Output array of script names
 * @param count Output count of scripts
 * @return CPM_RESULT_SUCCESS on success
 */
CPM_Result cpm_list_scripts(const Package* pkg, char*** script_names, size_t* count);

// --- Dependency Management ---

/**
 * @brief Resolves a single dependency
 * @param dep_spec Dependency specification (name@version)
 * @param config CPM configuration
 * @return Promise that resolves with resolved package
 */
Promise* cpm_resolve_dependency(const char* dep_spec, const CPM_Config* config);

/**
 * @brief Checks if dependency is satisfied
 * @param dep_spec Dependency specification
 * @param installed_version Currently installed version
 * @return true if dependency is satisfied
 */
bool cpm_dependency_satisfied(const char* dep_spec, const char* installed_version);

/**
 * @brief Gets dependency tree for package
 * @param pkg Package to analyze
 * @param config CPM configuration
 * @return Promise that resolves with dependency tree
 */
Promise* cpm_get_dependency_tree(const Package* pkg, const CPM_Config* config);

// --- Version Management ---

/**
 * @brief Compares two version strings
 * @param version1 First version
 * @param version2 Second version
 * @return -1 if version1 < version2, 0 if equal, 1 if version1 > version2
 */
int cpm_version_compare(const char* version1, const char* version2);

/**
 * @brief Checks if version satisfies range
 * @param version Version to check
 * @param range Version range specification
 * @return true if version satisfies range
 */
bool cpm_version_satisfies(const char* version, const char* range);

/**
 * @brief Parses version specification
 * @param version_spec Version specification string
 * @param name Output package name
 * @param version Output version
 * @return CPM_RESULT_SUCCESS on success
 */
CPM_Result cpm_parse_version_spec(const char* version_spec, char** name, char** version);

// --- Registry Communication ---

/**
 * @brief Fetches package metadata from registry
 * @param name Package name
 * @param registry_url Registry URL
 * @return Promise that resolves with package metadata
 */
Promise* cpm_fetch_package_metadata(const char* name, const char* registry_url);

/**
 * @brief Lists available versions for package
 * @param name Package name
 * @param registry_url Registry URL
 * @return Promise that resolves with version list
 */
Promise* cpm_list_package_versions(const char* name, const char* registry_url);

/**
 * @brief Checks package availability
 * @param name Package name
 * @param version Package version
 * @param registry_url Registry URL
 * @return Promise that resolves with availability status
 */
Promise* cpm_check_package_availability(const char* name, const char* version, const char* registry_url);

#endif // CPM_PACKAGE_H
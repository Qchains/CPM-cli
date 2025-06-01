/*
 * File: include/cpm_package.h
 * Description: Package structure and parsing functions for CPM
 * Author: Dr. Q Josef Kurk Edwards
 */

#ifndef CPM_PACKAGE_H
#define CPM_PACKAGE_H

#include "cpm_types.h"
#include "cpm_promise.h"
#include <time.h>

// --- Package Metadata Structures ---

/**
 * @brief Package dependency specification
 */
typedef struct {
    char* name;                // Package name
    char* version_spec;        // Version specification (e.g., ">=1.0.0", "^2.1.0")
    char* resolved_version;    // Actual resolved version
    bool is_dev_dependency;    // Development dependency flag
    bool is_optional;          // Optional dependency flag
} PackageDependency;

/**
 * @brief Package script definition
 */
typedef struct {
    char* name;               // Script name (e.g., "build", "test", "install")
    char* command;            // Command to execute
    char* description;        // Script description
    bool requires_build;      // Whether this script requires build first
} PackageScript;

/**
 * @brief Package build configuration
 */
typedef struct {
    char* build_system;       // Build system (e.g., "cmake", "make", "autotools")
    char* build_script;       // Custom build script
    char* configure_args;     // Configuration arguments
    char* make_args;          // Make arguments
    char** include_dirs;      // Include directories
    size_t include_dir_count;
    char** library_dirs;      // Library directories  
    size_t library_dir_count;
    char** libraries;         // Libraries to link
    size_t library_count;
} PackageBuildConfig;

/**
 * @brief Package installation metadata
 */
typedef struct {
    time_t install_time;      // When package was installed
    char* install_path;       // Where package is installed
    char* checksum;           // Package checksum
    char* signature;          // Package signature
    bool verified;            // Whether package was verified
} PackageInstallInfo;

/**
 * @brief Extended package structure
 */
typedef struct Package {
    // Basic metadata
    char* name;
    char* version;
    char* description;
    char* author;
    char* license;
    char* homepage;
    char* repository;
    char* bugs_url;
    
    // Dependencies
    PackageDependency* dependencies;
    size_t dep_count;
    PackageDependency* dev_dependencies;
    size_t dev_dep_count;
    
    // Scripts
    PackageScript* scripts;
    size_t script_count;
    
    // Build configuration
    PackageBuildConfig* build_config;
    
    // CPM specific paths
    char* include_path;
    char* lib_path;
    char* bin_path;
    char* doc_path;
    
    // Installation info
    PackageInstallInfo* install_info;
    
    // Keywords and categories
    char** keywords;
    size_t keyword_count;
    char* category;
    
    // Version constraints
    char* min_cpm_version;
    char* min_c_standard;
    
    // Package flags
    bool is_binary_package;   // Pre-compiled binary package
    bool is_header_only;      // Header-only library
    bool requires_build;      // Requires compilation
    bool is_system_package;   // System-level package
} Package;

// --- Package Parsing Functions ---

/**
 * @brief Parses a CPM package specification file
 * @param filepath Path to cpm_package.spec file
 * @return Parsed package or NULL on failure
 */
Package* cpm_package_parse_file(const char* filepath);

/**
 * @brief Parses a package specification from string
 * @param spec_content Package spec content
 * @return Parsed package or NULL on failure
 */
Package* cpm_package_parse_string(const char* spec_content);

/**
 * @brief Validates a package specification
 * @param pkg Package to validate
 * @return CPM_RESULT_SUCCESS if valid
 */
CPM_Result cpm_package_validate(const Package* pkg);

/**
 * @brief Writes a package specification to file
 * @param pkg Package to write
 * @param filepath Output file path
 * @return CPM_RESULT_SUCCESS on success
 */
CPM_Result cpm_package_write_file(const Package* pkg, const char* filepath);

/**
 * @brief Converts package to JSON string
 * @param pkg Package to convert
 * @return JSON string (caller must free) or NULL on failure
 */
char* cpm_package_to_json(const Package* pkg);

/**
 * @brief Frees a package structure
 * @param pkg Package to free
 */
void cpm_package_free(Package* pkg);

// --- Package Resolution Functions ---

/**
 * @brief Resolves a package name to full package information
 * @param name Package name (e.g., "mylib", "mylib@1.0.0")
 * @param registry_url Registry URL to query
 * @return Promise that resolves to Package* or rejects with error
 */
Promise* cpm_package_resolve_remote(const char* name, const char* registry_url);

/**
 * @brief Resolves package dependencies recursively
 * @param pkg Root package
 * @param registry_url Registry URL
 * @return Promise that resolves to array of all dependencies
 */
Promise* cpm_package_resolve_dependencies(const Package* pkg, const char* registry_url);

/**
 * @brief Checks if package is already installed
 * @param name Package name
 * @param version Package version (NULL for any version)
 * @param modules_dir Modules directory path
 * @return Installed package or NULL if not found
 */
Package* cpm_package_find_installed(const char* name, 
                                   const char* version,
                                   const char* modules_dir);

// --- Package Installation Functions ---

/**
 * @brief Downloads a package from registry
 * @param pkg Package to download
 * @param registry_url Registry URL
 * @param download_dir Download directory
 * @return Promise that resolves to download path
 */
Promise* cpm_package_download(const Package* pkg, 
                             const char* registry_url,
                             const char* download_dir);

/**
 * @brief Extracts a downloaded package
 * @param archive_path Path to package archive
 * @param extract_dir Directory to extract to
 * @return Promise that resolves to extraction path
 */
Promise* cpm_package_extract(const char* archive_path, const char* extract_dir);

/**
 * @brief Builds a package from source
 * @param pkg Package to build
 * @param source_dir Source directory
 * @param build_dir Build directory
 * @return Promise that resolves on successful build
 */
Promise* cpm_package_build(const Package* pkg, 
                          const char* source_dir,
                          const char* build_dir);

/**
 * @brief Installs a built package
 * @param pkg Package to install
 * @param build_dir Build directory
 * @param install_dir Installation directory
 * @return Promise that resolves on successful installation
 */
Promise* cpm_package_install(const Package* pkg,
                            const char* build_dir,
                            const char* install_dir);

// --- Script Execution Functions ---

/**
 * @brief Executes a package script
 * @param pkg Package containing script
 * @param script_name Name of script to run
 * @param args Additional arguments
 * @param working_dir Working directory for script
 * @return Promise that resolves to script exit code
 */
Promise* cpm_package_run_script(const Package* pkg,
                               const char* script_name,
                               const char* args,
                               const char* working_dir);

// --- Version Management Functions ---

/**
 * @brief Compares two version strings
 * @param version1 First version
 * @param version2 Second version
 * @return -1 if version1 < version2, 0 if equal, 1 if version1 > version2
 */
int cpm_package_version_compare(const char* version1, const char* version2);

/**
 * @brief Checks if version satisfies specification
 * @param version Version to check
 * @param spec Version specification (e.g., ">=1.0.0", "^2.1.0")
 * @return true if version satisfies spec
 */
bool cpm_package_version_satisfies(const char* version, const char* spec);

/**
 * @brief Gets latest version matching specification
 * @param name Package name
 * @param spec Version specification
 * @param registry_url Registry URL
 * @return Promise that resolves to latest matching version
 */
Promise* cpm_package_get_latest_version(const char* name,
                                       const char* spec,
                                       const char* registry_url);

// --- Package Creation Functions ---

/**
 * @brief Creates a new package specification interactively
 * @param name Package name
 * @param working_dir Working directory
 * @return Promise that resolves to created Package
 */
Promise* cpm_package_create_interactive(const char* name, const char* working_dir);

/**
 * @brief Creates a package specification from template
 * @param template_name Template name
 * @param package_name Package name
 * @param working_dir Working directory
 * @return Promise that resolves to created Package
 */
Promise* cpm_package_create_from_template(const char* template_name,
                                         const char* package_name,
                                         const char* working_dir);

#endif // CPM_PACKAGE_H
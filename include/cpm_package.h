/*
 * File: include/cpm_package.h
 * Description: Package structure and parsing functions for CPM.
 * Handles cpm_package.spec files and package metadata.
 * Author: Dr. Q Josef Kurk Edwards
 */

#ifndef CPM_PACKAGE_H
#define CPM_PACKAGE_H

#include <stddef.h>
#include "cpm_types.h"
#include "cpm_promise.h"

// --- Package Structure ---
typedef struct {
    char* name;
    char* version;
    char* description;
    char* author;
    char* license;
    
    // Dependencies
    size_t dep_count;
    char** dependencies;
    
    // Scripts
    size_t script_count;
    char** scripts;
    
    // Build configuration
    char* build_command;
    char* install_command;
    char* test_command;
} Package;

// --- Package Operations ---
Package* cpm_parse_package_file(const char* filepath);
void cpm_free_package(Package* pkg);
CPM_Result cpm_save_package_file(const Package* pkg, const char* filepath);

// --- Package Resolution ---
Package* cpm_package_resolve_remote(const char* package_spec, const char* registry_url);
Promise* cpm_package_install_async(const Package* pkg, const char* install_dir);
Promise* cpm_package_build_async(const Package* pkg, const char* package_dir);

// --- Package Validation ---
bool cpm_package_validate(const Package* pkg);
bool cpm_package_spec_exists(const char* directory);

#endif // CPM_PACKAGE_H
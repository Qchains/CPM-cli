/*
 * File: cpm_deps.h
 * Description: Dependency resolution system for CPM - npm-like dependency management
 * Author: Dr. Q Josef Kurk Edwards
 */

#ifndef CPM_DEPS_H
#define CPM_DEPS_H

#include <stdbool.h>
#include <stddef.h>
#include "cpm_types.h"
#include "cpm_semver.h"
#include "cpm_package.h"

// --- Dependency Structure ---
typedef struct Dependency {
    char* name;
    VersionConstraint* constraint;
    bool is_dev_dependency;
    struct Dependency* next;
} Dependency;

// --- Dependency Tree Node ---
typedef struct DepNode {
    char* name;
    SemVer* version;
    char* resolved_url;
    Dependency* dependencies;
    struct DepNode** children;
    size_t child_count;
    bool installed;
} DepNode;

// --- Dependency Resolution Result ---
typedef struct {
    DepNode* root;
    DepNode** install_order;
    size_t install_count;
    char** conflicts;
    size_t conflict_count;
} DepResolution;

// --- Dependency Operations ---
Dependency* cpm_dependency_create(const char* name, const char* constraint_str);
void cpm_dependency_free(Dependency* dep);
void cpm_dependency_list_free(Dependency* head);

// --- Dependency Tree Operations ---
DepNode* cpm_depnode_create(const char* name, const SemVer* version);
void cpm_depnode_free(DepNode* node);
CPM_Result cpm_depnode_add_child(DepNode* parent, DepNode* child);

// --- Dependency Resolution ---
DepResolution* cpm_resolve_dependencies(const Package* root_package, const char* registry_url);
void cpm_resolution_free(DepResolution* resolution);

// --- Dependency Installation ---
CPM_Result cpm_install_dependencies(const DepResolution* resolution, const char* target_dir);
CPM_Result cpm_install_dependency(const DepNode* dep, const char* target_dir);

// --- Dependency Utilities ---
bool cpm_dependency_is_satisfied(const char* name, const SemVer* installed_version, const VersionConstraint* constraint);
DepNode* cpm_find_installed_dependency(const char* name, const char* modules_dir);
SemVer** cpm_get_available_versions(const char* package_name, const char* registry_url, size_t* count);

// --- Conflict Detection ---
bool cpm_detect_dependency_conflicts(const DepResolution* resolution);
char** cpm_get_conflict_descriptions(const DepResolution* resolution, size_t* count);

// --- Dependency Graph Utilities ---
void cpm_print_dependency_tree(const DepNode* root, int depth);
bool cpm_dependency_has_circular_reference(const DepNode* root);

#endif // CPM_DEPS_H
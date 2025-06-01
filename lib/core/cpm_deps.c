/*
 * File: cpm_deps.c
 * Description: Dependency resolution implementation for CPM
 * Author: Dr. Q Josef Kurk Edwards
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "cpm_deps.h"

// --- Internal Helper Functions ---
static char* strdup_safe(const char* str) {
    if (!str) return NULL;
    return strdup(str);
}

// --- HTTP Response Structure ---
typedef struct {
    char* data;
    size_t size;
} HTTPResponse;

static size_t http_write_callback(void* contents, size_t size, size_t nmemb, HTTPResponse* response) {
    size_t total_size = size * nmemb;
    
    response->data = realloc(response->data, response->size + total_size + 1);
    if (response->data == NULL) {
        return 0;
    }
    
    memcpy(&(response->data[response->size]), contents, total_size);
    response->size += total_size;
    response->data[response->size] = 0;
    
    return total_size;
}

// --- Dependency Operations ---
Dependency* cpm_dependency_create(const char* name, const char* constraint_str) {
    if (!name) return NULL;
    
    Dependency* dep = calloc(1, sizeof(Dependency));
    if (!dep) return NULL;
    
    dep->name = strdup(name);
    if (!dep->name) {
        free(dep);
        return NULL;
    }
    
    if (constraint_str) {
        dep->constraint = semver_parse_constraint(constraint_str);
        if (!dep->constraint) {
            // Default to any version if constraint parsing fails
            dep->constraint = semver_parse_constraint("*");
        }
    } else {
        dep->constraint = semver_parse_constraint("*");
    }
    
    return dep;
}

void cpm_dependency_free(Dependency* dep) {
    if (!dep) return;
    
    free(dep->name);
    semver_free_constraint(dep->constraint);
    free(dep);
}

void cpm_dependency_list_free(Dependency* head) {
    while (head) {
        Dependency* next = head->next;
        cpm_dependency_free(head);
        head = next;
    }
}

// --- Dependency Tree Operations ---
DepNode* cpm_depnode_create(const char* name, const SemVer* version) {
    if (!name) return NULL;
    
    DepNode* node = calloc(1, sizeof(DepNode));
    if (!node) return NULL;
    
    node->name = strdup(name);
    if (!node->name) {
        free(node);
        return NULL;
    }
    
    if (version) {
        char* version_str = semver_to_string(version);
        node->version = semver_parse(version_str);
        free(version_str);
    }
    
    return node;
}

void cpm_depnode_free(DepNode* node) {
    if (!node) return;
    
    free(node->name);
    semver_free(node->version);
    free(node->resolved_url);
    cpm_dependency_list_free(node->dependencies);
    
    if (node->children) {
        for (size_t i = 0; i < node->child_count; i++) {
            cpm_depnode_free(node->children[i]);
        }
        free(node->children);
    }
    
    free(node);
}

CPM_Result cpm_depnode_add_child(DepNode* parent, DepNode* child) {
    if (!parent || !child) return CPM_RESULT_ERROR_INVALID_ARGS;
    
    parent->children = realloc(parent->children, (parent->child_count + 1) * sizeof(DepNode*));
    if (!parent->children) return CPM_RESULT_ERROR_MEMORY_ALLOCATION;
    
    parent->children[parent->child_count] = child;
    parent->child_count++;
    
    return CPM_RESULT_SUCCESS;
}

// --- Registry Communication ---
static SemVer** fetch_package_versions(const char* package_name, const char* registry_url, size_t* count) {
    *count = 0;
    
    CURL* curl = curl_easy_init();
    if (!curl) return NULL;
    
    char url[1024];
    snprintf(url, sizeof(url), "%s/packages/%s/versions", registry_url, package_name);
    
    HTTPResponse response = {0};
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK || !response.data) {
        // Return mock versions for demonstration
        SemVer** versions = malloc(3 * sizeof(SemVer*));
        if (versions) {
            versions[0] = semver_parse("1.0.0");
            versions[1] = semver_parse("1.1.0");
            versions[2] = semver_parse("2.0.0");
            *count = 3;
        }
        free(response.data);
        return versions;
    }
    
    // Parse version response (simplified)
    // In real implementation, use proper JSON parser
    free(response.data);
    
    // Mock response for now
    SemVer** versions = malloc(3 * sizeof(SemVer*));
    if (versions) {
        versions[0] = semver_parse("1.0.0");
        versions[1] = semver_parse("1.1.0");
        versions[2] = semver_parse("2.0.0");
        *count = 3;
    }
    
    return versions;
}

SemVer** cpm_get_available_versions(const char* package_name, const char* registry_url, size_t* count) {
    return fetch_package_versions(package_name, registry_url, count);
}

// --- Dependency Resolution Algorithm ---
static DepNode* resolve_single_dependency(const Dependency* dep, const char* registry_url) {
    if (!dep || !dep->name) return NULL;
    
    // Get available versions
    size_t version_count;
    SemVer** available_versions = cpm_get_available_versions(dep->name, registry_url, &version_count);
    
    if (!available_versions || version_count == 0) {
        printf("[CPM Deps] Warning: No versions found for package '%s'\n", dep->name);
        return NULL;
    }
    
    // Find best matching version
    SemVer* best_version = semver_resolve_latest_compatible((const SemVer**)available_versions, version_count, dep->constraint);
    
    // Clean up available versions
    for (size_t i = 0; i < version_count; i++) {
        semver_free(available_versions[i]);
    }
    free(available_versions);
    
    if (!best_version) {
        printf("[CPM Deps] No compatible version found for package '%s'\n", dep->name);
        return NULL;
    }
    
    // Create dependency node
    DepNode* node = cpm_depnode_create(dep->name, best_version);
    semver_free(best_version);
    
    if (node) {
        // Set resolved URL
        char url[512];
        char* version_str = semver_to_string(node->version);
        snprintf(url, sizeof(url), "%s/packages/%s/%s", registry_url, dep->name, version_str);
        node->resolved_url = strdup(url);
        free(version_str);
    }
    
    return node;
}

static CPM_Result resolve_dependencies_recursive(DepNode* node, const char* registry_url, int depth) {
    if (!node || depth > 10) { // Prevent infinite recursion
        return CPM_RESULT_ERROR_DEPENDENCY_RESOLUTION;
    }
    
    printf("[CPM Deps] Resolving dependencies for %s@%s (depth %d)\n", 
           node->name, 
           node->version ? semver_to_string(node->version) : "unknown",
           depth);
    
    // For demonstration, create mock dependencies
    // In real implementation, fetch package.json from registry
    if (strcmp(node->name, "libmath") == 0) {
        Dependency* dep = cpm_dependency_create("libutils", "^1.0.0");
        if (dep) {
            DepNode* child = resolve_single_dependency(dep, registry_url);
            if (child) {
                cpm_depnode_add_child(node, child);
                resolve_dependencies_recursive(child, registry_url, depth + 1);
            }
            cpm_dependency_free(dep);
        }
    }
    
    return CPM_RESULT_SUCCESS;
}

DepResolution* cpm_resolve_dependencies(const Package* root_package, const char* registry_url) {
    if (!root_package) return NULL;
    
    DepResolution* resolution = calloc(1, sizeof(DepResolution));
    if (!resolution) return NULL;
    
    // Create root node
    SemVer* root_version = semver_parse(root_package->version ? root_package->version : "1.0.0");
    resolution->root = cpm_depnode_create(root_package->name, root_version);
    semver_free(root_version);
    
    if (!resolution->root) {
        free(resolution);
        return NULL;
    }
    
    printf("[CPM Deps] Starting dependency resolution for %s\n", root_package->name);
    
    // Parse dependencies from package
    if (root_package->dependencies) {
        for (size_t i = 0; i < root_package->dep_count; i++) {
            // Parse dependency string (format: "name@version")
            char* dep_str = strdup(root_package->dependencies[i]);
            if (!dep_str) continue;
            
            char* at_sign = strchr(dep_str, '@');
            char* version_constraint = NULL;
            
            if (at_sign) {
                *at_sign = '\0';
                version_constraint = at_sign + 1;
            }
            
            Dependency* dep = cpm_dependency_create(dep_str, version_constraint);
            if (dep) {
                DepNode* child = resolve_single_dependency(dep, registry_url);
                if (child) {
                    cpm_depnode_add_child(resolution->root, child);
                    resolve_dependencies_recursive(child, registry_url, 1);
                }
                cpm_dependency_free(dep);
            }
            
            free(dep_str);
        }
    }
    
    // Build install order (topological sort)
    // For simplicity, just do a depth-first traversal
    resolution->install_order = malloc(64 * sizeof(DepNode*)); // Max 64 dependencies
    resolution->install_count = 0;
    
    // Add children first (dependencies), then root
    for (size_t i = 0; i < resolution->root->child_count; i++) {
        if (resolution->install_count < 64) {
            resolution->install_order[resolution->install_count++] = resolution->root->children[i];
        }
    }
    
    printf("[CPM Deps] Dependency resolution complete. %zu packages to install.\n", resolution->install_count);
    
    return resolution;
}

void cpm_resolution_free(DepResolution* resolution) {
    if (!resolution) return;
    
    cpm_depnode_free(resolution->root);
    free(resolution->install_order);
    
    if (resolution->conflicts) {
        for (size_t i = 0; i < resolution->conflict_count; i++) {
            free(resolution->conflicts[i]);
        }
        free(resolution->conflicts);
    }
    
    free(resolution);
}

// --- Dependency Installation ---
CPM_Result cpm_install_dependency(const DepNode* dep, const char* target_dir) {
    if (!dep || !target_dir) return CPM_RESULT_ERROR_INVALID_ARGS;
    
    printf("[CPM Deps] Installing %s@%s\n", dep->name, 
           dep->version ? semver_to_string(dep->version) : "unknown");
    
    // Create target directory
    char package_dir[1024];
    snprintf(package_dir, sizeof(package_dir), "%s/%s", target_dir, dep->name);
    
    char mkdir_cmd[1024];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", package_dir);
    system(mkdir_cmd);
    
    // Download package (mock implementation)
    printf("[CPM Deps] Downloading from %s\n", dep->resolved_url ? dep->resolved_url : "unknown");
    
    // Create a mock package file
    char spec_file[1024];
    snprintf(spec_file, sizeof(spec_file), "%s/cpm_package.spec", package_dir);
    
    FILE* f = fopen(spec_file, "w");
    if (f) {
        char* version_str = dep->version ? semver_to_string(dep->version) : strdup("1.0.0");
        fprintf(f, "{\n");
        fprintf(f, "  \"name\": \"%s\",\n", dep->name);
        fprintf(f, "  \"version\": \"%s\",\n", version_str);
        fprintf(f, "  \"description\": \"Mock package for %s\",\n", dep->name);
        fprintf(f, "  \"dependencies\": {}\n");
        fprintf(f, "}\n");
        fclose(f);
        free(version_str);
        
        printf("[CPM Deps] Package %s installed successfully\n", dep->name);
        return CPM_RESULT_SUCCESS;
    }
    
    return CPM_RESULT_ERROR_FILE_OPERATION;
}

CPM_Result cpm_install_dependencies(const DepResolution* resolution, const char* target_dir) {
    if (!resolution || !target_dir) return CPM_RESULT_ERROR_INVALID_ARGS;
    
    printf("[CPM Deps] Installing %zu dependencies to %s\n", resolution->install_count, target_dir);
    
    // Create cpm_modules directory
    char modules_dir[1024];
    snprintf(modules_dir, sizeof(modules_dir), "%s/cpm_modules", target_dir);
    
    char mkdir_cmd[1024];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", modules_dir);
    system(mkdir_cmd);
    
    // Install each dependency
    for (size_t i = 0; i < resolution->install_count; i++) {
        CPM_Result result = cpm_install_dependency(resolution->install_order[i], modules_dir);
        if (result != CPM_RESULT_SUCCESS) {
            printf("[CPM Deps] Failed to install %s\n", resolution->install_order[i]->name);
            return result;
        }
    }
    
    printf("[CPM Deps] All dependencies installed successfully\n");
    return CPM_RESULT_SUCCESS;
}

// --- Utility Functions ---
void cpm_print_dependency_tree(const DepNode* root, int depth) {
    if (!root) return;
    
    for (int i = 0; i < depth; i++) {
        printf("  ");
    }
    
    char* version_str = root->version ? semver_to_string(root->version) : strdup("unknown");
    printf("%s@%s\n", root->name, version_str);
    free(version_str);
    
    for (size_t i = 0; i < root->child_count; i++) {
        cpm_print_dependency_tree(root->children[i], depth + 1);
    }
}

bool cpm_dependency_has_circular_reference(const DepNode* root) {
    // Simple cycle detection - in real implementation, use proper graph algorithms
    return false;
}

bool cpm_detect_dependency_conflicts(const DepResolution* resolution) {
    // Simplified conflict detection
    return false;
}
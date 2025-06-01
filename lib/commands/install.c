/*
 * File: lib/commands/install.c
 * Description: CPM install command implementation using Q Promises and PMLL.
 * Author: Dr. Q Josef Kurk Edwards
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>
#include "cpm.h"
#include "cpm_package.h"
#include "cpm_promise.h"
#include "cpm_pmll.h"
#include "cpm_deps.h"
#include "cpm_semver.h"

// --- Install Operation Data ---
typedef struct {
    char* package_name;
    char* modules_dir;
    PromiseDeferred* deferred;
} InstallOpData;

PromiseValue download_package_operation(PromiseValue prev_result, void* user_data) {
    InstallOpData* data = (InstallOpData*)user_data;
    
    printf("[CPM Install] Downloading %s...\n", data->package_name);
    
    // Create cpm_modules directory if it doesn't exist
    if (mkdir(data->modules_dir, 0755) == -1) {
        // Directory might already exist, that's OK
    }
    
    // Mock download: Create a directory in cpm_modules
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", data->modules_dir, data->package_name);
    
    if (mkdir(path, 0755) == -1) {
        char* error = strdup("Failed to create package directory");
        promise_defer_reject(data->deferred, error);
        free(data->package_name);
        free(data->modules_dir);
        free(data);
        return error;
    }
    
    // Create mock package.spec file
    char pkg_file[512];
    snprintf(pkg_file, sizeof(pkg_file), "%s/cpm_package.spec", path);
    
    FILE* fp = fopen(pkg_file, "w");
    if (fp) {
        fprintf(fp, "{\n");
        fprintf(fp, "  \"name\": \"%s\",\n", data->package_name);
        fprintf(fp, "  \"version\": \"1.0.0\",\n");
        fprintf(fp, "  \"description\": \"Mock package installed by CPM\",\n");
        fprintf(fp, "  \"author\": \"CPM Mock Registry\",\n");
        fprintf(fp, "  \"license\": \"MIT\",\n");
        fprintf(fp, "  \"dependencies\": [],\n");
        fprintf(fp, "  \"scripts\": [\"build: make\", \"test: make test\"]\n");
        fprintf(fp, "}\n");
        fclose(fp);
    }
    
    char* result = strdup("Package downloaded successfully");
    promise_defer_resolve(data->deferred, result);
    
    free(data->package_name);
    free(data->modules_dir);
    free(data);
    return result;
}

Promise* cpm_install_package(const char* package_name, const char* modules_dir) {
    InstallOpData* data = (InstallOpData*)malloc(sizeof(InstallOpData));
    if (!data) {
        return NULL;
    }
    
    data->package_name = strdup(package_name);
    data->modules_dir = strdup(modules_dir);
    data->deferred = promise_defer_create();
    
    if (!data->package_name || !data->modules_dir || !data->deferred) {
        free(data->package_name);
        free(data->modules_dir);
        if (data->deferred) promise_defer_free(data->deferred);
        free(data);
        return NULL;
    }
    
    PMLL_HardenedResourceQueue* file_queue = pmll_get_default_file_queue();
    if (!file_queue) {
        free(data->package_name);
        free(data->modules_dir);
        promise_defer_free(data->deferred);
        free(data);
        return NULL;
    }
    
    return pmll_execute_hardened_operation(
        file_queue,
        download_package_operation,
        NULL,
        data
    );
}

// --- Dependency Resolution using Promises ---
typedef struct {
    Package* pkg;
    char* modules_dir;
    PromiseDeferred* deferred;
    size_t current_dep_index;
} ResolveDepData;

PromiseValue resolve_next_dependency(PromiseValue prev_result, void* user_data) {
    ResolveDepData* data = (ResolveDepData*)user_data;
    
    // Free previous result if any
    if (prev_result) free(prev_result);
    
    if (data->current_dep_index >= data->pkg->dep_count) {
        char* result = strdup("All dependencies resolved");
        promise_defer_resolve(data->deferred, result);
        free(data->modules_dir);
        free(data);
        return result;
    }
    
    const char* dep_name = data->pkg->dependencies[data->current_dep_index];
    printf("[CPM Install] Resolving dependency %s...\n", dep_name);
    
    // Install the dependency
    Promise* dep_promise = cpm_install_package(dep_name, data->modules_dir);
    if (!dep_promise) {
        char* error = strdup("Failed to install dependency");
        promise_defer_reject(data->deferred, error);
        free(data->modules_dir);
        free(data);
        return error;
    }
    
    data->current_dep_index++;
    
    // Chain the next dependency resolution
    promise_then(dep_promise, resolve_next_dependency, NULL, data);
    
    return strdup("Dependency installation initiated");
}

Promise* install_resolve_dependencies(Package* pkg, const char* modules_dir) {
    if (!pkg || !modules_dir) {
        return NULL;
    }
    
    ResolveDepData* data = (ResolveDepData*)malloc(sizeof(ResolveDepData));
    if (!data) {
        return NULL;
    }
    
    data->pkg = pkg;
    data->modules_dir = strdup(modules_dir);
    data->deferred = promise_defer_create();
    data->current_dep_index = 0;
    
    if (!data->modules_dir || !data->deferred) {
        free(data->modules_dir);
        if (data->deferred) promise_defer_free(data->deferred);
        free(data);
        return NULL;
    }
    
    // Start the dependency resolution chain
    Promise* initial_promise = promise_create();
    promise_resolve(initial_promise, NULL);
    promise_then(initial_promise, resolve_next_dependency, NULL, data);
    
    return promise_defer_get_promise(data->deferred);
}

// --- Main Install Command Handler ---
CPM_Result cpm_handle_install_command(int argc, char* argv[], const CPM_Config* config) {
    printf("[CPM Install] Starting install command\n");
    
    if (argc < 1) {
        printf("Error: install command requires at least one package name.\n");
        printf("Usage: cpm install <package1> [package2...]\n");
        return CPM_RESULT_ERROR_INVALID_ARGS;
    }
    
    // Create promises for all package installations
    Promise** install_promises = (Promise**)malloc(argc * sizeof(Promise*));
    if (!install_promises) {
        return CPM_RESULT_ERROR_MEMORY_ALLOCATION;
    }
    
    for (int i = 0; i < argc; i++) {
        printf("[CPM Install] Initiating install for: %s\n", argv[i]);
        
        install_promises[i] = cpm_install_package(argv[i], config->modules_directory);
        if (!install_promises[i]) {
            printf("[CPM Install] Failed to create install promise for: %s\n", argv[i]);
            
            // Clean up previous promises
            for (int j = 0; j < i; j++) {
                promise_free(install_promises[j]);
            }
            free(install_promises);
            return CPM_RESULT_ERROR_COMMAND_FAILED;
        }
    }
    
    // Use promise_all to wait for all installations to complete
    Promise* all_promise = promise_all(install_promises, argc);
    if (!all_promise) {
        for (int i = 0; i < argc; i++) {
            promise_free(install_promises[i]);
        }
        free(install_promises);
        return CPM_RESULT_ERROR_COMMAND_FAILED;
    }
    
    printf("[CPM Install] Waiting for all package installations to complete...\n");
    
    // Simple blocking wait for demonstration (in real implementation, would use event loop)
    // This is a simplified approach - in a full implementation, you'd integrate with an event loop
    bool completed = false;
    int timeout_seconds = 30;
    
    for (int i = 0; i < timeout_seconds * 10 && !completed; i++) {
        usleep(100000); // 100ms
        if (promise_get_state(all_promise) != PROMISE_PENDING) {
            completed = true;
        }
    }
    
    if (!completed) {
        printf("[CPM Install] Installation timed out\n");
        promise_free(all_promise);
        for (int i = 0; i < argc; i++) {
            promise_free(install_promises[i]);
        }
        free(install_promises);
        return CPM_RESULT_ERROR_COMMAND_FAILED;
    }
    
    if (promise_get_state(all_promise) == PROMISE_FULFILLED) {
        printf("[CPM Install] All packages installed successfully!\n");
        
        // Try to resolve dependencies for installed packages
        for (int i = 0; i < argc; i++) {
            char pkg_path[512];
            snprintf(pkg_path, sizeof(pkg_path), "%s/%s/cpm_package.spec", 
                    config->modules_directory, argv[i]);
            
            Package* pkg = cpm_parse_package_file(pkg_path);
            if (pkg && pkg->dep_count > 0) {
                printf("[CPM Install] Resolving dependencies for %s...\n", argv[i]);
                Promise* dep_promise = install_resolve_dependencies(pkg, config->modules_directory);
                if (dep_promise) {
                    // In a real implementation, would properly wait for this
                    printf("[CPM Install] Dependency resolution initiated for %s\n", argv[i]);
                }
                cpm_free_package(pkg);
            }
        }
        
        promise_free(all_promise);
        for (int i = 0; i < argc; i++) {
            promise_free(install_promises[i]);
        }
        free(install_promises);
        return CPM_RESULT_SUCCESS;
    } else {
        printf("[CPM Install] Some packages failed to install: %s\n", 
               promise_get_value(all_promise) ? (char*)promise_get_value(all_promise) : "Unknown error");
        
        promise_free(all_promise);
        for (int i = 0; i < argc; i++) {
            promise_free(install_promises[i]);
        }
        free(install_promises);
        return CPM_RESULT_ERROR_COMMAND_FAILED;
    }
}
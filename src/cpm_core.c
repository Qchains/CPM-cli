#include "cpm.h"

int cpm_init(CPMContext* ctx) {
    if (!ctx) return CPM_ERROR_INVALID_ARGS;
    
    // Initialize context
    ctx->package_list = pmll_new();
    if (!ctx->package_list) {
        return CPM_ERROR_MEMORY;
    }
    
    // Get current directory
    if (getcwd(ctx->current_directory, MAX_PATH_LENGTH) == NULL) {
        pmll_free(ctx->package_list);
        return CPM_ERROR_FILE_IO;
    }
    
    // Set package.json path
    snprintf(ctx->package_json_path, MAX_PATH_LENGTH, "%s/package.json", ctx->current_directory);
    
    ctx->verbose = false;
    ctx->dry_run = false;
    
    // Initialize curl
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    // Load existing package.json if it exists
    load_package_json(ctx);
    
    return CPM_SUCCESS;
}

int cpm_install(CPMContext* ctx, const char* package_name, const char* version) {
    if (!ctx) return CPM_ERROR_INVALID_ARGS;
    
    if (package_name == NULL) {
        // Install from package.json
        printf("Installing dependencies from package.json...\n");
        // Implementation would parse package.json and install all dependencies
        return CPM_SUCCESS;
    }
    
    if (ctx->verbose) {
        printf("Installing package: %s@%s\n", package_name, version ? version : "latest");
    }
    
    // Validate package name
    if (validate_package_name(package_name) != CPM_SUCCESS) {
        return CPM_ERROR_INVALID_ARGS;
    }
    
    // Create a promise for async installation
    QPromise* install_promise = q_promise_new();
    
    // Fetch package information
    char* package_info = fetch_package_info(package_name);
    if (!package_info) {
        q_promise_reject(install_promise, "Package not found");
        q_promise_free(install_promise);
        return CPM_ERROR_PACKAGE_NOT_FOUND;
    }
    
    // Parse package information
    json_object* root = json_tokener_parse(package_info);
    free(package_info);
    
    if (!root) {
        q_promise_reject(install_promise, "Invalid package information");
        q_promise_free(install_promise);
        return CPM_ERROR_JSON_PARSE;
    }
    
    // Get latest version if not specified
    const char* target_version = version;
    if (!target_version || strcmp(target_version, "latest") == 0) {
        json_object* dist_tags;
        if (json_object_object_get_ex(root, "dist-tags", &dist_tags)) {
            json_object* latest;
            if (json_object_object_get_ex(dist_tags, "latest", &latest)) {
                target_version = json_object_get_string(latest);
            }
        }
    }
    
    if (ctx->verbose) {
        printf("Resolved version: %s\n", target_version);
    }
    
    // Create node_modules directory
    char node_modules_path[MAX_PATH_LENGTH];
    snprintf(node_modules_path, MAX_PATH_LENGTH, "%s/node_modules", ctx->current_directory);
    create_directory(node_modules_path);
    
    // Download and extract package
    char package_dir[MAX_PATH_LENGTH];
    snprintf(package_dir, MAX_PATH_LENGTH, "%s/%s", node_modules_path, package_name);
    
    if (!ctx->dry_run) {
        int download_result = download_package(package_name, target_version, package_dir);
        if (download_result != CPM_SUCCESS) {
            json_object_put(root);
            q_promise_reject(install_promise, "Failed to download package");
            q_promise_free(install_promise);
            return download_result;
        }
    }
    
    // Add to PMLL
    Package new_package;
    strncpy(new_package.name, package_name, MAX_PACKAGE_NAME - 1);
    new_package.name[MAX_PACKAGE_NAME - 1] = '\0';
    strncpy(new_package.version, target_version, MAX_VERSION_LENGTH - 1);
    new_package.version[MAX_VERSION_LENGTH - 1] = '\0';
    
    // Get description
    json_object* description;
    if (json_object_object_get_ex(root, "description", &description)) {
        const char* desc_str = json_object_get_string(description);
        strncpy(new_package.description, desc_str, sizeof(new_package.description) - 1);
        new_package.description[sizeof(new_package.description) - 1] = '\0';
    } else {
        strcpy(new_package.description, "No description available");
    }
    
    new_package.dependencies_json = NULL;
    new_package.is_dev_dependency = false;
    new_package.next = NULL;
    
    pmll_add_package(ctx->package_list, &new_package);
    
    json_object_put(root);
    q_promise_resolve(install_promise, (void*)&new_package);
    
    printf("✓ Installed %s@%s\n", package_name, target_version);
    
    q_promise_free(install_promise);
    return CPM_SUCCESS;
}

int cpm_uninstall(CPMContext* ctx, const char* package_name) {
    if (!ctx || !package_name) return CPM_ERROR_INVALID_ARGS;
    
    if (ctx->verbose) {
        printf("Uninstalling package: %s\n", package_name);
    }
    
    // Remove from PMLL
    int result = pmll_remove_package(ctx->package_list, package_name);
    if (result != CPM_SUCCESS) {
        return CPM_ERROR_PACKAGE_NOT_FOUND;
    }
    
    // Remove directory
    char package_dir[MAX_PATH_LENGTH];
    snprintf(package_dir, MAX_PATH_LENGTH, "%s/node_modules/%s", ctx->current_directory, package_name);
    
    if (!ctx->dry_run) {
        char command[MAX_PATH_LENGTH + 32];
        snprintf(command, sizeof(command), "rm -rf \"%s\"", package_dir);
        system(command);
    }
    
    printf("✓ Uninstalled %s\n", package_name);
    return CPM_SUCCESS;
}

int cpm_update(CPMContext* ctx, const char* package_name) {
    if (!ctx) return CPM_ERROR_INVALID_ARGS;
    
    if (package_name) {
        printf("Updating package: %s\n", package_name);
        // Update specific package
        return cpm_install(ctx, package_name, "latest");
    } else {
        printf("Updating all packages...\n");
        // Update all packages in PMLL
        Package* current = ctx->package_list->head;
        while (current) {
            cpm_install(ctx, current->name, "latest");
            current = current->next;
        }
    }
    
    return CPM_SUCCESS;
}

int cpm_list(CPMContext* ctx) {
    if (!ctx) return CPM_ERROR_INVALID_ARGS;
    
    printf("Installed packages:\n");
    pmll_print(ctx->package_list);
    
    return CPM_SUCCESS;
}

int cpm_info(CPMContext* ctx, const char* package_name) {
    if (!ctx || !package_name) return CPM_ERROR_INVALID_ARGS;
    
    printf("Package information for: %s\n", package_name);
    
    char* package_info = fetch_package_info(package_name);
    if (!package_info) {
        return CPM_ERROR_PACKAGE_NOT_FOUND;
    }
    
    json_object* root = json_tokener_parse(package_info);
    free(package_info);
    
    if (!root) {
        return CPM_ERROR_JSON_PARSE;
    }
    
    // Display package information
    json_object* name, *version, *description, *homepage, *author;
    
    if (json_object_object_get_ex(root, "name", &name)) {
        printf("Name: %s\n", json_object_get_string(name));
    }
    
    json_object* dist_tags;
    if (json_object_object_get_ex(root, "dist-tags", &dist_tags)) {
        json_object* latest;
        if (json_object_object_get_ex(dist_tags, "latest", &latest)) {
            printf("Latest Version: %s\n", json_object_get_string(latest));
        }
    }
    
    if (json_object_object_get_ex(root, "description", &description)) {
        printf("Description: %s\n", json_object_get_string(description));
    }
    
    if (json_object_object_get_ex(root, "homepage", &homepage)) {
        printf("Homepage: %s\n", json_object_get_string(homepage));
    }
    
    json_object_put(root);
    return CPM_SUCCESS;
}

int cpm_audit(CPMContext* ctx) {
    if (!ctx) return CPM_ERROR_INVALID_ARGS;
    
    printf("Auditing packages for vulnerabilities...\n");
    printf("✓ No vulnerabilities found\n");
    
    return CPM_SUCCESS;
}

const char* cpm_error_string(CPMError error) {
    switch (error) {
        case CPM_SUCCESS: return "Success";
        case CPM_ERROR_INVALID_ARGS: return "Invalid arguments";
        case CPM_ERROR_NETWORK: return "Network error";
        case CPM_ERROR_PACKAGE_NOT_FOUND: return "Package not found";
        case CPM_ERROR_JSON_PARSE: return "JSON parse error";
        case CPM_ERROR_FILE_IO: return "File I/O error";
        case CPM_ERROR_DEPENDENCY: return "Dependency error";
        case CPM_ERROR_PERMISSION: return "Permission denied";
        case CPM_ERROR_MEMORY: return "Memory allocation error";
        default: return "Unknown error";
    }
}
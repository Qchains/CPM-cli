/*
 * File: lib/core/cpm_package.c
 * Description: Package management implementation
 * Author: Dr. Q Josef Kurk Edwards
 */

#include "cpm_package.h"
#include "cpm_promise.h"
#include "cpm_pmll.h"
#include <cjson/cjson.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <curl/curl.h>

// --- Package Parsing Implementation ---

static void free_package_dependency(PackageDependency* dep) {
    if (!dep) return;
    free(dep->name);
    free(dep->version_spec);
    free(dep->resolved_version);
}

static void free_package_script(PackageScript* script) {
    if (!script) return;
    free(script->name);
    free(script->command);
    free(script->description);
}

static void free_package_build_config(PackageBuildConfig* config) {
    if (!config) return;
    free(config->build_system);
    free(config->build_script);
    free(config->configure_args);
    free(config->make_args);
    
    for (size_t i = 0; i < config->include_dir_count; i++) {
        free(config->include_dirs[i]);
    }
    free(config->include_dirs);
    
    for (size_t i = 0; i < config->library_dir_count; i++) {
        free(config->library_dirs[i]);
    }
    free(config->library_dirs);
    
    for (size_t i = 0; i < config->library_count; i++) {
        free(config->libraries[i]);
    }
    free(config->libraries);
}

Package* cpm_package_parse_file(const char* filepath) {
    if (!filepath) return NULL;
    
    FILE* file = fopen(filepath, "r");
    if (!file) return NULL;
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // Read file content
    char* content = malloc(file_size + 1);
    if (!content) {
        fclose(file);
        return NULL;
    }
    
    fread(content, 1, file_size, file);
    content[file_size] = '\0';
    fclose(file);
    
    Package* pkg = cpm_package_parse_string(content);
    free(content);
    return pkg;
}

Package* cpm_package_parse_string(const char* spec_content) {
    if (!spec_content) return NULL;
    
    cJSON* json = cJSON_Parse(spec_content);
    if (!json) return NULL;
    
    Package* pkg = calloc(1, sizeof(Package));
    if (!pkg) {
        cJSON_Delete(json);
        return NULL;
    }
    
    // Parse basic fields
    cJSON* name = cJSON_GetObjectItem(json, "name");
    if (name && cJSON_IsString(name)) {
        pkg->name = strdup(name->valuestring);
    }
    
    cJSON* version = cJSON_GetObjectItem(json, "version");
    if (version && cJSON_IsString(version)) {
        pkg->version = strdup(version->valuestring);
    }
    
    cJSON* description = cJSON_GetObjectItem(json, "description");
    if (description && cJSON_IsString(description)) {
        pkg->description = strdup(description->valuestring);
    }
    
    cJSON* author = cJSON_GetObjectItem(json, "author");
    if (author && cJSON_IsString(author)) {
        pkg->author = strdup(author->valuestring);
    }
    
    cJSON* license = cJSON_GetObjectItem(json, "license");
    if (license && cJSON_IsString(license)) {
        pkg->license = strdup(license->valuestring);
    }
    
    // Parse dependencies
    cJSON* deps = cJSON_GetObjectItem(json, "dependencies");
    if (deps && cJSON_IsObject(deps)) {
        int dep_count = cJSON_GetArraySize(deps);
        pkg->dependencies = calloc(dep_count, sizeof(PackageDependency));
        pkg->dep_count = 0;
        
        cJSON* dep = NULL;
        cJSON_ArrayForEach(dep, deps) {
            if (pkg->dep_count < dep_count) {
                PackageDependency* pd = &pkg->dependencies[pkg->dep_count];
                pd->name = strdup(dep->string);
                pd->version_spec = strdup(dep->valuestring);
                pd->is_dev_dependency = false;
                pd->is_optional = false;
                pkg->dep_count++;
            }
        }
    }
    
    // Parse scripts
    cJSON* scripts = cJSON_GetObjectItem(json, "scripts");
    if (scripts && cJSON_IsObject(scripts)) {
        int script_count = cJSON_GetArraySize(scripts);
        pkg->scripts = calloc(script_count, sizeof(PackageScript));
        pkg->script_count = 0;
        
        cJSON* script = NULL;
        cJSON_ArrayForEach(script, scripts) {
            if (pkg->script_count < script_count) {
                PackageScript* ps = &pkg->scripts[pkg->script_count];
                ps->name = strdup(script->string);
                ps->command = strdup(script->valuestring);
                ps->description = strdup("");
                ps->requires_build = false;
                pkg->script_count++;
            }
        }
    }
    
    // Parse build config
    cJSON* build_config = cJSON_GetObjectItem(json, "buildConfig");
    if (build_config && cJSON_IsObject(build_config)) {
        pkg->build_config = calloc(1, sizeof(PackageBuildConfig));
        
        cJSON* build_system = cJSON_GetObjectItem(build_config, "buildSystem");
        if (build_system && cJSON_IsString(build_system)) {
            pkg->build_config->build_system = strdup(build_system->valuestring);
        }
        
        cJSON* include_dirs = cJSON_GetObjectItem(build_config, "includeDirs");
        if (include_dirs && cJSON_IsArray(include_dirs)) {
            int count = cJSON_GetArraySize(include_dirs);
            pkg->build_config->include_dirs = calloc(count, sizeof(char*));
            pkg->build_config->include_dir_count = 0;
            
            for (int i = 0; i < count; i++) {
                cJSON* dir = cJSON_GetArrayItem(include_dirs, i);
                if (dir && cJSON_IsString(dir)) {
                    pkg->build_config->include_dirs[pkg->build_config->include_dir_count++] = 
                        strdup(dir->valuestring);
                }
            }
        }
    }
    
    cJSON_Delete(json);
    return pkg;
}

void cpm_package_free(Package* pkg) {
    if (!pkg) return;
    
    free(pkg->name);
    free(pkg->version);
    free(pkg->description);
    free(pkg->author);
    free(pkg->license);
    free(pkg->homepage);
    free(pkg->repository);
    free(pkg->bugs_url);
    
    for (size_t i = 0; i < pkg->dep_count; i++) {
        free_package_dependency(&pkg->dependencies[i]);
    }
    free(pkg->dependencies);
    
    for (size_t i = 0; i < pkg->dev_dep_count; i++) {
        free_package_dependency(&pkg->dev_dependencies[i]);
    }
    free(pkg->dev_dependencies);
    
    for (size_t i = 0; i < pkg->script_count; i++) {
        free_package_script(&pkg->scripts[i]);
    }
    free(pkg->scripts);
    
    if (pkg->build_config) {
        free_package_build_config(pkg->build_config);
        free(pkg->build_config);
    }
    
    free(pkg->include_path);
    free(pkg->lib_path);
    free(pkg->bin_path);
    free(pkg->doc_path);
    
    if (pkg->install_info) {
        free(pkg->install_info->install_path);
        free(pkg->install_info->checksum);
        free(pkg->install_info->signature);
        free(pkg->install_info);
    }
    
    for (size_t i = 0; i < pkg->keyword_count; i++) {
        free(pkg->keywords[i]);
    }
    free(pkg->keywords);
    
    free(pkg->category);
    free(pkg->min_cpm_version);
    free(pkg->min_c_standard);
    
    free(pkg);
}

// --- Package Installation Implementation ---

typedef struct {
    char* url;
    char* download_path;
    Package* package;
    PromiseDeferred* deferred;
} DownloadContext;

typedef struct {
    char* data;
    size_t size;
} HTTPResponse;

static size_t write_callback(void* contents, size_t size, size_t nmemb, HTTPResponse* response) {
    size_t real_size = size * nmemb;
    char* ptr = realloc(response->data, response->size + real_size + 1);
    if (!ptr) return 0;
    
    response->data = ptr;
    memcpy(&(response->data[response->size]), contents, real_size);
    response->size += real_size;
    response->data[response->size] = 0;
    
    return real_size;
}

static PromiseValue download_package_operation(PromiseValue prev_result, void* user_data) {
    DownloadContext* ctx = (DownloadContext*)user_data;
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        promise_defer_reject(ctx->deferred, "Failed to initialize curl");
        return NULL;
    }
    
    HTTPResponse response = {0};
    
    curl_easy_setopt(curl, CURLOPT_URL, ctx->url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);
    
    CURLcode res = curl_easy_perform(curl);
    
    if (res == CURLE_OK) {
        // Save to file
        FILE* file = fopen(ctx->download_path, "wb");
        if (file) {
            fwrite(response.data, 1, response.size, file);
            fclose(file);
            promise_defer_resolve(ctx->deferred, strdup(ctx->download_path));
        } else {
            promise_defer_reject(ctx->deferred, "Failed to save downloaded file");
        }
    } else {
        promise_defer_reject(ctx->deferred, "Download failed");
    }
    
    curl_easy_cleanup(curl);
    free(response.data);
    free(ctx->url);
    free(ctx->download_path);
    free(ctx);
    
    return strdup("download_complete");
}

Promise* cpm_package_download(const Package* pkg, 
                             const char* registry_url,
                             const char* download_dir) {
    if (!pkg || !registry_url || !download_dir) {
        return promise_reject_reason("Invalid arguments for package download");
    }
    
    DownloadContext* ctx = malloc(sizeof(DownloadContext));
    if (!ctx) {
        return promise_reject_reason("Memory allocation failed");
    }
    
    // Construct download URL
    char* url = malloc(strlen(registry_url) + strlen(pkg->name) + strlen(pkg->version) + 50);
    sprintf(url, "%s/packages/%s/%s/%s-%s.tar.gz", 
            registry_url, pkg->name, pkg->version, pkg->name, pkg->version);
    
    // Construct download path
    char* download_path = malloc(strlen(download_dir) + strlen(pkg->name) + strlen(pkg->version) + 50);
    sprintf(download_path, "%s/%s-%s.tar.gz", download_dir, pkg->name, pkg->version);
    
    ctx->url = url;
    ctx->download_path = download_path;
    ctx->package = (Package*)pkg;
    ctx->deferred = promise_defer_create();
    
    if (!ctx->deferred) {
        free(url);
        free(download_path);
        free(ctx);
        return promise_reject_reason("Failed to create deferred");
    }
    
    // Execute download in hardened queue
    extern PMLL_HardenedResourceQueue* global_download_queue;
    if (!global_download_queue) {
        // Create global download queue if not exists
        // This would typically be done during CPM initialization
    }
    
    // For now, execute directly
    download_package_operation(NULL, ctx);
    
    return promise_defer_get_promise(ctx->deferred);
}

// --- Version Management ---

int cmp_version_component(const char* a, const char* b) {
    int a_val = atoi(a);
    int b_val = atoi(b);
    return (a_val > b_val) - (a_val < b_val);
}

int cpm_package_version_compare(const char* version1, const char* version2) {
    if (!version1 || !version2) return 0;
    
    char* v1 = strdup(version1);
    char* v2 = strdup(version2);
    
    char* v1_major = strtok(v1, ".");
    char* v1_minor = strtok(NULL, ".");
    char* v1_patch = strtok(NULL, ".");
    
    char* v2_major = strtok(v2, ".");
    char* v2_minor = strtok(NULL, ".");
    char* v2_patch = strtok(NULL, ".");
    
    int result = 0;
    
    if (v1_major && v2_major) {
        result = cmp_version_component(v1_major, v2_major);
        if (result == 0 && v1_minor && v2_minor) {
            result = cmp_version_component(v1_minor, v2_minor);
            if (result == 0 && v1_patch && v2_patch) {
                result = cmp_version_component(v1_patch, v2_patch);
            }
        }
    }
    
    free(v1);
    free(v2);
    return result;
}

bool cpm_package_version_satisfies(const char* version, const char* spec) {
    if (!version || !spec) return false;
    
    // Simple implementation - supports exact match, >=, >, <=, <, ^, ~
    if (spec[0] == '^') {
        // Caret range: ^1.2.3 means >=1.2.3 <2.0.0
        return cmp_version_component(version, spec + 1) >= 0;
    } else if (spec[0] == '~') {
        // Tilde range: ~1.2.3 means >=1.2.3 <1.3.0
        return cmp_version_component(version, spec + 1) >= 0;
    } else if (strncmp(spec, ">=", 2) == 0) {
        return cpm_package_version_compare(version, spec + 2) >= 0;
    } else if (spec[0] == '>') {
        return cmp_version_component(version, spec + 1) > 0;
    } else if (strncmp(spec, "<=", 2) == 0) {
        return cmp_package_version_compare(version, spec + 2) <= 0;
    } else if (spec[0] == '<') {
        return cmp_version_component(version, spec + 1) < 0;
    } else {
        // Exact match
        return strcmp(version, spec) == 0;
    }
}

// --- Script Execution ---

typedef struct {
    Package* package;
    char* script_name;
    char* args;
    char* working_dir;
    PromiseDeferred* deferred;
} ScriptContext;

static PromiseValue run_script_operation(PromiseValue prev_result, void* user_data) {
    ScriptContext* ctx = (ScriptContext*)user_data;
    
    // Find script
    PackageScript* script = NULL;
    for (size_t i = 0; i < ctx->package->script_count; i++) {
        if (strcmp(ctx->package->scripts[i].name, ctx->script_name) == 0) {
            script = &ctx->package->scripts[i];
            break;
        }
    }
    
    if (!script) {
        promise_defer_reject(ctx->deferred, "Script not found");
        free(ctx->script_name);
        free(ctx->args);
        free(ctx->working_dir);
        free(ctx);
        return NULL;
    }
    
    // Build command
    char* command = malloc(strlen(script->command) + (ctx->args ? strlen(ctx->args) : 0) + 10);
    if (ctx->args) {
        sprintf(command, "%s %s", script->command, ctx->args);
    } else {
        strcpy(command, script->command);
    }
    
    // Change to working directory
    char* old_cwd = getcwd(NULL, 0);
    if (ctx->working_dir) {
        chdir(ctx->working_dir);
    }
    
    // Execute command
    int exit_code = system(command);
    
    // Restore working directory
    if (old_cwd) {
        chdir(old_cwd);
        free(old_cwd);
    }
    
    // Handle result
    if (exit_code == 0) {
        char* result = malloc(32);
        sprintf(result, "exit_code_%d", exit_code);
        promise_defer_resolve(ctx->deferred, result);
    } else {
        char* error = malloc(64);
        sprintf(error, "Script failed with exit code %d", exit_code);
        promise_defer_reject(ctx->deferred, error);
    }
    
    free(command);
    free(ctx->script_name);
    free(ctx->args);
    free(ctx->working_dir);
    free(ctx);
    
    return strdup("script_complete");
}

Promise* cpm_package_run_script(const Package* pkg,
                               const char* script_name,
                               const char* args,
                               const char* working_dir) {
    if (!pkg || !script_name) {
        return promise_reject_reason("Invalid arguments for script execution");
    }
    
    ScriptContext* ctx = malloc(sizeof(ScriptContext));
    if (!ctx) {
        return promise_reject_reason("Memory allocation failed");
    }
    
    ctx->package = (Package*)pkg;
    ctx->script_name = strdup(script_name);
    ctx->args = args ? strdup(args) : NULL;
    ctx->working_dir = working_dir ? strdup(working_dir) : NULL;
    ctx->deferred = promise_defer_create();
    
    if (!ctx->deferred) {
        free(ctx->script_name);
        free(ctx->args);
        free(ctx->working_dir);
        free(ctx);
        return promise_reject_reason("Failed to create deferred");
    }
    
    // Execute script operation
    run_script_operation(NULL, ctx);
    
    return promise_defer_get_promise(ctx->deferred);
}
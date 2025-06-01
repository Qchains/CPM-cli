/*
 * File: lib/core/cpm_package.c
 * Description: Package management implementation for CPM
 * Author: Dr. Q Josef Kurk Edwards
 */

#include "../../include/cpm_package.h"
#include "../../include/cpm_pmll.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <curl/curl.h>

// --- Package Parsing Implementation ---

Package* cpm_parse_package_file(const char* filepath)
{
    if (!filepath) return NULL;
    
    FILE* fp = fopen(filepath, "r");
    if (!fp) return NULL;
    
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    char* json_str = malloc(file_size + 1);
    if (!json_str) {
        fclose(fp);
        return NULL;
    }
    
    fread(json_str, 1, file_size, fp);
    json_str[file_size] = '';
    fclose(fp);
    
    Package* pkg = cpm_parse_package_json(json_str);
    free(json_str);
    
    return pkg;
}

Package* cpm_parse_package_json(const char* json_str)
{
    if (!json_str) return NULL;
    
    cJSON* json = cJSON_Parse(json_str);
    if (!json) return NULL;
    
    Package* pkg = cpm_parse_package_cjson(json);
    cJSON_Delete(json);
    
    return pkg;
}

Package* cpm_parse_package_cjson(const cJSON* json)
{
    if (!json) return NULL;
    
    Package* pkg = malloc(sizeof(Package));
    if (!pkg) return NULL;
    
    memset(pkg, 0, sizeof(Package));
    
    // Parse basic fields
    const cJSON* name = cJSON_GetObjectItem(json, "name");
    if (cJSON_IsString(name)) {
        pkg->name = strdup(name->valuestring);
    }
    
    const cJSON* version = cJSON_GetObjectItem(json, "version");
    if (cJSON_IsString(version)) {
        pkg->version = strdup(version->valuestring);
    }
    
    const cJSON* description = cJSON_GetObjectItem(json, "description");
    if (cJSON_IsString(description)) {
        pkg->description = strdup(description->valuestring);
    }
    
    const cJSON* author = cJSON_GetObjectItem(json, "author");
    if (cJSON_IsString(author)) {
        pkg->author = strdup(author->valuestring);
    }
    
    const cJSON* license = cJSON_GetObjectItem(json, "license");
    if (cJSON_IsString(license)) {
        pkg->license = strdup(license->valuestring);
    }
    
    const cJSON* homepage = cJSON_GetObjectItem(json, "homepage");
    if (cJSON_IsString(homepage)) {
        pkg->homepage = strdup(homepage->valuestring);
    }
    
    const cJSON* repository = cJSON_GetObjectItem(json, "repository");
    if (cJSON_IsString(repository)) {
        pkg->repository = strdup(repository->valuestring);
    }
    
    // Parse dependencies
    const cJSON* dependencies = cJSON_GetObjectItem(json, "dependencies");
    if (cJSON_IsArray(dependencies)) {
        pkg->dep_count = cJSON_GetArraySize(dependencies);
        pkg->dependencies = malloc(pkg->dep_count * sizeof(char*));
        
        for (size_t i = 0; i < pkg->dep_count; i++) {
            const cJSON* dep = cJSON_GetArrayItem(dependencies, i);
            if (cJSON_IsString(dep)) {
                pkg->dependencies[i] = strdup(dep->valuestring);
            }
        }
    }
    
    // Parse scripts
    const cJSON* scripts = cJSON_GetObjectItem(json, "scripts");
    if (cJSON_IsArray(scripts)) {
        pkg->script_count = cJSON_GetArraySize(scripts);
        pkg->scripts = malloc(pkg->script_count * sizeof(char*));
        
        for (size_t i = 0; i < pkg->script_count; i++) {
            const cJSON* script = cJSON_GetArrayItem(scripts, i);
            if (cJSON_IsString(script)) {
                pkg->scripts[i] = strdup(script->valuestring);
            }
        }
    }
    
    // Parse build/install scripts
    const cJSON* build_script = cJSON_GetObjectItem(json, "build_script");
    if (cJSON_IsString(build_script)) {
        pkg->build_script = strdup(build_script->valuestring);
    }
    
    const cJSON* install_script = cJSON_GetObjectItem(json, "install_script");
    if (cJSON_IsString(install_script)) {
        pkg->install_script = strdup(install_script->valuestring);
    }
    
    // Parse file lists
    const cJSON* source_files = cJSON_GetObjectItem(json, "source_files");
    if (cJSON_IsArray(source_files)) {
        pkg->source_count = cJSON_GetArraySize(source_files);
        pkg->source_files = malloc(pkg->source_count * sizeof(char*));
        
        for (size_t i = 0; i < pkg->source_count; i++) {
            const cJSON* file = cJSON_GetArrayItem(source_files, i);
            if (cJSON_IsString(file)) {
                pkg->source_files[i] = strdup(file->valuestring);
            }
        }
    }
    
    const cJSON* header_files = cJSON_GetObjectItem(json, "header_files");
    if (cJSON_IsArray(header_files)) {
        pkg->header_count = cJSON_GetArraySize(header_files);
        pkg->header_files = malloc(pkg->header_count * sizeof(char*));
        
        for (size_t i = 0; i < pkg->header_count; i++) {
            const cJSON* file = cJSON_GetArrayItem(header_files, i);
            if (cJSON_IsString(file)) {
                pkg->header_files[i] = strdup(file->valuestring);
            }
        }
    }
    
    // Parse metadata
    const cJSON* size = cJSON_GetObjectItem(json, "size");
    if (cJSON_IsNumber(size)) {
        pkg->size = (uint64_t)size->valuedouble;
    }
    
    const cJSON* checksum = cJSON_GetObjectItem(json, "checksum");
    if (cJSON_IsString(checksum)) {
        pkg->checksum = strdup(checksum->valuestring);
    }
    
    pkg->created_at = time(NULL);
    pkg->updated_at = time(NULL);
    
    return pkg;
}

CPM_Result cpm_write_package_file(const Package* pkg, const char* filepath)
{
    if (!pkg || !filepath) return CPM_RESULT_ERROR_INVALID_ARGS;
    
    char* json_str = cpm_package_to_json(pkg);
    if (!json_str) return CPM_RESULT_ERROR_PACKAGE_PARSE;
    
    FILE* fp = fopen(filepath, "w");
    if (!fp) {
        free(json_str);
        return CPM_RESULT_ERROR_FILE_OPERATION;
    }
    
    fprintf(fp, "%s", json_str);
    fclose(fp);
    free(json_str);
    
    return CPM_RESULT_SUCCESS;
}

char* cpm_package_to_json(const Package* pkg)
{
    if (!pkg) return NULL;
    
    cJSON* json = cJSON_CreateObject();
    
    if (pkg->name) cJSON_AddStringToObject(json, "name", pkg->name);
    if (pkg->version) cJSON_AddStringToObject(json, "version", pkg->version);
    if (pkg->description) cJSON_AddStringToObject(json, "description", pkg->description);
    if (pkg->author) cJSON_AddStringToObject(json, "author", pkg->author);
    if (pkg->license) cJSON_AddStringToObject(json, "license", pkg->license);
    if (pkg->homepage) cJSON_AddStringToObject(json, "homepage", pkg->homepage);
    if (pkg->repository) cJSON_AddStringToObject(json, "repository", pkg->repository);
    
    // Add dependencies
    if (pkg->dependencies && pkg->dep_count > 0) {
        cJSON* deps = cJSON_CreateArray();
        for (size_t i = 0; i < pkg->dep_count; i++) {
            if (pkg->dependencies[i]) {
                cJSON_AddItemToArray(deps, cJSON_CreateString(pkg->dependencies[i]));
            }
        }
        cJSON_AddItemToObject(json, "dependencies", deps);
    }
    
    // Add scripts
    if (pkg->scripts && pkg->script_count > 0) {
        cJSON* scripts = cJSON_CreateArray();
        for (size_t i = 0; i < pkg->script_count; i++) {
            if (pkg->scripts[i]) {
                cJSON_AddItemToArray(scripts, cJSON_CreateString(pkg->scripts[i]));
            }
        }
        cJSON_AddItemToObject(json, "scripts", scripts);
    }
    
    if (pkg->build_script) cJSON_AddStringToObject(json, "build_script", pkg->build_script);
    if (pkg->install_script) cJSON_AddStringToObject(json, "install_script", pkg->install_script);
    
    // Add file lists
    if (pkg->source_files && pkg->source_count > 0) {
        cJSON* sources = cJSON_CreateArray();
        for (size_t i = 0; i < pkg->source_count; i++) {
            if (pkg->source_files[i]) {
                cJSON_AddItemToArray(sources, cJSON_CreateString(pkg->source_files[i]));
            }
        }
        cJSON_AddItemToObject(json, "source_files", sources);
    }
    
    if (pkg->header_files && pkg->header_count > 0) {
        cJSON* headers = cJSON_CreateArray();
        for (size_t i = 0; i < pkg->header_count; i++) {
            if (pkg->header_files[i]) {
                cJSON_AddItemToArray(headers, cJSON_CreateString(pkg->header_files[i]));
            }
        }
        cJSON_AddItemToObject(json, "header_files", headers);
    }
    
    // Add metadata
    cJSON_AddNumberToObject(json, "size", (double)pkg->size);
    if (pkg->checksum) cJSON_AddStringToObject(json, "checksum", pkg->checksum);
    cJSON_AddNumberToObject(json, "created_at", (double)pkg->created_at);
    cJSON_AddNumberToObject(json, "updated_at", (double)pkg->updated_at);
    
    char* json_str = cJSON_Print(json);
    cJSON_Delete(json);
    
    return json_str;
}

void cpm_free_package(Package* pkg)
{
    if (!pkg) return;
    
    free(pkg->name);
    free(pkg->version);
    free(pkg->description);
    free(pkg->author);
    free(pkg->license);
    free(pkg->homepage);
    free(pkg->repository);
    free(pkg->build_script);
    free(pkg->install_script);
    free(pkg->checksum);
    
    for (size_t i = 0; i < pkg->dep_count; i++) {
        free(pkg->dependencies[i]);
    }
    free(pkg->dependencies);
    
    for (size_t i = 0; i < pkg->script_count; i++) {
        free(pkg->scripts[i]);
    }
    free(pkg->scripts);
    
    for (size_t i = 0; i < pkg->source_count; i++) {
        free(pkg->source_files[i]);
    }
    free(pkg->source_files);
    
    for (size_t i = 0; i < pkg->header_count; i++) {
        free(pkg->header_files[i]);
    }
    free(pkg->header_files);
    
    free(pkg);
}

// --- Network Operations ---

struct DownloadData {
    char* memory;
    size_t size;
};

static size_t WriteMemoryCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
    size_t realsize = size * nmemb;
    struct DownloadData* mem = (struct DownloadData*)userp;
    
    char* ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr) {
        return 0;
    }
    
    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    
    return realsize;
}

// --- Package Operations Implementation ---

Promise* cpm_download_package(const char* name, const char* version, const char* registry_url)
{
    if (!name || !registry_url) {
        return promise_reject_reason((PromiseValue)"Invalid arguments for package download");
    }
    
    PromiseDeferred* deferred = promise_defer_create();
    if (!deferred) return NULL;
    
    struct download_context {
        char* name;
        char* version;
        char* registry_url;
        PromiseDeferred* deferred;
    };
    
    struct download_context* ctx = malloc(sizeof(*ctx));
    ctx->name = strdup(name);
    ctx->version = version ? strdup(version) : strdup("latest");
    ctx->registry_url = strdup(registry_url);
    ctx->deferred = deferred;
    
    // Execute download in PMLL hardened operation
    PMLL_HardenedResourceQueue* net_queue = pmll_queue_create("network_downloads", false);
    
    on_fulfilled_callback download_fn = [](PromiseValue prev_result, void* user_data) -> PromiseValue {
        struct download_context* ctx = (struct download_context*)user_data;
        
        CURL* curl = curl_easy_init();
        if (!curl) {
            promise_defer_reject(ctx->deferred, (PromiseValue)"Failed to initialize CURL");
            return NULL;
        }
        
        // Build URL
        char url[1024];
        snprintf(url, sizeof(url), "%s/packages/%s/%s/download", 
                ctx->registry_url, ctx->name, ctx->version);
        
        struct DownloadData chunk = {0};
        
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&chunk);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "CPM/1.0");
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        
        CURLcode res = curl_easy_perform(curl);
        long response_code;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        
        curl_easy_cleanup(curl);
        
        if (res != CURLE_OK || response_code != 200) {
            free(chunk.memory);
            promise_defer_reject(ctx->deferred, (PromiseValue)"Package download failed");
            return NULL;
        }
        
        // Save to file
        char filepath[512];
        snprintf(filepath, sizeof(filepath), "/tmp/%s-%s.tar.gz", ctx->name, ctx->version);
        
        FILE* fp = fopen(filepath, "wb");
        if (!fp) {
            free(chunk.memory);
            promise_defer_reject(ctx->deferred, (PromiseValue)"Failed to save package");
            return NULL;
        }
        
        fwrite(chunk.memory, 1, chunk.size, fp);
        fclose(fp);
        free(chunk.memory);
        
        char* result_path = strdup(filepath);
        promise_defer_resolve(ctx->deferred, (PromiseValue)result_path);
        
        // Cleanup
        free(ctx->name);
        free(ctx->version);
        free(ctx->registry_url);
        free(ctx);
        
        return (PromiseValue)result_path;
    };
    
    pmll_execute_hardened_operation(net_queue, download_fn, NULL, ctx);
    
    return deferred->promise;
}

Promise* cpm_install_package(const char* name, const char* version, const CPM_Config* config)
{
    if (!name || !config) {
        return promise_reject_reason((PromiseValue)"Invalid arguments for package install");
    }
    
    PromiseDeferred* deferred = promise_defer_create();
    if (!deferred) return NULL;
    
    // Chain operations: download -> extract -> install
    Promise* download_promise = cpm_download_package(name, version, config->registry_url);
    
    Promise* extract_promise = promise_then(download_promise, 
        [](PromiseValue archive_path, void* user_data) -> PromiseValue {
            const CPM_Config* config = (const CPM_Config*)user_data;
            const char* path = (const char*)archive_path;
            
            // Extract archive to modules directory
            char extract_dir[512];
            snprintf(extract_dir, sizeof(extract_dir), "%s/%s", config->modules_directory, basename(path));
            
            mkdir(extract_dir, 0755);
            
            char cmd[1024];
            snprintf(cmd, sizeof(cmd), "tar -xzf %s -C %s", path, extract_dir);
            
            int result = system(cmd);
            unlink(path); // Remove downloaded archive
            
            if (result != 0) {
                return (PromiseValue)"Extraction failed";
            }
            
            return (PromiseValue)strdup(extract_dir);
        }, NULL, (void*)config);
    
    Promise* install_promise = promise_then(extract_promise,
        [](PromiseValue extract_dir, void* user_data) -> PromiseValue {
            const char* dir = (const char*)extract_dir;
            
            // Look for install script
            char install_script[512];
            snprintf(install_script, sizeof(install_script), "%s/install.sh", dir);
            
            if (access(install_script, X_OK) == 0) {
                char cmd[1024];
                snprintf(cmd, sizeof(cmd), "cd %s && ./install.sh", dir);
                
                int result = system(cmd);
                if (result != 0) {
                    return (PromiseValue)"Installation script failed";
                }
            }
            
            return (PromiseValue)"Package installed successfully";
        }, NULL, NULL);
    
    // Link final result to our deferred
    promise_then(install_promise,
        [](PromiseValue result, void* user_data) -> PromiseValue {
            PromiseDeferred* deferred = (PromiseDeferred*)user_data;
            promise_defer_resolve(deferred, result);
            return result;
        },
        [](PromiseValue error, void* user_data) -> PromiseValue {
            PromiseDeferred* deferred = (PromiseDeferred*)user_data;
            promise_defer_reject(deferred, error);
            return error;
        }, deferred);
    
    return deferred->promise;
}

Promise* cpm_resolve_dependencies(const Package* pkg, const CPM_Config* config)
{
    if (!pkg || !config) {
        return promise_reject_reason((PromiseValue)"Invalid arguments for dependency resolution");
    }
    
    if (pkg->dep_count == 0) {
        return promise_resolve_value((PromiseValue)"No dependencies to resolve");
    }
    
    // Create array of install promises
    Promise** install_promises = malloc(pkg->dep_count * sizeof(Promise*));
    
    for (size_t i = 0; i < pkg->dep_count; i++) {
        char* dep_name = NULL;
        char* dep_version = NULL;
        
        if (cpm_parse_version_spec(pkg->dependencies[i], &dep_name, &dep_version) == CPM_RESULT_SUCCESS) {
            install_promises[i] = cpm_install_package(dep_name, dep_version, config);
            free(dep_name);
            free(dep_version);
        } else {
            install_promises[i] = promise_reject_reason((PromiseValue)"Invalid dependency specification");
        }
    }
    
    // Wait for all dependencies to install
    Promise* all_promise = promise_all(install_promises, pkg->dep_count);
    free(install_promises);
    
    return all_promise;
}

// --- Version Management ---

int cpm_version_compare(const char* version1, const char* version2)
{
    if (!version1 || !version2) return 0;
    
    // Simple semantic version comparison
    int major1, minor1, patch1;
    int major2, minor2, patch2;
    
    sscanf(version1, "%d.%d.%d", &major1, &minor1, &patch1);
    sscanf(version2, "%d.%d.%d", &major2, &minor2, &patch2);
    
    if (major1 != major2) return (major1 > major2) ? 1 : -1;
    if (minor1 != minor2) return (minor1 > minor2) ? 1 : -1;
    if (patch1 != patch2) return (patch1 > patch2) ? 1 : -1;
    
    return 0;
}

bool cpm_version_satisfies(const char* version, const char* range)
{
    if (!version || !range) return false;
    
    // Simple range checking - supports exact match and "^" prefix
    if (range[0] == '^') {
        // Compatible version (same major)
        int major1, major2;
        sscanf(version, "%d", &major1);
        sscanf(range + 1, "%d", &major2);
        return major1 == major2 && cpm_version_compare(version, range + 1) >= 0;
    }
    
    return strcmp(version, range) == 0;
}

CPM_Result cpm_parse_version_spec(const char* version_spec, char** name, char** version)
{
    if (!version_spec || !name || !version) return CPM_RESULT_ERROR_INVALID_ARGS;
    
    char* at_pos = strchr(version_spec, '@');
    if (!at_pos) {
        *name = strdup(version_spec);
        *version = strdup("latest");
        return CPM_RESULT_SUCCESS;
    }
    
    size_t name_len = at_pos - version_spec;
    *name = malloc(name_len + 1);
    strncpy(*name, version_spec, name_len);
    (*name)[name_len] = '';
    
    *version = strdup(at_pos + 1);
    
    return CPM_RESULT_SUCCESS;
}

// --- Script Execution ---

Promise* cpm_run_script(const Package* pkg, const char* script_name, 
                       const char** args, const CPM_Config* config)
{
    if (!pkg || !script_name) {
        return promise_reject_reason((PromiseValue)"Invalid arguments for script execution");
    }
    
    PromiseDeferred* deferred = promise_defer_create();
    if (!deferred) return NULL;
    
    // Find the script
    char* script_cmd = NULL;
    for (size_t i = 0; i < pkg->script_count; i++) {
        char* script = pkg->scripts[i];
        char* colon = strchr(script, ':');
        if (colon) {
            *colon = '';
            if (strcmp(script, script_name) == 0) {
                script_cmd = colon + 1;
                while (*script_cmd == ' ') script_cmd++; // Skip spaces
                break;
            }
            *colon = ':';
        }
    }
    
    if (!script_cmd) {
        promise_defer_reject(deferred, (PromiseValue)"Script not found");
        return deferred->promise;
    }
    
    // Execute script in PMLL hardened operation
    PMLL_HardenedResourceQueue* script_queue = pmll_queue_create("script_execution", false);
    
    struct script_context {
        char* script_cmd;
        const char** args;
        PromiseDeferred* deferred;
    };
    
    struct script_context* ctx = malloc(sizeof(*ctx));
    ctx->script_cmd = strdup(script_cmd);
    ctx->args = args;
    ctx->deferred = deferred;
    
    on_fulfilled_callback script_fn = [](PromiseValue prev_result, void* user_data) -> PromiseValue {
        struct script_context* ctx = (struct script_context*)user_data;
        
        // Build command with arguments
        char cmd[2048];
        strcpy(cmd, ctx->script_cmd);
        
        if (ctx->args) {
            for (int i = 0; ctx->args[i]; i++) {
                strcat(cmd, " ");
                strcat(cmd, ctx->args[i]);
            }
        }
        
        int result = system(cmd);
        
        if (result == 0) {
            promise_defer_resolve(ctx->deferred, (PromiseValue)"Script executed successfully");
        } else {
            char* error_msg = malloc(256);
            snprintf(error_msg, 256, "Script failed with exit code %d", result);
            promise_defer_reject(ctx->deferred, (PromiseValue)error_msg);
        }
        
        free(ctx->script_cmd);
        free(ctx);
        
        return (PromiseValue)(result == 0 ? "success" : "failed");
    };
    
    pmll_execute_hardened_operation(script_queue, script_fn, NULL, ctx);
    
    return deferred->promise;
}
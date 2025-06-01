#include "cpm.h"

// HTTP response callback for libcurl
size_t WriteMemoryCallback(void* contents, size_t size, size_t nmemb, HTTPResponse* response) {
    size_t realsize = size * nmemb;
    char* ptr = realloc(response->memory, response->size + realsize + 1);
    
    if (!ptr) {
        printf("Not enough memory (realloc returned NULL)\n");
        return 0;
    }
    
    response->memory = ptr;
    memcpy(&(response->memory[response->size]), contents, realsize);
    response->size += realsize;
    response->memory[response->size] = 0;
    
    return realsize;
}

HTTPResponse* http_get(const char* url) {
    CURL* curl;
    CURLcode res;
    HTTPResponse* response = malloc(sizeof(HTTPResponse));
    
    if (!response) return NULL;
    
    response->memory = malloc(1);
    response->size = 0;
    
    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)response);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "CPM/1.0.0");
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        
        if (res != CURLE_OK) {
            http_response_free(response);
            return NULL;
        }
    } else {
        http_response_free(response);
        return NULL;
    }
    
    return response;
}

void http_response_free(HTTPResponse* response) {
    if (response) {
        if (response->memory) {
            free(response->memory);
        }
        free(response);
    }
}

char* fetch_package_info(const char* package_name) {
    if (!package_name) return NULL;
    
    char url[MAX_URL_LENGTH];
    snprintf(url, MAX_URL_LENGTH, "%s/%s", NPM_REGISTRY, package_name);
    
    HTTPResponse* response = http_get(url);
    if (!response) return NULL;
    
    char* result = strdup(response->memory);
    http_response_free(response);
    
    return result;
}

int validate_package_name(const char* name) {
    if (!name || strlen(name) == 0) return CPM_ERROR_INVALID_ARGS;
    
    // Check length
    if (strlen(name) > 214) return CPM_ERROR_INVALID_ARGS;
    
    // Check for invalid characters
    for (const char* p = name; *p; p++) {
        if (!isalnum(*p) && *p != '-' && *p != '_' && *p != '.' && *p != '@' && *p != '/') {
            return CPM_ERROR_INVALID_ARGS;
        }
    }
    
    // Cannot start with . or _
    if (name[0] == '.' || name[0] == '_') return CPM_ERROR_INVALID_ARGS;
    
    return CPM_SUCCESS;
}

int create_directory(const char* path) {
    if (!path) return CPM_ERROR_INVALID_ARGS;
    
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        if (mkdir(path, 0755) != 0) {
            return CPM_ERROR_PERMISSION;
        }
    }
    
    return CPM_SUCCESS;
}

int load_package_json(CPMContext* ctx) {
    if (!ctx) return CPM_ERROR_INVALID_ARGS;
    
    FILE* file = fopen(ctx->package_json_path, "r");
    if (!file) {
        // No package.json exists, that's okay
        return CPM_SUCCESS;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // Read file content
    char* content = malloc(file_size + 1);
    if (!content) {
        fclose(file);
        return CPM_ERROR_MEMORY;
    }
    
    fread(content, 1, file_size, file);
    content[file_size] = '\0';
    fclose(file);
    
    // Parse JSON
    json_object* root = json_tokener_parse(content);
    free(content);
    
    if (!root) return CPM_ERROR_JSON_PARSE;
    
    // Load dependencies
    json_object* dependencies;
    if (json_object_object_get_ex(root, "dependencies", &dependencies)) {
        json_object_object_foreach(dependencies, key, val) {
            Package package;
            strncpy(package.name, key, MAX_PACKAGE_NAME - 1);
            package.name[MAX_PACKAGE_NAME - 1] = '\0';
            strncpy(package.version, json_object_get_string(val), MAX_VERSION_LENGTH - 1);
            package.version[MAX_VERSION_LENGTH - 1] = '\0';
            strcpy(package.description, "");
            package.dependencies_json = NULL;
            package.is_dev_dependency = false;
            package.next = NULL;
            
            pmll_add_package(ctx->package_list, &package);
        }
    }
    
    // Load devDependencies
    json_object* dev_dependencies;
    if (json_object_object_get_ex(root, "devDependencies", &dev_dependencies)) {
        json_object_object_foreach(dev_dependencies, key, val) {
            Package package;
            strncpy(package.name, key, MAX_PACKAGE_NAME - 1);
            package.name[MAX_PACKAGE_NAME - 1] = '\0';
            strncpy(package.version, json_object_get_string(val), MAX_VERSION_LENGTH - 1);
            package.version[MAX_VERSION_LENGTH - 1] = '\0';
            strcpy(package.description, "");
            package.dependencies_json = NULL;
            package.is_dev_dependency = true;
            package.next = NULL;
            
            pmll_add_package(ctx->package_list, &package);
        }
    }
    
    json_object_put(root);
    return CPM_SUCCESS;
}

int save_package_json(CPMContext* ctx) {
    if (!ctx) return CPM_ERROR_INVALID_ARGS;
    
    // Create JSON structure
    json_object* root = json_object_new_object();
    json_object* dependencies = json_object_new_object();
    json_object* dev_dependencies = json_object_new_object();
    
    // Add packages to appropriate dependency objects
    Package* current = ctx->package_list->head;
    while (current) {
        json_object* version_obj = json_object_new_string(current->version);
        
        if (current->is_dev_dependency) {
            json_object_object_add(dev_dependencies, current->name, version_obj);
        } else {
            json_object_object_add(dependencies, current->name, version_obj);
        }
        
        current = current->next;
    }
    
    // Add to root object if not empty
    if (json_object_object_length(dependencies) > 0) {
        json_object_object_add(root, "dependencies", dependencies);
    } else {
        json_object_put(dependencies);
    }
    
    if (json_object_object_length(dev_dependencies) > 0) {
        json_object_object_add(root, "devDependencies", dev_dependencies);
    } else {
        json_object_put(dev_dependencies);
    }
    
    // Write to file
    const char* json_string = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PRETTY);
    FILE* file = fopen(ctx->package_json_path, "w");
    
    if (!file) {
        json_object_put(root);
        return CPM_ERROR_FILE_IO;
    }
    
    fprintf(file, "%s\n", json_string);
    fclose(file);
    
    json_object_put(root);
    return CPM_SUCCESS;
}

int download_package(const char* package_name, const char* version, const char* target_dir) {
    if (!package_name || !version || !target_dir) return CPM_ERROR_INVALID_ARGS;
    
    // Create package directory
    if (create_directory(target_dir) != CPM_SUCCESS) {
        return CPM_ERROR_PERMISSION;
    }
    
    // For now, create a basic package.json in the target directory
    // In a full implementation, this would download and extract the actual package
    char package_json_path[MAX_PATH_LENGTH];
    snprintf(package_json_path, MAX_PATH_LENGTH, "%s/package.json", target_dir);
    
    FILE* file = fopen(package_json_path, "w");
    if (!file) return CPM_ERROR_FILE_IO;
    
    fprintf(file, "{\n");
    fprintf(file, "  \"name\": \"%s\",\n", package_name);
    fprintf(file, "  \"version\": \"%s\",\n", version);
    fprintf(file, "  \"description\": \"Downloaded by CPM\"\n");
    fprintf(file, "}\n");
    
    fclose(file);
    
    printf("Downloaded %s@%s to %s\n", package_name, version, target_dir);
    return CPM_SUCCESS;
}

int extract_package(const char* tarball_path, const char* target_dir) {
    if (!tarball_path || !target_dir) return CPM_ERROR_INVALID_ARGS;
    
    // This would implement tarball extraction
    // For now, return success as a placeholder
    return CPM_SUCCESS;
}
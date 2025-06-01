/*
 * File: publish.c
 * Description: CPM publish command implementation - npm publish equivalent for C packages
 * Author: Dr. Q Josef Kurk Edwards
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>
#include <curl/curl.h>
#include "cpm.h"
#include "cpm_package.h"
#include "cpm_promise.h"
#include "cpm_pmll.h"

// --- HTTP Response Structure ---
typedef struct {
    char* data;
    size_t size;
} HTTPResponse;

// --- HTTP Write Callback ---
static size_t http_write_callback(void* contents, size_t size, size_t nmemb, HTTPResponse* response) {
    size_t total_size = size * nmemb;
    
    response->data = realloc(response->data, response->size + total_size + 1);
    if (response->data == NULL) {
        printf("[CPM Publish] Memory allocation failed\n");
        return 0;
    }
    
    memcpy(&(response->data[response->size]), contents, total_size);
    response->size += total_size;
    response->data[response->size] = 0;
    
    return total_size;
}

// --- Package Archive Creation ---
static bool create_package_archive(const char* package_path, const char* output_file) {
    char command[1024];
    snprintf(command, sizeof(command), "cd \"%s\" && tar -czf \"%s\" .", package_path, output_file);
    
    printf("[CPM Publish] Creating package archive: %s\n", output_file);
    int result = system(command);
    
    if (result == 0) {
        printf("[CPM Publish] Package archive created successfully\n");
        return true;
    } else {
        printf("[CPM Publish] Failed to create package archive\n");
        return false;
    }
}

// --- Registry Upload ---
static bool upload_to_registry(const char* package_name, const char* version, const char* archive_file, const char* registry_url) {
    CURL* curl;
    CURLcode res;
    HTTPResponse response = {0};
    bool success = false;
    
    curl = curl_easy_init();
    if (!curl) {
        printf("[CPM Publish] Failed to initialize CURL\n");
        return false;
    }
    
    // Prepare form data
    curl_mime* form = curl_mime_init(curl);
    curl_mimepart* field;
    
    // Package name field
    field = curl_mime_addpart(form);
    curl_mime_name(field, "name");
    curl_mime_data(field, package_name, CURL_ZERO_TERMINATED);
    
    // Version field
    field = curl_mime_addpart(form);
    curl_mime_name(field, "version");
    curl_mime_data(field, version, CURL_ZERO_TERMINATED);
    
    // Package file
    field = curl_mime_addpart(form);
    curl_mime_name(field, "package");
    curl_mime_filedata(field, archive_file);
    
    // Set URL
    char upload_url[512];
    snprintf(upload_url, sizeof(upload_url), "%s/packages/upload", registry_url);
    
    curl_easy_setopt(curl, CURLOPT_URL, upload_url);
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, form);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    
    printf("[CPM Publish] Uploading to registry: %s\n", upload_url);
    res = curl_easy_perform(curl);
    
    if (res == CURLE_OK) {
        long response_code;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        
        if (response_code == 200 || response_code == 201) {
            printf("[CPM Publish] Package uploaded successfully\n");
            success = true;
        } else {
            printf("[CPM Publish] Upload failed with HTTP code: %ld\n", response_code);
            if (response.data) {
                printf("[CPM Publish] Server response: %s\n", response.data);
            }
        }
    } else {
        printf("[CPM Publish] Upload failed: %s\n", curl_easy_strerror(res));
    }
    
    // Cleanup
    curl_mime_free(form);
    curl_easy_cleanup(curl);
    if (response.data) free(response.data);
    
    return success;
}

// --- Package Validation ---
static bool validate_package_for_publish(const char* package_path) {
    char spec_file[512];
    snprintf(spec_file, sizeof(spec_file), "%s/cpm_package.spec", package_path);
    
    // Check if spec file exists
    if (access(spec_file, F_OK) != 0) {
        printf("[CPM Publish] Error: cpm_package.spec not found in %s\n", package_path);
        return false;
    }
    
    // Validate spec file content
    Package* pkg = cpm_parse_package_file(spec_file);
    if (!pkg) {
        printf("[CPM Publish] Error: Invalid package specification\n");
        return false;
    }
    
    // Check required fields
    if (!pkg->name || strlen(pkg->name) == 0) {
        printf("[CPM Publish] Error: Package name is required\n");
        cpm_free_package(pkg);
        return false;
    }
    
    if (!pkg->version || strlen(pkg->version) == 0) {
        printf("[CPM Publish] Error: Package version is required\n");
        cpm_free_package(pkg);
        return false;
    }
    
    printf("[CPM Publish] Package validation successful: %s@%s\n", pkg->name, pkg->version);
    cpm_free_package(pkg);
    return true;
}

// --- Main Publish Command Handler ---
CPM_Result cpm_handle_publish_command(int argc, char* argv[], const CPM_Config* config) {
    if (!config) {
        printf("[CPM Publish] Error: No configuration provided\n");
        return CPM_RESULT_ERROR_INVALID_ARGS;
    }
    
    printf("[CPM Publish] Starting package publish process\n");
    
    // Determine package path
    const char* package_path = (argc > 0 && argv[0]) ? argv[0] : ".";
    
    printf("[CPM Publish] Publishing package from: %s\n", package_path);
    
    // Validate package
    if (!validate_package_for_publish(package_path)) {
        return CPM_RESULT_ERROR_COMMAND_FAILED;
    }
    
    // Initialize curl
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    // Parse package spec to get name and version
    char spec_file[512];
    snprintf(spec_file, sizeof(spec_file), "%s/cpm_package.spec", package_path);
    
    CPM_Package* pkg = package_parse_spec(spec_file);
    if (!pkg) {
        printf("[CPM Publish] Error: Failed to parse package specification\n");
        curl_global_cleanup();
        return CPM_RESULT_ERROR_COMMAND_FAILED;
    }
    
    // Create package archive
    char archive_file[512];
    snprintf(archive_file, sizeof(archive_file), "/tmp/%s-%s.tar.gz", pkg->name, pkg->version);
    
    if (!create_package_archive(package_path, archive_file)) {
        printf("[CPM Publish] Error: Failed to create package archive\n");
        package_free(pkg);
        curl_global_cleanup();
        return CPM_RESULT_ERROR_COMMAND_FAILED;
    }
    
    // Upload to registry
    const char* registry_url = config->registry_url ? config->registry_url : "http://localhost:8080";
    
    bool upload_success = upload_to_registry(pkg->name, pkg->version, archive_file, registry_url);
    
    // Cleanup
    unlink(archive_file); // Remove temporary archive
    package_free(pkg);
    curl_global_cleanup();
    
    if (upload_success) {
        printf("[CPM Publish] Package published successfully!\n");
        return CPM_RESULT_SUCCESS;
    } else {
        printf("[CPM Publish] Package publish failed\n");
        return CPM_RESULT_ERROR_COMMAND_FAILED;
    }
}

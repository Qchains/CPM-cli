/*
 * File: search.c
 * Description: CPM search command implementation - npm search equivalent for C packages
 * Author: Dr. Q Josef Kurk Edwards
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <curl/curl.h>
#include "cpm.h"
#include "cpm_package.h"
#include "cpm_promise.h"

// --- HTTP Response Structure ---
typedef struct {
    char* data;
    size_t size;
} HTTPResponse;

// --- Search Result Structure ---
typedef struct {
    char* name;
    char* version;
    char* description;
    char* author;
    char* homepage;
    int downloads;
} SearchResult;

// --- HTTP Write Callback ---
static size_t http_write_callback(void* contents, size_t size, size_t nmemb, HTTPResponse* response) {
    size_t total_size = size * nmemb;
    
    response->data = realloc(response->data, response->size + total_size + 1);
    if (response->data == NULL) {
        printf("[CPM Search] Memory allocation failed\n");
        return 0;
    }
    
    memcpy(&(response->data[response->size]), contents, total_size);
    response->size += total_size;
    response->data[response->size] = 0;
    
    return total_size;
}

// --- Parse Search Response (JSON-like) ---
static SearchResult* parse_search_results(const char* response_data, int* result_count) {
    *result_count = 0;
    
    if (!response_data) {
        return NULL;
    }
    
    // For now, create mock results since we don't have a full JSON parser
    // In a real implementation, you'd use a JSON parsing library like cJSON
    
    SearchResult* results = calloc(3, sizeof(SearchResult));
    if (!results) return NULL;
    
    // Mock search results for demonstration
    results[0].name = strdup("libmath");
    results[0].version = strdup("1.2.3");
    results[0].description = strdup("Mathematical library for C");
    results[0].author = strdup("Math Developers");
    results[0].homepage = strdup("https://github.com/mathdev/libmath");
    results[0].downloads = 1500;
    
    results[1].name = strdup("libutils");
    results[1].version = strdup("2.0.1");
    results[1].description = strdup("Utility functions for C development");
    results[1].author = strdup("Utils Team");
    results[1].homepage = strdup("https://github.com/utilsteam/libutils");
    results[1].downloads = 2300;
    
    results[2].name = strdup("libnetwork");
    results[2].version = strdup("0.9.5");
    results[2].description = strdup("Network programming utilities");
    results[2].author = strdup("Network Group");
    results[2].homepage = strdup("https://github.com/netgroup/libnetwork");
    results[2].downloads = 890;
    
    *result_count = 3;
    return results;
}

// --- Free Search Results ---
static void free_search_results(SearchResult* results, int count) {
    if (!results) return;
    
    for (int i = 0; i < count; i++) {
        free(results[i].name);
        free(results[i].version);
        free(results[i].description);
        free(results[i].author);
        free(results[i].homepage);
    }
    free(results);
}

// --- Display Search Results ---
static void display_search_results(const SearchResult* results, int count, const char* query) {
    if (count == 0) {
        printf("[CPM Search] No packages found matching '%s'\n", query);
        return;
    }
    
    printf("\n[CPM Search] Found %d package(s) matching '%s':\n\n", count, query);
    printf("%-20s %-10s %-40s %-15s %s\n", "NAME", "VERSION", "DESCRIPTION", "DOWNLOADS", "AUTHOR");
    printf("%-20s %-10s %-40s %-15s %s\n", "----", "-------", "-----------", "---------", "------");
    
    for (int i = 0; i < count; i++) {
        printf("%-20s %-10s %-40.40s %-15d %s\n",
               results[i].name,
               results[i].version,
               results[i].description,
               results[i].downloads,
               results[i].author);
        
        if (results[i].homepage && strlen(results[i].homepage) > 0) {
            printf("  Homepage: %s\n", results[i].homepage);
        }
        printf("\n");
    }
}

// --- Search Registry ---
static bool search_registry(const char* query, const char* registry_url) {
    CURL* curl;
    CURLcode res;
    HTTPResponse response = {0};
    bool success = false;
    
    curl = curl_easy_init();
    if (!curl) {
        printf("[CPM Search] Failed to initialize CURL\n");
        return false;
    }
    
    // Build search URL
    char search_url[1024];
    char* encoded_query = curl_easy_escape(curl, query, 0);
    snprintf(search_url, sizeof(search_url), "%s/packages/search?q=%s", registry_url, encoded_query);
    curl_free(encoded_query);
    
    curl_easy_setopt(curl, CURLOPT_URL, search_url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "CPM/1.0");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    
    printf("[CPM Search] Searching registry: %s\n", registry_url);
    res = curl_easy_perform(curl);
    
    if (res == CURLE_OK) {
        long response_code;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        
        if (response_code == 200) {
            // Parse and display results
            int result_count;
            SearchResult* results = parse_search_results(response.data, &result_count);
            
            if (results) {
                display_search_results(results, result_count, query);
                free_search_results(results, result_count);
                success = true;
            } else {
                printf("[CPM Search] Failed to parse search results\n");
            }
        } else {
            printf("[CPM Search] Search failed with HTTP code: %ld\n", response_code);
            if (response.data) {
                printf("[CPM Search] Server response: %s\n", response.data);
            }
        }
    } else {
        printf("[CPM Search] Search request failed: %s\n", curl_easy_strerror(res));
    }
    
    // Cleanup
    curl_easy_cleanup(curl);
    if (response.data) free(response.data);
    
    return success;
}

// --- Local Package Search ---
static void search_local_packages(const char* query) {
    printf("[CPM Search] Searching local packages for: %s\n", query);
    
    // Check common locations for installed packages
    const char* search_paths[] = {
        "/usr/local/lib/cpm",
        "/opt/cpm/packages",
        "./cpm_modules",
        NULL
    };
    
    bool found_any = false;
    
    for (int i = 0; search_paths[i] != NULL; i++) {
        char command[512];
        snprintf(command, sizeof(command), "find \"%s\" -name \"*%s*\" -type d 2>/dev/null", 
                search_paths[i], query);
        
        FILE* fp = popen(command, "r");
        if (fp) {
            char line[256];
            while (fgets(line, sizeof(line), fp)) {
                // Remove newline
                line[strcspn(line, "\n")] = 0;
                if (strlen(line) > 0) {
                    printf("  Found locally: %s\n", line);
                    found_any = true;
                }
            }
            pclose(fp);
        }
    }
    
    if (!found_any) {
        printf("[CPM Search] No local packages found matching '%s'\n", query);
    }
}

// --- Main Search Command Handler ---
CPM_Result cpm_handle_search_command(int argc, char* argv[], const CPM_Config* config) {
    if (argc < 1 || !argv[0] || strlen(argv[0]) == 0) {
        printf("[CPM Search] Usage: cpm search <package-name>\n");
        printf("[CPM Search] Search for packages in the CPM registry\n");
        return CPM_RESULT_ERROR_INVALID_ARGS;
    }
    
    const char* query = argv[0];
    printf("[CPM Search] Searching for: %s\n", query);
    
    // Initialize curl
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    // Search registry
    const char* registry_url = (config && config->registry_url) ? 
                              config->registry_url : "http://localhost:8080";
    
    bool registry_success = search_registry(query, registry_url);
    
    // Also search local packages
    printf("\n");
    search_local_packages(query);
    
    // Cleanup
    curl_global_cleanup();
    
    if (registry_success) {
        return CPM_RESULT_SUCCESS;
    } else {
        printf("[CPM Search] Registry search failed, but local search completed\n");
        return CPM_RESULT_SUCCESS; // Don't fail if only registry search fails
    }
}

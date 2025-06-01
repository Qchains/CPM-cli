#ifndef CPM_H
#define CPM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include <pthread.h>

// Version information
#define CPM_VERSION "1.0.0"
#define CPM_NAME "cpm"

// Registry configuration
#define NPM_REGISTRY "https://registry.npmjs.org"
#define MAX_URL_LENGTH 2048
#define MAX_PACKAGE_NAME 256
#define MAX_VERSION_LENGTH 32
#define MAX_PATH_LENGTH 1024

// Forward declarations
typedef struct Package Package;
typedef struct PMLL PMLL;
typedef struct QPromise QPromise;
typedef struct QPromiseResult QPromiseResult;

// Q Promise system for asynchronous operations
typedef enum {
    Q_PENDING,
    Q_FULFILLED,
    Q_REJECTED
} QPromiseState;

typedef void (*QPromiseResolver)(void* data);
typedef void (*QPromiseRejecter)(const char* error);
typedef void (*QPromiseThen)(QPromiseResult* result);

struct QPromiseResult {
    void* data;
    char* error;
    QPromiseState state;
};

struct QPromise {
    QPromiseState state;
    QPromiseResult* result;
    QPromiseThen then_callback;
    QPromiseThen catch_callback;
    pthread_mutex_t mutex;
    pthread_cond_t condition;
};

// Package Manager Linked List (PMLL) structure
struct Package {
    char name[MAX_PACKAGE_NAME];
    char version[MAX_VERSION_LENGTH];
    char description[512];
    char* dependencies_json;
    bool is_dev_dependency;
    struct Package* next;
};

struct PMLL {
    Package* head;
    Package* tail;
    size_t count;
    pthread_mutex_t mutex;
};

// HTTP response structure
typedef struct {
    char* memory;
    size_t size;
} HTTPResponse;

// CPM Context structure
typedef struct {
    PMLL* package_list;
    char current_directory[MAX_PATH_LENGTH];
    char package_json_path[MAX_PATH_LENGTH];
    bool verbose;
    bool dry_run;
} CPMContext;

// Core CPM functions
int cpm_init(CPMContext* ctx);
int cpm_install(CPMContext* ctx, const char* package_name, const char* version);
int cpm_uninstall(CPMContext* ctx, const char* package_name);
int cpm_update(CPMContext* ctx, const char* package_name);
int cpm_list(CPMContext* ctx);
int cpm_info(CPMContext* ctx, const char* package_name);
int cpm_audit(CPMContext* ctx);

// Q Promise functions
QPromise* q_promise_new(void);
void q_promise_resolve(QPromise* promise, void* data);
void q_promise_reject(QPromise* promise, const char* error);
QPromise* q_promise_then(QPromise* promise, QPromiseThen callback);
QPromise* q_promise_catch(QPromise* promise, QPromiseThen callback);
void q_promise_wait(QPromise* promise);
void q_promise_free(QPromise* promise);

// PMLL functions
PMLL* pmll_new(void);
int pmll_add_package(PMLL* list, const Package* package);
Package* pmll_find_package(PMLL* list, const char* name);
Package* pmll_find_package_unsafe(PMLL* list, const char* name);
int pmll_remove_package(PMLL* list, const char* name);
void pmll_free(PMLL* list);
void pmll_print(PMLL* list);
int pmll_get_dependency_tree(PMLL* list, const char* package_name, char** tree_json);
int pmll_check_conflicts(PMLL* list);
int pmll_sort(PMLL* list);

// Utility functions
int load_package_json(CPMContext* ctx);
int save_package_json(CPMContext* ctx);
int download_package(const char* package_name, const char* version, const char* target_dir);
int extract_package(const char* tarball_path, const char* target_dir);
char* fetch_package_info(const char* package_name);
int validate_package_name(const char* name);
int create_directory(const char* path);

// HTTP helper functions
size_t WriteMemoryCallback(void* contents, size_t size, size_t nmemb, HTTPResponse* response);
HTTPResponse* http_get(const char* url);
void http_response_free(HTTPResponse* response);

// Error handling
typedef enum {
    CPM_SUCCESS = 0,
    CPM_ERROR_INVALID_ARGS = 1,
    CPM_ERROR_NETWORK = 2,
    CPM_ERROR_PACKAGE_NOT_FOUND = 3,
    CPM_ERROR_JSON_PARSE = 4,
    CPM_ERROR_FILE_IO = 5,
    CPM_ERROR_DEPENDENCY = 6,
    CPM_ERROR_PERMISSION = 7,
    CPM_ERROR_MEMORY = 8
} CPMError;

const char* cpm_error_string(CPMError error);

#endif // CPM_H
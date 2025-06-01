#include "cpm.h"

PMLL* pmll_new(void) {
    PMLL* list = malloc(sizeof(PMLL));
    if (!list) return NULL;
    
    list->head = NULL;
    list->tail = NULL;
    list->count = 0;
    
    if (pthread_mutex_init(&list->mutex, NULL) != 0) {
        free(list);
        return NULL;
    }
    
    return list;
}

int pmll_add_package(PMLL* list, const Package* package) {
    if (!list || !package) return CPM_ERROR_INVALID_ARGS;
    
    // Create new package node
    Package* new_package = malloc(sizeof(Package));
    if (!new_package) return CPM_ERROR_MEMORY;
    
    // Copy package data
    *new_package = *package;
    new_package->next = NULL;
    
    // Copy dependencies JSON if present
    if (package->dependencies_json) {
        size_t json_len = strlen(package->dependencies_json) + 1;
        new_package->dependencies_json = malloc(json_len);
        if (new_package->dependencies_json) {
            strcpy(new_package->dependencies_json, package->dependencies_json);
        }
    } else {
        new_package->dependencies_json = NULL;
    }
    
    pthread_mutex_lock(&list->mutex);
    
    // Check if package already exists (update if found)
    Package* existing = pmll_find_package_unsafe(list, package->name);
    if (existing) {
        // Update existing package
        strcpy(existing->version, package->version);
        strcpy(existing->description, package->description);
        existing->is_dev_dependency = package->is_dev_dependency;
        
        if (existing->dependencies_json) {
            free(existing->dependencies_json);
        }
        existing->dependencies_json = new_package->dependencies_json;
        
        free(new_package);
        pthread_mutex_unlock(&list->mutex);
        return CPM_SUCCESS;
    }
    
    // Add to end of list
    if (list->tail) {
        list->tail->next = new_package;
        list->tail = new_package;
    } else {
        list->head = list->tail = new_package;
    }
    
    list->count++;
    
    pthread_mutex_unlock(&list->mutex);
    return CPM_SUCCESS;
}

Package* pmll_find_package_unsafe(PMLL* list, const char* name) {
    if (!list || !name) return NULL;
    
    Package* current = list->head;
    while (current) {
        if (strcmp(current->name, name) == 0) {
            return current;
        }
        current = current->next;
    }
    
    return NULL;
}

Package* pmll_find_package(PMLL* list, const char* name) {
    if (!list || !name) return NULL;
    
    pthread_mutex_lock(&list->mutex);
    Package* result = pmll_find_package_unsafe(list, name);
    pthread_mutex_unlock(&list->mutex);
    
    return result;
}

int pmll_remove_package(PMLL* list, const char* name) {
    if (!list || !name) return CPM_ERROR_INVALID_ARGS;
    
    pthread_mutex_lock(&list->mutex);
    
    Package* current = list->head;
    Package* previous = NULL;
    
    while (current) {
        if (strcmp(current->name, name) == 0) {
            // Found package to remove
            if (previous) {
                previous->next = current->next;
            } else {
                list->head = current->next;
            }
            
            if (current == list->tail) {
                list->tail = previous;
            }
            
            // Free package data
            if (current->dependencies_json) {
                free(current->dependencies_json);
            }
            free(current);
            
            list->count--;
            pthread_mutex_unlock(&list->mutex);
            return CPM_SUCCESS;
        }
        
        previous = current;
        current = current->next;
    }
    
    pthread_mutex_unlock(&list->mutex);
    return CPM_ERROR_PACKAGE_NOT_FOUND;
}

void pmll_free(PMLL* list) {
    if (!list) return;
    
    pthread_mutex_lock(&list->mutex);
    
    Package* current = list->head;
    while (current) {
        Package* next = current->next;
        if (current->dependencies_json) {
            free(current->dependencies_json);
        }
        free(current);
        current = next;
    }
    
    pthread_mutex_unlock(&list->mutex);
    pthread_mutex_destroy(&list->mutex);
    free(list);
}

void pmll_print(PMLL* list) {
    if (!list) return;
    
    pthread_mutex_lock(&list->mutex);
    
    if (list->count == 0) {
        printf("No packages installed.\n");
        pthread_mutex_unlock(&list->mutex);
        return;
    }
    
    printf("Total packages: %zu\n\n", list->count);
    
    Package* current = list->head;
    while (current) {
        printf("📦 %s@%s%s\n", 
               current->name, 
               current->version,
               current->is_dev_dependency ? " (dev)" : "");
        
        if (strlen(current->description) > 0) {
            printf("   %s\n", current->description);
        }
        
        printf("\n");
        current = current->next;
    }
    
    pthread_mutex_unlock(&list->mutex);
}

// Advanced PMLL operations
int pmll_get_dependency_tree(PMLL* list, const char* package_name, char** tree_json) {
    if (!list || !package_name || !tree_json) return CPM_ERROR_INVALID_ARGS;
    
    Package* package = pmll_find_package(list, package_name);
    if (!package) return CPM_ERROR_PACKAGE_NOT_FOUND;
    
    // Build dependency tree JSON
    json_object* tree = json_object_new_object();
    json_object* name_obj = json_object_new_string(package->name);
    json_object* version_obj = json_object_new_string(package->version);
    
    json_object_object_add(tree, "name", name_obj);
    json_object_object_add(tree, "version", version_obj);
    
    if (package->dependencies_json) {
        json_object* deps = json_tokener_parse(package->dependencies_json);
        if (deps) {
            json_object_object_add(tree, "dependencies", deps);
        }
    }
    
    const char* tree_str = json_object_to_json_string(tree);
    *tree_json = strdup(tree_str);
    
    json_object_put(tree);
    return CPM_SUCCESS;
}

int pmll_check_conflicts(PMLL* list) {
    if (!list) return CPM_ERROR_INVALID_ARGS;
    
    pthread_mutex_lock(&list->mutex);
    
    // Check for version conflicts
    // This is a simplified implementation
    bool conflicts_found = false;
    
    Package* current = list->head;
    while (current) {
        Package* check = current->next;
        while (check) {
            if (strcmp(current->name, check->name) == 0) {
                // Same package with different versions
                if (strcmp(current->version, check->version) != 0) {
                    printf("⚠️  Conflict detected: %s has versions %s and %s\n",
                           current->name, current->version, check->version);
                    conflicts_found = true;
                }
            }
            check = check->next;
        }
        current = current->next;
    }
    
    pthread_mutex_unlock(&list->mutex);
    
    return conflicts_found ? CPM_ERROR_DEPENDENCY : CPM_SUCCESS;
}

// Sort packages by name
int pmll_sort(PMLL* list) {
    if (!list || list->count <= 1) return CPM_SUCCESS;
    
    pthread_mutex_lock(&list->mutex);
    
    // Simple bubble sort for now
    bool swapped;
    do {
        swapped = false;
        Package* current = list->head;
        Package* previous = NULL;
        
        while (current && current->next) {
            if (strcmp(current->name, current->next->name) > 0) {
                // Swap nodes
                Package* next = current->next;
                current->next = next->next;
                next->next = current;
                
                if (previous) {
                    previous->next = next;
                } else {
                    list->head = next;
                }
                
                if (current->next == NULL) {
                    list->tail = current;
                }
                
                swapped = true;
                previous = next;
            } else {
                previous = current;
                current = current->next;
            }
        }
    } while (swapped);
    
    pthread_mutex_unlock(&list->mutex);
    return CPM_SUCCESS;
}
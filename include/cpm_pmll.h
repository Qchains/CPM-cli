/*
 * File: include/cpm_pmll.h
 * Description: PMLL (Persistent Memory Lock Library) hardened queue API.
 * Provides serialized operations to prevent race conditions in shared resources.
 * Author: Dr. Q Josef Kurk Edwards
 */

#ifndef CPM_PMLL_H
#define CPM_PMLL_H

#include <stdbool.h>
#include <pthread.h>
#include "cpm_promise.h"

// --- PMLL Hardened Resource Queue ---
typedef struct {
    const char* resource_id;
    Promise* operation_queue_promise;
    PMLL_Lock queue_lock;
    PMEMContextHandle pmem_queue_ctx;
} PMLL_HardenedResourceQueue;

// --- PMLL Queue Operations ---
PMLL_HardenedResourceQueue* pmll_queue_create(const char* resource_id, bool persistent_queue);
Promise* pmll_execute_hardened_operation(
    PMLL_HardenedResourceQueue* hq,
    on_fulfilled_callback operation_fn,
    on_rejected_callback error_fn,
    void* op_user_data);
void pmll_queue_free(PMLL_HardenedResourceQueue* hq);

// --- Global PMLL Management ---
bool pmll_init_global_system(void);
void pmll_shutdown_global_system(void);
PMLL_HardenedResourceQueue* pmll_get_default_file_queue(void);

#endif // CPM_PMLL_H
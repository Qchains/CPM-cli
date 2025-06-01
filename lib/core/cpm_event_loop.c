/*
 * File: lib/core/cpm_event_loop.c
 * Description: Event loop implementation for CPM promises
 * Author: Dr. Q Josef Kurk Edwards
 */

#include "cpm_promise.h"
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>

// --- Event Loop Structures ---

typedef struct MicrotaskItem {
    void (*task)(void* data);
    void* data;
    struct MicrotaskItem* next;
} MicrotaskItem;

typedef struct {
    MicrotaskItem* head;
    MicrotaskItem* tail;
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    bool should_exit;
    size_t pending_tasks;
} MicrotaskQueue;

typedef struct {
    pthread_t thread_id;
    bool is_running;
    bool is_initialized;
    MicrotaskQueue microtask_queue;
    
    // Statistics
    uint64_t tasks_executed;
    uint64_t total_iterations;
    struct timeval start_time;
} EventLoop;

// --- Global Event Loop Instance ---
static EventLoop g_event_loop = {0};

// --- Microtask Queue Management ---

static void init_microtask_queue(MicrotaskQueue* queue) {
    queue->head = NULL;
    queue->tail = NULL;
    queue->should_exit = false;
    queue->pending_tasks = 0;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->condition, NULL);
}

static void cleanup_microtask_queue(MicrotaskQueue* queue) {
    pthread_mutex_lock(&queue->mutex);
    
    // Clear remaining tasks
    MicrotaskItem* current = queue->head;
    while (current) {
        MicrotaskItem* next = current->next;
        free(current);
        current = next;
    }
    
    queue->head = NULL;
    queue->tail = NULL;
    queue->pending_tasks = 0;
    
    pthread_mutex_unlock(&queue->mutex);
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->condition);
}

static bool enqueue_microtask(MicrotaskQueue* queue, void (*task)(void* data), void* data) {
    MicrotaskItem* item = malloc(sizeof(MicrotaskItem));
    if (!item) {
        return false;
    }
    
    item->task = task;
    item->data = data;
    item->next = NULL;
    
    pthread_mutex_lock(&queue->mutex);
    
    if (queue->tail) {
        queue->tail->next = item;
    } else {
        queue->head = item;
    }
    queue->tail = item;
    queue->pending_tasks++;
    
    pthread_cond_signal(&queue->condition);
    pthread_mutex_unlock(&queue->mutex);
    
    return true;
}

static MicrotaskItem* dequeue_microtask(MicrotaskQueue* queue) {
    pthread_mutex_lock(&queue->mutex);
    
    MicrotaskItem* item = queue->head;
    if (item) {
        queue->head = item->next;
        if (!queue->head) {
            queue->tail = NULL;
        }
        queue->pending_tasks--;
    }
    
    pthread_mutex_unlock(&queue->mutex);
    return item;
}

static size_t get_pending_tasks(MicrotaskQueue* queue) {
    pthread_mutex_lock(&queue->mutex);
    size_t count = queue->pending_tasks;
    pthread_mutex_unlock(&queue->mutex);
    return count;
}

// --- Event Loop Implementation ---

static void* event_loop_thread(void* arg) {
    EventLoop* loop = (EventLoop*)arg;
    
    while (!loop->microtask_queue.should_exit) {
        bool has_work = false;
        
        // Process all available microtasks
        MicrotaskItem* task;
        while ((task = dequeue_microtask(&loop->microtask_queue)) != NULL) {
            if (task->task) {
                task->task(task->data);
                loop->tasks_executed++;
            }
            free(task);
            has_work = true;
        }
        
        loop->total_iterations++;
        
        if (!has_work) {
            // No work available, wait for signal or timeout
            pthread_mutex_lock(&loop->microtask_queue.mutex);
            
            struct timespec timeout;
            clock_gettime(CLOCK_REALTIME, &timeout);
            timeout.tv_nsec += 1000000;  // 1ms timeout
            if (timeout.tv_nsec >= 1000000000) {
                timeout.tv_sec += 1;
                timeout.tv_nsec -= 1000000000;
            }
            
            pthread_cond_timedwait(&loop->microtask_queue.condition, 
                                  &loop->microtask_queue.mutex, 
                                  &timeout);
            
            pthread_mutex_unlock(&loop->microtask_queue.mutex);
        }
    }
    
    return NULL;
}

// --- Public Event Loop API ---

void cpm_event_loop_init(void) {
    if (g_event_loop.is_initialized) {
        return;
    }
    
    init_microtask_queue(&g_event_loop.microtask_queue);
    g_event_loop.is_running = false;
    g_event_loop.is_initialized = true;
    g_event_loop.tasks_executed = 0;
    g_event_loop.total_iterations = 0;
    
    gettimeofday(&g_event_loop.start_time, NULL);
    
    // Start event loop thread
    if (pthread_create(&g_event_loop.thread_id, NULL, event_loop_thread, &g_event_loop) == 0) {
        g_event_loop.is_running = true;
    }
}

bool cpm_event_loop_run_once(void) {
    if (!g_event_loop.is_initialized) {
        return false;
    }
    
    // Check if there are pending tasks
    size_t pending = get_pending_tasks(&g_event_loop.microtask_queue);
    
    if (pending > 0) {
        // Process one microtask
        MicrotaskItem* task = dequeue_microtask(&g_event_loop.microtask_queue);
        if (task) {
            if (task->task) {
                task->task(task->data);
                g_event_loop.tasks_executed++;
            }
            free(task);
            return true;
        }
    }
    
    return pending > 0;
}

void cpm_event_loop_run_until_complete(void) {
    if (!g_event_loop.is_initialized) {
        return;
    }
    
    // Process all pending microtasks
    while (get_pending_tasks(&g_event_loop.microtask_queue) > 0) {
        cpm_event_loop_run_once();
        usleep(100);  // Small delay to prevent busy waiting
    }
}

void cpm_event_loop_enqueue_microtask(void (*task)(void* data), void* data) {
    if (!g_event_loop.is_initialized || !task) {
        return;
    }
    
    enqueue_microtask(&g_event_loop.microtask_queue, task, data);
}

void cpm_event_loop_shutdown(void) {
    if (!g_event_loop.is_initialized) {
        return;
    }
    
    // Signal shutdown
    pthread_mutex_lock(&g_event_loop.microtask_queue.mutex);
    g_event_loop.microtask_queue.should_exit = true;
    pthread_cond_broadcast(&g_event_loop.microtask_queue.condition);
    pthread_mutex_unlock(&g_event_loop.microtask_queue.mutex);
    
    // Wait for thread to finish
    if (g_event_loop.is_running) {
        pthread_join(g_event_loop.thread_id, NULL);
        g_event_loop.is_running = false;
    }
    
    // Cleanup
    cleanup_microtask_queue(&g_event_loop.microtask_queue);
    g_event_loop.is_initialized = false;
}

// --- Event Loop Statistics ---

void cpm_event_loop_get_stats(uint64_t* tasks_executed, 
                             uint64_t* total_iterations,
                             uint64_t* pending_tasks,
                             double* uptime_seconds) {
    if (tasks_executed) {
        *tasks_executed = g_event_loop.tasks_executed;
    }
    
    if (total_iterations) {
        *total_iterations = g_event_loop.total_iterations;
    }
    
    if (pending_tasks) {
        *pending_tasks = get_pending_tasks(&g_event_loop.microtask_queue);
    }
    
    if (uptime_seconds && g_event_loop.is_initialized) {
        struct timeval current_time;
        gettimeofday(&current_time, NULL);
        *uptime_seconds = (current_time.tv_sec - g_event_loop.start_time.tv_sec) +
                         (current_time.tv_usec - g_event_loop.start_time.tv_usec) / 1000000.0;
    }
}
#include "thread_manager.h"
#include "adc_simulator.h"
#include "tcp_client.h"
#include "timestamp.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#endif

/* Thread context */
typedef struct {
    thread_manager_t *manager;
    thread_type_t type;
    thread_worker_t worker;
    void *user_data;
} thread_context_t;

/* Thread manager internal structure */
struct thread_manager {
    buffer_manager_t *buffer_manager;
    system_stats_t stats;
    
    /* Thread related */
    volatile thread_state_t states[THREAD_COUNT];
    volatile bool running;
    volatile bool paused;
    
#ifdef _WIN32
    HANDLE threads[THREAD_COUNT];
    CRITICAL_SECTION stats_lock;
#else
    pthread_t threads[THREAD_COUNT];
    pthread_mutex_t stats_lock;
#endif
    
    /* Thread statistics */
    thread_stats_t thread_stats[THREAD_COUNT];
    thread_context_t contexts[THREAD_COUNT];
    
    /* External components */
    adc_simulator_t *adc;
    tcp_client_t *tcp_client;
};

/* Set thread priority */
static void set_thread_priority(thread_type_t type) {
#ifdef _WIN32
    int priority = THREAD_PRIORITY_NORMAL;
    switch (type) {
        case THREAD_CAPTURE:  priority = THREAD_PRIORITY_CAPTURE; break;
        case THREAD_PROCESS:  priority = THREAD_PRIORITY_PROCESS; break;
        case THREAD_NETWORK:  priority = THREAD_PRIORITY_NETWORK; break;
        default: break;
    }
    SetThreadPriority(GetCurrentThread(), priority);
#else
    int policy = SCHED_FIFO;
    struct sched_param param;
    
    switch (type) {
        case THREAD_CAPTURE:  param.sched_priority = 99; break;
        case THREAD_PROCESS:  param.sched_priority = 80; break;
        case THREAD_NETWORK:  param.sched_priority = 70; break;
        default: param.sched_priority = 50;
    }
    pthread_setschedparam(pthread_self(), policy, &param);
#endif
}

/* Thread wrapper function */
#ifdef _WIN32
static DWORD WINAPI thread_wrapper(LPVOID param) {
#else
static void* thread_wrapper(void *param) {
#endif
    thread_context_t *ctx = (thread_context_t *)param;
    thread_manager_t *tm = ctx->manager;
    thread_type_t type = ctx->type;
    
    /* Set thread priority */
    set_thread_priority(type);
    
    LOG_INFO("Thread %d started", type);
    
    while (tm->running) {
        if (tm->paused) {
            sleep_ms(1);
            continue;
        }
        
        if (tm->states[type] == THREAD_STATE_RUNNING) {
            uint64_t start_cycles = get_cpu_cycles();
            
            /* Execute worker function */
            if (ctx->worker) {
                ctx->worker(tm, type, ctx->user_data);
            }
            
            /* Update statistics */
            uint64_t end_cycles = get_cpu_cycles();
            double cycle_time = (double)(end_cycles - start_cycles);
            
#ifdef _WIN32
            EnterCriticalSection(&tm->stats_lock);
#else
            pthread_mutex_lock(&tm->stats_lock);
#endif
            tm->thread_stats[type].loop_count++;
            tm->thread_stats[type].avg_cycle_time_us = 
                (tm->thread_stats[type].avg_cycle_time_us * 0.9) + (cycle_time * 0.1);
            if (cycle_time > tm->thread_stats[type].max_cycle_time_us) {
                tm->thread_stats[type].max_cycle_time_us = cycle_time;
            }
#ifdef _WIN32
            LeaveCriticalSection(&tm->stats_lock);
#else
            pthread_mutex_unlock(&tm->stats_lock);
#endif
        }
    }
    
    tm->states[type] = THREAD_STATE_STOPPED;
    LOG_INFO("Thread %d stopped", type);
    
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

/* Create thread manager */
thread_manager_t* thread_manager_create(void) {
    thread_manager_t *tm = (thread_manager_t *)calloc(1, sizeof(thread_manager_t));
    if (!tm) {
        LOG_ERROR("Failed to allocate thread manager memory");
        return NULL;
    }
    
    tm->running = false;
    tm->paused = false;
    tm->buffer_manager = NULL;
    tm->adc = NULL;
    tm->tcp_client = NULL;
    
    for (int i = 0; i < THREAD_COUNT; i++) {
        tm->states[i] = THREAD_STATE_IDLE;
        memset(&tm->thread_stats[i], 0, sizeof(thread_stats_t));
        tm->contexts[i].manager = tm;
        tm->contexts[i].type = i;
        tm->contexts[i].worker = NULL;
        tm->contexts[i].user_data = NULL;
    }
    
#ifdef _WIN32
    InitializeCriticalSection(&tm->stats_lock);
#else
    pthread_mutex_init(&tm->stats_lock, NULL);
#endif
    
    LOG_INFO("Thread manager created");
    return tm;
}

/* Destroy thread manager */
void thread_manager_destroy(thread_manager_t *tm) {
    if (!tm) return;
    
    if (tm->running) {
        thread_manager_stop(tm);
    }
    
#ifdef _WIN32
    DeleteCriticalSection(&tm->stats_lock);
#else
    pthread_mutex_destroy(&tm->stats_lock);
#endif
    
    free(tm);
    LOG_INFO("Thread manager destroyed");
}

/* Initialize thread manager */
adc_error_t thread_manager_init(thread_manager_t *tm, buffer_manager_t *bm) {
    if (!tm || !bm) return ADC_ERROR_INVALID_PARAM;
    
    tm->buffer_manager = bm;
    
    /* Set default worker functions */
    tm->contexts[THREAD_CAPTURE].worker = capture_thread_worker;
    tm->contexts[THREAD_PROCESS].worker = process_thread_worker;
    tm->contexts[THREAD_NETWORK].worker = network_thread_worker;
    
    LOG_INFO("Thread manager initialized");
    return ADC_OK;
}

/* Start all threads */
adc_error_t thread_manager_start(thread_manager_t *tm) {
    if (!tm) return ADC_ERROR_INVALID_PARAM;
    if (tm->running) return ADC_OK;
    
    tm->running = true;
    tm->paused = false;
    
    for (int i = 0; i < THREAD_COUNT; i++) {
        tm->states[i] = THREAD_STATE_RUNNING;
        
#ifdef _WIN32
        tm->threads[i] = CreateThread(NULL, 0, thread_wrapper, &tm->contexts[i], 0, NULL);
        if (!tm->threads[i]) {
            LOG_ERROR("Failed to create thread %d", i);
            tm->running = false;
            return ADC_ERROR_THREAD;
        }
#else
        if (pthread_create(&tm->threads[i], NULL, thread_wrapper, &tm->contexts[i]) != 0) {
            LOG_ERROR("Failed to create thread %d", i);
            tm->running = false;
            return ADC_ERROR_THREAD;
        }
#endif
    }
    
    LOG_INFO("All threads started");
    return ADC_OK;
}

/* Stop all threads */
adc_error_t thread_manager_stop(thread_manager_t *tm) {
    if (!tm) return ADC_ERROR_INVALID_PARAM;
    if (!tm->running) return ADC_OK;
    
    tm->running = false;
    
    for (int i = 0; i < THREAD_COUNT; i++) {
#ifdef _WIN32
        if (tm->threads[i]) {
            WaitForSingleObject(tm->threads[i], INFINITE);
            CloseHandle(tm->threads[i]);
            tm->threads[i] = NULL;
        }
#else
        pthread_join(tm->threads[i], NULL);
#endif
        tm->states[i] = THREAD_STATE_STOPPED;
    }
    
    LOG_INFO("All threads stopped");
    return ADC_OK;
}

/* Pause all threads */
adc_error_t thread_manager_pause(thread_manager_t *tm) {
    if (!tm) return ADC_ERROR_INVALID_PARAM;
    tm->paused = true;
    LOG_INFO("All threads paused");
    return ADC_OK;
}

/* Resume all threads */
adc_error_t thread_manager_resume(thread_manager_t *tm) {
    if (!tm) return ADC_ERROR_INVALID_PARAM;
    tm->paused = false;
    LOG_INFO("All threads resumed");
    return ADC_OK;
}

/* Check if running */
bool thread_manager_is_running(const thread_manager_t *tm) {
    return tm ? tm->running : false;
}

/* Get thread state */
thread_state_t thread_manager_get_state(const thread_manager_t *tm, thread_type_t type) {
    if (!tm || type < 0 || type >= THREAD_COUNT) return THREAD_STATE_IDLE;
    return tm->states[type];
}

/* Get thread statistics */
adc_error_t thread_manager_get_stats(const thread_manager_t *tm, thread_type_t type, thread_stats_t *stats) {
    if (!tm || !stats || type < 0 || type >= THREAD_COUNT) return ADC_ERROR_INVALID_PARAM;
    
#ifdef _WIN32
    EnterCriticalSection(&tm->stats_lock);
#else
    thread_manager_t *tm_mutable = (thread_manager_t *)tm;
    pthread_mutex_lock(&tm_mutable->stats_lock);
#endif
    memcpy(stats, &tm->thread_stats[type], sizeof(thread_stats_t));
#ifdef _WIN32
    LeaveCriticalSection(&tm->stats_lock);
#else
    pthread_mutex_unlock(&tm_mutable->stats_lock);
#endif
    
    return ADC_OK;
}

/* Set worker function */
adc_error_t thread_manager_set_worker(thread_manager_t *tm, thread_type_t type, 
                                       thread_worker_t worker, void *user_data) {
    if (!tm || type < 0 || type >= THREAD_COUNT) return ADC_ERROR_INVALID_PARAM;
    
    tm->contexts[type].worker = worker;
    tm->contexts[type].user_data = user_data;
    
    return ADC_OK;
}

/* Get buffer manager */
buffer_manager_t* thread_manager_get_buffer_manager(thread_manager_t *tm) {
    return tm ? tm->buffer_manager : NULL;
}

/* Get system statistics */
system_stats_t* thread_manager_get_system_stats(thread_manager_t *tm) {
    return tm ? &tm->stats : NULL;
}

/* Set ADC simulator */
void thread_manager_set_adc(thread_manager_t *tm, adc_simulator_t *adc) {
    if (tm) tm->adc = adc;
}

/* Set TCP client */
void thread_manager_set_tcp_client(thread_manager_t *tm, tcp_client_t *client) {
    if (tm) tm->tcp_client = client;
}

/* Get ADC simulator */
adc_simulator_t* thread_manager_get_adc(thread_manager_t *tm) {
    return tm ? tm->adc : NULL;
}

/* Get TCP client */
tcp_client_t* thread_manager_get_tcp_client(thread_manager_t *tm) {
    return tm ? tm->tcp_client : NULL;
}

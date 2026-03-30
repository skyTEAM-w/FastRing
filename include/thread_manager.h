/**
 * @file thread_manager.h
 * @brief Multi-thread manager for ADC data acquisition system
 *
 * This module manages the three main threads in the ADC data pipeline:
 * capture thread, processing thread, and network transmission thread.
 * It provides thread lifecycle management, synchronization, and statistics.
 *
 * @author WuChengpei_Sky
 * @version 1.0.0
 * @date 2026-03-22
 */

#ifndef THREAD_MANAGER_H
#define THREAD_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "adc_types.h"
#include "ring_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Thread type enumeration
 */
typedef enum {
    THREAD_CAPTURE = 0,     /**< Data capture/acquisition thread */
    THREAD_PROCESS,         /**< Data processing thread */
    THREAD_NETWORK,         /**< Network transmission thread */
    THREAD_COUNT            /**< Total number of thread types */
} thread_type_t;

/**
 * @brief Thread state enumeration
 */
typedef enum {
    THREAD_STATE_IDLE = 0,    /**< Thread is created but not started */
    THREAD_STATE_RUNNING,     /**< Thread is actively running */
    THREAD_STATE_PAUSED,      /**< Thread is paused */
    THREAD_STATE_ERROR,       /**< Thread encountered an error */
    THREAD_STATE_STOPPED      /**< Thread has stopped execution */
} thread_state_t;

/**
 * @brief Thread statistics structure
 *
 * Tracks performance metrics for each thread.
 */
typedef struct {
    uint64_t loop_count;        /**< Number of loop iterations */
    uint64_t item_count;        /**< Number of items processed */
    uint64_t error_count;       /**< Number of errors encountered */
    double   avg_cycle_time_us; /**< Average cycle time (microseconds) */
    double   max_cycle_time_us; /**< Maximum cycle time (microseconds) */
} thread_stats_t;

/**
 * @brief Opaque thread manager handle
 */
typedef struct thread_manager thread_manager_t;

/**
 * @brief Thread worker function prototype
 * @param tm Pointer to the thread manager
 * @param type Thread type identifier
 * @param user_data User-provided data pointer
 */
typedef void (*thread_worker_t)(thread_manager_t *tm, thread_type_t type, void *user_data);

/**
 * @brief Create a new thread manager instance
 * @return Pointer to newly created thread manager, or NULL on failure
 * @note Caller is responsible for calling thread_manager_destroy() to free resources
 */
thread_manager_t* thread_manager_create(void);

/**
 * @brief Destroy a thread manager and free all resources
 * @param tm Pointer to the thread manager to destroy
 * @note This will stop all threads before destroying
 */
void thread_manager_destroy(thread_manager_t *tm);

/**
 * @brief Initialize the thread manager with buffer manager
 * @param tm Pointer to the thread manager
 * @param bm Pointer to the buffer manager for inter-thread communication
 * @return ADC_OK on success, error code on failure
 */
adc_error_t thread_manager_init(thread_manager_t *tm, buffer_manager_t *bm);

/**
 * @brief Start all managed threads
 * @param tm Pointer to the thread manager
 * @return ADC_OK on success, error code on failure
 */
adc_error_t thread_manager_start(thread_manager_t *tm);

/**
 * @brief Stop all managed threads
 * @param tm Pointer to the thread manager
 * @return ADC_OK on success, error code on failure
 */
adc_error_t thread_manager_stop(thread_manager_t *tm);

/**
 * @brief Pause all managed threads
 * @param tm Pointer to the thread manager
 * @return ADC_OK on success, error code on failure
 */
adc_error_t thread_manager_pause(thread_manager_t *tm);

/**
 * @brief Resume all paused threads
 * @param tm Pointer to the thread manager
 * @return ADC_OK on success, error code on failure
 */
adc_error_t thread_manager_resume(thread_manager_t *tm);

/**
 * @brief Check if the thread manager is running
 * @param tm Pointer to the thread manager
 * @return true if any thread is running, false otherwise
 */
bool thread_manager_is_running(const thread_manager_t *tm);

/**
 * @brief Get the state of a specific thread
 * @param tm Pointer to the thread manager
 * @param type Thread type to query
 * @return Current state of the specified thread
 */
thread_state_t thread_manager_get_state(const thread_manager_t *tm, thread_type_t type);

/**
 * @brief Get statistics for a specific thread
 * @param tm Pointer to the thread manager
 * @param type Thread type to query
 * @param stats Pointer to store the statistics
 * @return ADC_OK on success, error code on failure
 */
adc_error_t thread_manager_get_stats(const thread_manager_t *tm, thread_type_t type, thread_stats_t *stats);

/**
 * @brief Set a custom worker function for a specific thread
 * @param tm Pointer to the thread manager
 * @param type Thread type to set worker for
 * @param worker Worker function pointer
 * @param user_data User data to pass to the worker
 * @return ADC_OK on success, error code on failure
 */
adc_error_t thread_manager_set_worker(thread_manager_t *tm, thread_type_t type, thread_worker_t worker, void *user_data);

/**
 * @brief Get the buffer manager associated with this thread manager
 * @param tm Pointer to the thread manager
 * @return Pointer to the buffer manager
 */
buffer_manager_t* thread_manager_get_buffer_manager(thread_manager_t *tm);

/**
 * @brief Get system-wide statistics
 * @param tm Pointer to the thread manager
 * @return Pointer to the system statistics structure
 */
system_stats_t* thread_manager_get_system_stats(thread_manager_t *tm);

/**
 * @brief Default capture thread worker implementation
 * @param tm Pointer to the thread manager
 * @param type Thread type (should be THREAD_CAPTURE)
 * @param user_data User data (unused)
 */
void capture_thread_worker(thread_manager_t *tm, thread_type_t type, void *user_data);

/**
 * @brief Default processing thread worker implementation
 * @param tm Pointer to the thread manager
 * @param type Thread type (should be THREAD_PROCESS)
 * @param user_data User data (unused)
 */
void process_thread_worker(thread_manager_t *tm, thread_type_t type, void *user_data);

/**
 * @brief Default network transmission thread worker implementation
 * @param tm Pointer to the thread manager
 * @param type Thread type (should be THREAD_NETWORK)
 * @param user_data User data (unused)
 */
void network_thread_worker(thread_manager_t *tm, thread_type_t type, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* THREAD_MANAGER_H */

/**
 * @file ring_buffer.h
 * @brief Lock-free ring buffer implementation for ADC data
 *
 * This module provides a high-performance, lock-free ring buffer
 * implementation for inter-thread communication in the ADC
 * data acquisition pipeline.
 *
 * @author ADC Data Acquisition Team
 * @version 1.0.0
 * @date 2026-03-22
 */

#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "adc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Lock-free ring buffer structure
 *
 * Implements a single-producer single-consumer (SPSC) ring buffer
 * using atomic operations for thread-safe data exchange.
 */
typedef struct {
    volatile uint32_t head;         /**< Write pointer (producer side) */
    volatile uint32_t tail;         /**< Read pointer (consumer side) */
    uint32_t capacity;              /**< Buffer capacity */
    uint32_t mask;                  /**< Bit mask for fast modulo operation */
    adc_sample_t *buffer;           /**< Data buffer array */
    volatile uint64_t dropped;      /**< Dropped sample counter */
} ring_buffer_t;

/**
 * @brief Buffer manager for three-stage pipeline
 *
 * Manages the three ring buffers used in the data acquisition pipeline:
 * capture -> process -> network
 */
typedef struct {
    ring_buffer_t *capture_to_process;   /**< Buffer: capture to processing stage */
    ring_buffer_t *process_to_network;   /**< Buffer: processing to network stage */
    ring_buffer_t *network_pool;         /**< Buffer pool for network transmission */
} buffer_manager_t;

/**
 * @brief Create a new ring buffer
 * @param capacity Number of samples the buffer can hold (will be rounded up to power of 2)
 * @return Pointer to newly created ring buffer, or NULL on failure
 * @note Caller is responsible for calling ring_buffer_destroy() to free resources
 */
ring_buffer_t* ring_buffer_create(uint32_t capacity);

/**
 * @brief Destroy a ring buffer and free all resources
 * @param rb Pointer to the ring buffer to destroy
 */
void ring_buffer_destroy(ring_buffer_t *rb);

/**
 * @brief Push a single sample to the ring buffer (producer side)
 * @param rb Pointer to the ring buffer
 * @param sample Pointer to the sample to push
 * @return true if sample was pushed successfully, false if buffer is full
 * @note This function is thread-safe for single producer
 */
bool ring_buffer_push(ring_buffer_t *rb, const adc_sample_t *sample);

/**
 * @brief Pop a single sample from the ring buffer (consumer side)
 * @param rb Pointer to the ring buffer
 * @param sample Pointer to store the popped sample
 * @return true if sample was popped successfully, false if buffer is empty
 * @note This function is thread-safe for single consumer
 */
bool ring_buffer_pop(ring_buffer_t *rb, adc_sample_t *sample);

/**
 * @brief Push multiple samples to the ring buffer in a batch operation
 * @param rb Pointer to the ring buffer
 * @param samples Pointer to the array of samples to push
 * @param count Number of samples in the array
 * @param pushed Pointer to store the actual number of samples pushed
 * @return true if all samples were pushed, false if partial or none
 * @note This function is thread-safe for single producer
 */
bool ring_buffer_push_batch(ring_buffer_t *rb, const adc_sample_t *samples, uint32_t count, uint32_t *pushed);

/**
 * @brief Pop multiple samples from the ring buffer in a batch operation
 * @param rb Pointer to the ring buffer
 * @param samples Pointer to the array to store popped samples
 * @param count Maximum number of samples to pop
 * @param popped Pointer to store the actual number of samples popped
 * @return true if at least one sample was popped, false if buffer is empty
 * @note This function is thread-safe for single consumer
 */
bool ring_buffer_pop_batch(ring_buffer_t *rb, adc_sample_t *samples, uint32_t count, uint32_t *popped);

/**
 * @brief Get the number of samples currently in the buffer
 * @param rb Pointer to the ring buffer
 * @return Number of samples available to read
 */
uint32_t ring_buffer_count(const ring_buffer_t *rb);

/**
 * @brief Check if the ring buffer is empty
 * @param rb Pointer to the ring buffer
 * @return true if buffer is empty, false otherwise
 */
bool ring_buffer_is_empty(const ring_buffer_t *rb);

/**
 * @brief Check if the ring buffer is full
 * @param rb Pointer to the ring buffer
 * @return true if buffer is full, false otherwise
 */
bool ring_buffer_is_full(const ring_buffer_t *rb);

/**
 * @brief Clear all samples from the ring buffer
 * @param rb Pointer to the ring buffer to clear
 * @note This function is not thread-safe; ensure no producer/consumer access during clear
 */
void ring_buffer_clear(ring_buffer_t *rb);

/**
 * @brief Get the number of samples that can be pushed without blocking
 * @param rb Pointer to the ring buffer
 * @return Number of available slots for writing
 */
uint32_t ring_buffer_available(const ring_buffer_t *rb);

/**
 * @brief Create a buffer manager for the three-stage pipeline
 * @return Pointer to newly created buffer manager, or NULL on failure
 * @note Caller is responsible for calling buffer_manager_destroy() to free resources
 */
buffer_manager_t* buffer_manager_create(void);

/**
 * @brief Destroy a buffer manager and all associated buffers
 * @param bm Pointer to the buffer manager to destroy
 */
void buffer_manager_destroy(buffer_manager_t *bm);

/**
 * @brief Initialize the buffer manager with default settings
 * @param bm Pointer to the buffer manager to initialize
 * @return ADC_OK on success, error code on failure
 */
adc_error_t buffer_manager_init(buffer_manager_t *bm);

#ifdef __cplusplus
}
#endif

#endif /* RING_BUFFER_H */

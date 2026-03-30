/**
 * @file timestamp.h
 * @brief High-precision timestamp and timing utilities
 *
 * This module provides high-resolution timing functions for
 * precise timing control required by the 40kHz ADC sampling system.
 * It uses Windows QueryPerformanceCounter for microsecond precision.
 *
 * @author WuChengpei_Sky
 * @version 1.0.0
 * @date 2026-03-22
 */

#ifndef TIMESTAMP_H
#define TIMESTAMP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the timestamp module
 *
 * Must be called before using any timing functions.
 * On Windows, this initializes the high-resolution performance counter.
 */
void timestamp_init(void);

/**
 * @brief Get current timestamp in microseconds
 * @return Current timestamp value in microseconds
 */
uint64_t get_timestamp_us(void);

/**
 * @brief Get current timestamp in milliseconds
 * @return Current timestamp value in milliseconds
 */
uint64_t get_timestamp_ms(void);

/**
 * @brief Get high-precision timestamp in nanoseconds
 * @return Current timestamp value in nanoseconds
 * @note May have reduced precision on some systems
 */
uint64_t get_timestamp_ns(void);

/**
 * @brief Calculate elapsed time between two timestamps
 * @param start Start timestamp
 * @param end End timestamp
 * @return Elapsed time in microseconds
 * @note Handles timer wrap-around correctly
 */
static inline uint64_t elapsed_us(uint64_t start, uint64_t end) {
    return (end >= start) ? (end - start) : (0xFFFFFFFFFFFFFFFFULL - start + end + 1);
}

/**
 * @brief Sleep for specified number of microseconds
 * @param us Number of microseconds to sleep
 * @note Uses high-resolution timer for precise delays
 */
void sleep_us(uint64_t us);

/**
 * @brief Sleep for specified number of milliseconds
 * @param ms Number of milliseconds to sleep
 */
void sleep_ms(uint32_t ms);

/**
 * @brief Get CPU cycle count (converted to microseconds)
 * @return CPU cycle count converted to microseconds
 * @note Useful for profiling and performance measurements
 */
uint64_t get_cpu_cycles(void);

#ifdef __cplusplus
}
#endif

#endif /* TIMESTAMP_H */

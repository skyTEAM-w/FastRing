/**
 * @file adc_types.h
 * @brief ADC data acquisition system type definitions
 *
 * This file defines the core data types, constants, and structures
 * for the high-performance ADC data acquisition and transmission system.
 *
 * @author ADC Data Acquisition Team
 * @version 1.0.0
 * @date 2026-03-22
 */

#ifndef ADC_TYPES_H
#define ADC_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief ADC configuration parameters
 * @{
 */
#define ADC_SAMPLE_RATE     40000       /**< Sampling rate: 40kHz */
#define ADC_RESOLUTION      32          /**< Resolution: 32-bit */
#define ADC_CHANNEL_COUNT   1           /**< Number of channels: single channel */
#define ADC_SAMPLE_SIZE     4           /**< Sample size: 4 bytes (32-bit) */
/** @} */

/**
 * @brief Buffer configuration
 * @{
 */
#define RING_BUFFER_SIZE    16384       /**< Ring buffer size (number of samples) */
#define BUFFER_COUNT        3           /**< Number of buffers (three-stage pipeline) */
/** @} */

/**
 * @brief Network configuration
 * @{
 */
#define DEFAULT_SERVER_IP   "127.0.0.1"
#define DEFAULT_SERVER_PORT 8080
#define TCP_SEND_BUFFER_SIZE (1024 * 64) /**< 64KB send buffer */
/** @} */

/**
 * @brief Thread priority configuration (Windows)
 * @{
 */
#define THREAD_PRIORITY_CAPTURE    THREAD_PRIORITY_TIME_CRITICAL
#define THREAD_PRIORITY_PROCESS    THREAD_PRIORITY_HIGHEST
#define THREAD_PRIORITY_NETWORK    THREAD_PRIORITY_ABOVE_NORMAL
/** @} */

/**
 * @brief Error code definitions for ADC operations
 */
typedef enum {
    ADC_OK = 0,                /**< Operation successful */
    ADC_ERROR_INIT = -1,        /**< Initialization error */
    ADC_ERROR_BUFFER = -2,      /**< Buffer operation error */
    ADC_ERROR_NETWORK = -3,     /**< Network operation error */
    ADC_ERROR_THREAD = -4,      /**< Thread operation error */
    ADC_ERROR_TIMEOUT = -5,    /**< Operation timeout */
    ADC_ERROR_INVALID_PARAM = -6 /**< Invalid parameter */
} adc_error_t;

/**
 * @brief ADC sample data structure
 *
 * Represents a single ADC sample with timestamp, value, and metadata.
 */
typedef struct {
    uint32_t timestamp;     /**< Timestamp (microseconds) */
    int32_t  value;         /**< ADC sample value (32-bit signed) */
    uint32_t sequence;     /**< Sequence number */
    uint16_t channel;       /**< Channel number */
    uint16_t flags;        /**< Flags */
} adc_sample_t;

/**
 * @brief Data block structure for batch transmission
 *
 * Used for efficient network transmission of multiple samples.
 */
typedef struct {
    uint32_t    magic;          /**< Magic number: 0xADC0DA7A */
    uint32_t    version;        /**< Protocol version */
    uint32_t    sequence;       /**< Block sequence number */
    uint32_t    sample_count;   /**< Number of samples */
    uint64_t    timestamp;      /**< Block timestamp */
    adc_sample_t samples[];     /**< Sample array (flexible array member) */
} data_block_t;

#define DATA_BLOCK_MAGIC    0xADC0DA7A /**< Data block magic number */
#define DATA_BLOCK_VERSION  1          /**< Data block protocol version */
#define SAMPLES_PER_BLOCK   256         /**< Number of samples per block */

/**
 * @brief System statistics information
 *
 * Tracks overall system performance metrics.
 */
typedef struct {
    uint64_t total_samples;     /**< Total number of samples acquired */
    uint64_t dropped_samples;   /**< Number of dropped samples */
    uint64_t sent_bytes;        /**< Total bytes sent */
    uint64_t send_errors;       /**< Number of send errors */
    double   avg_sample_rate;   /**< Average sampling rate (Hz) */
    double   avg_latency_ms;    /**< Average latency (milliseconds) */
    double   cpu_usage;         /**< CPU usage percentage */
} system_stats_t;

#ifdef __cplusplus
}
#endif

#endif /* ADC_TYPES_H */

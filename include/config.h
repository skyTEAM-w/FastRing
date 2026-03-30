/**
 * @file config.h
 * @brief System configuration management module
 *
 * This module provides centralized configuration management for the
 * ADC data acquisition and transmission system.
 *
 * @author WuChengpei_Sky
 * @version 1.0.0
 * @date 2026-03-26
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief ADC configuration structure
 */
typedef struct {
    uint32_t sample_rate;       /**< Sampling rate in Hz (default: 40000) */
    uint32_t resolution;        /**< ADC resolution in bits (default: 32) */
    uint32_t channel_count;     /**< Number of channels (default: 1) */
    double   signal_freq;       /**< Simulated signal frequency in Hz */
    double   noise_level;       /**< Noise level (0.0-1.0) */
    bool     enable_trigger;    /**< Enable trigger mode */
} adc_config_t;

/**
 * @brief Buffer configuration structure
 */
typedef struct {
    uint32_t ring_buffer_size;  /**< Ring buffer size in samples */
    uint32_t buffer_count;      /**< Number of pipeline buffers */
    uint32_t batch_size;        /**< Batch operation size */
} buffer_config_t;

/**
 * @brief Network configuration structure
 */
typedef struct {
    char     server_ip[16];     /**< Server IP address */
    uint16_t server_port;       /**< Server port */
    uint32_t send_timeout_ms;   /**< Send timeout in milliseconds */
    uint32_t recv_timeout_ms;   /**< Receive timeout in milliseconds */
    uint32_t buffer_size;       /**< Socket buffer size */
    bool     keep_alive;        /**< Enable TCP keep-alive */
    bool     no_delay;          /**< Enable TCP_NODELAY */
} network_config_t;

/**
 * @brief Thread configuration structure
 */
typedef struct {
    int      capture_priority;  /**< Capture thread priority */
    int      process_priority;  /**< Processing thread priority */
    int      network_priority;  /**< Network thread priority */
    uint32_t stack_size;        /**< Thread stack size (0 = default) */
    bool     set_affinity;      /**< Set CPU affinity */
    int      cpu_core;          /**< Target CPU core (if affinity enabled) */
} thread_config_t;

/**
 * @brief Log configuration structure
 */
typedef struct {
    char     log_file[256];     /**< Log file path (empty = stdout) */
    int      log_level;         /**< Log level (0=DEBUG, 1=INFO, 2=WARN, 3=ERROR) */
    bool     log_to_file;       /**< Enable file logging */
    bool     log_to_console;    /**< Enable console logging */
    uint32_t max_file_size;     /**< Max log file size in bytes */
    uint32_t max_backup_files;  /**< Max number of backup log files */
} log_config_t;

/**
 * @brief System-wide configuration structure
 */
typedef struct {
    adc_config_t     adc;       /**< ADC configuration */
    buffer_config_t  buffer;    /**< Buffer configuration */
    network_config_t network;   /**< Network configuration */
    thread_config_t  thread;    /**< Thread configuration */
    log_config_t     log;       /**< Logging configuration */
    int              run_time_seconds; /**< Run time (-1 = infinite) */
} system_config_t;

/**
 * @brief Get default system configuration
 * @param config Pointer to configuration structure to fill
 */
void config_get_default(system_config_t *config);

/**
 * @brief Get default ADC configuration
 * @param config Pointer to ADC configuration structure
 */
void config_get_adc_default(adc_config_t *config);

/**
 * @brief Get default buffer configuration
 * @param config Pointer to buffer configuration structure
 */
void config_get_buffer_default(buffer_config_t *config);

/**
 * @brief Get default network configuration
 * @param config Pointer to network configuration structure
 */
void config_get_network_default(network_config_t *config);

/**
 * @brief Get default thread configuration
 * @param config Pointer to thread configuration structure
 */
void config_get_thread_default(thread_config_t *config);

/**
 * @brief Get default log configuration
 * @param config Pointer to log configuration structure
 */
void config_get_log_default(log_config_t *config);

/**
 * @brief Validate configuration parameters
 * @param config Pointer to configuration structure
 * @return true if configuration is valid, false otherwise
 */
bool config_validate(const system_config_t *config);

/**
 * @brief Print configuration to stdout
 * @param config Pointer to configuration structure
 */
void config_print(const system_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_H */

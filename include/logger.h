/**
 * @file logger.h
 * @brief Logging utility for system diagnostics
 *
 * This module provides a thread-safe logging facility with
 * multiple log levels and optional file output. It supports
 * debug, info, warning, error, and fatal log levels.
 *
 * @author WuChengpei_Sky
 * @version 1.0.0
 * @date 2026-03-22
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Log level enumeration
 */
typedef enum {
    LOG_LEVEL_DEBUG = 0, /**< Debug level: detailed diagnostic information */
    LOG_LEVEL_INFO,       /**< Info level: general operational information */
    LOG_LEVEL_WARN,       /**< Warning level: potential issues */
    LOG_LEVEL_ERROR,      /**< Error level: errors that need attention */
    LOG_LEVEL_FATAL,      /**< Fatal level: critical errors causing system failure */
    LOG_LEVEL_COUNT       /**< Total number of log levels */
} log_level_t;

/**
 * @brief Initialize the logger
 * @param filename Path to the log file (NULL for console-only logging)
 * @param level Minimum log level to output
 * @return 0 on success, -1 on failure
 */
int logger_init(const char *filename, log_level_t level);

/**
 * @brief Close the logger and flush all output
 */
void logger_close(void);

/**
 * @brief Log a message at the specified level
 * @param level Log level for this message
 * @param file Source file name (usually __FILE__)
 * @param line Source line number (usually __LINE__)
 * @param fmt Printf-style format string
 * @note This function is usually called via the LOG_* macros
 */
void log_message(log_level_t level, const char *file, int line, const char *fmt, ...);

/**
 * @brief Log a debug message
 * @param fmt Printf-style format string
 * @param ... Arguments for format string
 */
#define LOG_DEBUG(fmt, ...) log_message(LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * @brief Log an info message
 * @param fmt Printf-style format string
 * @param ... Arguments for format string
 */
#define LOG_INFO(fmt, ...)  log_message(LOG_LEVEL_INFO,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * @brief Log a warning message
 * @param fmt Printf-style format string
 * @param ... Arguments for format string
 */
#define LOG_WARN(fmt, ...)  log_message(LOG_LEVEL_WARN,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * @brief Log an error message
 * @param fmt Printf-style format string
 * @param ... Arguments for format string
 */
#define LOG_ERROR(fmt, ...) log_message(LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * @brief Log a fatal error message
 * @param fmt Printf-style format string
 * @param ... Arguments for format string
 */
#define LOG_FATAL(fmt, ...) log_message(LOG_LEVEL_FATAL, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * @brief Set the minimum log level
 * @param level New minimum log level
 */
void logger_set_level(log_level_t level);

/**
 * @brief Get the current minimum log level
 * @return Current minimum log level
 */
log_level_t logger_get_level(void);

#ifdef __cplusplus
}
#endif

#endif /* LOGGER_H */

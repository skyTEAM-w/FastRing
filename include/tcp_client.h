/**
 * @file tcp_client.h
 * @brief TCP client implementation for network data transmission
 *
 * This module provides a TCP client implementation for reliable
 * network transmission of ADC data to the server. It supports
 * connection management, data sending/receiving, and keep-alive.
 *
 * @author WuChengpei_Sky
 * @version 1.0.0
 * @date 2026-03-22
 */

#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "adc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque TCP client handle
 */
typedef struct tcp_client tcp_client_t;

/**
 * @brief TCP client configuration structure
 */
typedef struct {
    char     server_ip[16];     /**< Server IP address (dot-decimal format) */
    uint16_t server_port;       /**< Server port number */
    uint32_t send_timeout_ms;   /**< Send timeout in milliseconds */
    uint32_t recv_timeout_ms;   /**< Receive timeout in milliseconds */
    uint32_t buffer_size;       /**< Internal buffer size */
    bool     keep_alive;        /**< Enable TCP keep-alive */
} tcp_config_t;

/**
 * @brief Create a new TCP client instance
 * @param config Pointer to the configuration structure
 * @return Pointer to newly created TCP client, or NULL on failure
 * @note Caller is responsible for calling tcp_client_destroy() to free resources
 */
tcp_client_t* tcp_client_create(const tcp_config_t *config);

/**
 * @brief Destroy a TCP client and free all resources
 * @param client Pointer to the TCP client to destroy
 * @note This will disconnect if connected before destroying
 */
void tcp_client_destroy(tcp_client_t *client);

/**
 * @brief Connect to the TCP server
 * @param client Pointer to the TCP client
 * @return ADC_OK on success, error code on failure
 */
adc_error_t tcp_client_connect(tcp_client_t *client);

/**
 * @brief Disconnect from the TCP server
 * @param client Pointer to the TCP client
 * @return ADC_OK on success, error code on failure
 */
adc_error_t tcp_client_disconnect(tcp_client_t *client);

/**
 * @brief Check if the TCP client is connected
 * @param client Pointer to the TCP client
 * @return true if connected, false otherwise
 */
bool tcp_client_is_connected(const tcp_client_t *client);

/**
 * @brief Send data to the server (may send partial)
 * @param client Pointer to the TCP client
 * @param data Pointer to the data to send
 * @param len Length of the data in bytes
 * @param sent Pointer to store the number of bytes actually sent
 * @return ADC_OK on success, error code on failure
 */
adc_error_t tcp_client_send(tcp_client_t *client, const void *data, size_t len, size_t *sent);

/**
 * @brief Send all data to the server (will retry until all sent or error)
 * @param client Pointer to the TCP client
 * @param data Pointer to the data to send
 * @param len Length of the data in bytes
 * @return ADC_OK on success, error code on failure
 */
adc_error_t tcp_client_send_all(tcp_client_t *client, const void *data, size_t len);

/**
 * @brief Receive data from the server
 * @param client Pointer to the TCP client
 * @param buffer Pointer to the buffer to store received data
 * @param len Maximum length of the buffer
 * @param received Pointer to store the number of bytes received
 * @return ADC_OK on success, error code on failure
 */
adc_error_t tcp_client_receive(tcp_client_t *client, void *buffer, size_t len, size_t *received);

/**
 * @brief Send a data block to the server
 * @param client Pointer to the TCP client
 * @param block Pointer to the data block to send
 * @return ADC_OK on success, error code on failure
 */
adc_error_t tcp_client_send_data_block(tcp_client_t *client, const data_block_t *block);

/**
 * @brief Get the default TCP client configuration
 * @param config Pointer to the configuration structure to fill with defaults
 */
void tcp_config_get_default(tcp_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* TCP_CLIENT_H */

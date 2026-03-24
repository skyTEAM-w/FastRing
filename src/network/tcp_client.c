#include "tcp_client.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#endif

/* TCP client internal structure */
struct tcp_client {
    tcp_config_t config;
    int socket_fd;
    volatile bool connected;
    uint8_t *send_buffer;
    uint32_t block_sequence;
};

/* Default configuration */
void tcp_config_get_default(tcp_config_t *config) {
    if (config) {
        strncpy(config->server_ip, DEFAULT_SERVER_IP, sizeof(config->server_ip) - 1);
        config->server_ip[sizeof(config->server_ip) - 1] = '\0';
        config->server_port = DEFAULT_SERVER_PORT;
        config->send_timeout_ms = 5000;
        config->recv_timeout_ms = 5000;
        config->buffer_size = TCP_SEND_BUFFER_SIZE;
        config->keep_alive = true;
    }
}

/* Create TCP client */
tcp_client_t* tcp_client_create(const tcp_config_t *config) {
    tcp_client_t *client = (tcp_client_t *)calloc(1, sizeof(tcp_client_t));
    if (!client) {
        LOG_ERROR("Failed to allocate TCP client memory");
        return NULL;
    }
    
    if (config) {
        memcpy(&client->config, config, sizeof(tcp_config_t));
    } else {
        tcp_config_get_default(&client->config);
    }
    
    client->socket_fd = -1;
    client->connected = false;
    client->block_sequence = 0;
    
    client->send_buffer = (uint8_t *)malloc(client->config.buffer_size);
    if (!client->send_buffer) {
        LOG_ERROR("Failed to allocate send buffer");
        free(client);
        return NULL;
    }
    
#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        LOG_ERROR("WSAStartup failed");
        free(client->send_buffer);
        free(client);
        return NULL;
    }
#endif
    
    LOG_INFO("TCP client created, target: %s:%d", 
             client->config.server_ip, client->config.server_port);
    return client;
}

/* Destroy TCP client */
void tcp_client_destroy(tcp_client_t *client) {
    if (!client) return;
    
    if (client->connected) {
        tcp_client_disconnect(client);
    }
    
    if (client->send_buffer) {
        free(client->send_buffer);
        client->send_buffer = NULL;
    }
    
#ifdef _WIN32
    WSACleanup();
#endif
    
    free(client);
    LOG_INFO("TCP client destroyed");
}

/* Connect to server */
adc_error_t tcp_client_connect(tcp_client_t *client) {
    if (!client) return ADC_ERROR_INVALID_PARAM;
    if (client->connected) return ADC_OK;
    
    /* Create socket */
    client->socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client->socket_fd < 0) {
        LOG_ERROR("Failed to create socket");
        return ADC_ERROR_NETWORK;
    }
    
    /* Set send buffer size */
    int buf_size = client->config.buffer_size;
    setsockopt(client->socket_fd, SOL_SOCKET, SO_SNDBUF, 
               (const char *)&buf_size, sizeof(buf_size));
    
    /* Set TCP_NODELAY (disable Nagle algorithm for lower latency) */
    int nodelay = 1;
    setsockopt(client->socket_fd, IPPROTO_TCP, TCP_NODELAY, 
               (const char *)&nodelay, sizeof(nodelay));
    
    /* Set keep alive */
    if (client->config.keep_alive) {
        int keepalive = 1;
        setsockopt(client->socket_fd, SOL_SOCKET, SO_KEEPALIVE, 
                   (const char *)&keepalive, sizeof(keepalive));
    }
    
    /* Set timeout */
#ifdef _WIN32
    DWORD timeout = client->config.send_timeout_ms;
    setsockopt(client->socket_fd, SOL_SOCKET, SO_SNDTIMEO, 
               (const char *)&timeout, sizeof(timeout));
    timeout = client->config.recv_timeout_ms;
    setsockopt(client->socket_fd, SOL_SOCKET, SO_RCVTIMEO, 
               (const char *)&timeout, sizeof(timeout));
#else
    struct timeval tv;
    tv.tv_sec = client->config.send_timeout_ms / 1000;
    tv.tv_usec = (client->config.send_timeout_ms % 1000) * 1000;
    setsockopt(client->socket_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    tv.tv_sec = client->config.recv_timeout_ms / 1000;
    tv.tv_usec = (client->config.recv_timeout_ms % 1000) * 1000;
    setsockopt(client->socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
    
    /* Connect to server */
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(client->config.server_port);
    
    if (inet_pton(AF_INET, client->config.server_ip, &server_addr.sin_addr) <= 0) {
        LOG_ERROR("Invalid IP address: %s", client->config.server_ip);
#ifdef _WIN32
        closesocket(client->socket_fd);
#else
        close(client->socket_fd);
#endif
        client->socket_fd = -1;
        return ADC_ERROR_NETWORK;
    }
    
    if (connect(client->socket_fd, (struct sockaddr *)&server_addr, 
                sizeof(server_addr)) < 0) {
        LOG_ERROR("Failed to connect to server: %s:%d", client->config.server_ip, client->config.server_port);
#ifdef _WIN32
        closesocket(client->socket_fd);
#else
        close(client->socket_fd);
#endif
        client->socket_fd = -1;
        return ADC_ERROR_NETWORK;
    }
    
    client->connected = true;
    LOG_INFO("Connected to server: %s:%d", client->config.server_ip, client->config.server_port);
    return ADC_OK;
}

/* Disconnect */
adc_error_t tcp_client_disconnect(tcp_client_t *client) {
    if (!client) return ADC_ERROR_INVALID_PARAM;
    if (!client->connected) return ADC_OK;
    
#ifdef _WIN32
    closesocket(client->socket_fd);
#else
    close(client->socket_fd);
#endif
    
    client->socket_fd = -1;
    client->connected = false;
    
    LOG_INFO("Disconnected from server");
    return ADC_OK;
}

/* Check connection status */
bool tcp_client_is_connected(const tcp_client_t *client) {
    return client ? client->connected : false;
}

/* Send data */
adc_error_t tcp_client_send(tcp_client_t *client, const void *data, size_t len, size_t *sent) {
    if (!client || !data || len == 0) return ADC_ERROR_INVALID_PARAM;
    if (!client->connected) return ADC_ERROR_NETWORK;
    
    int result = send(client->socket_fd, (const char *)data, (int)len, 0);
    
    if (result < 0) {
#ifdef _WIN32
        int error = WSAGetLastError();
        if (error == WSAEWOULDBLOCK || error == WSAETIMEDOUT) {
            if (sent) *sent = 0;
            return ADC_ERROR_TIMEOUT;
        }
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            if (sent) *sent = 0;
            return ADC_ERROR_TIMEOUT;
        }
#endif
        LOG_ERROR("Failed to send data");
        client->connected = false;
        return ADC_ERROR_NETWORK;
    }
    
    if (sent) *sent = (size_t)result;
    return ADC_OK;
}

/* Send all data (blocking until complete) */
adc_error_t tcp_client_send_all(tcp_client_t *client, const void *data, size_t len) {
    if (!client || !data || len == 0) return ADC_ERROR_INVALID_PARAM;
    if (!client->connected) return ADC_ERROR_NETWORK;
    
    const uint8_t *ptr = (const uint8_t *)data;
    size_t remaining = len;
    
    while (remaining > 0) {
        size_t sent = 0;
        adc_error_t err = tcp_client_send(client, ptr, remaining, &sent);
        if (err != ADC_OK) {
            return err;
        }
        ptr += sent;
        remaining -= sent;
    }
    
    return ADC_OK;
}

/* Receive data */
adc_error_t tcp_client_receive(tcp_client_t *client, void *buffer, size_t len, size_t *received) {
    if (!client || !buffer || len == 0) return ADC_ERROR_INVALID_PARAM;
    if (!client->connected) return ADC_ERROR_NETWORK;
    
    int result = recv(client->socket_fd, (char *)buffer, (int)len, 0);
    
    if (result < 0) {
#ifdef _WIN32
        int error = WSAGetLastError();
        if (error == WSAEWOULDBLOCK || error == WSAETIMEDOUT) {
            if (received) *received = 0;
            return ADC_ERROR_TIMEOUT;
        }
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            if (received) *received = 0;
            return ADC_ERROR_TIMEOUT;
        }
#endif
        LOG_ERROR("Failed to receive data");
        client->connected = false;
        return ADC_ERROR_NETWORK;
    }
    
    if (result == 0) {
        /* Connection closed */
        LOG_WARN("Server closed connection");
        client->connected = false;
        return ADC_ERROR_NETWORK;
    }
    
    if (received) *received = (size_t)result;
    return ADC_OK;
}

/* Send data block */
adc_error_t tcp_client_send_data_block(tcp_client_t *client, const data_block_t *block) {
    if (!client || !block) return ADC_ERROR_INVALID_PARAM;
    
    size_t block_size = sizeof(data_block_t) + 
                        block->sample_count * sizeof(adc_sample_t);
    
    return tcp_client_send_all(client, block, block_size);
}

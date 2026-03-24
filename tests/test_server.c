/* Test Server - Simulate Host PC for Data Reception */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "adc_types.h"
#include "timestamp.h"
#include "logger.h"

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

#define SERVER_PORT 8080
#define BUFFER_SIZE (1024 * 1024)  /* 1MB receive buffer */

static volatile int g_running = 1;

/* Signal handler */
static void signal_handler(int sig) {
    (void)sig;
    printf("\nReceived termination signal, shutting down...\n");
    g_running = 0;
}

/* Print data block info */
static void print_data_block(const data_block_t *block) {
    if (block->magic != DATA_BLOCK_MAGIC) {
        printf("Warning: Magic number mismatch! Expected: 0x%08X, Actual: 0x%08X\n", 
               DATA_BLOCK_MAGIC, block->magic);
        return;
    }
    
    printf("Data Block #%u: samples=%u, timestamp=%llu\n",
           block->sequence,
           block->sample_count,
           (unsigned long long)block->timestamp);
    
    /* Print first few samples */
    if (block->sample_count > 0) {
        printf("  First 3 samples:\n");
        for (uint32_t i = 0; i < block->sample_count && i < 3; i++) {
            printf("    [%u] seq=%u, ts=%u, val=%d\n",
                   i,
                   block->samples[i].sequence,
                   block->samples[i].timestamp,
                   block->samples[i].value);
        }
    }
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    /* Initialize logging */
    logger_init(NULL, LOG_LEVEL_INFO);
    
    /* Set signal handler */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    LOG_INFO("========================================");
    LOG_INFO("Test Server Started (Simulating Host PC)");
    LOG_INFO("Listening port: %d", SERVER_PORT);
    LOG_INFO("========================================");
    
#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        LOG_FATAL("WSAStartup failed");
        return 1;
    }
#endif
    
    /* Create listening socket */
    int listen_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_fd < 0) {
        LOG_FATAL("Failed to create socket");
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    
    /* Allow address reuse */
    int reuse = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, 
               (const char *)&reuse, sizeof(reuse));
    
    /* Bind address */
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);
    
    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        LOG_FATAL("Failed to bind address");
#ifdef _WIN32
        closesocket(listen_fd);
        WSACleanup();
#else
        close(listen_fd);
#endif
        return 1;
    }
    
    /* Start listening */
    if (listen(listen_fd, 5) < 0) {
        LOG_FATAL("Failed to listen");
#ifdef _WIN32
        closesocket(listen_fd);
        WSACleanup();
#else
        close(listen_fd);
#endif
        return 1;
    }
    
    LOG_INFO("Waiting for client connection...");
    
    /* Accept connection */
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
    
    if (client_fd < 0) {
        LOG_FATAL("Failed to accept connection");
#ifdef _WIN32
        closesocket(listen_fd);
        WSACleanup();
#else
        close(listen_fd);
#endif
        return 1;
    }
    
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
    LOG_INFO("Client connected: %s:%d", client_ip, ntohs(client_addr.sin_port));
    
    /* Receive data */
    uint8_t *buffer = (uint8_t *)malloc(BUFFER_SIZE);
    if (!buffer) {
        LOG_FATAL("Failed to allocate buffer");
#ifdef _WIN32
        closesocket(client_fd);
        closesocket(listen_fd);
        WSACleanup();
#else
        close(client_fd);
        close(listen_fd);
#endif
        return 1;
    }
    
    uint64_t total_bytes = 0;
    uint64_t total_blocks = 0;
    uint64_t start_time = get_timestamp_ms();
    uint64_t last_stats_time = start_time;
    
    while (g_running) {
        int received = recv(client_fd, (char *)buffer, BUFFER_SIZE, 0);
        
        if (received < 0) {
#ifdef _WIN32
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK || error == WSAETIMEDOUT) {
                sleep_ms(1);
                continue;
            }
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                sleep_ms(1);
                continue;
            }
#endif
            LOG_ERROR("Failed to receive data");
            break;
        }
        
        if (received == 0) {
            LOG_INFO("Client disconnected");
            break;
        }
        
        total_bytes += received;
        
        /* Parse data blocks */
        size_t offset = 0;
        while (offset + sizeof(data_block_t) <= (size_t)received) {
            data_block_t *block = (data_block_t *)(buffer + offset);
            
            if (block->magic != DATA_BLOCK_MAGIC) {
                /* Find next possible magic position */
                offset += 4;
                continue;
            }
            
            size_t block_size = sizeof(data_block_t) + 
                               block->sample_count * sizeof(adc_sample_t);
            
            if (offset + block_size > (size_t)received) {
                break;  /* Incomplete block, wait for more data */
            }
            
            total_blocks++;
            
            /* Print every 100 blocks */
            if (total_blocks % 100 == 0) {
                print_data_block(block);
            }
            
            offset += block_size;
        }
        
        /* Print stats every 5 seconds */
        uint64_t current_time = get_timestamp_ms();
        if ((current_time - last_stats_time) >= 5000) {
            double elapsed_sec = (current_time - start_time) / 1000.0;
            double rate_kbps = (total_bytes * 8.0 / 1000.0) / elapsed_sec;
            
            printf("\n========== Receive Statistics ==========\n");
            printf("Total bytes: %llu\n", (unsigned long long)total_bytes);
            printf("Total blocks: %llu\n", (unsigned long long)total_blocks);
            printf("Running time: %.1f sec\n", elapsed_sec);
            printf("Average rate: %.2f Kbps (%.2f KB/s)\n", rate_kbps, rate_kbps / 8.0);
            printf("========================================\n");
            
            last_stats_time = current_time;
        }
    }
    
    /* Final statistics */
    uint64_t end_time = get_timestamp_ms();
    double total_elapsed = (end_time - start_time) / 1000.0;
    
    printf("\n========== Final Statistics ==========\n");
    printf("Total bytes: %llu\n", (unsigned long long)total_bytes);
    printf("Total blocks: %llu\n", (unsigned long long)total_blocks);
    printf("Running time: %.1f sec\n", total_elapsed);
    if (total_elapsed > 0) {
        double avg_rate_kbps = (total_bytes * 8.0 / 1000.0) / total_elapsed;
        printf("Average rate: %.2f Kbps (%.2f KB/s)\n", avg_rate_kbps, avg_rate_kbps / 8.0);
    }
    printf("========================================\n");
    
    /* Cleanup */
    free(buffer);
#ifdef _WIN32
    closesocket(client_fd);
    closesocket(listen_fd);
    WSACleanup();
#else
    close(client_fd);
    close(listen_fd);
#endif
    
    LOG_INFO("Test server shutdown");
    logger_close();
    
    return 0;
}

/**
 * @file config.c
 * @brief System configuration management implementation
 */

#include <stdio.h>
#include <string.h>
#include "config.h"
#include "adc_types.h"
#include "logger.h"

#ifdef _WIN32
#include <windows.h>
#endif

void config_get_default(system_config_t *config) {
    if (!config) return;

    config_get_adc_default(&config->adc);
    config_get_buffer_default(&config->buffer);
    config_get_network_default(&config->network);
    config_get_thread_default(&config->thread);
    config_get_log_default(&config->log);
    config->run_time_seconds = -1;
}

void config_get_adc_default(adc_config_t *config) {
    if (!config) return;

    config->sample_rate = ADC_SAMPLE_RATE;
    config->resolution = ADC_RESOLUTION;
    config->channel_count = ADC_CHANNEL_COUNT;
    config->signal_freq = 1000.0;
    config->noise_level = 0.01;
    config->enable_trigger = false;
}

void config_get_buffer_default(buffer_config_t *config) {
    if (!config) return;

    config->ring_buffer_size = RING_BUFFER_SIZE;
    config->buffer_count = BUFFER_COUNT;
    config->batch_size = 256;
}

void config_get_network_default(network_config_t *config) {
    if (!config) return;

    strncpy(config->server_ip, DEFAULT_SERVER_IP, sizeof(config->server_ip) - 1);
    config->server_ip[sizeof(config->server_ip) - 1] = '\0';
    config->server_port = DEFAULT_SERVER_PORT;
    config->send_timeout_ms = 5000;
    config->recv_timeout_ms = 5000;
    config->buffer_size = TCP_SEND_BUFFER_SIZE;
    config->keep_alive = true;
    config->no_delay = true;
}

void config_get_thread_default(thread_config_t *config) {
    if (!config) return;

#ifdef _WIN32
    config->capture_priority = THREAD_PRIORITY_CAPTURE;
    config->process_priority = THREAD_PRIORITY_PROCESS;
    config->network_priority = THREAD_PRIORITY_NETWORK;
#else
    config->capture_priority = 99;
    config->process_priority = 80;
    config->network_priority = 70;
#endif
    config->stack_size = 0;
    config->set_affinity = false;
    config->cpu_core = 0;
}

void config_get_log_default(log_config_t *config) {
    if (!config) return;

    config->log_file[0] = '\0';
    config->log_level = LOG_LEVEL_INFO;
    config->log_to_file = false;
    config->log_to_console = true;
    config->max_file_size = 10 * 1024 * 1024;
    config->max_backup_files = 3;
}

bool config_validate(const system_config_t *config) {
    if (!config) return false;

    if (config->adc.sample_rate == 0 || config->adc.sample_rate > 1000000) {
        LOG_ERROR("Invalid sample rate: %u", config->adc.sample_rate);
        return false;
    }

    if (config->adc.resolution == 0 || config->adc.resolution > 32) {
        LOG_ERROR("Invalid ADC resolution: %u", config->adc.resolution);
        return false;
    }

    if (config->buffer.ring_buffer_size < 256 || config->buffer.ring_buffer_size > 1048576) {
        LOG_ERROR("Invalid buffer size: %u", config->buffer.ring_buffer_size);
        return false;
    }

    if (config->network.server_port == 0) {
        LOG_ERROR("Invalid port: %u", config->network.server_port);
        return false;
    }

    return true;
}

void config_print(const system_config_t *config) {
    if (!config) return;

    printf("========== System Configuration ==========\n");
    printf("ADC Configuration:\n");
    printf("  Sample Rate: %u Hz\n", config->adc.sample_rate);
    printf("  Resolution: %u bits\n", config->adc.resolution);
    printf("  Channel Count: %u\n", config->adc.channel_count);
    printf("  Signal Frequency: %.2f Hz\n", config->adc.signal_freq);
    printf("  Noise Level: %.4f\n", config->adc.noise_level);

    printf("Buffer Configuration:\n");
    printf("  Ring Buffer Size: %u\n", config->buffer.ring_buffer_size);
    printf("  Buffer Count: %u\n", config->buffer.buffer_count);
    printf("  Batch Size: %u\n", config->buffer.batch_size);

    printf("Network Configuration:\n");
    printf("  Server IP: %s\n", config->network.server_ip);
    printf("  Server Port: %u\n", config->network.server_port);
    printf("  Send Timeout: %u ms\n", config->network.send_timeout_ms);
    printf("  Keep Alive: %s\n", config->network.keep_alive ? "Yes" : "No");
    printf("  No Delay: %s\n", config->network.no_delay ? "Yes" : "No");

    printf("Thread Configuration:\n");
    printf("  Capture Priority: %d\n", config->thread.capture_priority);
    printf("  Process Priority: %d\n", config->thread.process_priority);
    printf("  Network Priority: %d\n", config->thread.network_priority);

    printf("Log Configuration:\n");
    printf("  Level: %d\n", config->log.log_level);
    printf("  Console: %s\n", config->log.log_to_console ? "Yes" : "No");
    printf("  File: %s\n", config->log.log_to_file ? "Yes" : "No");

    printf("Run Time: %d seconds\n", config->run_time_seconds);
    printf("==========================================\n");
}

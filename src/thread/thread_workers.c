#include "thread_manager.h"
#include "adc_simulator.h"
#include "tcp_client.h"
#include "ring_buffer.h"
#include "timestamp.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <stdatomic.h>
#endif

/* External declarations for thread_manager.c */
extern adc_simulator_t* thread_manager_get_adc(thread_manager_t *tm);
extern tcp_client_t* thread_manager_get_tcp_client(thread_manager_t *tm);
extern void thread_manager_set_adc(thread_manager_t *tm, adc_simulator_t *adc);
extern void thread_manager_set_tcp_client(thread_manager_t *tm, tcp_client_t *client);

/* ADC callback data structure */
typedef struct {
    ring_buffer_t *buffer;
    system_stats_t *stats;
} adc_callback_data_t;

/* ADC callback function */
static void adc_sample_callback(const adc_sample_t *sample, void *user_data) {
    adc_callback_data_t *cb_data = (adc_callback_data_t *)user_data;
    if (!cb_data || !cb_data->buffer) return;
    
    /* Push to capture buffer */
    if (!ring_buffer_push(cb_data->buffer, sample)) {
        /* Update drop statistics */
        if (cb_data->stats) {
            cb_data->stats->dropped_samples++;
        }
    } else {
        if (cb_data->stats) {
            cb_data->stats->total_samples++;
        }
    }
}

/* Capture thread worker function */
void capture_thread_worker(thread_manager_t *tm, thread_type_t type, void *user_data) {
    (void)type;
    (void)user_data;
    
    buffer_manager_t *bm = thread_manager_get_buffer_manager(tm);
    system_stats_t *stats = thread_manager_get_system_stats(tm);
    
    if (!bm || !bm->capture_to_process) {
        sleep_ms(1);
        return;
    }
    
    /* Get or create ADC simulator */
    adc_simulator_t *adc = thread_manager_get_adc(tm);
    if (!adc) {
        adc_config_t config;
        adc_config_get_default(&config);
        adc = adc_simulator_create(&config);
        if (!adc) {
            LOG_ERROR("Failed to create ADC simulator");
            sleep_ms(10);
            return;
        }
        thread_manager_set_adc(tm, adc);
        
        /* Set callback */
        static adc_callback_data_t cb_data;
        cb_data.buffer = bm->capture_to_process;
        cb_data.stats = stats;
        adc_simulator_set_callback(adc, adc_sample_callback, &cb_data);
        
        /* Start ADC */
        adc_simulator_start(adc);
    }
    
    /* ADC runs in independent thread, just check status here */
    if (!adc_simulator_is_running(adc)) {
        LOG_WARN("ADC capture stopped, attempting restart");
        adc_simulator_start(adc);
    }
    
    /* Short sleep to yield CPU */
    sleep_us(100);
}

/* Process thread worker function */
void process_thread_worker(thread_manager_t *tm, thread_type_t type, void *user_data) {
    (void)type;
    (void)user_data;
    
    buffer_manager_t *bm = thread_manager_get_buffer_manager(tm);
    
    if (!bm || !bm->capture_to_process || !bm->process_to_network) {
        sleep_ms(1);
        return;
    }
    
    /* Batch process samples - increased batch size for better throughput */
    adc_sample_t samples[256];
    uint32_t popped = 0;
    
    /* Read from capture buffer */
    if (ring_buffer_pop_batch(bm->capture_to_process, samples, 256, &popped)) {
        uint32_t pushed = 0;
        
        /* Push to process buffer (data processing can be added here) */
        ring_buffer_push_batch(bm->process_to_network, samples, popped, &pushed);
        
        /* Record processing statistics */
        if (pushed < popped) {
            system_stats_t *stats = thread_manager_get_system_stats(tm);
            if (stats) {
                stats->dropped_samples += (popped - pushed);
            }
        }
    } else {
        /* No data, short sleep */
        sleep_us(10);
    }
}

/* Pre-allocated block pool for network transmission */
#define BLOCK_POOL_SIZE 16
static data_block_t *g_block_pool[BLOCK_POOL_SIZE];
static volatile uint32_t g_block_pool_ready = 0;

/* Initialize block pool */
static void init_block_pool(void) {
    if (g_block_pool_ready) return;
    
    for (int i = 0; i < BLOCK_POOL_SIZE; i++) {
        /* Allocate maximum size block */
        size_t max_block_size = sizeof(data_block_t) + SAMPLES_PER_BLOCK * sizeof(adc_sample_t);
        g_block_pool[i] = (data_block_t *)malloc(max_block_size);
    }
    g_block_pool_ready = 1;
}

/* Get block from pool */
static data_block_t* get_block_from_pool(void) {
    if (!g_block_pool_ready) {
        init_block_pool();
    }
    
    /* Simple round-robin allocation */
#ifdef _WIN32
    static volatile LONG pool_index = 0;
    uint32_t idx = (uint32_t)InterlockedIncrement(&pool_index) % BLOCK_POOL_SIZE;
#else
    static atomic_uint pool_index = 0;
    uint32_t idx = atomic_fetch_add(&pool_index, 1) % BLOCK_POOL_SIZE;
#endif
    return g_block_pool[idx];
}

/* Network send thread worker function */
void network_thread_worker(thread_manager_t *tm, thread_type_t type, void *user_data) {
    (void)type;
    (void)user_data;
    
    buffer_manager_t *bm = thread_manager_get_buffer_manager(tm);
    system_stats_t *stats = thread_manager_get_system_stats(tm);
    
    if (!bm || !bm->process_to_network) {
        sleep_ms(1);
        return;
    }
    
    /* Get or create TCP client */
    tcp_client_t *client = thread_manager_get_tcp_client(tm);
    if (!client) {
        tcp_config_t config;
        tcp_config_get_default(&config);
        client = tcp_client_create(&config);
        if (!client) {
            LOG_ERROR("Failed to create TCP client");
            sleep_ms(100);
            return;
        }
        thread_manager_set_tcp_client(tm, client);
    }
    
    /* Connect to server */
    if (!tcp_client_is_connected(client)) {
        LOG_INFO("Attempting to connect to server...");
        if (tcp_client_connect(client) != ADC_OK) {
            sleep_ms(1000);
            return;
        }
    }
    
    /* Collect samples and send - increased batch size */
    adc_sample_t samples[SAMPLES_PER_BLOCK];
    uint32_t popped = 0;
    
    if (ring_buffer_pop_batch(bm->process_to_network, samples, SAMPLES_PER_BLOCK, &popped)) {
        if (popped > 0) {
            /* Use pre-allocated block from pool */
            data_block_t *block = get_block_from_pool();
            if (!block) {
                LOG_ERROR("Failed to get block from pool");
                sleep_ms(1);
                return;
            }
            
            block->magic = DATA_BLOCK_MAGIC;
            block->version = DATA_BLOCK_VERSION;
            block->sequence = 0;  /* Managed internally by tcp_client */
            block->sample_count = popped;
            block->timestamp = get_timestamp_us();
            memcpy(block->samples, samples, popped * sizeof(adc_sample_t));
            
            /* Send data block */
            adc_error_t err = tcp_client_send_data_block(client, block);
            if (err == ADC_OK) {
                if (stats) {
                    stats->sent_bytes += sizeof(data_block_t) + popped * sizeof(adc_sample_t);
                }
            } else {
                if (stats) {
                    stats->send_errors++;
                }
                LOG_WARN("Failed to send data block: %d", err);
            }
            
            /* Block is reused from pool, no need to free */
        }
    } else {
        /* No data, short sleep */
        sleep_us(50);
    }
}

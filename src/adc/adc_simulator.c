#include "adc_simulator.h"
#include "timestamp.h"
#include "logger.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <limits.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

#define PI 3.14159265358979323846

/* ADC simulator internal structure */
struct adc_simulator {
    adc_config_t config;
    volatile bool running;
    uint32_t sequence;
    uint64_t start_time;
    
    adc_callback_t callback;
    void *callback_data;
    
#ifdef _WIN32
    HANDLE thread;
    CRITICAL_SECTION lock;
#else
    pthread_t thread;
    pthread_mutex_t lock;
#endif
};

/* Generate simulated ADC sample */
static int32_t generate_sample(adc_simulator_t *adc, uint64_t timestamp_us) {
    double t = (double)timestamp_us / 1000000.0;
    double signal = sin(2.0 * PI * adc->config.signal_freq * t);
    
    /* Add noise */
    if (adc->config.noise_level > 0.0) {
        double noise = ((double)rand() / RAND_MAX - 0.5) * 2.0 * adc->config.noise_level;
        signal += noise;
    }
    
    /* Clamp range and convert to 32-bit integer */
    if (signal > 1.0) signal = 1.0;
    if (signal < -1.0) signal = -1.0;
    
    /* 32-bit signed integer range: -2^31 to 2^31-1 */
    int32_t max_val = INT32_MAX;
    if (adc->config.resolution > 0 && adc->config.resolution < 32) {
        max_val = (int32_t)((1U << (adc->config.resolution - 1)) - 1U);
    }
    return (int32_t)(signal * max_val);
}

/* Capture thread function */
#ifdef _WIN32
static DWORD WINAPI capture_thread(LPVOID param) {
#else
static void* capture_thread(void *param) {
#endif
    adc_simulator_t *adc = (adc_simulator_t *)param;
    uint64_t sample_interval_us = 1000000ULL / adc->config.sample_rate;
    uint64_t next_sample_time = get_timestamp_us();
    
    LOG_INFO("ADC capture thread started, sample rate: %d Hz, interval: %llu us", 
             adc->config.sample_rate, sample_interval_us);
    
    while (adc->running) {
        uint64_t current_time = get_timestamp_us();
        
        /* Check if it's time to sample */
        if (current_time >= next_sample_time) {
            adc_sample_t sample;
            adc_callback_t callback = NULL;
            void *callback_data = NULL;
            sample.timestamp = (uint32_t)(current_time - adc->start_time);
            sample.value = generate_sample(adc, current_time);
            sample.sequence = adc->sequence++;
            sample.channel = 0;
            sample.flags = 0;

#ifdef _WIN32
            EnterCriticalSection(&adc->lock);
#else
            pthread_mutex_lock(&adc->lock);
#endif
            callback = adc->callback;
            callback_data = adc->callback_data;
#ifdef _WIN32
            LeaveCriticalSection(&adc->lock);
#else
            pthread_mutex_unlock(&adc->lock);
#endif

            if (callback) {
                callback(&sample, callback_data);
            }
            
            /* Calculate next sample time */
            next_sample_time += sample_interval_us;
            
            /* Adjust if falling behind */
            if (next_sample_time < current_time) {
                next_sample_time = current_time + sample_interval_us;
            }
        }
        
        /* Short sleep to reduce CPU usage */
        sleep_us(1);
    }
    
    LOG_INFO("ADC capture thread stopped");
    
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

/* Default configuration */
void adc_config_get_default(adc_config_t *config) {
    if (config) {
        config->sample_rate = ADC_SAMPLE_RATE;
        config->resolution = ADC_RESOLUTION;
        config->channel_count = ADC_CHANNEL_COUNT;
        config->signal_freq = 1000.0;  /* 1kHz test signal */
        config->noise_level = 0.01;     /* 1% noise */
        config->enable_trigger = false;
    }
}

/* Create ADC simulator */
adc_simulator_t* adc_simulator_create(const adc_config_t *config) {
    adc_simulator_t *adc = (adc_simulator_t *)calloc(1, sizeof(adc_simulator_t));
    if (!adc) {
        LOG_ERROR("Failed to allocate ADC simulator memory");
        return NULL;
    }
    
    if (config) {
        memcpy(&adc->config, config, sizeof(adc_config_t));
    } else {
        adc_config_get_default(&adc->config);
    }
    
    adc->running = false;
    adc->sequence = 0;
    adc->callback = NULL;
    adc->callback_data = NULL;
    
#ifdef _WIN32
    InitializeCriticalSection(&adc->lock);
#else
    pthread_mutex_init(&adc->lock, NULL);
#endif
    
    LOG_INFO("ADC simulator created");
    return adc;
}

/* Destroy ADC simulator */
void adc_simulator_destroy(adc_simulator_t *adc) {
    if (!adc) return;
    
    if (adc->running) {
        adc_simulator_stop(adc);
    }
    
#ifdef _WIN32
    DeleteCriticalSection(&adc->lock);
#else
    pthread_mutex_destroy(&adc->lock);
#endif
    
    free(adc);
    LOG_INFO("ADC simulator destroyed");
}

/* Start ADC capture */
adc_error_t adc_simulator_start(adc_simulator_t *adc) {
    if (!adc) return ADC_ERROR_INVALID_PARAM;
    if (adc->running) return ADC_OK;
    
    adc->running = true;
    adc->start_time = get_timestamp_us();
    adc->sequence = 0;
    
#ifdef _WIN32
    adc->thread = CreateThread(NULL, 0, capture_thread, adc, 0, NULL);
    if (!adc->thread) {
        adc->running = false;
        LOG_ERROR("Failed to create capture thread");
        return ADC_ERROR_THREAD;
    }
#else
    if (pthread_create(&adc->thread, NULL, capture_thread, adc) != 0) {
        adc->running = false;
        LOG_ERROR("Failed to create capture thread");
        return ADC_ERROR_THREAD;
    }
#endif
    
    LOG_INFO("ADC capture started");
    return ADC_OK;
}

/* Stop ADC capture */
adc_error_t adc_simulator_stop(adc_simulator_t *adc) {
    if (!adc) return ADC_ERROR_INVALID_PARAM;
    if (!adc->running) return ADC_OK;
    
    adc->running = false;
    
#ifdef _WIN32
    WaitForSingleObject(adc->thread, INFINITE);
    CloseHandle(adc->thread);
    adc->thread = NULL;
#else
    pthread_join(adc->thread, NULL);
#endif
    
    LOG_INFO("ADC capture stopped");
    return ADC_OK;
}

/* Check if running */
bool adc_simulator_is_running(const adc_simulator_t *adc) {
    return adc ? adc->running : false;
}

/* Set callback function */
adc_error_t adc_simulator_set_callback(adc_simulator_t *adc, adc_callback_t callback, void *user_data) {
    if (!adc) return ADC_ERROR_INVALID_PARAM;
    
#ifdef _WIN32
    EnterCriticalSection(&adc->lock);
#else
    pthread_mutex_lock(&adc->lock);
#endif
    
    adc->callback = callback;
    adc->callback_data = user_data;
    
#ifdef _WIN32
    LeaveCriticalSection(&adc->lock);
#else
    pthread_mutex_unlock(&adc->lock);
#endif
    
    return ADC_OK;
}

/* Get single sample (non-threaded) */
adc_error_t adc_simulator_get_sample(adc_simulator_t *adc, adc_sample_t *sample) {
    if (!adc || !sample) return ADC_ERROR_INVALID_PARAM;
    
    uint64_t current_time = get_timestamp_us();
    sample->timestamp = (uint32_t)(current_time - adc->start_time);
    sample->value = generate_sample(adc, current_time);
    sample->sequence = adc->sequence++;
    sample->channel = 0;
    sample->flags = 0;
    
    return ADC_OK;
}

/* System Performance Test */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "adc_types.h"
#include "ring_buffer.h"
#include "timestamp.h"
#include "logger.h"

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#include <unistd.h>
#endif

#define TEST_DURATION_MS 5000  /* Test 5 seconds */
#define SAMPLE_RATE 40000

/* Get CPU usage */
static double get_cpu_usage(void) {
#ifdef _WIN32
    FILETIME create_time, exit_time, kernel_time, user_time;
    if (GetProcessTimes(GetCurrentProcess(), &create_time, &exit_time, 
                        &kernel_time, &user_time)) {
        ULARGE_INTEGER kernel, user;
        kernel.LowPart = kernel_time.dwLowDateTime;
        kernel.HighPart = kernel_time.dwHighDateTime;
        user.LowPart = user_time.dwLowDateTime;
        user.HighPart = user_time.dwHighDateTime;
        return (double)(kernel.QuadPart + user.QuadPart) / 10000000.0;
    }
    return 0.0;
#else
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        return (double)(usage.ru_utime.tv_sec + usage.ru_stime.tv_sec) +
               (double)(usage.ru_utime.tv_usec + usage.ru_stime.tv_usec) / 1000000.0;
    }
    return 0.0;
#endif
}

/* Get memory usage */
static size_t get_memory_usage(void) {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize;
    }
    return 0;
#else
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        return usage.ru_maxrss * 1024;
    }
    return 0;
#endif
}

/* Test buffer throughput */
static void test_buffer_throughput(void) {
    printf("\n=== Buffer Throughput Test ===\n");
    
    ring_buffer_t *rb = ring_buffer_create(65536);
    if (!rb) {
        printf("Failed to create buffer\n");
        return;
    }
    
    adc_sample_t sample = {.value = 12345};
    int iterations = 10000000;
    
    /* Write test */
    uint64_t start = get_timestamp_us();
    for (int i = 0; i < iterations; i++) {
        sample.sequence = i;
        ring_buffer_push(rb, &sample);
    }
    uint64_t write_time = get_timestamp_us() - start;
    
    /* Read test */
    start = get_timestamp_us();
    for (int i = 0; i < iterations; i++) {
        ring_buffer_pop(rb, &sample);
    }
    uint64_t read_time = get_timestamp_us() - start;
    
    double write_throughput = (double)iterations * 1000000.0 / write_time;
    double read_throughput = (double)iterations * 1000000.0 / read_time;
    
    printf("Write %d samples:\n", iterations);
    printf("  Time: %.3f ms\n", write_time / 1000.0);
    printf("  Throughput: %.2f M samples/sec\n", write_throughput / 1000000.0);
    printf("  Per sample: %.3f ns\n", (double)write_time * 1000.0 / iterations);
    
    printf("Read %d samples:\n", iterations);
    printf("  Time: %.3f ms\n", read_time / 1000.0);
    printf("  Throughput: %.2f M samples/sec\n", read_throughput / 1000000.0);
    printf("  Per sample: %.3f ns\n", (double)read_time * 1000.0 / iterations);
    
    /* Batch operation test */
    adc_sample_t samples[256];
    for (int i = 0; i < 256; i++) {
        samples[i].value = i;
    }
    
    iterations = 1000000;
    uint32_t pushed, popped;
    
    start = get_timestamp_us();
    for (int i = 0; i < iterations; i++) {
        ring_buffer_push_batch(rb, samples, 256, &pushed);
        ring_buffer_pop_batch(rb, samples, 256, &popped);
    }
    uint64_t batch_time = get_timestamp_us() - start;
    
    double batch_throughput = (double)iterations * 256 * 1000000.0 / batch_time;
    printf("\nBatch operation (256 samples/batch):\n");
    printf("  Total samples: %d\n", iterations * 256);
    printf("  Time: %.3f ms\n", batch_time / 1000.0);
    printf("  Throughput: %.2f M samples/sec\n", batch_throughput / 1000000.0);
    printf("  Per sample: %.3f ns\n", (double)batch_time * 1000.0 / (iterations * 256));
    
    ring_buffer_destroy(rb);
}

/* Simulate complete system performance test */
static void test_system_performance(void) {
    printf("\n=== System Performance Test (Simulate %d Hz sample rate) ===\n", SAMPLE_RATE);
    
    /* Create three-stage pipeline buffers */
    ring_buffer_t *capture_buf = ring_buffer_create(65536);
    ring_buffer_t *process_buf = ring_buffer_create(65536);
    ring_buffer_t *network_buf = ring_buffer_create(65536);
    
    if (!capture_buf || !process_buf || !network_buf) {
        printf("Failed to create buffers\n");
        return;
    }
    
    /* Statistics */
    uint64_t total_samples = 0;
    uint64_t dropped_samples = 0;
    uint64_t start_time = get_timestamp_us();
    uint64_t last_report = start_time;
    
    double cpu_start = get_cpu_usage();
    size_t mem_start = get_memory_usage();
    
    printf("Running %d seconds performance test...\n", TEST_DURATION_MS / 1000);
    
    /* Simulate acquisition */
    adc_sample_t sample;
    uint64_t next_sample_time = get_timestamp_us();
    uint64_t sample_interval = 1000000ULL / SAMPLE_RATE;
    
    while (1) {
        uint64_t current_time = get_timestamp_us();
        
        /* Check test time */
        if (current_time - start_time >= TEST_DURATION_MS * 1000) {
            break;
        }
        
        /* Simulate acquisition */
        while (current_time >= next_sample_time) {
            sample.sequence = (uint32_t)total_samples;
            sample.timestamp = (uint32_t)(current_time - start_time);
            sample.value = (int32_t)(sin(2.0 * 3.14159 * 1000.0 * 
                                         (current_time - start_time) / 1000000.0) * 1000000);
            
            if (ring_buffer_push(capture_buf, &sample)) {
                total_samples++;
            } else {
                dropped_samples++;
            }
            
            next_sample_time += sample_interval;
        }
        
        /* Simulate processing */
        adc_sample_t proc_samples[64];
        uint32_t popped;
        if (ring_buffer_pop_batch(capture_buf, proc_samples, 64, &popped)) {
            uint32_t pushed;
            ring_buffer_push_batch(process_buf, proc_samples, popped, &pushed);
        }
        
        /* Simulate network transmission */
        adc_sample_t net_samples[256];
        if (ring_buffer_pop_batch(process_buf, net_samples, 256, &popped)) {
            /* Simulate send */
            (void)popped;
        }
        
        /* Periodic report */
        if (current_time - last_report >= 1000000) {  /* Every second */
            double elapsed = (current_time - start_time) / 1000000.0;
            double rate = total_samples / elapsed;
            double drop_rate = (total_samples > 0) ? 
                              (double)dropped_samples / total_samples * 100.0 : 0.0;
            
            printf("  [%.1fs] samples: %llu, rate: %.2f Hz, drop: %.2f%%\n",
                   elapsed, (unsigned long long)total_samples, rate, drop_rate);
            
            last_report = current_time;
        }
        
        /* Brief sleep to avoid high CPU usage */
        sleep_us(1);
    }
    
    uint64_t total_elapsed = get_timestamp_us() - start_time;
    double cpu_end = get_cpu_usage();
    size_t mem_end = get_memory_usage();
    
    /* Final report */
    double avg_rate = total_samples / (total_elapsed / 1000000.0);
    double drop_rate = (total_samples > 0) ? 
                       (double)dropped_samples / total_samples * 100.0 : 0.0;
    
    printf("\nPerformance test results:\n");
    printf("  Total samples: %llu\n", (unsigned long long)total_samples);
    printf("  Dropped samples: %llu (%.4f%%)\n", (unsigned long long)dropped_samples, drop_rate);
    printf("  Target sample rate: %d Hz\n", SAMPLE_RATE);
    printf("  Actual sample rate: %.2f Hz\n", avg_rate);
    printf("  Achievement: %.2f%%\n", (avg_rate / SAMPLE_RATE) * 100);
    printf("  CPU time: %.3f sec\n", cpu_end - cpu_start);
    printf("  Memory usage: %zu KB\n", (mem_end - mem_start) / 1024);
    
    ring_buffer_destroy(capture_buf);
    ring_buffer_destroy(process_buf);
    ring_buffer_destroy(network_buf);
}

/* Latency test */
static void test_latency(void) {
    printf("\n=== Latency Test ===\n");
    
    ring_buffer_t *rb = ring_buffer_create(1024);
    if (!rb) {
        printf("Failed to create buffer\n");
        return;
    }
    
    /* Measure single sample latency */
    int iterations = 100000;
    uint64_t total_latency = 0;
    uint64_t max_latency = 0;
    uint64_t min_latency = UINT64_MAX;
    
    adc_sample_t sample;
    
    for (int i = 0; i < iterations; i++) {
        sample.sequence = i;
        
        uint64_t start = get_timestamp_us();
        ring_buffer_push(rb, &sample);
        ring_buffer_pop(rb, &sample);
        uint64_t latency = get_timestamp_us() - start;
        
        total_latency += latency;
        if (latency > max_latency) max_latency = latency;
        if (latency < min_latency) min_latency = latency;
    }
    
    double avg_latency = (double)total_latency / iterations;
    
    printf("Single sample push+pop:\n");
    printf("  Average: %.3f us\n", avg_latency);
    printf("  Min: %.3f us\n", (double)min_latency);
    printf("  Max: %.3f us\n", (double)max_latency);
    
    /* Measure batch latency */
    adc_sample_t samples[256];
    for (int i = 0; i < 256; i++) {
        samples[i].value = i;
    }
    
    iterations = 10000;
    total_latency = 0;
    max_latency = 0;
    min_latency = UINT64_MAX;
    
    for (int i = 0; i < iterations; i++) {
        uint32_t pushed, popped;
        
        uint64_t start = get_timestamp_us();
        ring_buffer_push_batch(rb, samples, 256, &pushed);
        ring_buffer_pop_batch(rb, samples, 256, &popped);
        uint64_t latency = get_timestamp_us() - start;
        
        total_latency += latency;
        if (latency > max_latency) max_latency = latency;
        if (latency < min_latency) min_latency = latency;
    }
    
    avg_latency = (double)total_latency / iterations;
    
    printf("\nBatch (256 samples) push+pop:\n");
    printf("  Average: %.3f us (%.3f us/sample)\n", avg_latency, avg_latency / 256.0);
    printf("  Min: %.3f us\n", (double)min_latency);
    printf("  Max: %.3f us\n", (double)max_latency);
    
    ring_buffer_destroy(rb);
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    printf("========================================\n");
    printf("System Performance Test Program\n");
    printf("========================================\n");
    
    logger_init(NULL, LOG_LEVEL_ERROR);
    timestamp_init();
    
    test_buffer_throughput();
    test_system_performance();
    test_latency();
    
    printf("\n========================================\n");
    printf("Performance test completed!\n");
    printf("========================================\n");
    
    logger_close();
    return 0;
}

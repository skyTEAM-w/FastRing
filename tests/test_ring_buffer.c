/* Ring Buffer Test */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "ring_buffer.h"
#include "timestamp.h"
#include "logger.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

#define TEST_BUFFER_SIZE 1024
#define TEST_ITERATIONS 100000

/* Test basic operations */
static void test_basic_operations(void) {
    printf("\n=== Test Basic Operations ===\n");
    
    ring_buffer_t *rb = ring_buffer_create(TEST_BUFFER_SIZE);
    assert(rb != NULL);
    printf("Ring buffer created successfully, capacity: %u\n", rb->capacity);
    
    /* Test empty buffer */
    assert(ring_buffer_is_empty(rb) == true);
    assert(ring_buffer_count(rb) == 0);
    printf("Empty buffer check passed\n");
    
    /* Test push */
    adc_sample_t sample = {
        .timestamp = 1000,
        .value = 12345,
        .sequence = 1,
        .channel = 0,
        .flags = 0
    };
    
    assert(ring_buffer_push(rb, &sample) == true);
    assert(ring_buffer_is_empty(rb) == false);
    assert(ring_buffer_count(rb) == 1);
    printf("Push sample check passed\n");
    
    /* Test pop */
    adc_sample_t popped;
    assert(ring_buffer_pop(rb, &popped) == true);
    assert(popped.timestamp == sample.timestamp);
    assert(popped.value == sample.value);
    assert(popped.sequence == sample.sequence);
    assert(ring_buffer_is_empty(rb) == true);
    printf("Pop sample check passed\n");
    
    /* Test clear */
    for (int i = 0; i < 10; i++) {
        sample.sequence = i;
        ring_buffer_push(rb, &sample);
    }
    assert(ring_buffer_count(rb) == 10);
    ring_buffer_clear(rb);
    assert(ring_buffer_is_empty(rb) == true);
    printf("Clear buffer check passed\n");
    
    ring_buffer_destroy(rb);
    printf("Basic operations test passed!\n");
}

/* Test batch operations */
static void test_batch_operations(void) {
    printf("\n=== Test Batch Operations ===\n");
    
    ring_buffer_t *rb = ring_buffer_create(TEST_BUFFER_SIZE);
    assert(rb != NULL);
    
    /* Batch push */
    adc_sample_t samples[100];
    for (int i = 0; i < 100; i++) {
        samples[i].sequence = i;
        samples[i].value = i * 100;
    }
    
    uint32_t pushed = 0;
    bool result = ring_buffer_push_batch(rb, samples, 100, &pushed);
    printf("Batch push: requested=%d, actual=%u, result=%s\n", 100, pushed, result ? "success" : "partial");
    assert(pushed == 100);
    assert(ring_buffer_count(rb) == 100);
    
    /* Batch pop */
    adc_sample_t popped[50];
    uint32_t popped_count = 0;
    result = ring_buffer_pop_batch(rb, popped, 50, &popped_count);
    printf("Batch pop: requested=%d, actual=%u, result=%s\n", 50, popped_count, result ? "success" : "partial");
    assert(popped_count == 50);
    assert(ring_buffer_count(rb) == 50);
    
    /* Verify data */
    for (int i = 0; i < 50; i++) {
        assert(popped[i].sequence == i);
        assert(popped[i].value == i * 100);
    }
    printf("Data verification passed\n");
    
    ring_buffer_destroy(rb);
    printf("Batch operations test passed!\n");
}

/* Test buffer full condition */
static void test_buffer_full(void) {
    printf("\n=== Test Buffer Full ===\n");
    
    /* Create small buffer */
    ring_buffer_t *rb = ring_buffer_create(16);
    assert(rb != NULL);
    printf("Buffer capacity: %u\n", rb->capacity);
    
    /* Fill buffer */
    adc_sample_t sample;
    int pushed = 0;
    for (int i = 0; i < 20; i++) {
        sample.sequence = i;
        if (ring_buffer_push(rb, &sample)) {
            pushed++;
        }
    }
    
    printf("Successfully pushed: %d, capacity: %u, actual count: %u, dropped: %llu\n",
           pushed, rb->capacity, ring_buffer_count(rb), (unsigned long long)rb->dropped);
    
    assert(pushed == (int)rb->capacity - 1);  /* Leave one slot to distinguish full/empty */
    assert(rb->dropped == 20 - pushed);
    
    ring_buffer_destroy(rb);
    printf("Buffer full test passed!\n");
}

/* Performance test */
static void test_performance(void) {
    printf("\n=== Performance Test ===\n");
    
    ring_buffer_t *rb = ring_buffer_create(65536);
    assert(rb != NULL);
    
    adc_sample_t sample = {.value = 12345};
    
    /* Single operation performance test */
    uint64_t start = get_timestamp_us();
    for (int i = 0; i < TEST_ITERATIONS; i++) {
        sample.sequence = i;
        ring_buffer_push(rb, &sample);
        ring_buffer_pop(rb, &sample);
    }
    uint64_t elapsed = get_timestamp_us() - start;
    
    double ops_per_sec = (double)TEST_ITERATIONS * 1000000.0 / elapsed;
    printf("Single operation performance: %d push/pop operations\n", TEST_ITERATIONS);
    printf("Time: %.3f ms\n", elapsed / 1000.0);
    printf("Throughput: %.0f ops/sec\n", ops_per_sec);
    printf("Per operation: %.3f us\n", (double)elapsed / TEST_ITERATIONS);
    
    /* Batch operation performance test */
    adc_sample_t samples[256];
    for (int i = 0; i < 256; i++) {
        samples[i].value = i;
    }
    
    start = get_timestamp_us();
    uint32_t pushed, popped;
    for (int i = 0; i < TEST_ITERATIONS / 256; i++) {
        ring_buffer_push_batch(rb, samples, 256, &pushed);
        ring_buffer_pop_batch(rb, samples, 256, &popped);
    }
    elapsed = get_timestamp_us() - start;
    
    ops_per_sec = (double)TEST_ITERATIONS * 1000000.0 / elapsed;
    printf("\nBatch operation performance: %d samples (256 batch)\n", TEST_ITERATIONS);
    printf("Time: %.3f ms\n", elapsed / 1000.0);
    printf("Throughput: %.0f samples/sec\n", ops_per_sec);
    printf("Per sample: %.3f us\n", (double)elapsed / TEST_ITERATIONS);
    
    ring_buffer_destroy(rb);
    printf("\nPerformance test completed!\n");
}

/* Multi-threaded stress test structure */
typedef struct {
    ring_buffer_t *rb;
    volatile int running;
    uint64_t push_count;
    uint64_t pop_count;
} thread_test_data_t;

#ifdef _WIN32
static DWORD WINAPI producer_thread(LPVOID param) {
#else
static void* producer_thread(void *param) {
#endif
    thread_test_data_t *data = (thread_test_data_t *)param;
    adc_sample_t sample;
    
    while (data->running) {
        sample.sequence = (uint32_t)data->push_count;
        if (ring_buffer_push(data->rb, &sample)) {
            data->push_count++;
        }
    }
    
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

#ifdef _WIN32
static DWORD WINAPI consumer_thread(LPVOID param) {
#else
static void* consumer_thread(void *param) {
#endif
    thread_test_data_t *data = (thread_test_data_t *)param;
    adc_sample_t sample;
    
    while (data->running) {
        if (ring_buffer_pop(data->rb, &sample)) {
            data->pop_count++;
        }
    }
    
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

/* Multi-threaded test */
static void test_multithreaded(void) {
    printf("\n=== Multi-threaded Test ===\n");
    
    ring_buffer_t *rb = ring_buffer_create(65536);
    assert(rb != NULL);
    
    thread_test_data_t data = {
        .rb = rb,
        .running = 1,
        .push_count = 0,
        .pop_count = 0
    };
    
    printf("Starting producer and consumer threads...\n");
    
#ifdef _WIN32
    HANDLE producer = CreateThread(NULL, 0, producer_thread, &data, 0, NULL);
    HANDLE consumer = CreateThread(NULL, 0, consumer_thread, &data, 0, NULL);
#else
    pthread_t producer, consumer;
    pthread_create(&producer, NULL, producer_thread, &data);
    pthread_create(&consumer, NULL, consumer_thread, &data);
#endif
    
    /* Run for 2 seconds */
    sleep_ms(2000);
    data.running = 0;
    
#ifdef _WIN32
    WaitForSingleObject(producer, INFINITE);
    WaitForSingleObject(consumer, INFINITE);
    CloseHandle(producer);
    CloseHandle(consumer);
#else
    pthread_join(producer, NULL);
    pthread_join(consumer, NULL);
#endif
    
    printf("Producer pushed: %llu\n", (unsigned long long)data.push_count);
    printf("Consumer popped: %llu\n", (unsigned long long)data.pop_count);
    printf("Buffer remaining: %u\n", ring_buffer_count(rb));
    printf("Dropped count: %llu\n", (unsigned long long)rb->dropped);
    
    int64_t diff = (int64_t)data.push_count - (int64_t)data.pop_count - (int64_t)ring_buffer_count(rb);
    printf("Data consistency: %s (diff: %lld)\n", diff == 0 ? "PASSED" : "FAILED", diff);
    
    ring_buffer_destroy(rb);
    printf("Multi-threaded test completed!\n");
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    printf("========================================\n");
    printf("Ring Buffer Test Program\n");
    printf("========================================\n");
    
    logger_init(NULL, LOG_LEVEL_WARN);
    timestamp_init();
    
    test_basic_operations();
    test_batch_operations();
    test_buffer_full();
    test_performance();
    test_multithreaded();
    
    printf("\n========================================\n");
    printf("All tests passed!\n");
    printf("========================================\n");
    
    logger_close();
    return 0;
}

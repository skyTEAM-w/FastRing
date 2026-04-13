#include <assert.h>
#include <stdio.h>

#include "timestamp.h"
#include "logger.h"

static void test_monotonic_progress(void) {
    uint64_t t1 = get_timestamp_us();
    sleep_ms(5);
    uint64_t t2 = get_timestamp_us();

    assert(t2 >= t1);
    assert(elapsed_us(t1, t2) >= 4000);
}

static void test_sleep_us_accuracy(void) {
    uint64_t start = get_timestamp_us();
    sleep_us(2000);
    uint64_t elapsed = get_timestamp_us() - start;

    assert(elapsed >= 1000);
}

static void test_ns_conversion(void) {
    uint64_t ns_before = get_timestamp_ns();
    sleep_us(500);
    uint64_t ns_after = get_timestamp_ns();

    assert(ns_after >= ns_before);
    assert(ns_after - ns_before >= 100000);
}

int main(void) {
    printf("========================================\n");
    printf("Timestamp Test Program\n");
    printf("========================================\n");

    logger_init(NULL, LOG_LEVEL_WARN);
    timestamp_init();

    test_monotonic_progress();
    test_sleep_us_accuracy();
    test_ns_conversion();

    printf("All timestamp tests passed!\n");
    logger_close();
    return 0;
}

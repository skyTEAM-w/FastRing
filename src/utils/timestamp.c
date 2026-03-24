#include "timestamp.h"
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>

static LARGE_INTEGER g_freq;
static BOOL g_freq_initialized = FALSE;

/* 高精度睡眠使用的定时器池 */
#define TIMER_POOL_SIZE 4
static HANDLE g_timer_pool[TIMER_POOL_SIZE];
static volatile LONG g_timer_index = 0;
static BOOL g_timer_pool_initialized = FALSE;

void timestamp_init(void) {
    g_freq_initialized = QueryPerformanceFrequency(&g_freq);
    
    /* 初始化定时器池 */
    if (!g_timer_pool_initialized) {
        for (int i = 0; i < TIMER_POOL_SIZE; i++) {
            g_timer_pool[i] = CreateWaitableTimer(NULL, TRUE, NULL);
        }
        g_timer_pool_initialized = TRUE;
    }
}

uint64_t get_timestamp_us(void) {
    if (!g_freq_initialized) {
        timestamp_init();
    }
    
    LARGE_INTEGER count;
    QueryPerformanceCounter(&count);
    
    /* 使用64位乘法避免溢出 */
    return (uint64_t)((count.QuadPart * 1000000ULL) / g_freq.QuadPart);
}

uint64_t get_timestamp_ms(void) {
    return get_timestamp_us() / 1000;
}

uint64_t get_timestamp_ns(void) {
    if (!g_freq_initialized) {
        timestamp_init();
    }
    
    LARGE_INTEGER count;
    QueryPerformanceCounter(&count);
    
    return (uint64_t)((count.QuadPart * 1000000000ULL) / g_freq.QuadPart);
}

/* 自旋锁等待（用于微秒级延迟） */
static void spin_wait_us(uint64_t us) {
    uint64_t start = get_timestamp_us();
    while ((get_timestamp_us() - start) < us) {
        /* 使用PAUSE指令减少功耗 */
        #ifdef _WIN32
        YieldProcessor();
        #else
        __asm__ volatile ("pause");
        #endif
    }
}

void sleep_us(uint64_t us) {
    if (us == 0) return;
    
    /* 小于100微秒使用自旋等待 */
    if (us < 100) {
        spin_wait_us(us);
        return;
    }
    
    /* 使用定时器池 */
    if (g_timer_pool_initialized) {
        LONG idx = InterlockedIncrement(&g_timer_index) % TIMER_POOL_SIZE;
        HANDLE timer = g_timer_pool[idx];
        
        LARGE_INTEGER due_time;
        due_time.QuadPart = -(LONGLONG)(us * 10); /* 100纳秒为单位 */
        
        if (SetWaitableTimer(timer, &due_time, 0, NULL, NULL, FALSE)) {
            WaitForSingleObject(timer, INFINITE);
            return;
        }
    }
    
    /* 回退到Sleep */
    Sleep((DWORD)((us + 999) / 1000));
}

void sleep_ms(uint32_t ms) {
    Sleep(ms);
}

uint64_t get_cpu_cycles(void) {
    #if defined(_WIN64)
        return __rdtsc();
    #elif defined(_WIN32)
        return get_timestamp_us();
    #else
        return get_timestamp_us();
    #endif
}

#else /* Linux/Unix */

#include <time.h>
#include <unistd.h>

void timestamp_init(void) {
    /* No initialization needed */
}

uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000;
}

uint64_t get_timestamp_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000;
}

uint64_t get_timestamp_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

void sleep_us(uint64_t us) {
    struct timespec ts;
    ts.tv_sec = (time_t)(us / 1000000);
    ts.tv_nsec = (long)((us % 1000000) * 1000);
    nanosleep(&ts, NULL);
}

void sleep_ms(uint32_t ms) {
    usleep(ms * 1000);
}

uint64_t get_cpu_cycles(void) {
    #if defined(__x86_64__) || defined(__i386__)
        unsigned int lo, hi;
        __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
        return ((uint64_t)hi << 32) | lo;
    #else
        return get_timestamp_us();
    #endif
}

#endif

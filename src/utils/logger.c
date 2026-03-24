#include "logger.h"
#include "timestamp.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

static FILE *g_log_file = NULL;
static log_level_t g_log_level = LOG_LEVEL_INFO;

#ifdef _WIN32
static CRITICAL_SECTION g_log_lock;
static BOOL g_lock_initialized = FALSE;
#else
static pthread_mutex_t g_log_lock = PTHREAD_MUTEX_INITIALIZER;
#endif

static const char *g_level_names[] = {
    "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

/* 初始化日志 */
int logger_init(const char *filename, log_level_t level) {
    if (g_log_file) {
        fclose(g_log_file);
        g_log_file = NULL;
    }
    
    if (filename) {
        g_log_file = fopen(filename, "a");
        if (!g_log_file) {
            fprintf(stderr, "Failed to open log file: %s\n", filename);
            return -1;
        }
    }
    
    g_log_level = level;
    
#ifdef _WIN32
    if (!g_lock_initialized) {
        InitializeCriticalSection(&g_log_lock);
        g_lock_initialized = TRUE;
    }
#endif
    
    LOG_INFO("Logger system initialized, level: %s", g_level_names[level]);
    return 0;
}

/* 关闭日志 */
void logger_close(void) {
    if (g_log_file) {
        fclose(g_log_file);
        g_log_file = NULL;
    }
    
#ifdef _WIN32
    if (g_lock_initialized) {
        DeleteCriticalSection(&g_log_lock);
        g_lock_initialized = FALSE;
    }
#endif
}

/* 输出日志消息 */
void log_message(log_level_t level, const char *file, int line, const char *fmt, ...) {
    if (level < g_log_level) return;
    
    /* 提取文件名（不包含路径） */
    const char *filename = file;
    const char *last_slash = strrchr(file, '/');
    const char *last_backslash = strrchr(file, '\\');
    if (last_backslash && (!last_slash || last_backslash > last_slash)) {
        last_slash = last_backslash;
    }
    if (last_slash) {
        filename = last_slash + 1;
    }
    
    /* 格式化时间戳 */
    uint64_t timestamp_ms = get_timestamp_ms();
    uint32_t hours = (uint32_t)((timestamp_ms / 3600000) % 24);
    uint32_t minutes = (uint32_t)((timestamp_ms / 60000) % 60);
    uint32_t seconds = (uint32_t)((timestamp_ms / 1000) % 60);
    uint32_t millis = (uint32_t)(timestamp_ms % 1000);
    
    /* 构建日志消息 */
    char buffer[4096];
    int offset = snprintf(buffer, sizeof(buffer), 
                          "[%02u:%02u:%02u.%03u] [%s] [%s:%d] ",
                          hours, minutes, seconds, millis,
                          g_level_names[level], filename, line);
    
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer + offset, sizeof(buffer) - offset, fmt, args);
    va_end(args);
    
    /* 添加换行符 */
    size_t len = strlen(buffer);
    if (len < sizeof(buffer) - 1) {
        buffer[len] = '\n';
        buffer[len + 1] = '\0';
    }
    
#ifdef _WIN32
    if (g_lock_initialized) {
        EnterCriticalSection(&g_log_lock);
    }
#else
    pthread_mutex_lock(&g_log_lock);
#endif
    
    /* 输出到控制台 */
    printf("%s", buffer);
    fflush(stdout);
    
    /* 输出到文件 */
    if (g_log_file) {
        fprintf(g_log_file, "%s", buffer);
        fflush(g_log_file);
    }
    
#ifdef _WIN32
    if (g_lock_initialized) {
        LeaveCriticalSection(&g_log_lock);
    }
#else
    pthread_mutex_unlock(&g_log_lock);
#endif
}

/* 设置日志级别 */
void logger_set_level(log_level_t level) {
    g_log_level = level;
}

/* 获取日志级别 */
log_level_t logger_get_level(void) {
    return g_log_level;
}

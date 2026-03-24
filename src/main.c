#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include "adc_types.h"
#include "adc_simulator.h"
#include "ring_buffer.h"
#include "thread_manager.h"
#include "tcp_client.h"
#include "timestamp.h"
#include "logger.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

/* Global variables */
static volatile int g_running = 1;
static thread_manager_t *g_thread_manager = NULL;

/* Signal handler */
static void signal_handler(int sig) {
    (void)sig;
    LOG_INFO("Shutdown signal received, closing...");
    g_running = 0;
}

/* 打印系统统计 */
static void print_stats(const system_stats_t *stats) {
    printf("\n========== System Statistics ==========\n");
    printf("Total Samples: %llu\n", (unsigned long long)stats->total_samples);
    printf("Dropped Samples: %llu\n", (unsigned long long)stats->dropped_samples);
    printf("Sent Bytes: %llu\n", (unsigned long long)stats->sent_bytes);
    printf("Send Errors: %llu\n", (unsigned long long)stats->send_errors);
    printf("Average Sample Rate: %.2f Hz\n", stats->avg_sample_rate);
    printf("Average Latency: %.3f ms\n", stats->avg_latency_ms);
    printf("CPU Usage: %.2f%%\n", stats->cpu_usage);
    printf("========================================\n");
}

/* 打印线程统计 */
static void print_thread_stats(thread_manager_t *tm) {
    const char *thread_names[] = {"Acquisition", "Processing", "Network"};

    printf("\n========== Thread Statistics ==========\n");
    for (int i = 0; i < THREAD_COUNT; i++) {
        thread_stats_t stats;
        if (thread_manager_get_stats(tm, i, &stats) == ADC_OK) {
            printf("[%s Thread] Loops: %llu, Processed: %llu, Errors: %llu, Avg Cycle: %.2f us, Max Cycle: %.2f us\n",
                   thread_names[i],
                   (unsigned long long)stats.loop_count,
                   (unsigned long long)stats.item_count,
                   (unsigned long long)stats.error_count,
                   stats.avg_cycle_time_us,
                   stats.max_cycle_time_us);
        }
    }
    printf("========================================\n");
}

static void print_help(const char *program) {
    printf("Usage: %s [options]\n", program);
    printf("Options:\n");
    printf("  -h, --help          Show this help message\n");
    printf("  -s, --server IP     Set server IP address (default: 127.0.0.1)\n");
    printf("  -p, --port PORT     Set server port (default: 8080)\n");
    printf("  -r, --rate HZ       Set sample rate (default: 40000)\n");
    printf("  -t, --time SECONDS  Run time in seconds (default: infinite)\n");
    printf("  -l, --log FILE      Log file path\n");
    printf("  -v, --verbose       Verbose logging\n");
}

int main(int argc, char *argv[]) {
    /* Default configuration */
    char server_ip[16] = DEFAULT_SERVER_IP;
    uint16_t server_port = DEFAULT_SERVER_PORT;
    uint32_t sample_rate = ADC_SAMPLE_RATE;
    int run_time_seconds = -1;  /* -1 means infinite run */
    char *log_file = NULL;
    log_level_t log_level = LOG_LEVEL_INFO;

    /* Parse command line arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help(argv[0]);
            return 0;
        } else if ((strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--server") == 0) && i + 1 < argc) {
            strncpy(server_ip, argv[++i], sizeof(server_ip) - 1);
            server_ip[sizeof(server_ip) - 1] = '\0';
        } else if ((strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) && i + 1 < argc) {
            server_port = (uint16_t)atoi(argv[++i]);
        } else if ((strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--rate") == 0) && i + 1 < argc) {
            sample_rate = (uint32_t)atoi(argv[++i]);
        } else if ((strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--time") == 0) && i + 1 < argc) {
            run_time_seconds = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--log") == 0) && i + 1 < argc) {
            log_file = argv[++i];
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            log_level = LOG_LEVEL_DEBUG;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_help(argv[0]);
            return 1;
        }
    }

    /* Initialize logger */
    if (logger_init(log_file, log_level) != 0) {
        fprintf(stderr, "Failed to initialize logger\n");
        return 1;
    }

    LOG_INFO("========================================");
    LOG_INFO("High-Performance ADC Data Acquisition and Transmission System");
    LOG_INFO("========================================");
    LOG_INFO("Server: %s:%d", server_ip, server_port);
    LOG_INFO("Sample Rate: %d Hz", sample_rate);
    LOG_INFO("Resolution: %d bits", ADC_RESOLUTION);
    LOG_INFO("========================================");

    /* Set up signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
#ifdef _WIN32
    signal(SIGBREAK, signal_handler);
#else
    signal(SIGQUIT, signal_handler);
#endif

    /* Initialize timestamp system */
    timestamp_init();

    /* Create buffer manager */
    buffer_manager_t *bm = buffer_manager_create();
    if (!bm) {
        LOG_FATAL("Failed to create buffer manager");
        logger_close();
        return 1;
    }

    if (buffer_manager_init(bm) != ADC_OK) {
        LOG_FATAL("Failed to initialize buffer manager");
        buffer_manager_destroy(bm);
        logger_close();
        return 1;
    }

    /* Create thread manager */
    g_thread_manager = thread_manager_create();
    if (!g_thread_manager) {
        LOG_FATAL("Failed to create thread manager");
        buffer_manager_destroy(bm);
        logger_close();
        return 1;
    }

    if (thread_manager_init(g_thread_manager, bm) != ADC_OK) {
        LOG_FATAL("Failed to initialize thread manager");
        thread_manager_destroy(g_thread_manager);
        buffer_manager_destroy(bm);
        logger_close();
        return 1;
    }

    /* Start all threads */
    if (thread_manager_start(g_thread_manager) != ADC_OK) {
        LOG_FATAL("Failed to start threads");
        thread_manager_destroy(g_thread_manager);
        buffer_manager_destroy(bm);
        logger_close();
        return 1;
    }
    
    LOG_INFO("System started successfully. Press Ctrl+C to stop");

    /* Main loop */
    uint64_t start_time = get_timestamp_ms();
    uint64_t last_stats_time = start_time;

    while (g_running) {
        uint64_t current_time = get_timestamp_ms();

        /* Check run time */
        if (run_time_seconds > 0) {
            if ((current_time - start_time) >= (uint64_t)run_time_seconds * 1000) {
                LOG_INFO("Run time limit reached, stopping...");
                break;
            }
        }

        /* Print stats every 5 seconds */
        if ((current_time - last_stats_time) >= 5000) {
            system_stats_t *stats = thread_manager_get_system_stats(g_thread_manager);
            if (stats) {
                /* Calculate average sample rate */
                uint64_t elapsed_sec = (current_time - start_time) / 1000;
                if (elapsed_sec > 0) {
                    stats->avg_sample_rate = (double)stats->total_samples / elapsed_sec;
                }

                print_stats(stats);
                print_thread_stats(g_thread_manager);
            }
            last_stats_time = current_time;
        }

        sleep_ms(100);
    }

    /* Stop system */
    LOG_INFO("Stopping system...");

    /* Print final stats */
    system_stats_t *final_stats = thread_manager_get_system_stats(g_thread_manager);
    if (final_stats) {
        uint64_t elapsed_sec = (get_timestamp_ms() - start_time) / 1000;
        if (elapsed_sec > 0) {
            final_stats->avg_sample_rate = (double)final_stats->total_samples / elapsed_sec;
        }
        print_stats(final_stats);
        print_thread_stats(g_thread_manager);
    }

    /* Cleanup resources */
    thread_manager_destroy(g_thread_manager);
    buffer_manager_destroy(bm);

    LOG_INFO("System safely shut down");
    logger_close();

    return 0;
}

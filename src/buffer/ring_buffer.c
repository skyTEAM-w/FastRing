#include "ring_buffer.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* Memory barrier macros - optimized for single producer single consumer */
#ifdef _WIN32
    #define MEMORY_BARRIER() MemoryBarrier()
    #define READ_BARRIER() MemoryBarrier()
    #define WRITE_BARRIER() MemoryBarrier()
#else
    #define MEMORY_BARRIER() atomic_thread_fence(memory_order_seq_cst)
    #define READ_BARRIER() atomic_thread_fence(memory_order_acquire)
    #define WRITE_BARRIER() atomic_thread_fence(memory_order_release)
#endif

/* Create ring buffer */
ring_buffer_t* ring_buffer_create(uint32_t capacity) {
    /* Reserve one slot to distinguish full vs empty, so the raw capacity must be >= 2. */
    if (capacity < 2) {
        capacity = 2;
    }

    /* Capacity must be power of 2 for fast modulo with bitmask */
    if ((capacity & (capacity - 1)) != 0) {
        /* Find next power of 2 if not already */
        uint32_t power = 1;
        while (power < capacity) {
            power <<= 1;
        }
        capacity = power;
    }
    
    ring_buffer_t *rb = (ring_buffer_t *)calloc(1, sizeof(ring_buffer_t));
    if (!rb) {
        LOG_ERROR("Failed to allocate ring buffer control structure");
        return NULL;
    }
    
    /* Use aligned allocation for better cache performance */
    #ifdef _WIN32
        rb->buffer = (adc_sample_t *)_aligned_malloc(capacity * sizeof(adc_sample_t), 64);
    #else
        if (posix_memalign((void **)&rb->buffer, 64, capacity * sizeof(adc_sample_t)) != 0) {
            rb->buffer = NULL;
        }
    #endif
    
    if (!rb->buffer) {
        LOG_ERROR("Failed to allocate ring buffer data memory, capacity: %u", capacity);
        free(rb);
        return NULL;
    }
    
    rb->capacity = capacity;
    rb->mask = capacity - 1;
    rb->head = 0;
    rb->tail = 0;
    rb->dropped = 0;
    
    LOG_DEBUG("Ring buffer created, capacity: %u", capacity);
    return rb;
}

/* Destroy ring buffer */
void ring_buffer_destroy(ring_buffer_t *rb) {
    if (!rb) return;
    
    if (rb->buffer) {
        #ifdef _WIN32
            _aligned_free(rb->buffer);
        #else
            free(rb->buffer);
        #endif
        rb->buffer = NULL;
    }
    
    free(rb);
    LOG_DEBUG("Ring buffer destroyed");
}

/* Get current element count */
uint32_t ring_buffer_count(const ring_buffer_t *rb) {
    if (!rb) return 0;
    
    READ_BARRIER();
    uint32_t head = rb->head;
    uint32_t tail = rb->tail;
    
    return (head - tail) & rb->mask;
}

/* Check if empty */
bool ring_buffer_is_empty(const ring_buffer_t *rb) {
    if (!rb) return true;
    
    READ_BARRIER();
    return rb->head == rb->tail;
}

/* Check if full */
bool ring_buffer_is_full(const ring_buffer_t *rb) {
    if (!rb) return true;
    
    READ_BARRIER();
    uint32_t next_head = (rb->head + 1) & rb->mask;
    return next_head == rb->tail;
}

/* Get available space */
uint32_t ring_buffer_available(const ring_buffer_t *rb) {
    if (!rb) return 0;
    
    return rb->capacity - ring_buffer_count(rb) - 1;
}

/* Push single sample - lock-free for single producer */
bool ring_buffer_push(ring_buffer_t *rb, const adc_sample_t *sample) {
    if (!rb || !sample) return false;
    
    uint32_t head = rb->head;
    uint32_t next_head = (head + 1) & rb->mask;
    
    /* Check if buffer is full */
    if (next_head == rb->tail) {
        rb->dropped++;
        return false;
    }
    
    /* Write data */
    rb->buffer[head] = *sample;
    WRITE_BARRIER();
    
    /* Update head pointer */
    rb->head = next_head;
    
    return true;
}

/* Pop single sample - lock-free for single consumer */
bool ring_buffer_pop(ring_buffer_t *rb, adc_sample_t *sample) {
    if (!rb || !sample) return false;
    
    /* Check if buffer is empty */
    if (rb->head == rb->tail) {
        return false;
    }
    
    uint32_t tail = rb->tail;
    
    /* Read data */
    *sample = rb->buffer[tail];
    READ_BARRIER();
    
    /* Update tail pointer */
    rb->tail = (tail + 1) & rb->mask;
    
    return true;
}

/* Optimized batch push - reduces memory barrier overhead */
bool ring_buffer_push_batch(ring_buffer_t *rb, const adc_sample_t *samples, 
                            uint32_t count, uint32_t *pushed) {
    if (!rb || !samples || count == 0) {
        if (pushed) *pushed = 0;
        return false;
    }
    
    uint32_t available = ring_buffer_available(rb);
    uint32_t to_push = (count < available) ? count : available;
    
    if (to_push == 0) {
        rb->dropped += count;
        if (pushed) *pushed = 0;
        return false;
    }
    
    uint32_t head = rb->head;
    
    /* Copy data in chunks to handle wrap-around */
    uint32_t first_chunk = to_push;
    uint32_t wrap_point = rb->capacity - head;
    
    if (first_chunk > wrap_point) {
        first_chunk = wrap_point;
    }
    
    /* First chunk */
    memcpy(&rb->buffer[head], samples, first_chunk * sizeof(adc_sample_t));
    
    /* Second chunk (if wrapped) */
    if (first_chunk < to_push) {
        uint32_t second_chunk = to_push - first_chunk;
        memcpy(&rb->buffer[0], &samples[first_chunk], second_chunk * sizeof(adc_sample_t));
    }
    
    WRITE_BARRIER();
    
    /* Update head pointer once after all writes */
    rb->head = (head + to_push) & rb->mask;
    
    /* Count dropped samples */
    if (to_push < count) {
        rb->dropped += (count - to_push);
    }
    
    if (pushed) *pushed = to_push;
    return (to_push == count);
}

/* Optimized batch pop - reduces memory barrier overhead */
bool ring_buffer_pop_batch(ring_buffer_t *rb, adc_sample_t *samples, 
                           uint32_t count, uint32_t *popped) {
    if (!rb || !samples || count == 0) {
        if (popped) *popped = 0;
        return false;
    }
    
    uint32_t current_count = ring_buffer_count(rb);
    uint32_t to_pop = (count < current_count) ? count : current_count;
    
    if (to_pop == 0) {
        if (popped) *popped = 0;
        return false;
    }
    
    uint32_t tail = rb->tail;
    
    /* Copy data in chunks to handle wrap-around */
    uint32_t first_chunk = to_pop;
    uint32_t wrap_point = rb->capacity - tail;
    
    if (first_chunk > wrap_point) {
        first_chunk = wrap_point;
    }
    
    /* First chunk */
    memcpy(samples, &rb->buffer[tail], first_chunk * sizeof(adc_sample_t));
    
    /* Second chunk (if wrapped) */
    if (first_chunk < to_pop) {
        uint32_t second_chunk = to_pop - first_chunk;
        memcpy(&samples[first_chunk], &rb->buffer[0], second_chunk * sizeof(adc_sample_t));
    }
    
    READ_BARRIER();
    
    /* Update tail pointer once after all reads */
    rb->tail = (tail + to_pop) & rb->mask;
    
    if (popped) *popped = to_pop;
    return (to_pop > 0);
}

/* Clear buffer */
void ring_buffer_clear(ring_buffer_t *rb) {
    if (!rb) return;
    
    rb->head = 0;
    rb->tail = 0;
    rb->dropped = 0;
    MEMORY_BARRIER();
}

/* Create buffer manager */
buffer_manager_t* buffer_manager_create(void) {
    buffer_manager_t *bm = (buffer_manager_t *)calloc(1, sizeof(buffer_manager_t));
    if (!bm) {
        LOG_ERROR("Failed to allocate buffer manager memory");
        return NULL;
    }
    
    return bm;
}

/* Destroy buffer manager */
void buffer_manager_destroy(buffer_manager_t *bm) {
    if (!bm) return;
    
    if (bm->capture_to_process) {
        ring_buffer_destroy(bm->capture_to_process);
        bm->capture_to_process = NULL;
    }
    
    if (bm->process_to_network) {
        ring_buffer_destroy(bm->process_to_network);
        bm->process_to_network = NULL;
    }
    
    if (bm->network_pool) {
        ring_buffer_destroy(bm->network_pool);
        bm->network_pool = NULL;
    }
    
    free(bm);
    LOG_INFO("Buffer manager destroyed");
}

/* Initialize buffer manager */
adc_error_t buffer_manager_init(buffer_manager_t *bm) {
    if (!bm) return ADC_ERROR_INVALID_PARAM;
    
    /* Create capture->process buffer */
    bm->capture_to_process = ring_buffer_create(RING_BUFFER_SIZE);
    if (!bm->capture_to_process) {
        LOG_ERROR("Failed to create capture buffer");
        return ADC_ERROR_BUFFER;
    }
    
    /* Create process->network buffer */
    bm->process_to_network = ring_buffer_create(RING_BUFFER_SIZE);
    if (!bm->process_to_network) {
        LOG_ERROR("Failed to create process buffer");
        ring_buffer_destroy(bm->capture_to_process);
        bm->capture_to_process = NULL;
        return ADC_ERROR_BUFFER;
    }
    
    /* Create network send pool */
    bm->network_pool = ring_buffer_create(RING_BUFFER_SIZE);
    if (!bm->network_pool) {
        LOG_ERROR("Failed to create network buffer");
        ring_buffer_destroy(bm->capture_to_process);
        ring_buffer_destroy(bm->process_to_network);
        bm->capture_to_process = NULL;
        bm->process_to_network = NULL;
        return ADC_ERROR_BUFFER;
    }
    
    LOG_INFO("Buffer manager initialized");
    return ADC_OK;
}

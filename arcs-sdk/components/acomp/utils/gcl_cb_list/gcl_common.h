/*
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Error codes */
#define GCL_OK                  0
#define GCL_ERR_NOT_FOUND      -1
#define GCL_ERR_NO_MEM         -2
#define GCL_ERR_INVALID_ARG    -3
#define GCL_ERR_TIMEOUT        -4

/* Memory management functions */
#define gcl_heap_free(ptr)      psram_free(ptr)

/* Simple mutex implementation using critical sections */
typedef struct {
    volatile uint32_t locked;
} gcl_mutex_t;

static inline void gcl_mutex_init(gcl_mutex_t *mutex) {
    mutex->locked = 0;
}

static inline void gcl_mutex_lock(gcl_mutex_t *mutex) {
    // Simple spinlock implementation
    while (__sync_lock_test_and_set(&mutex->locked, 1)) {
        // Spin wait
    }
}

static inline void gcl_mutex_unlock(gcl_mutex_t *mutex) {
    __sync_lock_release(&mutex->locked);
}

static inline void gcl_mutex_delete(gcl_mutex_t *mutex) {
    mutex->locked = 0;
}

#ifdef __cplusplus
}
#endif

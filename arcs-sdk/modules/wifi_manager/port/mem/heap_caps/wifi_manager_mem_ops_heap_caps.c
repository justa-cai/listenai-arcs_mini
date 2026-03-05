/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "wifi_manager/wifi_manager.h"
#include "esp_heap_caps.h"
#include <limits.h>
#include <string.h>

// 定义内存分配标志，基于platform_dev.h中的宏定义
#define HEAP_CAPS_DEFAULT_FLAGS   MALLOC_CAP_SPIRAM
#define HEAP_CAPS_NOCACHE_FLAGS   MALLOC_CAP_INTERNAL
#define HEAP_CAPS_WORD_ALIGN      32U

static size_t heap_caps_align_up(size_t size, size_t align)
{
    return (size + align - 1) & ~(align - 1);
}

static void *heap_caps_malloc_wrapper(size_t size) {
    if (size == 0) {
        return heap_caps_malloc(size, HEAP_CAPS_DEFAULT_FLAGS);
    }

    size_t aligned_size = heap_caps_align_up(size, HEAP_CAPS_WORD_ALIGN);
    return heap_caps_aligned_alloc(HEAP_CAPS_WORD_ALIGN, aligned_size, HEAP_CAPS_DEFAULT_FLAGS);
}

static void *heap_caps_calloc_wrapper(size_t nmemb, size_t size) {
    if (nmemb == 0 || size == 0) {
        return heap_caps_calloc(nmemb, size, HEAP_CAPS_DEFAULT_FLAGS);
    }
    if (size > (SIZE_MAX / nmemb)) {
        return NULL;
    }

    size_t total = nmemb * size;
    size_t aligned_size = heap_caps_align_up(total, HEAP_CAPS_WORD_ALIGN);
    void *ptr = heap_caps_aligned_alloc(HEAP_CAPS_WORD_ALIGN, aligned_size, HEAP_CAPS_DEFAULT_FLAGS);
    if (ptr != NULL) {
        memset(ptr, 0, aligned_size);
    }
    return ptr;
}

static void *heap_caps_align_malloc_wrapper(size_t align, size_t size) {
    if (align == 0) {
        return NULL;
    }
    if (size == 0) {
        return heap_caps_aligned_alloc(align, size, HEAP_CAPS_DEFAULT_FLAGS);
    }
    size_t aligned_size = heap_caps_align_up(size, align);
    return heap_caps_aligned_alloc(align, aligned_size, HEAP_CAPS_DEFAULT_FLAGS);
}

static void *heap_caps_nocache_malloc_wrapper(size_t size) {
    return heap_caps_malloc(size, HEAP_CAPS_NOCACHE_FLAGS);
}

static void heap_caps_free_wrapper(void *ptr) {
    heap_caps_free(ptr);
}

wifi_manager_mem_ops_t wifi_manager_mem_ops_heap_caps = {
    .malloc = heap_caps_malloc_wrapper,
    .calloc = heap_caps_calloc_wrapper,
    .align_malloc = heap_caps_align_malloc_wrapper,
    .nocache_malloc = heap_caps_nocache_malloc_wrapper,
    .free = heap_caps_free_wrapper
}; 

wifi_manager_mem_ops_t* wifi_manager_heap_caps_mem_ops_get(void) {
    return &wifi_manager_mem_ops_heap_caps;
}

/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mock_esp_heap_caps.h"

#include "fff.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

DEFINE_FAKE_VALUE_FUNC(void *, heap_caps_malloc, size_t, uint32_t);
DEFINE_FAKE_VALUE_FUNC(void *, heap_caps_calloc, size_t, size_t, uint32_t);
DEFINE_FAKE_VALUE_FUNC(void *, heap_caps_aligned_alloc, size_t, size_t, uint32_t);
DEFINE_FAKE_VALUE_FUNC(void *, heap_caps_realloc, void *, size_t, uint32_t);
DEFINE_FAKE_VOID_FUNC(heap_caps_free, void *);

void* custom_esp_heap_caps_malloc(size_t size, uint32_t caps)
{
    void *p = malloc(size);
    // printf("malloc size: %zu, ptr: %p\n", size, p);
    return p;
}

void* custom_esp_heap_caps_calloc(size_t n, size_t size, uint32_t caps)
{
    void *p = calloc(n, size);
    // printf("calloc n: %zu, size: %zu, ptr: %p\n", n, size, p);
    return p;
}

void *custom_esp_heap_caps_aligned_alloc(size_t alignment, size_t size, uint32_t caps) {
    (void)caps;
    void *p = aligned_alloc(alignment, size);
    // printf("aligned_alloc alignment: %zu, size: %zu, ptr: %p\n", alignment, size, p);
    return p;
}

void *custom_esp_heap_caps_realloc(void *ptr, size_t size, uint32_t caps) {
    (void)caps;
    void *p = realloc(ptr, size);
    // printf("realloc ptr: %p, size: %zu\n", p, size);
    return p;
}

void custom_esp_heap_caps_free(void *ptr) {
    // printf("esp heap free ptr: %p\n", ptr);
    free(ptr);
}

void mock_esp_heap_caps_init()
{
    RESET_FAKE(heap_caps_malloc);
    RESET_FAKE(heap_caps_calloc);
    RESET_FAKE(heap_caps_aligned_alloc);
    RESET_FAKE(heap_caps_realloc);
    RESET_FAKE(heap_caps_free);

    heap_caps_malloc_fake.custom_fake = custom_esp_heap_caps_malloc;
    heap_caps_calloc_fake.custom_fake = custom_esp_heap_caps_calloc;
    heap_caps_aligned_alloc_fake.custom_fake = custom_esp_heap_caps_aligned_alloc;
    heap_caps_realloc_fake.custom_fake = custom_esp_heap_caps_realloc;
    heap_caps_free_fake.custom_fake = custom_esp_heap_caps_free;
}

void mock_esp_heap_caps_reset()
{
    mock_esp_heap_caps_init();
}
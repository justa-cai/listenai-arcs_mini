/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MALLOC_CAP_SPIRAM           (1<<10) ///< Memory must be in SPI RAM
#define MALLOC_CAP_INTERNAL         (1<<11) ///< Internal memory


void *heap_caps_malloc(size_t size, uint32_t caps);
void *heap_caps_calloc(size_t n, size_t size, uint32_t caps);
void *heap_caps_aligned_alloc(size_t alignment, size_t size, uint32_t caps);
void *heap_caps_realloc(void *ptr, size_t size, uint32_t caps);
void heap_caps_free(void *ptr);


#ifdef __cplusplus
}
#endif

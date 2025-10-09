/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void *(*malloc)(size_t size);
    void *(*calloc)(size_t nmemb, size_t size);
    void *(*align_malloc)(size_t align, size_t size);
    void *(*nocache_malloc)(size_t size);
    void (*free)(void *ptr);
} wifi_manager_mem_ops_t;


#ifdef __cplusplus
}
#endif

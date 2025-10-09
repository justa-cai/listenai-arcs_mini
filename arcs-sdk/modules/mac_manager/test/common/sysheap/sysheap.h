/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *exram_malloc(int heap_id, size_t size);

void exram_free(void *ptr);


#ifdef __cplusplus
}
#endif

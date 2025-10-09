/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_heap_caps.h"
#include "fff.h"

#ifdef __cplusplus
extern "C" {
#endif

DECLARE_FAKE_VALUE_FUNC(void *, heap_caps_malloc, size_t, uint32_t);
DECLARE_FAKE_VALUE_FUNC(void *, heap_caps_calloc, size_t, size_t, uint32_t);
DECLARE_FAKE_VALUE_FUNC(void *, heap_caps_aligned_alloc, size_t, size_t, uint32_t);
DECLARE_FAKE_VALUE_FUNC(void *, heap_caps_realloc, void *, size_t, uint32_t);
DECLARE_FAKE_VOID_FUNC(heap_caps_free, void *);


void mock_esp_heap_caps_init();
void mock_esp_heap_caps_reset();


#ifdef __cplusplus
}
#endif

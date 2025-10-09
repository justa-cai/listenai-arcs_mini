/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "sysheap.h"

#include "fff.h"

#ifdef __cplusplus
extern "C" {
#endif

DECLARE_FAKE_VALUE_FUNC(void *, exram_malloc, int, size_t);
DECLARE_FAKE_VOID_FUNC(exram_free, void *);

void mock_sysheap_init(void);
void mock_sysheap_reset(void);

#ifdef __cplusplus
}
#endif


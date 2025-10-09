/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "lisa_kv.h"

#include "fff.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

DECLARE_FAKE_VOID_FUNC(lisa_kv_free, void *);
DECLARE_FAKE_VALUE_FUNC(int, lisa_kv_get_blob, const char *, uint8_t **, int *);
DECLARE_FAKE_VALUE_FUNC(int, lisa_kv_set_blob, const char *, uint8_t *, int);
DECLARE_FAKE_VALUE_FUNC(int, lisa_kv_del, const char *);
DECLARE_FAKE_VALUE_FUNC(int, lisa_kv_get_int, const char *, int *);
DECLARE_FAKE_VALUE_FUNC(int, lisa_kv_set_int, const char *, int);

void mock_lisa_kv_init(void);
void mock_lisa_kv_reset(void);

#ifdef __cplusplus
}
#endif

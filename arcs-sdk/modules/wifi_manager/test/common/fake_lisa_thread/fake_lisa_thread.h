/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "lisa_thread.h"
#include "fff.h"

#ifdef __cplusplus
extern "C" {
#endif


DECLARE_FAKE_VALUE_FUNC(lisa_thread_t *, lisa_thread_create, const lisa_thread_attr_t *, lisa_thread_entry_t, void *);

DECLARE_FAKE_VALUE_FUNC(lisa_err_t, lisa_thread_delete, lisa_thread_t *);

void fake_lisa_thread_init(void);

void fake_lisa_thread_reset(void);



#ifdef __cplusplus
}
#endif
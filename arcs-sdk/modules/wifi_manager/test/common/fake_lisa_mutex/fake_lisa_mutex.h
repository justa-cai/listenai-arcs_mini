/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "fff.h"
#include "lisa_err.h"

#include "lisa_mutex.h"

#ifdef __cplusplus
extern "C" {
#endif

DECLARE_FAKE_VALUE_FUNC(lisa_mutex_t*, lisa_mutex_create);
DECLARE_FAKE_VALUE_FUNC(lisa_err_t, lisa_mutex_lock, lisa_mutex_t *, int32_t);
DECLARE_FAKE_VALUE_FUNC(lisa_err_t, lisa_mutex_unlock, lisa_mutex_t *);
DECLARE_FAKE_VALUE_FUNC(lisa_err_t, lisa_mutex_delete, lisa_mutex_t *);

void fake_lisa_mutex_init(void);
void fake_lisa_mutex_reset(void);



#ifdef __cplusplus
}
#endif
/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "lisa_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	void *handle;
	bool locked;
} lisa_mutex_t;

lisa_mutex_t * lisa_mutex_create();
lisa_err_t lisa_mutex_lock(lisa_mutex_t *mutex, int32_t block_time);
lisa_err_t lisa_mutex_unlock(lisa_mutex_t *mutex);
lisa_err_t lisa_mutex_delete(lisa_mutex_t *mutex);


#ifdef __cplusplus
}
#endif

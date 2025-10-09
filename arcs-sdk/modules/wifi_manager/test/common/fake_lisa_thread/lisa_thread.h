/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "lisa_err.h"
#include "fff.h"

#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct LisaThreadArg {
	void (*fn)(void *);
	void *arg;
} LisaThreadArg;

typedef struct {
	uint8_t *name;
	uint32_t stack_size;
	uint32_t priority;
} lisa_thread_attr_t;

typedef struct {
	pthread_t handle;
} lisa_thread_t;

typedef void (*lisa_thread_entry_t)(void *);

lisa_thread_t *lisa_thread_create(const lisa_thread_attr_t *, lisa_thread_entry_t, void *arg);
lisa_err_t lisa_thread_delete(lisa_thread_t *thread);

#ifdef __cplusplus
}
#endif

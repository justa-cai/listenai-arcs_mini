/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "console.h"
#include <stddef.h>

static const console_backend_t *console_be = NULL;

#if CONFIG_MODULE_FREERTOS
#include "FreeRTOS.h"
#include "semphr.h"
static SemaphoreHandle_t console_mutex = NULL;

void console_init(void)
{
}

static inline void console_lock(void)
{
}

static inline void console_unlock(void)
{
}
#else
void console_init(void) {}
static inline void console_lock(void) {}
static inline void console_unlock(void) {}
#endif

void console_backend_register(const console_backend_t *backend)
{
    if (backend) {
        console_be = backend;
    }
}

int console_write(const char *data, int len)
{
    if (!console_be || !console_be->write) {
        return -1;
    }
    console_lock();
    int ret = console_be->write(data, len);
    console_unlock();
    return ret;
}

int console_read(char *data, int len)
{
    if (!console_be || !console_be->read) {
        return -1;
    }
    return console_be->read(data, len);
}

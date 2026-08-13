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
#include "task.h"
static SemaphoreHandle_t console_mutex = NULL;

#define CONSOLE_CAN_LOCK() \
    (console_mutex != NULL && \
     xTaskGetSchedulerState() == taskSCHEDULER_RUNNING && \
     !(xPortIsInsideInterrupt() || xPortIsInsideCritical()))

void console_init(void)
{
    if (console_mutex == NULL) {
        console_mutex = xSemaphoreCreateRecursiveMutex();
    }
}

static inline void console_lock(void)
{
    if (CONSOLE_CAN_LOCK()) {
        xSemaphoreTakeRecursive(console_mutex, portMAX_DELAY);
    }
}

static inline void console_unlock(void)
{
    if (CONSOLE_CAN_LOCK()) {
        xSemaphoreGiveRecursive(console_mutex);
    }
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

void console_flush(void)
{
    if (console_be && console_be->flush) {
        console_be->flush();
    }
}

int console_read(char *data, int len)
{
    if (!console_be || !console_be->read) {
        return -1;
    }
    return console_be->read(data, len);
}

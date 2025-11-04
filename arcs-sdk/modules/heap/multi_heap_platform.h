/*
 * SPDX-FileCopyrightText: 2015-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#define MULTI_HEAP_FREERTOS 1// by danny


#ifdef MULTI_HEAP_FREERTOS

#include "FreeRTOS.h"

#include "autoconf.h"
// #include "esp_rom_sys.h"
#include <assert.h>

// typedef portMUX_TYPE multi_heap_lock_t;
typedef uint32_t multi_heap_lock_t;

/* Because malloc/free can happen inside an ISR context,
   we need to use portmux spinlocks here not RTOS mutexes */
#define MULTI_HEAP_LOCK(PLOCK) portENTER_CRITICAL()
#define MULTI_HEAP_UNLOCK(PLOCK) portEXIT_CRITICAL()
#define MULTI_HEAP_LOCK_INIT(PLOCK)         {}
#define MULTI_HEAP_LOCK_STATIC_INITIALIZER  0

/* Not safe to use std i/o while in a portmux critical section,
   can deadlock, so we use the ROM equivalent functions. */
extern int printk(const char *__restrict fmt, ...);
#define MULTI_HEAP_PRINTF printk
#define MULTI_HEAP_STDERR_PRINTF(MSG, ...) printk(MSG, __VA_ARGS__)

inline static void multi_heap_assert(bool condition, const char *format, int line, intptr_t address)
{
    /* Can't use libc assert() here as it calls printf() which can cause another malloc() for a newlib lock.

       Also, it's useful to be able to print the memory address where corruption was detected.
    */
#ifndef NDEBUG
    if(!condition) {
#ifndef CONFIG_COMPILER_OPTIMIZATION_ASSERTIONS_SILENT
        printk(format, line, address);
#endif  // CONFIG_COMPILER_OPTIMIZATION_ASSERTIONS_SILENT
        abort();
    }
#else // NDEBUG
    (void) condition;
#endif // NDEBUG
}

#define MULTI_HEAP_ASSERT(CONDITION, ADDRESS) \
    multi_heap_assert((CONDITION), "CORRUPT HEAP: multi_heap.c:%d detected at 0x%08x\n", \
                      __LINE__, (intptr_t)(ADDRESS))

#ifdef CONFIG_HEAP_TASK_TRACKING
#include <freertos/task.h>
#define MULTI_HEAP_SET_BLOCK_OWNER(HEAD) *((TaskHandle_t*)HEAD) = xTaskGetCurrentTaskHandle()
#define MULTI_HEAP_GET_BLOCK_OWNER(HEAD) *((TaskHandle_t*)HEAD)
#define MULTI_HEAP_ADD_BLOCK_OWNER_OFFSET(HEAD) ((TaskHandle_t*)(HEAD) + 1)
#define MULTI_HEAP_REMOVE_BLOCK_OWNER_OFFSET(HEAD) ((TaskHandle_t*)(HEAD) - 1)
#define MULTI_HEAP_ADD_BLOCK_OWNER_SIZE(SIZE) ((SIZE) + sizeof(TaskHandle_t))
#define MULTI_HEAP_REMOVE_BLOCK_OWNER_SIZE(SIZE) ((SIZE) - sizeof(TaskHandle_t))
#define MULTI_HEAP_BLOCK_OWNER_SIZE() sizeof(TaskHandle_t)
#else
#define MULTI_HEAP_SET_BLOCK_OWNER(HEAD)
#define MULTI_HEAP_GET_BLOCK_OWNER(HEAD) (NULL)
#define MULTI_HEAP_ADD_BLOCK_OWNER_OFFSET(HEAD) (HEAD)
#define MULTI_HEAP_REMOVE_BLOCK_OWNER_OFFSET(HEAD) (HEAD)
#define MULTI_HEAP_ADD_BLOCK_OWNER_SIZE(SIZE) (SIZE)
#define MULTI_HEAP_REMOVE_BLOCK_OWNER_SIZE(SIZE) (SIZE)
#define MULTI_HEAP_BLOCK_OWNER_SIZE() 0
#endif // CONFIG_HEAP_TASK_TRACKING

#else // MULTI_HEAP_FREERTOS

#include <assert.h>

#define MULTI_HEAP_PRINTF printf
#define MULTI_HEAP_STDERR_PRINTF(MSG, ...) fprintf(stderr, MSG, __VA_ARGS__)
#define MULTI_HEAP_LOCK(PLOCK)  (void) (PLOCK)
#define MULTI_HEAP_UNLOCK(PLOCK)  (void) (PLOCK)
#define MULTI_HEAP_LOCK_INIT(PLOCK)  (void) (PLOCK)
#define MULTI_HEAP_LOCK_STATIC_INITIALIZER  0

#define MULTI_HEAP_ASSERT(CONDITION, ADDRESS) assert((CONDITION) && "Heap corrupt")

#define MULTI_HEAP_BLOCK_OWNER
#define MULTI_HEAP_SET_BLOCK_OWNER(HEAD)
#define MULTI_HEAP_GET_BLOCK_OWNER(HEAD) (NULL)

#endif // MULTI_HEAP_FREERTOS

/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "venusa_ap.h"

#include <stdio.h>
#include <stdbool.h>

#if CONFIG_MODULE_FREERTOS

#include "FreeRTOS.h"
#include "task.h"

#if configUSE_IDLE_HOOK
void vApplicationIdleHook(void)
{
    __WFI();
}
#endif

#if configUSE_MALLOC_FAILED_HOOK
void vApplicationMallocFailedHook(void)
{
    printf("MallocFailed@%s\r\n", xPortIsInsideInterrupt() ? "<ISR>" : pcTaskGetName(xTaskGetCurrentTaskHandle()));
    __builtin_trap();
}
#endif

#if configCHECK_FOR_STACK_OVERFLOW
void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    printf("StackOverflow@%s\r\n", name);
    __builtin_trap();
}
#endif

#if configUSE_TICK_INITAL_HOOK
TickType_t portTickInitalHook(TickType_t tick)
{
    return tick;
}
#endif

#endif

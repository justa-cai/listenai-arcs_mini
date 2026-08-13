/*
 * Copyright (c) 2026, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 *
 * C glue for the arcs-embassy crate: exposes the FreeRTOS macros / inline
 * helpers that the Rust embassy integration needs as plain linkable symbols.
 * Compiled as part of the consuming sample (so it sees the full SDK / FreeRTOS
 * include path + autoconf.h). Add to the sample's target_sources.
 */
#include "arcs_embassy_glue.h"

#include "FreeRTOS.h"
#include "task.h"

/* Monotonic 64-bit microsecond clock (1 MHz SysTimer). */
uint64_t arcs_embassy_now_us(void)
{
    return ulPortGetRunTimeCounterValue();
}

/* critical-section: raise/restore the FreeRTOS interrupt mask (ECLIC MTH). */
uint8_t arcs_embassy_cs_acquire(void)
{
    return ulPortRaiseBASEPRI();
}

void arcs_embassy_cs_release(uint8_t state)
{
    vPortSetBASEPRI(state);
}

/* Handle of the task running the executor (used as the pender context). */
void *arcs_embassy_cur_task(void)
{
    return (void *)xTaskGetCurrentTaskHandle();
}

/* Wake the executor task (the embassy "pender"). Task-context only. */
void arcs_embassy_notify(void *task)
{
    xTaskNotifyGive((TaskHandle_t)task);
}

/* Block the executor task until notified or `ticks` (1 ms) elapse. */
uint32_t arcs_embassy_notify_take(uint32_t clear, uint32_t ticks)
{
    return ulTaskNotifyTake(clear ? pdTRUE : pdFALSE, (TickType_t)ticks);
}

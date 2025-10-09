/**
 ****************************************************************************************
 *
 * @file sys_arch.c
 *
 * @brief Implementation of simulate socket
 *
 * Copyright (C) ListenAI 2020-2023
 *
 * Created on: Nov 11, 2023
 *
 *
 ****************************************************************************************
 */

#include <stdio.h>
#include <rtos_def.h>
#include <rtos_al.h>
#include "sys_arch.h"

#define SYS_STATS_INC(x)
#define SYS_STATS_DEC(x)
#define SYS_STATS_INC_USED(x)
#define SYS_STATS_DISPLAY()

#ifndef LWIP_FREERTOS_SYS_ARCH_PROTECT_USES_MUTEX
#define LWIP_FREERTOS_SYS_ARCH_PROTECT_USES_MUTEX     1
#endif

#if LWIP_FREERTOS_SYS_ARCH_PROTECT_USES_MUTEX
static SemaphoreHandle_t sys_arch_protect_mutex;
#endif

uint32_t sys_now(void)
{
    return rtos_now(0);
}

uint32_t sys_arch_sem_wait(rtos_semaphore *pxSemaphore, uint32_t ulTimeOut)
{
    uint32_t xStartTime;
    unsigned long ulReturn;
    int timeout;

    xStartTime = sys_now();

    if (ulTimeOut == 0)
        timeout = -1;
    else
        timeout = ulTimeOut;

    if (rtos_semaphore_wait(*pxSemaphore, timeout))
    {
        /* Timed out. */
        ulReturn = SYS_ARCH_TIMEOUT;
    }
    else
    {
        ulReturn = sys_now() - xStartTime;
    }

    return ulReturn;
}

int32_t sys_sem_new(rtos_semaphore *pxSemaphore, uint8_t ucCount)
{
    int32_t xReturn = -1;

    if (rtos_semaphore_create(pxSemaphore, 1, ucCount) == 0)
    {
        xReturn = 0;
        SYS_STATS_INC_USED( sem );
    }
    else
    {
        SYS_STATS_INC( sem.err );
    }

    return xReturn;
}

void sys_sem_free(rtos_semaphore *pxSemaphore)
{
    SYS_STATS_DEC(sem.used);
    rtos_semaphore_delete(*pxSemaphore);
}

void sys_sem_signal(rtos_semaphore *pxSemaphore)
{
    rtos_semaphore_signal(*pxSemaphore, 0);
}

void sys_init(void)
{
#if LWIP_FREERTOS_SYS_ARCH_PROTECT_USES_MUTEX
    sys_arch_protect_mutex = xSemaphoreCreateMutex();
#endif
}

uint32_t sys_arch_protect(void)
{
#if LWIP_FREERTOS_SYS_ARCH_PROTECT_USES_MUTEX
    xSemaphoreTake(sys_arch_protect_mutex, portMAX_DELAY);
#else
    taskENTER_CRITICAL();
#endif
    return 0;
}

void sys_arch_unprotect(void)
{
#if LWIP_FREERTOS_SYS_ARCH_PROTECT_USES_MUTEX
    xSemaphoreGive(sys_arch_protect_mutex);
#else
    taskEXIT_CRITICAL();
#endif
}

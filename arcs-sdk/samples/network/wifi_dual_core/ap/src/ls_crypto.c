/*
 * ls_crypto.c
 *
 *  Created on: 2024-11-7
 *  
 *      Author: zyyang
 *
 */

/*
 * INCLUDES
 ****************************************************************************************
 */

#include "log_print.h"
#include "Driver_CRYPTO.h"
#include "rtos_al.h"
#if CONFIG_ARCS_HAL_IC_MUTEX
#include "ic_mutex.h"
#endif

#if CONFIG_ARCS_HAL_IC_MUTEX
static IC_Mutex crypto_ic_mutex;
#endif

void* CRYPTO0_Handler = NULL;

static void CRYPTO_Init_Handler(void){
    CRYPTO0_Handler = CRYPTO0();
}

static volatile int32_t CRYPTO_Result = CSK_DRIVER_OK;
#ifdef CFG_RTOS
#include "FreeRTOS.h"
#include <semphr.h>

static SemaphoreHandle_t CRYPTO_res_semaphore;
static SemaphoreHandle_t CRYPTO_tsk_semaphore;
static int32_t CRYPTO_EventCallback_RTOS(uint32_t event, int32_t result, void* workspace){
    //CLOGD("CRYPTO_EventCallback_RTOS, event:%d", event);
    if(CSK_CRYPTO_EVENT_WAIT_BUSY == event)
    {
    #if CONFIG_ARCS_HAL_IC_MUTEX
        IC_Mutex_acquire(&crypto_ic_mutex);
    #else
        // wait other procedure finish
        xSemaphoreTake(CRYPTO_res_semaphore, -1);
    #endif
    }
    else if(CSK_CRYPTO_EVENT_FINISHED == event)
    {
    #if CONFIG_ARCS_HAL_IC_MUTEX
        IC_Mutex_release(&crypto_ic_mutex);
    #else
        xSemaphoreGive(CRYPTO_res_semaphore);
    #endif
    }
    else if(CSK_CRYPTO_EVENT_WAIT_DONE == event)
    {
        xSemaphoreTake(CRYPTO_tsk_semaphore, -1);
        return CRYPTO_Result;
    }
    else if(CSK_CRYPTO_EVENT_DONE == event)
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        CRYPTO_Result = result;
        xSemaphoreGiveFromISR(CRYPTO_tsk_semaphore, &xHigherPriorityTaskWoken);
    }

    return CSK_DRIVER_OK;
}
#else
static volatile uint32_t CRYPTO_BUSY = 0;
static volatile uint32_t CRYPTO_DONE = 0;
static int32_t CRYPTO_EventCallback_NOS(uint32_t event, int32_t result, void* workspace)
{
    if(CSK_CRYPTO_EVENT_WAIT_BUSY == event)
    {
        // wait other procedure finish
        while(CRYPTO_BUSY);
        CRYPTO_BUSY = 1;
    }
    else if(CSK_CRYPTO_EVENT_FINISHED == event)
    {
        CRYPTO_BUSY = 0;
    }
    else if(CSK_CRYPTO_EVENT_WAIT_DONE == event)
    {
        while(!CRYPTO_DONE);
        CRYPTO_DONE = 0;
        return CRYPTO_Result;
    }
    else if(CSK_CRYPTO_EVENT_DONE == event)
    {
        CRYPTO_Result = result;
        CRYPTO_DONE = 1;
    }

    return CSK_DRIVER_OK;
}
#endif

void ls_crypto_init(void)
{

#ifdef CFG_RTOS
    CRYPTO_res_semaphore = xSemaphoreCreateBinary();
    CRYPTO_tsk_semaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(CRYPTO_res_semaphore);
    //xSemaphoreGive(CRYPTO_tsk_semaphore);
#endif
    CRYPTO_Init_Handler();
#ifndef CFG_RTOS
    CRYPTO_Initialize(CRYPTO0_Handler, CRYPTO_EventCallback_NOS, NULL);
#else
    CRYPTO_Initialize(CRYPTO0_Handler, CRYPTO_EventCallback_RTOS, NULL);
#endif

#if CONFIG_ARCS_HAL_IC_MUTEX
    IC_Mutex_init(&crypto_ic_mutex, IC_MUTEX_SLEEP_WAIT, IC_MUTEX_ID_CRYPTO);
#endif
}

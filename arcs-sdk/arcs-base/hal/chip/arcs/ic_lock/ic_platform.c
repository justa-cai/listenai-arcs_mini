/*
 * Inter-cores synchronization primitives for multiprocessor system.
 * Copyright 2024 ListenAI
 */

#include "FreeRTOS.h"
#include "task.h"
#include "Driver_MBX.h"
#include "ic_platform.h"

static TaskHandle_t IC_Mutex_TaskToNotify[IC_MUTEX_CHANNEL_MAX];


/* Interrupt handler for the IC_mutex interrupt. Notifies the task waiting.
 *
 * arg : unused
 */
static int8_t IC_Mutex_interruptHandler(uint32_t event, uint32_t channel)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    channel -= IC_MUTEX_IRQ_OFFSET;
    if (IC_Mutex_TaskToNotify[channel] != NULL) {
        /* Notify the task. */
        vTaskNotifyGiveFromISR( IC_Mutex_TaskToNotify[channel], &xHigherPriorityTaskWoken );

        // 唤醒后 重置 task handle
        IC_Mutex_TaskToNotify[channel] = NULL;

        /* If xHigherPriorityTaskWoken is now set to pdTRUE then a context switch
        should be performed to ensure the interrupt returns directly to the highest
        priority task. The macro used for this purpose is dependent on the port in
        use and may be called portEND_SWITCHING_ISR(). */
        portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
    }

    return 0;
}

void IC_Mutex_set_task_handle(uint32_t channel, TaskHandle_t task)
{
    if (channel < IC_MUTEX_CHANNEL_MAX){
        IC_Mutex_TaskToNotify[channel] = task;
    }
}

/* Use mailbox channel MBX_MUTEX_CHANNEL(14) for inter-cores wakeup interrupt.
 */
void IC_Mutex_linkInterrupt(void)
{
    // mbox->互斥量
    // 挂接中断处理
    MBX1_Initialize(NULL, IC_Mutex_interruptHandler);
}

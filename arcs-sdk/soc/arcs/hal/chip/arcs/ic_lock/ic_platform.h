/*
 * Inter-cores synchronization primitives for multiprocessor system.
 * Copyright 2024 ListenAI
 */
#ifndef __IC_PLATFORM_H__
#define __IC_PLATFORM_H__

#include <stdint.h>
#include <stddef.h>
#include "FreeRTOS.h"
#include "task.h"
#include "Driver_MBX.h"

#define AP_PROCESSOR_ID             0
#define CP_PROCESSOR_ID             1
#define IC_MUTEX_CHANNEL_MAX        8
#define IC_MUTEX_IRQ_OFFSET         8
#define IC_MUTEX_CHANNEL(mutex)     mutex


extern void IC_Mutex_linkInterrupt(void);
extern void IC_Mutex_set_task_handle(uint32_t channel, TaskHandle_t task);


/*!
 * Returns the id of this processor
 *
 * \return The id of this processor
 */
__attribute__((unused)) static int
IC_get_my_pid()
{
#ifdef MBX_AP_WORK
  return AP_PROCESSOR_ID;
#elif defined(MBX_CP_WORK)
  return CP_PROCESSOR_ID;
#endif
}

/*!
 * Notify target processor with the default inter-processor interrupt
 *
 * \param pid The target processor to notify
 */
__attribute__((unused)) static inline void
IC_proc_notify(uint32_t channel)
{
    MBX_Trigger(NULL, (channel + IC_MUTEX_IRQ_OFFSET));
}

__attribute__((unused)) static inline void
IC_disable_interrupts(uint32_t channel)
{
    vPortEnterCritical();
    MBX_DisableInterrupt_l(NULL, channel + IC_MUTEX_IRQ_OFFSET);
    vPortExitCritical();
}

__attribute__((unused)) static inline void
IC_enable_interrupts(uint32_t channel)
{
    vPortEnterCritical();
    MBX_EnableInterrupt_l(NULL, channel + IC_MUTEX_IRQ_OFFSET);
    vPortExitCritical();
}

__attribute__((unused)) static inline uint32_t
IC_get_my_thread_id()
{
  return (uint32_t)xTaskGetCurrentTaskHandle();
}

__attribute__((unused)) static inline void
IC_disable_preemption()
{
  vTaskSuspendAll();
}

__attribute__((unused)) static inline void
IC_enable_preemption()
{
  xTaskResumeAll();
}

#endif /* __IC_PLATFORM_H__ */

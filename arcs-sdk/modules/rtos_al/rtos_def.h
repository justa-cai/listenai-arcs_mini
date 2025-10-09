/**
 ****************************************************************************************
 *
 * @file rtos_def.h
 *
 * @brief Declaration of FreeRTOS abstraction layer.
 *
 * Copyright (C) ListenAI  2024-2025
 *
 ****************************************************************************************
 */

#ifndef RTOS_DEF_H_
#define RTOS_DEF_H_

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"

/*
 * DEFINITIONS
 ****************************************************************************************
 */
/// RTOS task handle
typedef TaskHandle_t        rtos_task_handle;

/// RTOS priority
typedef UBaseType_t         rtos_prio;

/// RTOS task function
typedef TaskFunction_t      rtos_task_fct;

/// RTOS queue
typedef QueueHandle_t       rtos_queue;

/// RTOS semaphore
typedef SemaphoreHandle_t   rtos_semaphore;

/// RTOS mutex
typedef SemaphoreHandle_t   rtos_mutex;

/// RTOS event
typedef EventGroupHandle_t  rtos_event;
typedef EventBits_t         rtos_event_bit;
typedef BaseType_t          rtos_base_type;
typedef TimerHandle_t       rtos_timer;

typedef StaticTask_t        rtos_static_task_tcb;
typedef StackType_t         rtos_stack_type;
/*
 * MACROS
 ****************************************************************************************
 */
/// Macro building the prototype of a RTOS task function
#define RTOS_TASK_FCT(name)        portTASK_FUNCTION(name, env)

/// Macro building a task priority as an offset of the IDLE task priority
#define RTOS_TASK_PRIORITY(prio)  (tskIDLE_PRIORITY + (prio))

/// Macro defining a null RTOS task handle
#define RTOS_TASK_NULL             NULL

#endif // RTOS_DEF_H_

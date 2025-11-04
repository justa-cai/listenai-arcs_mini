/**
 ****************************************************************************************
 * @file btos_def.h
 *
 * @brief  BT Rtos Define
 *
 * Copyright (C) Listenai 2023
 *
 ****************************************************************************************
 */


#ifndef BTOS_DEF_H_
#define BTOS_DEF_H_

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "timers.h"

/*
 * DEFINITIONS
 ****************************************************************************************
 */
 
/// Message identifier index
#define OS_TASK_FIRST_MSG(task) ((uint16_t)((task) << 8))
#define OS_MSG_ID(task, idx) (OS_TASK_FIRST_MSG((OS_TASK_ID_ ## task)) + idx)

/// RTOS task handle
typedef TaskHandle_t        btos_task_handle;

/// RTOS priority
typedef UBaseType_t         btos_prio;

/// RTOS task function
typedef TaskFunction_t      btos_task_fct;

/// RTOS queue
typedef QueueHandle_t       btos_queue;

/// RTOS semaphore
typedef SemaphoreHandle_t   btos_semaphore;

/// RTOS mutex
typedef SemaphoreHandle_t   btos_mutex;
/*
 * MACROS
 ****************************************************************************************
 */
/// Macro building the prototype of a RTOS task function
#define BTOS_TASK_FCT(name)        portTASK_FUNCTION(name, env)

/// Macro building a task priority as an offset of the IDLE task priority
#define BTOS_TASK_PRIORITY(prio)  (tskIDLE_PRIORITY + (prio))

/// Macro defining a null RTOS task handle
#define BTOS_TASK_NULL             NULL
/// Macro defining max delay for task
#define BTOS_TASK_MAX_DELAY        portMAX_DELAY
/// Macro defining max delay for task
#define BTOS_TASK_NO_DELAY         (0)

#endif // RTOS_DEF_H_

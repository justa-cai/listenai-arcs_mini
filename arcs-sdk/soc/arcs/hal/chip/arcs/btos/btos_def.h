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

/**
 * btos task identifier
 */
#ifndef __ARRAY_EMPTY
#define __ARRAY_EMPTY
#endif

#define OS_TASK_PRIORITY_BASE    (7)


typedef enum
{
    OS_TASK_ID_IDLE = 0,
    OS_TASK_ID_APP,
    OS_TASK_ID_BT,
    OS_TASK_ID_AUD,
    OS_TASK_ID_AUD_PRO,
    
    OS_TASK_ID_SHELL,
    OS_TASK_ID_TOTAL
}btos_task_id;
    
typedef struct btos_msg
{
    uint16_t        msg_id;
    uint16_t        param_len;
    uint8_t         param[__ARRAY_EMPTY];
}btos_msg_t;

typedef struct btos_msg_isr
{
    uint16_t        msg_id;
    uint16_t        param_len;
    uint8_t         param[4];
}btos_msg_isr_t;

typedef struct btos_event
{
    btos_msg_t      *msg_body;
}btos_event_t;

typedef struct btos_handle
{
    btos_task_id  taskid;
    btos_task_fct func;
    void          *name;
    uint16_t      stack_size;
    btos_prio     stack_prio;
    void          *queue;
    void          *task_handle;
    uint8_t       sole;
} btos_handle_t;

typedef enum
{
    TIMER_TYPE_IDLE,
    TIMER_TYPE_SINGLE,
    TIMER_TYPE_PERIODIC,
} timer_type_t;
    

#endif // RTOS_DEF_H_

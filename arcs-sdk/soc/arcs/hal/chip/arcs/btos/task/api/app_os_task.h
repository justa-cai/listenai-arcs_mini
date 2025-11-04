/**
 ****************************************************************************************
 *
 * @file app_os_task.h
 *
 * @brief Header file - APP OS TASK.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

#ifndef APP_OS_TASK_H_
#define APP_OS_TASK_H_

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "btos_al.h"
#include "os_task_init.h"

/*
 * DEFINES
 ****************************************************************************************
 */
#define APP_OS_TASK_STACK_SIZE      (256/2)
#define APP_OS_TASK_PRIORITY        (OS_TASK_PRIORITY_BASE+1)
#define APP_OS_TASK_NAME            "APP Task"


/*
 * ENUMERATIONS
 ****************************************************************************************
 */
enum app_os_msg_id
{
    /* Default event */
    /// Read Data Complete event
    APP_START_EVT                                        = OS_MSG_ID(APP, 0x00),
};

/*
 * GLOBAL VARIABLE DECLARATIONS
 ****************************************************************************************
 */
 
void app_os_init(os_task_cb_t *cb);
void app_os_task(void *args);

/// @} APP OS TASK
#endif // APP_OS_TASK_H_

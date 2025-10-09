/**
 ****************************************************************************************
 *
 * @file shell_os_task.h
 *
 * @brief Header file - SHELL OS TASK.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

#ifndef SHELL_OS_TASK_H_
#define SHELL_OS_TASK_H_

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "btos_al.h"
/*
 * DEFINES
 ****************************************************************************************
 */
#define SHELL_OS_TASK_STACK_SIZE      (128)
#define SHELL_OS_TASK_PRIORITY        (1)
#define SHELL_OS_TASK_NAME            "SHELL Task"


/*
 * ENUMERATIONS
 ****************************************************************************************
 */
enum shell_os_msg_id
{
    /* Default event */
    /// Read Data Complete event
    SHELL_READ_DATA_CMP_EVT                                        = OS_MSG_ID(SHELL, 0x00),
};

/*
 * GLOBAL VARIABLE DECLARATIONS
 ****************************************************************************************
 */
void shell_os_task(void *args);

/// @} AUD OS TASK
#endif // AUD_OS_TASK_H_


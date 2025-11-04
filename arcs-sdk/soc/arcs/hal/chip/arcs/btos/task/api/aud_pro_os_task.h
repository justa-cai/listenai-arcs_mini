/**
 ****************************************************************************************
 *
 * @file AUD_PRO_OS_task.h
 *
 * @brief Header file - AUDIO OS TASK.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

#ifndef AUD_PRO_OS_TASK_H_
#define AUD_PRO_OS_TASK_H_

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
#define AUD_PRO_OS_TASK_STACK_DEFAULT_SIZE      (1024+512)
#define AUD_PRO_OS_TASK_DEFAULT_PRIORITY        (OS_TASK_PRIORITY_BASE+1)
#define AUD_PRO_OS_TASK_NAME                    "AUD PRO Task"


/*
 * ENUMERATIONS
 ****************************************************************************************
 */
enum aud_pro_os_msg_id
{
    /* Aud event */
    ///
    AUD_PRO_OS_DEFAULT_EVT                                        = OS_MSG_ID(AUD_PRO, 0x00),

};

/*
 * GLOBAL VARIABLE DECLARATIONS
 ****************************************************************************************
 */
void aud_pro_os_init(os_task_cb_t *cb);
void aud_pro_os_task(void *args);

/// @} AUD OS TASK
#endif // AUD_PRO_OS_TASK_H_


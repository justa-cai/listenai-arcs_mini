/**
 ****************************************************************************************
 *
 * @file aud_os_task.h
 *
 * @brief Header file - AUDIO OS TASK.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

#ifndef AUD_OS_TASK_H_
#define AUD_OS_TASK_H_

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
#define AUD_OS_TASK_STACK_DEFAULT_SIZE      (512)
#define AUD_OS_TASK_DEFAULT_PRIORITY        (OS_TASK_PRIORITY_BASE+1)
#define AUD_OS_TASK_NAME                    "AUD Task"


/*
 * ENUMERATIONS
 ****************************************************************************************
 */
enum aud_os_msg_id
{
    /* Aud event */
    AUD_OS_START_EVT                                        = OS_MSG_ID(AUD, 0x00),
    AUD_OS_STOP_EVT                                         = OS_MSG_ID(AUD, 0x01),
    AUD_OS_PAUSE_EVT                                        = OS_MSG_ID(AUD, 0x02),
    AUD_OS_RESUME_EVT                                       = OS_MSG_ID(AUD, 0x03),
    AUD_OS_MEIDA_INFO_EVT                                   = OS_MSG_ID(AUD, 0x04),

    AUD_OS_RCV_DATA_EVT                                     = OS_MSG_ID(AUD, 0x10),
};

/*
 * GLOBAL VARIABLE DECLARATIONS
 ****************************************************************************************
 */
void aud_os_init(os_task_cb_t *cb);
void aud_os_task(void *args);

/// @} AUD OS TASK
#endif // AUD_OS_TASK_H_


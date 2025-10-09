/**
 ****************************************************************************************
 *
 * @file os_task_init.c
 *
 * @brief APP Task implementation
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup OS TASK INIT
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "os_task_init.h"
#include "os_task_config.h"

#include "btos_al.h"
#include <string.h>

#include <app_os_task.h>
#include <bt_os_task.h>
#include <aud_os_task.h>
#include <aud_pro_os_task.h>
#include <shell_os_task.h>

/*
 * EXPORTED FUNCTIONS DEFINITIONS
 ****************************************************************************************
 */


/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */

/*
 * GLOBAL VARIABLES DEFINITION
 ****************************************************************************************
 */
uint8_t os_task_init(uint8_t *args)
{
    ///create app os task.
    //btos_task_create(app_os_task, APP_OS_TASK_NAME, OS_TASK_ID_APP, APP_OS_TASK_STACK_SIZE, \
    //                   NULL, APP_OS_TASK_PRIORITY, NULL);
    ///create bt os task.
    btos_task_create(bt_os_task, BT_OS_TASK_NAME, OS_TASK_ID_BT, BT_OS_TASK_STACK_SIZE, \
                       NULL, BT_OS_TASK_PRIORITY, NULL);
    ///create aud os task.
    btos_task_create(aud_os_task, AUD_OS_TASK_NAME, OS_TASK_ID_AUD, AUD_OS_TASK_STACK_SIZE, \
                       NULL, AUD_OS_TASK_PRIORITY, NULL);
    ///create aud pro os task.
    btos_task_create(aud_pro_os_task, AUD_PRO_OS_TASK_NAME, OS_TASK_ID_AUD_PRO, AUD_PRO_OS_TASK_STACK_SIZE, \
                       NULL, AUD_PRO_OS_TASK_PRIORITY, NULL);
    ///create shell os task.
    //btos_task_create(shell_os_task, SHELL_OS_TASK_NAME, OS_TASK_ID_SHELL, SHELL_OS_TASK_STACK_SIZE, \
    //                   NULL, SHELL_OS_TASK_PRIORITY, NULL);
    return 0;
}
/// @} APPTASK

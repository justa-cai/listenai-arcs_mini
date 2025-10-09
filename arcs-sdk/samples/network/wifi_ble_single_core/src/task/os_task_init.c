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

#include <bt_os_task.h>


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
    ///create bt os task.
    btos_task_create(bt_os_task, BT_OS_TASK_NAME, OS_TASK_ID_BT, BT_OS_TASK_STACK_SIZE, \
                       NULL, BT_OS_TASK_PRIORITY, NULL);
    return 0;
}
/// @} APPTASK

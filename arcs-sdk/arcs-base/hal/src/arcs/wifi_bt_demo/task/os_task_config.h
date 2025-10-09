/**
 ****************************************************************************************
 *
 * @file os_task_config.h
 *
 * @brief Header file - Config task by user.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */


#ifndef OS_TASK_CONFIG_H_
#define OS_TASK_CONFIG_H_

/**
 ****************************************************************************************
 * @addtogroup APPTASK Task
 * @ingroup APP
 * @brief Routes ALL messages to/from APP block.
 *
 * The APPTASK is the block responsible for bridging the final application with the
 * BLE software host stack. It communicates with the different modules of the BLE host,
 * i.e. @ref SMP, @ref GAP and @ref GATT.
 *
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include <stdint.h>
#include "btos_al.h"

/*
 * DEFINES
 ****************************************************************************************
 */
///config stack by user.
/// bt task
#define BT_OS_TASK_STACK_SIZE         (4096/4)
#define BT_OS_TASK_PRIORITY           (OS_TASK_PRIORITY_BASE+2)
/// audio task
#define AUD_OS_TASK_STACK_SIZE        (512)
#define AUD_OS_TASK_PRIORITY          (OS_TASK_PRIORITY_BASE+1)
/// audio process task
#define AUD_PRO_OS_TASK_STACK_SIZE    (1024+512)
#define AUD_PRO_OS_TASK_PRIORITY      (OS_TASK_PRIORITY_BASE+1)

/*
 * ENUMERATIONS
 ****************************************************************************************
 */
/// Types of initialization of the IP

/*
 * GLOBAL VARIABLE DECLARATIONS
 ****************************************************************************************
 */

/// @} APPTASK


#endif // APP_TASK_H_

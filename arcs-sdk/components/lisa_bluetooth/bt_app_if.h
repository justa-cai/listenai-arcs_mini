/**
 ****************************************************************************************
 *
 * @file bt_app_if.h
 *
 * @brief Header file - BT STACK INTERFACE.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

#ifndef BT_APP_IF_H_
#define BT_APP_IF_H_

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "os_task_init.h"
/*
 * DEFINES
 ****************************************************************************************
*/
#if CONFIG_ARCS_HAL_AT_CMD
#define BT_AT_CMD_PRESENT   (1)
#else
#define BT_AT_CMD_PRESENT   (0)
#endif

/*
 * ENUMERATIONS
 ****************************************************************************************
 */


/*
 * GLOBAL VARIABLE DECLARATIONS
 ****************************************************************************************
 */
void app_user_bt_handler_init(void);

/// @} BT STACK
#endif // BT_BLE_IF_H_

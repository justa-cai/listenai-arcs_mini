/**
 ****************************************************************************************
 *
 * @file bt_classic_hal.h
 *
 * @brief Header file - BT STACK INTERFACE.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

#ifndef BT_CLASSIC_HAL_H_
#define BT_CLASSIC_HAL_H_

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "os_task_init.h"
#include "bt_os_task.h"

/*
 * DEFINES
 ****************************************************************************************
 */


/*
 * ENUMERATIONS
 ****************************************************************************************
 */

/*
 * GLOBAL VARIABLE DECLARATIONS
 ****************************************************************************************
 */
bool bt_stack_classic_connected(void);
void bt_stack_bt_inquiry(uint8_t disc_mode, uint8_t max_count);
void bt_stack_bt_scan(uint8_t scan_en);
void bt_stack_bt_connect(gap_bdaddr_t addr, uint8_t type, uint16_t clk_off, uint8_t page_scan_rep_mode);
void bt_stack_bt_connect_cancel(void);
void bt_stack_bt_inquiry_stop(void);

/// @} BT STACK
#endif // BT_CLASSIC_IF_H_

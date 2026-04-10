/**
 ****************************************************************************************
 *
 * @file bt_call_hal.h
 *
 * @brief Header file - BT STACK INTERFACE.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

#ifndef BT_CALL_HAL_H_
#define BT_CALL_HAL_H_

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "os_task_init.h"
#include "bt_hfp.h"
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
void bt_stack_hfp_enable(bt_hfp_cfg_t *cfg);
uint8_t bt_stack_hfp_send_start(uint8_t conidx, uint8_t codec);
void bt_stack_hfp_send_stop(uint8_t conidx, uint8_t status);
void bt_stack_hfp_send_data(uint8_t conidx, uint8_t pkt_sta, uint16_t len, uint8_t *data);
void bt_stack_hfp_connection_update(uint8_t conidx, bool connected);

/// @} BT STACK
#endif // BT_CALL_HAL_H_

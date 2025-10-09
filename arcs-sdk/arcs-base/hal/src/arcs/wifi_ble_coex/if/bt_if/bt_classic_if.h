/**
 ****************************************************************************************
 *
 * @file bt_stack_if.h
 *
 * @brief Header file - BT STACK INTERFACE.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

#ifndef BT_CLASSIC_IF_H_
#define BT_CLASSIC_IF_H_

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
 
void bt_stack_classic_enable_cmp(uint16_t status);
void bt_stack_classic_conn_ind(uint8_t conidx, uint16_t conhdl, gap_bdaddr_t *peer_addr);
void bt_stack_classic_disc_ind(uint8_t conidx, uint16_t conhdl, uint16_t reason);
void bt_stack_classic_bond_ind(uint8_t conidx, uint16_t status);
void bt_stack_classic_discover_ind(gap_bdaddr_t *peer_addr, uint16_t clk_off, int8_t rssi, uint8_t mode, uint32_t cod, struct gap_dev_name *name);
/// @} BT STACK
#endif // BT_CLASSIC_IF_H_

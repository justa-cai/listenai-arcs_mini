/**
 ****************************************************************************************
 *
 * @file bt_ble_if.h
 *
 * @brief Header file - BT STACK INTERFACE.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

#ifndef BT_BLE_IF_H_
#define BT_BLE_IF_H_

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "os_task_init.h"
/*
 * DEFINES
 ****************************************************************************************
 */
#define BLE_VOICE_SIMULATOR  (0)
/*
 * ENUMERATIONS
 ****************************************************************************************
 */


/*
 * GLOBAL VARIABLE DECLARATIONS
 ****************************************************************************************
 */
void bt_stack_ble_enable_cmp(uint16_t status);
void bt_stack_ble_conn_ind(uint8_t conidx, uint16_t conhdl, gap_bdaddr_t *peer_addr);
void bt_stack_ble_disc_ind(uint8_t conidx, uint16_t conhdl, uint16_t reason);
void bt_stack_ble_key_req(uint8_t conidx, uint8_t key_type, uint32_t key);
void bt_stack_ble_bond_ind(uint8_t conidx, uint16_t status);
void bt_stack_ble_info_ind(uint8_t conidx, uint8_t type, ble_info_data_t *data);
void bt_stack_ble_adv_report_ind(uint8_t flag, gap_bdaddr_t *peer_addr, int8_t rssi, uint8_t len, uint8_t *data);
void bt_stack_ble_para_update_ind(uint8_t conidx, uint16_t interval, uint16_t latency, uint16_t super_to);
void bt_stack_ble_add_paired_to_wlist(void);
void bt_stack_ble_scan_start(uint8_t scan_id, uint8_t type, uint8_t phy, uint16_t scan_intv, uint16_t scan_win);
void bt_stack_ble_scan_stop(uint8_t scan_id);
void bt_stack_ble_pre_sync_start(uint8_t type, gap_per_adv_bdaddr_t *adv_addr, uint8_t report_en, uint8_t past_conidx,uint16_t time_out);
void bt_stack_ble_pre_sync_stop(void);
void bt_stack_ble_conn_update(uint8_t conidx, uint16_t conn_intv_min, uint16_t conn_intv_max, uint16_t latency, uint16_t super_to);

/// @} BT STACK
#endif // BT_BLE_IF_H_

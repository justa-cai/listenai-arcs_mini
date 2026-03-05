/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
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

typedef struct ble_gen_adv
{
    /// user adv data length, if 0, use adv gen data.
    uint8_t  user_data_len;
    /// user adv data.
    uint8_t  *user_data;
    /// scan rsp data length.
    uint8_t  rsp_data_len;
    /// scan rsp data.
    uint8_t  *rsp_data;
}ble_gen_adv_t;

typedef struct ble_dir_adv
{
    uint8_t  peer_addr[6];
}ble_dir_adv_t;

typedef struct ble_adv_cfg
{
    /// adv number,max BLE_ACTIVITY_ADV_MAX.
    uint8_t  adv_id;
    /// adv type, @see enum gapm_adv_type.
    uint8_t  adv_type;
    /// adv discover mode, @see enum gapm_adv_disc_mode.
    uint8_t  disc_mode;
    /// adv flags, @see enum gap_adv_filter_policy.
    uint8_t  adv_filter;
    /// adv flags, @see enum gapm_adv_flag.
    uint16_t flags;
    /// min interval,1:625us,Must be greater than 20ms.
    uint16_t intv_min;
    /// max interval,1:625us,Must be greater than 20ms.
    uint16_t intv_max;

    union
    {
        ble_gen_adv_t gen_adv;
        ble_dir_adv_t dir_adv;
    } adv_param;

} ble_adv_cfg_t;

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

/// @} BT STACK
#endif // BT_BLE_IF_H_

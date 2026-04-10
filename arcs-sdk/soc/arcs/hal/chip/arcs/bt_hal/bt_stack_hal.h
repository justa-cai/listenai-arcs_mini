/**
 ****************************************************************************************
 *
 * @file bt_stack_hal.h
 *
 * @brief Header file - BT STACK INTERFACE.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

#ifndef BT_STACK_HAL_H_
#define BT_STACK_HAL_H_

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "os_task_init.h"
#include "ble_gap.h"
#include "bt_stack_cfg.h"
#define MAX_DISCOVER_DEVICE     (8)

/*
 * DEFINES
 ****************************************************************************************
 */
 
 typedef struct bt_if_scan_dev
 {
     gap_bdaddr_t                addr;
     int8_t                      rssi;
     uint8_t                     flag;
     struct gap_adv_report_data  rep_data;
 }bt_if_scan_dev_t;
 
 typedef struct bt_if_discover_dev
 {
     gap_bdaddr_t            addr;
     int8_t                  rssi;
     uint8_t                 mode;
     uint32_t                cod;
     struct gap_dev_name     name;
 }bt_if_discover_dev_t;
 
 typedef struct bt_if_bond
 {
     gap_bdaddr_t            addr;
 }bt_if_bond_dev_t;
 /// bt stack if environment variable
typedef struct bt_stack_if_env_tag
{
    uint8_t    bt_ble_connected;
    uint8_t    bt_ble_encryption;
    uint8_t    bt_notify_pending_num;
    uint16_t   bt_hid_send_cnt;
    //bt_if_bond_dev_t *ble_dev_list[MAX_BOND_BLE_DEVICE];
    bt_if_scan_dev_t *scan_list[MAX_DISCOVER_DEVICE];
    bt_gap_peer_info_t bt_ble_peer_info;
#if BT_STACK_PRESENT
    uint8_t bt_classic_connected;
    bt_if_discover_dev_t *discover_list[MAX_DISCOVER_DEVICE];
    //bt_if_bond_dev_t     *classic_dev_list[MAX_BOND_CLASSIC_DEVICE];
#if BT_MUSIC_PRESENT
    uint16_t bt_music_send_cnt;
#endif
#if BT_CALL_PRESENT
    uint16_t bt_call_send_cnt;
    uint8_t  bt_call_codec_type;
#endif

#endif
} bt_stack_if_env_tag_t;
 
/*
 * ENUMERATIONS
 ****************************************************************************************
 */

/*
 * GLOBAL VARIABLE DECLARATIONS
 ****************************************************************************************
 */
void bt_stack_if_init(uint8_t type);
uint8_t bt_stack_if_msg_handle(btos_event_t* msg);
uint8_t bt_stack_if_user_schedule(void);
uint8_t bt_send_schedule_notify(void);
uint8_t bt_send_schedule_notify_isr(void);
bt_stack_if_env_tag_t *bt_stack_if_get_env(void);
os_task_cb_t *bt_stack_if_get_cb(void);
uint8_t bt_stack_nvs_get(uint8_t param_id, uint8_t * lengthPtr, uint8_t *buf);
uint8_t bt_stack_nvs_set(uint8_t param_id, uint8_t length, uint8_t *buf);
uint8_t bt_stack_nvs_del(uint8_t param_id);

/// @} BT OS TASK
#endif // BT_STACK_HAL_H_

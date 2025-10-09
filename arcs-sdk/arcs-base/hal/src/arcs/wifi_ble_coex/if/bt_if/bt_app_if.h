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
 typedef struct ble_hogpd_info
{
    /// Bt connect index.
    uint8_t              conidx;
    /// Aud frame number.
    uint8_t              report_idx;
    /// Aud packet length.
    uint16_t             len;
    /// Aud data.
    uint8_t              value[__ARRAY_EMPTY];
}ble_hogpd_info_t;

 typedef struct ble_adv_info
{
    ///  app adv id.
    uint8_t              adv_id;
    /// app adv type @see enum app_adv_type
    uint8_t              adv_type;
}ble_adv_info_t;

 typedef struct ble_net_cfg_info
{
    /// Bt connect index.
    uint8_t              conidx;
    /// opcode.
    uint8_t              op;
    /// status.
    uint16_t             status;
    /// data length.
    uint16_t             len;
    /// data.
    uint8_t              value[__ARRAY_EMPTY];
}ble_net_cfg_info_t;

/*
 * ENUMERATIONS
 ****************************************************************************************
 */
enum app_adv_type
{
    BLE_ADV_GEN = 0,
    BLE_ADV_GEN_PAIRED,
    BLE_ADV_DIR,
    BLE_ADV_DIR_HDC,
};


/*
 * GLOBAL VARIABLE DECLARATIONS
 ****************************************************************************************
 */
uint8_t app_ble_hogpd_hid_send(uint8_t conidx, uint8_t report_idx, uint8_t length, uint8_t* value);
uint8_t app_ble_hogpd_hid_send_handler(ble_hogpd_info_t *hogpd_info);
uint8_t app_ble_connected_state(uint8_t conidx);
uint8_t app_ble_adv_start(uint8_t adv_id, uint8_t adv_type);
uint8_t app_ble_adv_stop(uint8_t adv_id);
uint8_t app_ble_adv_start_handler(ble_adv_info_t *adv_info);
uint8_t app_ble_adv_stop_handler(ble_adv_info_t *adv_info);
uint8_t app_ble_voice_data_send(uint8_t conidx, uint8_t report_idx, uint8_t length, uint8_t* value);
uint8_t app_ble_voice_data_send_handler(ble_hogpd_info_t *hogpd_info);

/// @} BT STACK
#endif // BT_BLE_IF_H_

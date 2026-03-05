/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BT_APP_IF_H_
#define BT_APP_IF_H_

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "os_task_init.h"
#include "ble_gap.h"
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

  typedef struct ble_scan_info
 {
    uint8_t   scan_id;

    uint8_t   scan_param_dft;

    uint8_t   scan_type;

    uint8_t   scan_phy;

    uint8_t   scan_intv;

    uint8_t   scan_win;
 }ble_scan_info_t;


  typedef struct ble_conn_info
 {
     gap_bdaddr_t addr;

     uint8_t  phy;

     uint16_t conn_intv_min;

     uint16_t conn_intv_max;

     uint16_t conn_latency;

     uint16_t conn_super_to;
 }ble_conn_info_t;



  typedef struct ble_disconn_info
 {
     uint8_t conidx;

     uint8_t reason;

 }ble_disconn_info_t;


 typedef struct ble_conn_update_info
 {
     uint8_t  conhdl;

     uint16_t intv_min;
     /// Connection interval maximum
     uint16_t intv_max;
     /// Latency
     uint16_t latency;
     /// Supervision timeout
     uint16_t time_out;

 }ble_conn_update_info_t;


 typedef struct bt_scan_info
 {
     uint8_t scan_en;
 }bt_scan_info_t;

  typedef struct bt_inquiry_info
 {
     uint8_t    disc_mode;
     uint8_t    max_count;
 }bt_inquiry_info_t;

  typedef struct bt_connect_info
 {
     ///addr
     gap_bdaddr_t addr;
     ///type
     uint8_t type;
     ///clock offset
     uint16_t clk_off;
     ///page scan repeat mode
     uint8_t page_scan_rep_mode;
 }bt_connect_info_t;


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

/**
 * @brief Get BLE advertisement data
 * 
 * @param[out] len Pointer to store the length of the data
 * @return const uint8_t* Pointer to the advertisement data
 * 
 * @note This function is defined as weak in the component. 
 *       Users can override it in their application to provide custom advertisement data.
 */
const uint8_t* lisa_bt_get_adv_data(uint8_t *len);

/**
 * @brief Get BLE scan response data
 * 
 * @param[out] len Pointer to store the length of the data
 * @return const uint8_t* Pointer to the scan response data
 * 
 * @note This function is defined as weak in the component. 
 *       Users can override it in their application to provide custom scan response data.
 */
const uint8_t* lisa_bt_get_scan_rsp_data(uint8_t *len);

/**
 * @brief Configure BLE GAP parameters
 * 
 * @param[in,out] cfg Pointer to the GAP configuration structure
 * 
 * @note This function is defined as weak in the component. 
 *       Users can override it in their application to modify the default GAP configuration.
 */
void lisa_bt_gap_config(ble_gap_cfg_t *cfg);

/// @} BT STACK
#endif // BT_BLE_IF_H_

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
uint8_t app_ble_adv_start(uint8_t adv_id, uint8_t adv_type);
uint8_t app_ble_adv_stop(uint8_t adv_id);
uint8_t app_ble_voice_data_send(uint8_t conidx, uint8_t report_idx, uint8_t length, uint8_t* value);
uint8_t app_ble_netcfg_bles_send_notify(uint8_t conidx, uint8_t op, uint8_t state, uint8_t length, uint8_t* value);
uint8_t app_ble_scan_param(uint8_t type, uint8_t phy, uint16_t scan_intv, uint16_t scan_win);
uint8_t app_ble_scan_stop(uint8_t scan_id);
uint8_t app_ble_scan_start(uint8_t scan_id);
uint8_t app_ble_conn(gap_bdaddr_t addr, uint8_t phy, uint16_t conn_intv_min, uint16_t conn_intv_max, uint16_t conn_latency, uint16_t conn_super_to);
uint8_t app_ble_conn_update(uint8_t conidx, uint16_t conn_intv_min, uint16_t conn_intv_max, uint16_t latency, uint16_t super_to);
uint8_t app_ble_dsiconnect(uint8_t conidx, uint8_t reason);



/// @} BT STACK
#endif // BT_BLE_IF_H_

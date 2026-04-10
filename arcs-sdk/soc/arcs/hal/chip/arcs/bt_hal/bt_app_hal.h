/**
 ****************************************************************************************
 *
 * @file bt_app_hal.h
 *
 * @brief Header file - BT STACK INTERFACE.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

#ifndef BT_APP_HAL_H_
#define BT_APP_HAL_H_

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

/// A2DP enable info
typedef struct bt_a2dp_enable_info
{
    ///
    uint8_t  a2dp_role;
    ///
    uint8_t  aac_support;
} bt_a2dp_enable_info_t;

/// A2DP connect info
typedef struct bt_a2dp_connect_info
{
    ///
    uint8_t  conidx;
    ///
    uint8_t  role;
} bt_a2dp_connect_info_t;

/// A2DP start info
typedef struct bt_a2dp_start_info
{
    uint8_t  conidx;
} bt_a2dp_start_info_t;

/// A2DP send media info (variable length)
typedef struct
{
    ///
    uint8_t  conidx;
    ///
    uint8_t  frame_num;
    ///
    uint16_t len;
    ///
    uint8_t  data[__ARRAY_EMPTY];
} bt_a2dp_send_media_info_t;

/// HFP enable info
typedef struct bt_hfp_enable_info
{
    ///
    uint8_t  hfp_role;
    ///
    uint16_t hfp_feats;
} bt_hfp_enable_info_t;

/// HFP connect info
typedef struct bt_hfp_connect_info
{
    ///
    uint8_t  conidx;
    ///
    uint8_t  peer_role;
} bt_hfp_connect_info_t;

/// HFP set codec type info
typedef struct bt_hfp_set_codec_info
{
    ///
    uint8_t  conidx;
    ///
    uint8_t  codec_type;
} bt_hfp_set_codec_info_t;

/// HFP call start info
typedef struct bt_hfp_call_start_info
{
    ///
    uint8_t  conidx;
    ///
    uint8_t  call_idx;
} bt_hfp_call_start_info_t;

/// HFP call add audio info
typedef struct bt_hfp_call_add_audio_info
{
    ///
    uint8_t  conidx;
    ///
    uint8_t  codec_type;
} bt_hfp_call_add_audio_info_t;

/// HFP call remove audio info
typedef struct bt_hfp_call_remove_audio_info
{
    ///
    uint8_t  conidx;
    ///
    uint8_t  reason;
} bt_hfp_call_remove_audio_info_t;

/// HFP call incomming info
typedef struct bt_hfp_call_incomming_info
{
    ///
    uint8_t  conidx;
    ///
    uint8_t  idx;
} bt_hfp_call_incomming_info_t;

/// HFP send audio data info (variable length)
typedef struct bt_hfp_send_aud_info
{
    ///
    uint8_t  conidx;
    ///
    uint16_t len;
    ///
    uint8_t  data[__ARRAY_EMPTY];
} bt_hfp_send_aud_info_t;

/// AVRCP play status set info
typedef struct bt_avrcp_play_status_info
{
    ///
    uint8_t  conidx;
    ///
    uint8_t  play_status;
} bt_avrcp_play_status_info_t;

/// GAP auth request info
typedef struct bt_gap_auth_req_info
{
    ///
    uint8_t  conidx;
    ///
    uint8_t  sec_req;
} bt_gap_auth_req_info_t;

/// GAP save link key info
typedef struct bt_gap_save_lk_info
{
    ///
    uint8_t  conidx;
} bt_gap_save_lk_info_t;

/// BT set ASIC CVSD enable info
typedef struct bt_set_asic_cvsd_info
{
    ///
    uint8_t  enable;
} bt_set_asic_cvsd_info_t;

/// A2DP audio send start info
typedef struct bt_aud_a2dp_send_start_info
{
    ///
    uint8_t  conidx;
    ///
    uint8_t  codec;
    ///
    uint8_t  ch;
    ///
    uint16_t sample_rate;
} bt_aud_a2dp_send_start_info_t;

/// A2DP audio send stop info
typedef struct bt_aud_a2dp_send_stop_info
{
    ///
    uint8_t  conidx;
    ///
    uint8_t  status;
} bt_aud_a2dp_send_stop_info_t;

/// A2DP audio send data info (variable length)
typedef struct bt_aud_a2dp_send_data_info
{
    ///
    uint8_t  conidx;
    ///
    uint8_t  frame_num;
    ///
    uint16_t seq;
    ///
    uint16_t len;
    ///
    uint8_t  data[__ARRAY_EMPTY];
} bt_aud_a2dp_send_data_info_t;

/// HFP audio send start info
typedef struct bt_aud_hfp_send_start_info
{
    ///
    uint8_t  conidx;
    ///
    uint8_t  codec_type;
} bt_aud_hfp_send_start_info_t;

/// HFP audio send stop info
typedef struct bt_aud_hfp_send_stop_info
{
    ///
    uint8_t  conidx;
    ///
    uint8_t  reason;
} bt_aud_hfp_send_stop_info_t;

/// HFP audio send data info (variable length)
typedef struct bt_aud_hfp_send_data_info
{
    ///
    uint8_t  conidx;
    ///
    uint8_t  pkt_sta;
    ///
    uint16_t len;
    ///
    uint8_t  data[__ARRAY_EMPTY];
} bt_aud_hfp_send_data_info_t;

typedef struct app_handler_by_user_cb
{
    uint8_t (*cb_app_ble_adv_start_handler)(ble_adv_info_t *adv_info);
    uint8_t (*cb_app_ble_netcfg_bles_send_notify_handler)(ble_net_cfg_info_t *netcfg_info);
} app_handler_by_user_cb_t;

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
uint8_t app_ble_conn_handler(ble_conn_info_t *conn_info);

#if (BT_STACK_PRESENT)
uint8_t app_bt_scan(uint8_t enable);
uint8_t app_bt_inq_start(uint8_t disc_mode, uint8_t max_count);
uint8_t app_bt_inq_stop(void);
uint8_t app_bt_conn(gap_bdaddr_t addr, uint8_t type, uint16_t clk_off, uint8_t page_scan_rep_mode);
uint8_t app_bt_conn_cancel(void);
uint8_t app_bt_disconnect(uint8_t conidx, uint8_t reason);
#endif



/// @} BT STACK
#endif // BT_BLE_IF_H_

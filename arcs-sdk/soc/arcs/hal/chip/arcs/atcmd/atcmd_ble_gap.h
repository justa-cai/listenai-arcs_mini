/**
 ****************************************************************************************
 *
 * @file atcmd_ble_gap.h
 *
 * @brief BLE GAP Header
 *
 * Copyright (C) ListenAI  2024-2025
 *
 ****************************************************************************************
 */

#ifndef _ATCMD_BLE_GAP_H_
#define _ATCMD_BLE_GAP_H_
#include <stdint.h>
#include "os_task_init.h"
#include "ble_gap.h"

#ifndef   MIN
#define MIN(a, b)         (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAC2STR
#define MAC2STR(a) (a)[5], (a)[4], (a)[3], (a)[2], (a)[1], (a)[0]
#define MACSTR "%02X:%02X:%02X:%02X:%02X:%02X"
#endif

#define DEVICE_NAME         "PG"

#define MAX_SCAN_BLE_DEVICE     (8)
#define MAX_BOND_BLE_DEVICE     (8)

#define MAX_DISCOVER_DEVICE     (8)
#define MAX_BOND_CLASSIC_DEVICE (8)

#define CONNECT_LAST_PEER_DEV     (0)

/// scan parm
#define BLE_SCAN_TYPE (GAPM_SCAN_TYPE_GEN_DISC)
/// (GAPM_SCAN_PROP_PHY_1M_BIT | GAPM_SCAN_PROP_ACTIVE_1M_BIT)
#define BLE_SCAN_PHY  ((1 << 0) | (1 << 2))
#define BLE_SCAN_INTV (320)
#define BLE_SCAN_WIN  (160)

/// adv parm
#define BLE_ADV_TYPE (GAPM_ADV_TYPE_LEGACY)
#define BLE_ADV_MODE (GAPM_ADV_MODE_GEN_DISC)
#define BLE_ADV_MIN  (160)
#define BLE_ADV_MAX  (320)

/// connect parm
//The Supervision_Timeout parameter shall define the link supervision timeout
//for the LE link. The Supervision_Timeout in milliseconds shall be larger than (1
//+ Connection_Latency) * Connection_Interval_Max * 2, where
//Connection_Interval_Max is given in milliseconds.
#define BLE_CON_LATENCY              99
#define BLE_CON_INTERVAL_MIN         8
#define BLE_CON_INTERVAL_MAX         8
#define BLE_CON_SUPERVISION_TIMEOUT  500
#define BLE_MAX_WLIST_NUM            (5)
#define REMOTE_PRODUCT_TEST          (0)

#define BLE_PEER_FEAT_CON_PARAM_DIS  (1)

#define BLE_CON_PHY                  (GAP_PHY_LE_1MBPS)

/// white list & ral list
#define WHITE_LIST_ADV_ENABLE        0
#define WHITE_LIST_ADD               0
#define RESOVLE_LIST_ADD             1

 /**
 * Default Scan response data
 * --------------------------------------------------------------------------------------
 * x09                             - Length
 * xFF                             - Vendor specific advertising type
 * x00\x60\x4C\x53\x2D\x48\x49\x44 - "LS-HID"
 * --------------------------------------------------------------------------------------
 */
#define APP_SCNRSP_DATA                 "\x03\x19\xC1\x03\x08\xFF\x49\x46\x4C\x59\x20\x52\x43"
#define APP_UUID_DATA                   "\x03\x03\x12\x18"

#define ADV_USER_DATA                   (1)
#define LEGA_ADV_DATA_LEN   0x1F

// uuid 
#define BLOOD_PRESSURE_UUID             (0x1810)
#define HID_UUID                        (0x1812)


#define BT_NOTIFY_PENDING_MAX                (3)

#define BT_STACK_NVDS_SUPPORT                (1)

enum {
    BLE_CONN_STATUS_DISCONNECT = 0x01,
    BLE_CONN_STATUS_CONNECT    = 0x02,
    BLE_CONN_STATUS_CONNECT_PAIRED = 0x03,
};

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
#endif
} bt_stack_if_env_tag_t;

struct gap_ral_dev_info
{
    /// Device identity
    gap_bdaddr_t addr;
    /// Privacy Mode
    uint8_t      priv_mode;
    /// Peer IRK
    uint8_t      peer_irk[GAP_KEY_LEN];
    /// Local IRK
    uint8_t      local_irk[GAP_KEY_LEN];
};

extern const ble_gap_cb_t atcmd_bt_stack_gap_cb;
uint8_t bt_stack_nvs_get(uint8_t param_id, uint8_t * lengthPtr, uint8_t *buf);
uint8_t bt_stack_nvs_set(uint8_t param_id, uint8_t length, uint8_t *buf);
uint8_t bt_stack_nvs_del(uint8_t param_id);

#endif /* _ATCMD_BLE_GAP_H_ */
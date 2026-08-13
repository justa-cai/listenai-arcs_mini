/**
 ****************************************************************************************
 *
 * @file bt_stack_cfg.h
 *
 * @brief Header file - BT STACK INTERFACE.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

#ifndef BT_STACK_CFG_H_
#define BT_STACK_CFG_H_

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
#ifndef   MIN
#define MIN(a, b)         (((a) < (b)) ? (a) : (b))
#endif

#ifdef CONFIG_LISA_BLUETOOTH_HFP_MSBC_SUPPORT
#define BT_STACK_HFP_MSBC_SUPPORT        (1)
#else
#define BT_STACK_HFP_MSBC_SUPPORT        (0)
#endif
#define BT_USE_ASIC_CVSD                 (1)

#ifdef CONFIG_LISA_BLUETOOTH_DEVICE_NAME
#define DEVICE_NAME         CONFIG_LISA_BLUETOOTH_DEVICE_NAME
#else
#define DEVICE_NAME         "ARCS"
#endif

#define  BT_CLASSIC_CFG_FLAG  (0)

///bt stack classic role 0:sink.  1:source.
#define BT_STACK_CLASSIC_SINK            (0)
#define BT_STACK_CLASSIC_SOURCE          (1)

#ifndef BT_STACK_CLASSIC_ROLE
#define BT_STACK_CLASSIC_ROLE            (BT_STACK_CLASSIC_SOURCE)
#endif

///1:1 slot 0.625ms
#define BT_STACK_LINK_TIMEOUT   (16000)///5s

#define MAX_SCAN_BLE_DEVICE     (8)
#define MAX_BOND_BLE_DEVICE     (8)

#define MAX_DISCOVER_DEVICE     (8)
#define MAX_BOND_CLASSIC_DEVICE (8)

#define BT_STACK_CLASSIC_BIG_ACL_SEND_MAX  (5)

#define CONNECT_LAST_PEER_DEV     (0)

#ifndef PLF_BUILD_FEAT_HCIT
#define PLF_BUILD_FEAT_HCIT   (HCIT_UART_PRESENT | (HCIT_USB_PRESENT * PLF_HCIT_USB) | (HCIT_AUD_PRESENT * PLF_HCIT_AUD))
#endif
#ifndef PLF_BUILD_FEAT_CORE
#define PLF_BUILD_FEAT_CORE   (BLE_EMB_PRESENT | (BT_EMB_PRESENT * PLF_CORE_BT) | (BLE_ISO_PRESENT * PLF_CORE_ISO))
#endif
#ifndef PLF_BUILD_FEAT_STACK
#define PLF_BUILD_FEAT_STACK  (BLE_HOST_PRESENT | (BT_STACK_PRESENT *PLF_STACK_BT) | (MESH_PRESENT * PLF_STACK_MESH) | (LEA_PRESENT * PLF_STACK_LEA))
#endif

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

int bt_stack_cfg_set_local_addr(const gap_bdaddr_t *addr);

#define BLE_CON_PHY                  (GAP_PHY_LE_1MBPS)

/// white list & ral list
#define WHITE_LIST_ADV_ENABLE        0
#define WHITE_LIST_ADD               0
#define RESOVLE_LIST_ADD             0


// uuid 
#define BLOOD_PRESSURE_UUID             (0x1810)
#define HID_UUID                        (0x1812)


#define BT_NOTIFY_PENDING_MAX                (3)

#ifndef CONFIG_LISA_BLUETOOTH_STORAGE_NVS
#define CONFIG_LISA_BLUETOOTH_STORAGE_NVS    0
#endif

#ifndef CONFIG_LISA_BLUETOOTH_STORAGE_KV
#define CONFIG_LISA_BLUETOOTH_STORAGE_KV     0
#endif

#define BT_STACK_NVDS_SUPPORT                (CONFIG_LISA_BLUETOOTH_STORAGE_NVS || CONFIG_LISA_BLUETOOTH_STORAGE_KV)


#define BT_STACK_BLE_HOGPD_HID_MAX_COUNT     (20)

// bt sniff (in slots)
//0x06--0x0540(3.75ms--840ms),must even number 
#define BT_SNIFF_MAX_INTERVAL (800)    //500ms
//min < max,must even number 
#define BT_SNIFF_MIN_INTERVAL (320)
//
#define BT_SNIFF_ATTEMPT      (4)
//
#define BT_SNIFF_TIMEOUT      (2)

/*
 * ENUMERATIONS
 ****************************************************************************************
 */

/*
 * GLOBAL VARIABLE DECLARATIONS
 ****************************************************************************************
 */
void bt_stack_reset_cmp(uint16_t status);


/// @} BT OS TASK
#endif // BT_STACK_CFG_H_

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
/*
 * DEFINES
 ****************************************************************************************
 */
///bt stack classic role 0:sink.  1:source.
#define BT_STACK_CLASSIC_SINK            (0)
#define BT_STACK_CLASSIC_SOURCE          (1)

#define BT_STACK_CLASSIC_ROLE            (BT_STACK_CLASSIC_SINK)//(BT_STACK_CLASSIC_SOURCE)
#define BT_USE_ASIC_CVSD                 (0)
#if (BT_STACK_CLASSIC_ROLE == BT_STACK_CLASSIC_SOURCE)
#define DEVICE_NAME         "BT_SOURCE_IPC"
#else
#define DEVICE_NAME         "BT_SINK_IPC"
#endif

#define MAX_SCAN_BLE_DEVICE     (8)
#define MAX_BOND_BLE_DEVICE     (8)

#define MAX_BOND_CLASSIC_DEVICE (8)

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

#define BLE_CON_PHY                  (GAP_PHY_LE_1MBPS)

/// white list & ral list
#define WHITE_LIST_ADV_ENABLE        0
#define WHITE_LIST_ADD               0
#define RESOVLE_LIST_ADD             0

 /**
 * Default Scan response data
 * --------------------------------------------------------------------------------------
 * x09                             - Length
 * xFF                             - Vendor specific advertising type
 * x00\x60\x4C\x53\x2D\x48\x49\x44 - "LS-HID"
 * --------------------------------------------------------------------------------------
 */
#define APP_SCNRSP_DATA                 "\x03\x19\xC1\x03\x08\xFF\x49\x46\x4C\x59\x20\x52\x43"

#define ADV_USER_DATA                   (0)

// uuid 
#define BLOOD_PRESSURE_UUID             (0x1810)
#define HID_UUID                        (0x1812)


#define BT_NOTIFY_PENDING_MAX                (3)

#define BT_STACK_NVDS_SUPPORT                (CFG_NVS)

#define BT_STACK_BLE_HOGPD_HID_MAX_COUNT     (20)


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

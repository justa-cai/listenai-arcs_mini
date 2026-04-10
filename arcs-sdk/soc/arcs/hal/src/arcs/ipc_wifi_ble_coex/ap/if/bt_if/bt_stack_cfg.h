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
#include "bt_stack_hal_cfg.h"
/*
 * DEFINES
 ****************************************************************************************
 */
#ifndef   MIN
#define MIN(a, b)         (((a) < (b)) ? (a) : (b))
#endif

#define DEVICE_NAME         "PG"

#define CONNECT_LAST_PEER_DEV     (0)

#define REMOTE_PRODUCT_TEST          (0)

#define BLE_PEER_FEAT_CON_PARAM_DIS  (1)

/// white list & ral list
#define WHITE_LIST_ADV_ENABLE        0
#define WHITE_LIST_ADD               1
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

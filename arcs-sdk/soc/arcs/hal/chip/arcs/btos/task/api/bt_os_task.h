/**
 ****************************************************************************************
 *
 * @file bt_os_task.h
 *
 * @brief Header file - BT OS TASK.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

#ifndef BT_OS_TASK_H_
#define BT_OS_TASK_H_

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
//#include "bt_config.h"

#include "btos_al.h"
#include "os_task_init.h"

/*
 * DEFINES
 ****************************************************************************************
 */
#define BT_OS_TASK_STACK_DEFAULT_SIZE      (4096/4)
#define BT_OS_TASK_DEFAULT_PRIORITY        (OS_TASK_PRIORITY_BASE+2)
#define BT_OS_TASK_NAME                    "BT Task"


/*
 * ENUMERATIONS
 ****************************************************************************************
 */
enum bt_os_msg_id
{
    /// Send notify to shcedule bt task event
    BT_OS_NOTIFY_EVT                                        = OS_MSG_ID(BT, 0x00),

    /// BT open event
    BT_OS_OPEN_EVT                                          = OS_MSG_ID(BT, 0x01),
    /// BT close event
    BT_OS_CLOSE_EVT                                         = OS_MSG_ID(BT, 0x02),
    /// Read Data Complete event
    BT_OS_INIT_EVT                                          = OS_MSG_ID(BT, 0x03),

    /// Scan
    BT_OS_SCAN_START_EVT                                    = OS_MSG_ID(BT, 0x04),
    BT_OS_SCAN_STOP_EVT                                     = OS_MSG_ID(BT, 0x05),
    /// Adv 
    BT_OS_ADV_START_EVT                                     = OS_MSG_ID(BT, 0x06),
    BT_OS_ADV_STOP_EVT                                      = OS_MSG_ID(BT, 0x07),
    /// Per sync
    BT_OS_PER_SYNC_START_EVT                                = OS_MSG_ID(BT, 0x08),
    BT_OS_PER_SYNC_STOP_EVT                                 = OS_MSG_ID(BT, 0x09),
    
    /// Connect
    BT_OS_CONNECT_EVT                                       = OS_MSG_ID(BT, 0x0a),
    /// Disconnect
    BT_OS_DISCONNECT_EVT                                    = OS_MSG_ID(BT, 0x0b),
#if LEA_PRESENT
    /// Scan lea peer dev
    BT_OS_LEA_SCAN_EVT                                      = OS_MSG_ID(BT, 0x10),
    /// Adv lea info
    BT_OS_LEA_ADV_EVT                                       = OS_MSG_ID(BT, 0x11),

    /// lea broadcast media receiver start
    BT_OS_LEA_BMR_START_EVT                                 = OS_MSG_ID(BT, 0x12),
    /// lea broadcast media receiver stop
    BT_OS_LEA_BMR_STOP_EVT                                  = OS_MSG_ID(BT, 0x13),

    /// lea broadcast media sender start
    BT_OS_LEA_BMS_START_EVT                                 = OS_MSG_ID(BT, 0x14),
    /// lea broadcast media sender stop
    BT_OS_LEA_BMS_STOP_EVT                                  = OS_MSG_ID(BT, 0x15),

    /// lea unicast media receiver start
    BT_OS_LEA_UMR_START_EVT                                 = OS_MSG_ID(BT, 0x16),
    /// lea unicast media receiver stop
    BT_OS_LEA_UMR_STOP_EVT                                  = OS_MSG_ID(BT, 0x17),

    /// lea unicast media sender start
    BT_OS_LEA_UMS_START_EVT                                 = OS_MSG_ID(BT, 0x18),
    /// lea unicast media sender stop
    BT_OS_LEA_UMS_STOP_EVT                                  = OS_MSG_ID(BT, 0x19),

    /// lea call gateway start
    BT_OS_LEA_CG_START_EVT                                  = OS_MSG_ID(BT, 0x1a),
    /// lea call gateway stop
    BT_OS_LEA_CG_STOP_EVT                                   = OS_MSG_ID(BT, 0x1b),

    /// lea call terminal start
    BT_OS_LEA_CT_START_EVT                                  = OS_MSG_ID(BT, 0x1c),
    /// lea call terminal stop
    BT_OS_LEA_CT_STOP_EVT                                   = OS_MSG_ID(BT, 0x1d),

#endif
    BT_OS_DATA_SEND_CNF_EVT                                 = OS_MSG_ID(BT, 0x1e),


    /// Hid send
    BT_OS_HID_SEND_EVT                                      = OS_MSG_ID(BT, 0x50),
    BT_OS_VOICE_DATA_SEND_EVT                               = OS_MSG_ID(BT, 0x51),

    /// Net config
    BT_OS_NET_CFG_SEND_EVT                                  = OS_MSG_ID(BT, 0x55),

    /// AT test
    BT_OS_AT_SEND_EVT                                       = OS_MSG_ID(BT, 0x60),

    BT_OS_CONNECT_UPDATE_EVT                                = OS_MSG_ID(BT, 0x70),

    BT_OS_BT_INQ_START_EVT                                  = OS_MSG_ID(BT, 0x71),

    BT_OS_BT_INQ_STOP_EVT                                   = OS_MSG_ID(BT, 0x72),

    BT_OS_BT_SCAN_EVT                                       = OS_MSG_ID(BT, 0x73),

    BT_OS_BT_CONNECT_EVT                                    = OS_MSG_ID(BT, 0x74),

    BT_OS_BT_DISCONNECT_EVT                                 = OS_MSG_ID(BT, 0x75),


};

/*
 * GLOBAL VARIABLE DECLARATIONS
 ****************************************************************************************
 */
void bt_os_init(os_task_cb_t *cb);
void bt_os_task(void *args);
/// @} BT OS TASK
#endif // BT_OS_TASK_H_


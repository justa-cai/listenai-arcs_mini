/* Don't modify this file, this file is generate from bt_configy.py */
#ifndef _BT_CONFIG_H_
#define _BT_CONFIG_H_
//#
//# bt ip config
//#

//# Top level product configuration

//# controller
#define  BT_EMB_PRESENT       1
#define  BLE_EMB_PRESENT      1
#define  BLE_ISO_PRESENT      0
#define  BT_DUAL_MODE         (BT_EMB_PRESENT && BLE_EMB_PRESENT)
//# run ble only on dual mode set 1, run dual mode set 0
#define  SINGLE_RUN_ON_DUAL   0

//# host
#define  BLE_HOST_PRESENT     1
#define  BT_STACK_PRESENT     1
//# classic profile
#define  BT_MUSIC_PRESENT     1
#define  BT_CALL_PRESENT      0
//# ble profile
#define  BLE_GAF_PRESENT      0
#define  SMP_PRESENT          1
#define  LEA_PRESENT          0
#define  MESH_PRESENT         0
//# transport
#define  HCIT_UART_PRESENT    1
#define  HCIT_USB_PRESENT     0
//#hci audio access
#define  HCIT_AUD_PRESENT     1

#define  HCI_PRESENT          1
#define  AHI_PRESENT          0
#define  BLE_APP_PRESENT      1

#define  TWS_PRESENT          0
#define  LISTENAI_TWS_SUPPORT   TWS_PRESENT
//# classic bt config
//# Maximum number of ACL links
#define  MAX_NB_ACTIVE_ACL    4
//# Maximum number of Synchronous connections (0 to 2)
#define  MAX_NB_SYNC          2

//# FPGA RF board
#define  RF_MAX2830_SUPPORT     0
#define  RF_ARCS_B0_SUPPORT     0


//# DEBUG SETUP
#define  LS_DEBUG                 1
#define  LS_DEBUG_MEM             1
#define  LS_DEBUG_FLASH           0
#define  LS_DEBUG_STACK_PROF      0

#define  DBG_LOG_PRESENT          0
//# for fpga two node direct connect test
#define  LS_DEBUG_FPGA_DIRECT_MODE   0
//#for bis aes generate gsk in interrupt,for testcase iso_bis_p2p@31
#define  LS_BIS_GEN_GSK_INT       1

//# TRACER SETUP
#define  TRACER_PRESENT           0
//# TRACE MASK, see dbg_trc_cfg_fields
#define  TRACE_CFG_MASK           0xffffffff

//#/// Support HL Message API
#define  BLE_HL_MSG_API           1
//#/// Support GATT Client
#define  BLE_GATT_CLI             1

//#/// Number of L2CAP COC channel that can be created per connection
#define  L2CAP_COC_CHAN_PER_CON_NB                        (10)
//#/// Total Number of L2CAP channel and GATT bearer that can be allocated in environment heap
#define  L2CAP_CHAN_IN_ENV_NB                             (10)
//# Classic L2cap flow mode
#define  L2CAP_FLOW_MODE_SUPPORT                          0
//# Maximal authorized MTU / MPS value - Depends on memory size available
#define  GAP_LE_MTU_MAX                                   (2048)
#define  GAP_LE_MPS_MAX                                   (2048)
//#/// Maximum attribute value length
#define  GATT_MAX_VALUE                                   (2048)

//#/// Maximum number of devices in RAL
#define  BLE_RAL_MAX               (3)

//#/// Maximum number of simultaneous BLE activities (scan, connection, advertising, initiating)
#define  BLE_ACTIVITY_MAX          (5)
//#/// Maximum number of simultaneous connections
#define  BLE_CONNECTION_MAX        (3)
//#/// LE Power Control
#define  BLE_PWR_CTRL              (1)

//# Maximum number of advertising BLE activities 
#define  BLE_ACTIVITY_ADV_MAX      (1)
//# Maximum number of scan BLE activities
#define  BLE_ACTIVITY_SCAN_MAX     (1)
//# Maximum number of connection BLE activities
#define  BLE_ACTIVITY_CON_MAX      (1)
//# Maximum number of initiating BLE activities
#define  BLE_ACTIVITY_INIT_MAX     (1)

//# ISO configure
//#// Connected Isochronous Stream
#define  BLE_CIS                   0
//#// Broadcast Isochronous Stream
#define  BLE_BIS                   0
//#/// Maximum number of ISO channel / streams
#define  BLE_ISO_CON               4
//#/// Proprietary ISO over HCI
#define  BLE_ISOOHCI               1
//#/// Internal ISO generator for validation purpose
#define  BLE_ISOGEN                1

//# LEA config
//# Connected Isochronous Stream
#define  LEA_CIS                   1
//# Broadcast Isochronous Stream
#define  LEA_BIS                   1
//# Max connected cis in one cig.
#define  LEA_CIS_MAX               2
//# Max  bis in one big.
#define  LEA_BIS_MAX               2


//# classic stack configure
//# HFP config
#define  HFP_AG_PRESENT            0
#define  HFP_HF_PRESENT            1
#define  HSP_PRESENT               1
//# phonebook via HFP
#define  HFP_PB_PRESENT            1
//# max hf connections 
#define  HFP_MAX_CON               2
#define  HFP_MSBC_EN               1

//# PBAP profile
#define  BT_PBAP_PRESENT           0

//#music config
#define  A2DP_MAX_CON              2

//#notify
#define  BT_RTOS_NOTIFY_SUPPORT    1

//#BT auto start iscan&pscan
#define  BT_AUTO_PSCAN_ISCAN       0


#endif // _BT_CONFIG_H_

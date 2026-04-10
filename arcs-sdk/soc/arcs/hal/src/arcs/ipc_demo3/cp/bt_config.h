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
#define  BLE_ISO_PRESENT      1
//# host
#define  BLE_HOST_PRESENT     1
#define  BT_STACK_PRESENT     0
//# classic profile
#define  BT_MUSIC_PRESENT     0
#define  BT_CALL_PRESENT      0
//# ble profile
#define  BLE_GAF_PRESENT      0
#define  SMP_PRESENT          0
#define  LEA_PRESENT          0
#define  MESH_PRESENT         0
//# transport
#define  HCIT_UART_PRESENT    1
#define  HCIT_USB_PRESENT     0
//#hci audio access
#define  HCIT_AUD_PRESENT     0

#define  HCI_PRESENT          1
#define  AHI_PRESENT          1
#define  BLE_APP_PRESENT      0

//# classic bt config
//# Maximum number of ACL links
#define  MAX_NB_ACTIVE_ACL    4
//# Maximum number of Synchronous connections (0 to 2)
#define  MAX_NB_SYNC          2

//# DEBUG SETUP
#define  RW_DEBUG                 1
#define  RW_DEBUG_MEM             1
#define  RW_DEBUG_FLASH           0
#define  RW_DEBUG_STACK_PROF      0
//# for fpga two node direct connect test
#define  RW_DEBUG_FPGA_DIRECT_MODE   0
//#for bis aes generate gsk in interrupt,for wvt case iso_bis_p2p@31
#define  RW_BIS_GEN_GSK_INT		  1

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
//#/// Maximal authorized MTU / MPS value - Depends on memory size available
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

//# ISO configure
//#// Connected Isochronous Stream
#define  BLE_CIS                   1
//#// Broadcast Isochronous Stream
#define  BLE_BIS                   1
//#/// Maximum number of ISO channel / streams
#define  BLE_ISO_CON               4
//#/// Proprietary ISO over HCI
#define  BLE_ISOOHCI               1
//#/// Internal ISO generator for validation purpose
#define  BLE_ISOGEN                1

#endif // _BT_CONFIG_H_

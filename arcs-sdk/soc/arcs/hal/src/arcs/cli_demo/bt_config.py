#
# bt ip config
#

# Top level product configuration

# controller
BT_EMB_PRESENT     = 1
BLE_EMB_PRESENT    = 1
BLE_ISO_PRESENT    = 1
# host
BLE_HOST_PRESENT   = 1
BT_STACK_PRESENT   = 0
# classic profile
BT_MUSIC_PRESENT   = 0
BT_CALL_PRESENT    = 0
# ble profile
BLE_GAF_PRESENT    = 0
SMP_PRESENT        = 0
LEA_PRESENT        = 0
MESH_PRESENT       = 0
# transport
HCIT_UART_PRESENT  = 1
HCIT_USB_PRESENT   = 0
#hci audio access
HCIT_AUD_PRESENT   = 0

HCI_PRESENT        = 1
AHI_PRESENT        = 1
BLE_APP_PRESENT    = 0

# classic bt config
# Maximum number of ACL links
MAX_NB_ACTIVE_ACL  = 4
# Maximum number of Synchronous connections (0 to 2)
MAX_NB_SYNC        = 2

# DEBUG SETUP
RW_DEBUG               = 1
RW_DEBUG_MEM           = 1
RW_DEBUG_FLASH         = 0
RW_DEBUG_STACK_PROF    = 0
# for fpga two node direct connect test
RW_DEBUG_FPGA_DIRECT_MODE = 0
#for bis aes generate gsk in interrupt,for testcase iso_bis_p2p@31
RW_BIS_GEN_GSK_INT		= 1

# TRACER SETUP
TRACER_PRESENT         = 0
# TRACE MASK, see dbg_trc_cfg_fields
TRACE_CFG_MASK         = 0xffffffff

#/// Support HL Message API
BLE_HL_MSG_API         = 1
#/// Support GATT Client
BLE_GATT_CLI           = 1

#/// Number of L2CAP COC channel that can be created per connection
L2CAP_COC_CHAN_PER_CON_NB                      = (10)
#/// Total Number of L2CAP channel and GATT bearer that can be allocated in environment heap
L2CAP_CHAN_IN_ENV_NB                           = (10)
#/// Maximal authorized MTU / MPS value - Depends on memory size available
GAP_LE_MTU_MAX                                 = (2048)
GAP_LE_MPS_MAX                                 = (2048)
#/// Maximum attribute value length
GATT_MAX_VALUE                                 = (2048)

#/// Maximum number of devices in RAL
BLE_RAL_MAX             = (3)

#/// Maximum number of simultaneous BLE activities (scan, connection, advertising, initiating)
BLE_ACTIVITY_MAX        = (5)
#/// Maximum number of simultaneous connections
BLE_CONNECTION_MAX      = (3)
#/// LE Power Control
BLE_PWR_CTRL            = (1)

# Maximum number of advertising BLE activities 
BLE_ACTIVITY_ADV_MAX    = (1)
# Maximum number of scan BLE activities
BLE_ACTIVITY_SCAN_MAX   = (1)
# Maximum number of connection BLE activities
BLE_ACTIVITY_CON_MAX    = (1)
# Maximum number of initiating BLE activities
BLE_ACTIVITY_INIT_MAX   = (1)

# ISO configure
#// Connected Isochronous Stream
BLE_CIS                 = 1
#// Broadcast Isochronous Stream
BLE_BIS                 = 1
#/// Maximum number of ISO channel / streams
BLE_ISO_CON             = 4
#/// Proprietary ISO over HCI
BLE_ISOOHCI             = 1
#/// Internal ISO generator for validation purpose
BLE_ISOGEN              = 1

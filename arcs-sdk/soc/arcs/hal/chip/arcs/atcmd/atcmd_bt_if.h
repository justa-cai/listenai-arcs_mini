/**
 ****************************************************************************************
 *
 * @file bt_at_if.h
 *
 * @brief Header file - BT OS TASK.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

#ifndef BT_AT_IF_H_
#define BT_AT_IF_H_

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#if BT_WIFI_COEX
#include "bt_config.h"
#endif
#include "ls_bt_type.h"

#include "btos_al.h"
#include "os_task_init.h"

/*
 * DEFINES
 ****************************************************************************************
 */
 /// Maximum frequency value for test mode HCI:7.8.28
#define TEST_FREQ_MAX                        39
/// Minimum PHY value for test mode HCI:7.8.50
#define TEST_PHY_MIN                         0x01
/// Maximum PHY value for the receiver test mode HCI:7.8.50
#define RX_TEST_PHY_MAX                      0x03
/// Maximum PHY value for the transmitter test mode HCI:7.8.51
#define TX_TEST_PHY_MAX                      0x04

 /// Modulation index
#define STANDARD_MOD_IDX       (0)
#define STABLE_MOD_IDX         (1)
 
 /// CTE length (in number of 8us periods)
#define NO_CTE                 (0)
#define CTE_LEN_MIN            (0x02)
#define CTE_LEN_MAX            (0x14)
 
 /// CTE type
#define CTE_TYPE_AOA           (0)
#define CTE_TYPE_AOD_1US       (1)
#define CTE_TYPE_AOD_2US       (2)
#define CTE_TYPE_NO_CTE        (0xFF)

 /// At identifier index
#define BT_AT_FIRST(module) ((uint16_t)((module) << 8))
#define BT_AT_CMD_ID(module, idx) (BT_AT_FIRST((BT_AT_MODULE_ID_ ## module)) + idx)
/// Maximum length of switching pattern
#define BLE_MAX_SW_PAT_LEN     12



#define BD_NAME_SIZE        0xF8 // Was 0x20 for BLE HL
#define SCAN_RSP_DATA_LEN   0x1F

#define KEY_LEN             0x10

#define BD_ADDR_LAP_LEN     3
#define ADV_DATA_LEN        0x1F

#define SCAN_INTERVAL_MIN   0x0004 //(2.5 ms)
#define SCAN_INTERVAL_MAX   0x4000 //(10.24 sec)
#define SCAN_INTERVAL_DFT   0x0010 //(10 ms)

/// Scanning window (in 625us slot) (chapter 2.E.7.8.10)
#define SCAN_WINDOW_MIN     0x0004 //(2.5 ms)
#define SCAN_WINDOW_MAX     0x4000 //(10.24 sec)
#define SCAN_WINDOW_DFT     0x0010 //(10 ms)

#define ADV_INTERVAL_MIN    0x0020 //(20 ms)
#define ADV_INTERVAL_MAX    0x4000 //(10.24 sec)
#define ADV_INTERVAL_DFT    0x0800 //(1.28 sec)


/// Connection interval (N*1.250ms) (chapter 2.E.7.8.12)

#define CON_INTERVAL_MIN    0x0006  //(7.5 msec)
#define CON_INTERVAL_MIN_NEW    0  //(0 msec)
#define CON_INTERVAL_MAX    0x0C80  //(4 sec)
/// Connection latency (N*cnx evt) (chapter 2.E.7.8.12)
#define CON_LATENCY_MIN     0x0000
#define CON_LATENCY_MAX     0x01F3  // (499)
/// Supervision TO (N*10ms) (chapter 2.E.7.8.12)
#define CON_SUP_TO_MIN      0x000A  //(100 msec)
#define CON_SUP_TO_MAX      0x0C80  //(32 sec)

/// HCI 7.8.33 LE Set Data Length Command
/// Preferred minimum number of payload octets
#define LE_MIN_OCTETS       (27)
/// Preferred minimum number of microseconds
#define LE_MIN_TIME         (328)
/// Preferred minimum number of microseconds LL:4.5.10
#define LE_MIN_TIME_CODED   (2704)
/// Preferred maximum number of payload octets
#define LE_MAX_OCTETS       (251)
/// Preferred maximum number of microseconds
#define LE_MAX_TIME         (2120)
/// Preferred maximum number of microseconds LL:4.5.10
#define LE_MAX_TIME_CODED   (17040)

#define BLE_MIN_OCTETS  (27) // number of octets
#define BLE_MIN_TIME    (328) // in us
#define BLE_MAX_OCTETS  (251) // number of octets
#define BLE_MAX_TIME    (17040) // in us


// BLE adv type define
#define BLE_GAP_AD_TYPE_FLAGS                           0x01
#define BLE_GAP_AD_TYPE_16BIT_SERVICE_UUID_MORE_AVAILABLE 0x02
#define BLE_GAP_AD_TYPE_16BIT_SERVICE_UUID_COMPLETE     0x03
#define BLE_GAP_AD_TYPE_32BIT_SERVICE_UUID_MORE_AVAILABLE 0x04
#define BLE_GAP_AD_TYPE_32BIT_SERVICE_UUID_COMPLETE     0x05
#define BLE_GAP_AD_TYPE_128BIT_SERVICE_UUID_MORE_AVAILABLE 0x06
#define BLE_GAP_AD_TYPE_128BIT_SERVICE_UUID_COMPLETE    0x07
#define BLE_GAP_AD_TYPE_SHORT_LOCAL_NAME                0x08
#define BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME             0x09
#define BLE_GAP_AD_TYPE_TX_POWER_LEVEL                  0x0A
#define BLE_GAP_AD_TYPE_CLASS_OF_DEVICE                 0x0D
#define BLE_GAP_AD_TYPE_SIMPLE_PAIRING_HASH_C           0x0E
#define BLE_GAP_AD_TYPE_SIMPLE_PAIRING_RANDOMIZER_R     0x0F
#define BLE_GAP_AD_TYPE_SECURITY_MANAGER_TK_VALUE       0x10
#define BLE_GAP_AD_TYPE_SECURITY_MANAGER_OOB_FLAGS      0x11
#define BLE_GAP_AD_TYPE_SLAVE_CONNECTION_INTERVAL_RANGE 0x12
#define BLE_GAP_AD_TYPE_SOLICITED_SERVICE_UUIDS_16BIT   0x14
#define BLE_GAP_AD_TYPE_SOLICITED_SERVICE_UUIDS_128BIT  0x15
#define BLE_GAP_AD_TYPE_SERVICE_DATA                    0x16
#define BLE_GAP_AD_TYPE_PUBLIC_TARGET_ADDRESS           0x17
#define BLE_GAP_AD_TYPE_RANDOM_TARGET_ADDRESS           0x18
#define BLE_GAP_AD_TYPE_APPEARANCE                      0x19
#define BLE_GAP_AD_TYPE_ADVERTISING_INTERVAL            0x1A
#define BLE_GAP_AD_TYPE_LE_BLUETOOTH_DEVICE_ADDRESS     0x1B
#define BLE_GAP_AD_TYPE_LE_ROLE                         0x1C
#define BLE_GAP_AD_TYPE_SIMPLE_PAIRING_HASH_C256        0x1D
#define BLE_GAP_AD_TYPE_SIMPLE_PAIRING_RANDOMIZER_R256  0x1E
#define BLE_GAP_AD_TYPE_SERVICE_DATA_32BIT_UUID         0x20
#define BLE_GAP_AD_TYPE_SERVICE_DATA_128BIT_UUID        0x21
#define BLE_GAP_AD_TYPE_LESC_CONFIRMATION_VALUE         0x22
#define BLE_GAP_AD_TYPE_LESC_RANDOM_VALUE               0x23
#define BLE_GAP_AD_TYPE_URI                             0x24
#define BLE_GAP_AD_TYPE_INDOOR_POSITIONING              0x25
#define BLE_GAP_AD_TYPE_TRANSPORT_DISCOVERY_DATA        0x26
#define BLE_GAP_AD_TYPE_LE_SUPPORTED_FEATURES           0x27
#define BLE_GAP_AD_TYPE_CHANNEL_MAP_UPDATE_INDICATION   0x28
#define BLE_GAP_AD_TYPE_PB_ADV                          0x29
#define BLE_GAP_AD_TYPE_MESH_MESSAGE                    0x2A
#define BLE_GAP_AD_TYPE_MESH_BEACON                     0x2B
#define BLE_GAP_AD_TYPE_3D_INFORMATION_DATA             0x3D
#define BLE_GAP_AD_TYPE_MANUFACTURER_SPECIFIC_DATA      0xFF


#define MAX_ADV_DATA_LENGTH     31   // Legacy adv max len 31 bytes
#define MAX_EXT_ADV_DATA_LENGTH 1650 // Extended adv max len 1650 bytes


/**
 * Test mode transmit power level in dBm HCI:7.8.122
 *   -127 - Lowest transmit power level
 *     20 - Highest transmit power level
 *   0x7E - Minimum transmit power level
 *   0x7F - Maximum transmit power level
 */
enum test_mode_tx_pwr_lvl
{
    LOW_TX_PWR_LVL  = -127,
    HIGH_TX_PWR_LVL =   20,
    MIN_TX_PWR_LVL  = 0x7E,
    MAX_TX_PWR_LVL  = 0x7F,
};
///Transmitter test Packet Payload Type
enum
{
    ///Pseudo-random 9 TX test payload type
    PAYL_PSEUDO_RAND_9            = 0x00,
    ///11110000 TX test payload type
    PAYL_11110000,
    ///10101010 TX test payload type
    PAYL_10101010,
    ///Pseudo-random 15 TX test payload type
    PAYL_PSEUDO_RAND_15,
    ///All 1s TX test payload type
    PAYL_ALL_1,
    ///All 0s TX test payload type
    PAYL_ALL_0,
    ///00001111 TX test payload type
    PAYL_00001111,
    ///01010101 TX test payload type
    PAYL_01010101,
    ///inifinite payload
    PAYL_INIFINITE,
    ///user define payload
    PAYL_USER_DEFINE, 
};

typedef enum
{
    BT_AT_MODULE_ID_IDLE = 0,
    BT_AT_MODULE_ID_COMMON,
    BT_AT_MODULE_ID_HOST,
    BT_AT_MODULE_ID_CONTROLLER,
    BT_AT_MODULE_ID_TEST,
}bt_at_module_id;

enum bt_at_msg_id
{
    /// COMMON AT CMD
    BT_AT_COMMON_CMD                                            = BT_AT_CMD_ID(COMMON, 0x00),
    BT_AT_COMMON_BLE_INIT                                       = BT_AT_CMD_ID(COMMON, 0x01),
    BT_AT_COMMON_BLE_NAME                                       = BT_AT_CMD_ID(COMMON, 0x02),


    /// HOST AT CMD
    BT_AT_HOST_CMD                                              = BT_AT_CMD_ID(HOST, 0x00),
    BT_AT_HOST_BLE_ADV_START                                    = BT_AT_CMD_ID(HOST, 0x01),
    BT_AT_HOST_BLE_ADV_STOP                                     = BT_AT_CMD_ID(HOST, 0x02),
    BT_AT_HOST_BLE_SEC_PARAM                                    = BT_AT_CMD_ID(HOST, 0x03),
    BT_AT_HOST_BLE_ENC                                          = BT_AT_CMD_ID(HOST, 0x04),
    BT_AT_HOST_BLE_KEY_REPLY                                    = BT_AT_CMD_ID(HOST, 0x05),
    BT_AT_HOST_BLE_ENC_DEV                                      = BT_AT_CMD_ID(HOST, 0x06),
    BT_AT_HOST_BLE_ENC_CLEAR                                    = BT_AT_CMD_ID(HOST, 0x07),


    /// CONTROLLER AT CMD
    BT_AT_CONTROLLER_CMD                                        = BT_AT_CMD_ID(CONTROLLER, 0x00),
    BT_AT_CONTROLLER_BT_INQUIRY                                 = BT_AT_CMD_ID(CONTROLLER, 0x01),
    BT_AT_CONTROLLER_BT_SCAN                                    = BT_AT_CMD_ID(CONTROLLER, 0x02),
    BT_AT_CONTROLLER_BT_CONN                                    = BT_AT_CMD_ID(CONTROLLER, 0x03),
    BT_AT_CONTROLLER_BT_DISCONN                                 = BT_AT_CMD_ID(CONTROLLER, 0x04),
    BT_AT_CONTROLLER_BLE_SCAN_PARAM                             = BT_AT_CMD_ID(CONTROLLER, 0x05),
    BT_AT_CONTROLLER_BLE_SCAN                                   = BT_AT_CMD_ID(CONTROLLER, 0x06),
    BT_AT_CONTROLLER_BLE_SCAN_RSP_DATA                          = BT_AT_CMD_ID(CONTROLLER, 0x07),
    BT_AT_CONTROLLER_BLE_ADV_PARAM                              = BT_AT_CMD_ID(CONTROLLER, 0x08),
    BT_AT_CONTROLLER_BLE_ADV_DATA                               = BT_AT_CMD_ID(CONTROLLER, 0x09),
    BT_AT_CONTROLLER_BLE_ADV_START                              = BT_AT_CMD_ID(CONTROLLER, 0x0A),
    BT_AT_CONTROLLER_BLE_ADV_STOP                               = BT_AT_CMD_ID(CONTROLLER, 0x0B),
    BT_AT_CONTROLLER_BLE_CONN                                   = BT_AT_CMD_ID(CONTROLLER, 0x0C),
    BT_AT_CONTROLLER_BLE_CONN_UPDATE                            = BT_AT_CMD_ID(CONTROLLER, 0x0D),
    BT_AT_CONTROLLER_BLE_DISCONN                                = BT_AT_CMD_ID(CONTROLLER, 0x0E),
    BT_AT_CONTROLLER_BLE_DATA_LEN                               = BT_AT_CMD_ID(CONTROLLER, 0x0F),


    /// TEST AT CMD
    BT_AT_TEST_CMD                                              = BT_AT_CMD_ID(TEST, 0x00),
    BT_AT_TEST_BLE_NONSIGNAL_TX                                 = BT_AT_CMD_ID(TEST, 0x01),
    BT_AT_TEST_BLE_NONSIGNAL_RX                                 = BT_AT_CMD_ID(TEST, 0x02),
    BT_AT_TEST_BLE_NONSIGNAL_END                                = BT_AT_CMD_ID(TEST, 0x03),
    BT_AT_TEST_BT_DUT_MODE                                      = BT_AT_CMD_ID(TEST, 0x04),
    BT_AT_TEST_BT_NONSIGNAL_TX                                  = BT_AT_CMD_ID(TEST, 0x05),
    BT_AT_TEST_BT_NONSIGNAL_RX                                  = BT_AT_CMD_ID(TEST, 0x06),
    BT_AT_TEST_BT_NONSIGNAL_DIS                                 = BT_AT_CMD_ID(TEST, 0x07),
    BT_AT_TEST_BT_NONSIGNAL_RX_GET_DATA                         = BT_AT_CMD_ID(TEST, 0x08),
    /// RF TEST TONE CMD
    BT_AT_RF_TEST_TONE_START_CMD                                = BT_AT_CMD_ID(TEST, 0x09),
    BT_AT_RF_TEST_TONE_STOP_CMD                                 = BT_AT_CMD_ID(TEST, 0x10),
    /// BT HCI TEST MODE
    BT_AT_BT_HCI_TEST_CMD                                       = BT_AT_CMD_ID(TEST, 0x11),
};


typedef struct
{
    bool found;
    bool is_complete;
    uint8_t name_length;
    char name[32];
} device_name_result_t;

typedef struct bt_at_cmd
{
    uint16_t        at_id;
    uint16_t        data_len;
    uint8_t         data[__ARRAY_EMPTY];
}bt_at_cmd_t;

/// Test mode parameters structure
typedef struct ble_test_params
{
    /// Type (0: RX | 1: TX)
    uint8_t type;

    /// RF channel, N = (F - 2402) / 2
    uint8_t channel;

    /// Length of test data
    uint8_t data_len;

    /**
     * Packet payload
     * 0x00 PRBS9 sequence "11111111100000111101" (in transmission order) as described in [Vol 6] Part F, Section 4.1.5
     * 0x01 Repeated "11110000" (in transmission order) sequence as described in [Vol 6] Part F, Section 4.1.5
     * 0x02 Repeated "10101010" (in transmission order) sequence as described in [Vol 6] Part F, Section 4.1.5
     * 0x03 PRBS15 sequence as described in [Vol 6] Part F, Section 4.1.5
     * 0x04 Repeated "11111111" (in transmission order) sequence
     * 0x05 Repeated "00000000" (in transmission order) sequence
     * 0x06 Repeated "00001111" (in transmission order) sequence
     * 0x07 Repeated "01010101" (in transmission order) sequence
     * 0x08-0xFF Reserved for future use
     */
    uint8_t payload;

    /**
     * Tx/Rx PHY
     * For Tx PHY:
     * 0x00 Reserved for future use
     * 0x01 LE 1M PHY
     * 0x02 LE 2M PHY
     * 0x03 LE Coded PHY with S=8 data coding
     * 0x04 LE Coded PHY with S=2 data coding
     * 0x05-0xFF Reserved for future use
     * For Rx PHY:
     * 0x00 Reserved for future use
     * 0x01 LE 1M PHY
     * 0x02 LE 2M PHY
     * 0x03 LE Coded PHY
     * 0x04-0xFF Reserved for future use
     */
    uint8_t phy;

     /**
     * 0x00 fhss not supported
     * 0x01 fhss supported
     */
    uint8_t  fhss;

    /**
     * CTE length
     * 0x00 No Constant Tone Extension
     * 0x02 - 0x14 Length of the Constant Tone Extension in 8 us units
     * All other values Reserved for future use
     */
    uint8_t cte_len;

    /**
     * CTE type
     * 0x00 AoA Constant Tone Extension
     * 0x01 AoD Constant Tone Extension with 1 us slots
     * 0x02 AoD Constant Tone Extension with 2 us slots
     * All other values Reserved for future use
     */
    uint8_t cte_type;

    /**
     * Slot durations
     * 0x01 Switching and sampling slots are 1 us each
     * 0x02 Switching and sampling slots are 2 us each
     * All other values Reserved for future use
     */
    uint8_t slot_dur;

    /**
     * Length of switching pattern
     * 0x02 - 0x4B The number of Antenna IDs in the pattern
     * All other values Reserved for future use
     */
    uint8_t switching_pattern_len;

    /// Antenna IDs
    uint8_t antenna_id[BLE_MAX_SW_PAT_LEN];

    /// Transmit power level in dBm (0x7E: minimum | 0x7F: maximum | range: -127 to +20)
    int8_t  tx_pwr_lvl;
    /// infinite rx test mode
    uint8_t infinite_rx_mode;
}ble_test_params_t;

typedef struct ble_test_scan_en_cmd
{
    ///Status of the scan enable
    uint8_t scan_en;
}ble_test_scan_en_cmd_t;

typedef struct rf_test_tone_start_cmd
{
    uint16_t channel;
    uint8_t power;
}rf_test_tone_start_cmd_t;


typedef struct
{
    ///Scan type - 0=passive / 1=active
    uint8_t        scan_type;
    ///Scan interval
    uint16_t       scan_intv;
    ///Scan window size
    uint16_t       scan_window;
    ///Own address type - public=0 / random=1 / rpa_or_pub=2 / rpa_or_rnd=3
    uint8_t        own_addr_type;
    ///Scan filter policy
    uint8_t        scan_filt_policy;
}ble_scan_params_t;

typedef struct
{
    ///0 disable, 1 enable
    uint8_t        enable;
    ///
    uint16_t       intv;
    ///
    uint16_t       filter_type;
    ///
    uint8_t        filter_param[__ARRAY_EMPTY];
}ble_scan_t;

typedef struct
{
    ///Scan enable - 0=disabled, 1=enabled
    uint8_t        scan_en;
    ///Enable for duplicates filtering - 0 =disabled/ 1=enabled
    uint8_t        filter_duplic;
}ble_scan_en_t;


///Scan response data structure
/*@TRACE*/
struct out_scan_rsp_data
{
    ///Maximum length data bytes array
    uint8_t        data[SCAN_RSP_DATA_LEN];
};


typedef struct
{
    ///
    uint8_t       data_len;
    ///
    struct out_scan_rsp_data rsp_data;
    ///
    uint8_t        type;
}ble_scan_rspdata_t;


typedef struct
{
    ///
    uint8_t    adv_type;
    ///
    uint8_t    adv_mode;
    ///
    uint16_t   adv_int_min;
    ///
    uint16_t   adv_int_max;
}ble_adv_param_t;


struct hci_out_le_set_adv_param_cmd
{
    ///Minimum interval for advertising
    uint16_t       adv_intv_min;
    ///Maximum interval for advertising
    uint16_t       adv_intv_max;
    ///Advertising type
    uint8_t        adv_type;
    ///Own address type:  public=0 / random=1 / rpa_or_pub=2 / rpa_or_rnd=3
    uint8_t        own_addr_type;
    ///Peer address type: public=0 / random=1
    uint8_t        peer_addr_type;
    ///Peer Bluetooth device address
    struct out_bd_addr peer_addr;
    ///Advertising channel map
    uint8_t        adv_chnl_map;
    ///Advertising filter policy
    uint8_t        adv_filt_policy;
};


struct out_adv_data
{
    ///Maximum length data bytes array
    uint8_t        data[ADV_DATA_LEN];
};


typedef struct
{
    ///
    uint16_t   data_len;
    ///
    struct out_adv_data data;
    ///
    uint8_t    adv_type;
}ble_adv_data_t;


typedef struct
{
    ///0 disable 1 enable
    uint8_t adv_en;
}ble_adv_en_t;

typedef struct
{
    ///
    uint8_t  addr_type;
    ///
    struct out_bd_addr  remote_addr;
    ///
    uint16_t timeout;
}ble_conn_t;

struct hci_out_le_create_con_cmd
{
    ///Scan interval (N * 0.625 ms)
    uint16_t       scan_intv;
    ///Scan window size (N * 0.625 ms)
    uint16_t       scan_window;
    ///Initiator filter policy
    uint8_t        init_filt_policy;
    ///Peer address type - public=0 / random=1 / rpa_or_pub=2 / rpa_or_rnd=3
    uint8_t        peer_addr_type;
    ///Peer BD address
    struct out_bd_addr peer_addr;
    ///Own address type - public=0 / random=1 / rpa_or_pub=2 / rpa_or_rnd=3
    uint8_t        own_addr_type;
    ///Minimum of connection interval (N * 1.25 ms)
    uint16_t       con_intv_min;
    ///Maximum of connection interval (N * 1.25 ms)
    uint16_t       con_intv_max;
    ///Connection latency
    uint16_t       con_latency;
    ///Link supervision timeout
    uint16_t       superv_to;
    ///Minimum CE length (N * 0.625 ms)
    uint16_t       ce_len_min;
    ///Maximum CE length (N * 0.625 ms)
    uint16_t       ce_len_max;
};



typedef struct
{
    ///
    uint16_t  conn_index;
    ///
    uint16_t min_interval;
    ///
    uint16_t max_interval;
    ///
    uint16_t con_latency;
    ///
    uint16_t timeout;
}ble_conn_update_t;


typedef struct
{
    ///
    uint8_t  addr_type;
    ///
    struct out_bd_addr  remote_addr;
}ble_disconn_t;

typedef struct
{
    ///
    uint8_t  addr_type;
    ///
    struct out_bd_addr  remote_addr;
}bt_disconn_t;


typedef struct
{
    /// connection handle
    uint16_t    conhdl;
    /// reason @see enum co_error
    uint8_t     reason;
}hci_disconnect_cmd_t;


typedef struct
{
    ///
    uint16_t  conn_index;
    ///
    uint16_t  pkt_data_len;
    ///
    uint16_t  tx_time;
}ble_data_len_t;

typedef struct
{
    ///
    uint16_t  auth_req;
    ///
    uint16_t  iocap;
    ///
    uint16_t  key_size;
    ///
    uint16_t  init_key;
    ///
    uint16_t  rsp_key;
}ble_sec_param_t;


typedef struct
{
    ///
    uint16_t  conn_index;
    ///1: SEC_NONE 2:SEC_ENCRYPT 3:SEC_ENCRYPT_NO_MIMT 4:SEC_ENCRYPT_MIMT
    uint16_t  sec_act;
}ble_enc_t;

struct out_ltk
{
    ///16-byte array for LTK value
    uint8_t ltk[KEY_LEN];
};


typedef struct
{
    ///
    uint8_t  conn_index;
    ///
    struct out_ltk key;
}ble_key_reply_t;



typedef struct
{
    ///
    uint8_t  type;
    ///
    struct   out_bd_addr bd_addr;
}ble_enc_clear_t;




/// lap structure
/*@TRACE*/
struct out_lap
{
    /// LAP
    uint8_t A[BD_ADDR_LAP_LEN];
};

typedef struct
{
    ///Lap
    struct out_lap  lap;
    ///Inquiry Length in units of 1.28 s
    uint8_t     inq_len;
    ///Number of response
    uint8_t     nb_rsp;

}bt_inq_t;

typedef struct
{
    /// BdAddr
    struct out_bd_addr  bd_addr;
    /// Packet Type
    uint16_t        pkt_type;
    /// Page Scan Repetition Mode
    uint8_t         page_scan_rep_mode;
    /// Reserved
    uint8_t         rsvd;
    /**
     * Clock Offset
     *
     * Bits 14-0 : Bits 16-2 of CLKNslave-CLK
     * Bit 15 : Clock_Offset_Valid_Flag
     *   Invalid Clock Offset = 0
     *   Valid Clock Offset = 1
     */
    uint16_t        clk_off;
    /// Allow Switch
    uint8_t         switch_en;


}bt_conn_t;


typedef struct
{
    ///
    uint8_t     pkt_type;
    ///
    uint16_t    pkt_len;
    ///
    uint8_t     pkt_per;
    ///
    uint8_t     pattern;
    ///
    uint8_t     tx_ch;
    ///
    uint8_t     tx_power;
    ///
    uint8_t     tx_value;
}bt_non_signal_tx_t;

typedef struct
{
    ///
    struct out_bd_addr peer_bd_addr;
    ///
    uint8_t        pkt_type;
    ///
    uint8_t        rx_ch;
    ///
    uint8_t        infinite_mode;
}bt_non_signal_rx_t;

/*
 * ENUMERATIONS
 ****************************************************************************************
 */

/*
 * GLOBAL VARIABLE DECLARATIONS
 ****************************************************************************************
 */
uint8_t atcmd_ble_init_send(uint8_t init);
uint8_t atcmd_ble_scan_param_send(ble_scan_params_t *params);
uint8_t atcmd_ble_scan_send(ble_scan_t *params);
uint8_t atcmd_ble_scan_rsp_data_send(ble_scan_rspdata_t *params);
uint8_t atcmd_ble_adv_param_send(ble_adv_param_t *params);
uint8_t atcmd_ble_adv_data_send(ble_adv_data_t *params);
uint8_t atcmd_ble_adv_start_send(ble_adv_en_t *params);
uint8_t atcmd_ble_adv_stop_send(ble_adv_en_t *params);
uint8_t atcmd_ble_conn_send(ble_conn_t *params);
uint8_t atcmd_ble_conn_update_send(ble_conn_update_t *params);
uint8_t atcmd_ble_disconn_send(ble_disconn_t *params);
uint8_t atcmd_ble_data_len_send(ble_data_len_t *params);
uint8_t atcmd_ble_sec_param_send(ble_sec_param_t *params);
uint8_t atcmd_ble_enc_send(ble_enc_t *params);
uint8_t atcmd_ble_key_reply_send(ble_key_reply_t *params);
uint8_t atcmd_ble_enc_clear_send(ble_enc_clear_t *params);

uint8_t atcmd_bt_inquiry_send(bt_inq_t *params);
uint8_t atcmd_bt_conn_send(bt_conn_t *params);
uint8_t atcmd_bt_disconn_send(bt_disconn_t *params);
uint8_t atcmd_bt_non_signal_tx_send(bt_non_signal_tx_t *params);
uint8_t atcmd_bt_non_signal_rx_send(bt_non_signal_rx_t *params);
uint8_t atcmd_bt_non_signal_disable_send(void);
uint8_t atcmd_bt_non_signal_rx_get_data_send(void);

uint8_t atcmd_ble_nonsignal_tx_send(uint8_t channel, uint8_t data_len, uint8_t payload, uint8_t phy, uint8_t fhss);
uint8_t atcmd_ble_nonsignal_rx_send(uint8_t channel, uint8_t phy, uint8_t mod_idx, uint8_t infinite_rx_mode);
uint8_t atcmd_ble_nonsignal_end_send(void);
uint8_t atcmd_bt_scan_send(uint8_t enable);
uint8_t atcmd_bt_dutmode_send(uint8_t enable);
uint8_t atcmd_hble_adv_start_send(uint8_t modes);
uint8_t atcmd_hble_adv_stop_send(void);
uint8_t atcmd_rf_test_tone_start_send(uint16_t channel, uint8_t power);
uint8_t atcmd_rf_test_tone_stop_send();
uint8_t atcmd_bt_hci_mode_send();
void bt_at_cmd_msg_handle(bt_at_cmd_t* msg);

/// @} BT OS TASK
#endif // BT_AT_IF_H_


#ifndef __LS_BT_TYPE_H_
#define __LS_BT_TYPE_H_

#pragma once

#include <stdbool.h>


#ifndef __PACKED
#define __PACKED __attribute__ ((__packed__))
#endif

#define BD_ADDR_LEN         6
#define DEV_CLASS_LEN       3
#define EIR_DATA_SIZE         240
#define BLE_ADV_REPORTS_MAX              (1)
#define ADV_DATA_LEN        0x1F
#define EXT_ADV_DATA_MAX_LEN    229 // HCI:7.7.65.13


/**************************/
/// bt event id define
typedef enum
{
    EVENT_BLE_INIT_DONE = 1,
    EVENT_BLE_CONNECTED,
    EVENT_BLE_DISCONNECT,
    EVENT_BLE_SCAN,
    EVENT_BLE_SCAN_ADV_REPORT,
    EVENT_BLE_SCAN_EXT_ADV_REPORT,
    EVENT_BLE_SCAN_DIR_ADV_REPORT,
    EVENT_BLE_DISCOVERY,
    EVENT_BLE_NON_SIGNAL_END,
    EVENT_BT_CONNECTED,
    EVENT_BT_DISCONNECT,
    EVENT_BT_INQUIRY,
    EVENT_BT_INQ_RESULT,
    EVENT_BT_INQ_RSSI_RESULT,
    EVENT_BT_INQ_EIR_RESULT,
    EVENT_BT_SCAN,
    EVENT_BT_NON_SIGNAL_END,
    EVENT_BT_NON_SIGNAL_RX_GET_DATA,

    EVENT_BT_MAX = 0xFF,
} bt_event_id_e;


typedef struct
{
    /// Status (BLE error code)
    uint8_t status;
    /// Number of packets received
    uint16_t nb_pkt_recv;

}event_ble_non_signal_end_param_t;



struct out_bd_addr
{
    ///6-byte array address value
    uint8_t  addr[BD_ADDR_LEN];
};

struct out_devclass
{
    /// class
    uint8_t A[DEV_CLASS_LEN];
};

typedef struct
{
    ///Number of response
    uint8_t     nb_rsp;
    ///BdAddr
    struct out_bd_addr bd_addr;
    ///Page Scan Repetition Mode
    uint8_t     page_scan_rep_mode;
    ///Reserved
    uint8_t     reserved1;
    ///Reserved
    uint8_t     reserved2;
    ///class of device
    struct out_devclass class_of_dev;
    ///Clock Offset
    uint16_t        clk_off;

}event_bt_inq_result_param_t;


typedef struct
{
    ///Number of response
    uint8_t     nb_rsp;
    ///BdAddr
    struct out_bd_addr  bd_addr;
    ///Page Scan Repetition Mode
    uint8_t     page_scan_rep_mode;
    ///Reserved
    uint8_t     reserved1;
    ///class of device
    struct out_devclass class_of_dev;
    ///Clock Offset
    uint16_t     clk_off;
    ///RSSI
    int8_t       rssi;

}event_bt_inq_rssi_result_param_t;

struct out_eir
{
    /// eir data
    uint8_t data[EIR_DATA_SIZE];
};


typedef struct
{
    ///Number of response
    uint8_t     nb_rsp;
    ///BdAddr
    struct out_bd_addr  bd_addr;
    ///Page Scan Repetition Mode
    uint8_t     page_scan_rep_mode;
    ///Reserved
    uint8_t     reserved1;
    ///class of device
    struct out_devclass class_of_dev;
    ///Clock Offset
    uint16_t        clk_off;
    ///RSSI
    int8_t          rssi;
    ///Extended inquiry response data
    struct out_eir      eir;

}event_bt_inq_eir_result_param_t;


typedef struct
{
    /// Status of the command reception
    uint8_t             status;
    ///received total packets
    uint32_t            total_packet;
    ///received error packets
    uint32_t            error_packet;
    ///received total bits
    uint32_t            total_bit;
    ///received error bits
    uint32_t            error_bit;
}event_bt_non_signal_get_rx_data_param_t;

struct adv_out_report
{
    ///Event type:
    /// - ADV_CONN_UNDIR: Connectable Undirected advertising
    /// - ADV_CONN_DIR: Connectable directed advertising
    /// - ADV_DISC_UNDIR: Discoverable undirected advertising
    /// - ADV_NONCONN_UNDIR: Non-connectable undirected advertising
	uint8_t        evt_type;
    ///Advertising address type: public/random
    uint8_t        adv_addr_type;
    ///Advertising address value
    struct out_bd_addr adv_addr;
    ///Data length in advertising packet
    uint8_t        data_len;
    ///Data of advertising packet
    uint8_t        data[ADV_DATA_LEN];
    ///RSSI value for advertising packet (in dBm, between -127 and +20 dBm)
    int8_t         rssi;
};

struct ext_adv_out_report
{
    ///Event type
    uint16_t       evt_type;
    ///Advertising address type: public/random
    uint8_t        adv_addr_type;
    ///Advertising address value
    struct out_bd_addr adv_addr;
    ///Primary PHY
    uint8_t        phy;
    ///Secondary PHY
    uint8_t        phy2;
    ///Advertising SID
    uint8_t        adv_sid;
    ///Tx Power
    uint8_t        tx_power;
    ///RSSI value for advertising packet (in dBm, between -127 and +20 dBm)
    int8_t         rssi;
    ///Periodic Advertising interval (Time=N*1.25ms)
    uint16_t       interval;
    ///Direct address type
    uint8_t        dir_addr_type;
    ///Direct address value
    struct out_bd_addr dir_addr;
    ///Data length in advertising packet
    uint8_t        data_len;
    ///Data of advertising packet
    uint8_t        data[EXT_ADV_DATA_MAX_LEN];
};



typedef struct
{
    ///LE Subevent code
    uint8_t             subcode;
    ///Number of advertising reports in this event
    uint8_t             nb_reports;
    ///Advertising reports structures array
    struct adv_out_report   adv_rep[BLE_ADV_REPORTS_MAX];
}event_ble_scan_adv_report_param_t;


typedef struct
{
    ///LE Subevent code
    uint8_t             subcode;
    ///Number of advertising reports in this event
    uint8_t             nb_reports;
    ///Advertising reports structures array
    struct ext_adv_out_report   adv_rep[BLE_ADV_REPORTS_MAX];
}event_ble_scan_ext_adv_report_param_t;


struct dir_adv_out_report
{
    ///Event type:
    /// - ADV_CONN_DIR: Connectable directed advertising
    uint8_t        evt_type;
    ///Address type: public/random
    uint8_t        addr_type;
    ///Address value
    struct out_bd_addr addr;
    ///Direct address type: public/random
    uint8_t        dir_addr_type;
    ///Direct address value
    struct out_bd_addr dir_addr;
    ///RSSI value for advertising packet (in dBm, between -127 and +20 dBm)
    int8_t         rssi;
};


typedef struct
{
    ///LE Subevent code
    uint8_t             subcode;
    ///Number of advertising reports in this event
    uint8_t             nb_reports;
    ///Advertising reports structures array
    struct dir_adv_out_report   adv_rep[BLE_ADV_REPORTS_MAX];
}event_ble_scan_dir_adv_report_param_t;


#endif


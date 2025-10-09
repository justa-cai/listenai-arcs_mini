#ifndef __BT_API_H_
#define __BT_API_H_



/**
 ****************************************************************************************
 * @addtogroup CO_ERROR Error Codes
 * @ingroup COMMON
 * @brief Defines error codes in messages.
 *
 * @{
 ****************************************************************************************
 */

///HCI enumeration of possible Command OP Codes.
/*@TRACE*/
enum hci_opcode
{
    HCI_CREATE_CON_CMD_OPCODE                 = 0x0405,
    HCI_DISCONNECT_CMD_OPCODE                 = 0x0406,

    //Controller and Baseband Commands
    HCI_WR_SCAN_EN_CMD_OPCODE                 = 0x0C1A,
    HCI_EN_DUT_MODE_CMD_OPCODE                = 0x1803,

};


/*
 * DEFINES
 ****************************************************************************************
 */
enum co_error
{
/*****************************************************
 ***              ERROR CODES                      ***
 *****************************************************/

    CO_ERROR_NO_ERROR                        = 0x00,
    CO_ERROR_UNKNOWN_HCI_COMMAND             = 0x01,
    CO_ERROR_UNKNOWN_CONNECTION_ID           = 0x02,
    CO_ERROR_HARDWARE_FAILURE                = 0x03,
    CO_ERROR_PAGE_TIMEOUT                    = 0x04,
    CO_ERROR_AUTH_FAILURE                    = 0x05,
    CO_ERROR_PIN_MISSING                     = 0x06,
    CO_ERROR_MEMORY_CAPA_EXCEED              = 0x07,
    CO_ERROR_CON_TIMEOUT                     = 0x08,
    CO_ERROR_CON_LIMIT_EXCEED                = 0x09,
    CO_ERROR_SYNC_CON_LIMIT_DEV_EXCEED       = 0x0A,
    CO_ERROR_CON_ALREADY_EXISTS              = 0x0B,
    CO_ERROR_COMMAND_DISALLOWED              = 0x0C,
    CO_ERROR_CONN_REJ_LIMITED_RESOURCES      = 0x0D,
    CO_ERROR_CONN_REJ_SECURITY_REASONS       = 0x0E,
    CO_ERROR_CONN_REJ_UNACCEPTABLE_BDADDR    = 0x0F,
    CO_ERROR_CONN_ACCEPT_TIMEOUT_EXCEED      = 0x10,
    CO_ERROR_UNSUPPORTED                     = 0x11,
    CO_ERROR_INVALID_HCI_PARAM               = 0x12,
    CO_ERROR_REMOTE_USER_TERM_CON            = 0x13,
    CO_ERROR_REMOTE_DEV_TERM_LOW_RESOURCES   = 0x14,
    CO_ERROR_REMOTE_DEV_POWER_OFF            = 0x15,
    CO_ERROR_CON_TERM_BY_LOCAL_HOST          = 0x16,
    CO_ERROR_REPEATED_ATTEMPTS               = 0x17,
    CO_ERROR_PAIRING_NOT_ALLOWED             = 0x18,
    CO_ERROR_UNKNOWN_LMP_PDU                 = 0x19,
    CO_ERROR_UNSUPPORTED_REMOTE_FEATURE      = 0x1A,
    CO_ERROR_SCO_OFFSET_REJECTED             = 0x1B,
    CO_ERROR_SCO_INTERVAL_REJECTED           = 0x1C,
    CO_ERROR_SCO_AIR_MODE_REJECTED           = 0x1D,
    CO_ERROR_INVALID_LMP_PARAM               = 0x1E,
    CO_ERROR_UNSPECIFIED_ERROR               = 0x1F,
    CO_ERROR_UNSUPPORTED_LMP_PARAM_VALUE     = 0x20,
    CO_ERROR_ROLE_CHANGE_NOT_ALLOWED         = 0x21,
    CO_ERROR_LMP_RSP_TIMEOUT                 = 0x22,
    CO_ERROR_LMP_COLLISION                   = 0x23,
    CO_ERROR_LMP_PDU_NOT_ALLOWED             = 0x24,
    CO_ERROR_ENC_MODE_NOT_ACCEPT             = 0x25,
    CO_ERROR_LINK_KEY_CANT_CHANGE            = 0x26,
    CO_ERROR_QOS_NOT_SUPPORTED               = 0x27,
    CO_ERROR_INSTANT_PASSED                  = 0x28,
    CO_ERROR_PAIRING_WITH_UNIT_KEY_NOT_SUP   = 0x29,
    CO_ERROR_DIFF_TRANSACTION_COLLISION      = 0x2A,
    CO_ERROR_QOS_UNACCEPTABLE_PARAM          = 0x2C,
    CO_ERROR_QOS_REJECTED                    = 0x2D,
    CO_ERROR_CHANNEL_CLASS_NOT_SUP           = 0x2E,
    CO_ERROR_INSUFFICIENT_SECURITY           = 0x2F,
    CO_ERROR_PARAM_OUT_OF_MAND_RANGE         = 0x30,
    CO_ERROR_ROLE_SWITCH_PEND                = 0x32, /* LM_ROLE_SWITCH_PENDING               */
    CO_ERROR_RESERVED_SLOT_VIOLATION         = 0x34, /* LM_RESERVED_SLOT_VIOLATION           */
    CO_ERROR_ROLE_SWITCH_FAIL                = 0x35, /* LM_ROLE_SWITCH_FAILED                */
    CO_ERROR_EIR_TOO_LARGE                   = 0x36, /* LM_EXTENDED_INQUIRY_RESPONSE_TOO_LARGE */
    CO_ERROR_SP_NOT_SUPPORTED_HOST           = 0x37,
    CO_ERROR_HOST_BUSY_PAIRING               = 0x38,
    CO_ERROR_CONTROLLER_BUSY                 = 0x3A,
    CO_ERROR_UNACCEPTABLE_CONN_PARAM         = 0x3B,
    CO_ERROR_ADV_TO                          = 0x3C,
    CO_ERROR_TERMINATED_MIC_FAILURE          = 0x3D,
    CO_ERROR_CONN_FAILED_TO_BE_EST           = 0x3E,
    CO_ERROR_CCA_REJ_USE_CLOCK_DRAG          = 0x40,
    CO_ERROR_TYPE0_SUBMAP_NOT_DEFINED        = 0x41,
    CO_ERROR_UNKNOWN_ADVERTISING_ID          = 0x42,
    CO_ERROR_LIMIT_REACHED                   = 0x43,
    CO_ERROR_OPERATION_CANCELED_BY_HOST      = 0x44,
    CO_ERROR_PKT_TOO_LONG                    = 0x45,

    CO_ERROR_UNDEFINED                       = 0xFF,


/*****************************************************
 ***              HW ERROR CODES                   ***
 *****************************************************/

    CO_ERROR_HW_UART_OUT_OF_SYNC            = 0x00,
    CO_ERROR_HW_MEM_ALLOC_FAIL              = 0x01,
};


#define BD_ADDR_LAP_LEN     3
#define BD_ADDR_LEN         6


struct lap
{
    /// LAP
    uint8_t A[BD_ADDR_LAP_LEN];
};

struct bd_addr
{
    ///6-byte array address value
    uint8_t  addr[BD_ADDR_LEN];
};


/*@TRACE*/
struct hci_wr_scan_en_cmd
{
    ///Status of the scan enable
    uint8_t scan_en;
};

struct ld_inquiry_params
{
    /// LAP to be used for the access code construction (GIAC or DIAC)
    struct lap lap;
    /// Minimum duration between consecutive inquiries in number of 1.28 seconds (0 for non-periodic inquiry)
    uint16_t per_min;
    /// Maximum duration between consecutive inquiries in number of 1.28 seconds (0 for non-periodic inquiry)
    uint16_t per_max;
    /// Length of the inquiry in number of 1.28 seconds (2048 slots)
    uint16_t inq_len;
    /// Max number of responses before halting the inquiry
    uint8_t nb_rsp_max;
    /// Inquiry TX power level (in dBm)
    int8_t tx_pwr_lvl;
    /// Enable/disable EIR response
    bool eir_en;
    /// Ninquiry
    uint16_t n_inq;
};

/// Inquiry Scan parameters structure
struct ld_inquiry_scan_params
{
    /// LAP to be used for the access code construction (GIAC or DIAC)
    struct lap lap;
    /// Amount of time between consecutive inquiry scans in slots (625 us)
    uint16_t iscan_intv;
    /// Amount of time for the duration of the inquiry scan in slots (625 us)
    uint16_t iscan_win;
    /// Inquiry Scan type
    uint8_t iscan_type;
    /// Page scan repetition mode
    uint8_t page_scan_rep_mode;
};

struct hci_create_con_cmd
{
    /// BdAddr
    struct bd_addr  bd_addr;
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
};

struct hci_disconnect_cmd
{
    /// connection handle
    uint16_t    conhdl;
    /// reason @see enum co_error
    uint8_t     reason;
};

struct hci_nonsig_tx_enable_cmd
{
    uint8_t pkt_type; //packet type
    uint16_t pkt_len; // packet length
    uint8_t packet_period; //packet period, default one frame 1
    uint8_t pattern;     // payload pattern 0xaa 0x55 PRBS9 PRBS15
    uint8_t tx_ch;       //rx_channel:0~78  0xff afh
    uint8_t tx_power;  // 0 index  1 control word
    uint8_t tx_value;
};

struct hci_nonsig_rx_enable_cmd
{
    struct bd_addr peer_addr;   // peer_addr
    uint8_t pkt_type;           //packet type
    uint8_t rx_ch;              //rx_channel:0~78  0xff afh
    uint8_t infinite_rx_mode;
};



void lsip_reset(void);


uint8_t ld_inq_start(struct ld_inquiry_params* params);


void hci_cmd_received(uint16_t opcode, uint8_t length, uint8_t *payload);


uint8_t ld_iscan_stop(void);


uint8_t ld_pscan_stop(void);




#endif


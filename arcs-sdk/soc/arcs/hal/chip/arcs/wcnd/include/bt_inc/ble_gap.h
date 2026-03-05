/**
 ****************************************************************************************
 *
 * @file ble_gap.h
 *
 * @brief Header file - BLE GAP External API
 *
 * Copyright (C) ListenAI 2022-2042
 ****************************************************************************************
 */

#ifndef BLE_GAP_H_
#define BLE_GAP_H_

/**
 ****************************************************************************************
 * @addtogroup GAP External API
 *
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */


/*
 * MACRO DEFINITIONS
 ****************************************************************************************
 */
#ifndef __ARRAY_EMPTY
#define __ARRAY_EMPTY
#endif


/*
 * DEFINES
 ****************************************************************************************
 */
#define GAP_BD_ADDR_LEN       (6)
#define GAP_NAME_LEN          (50)
#define GAP_KEY_LEN           (16)

/// No error
#define GAP_NO_ERROR                                    0x00
/// Invalid parameters set
#define GAP_INVALID_PARAM                               0x40
/// Request not supported by software configuration
#define GAP_NOT_SUPPORTED                               0x42
/// Request not allowed in current state.
#define GAP_COMMAND_DISALLOWED                          0x43

#define GAP_ADV_ID_0           (0)
#define GAP_ADV_ID_1           (1)
#define GAP_ADV_ID_2           (2)

#define GAP_SCAN_ID_0          (0)
#define GAP_SCAN_ID_1          (1)
#define GAP_SCAN_ID_2          (2)

#define GAP_INIT_ID_0          (0)
#define GAP_INIT_ID_1          (0)



enum gap_event_type
{
    GAPM_SET_WL_EVENT  = 0x53,
    GAPM_SET_RAL_EVENT = 0x54,
    GAPM_SET_PAL_EVENT = 0x55,
};

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */

/// for get info
enum gap_dev_info_type
{
    GAP_INFO_BDADDR,
    GAP_INFO_NAME,
    GAP_INFO_APPEARANCE,
    GAP_INFO_VERSION,
    GAP_INFO_FETURES,
    GAP_INFO_RSSI,
    GAP_INFO_WHITE_LIST_SIZE,
    GAP_INFO_RAL_LIST_SIZE,
    GAP_INFO_PAL_LIST_SIZE,
};


/// IO Capability Values
enum gap_io_cap
{
    /// Display Only
    GAP_IO_CAP_DISPLAY_ONLY = 0x00,
    /// Display Yes No
    GAP_IO_CAP_DISPLAY_YES_NO,
    /// Keyboard Only
    GAP_IO_CAP_KB_ONLY,
    /// No Input No Output
    GAP_IO_CAP_NO_INPUT_NO_OUTPUT,
    /// Keyboard Display
    GAP_IO_CAP_KB_DISPLAY,
    GAP_IO_CAP_LAST
};

/// Appearance type
enum gap_appearance_type
{
    GAP_APP_UNKNOWN                    =  0,
    GAP_APP_GENERIC_PHONE              =  64,
    GAP_APP_GENERIC_COMPUTER           =  128,
    GAP_APP_GENERIC_WATCH              =  192,
    GAP_APP_SPORTS_WATCH               =  193,
    GAP_APP_GENERIC_CLOCK              =  256,
    GAP_APP_GENERIC_DISPLAY            =  320,
    GAP_APP_GENERIC_REMOTE_CONTROL     =  384,
    GAP_APP_GENERIC_EYE_GLASSES        =  448,
    GAP_APP_GENERIC_TAG                =  512,
    GAP_APP_GENERIC_KEYRING            =  576,
    GAP_APP_GENERIC_MEDIA_PLAYER       =  640,
    GAP_APP_GENERIC_BARCODE_SCANNER    =  704,
    GAP_APP_GENERIC_THERMOMETER        =  768,
    GAP_APP_EAR_THERMOMETER            =  769,
    GAP_APP_GENERIC_HEART_RATE_SENSOR  =  832,
    GAP_APP_HEART_RATE_BELT            =  833,
    GAP_APP_GENERIC_BLOOD_PRESSURE     =  896,
    GAP_APP_BLOOD_PRESSURE_ARM         =  897,
    GAP_APP_BLOOD_PRESSURE_WRIST       =  898,
    GAP_APP_HUMAN_INTERFACE_DEVICE     =  960,
    GAP_APP_HID_KEYBOARD               =  961,
    GAP_APP_HID_MOUSE                  =  962,
    GAP_APP_HID_JOYSTICK               =  963,
    GAP_APP_HID_GAMEPAD                =  964,
    GAP_APP_HID_DIGITIZER_TABLET       =  965,
    GAP_APP_HID_CARD_READER            =  966,
    GAP_APP_HID_DIGITAL_PEN            =  967,
    GAP_APP_HID_BARCODE_SCANNER        =  968,
    GAP_APP_GLUCOSE_METER              =  1024,
    GAP_APP_RUNNING_WALKING_SENSOR     =  1088,
    GAP_APP_RUNNING_WALKING_IN_SHOE    =  1089,
    GAP_APP_RUNNING_WALKING_ON_SHOE    =  1090,
    GAP_APP_RUNNING_WALKING_ON_HIP     =  1091,
    GAP_APP_CYCLING                    =  1152,
    GAP_APP_CYCLING_COMPUTER           =  1153,
    GAP_APP_CYCLING_SPEED_SENSOR       =  1154,
    GAP_APP_CYCLING_CADENCE_SENSOR     =  1155,
    GAP_APP_CYCLING_POWER_SENSOR       =  1156,
    GAP_APP_CYCLING_SPEED_AND_CADENCE  =  1157,
    GAP_APP_GENERIC_PULSE_OXIMETER     =  3136,
    GAP_APP_FINGERTIP_PULSE_OXIMETER   =  3137,
    GAP_APP_WRISTWORN_PULSE_OXIMETER   =  3138,
    GAP_APP_GENERIC_OUTDOOR_SPORTS     =  5184,
    GAP_APP_LOCDISPDEV_OUTDOOR_SPORTS  =  5185,
    GAP_APP_LOCNAVIDEV_OUTDOOR_SPORTS  =  5186,
    GAP_APP_LOCPOD_OUTDOOR_SPORTS      =  5187,
    GAP_APP_LOCNAVIPOD_OUTDOOR_SPORTS  =  5188,
};
/// class bt of device type
enum gap_class_bt_type
{
    /// bt handfree
    GAP_APP_HANDSFREE                  = 0x200408,
    /// bt speaker
    GAP_APP_SPEAKER                    = 0x200414,
    /// LE headphones
    GAP_APP_LE_HEADPHONES              = 0x204418,
    /// LE audio HIFI
    GAP_APP_LE_HIFI                    = 0x204428,
    /// bt helmet
    GAP_APP_HELMET                     = 0x200710,
    /// bt remote controller
    GAP_APP_REMOTE_CONTROL             = 0x28054c,
    /// bt joystick with audio
    GAP_APP_JOYSTICK                   = 0x2805c4,
};

/// TK Type
enum gap_tk_type
{
    ///  TK get from out of band method
    GAP_TK_OOB         = 0x00,
    /// Displayed given TK in local device
    GAP_TK_DISPLAY,
    /// TK generated and shall be displayed by local device
    GAP_TK_GEN,
    /// TK shall be entered by user using device keyboard
    GAP_TK_KEY_ENTRY,
    /// display and compare the given TK
    GAP_TK_COMPARE,
};

/// Bit field use to select the preferred TX or RX LE PHY. 0 means no preferences
enum gap_phy
{
    /// No preferred PHY
    GAP_PHY_ANY               = 0x00,
    /// LE 1M PHY preferred for an active link
    GAP_PHY_LE_1MBPS          = (1 << 0),
    /// LE 2M PHY preferred for an active link
    GAP_PHY_LE_2MBPS          = (1 << 1),
    /// LE Coded PHY preferred for an active link
    GAP_PHY_LE_CODED          = (1 << 2),
};


/// Enumeration of TX/RX PHY values
enum gap_phy_val
{
    /// LE 1M PHY (TX or RX)
    GAP_PHY_1MBPS        = 1,
    /// LE 2M PHY (TX or RX)
    GAP_PHY_2MBPS        = 2,
    /// LE Coded PHY (RX Only)
    GAP_PHY_CODED        = 3,
    /// LE Coded PHY with S=8 data coding (TX Only)
    GAP_PHY_125KBPS      = 3,
    /// LE Coded PHY with S=2 data coding (TX Only)
    GAP_PHY_500KBPS      = 4,
};

///Advertising filter policy
enum gap_adv_filter_policy
{
    ///Allow both scan and connection requests from anyone
    GAP_ADV_SCAN_ANY_CON_ANY    = 0x00,
    ///Allow both scan req from White List devices only and connection req from anyone
    GAP_ADV_SCAN_WLST_CON_ANY,
    ///Allow both scan req from anyone and connection req from White List devices only
    GAP_ADV_SCAN_ANY_CON_WLST,
    ///Allow scan and connection requests from White List devices only
    GAP_ADV_SCAN_WLST_CON_WLST,
};

/// Generic Security key structure
typedef struct gap_sec_key
{
    /// Key value MSB -> LSB
    uint8_t key[GAP_KEY_LEN];
} gap_sec_key_t;

/// Bluetooth address
/*@TRACE*/
typedef struct gap_addr
{
    /// BD Address of device
    uint8_t addr[GAP_BD_ADDR_LEN];
} gap_addr_t;

/// Address information about a device address
/*@TRACE*/
typedef struct gap_bdaddr
{
    /// BD Address of device
    uint8_t addr[GAP_BD_ADDR_LEN];
    /// Address type of the device 0=public/1=private random
    uint8_t addr_type;
} gap_bdaddr_t;

/// Periodic advertising address information
/*@TRACE*/
typedef struct gap_per_adv_bdaddr
{
    /// BD Address of device
    uint8_t addr[GAP_BD_ADDR_LEN];
    /// Address type of the device 0=public/1=private random
    uint8_t addr_type;
    /// Advertising SID
    uint8_t adv_sid;
} gap_per_adv_bdaddr_t;

/// Device name
/*@TRACE*/
struct gap_dev_name
{
    /// Length of provided value
    uint16_t value_length;
    /// name value starting from offset to maximum length
    uint8_t  value[__ARRAY_EMPTY];
};

struct gap_adv_report_data
{
    /// Length of provided value
    uint16_t value_length;
    /// name value starting from offset to maximum length
    uint8_t  value[__ARRAY_EMPTY];
};

/// Authentication mask
enum gap_auth_mask
{
    /// No Flag set
    GAP_AUTH_NONE    = 0,
    /// Bond authentication
    GAP_AUTH_BOND    = (1 << 0),
    /// Man In the middle protection
    GAP_AUTH_MITM    = (1 << 2),
    /// Secure Connection
    GAP_AUTH_SEC_CON = (1 << 3),
    /// Key Notification
    GAP_AUTH_KEY_NOTIF = (1 << 4),
    /// Support h7 function
    GAP_AUTH_CT2      = (1 << 5),
};

/// Security Link Level
enum gap_sec_lvl
{
    /// Service accessible through an un-encrypted link
    GAP_SEC_NOT_ENC             = 0,
    /// Service require an unauthenticated pairing (just work pairing)
    GAP_SEC_UNAUTH,
    /// Service require an authenticated pairing (Legacy pairing with pin code or OOB)
    GAP_SEC_AUTH,
    /// Service require a secure connection pairing
    GAP_SEC_SECURE_CON,
};

enum gapm_pairing_mode
{
    /// No pairing authorized
    GAPM_PAIRING_DISABLE  = 0,
    /// Legacy pairing Authorized
    GAPM_PAIRING_LEGACY   = (1 << 0),
    /// Secure Connection pairing Authorized
    GAPM_PAIRING_SEC_CON  = (1 << 1),
};

/// Type of advertising that can be created
enum gapm_adv_type
{
    /// Legacy advertising
    GAPM_ADV_TYPE_LEGACY = 0,
    /// Extended advertising
    GAPM_ADV_TYPE_EXTENDED,
    /// Periodic advertising
    GAPM_ADV_TYPE_PERIODIC,
};

/// Advertising discovery mode
enum gapm_adv_disc_mode
{
    /// Mode in non-discoverable
    GAPM_ADV_MODE_NON_DISC = 0,
    /// Mode in general discoverable
    GAPM_ADV_MODE_GEN_DISC,
    /// Mode in limited discoverable
    GAPM_ADV_MODE_LIM_DISC,
    /// Broadcast mode without presence of AD_TYPE_FLAG in advertising data
    GAPM_ADV_MODE_BEACON,
    GAPM_ADV_MODE_MAX,
};

/// Advertising properties bit field bit positions
enum gapm_adv_flag
{
    /// Indicate that advertising is connectable, reception of CONNECT_REQ or AUX_CONNECT_REQ
    /// PDUs is accepted. Not applicable for periodic advertising.
    GAPM_ADV_PROP_CONNECTABLE     = 0x0001,

    /// Indicate that advertising is scannable, reception of SCAN_REQ or AUX_SCAN_REQ PDUs is
    /// accepted
    GAPM_ADV_PROP_SCANNABLE       = 0x0002,
    /// Indicate that advertising targets a specific device. Only apply in following cases:
    ///   - Legacy advertising: if connectable
    ///   - Extended advertising: connectable or (non connectable and non discoverable)
    GAPM_ADV_PROP_DIRECTED        = 0x0004,

    /// Indicate that High Duty Cycle has to be used for advertising on primary channel
    /// Apply only if created advertising is not an extended advertising
    GAPM_ADV_PROP_HDC             = 0x0008,

    /// Enable anonymous mode. Device address won't appear in send PDUs
    /// Valid only if created advertising is an extended advertising
    GAPM_ADV_PROP_ANONYMOUS       = 0x0020,

    /// Include TX Power in the extended header of the advertising PDU.
    /// Valid only if created advertising is not a legacy advertising
    GAPM_ADV_PROP_TX_PWR          = 0x0040,

    /// Include TX Power in the periodic advertising PDU.
    /// Valid only if created advertising is a periodic advertising
    GAPM_ADV_PROP_PER_TX_PWR      = 0x0080,

    /// Indicate if application must be informed about received scan requests PDUs
    GAPM_ADV_PROP_SCAN_REQ_NTF_EN = 0x0100,
};


/// Scanning Types
enum gapm_scan_type
{
    /// General discovery
    GAPM_SCAN_TYPE_GEN_DISC = 0,
    /// Limited discovery
    GAPM_SCAN_TYPE_LIM_DISC,
    /// Observer
    GAPM_SCAN_TYPE_OBSERVER,
    /// Selective observer
    GAPM_SCAN_TYPE_SEL_OBSERVER,
    /// Connectable discovery
    GAPM_SCAN_TYPE_CONN_DISC,
    /// Selective connectable discovery
    GAPM_SCAN_TYPE_SEL_CONN_DISC,
};

/// Packet Payload type for test mode
enum gap_pkt_pld_type
{
    /// PRBS9 sequence "11111111100000111101..." (in transmission order)
    GAP_PKT_PLD_PRBS9,
    /// Repeated "11110000" (in transmission order)
    GAP_PKT_PLD_REPEATED_11110000,
    /// Repeated "10101010" (in transmission order)
    GAP_PKT_PLD_REPEATED_10101010,
    /// PRBS15 sequence
    GAP_PKT_PLD_PRBS15,
    /// Repeated "11111111" (in transmission order) sequence
    GAP_PKT_PLD_REPEATED_11111111,
    /// Repeated "00000000" (in transmission order) sequence
    GAP_PKT_PLD_REPEATED_00000000,
    /// Repeated "00001111" (in transmission order) sequence
    GAP_PKT_PLD_REPEATED_00001111,
    /// Repeated "01010101" (in transmission order) sequence
    GAP_PKT_PLD_REPEATED_01010101,
};


/// Modulation index
enum gap_modulation_idx
{
    /// Assume transmitter will have a standard modulation index
    GAP_MODULATION_STANDARD,
    /// Assume transmitter will have a stable modulation index
    GAP_MODULATION_STABLE,
};

/// Bit field of enabled advertising reports
enum gapm_report_en_bf
{
    /// Periodic advertising reports reception enabled
    GAPM_REPORT_ADV_EN_BIT      = 0x01,
    GAPM_REPORT_ADV_EN_POS      = 0,
    /// BIG Info advertising reports reception enabled
    GAPM_REPORT_BIGINFO_EN_BIT  = 0x02,
    GAPM_REPORT_BIGINFO_EN_POS  = 1,
};

/// Periodic synchronization types
enum gapm_per_sync_type
{
    /// Do not use periodic advertiser list for synchronization. Use advertiser information provided
    /// in the GAPM_ACTIVITY_START_CMD.
    GAPM_PER_SYNC_TYPE_GENERAL = 0,
    /// Use periodic advertiser list for synchronization
    GAPM_PER_SYNC_TYPE_SELECTIVE,
    /// Use Periodic advertising sync transfer information send through connection for synchronization
    GAPM_PER_SYNC_TYPE_PAST,
};

/// bt Discoverability modes
enum gapm_discover_mode
{
    /// can not be discovered
    GAPM_NON_DISCOVERABLE     = 0,
    /// can be discovered, default general discoverable
    GAPM_DISCOVERABLE         = 1,
    /// general discoverable
    GAPM_GEN_DISCOVERABLE     = 1,
    /// limited discoverable, include general discoverable
    GAPM_LIM_DISCOVERABLE     = 2,
};

/// bt Connectability modes
enum gapm_connect_mode
{
    /// can not be connected
    GAPM_NON_CONNECTABLE = 0,
    /// can be connected
    GAPM_CONNECTABLE,
};

enum gapm_actv_type
{
    /// Advertising activity
    GAPM_ACTV_TYPE_ADV = 0,
    /// Scanning activity
    GAPM_ACTV_TYPE_SCAN,
    /// Initiating activity
    GAPM_ACTV_TYPE_INIT,
    /// Periodic synchronization activity
    GAPM_ACTV_TYPE_PER_SYNC,
	/// BR/EDR device discovery
	GAPM_ACTV_TYPE_DISCOVERY,
	/// BT/EDR device connect
	GAPM_ACTV_TYPE_CONNECT,
};

enum gap_encrypt_type
{
    GAP_BOND_IND = 0x00,
    GAP_ENCRYPT_REQ = 0x80,
};

enum bt_gap_scan_type
{
    BT_GAP_SCAN_DIS = 0x00,
    BT_GAP_ISCAN_EN = 0x01,
    BT_GAP_PSCAN_EN = 0x02,
    BT_GAP_ISCAN_PSCAN_EN = 0x03,
};

enum gap_exit_latency_type
{
    GAP_EXIT_LATENCY_CONNECT  = (1 << 0),
    GAP_EXIT_LATENCY_AUDIO    = (1 << 1),
    GAP_EXIT_LATENCY_OTA      = (1 << 2),

    GAP_EXIT_LATENCY_ALL      = 0xff,
};


typedef struct ble_gap_cfg
{
    /// device address
    gap_bdaddr_t addr;
    /// device name length
    uint8_t name_len;
    /// device name
    uint8_t name[GAP_NAME_LEN];
    /// ble appearance @see enum gap_appearance_type
    uint16_t appearance;
    /// auth io capability @see enum gap_io_cap
    uint8_t iocap;
    /// auth request @see enum gap_auth_mask
    uint8_t auth;
    /// Pairing mode authorized (@see enum gapm_pairing_mode)
    uint8_t pairing_mode;

} ble_gap_cfg_t;

typedef struct bt_gap_cfg
{
    /// Local device class
    uint32_t        cod;
    // -------------- BT Modes Config -----------------------
    /// Discoverability modes @see enum gapm_discover_mode
    uint8_t         discover_mode;
    /// Connectability modes @see enum gapm_connect_mode
    uint8_t         connect_mode;
    /// Inquiry scan interval
    uint16_t        iscan_interval;
    /// Page scan interval
    uint16_t        pscan_interval;

} bt_gap_cfg_t;

typedef struct bt_gap_peer_info
{
    uint8_t  phy_2m;
    uint8_t  bt_core;
    uint16_t comp_id;
} bt_gap_peer_info_t;


typedef union ble_info_data
{
    /// address
    gap_bdaddr_t addr;
    /// device name
    struct {
        uint16_t length;
        uint8_t *name;
    };
    /// appearance
    uint16_t appearance;
    /// version
    struct {
        uint8_t lmp_version;
        uint16_t subvers;
        uint16_t compid;
    };
    /// feature
    struct {
        uint8_t page;
        uint8_t features[8];
    };
    /// list size
    struct {
        uint8_t operation;
        uint8_t size;
    };
    /// rssi
    int8_t rssi;
} ble_info_data_t;

typedef struct ble_gap_cb
{
    /**
     ****************************************************************************************
     * @brief Reception of stack activate complete
     ****************************************************************************************
     */
    void (*cb_ble_enable_cmp)(uint16_t status);

    /**
     ****************************************************************************************
     * @brief Handles adv report event from the GAP
     *
     * @param[in] flag              ADV Report flag, @see enum gapm_adv_flag
     * @param[in] peer_addr         peer device address
     * @param[in] rssi              rssi of the adv report
     * @param[in] len               ADV Report Data Length
     * @param[in] data              ADV Report Data
     ****************************************************************************************
     */
    void (*cb_ble_adv_report_ind)(uint8_t flag, gap_bdaddr_t *peer_addr, int8_t rssi, uint8_t len, uint8_t *data);

    /**
     ****************************************************************************************
     * @brief Handles active start indicate event from the GAP
     *
     * @param[in] type              active type, @see enum gapm_actv_type
     * @param[in] actv_id           active id
     * @param[in] status            status of the start report
     ****************************************************************************************
     */
    void (*cb_ble_actv_start_ind)(uint8_t type, uint8_t actv_id, int16_t status);

    /**
     ****************************************************************************************
     * @brief Handles active stop indicate event from the GAP
     *
     * @param[in] type              active type, @see enum gapm_actv_type
     * @param[in] actv_id           active id
     * @param[in] status            status of the stop report
     ****************************************************************************************
     */
    void (*cb_ble_actv_stop_ind)(uint8_t type, uint8_t actv_id, int16_t status);

    /**
     ****************************************************************************************
     * @brief Handles connection complete event from the GAP
     ****************************************************************************************
     */
    void (*cb_ble_conn_ind)(uint8_t conidx, uint16_t conhdl, gap_bdaddr_t *peer_addr);

    /**
     ****************************************************************************************
     * @brief Handles connection update event from the GAP
     ****************************************************************************************
     */
    void (*cb_ble_conn_update)(uint8_t conidx, uint16_t interval, uint16_t latency, uint16_t super_to);


    /**
     ****************************************************************************************
     * @brief Handles connection disconnection event from the GAP
     ****************************************************************************************
     */
    void (*cb_ble_disc_ind)(uint8_t conidx, uint16_t conhdl, uint16_t reason);

    /**
     ****************************************************************************************
     * @brief Handles secure key request event from the GAP
     *
     * @param[in] conidx            Connection index
     * @param[in] key_type          Request key type @See gap_tk_type
     * @param[in] key               key value for display
     ****************************************************************************************
     */
    void (*cb_ble_key_req)(uint8_t conidx, uint8_t key_type, uint32_t key);

    /**
     ****************************************************************************************
     * @brief Handles bond complete event from the GAP
     ****************************************************************************************
     */
    void (*cb_ble_bond_ind)(uint8_t conidx, uint16_t status);

    /**
     ****************************************************************************************
     * @brief callback for the get_dev_info/get_con_info
     *
     * @param[in] conidx            Connection index, GAP_INVALID_CONIDX(0xff) for the info is for local device
     * @param[in] type              Information type @see enum gap_dev_info_type
     * @param[in] data              Pointer to the data @see ble_info_data_t
     ****************************************************************************************
     */
    void (*cb_ble_info_ind)(uint8_t conidx, uint8_t type, ble_info_data_t *data);

    /**
     ****************************************************************************************
     * @brief Reception of stack event complete
     ****************************************************************************************
     */
    void (*cb_ble_event_cmp)(uint8_t type, uint16_t status);

} ble_gap_cb_t;

typedef struct bt_gap_cb
{
    /**
     ****************************************************************************************
     * @brief Reception of stack activate complete
     ****************************************************************************************
     */
    void (*cb_bt_enable_cmp)(uint16_t status);
    /**
     ****************************************************************************************
     * @brief Handles connection complete event from the GAP
     ****************************************************************************************
     */
    void (*cb_bt_conn_ind)(uint8_t conidx, uint16_t conhdl, gap_bdaddr_t *peer_addr);
     /**
     ****************************************************************************************
     * @brief Handles sco connection event from the GAP
     ****************************************************************************************
     */
    void (*cb_bt_aud_conn_ind)(uint8_t conidx, uint16_t type, uint16_t status);
    /**
     ****************************************************************************************
     * @brief Handles sco connection disconnection event from the GAP
     ****************************************************************************************
     */
    void (*cb_bt_aud_disc_ind)(uint8_t conidx, uint16_t conhdl, uint16_t reason);
    /**
     ****************************************************************************************
     * @brief Handles discover report event from the GAP
     ****************************************************************************************
     */
    void (*cb_bt_discover_ind)(gap_bdaddr_t *peer_addr, uint16_t clk_off, int8_t rssi, uint8_t mode, uint32_t cod, struct gap_dev_name *name);
}bt_gap_cb_t;

/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */

/**
 * Enable gap ble stack
 *
 * @param cfg               Configure for gap
 * @param cb                Call back for gap
 *
 * @return None.
 */
void ble_gap_enable(const ble_gap_cfg_t *cfg, const ble_gap_cb_t *cb);


/**
 * Get local device info.
 *
 * @param info_type   Get local device info command  (@see enum gap_dev_info_type)
 *
 * @return None.
 */
void ble_gap_get_dev_info(uint8_t info_type);

/**
 * Prepare advertising activity parameter.
 *
 * @param tpye        Advertising type (@see enum gapm_adv_type)
 * @param disc_mode   Discovery mode (@see enum gapm_adv_disc_mode)
 * @param flags       Advertising flags (@see enum gapm_adv_flag)
 * @param peer_addr   Peer address configuration (only used in case of directed advertising)
 * @param max_adv_evt Maximum number of extended advertising events the controller shall attempt to send prior to
 *                    terminating the extending advertising
 *                    Valid only if extended advertising
 *
 * @return None.
 */
void ble_gap_adv_prepare(uint8_t adv_id, uint8_t type, uint8_t disc_mode, uint16_t flags, uint8_t filter_pol, uint32_t intv_min, uint32_t intv_max,
                                        gap_bdaddr_t peer_addr, uint8_t max_adv_evt);
/**
 * Generate advertising data.
 *
 * @param nb_uuid       Number of uuid list
 * @param uuids         UUID List
 *
 * @return None.
 */
void ble_gap_adv_gen_data(uint8_t adv_id, uint8_t nb_uuid, uint16_t *uuids);

/**
 * Set advertising data.
 *
 * @param adv_len       Advertising Data Length
 * @param avd_data      Advertising Data
 * @param rsp_len       Scan Response Data Length
 * @param rsp_data      Scan Response Data
 *
 * @return None.
 */
void ble_gap_adv_set_data(uint8_t adv_id, uint16_t adv_len, uint8_t *avd_data, uint16_t rsp_len, uint8_t *rsp_data);


/**
 * Start advertising activity.
 *
 * @return None.
 */

void ble_gap_adv_start(uint8_t adv_id);

/**
 * Stop advertising activity.
 *
 * @return None.
 */
void ble_gap_adv_stop(uint8_t adv_id);

/**
 * Stop advertising activity.
 *
 * @return Adv state.
 */
uint8_t ble_gap_get_adv_state(uint8_t adv_id);

/**
 * Start scan activity.
 *
 * @param type          Type of scanning to be started (@see enum gapm_scan_type)
 * @param phy           PHY select for the connection, @see enum gap_phy
 * @param scan_intv     Scan interval
 * @param scan_win      Scan window
 *
 * @return None.
 */
void ble_gap_scan_start(uint8_t scan_id, uint8_t type, uint8_t phy, uint16_t scan_intv, uint16_t scan_win);

/**
 * Stop scan activity
 *
 * @param None
 *
 * @return None.
 */
void ble_gap_scan_stop(uint8_t scan_id);

/**
 * Start create connection
 *
 * @param addr    	        Peer device address
 * @param phy               PHY select for the connection, @see enum gap_phy
 * @param conn_intv_min     Minimum value for the connection interval 7.5ms to 4s (in unit of 1.25ms).
 * @param conn_intv_max     Maximum value for the connection interval 7.5ms to 4s (in unit of 1.25ms).
 * @param latency           Connection Latency
 * @param super_to          Supervision timeout
 *
 * @return None.
 */
void ble_gap_connect(gap_bdaddr_t addr, uint8_t phy, uint16_t conn_intv_min, uint16_t conn_intv_max,
                    uint16_t latency, uint16_t super_to);

/**
 * Stop create connection
 *
 * @param None
 *
 * @return None.
 */
void ble_gap_connect_cancel();

/**
 * Update connection param
 *
 * @param conidx            Connection index
 * @param conn_intv_min     Minimum value for the connection interval 7.5ms to 4s (in unit of 1.25ms).
 * @param conn_intv_max     Maximum value for the connection interval 7.5ms to 4s (in unit of 1.25ms).
 * @param latency           Connection Latency
 * @param super_to          Supervision timeout
 *
 * @return None.
 */
void ble_gap_connect_update(uint8_t conidx, uint16_t conn_intv_min, uint16_t conn_intv_max, uint16_t latency, uint16_t super_to);

/**
 * Disconnect connection
 *
 * @param conidx            Connection index
 * @param reasion           Disconnect reason
 *
 * @return None.
 */
void ble_gap_disconnect(uint8_t conidx, uint8_t reason);

/**
 * Get connection information
 *
 * @param conidx            Connection index
 * @param type              Information type @see enum gap_dev_info_type
 *
 * @return None.
 */
void ble_gap_get_con_info(uint8_t conidx, uint8_t type);

/**
 * Start auth request
 *
 * @param conidx            Connection index
 * @param sec_lvl           Security level @see enum gap_sec_lvl
 *
 * @return None.
 */
void ble_gap_auth_req(uint8_t conidx, uint8_t sec_lvl);

/**
 ****************************************************************************************
 * @brief Handles secure key request event from the GAP
 *
 * @param[in] conidx            Connection index
 * @param[in] accept            Accept the key or not
 * @param[in] key               Key value, only for GAP_TK_GEN/GAP_TK_KEY_ENTRY
 ****************************************************************************************
 */
void ble_gap_key_cfm(uint8_t conidx, uint8_t accept, uint32_t key);

/**
 * Delete device bond information
 *
 * @param addr              BDADDR of the device to delete bond information, NULL for delete all bond information
 *
 * @return None.
 */
void ble_gap_delete_bond(gap_bdaddr_t *addr);

/**
 * Start tx test mode
 *
 * @param channel           Tx Channel (Range 0x00 to 0x27)
 * @param tx_pkt_payload    Packet Payload type (@see enum gap_pkt_pld_type)
 * @param tx_data_length    Length in bytes of payload data in each packet
 * @param phy               Test PHY rate (@see enum gap_phy_val)
 * @param tx_pwr_lvl        Transmit power level in dBm (range: -127 to +20)
 *
 * @return None.
 */
void ble_gap_test_mode_tx(uint8_t channel, uint8_t tx_pkt_payload, uint8_t tx_data_length, uint8_t phy, int8_t tx_pwr_lvl);

/**
 * Start rx test mode
 *
 * @param channel           Rx Channel (Range 0x00 to 0x27)
 * @param modulation_idx    Modulation Index (@see enum gap_modulation_idx)
 * @param slot_dur          Slot durations
 * @param phy               Test PHY rate (@see enum gap_phy_val)
 *
 * @return None.
 */
void ble_gap_test_mode_rx(uint8_t channel, uint8_t modulation_idx, uint8_t slot_dur, uint8_t phy);

/**
 * Stop create connection
 *
 * @param None
 *
 * @return None.
 */
void ble_gap_stop_test();

/**
 * Set le connect phy mode
 *
 * @param conidx           Connect index
 * @param rx_phy           Supported LE PHY for data reception (@see enum gap_phy)
 * @param tx_phy           Supported LE PHY for data reception (@see enum gap_phy)
 * @param phy_opt          PHY options (@see enum gapc_phy_option)
 *
 * @return None.
 */
uint8_t ble_gap_set_phy(uint8_t conidx, uint8_t rx_phy, uint8_t tx_phy, uint8_t phy_opt);

/**
 * Dis connect param req
 *
 * @param flag
 *
 * @return None.
 */
void ble_gap_set_con_param_dis(uint8_t flag);

/**
 * Set 1 to make connect exit latency.
 *
 * @param flag
 *
 * @return None.
 */

void ble_gap_set_con_exit_latency(uint8_t flag);

/**
 * entry latency apply.
 *
 * @param type (@see enum gap_exit_latency_type)
 *
 * @return None.
 */
void ble_gap_entry_latency(uint8_t type);

/**
 * exit latency apply.
 *
 * @param type (@see enum gap_exit_latency_type)
 *
 * @return None.
 */
void ble_gap_exit_latency(uint8_t type);

/**
 * Get peer rx mtu.
 *
 * @param conidx
 *
 * @return Mtu.
 */
uint16_t ble_gap_get_peer_mtu(uint8_t conidx);

/**
 * mtu exchange,this is not a standard interface when gatt service.
 *
 * @param conidx, user_lid
 *
 * @return status.
 */
uint8_t ble_gap_mtu_exch(uint8_t conidx, uint8_t user_lid);

/**
 * gap set ltk nvs default_id.
 *
 * @param None
 *
 * @return None.
 */
void ble_gap_set_ltk_nvs_default_id(void);

/**
 * gap set ltk use addr.
 *
 * @param addr            Device addr
 * @param p_ltk           store ltk pointer
 *
 * @return None.
 */
//uint8_t ble_gap_get_ltk_nocon(gap_addr_t * addr, uint8_t *p_ltk);

/**
 * gap set ltk use addr.
 *
 * @param addr            Device addr
 *
 * @return Number of paired addr.
 */
uint8_t ble_gap_get_paired_addr(gap_bdaddr_t *addr_buf);

/**
 * gap set white list.
 *
 * @param size            list count.
 * @param addr            white list addr.
 *
 * @return None.
 */
void ble_gap_wl_list_set(uint8_t size, gap_bdaddr_t* addr);

/**
 * Get disable update connect param by llcp req flag
 *
 * @param flag
 *
 * @return None.
 */

uint8_t ble_gap_get_con_param_dis(void);

/**
 * Gap set user adv data
 *
 * @param adv_id
 * @param len
 * @param adv_data
 *
 * @return None.
 */
void ble_gap_adv_user_data(uint8_t adv_id, uint8_t len , uint8_t *adv_data);

/**
 * Gap set data len
 *
 * @param conidx
 * @param tx_octets
 * @param tx_time
 *
 * @return None.
 */
void ble_gap_set_data_len(uint16_t conidx, uint16_t tx_octets, uint16_t tx_time);


/**
 * Prepare scan activity.
 *
 * @param scan id       ID number of scan.
 *
 * @return None.
 */
void ble_gap_scan_prepare(uint8_t scan_id);

/**
 * Start periodic synchronization activity.
 *
 * @param type          Type of Periodic synchronization to be started (@see enum gapm_per_sync_type)
 * @param adv_addr      Periodic advertising address information
 * @param report_en     report enable bitfile @see enum gapm_report_en_bf
 * @param past_conidx   only valid for GAPM_PER_SYNC_TYPE_PAST 
 * @param time_out      in unit of 10ms between 100ms and 163.84s
 *
 * @return None.
 */
void ble_gap_per_sync_start(uint8_t type, gap_per_adv_bdaddr_t *adv_addr, uint8_t report_en, uint8_t past_conidx,uint16_t time_out);

/**
 * Stop scan activity
 *
 * @param None
 *
 * @return None.
 */
void ble_gap_per_sync_stop();

/**
 * gap set local public addr.
 *
 * @param addr, public addr.
 *
 * @return None.
 */
void ble_gap_set_loc_pub_addr(uint8_t *addr);

#if (BT_STACK_PRESENT)
/**
 * Enable gap bt stack
 *
 * @param cfg               Configure for gap
 * @param cb                Call back for gap
 *
 * @return None.
 */
void bt_gap_enable(const bt_gap_cfg_t *cfg, const bt_gap_cb_t *cb);

/**
 * Enable gap bt discover create
 *
 * @param own_addr_type               addr type
 *
 * @return None.
 */
void bt_gap_discover_create(uint8_t own_addr_type);

/**
 * Enable gap bt discover start
 *
 * @param actv_idx               active idx
 * @param disc_mode              discover mode
 * @param max_count              max count
 * @param get_name               get name flag
 *
 * @return None.
 */
void bt_gap_discover_start(uint8_t actv_idx, uint8_t disc_mode, uint8_t max_count, bool get_name);

/**
 * Enable gap bt connect
 *
 * @param p_addr                 addr point
 * @param type                   type
 * @param clk_off                clk_off
 * @param page_scan_rep_mode     page_scan_rep_mode
 *
 * @return None.
 */

void bt_gap_connect(gap_bdaddr_t addr, uint8_t type, uint16_t clk_off, uint8_t page_scan_rep_mode);

/**
 * Bt scan enable
 *
 * @param enable,0:dia, 1:i_scan, 2:p_scan, 3:p_scan&i_scan
 *
 * @return None.
 */
void bt_classic_scan_enable(uint8_t enable);
#endif//(BT_STACK_PRESENT)

#endif//BLE_GAP_H_




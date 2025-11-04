#ifndef _HOGPRH_H_
#define _HOGPRH_H_


/**
 ****************************************************************************************
 * @addtogroup HOGPRH HID Over GATT Profile Report Host
 * @ingroup HOGP
 * @brief HID Over GATT Profile Report Host
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "hogp_common.h"
#include "ble_prf.h"
#include "ble_gatt.h"

/*
 * DEFINES
 ****************************************************************************************
 */

///Maximum number of HID Over GATT Report Host task instances
#define HOGPRH_IDX_MAX    (BLE_CONNECTION_MAX)

/// Maximal number of hids instances that can be handled
#define HOGPRH_NB_HIDS_INST_MAX              (1)
/// Maximal number of Report Char. that can be added in the DB for one HIDS - Up to 11
#define HOGPRH_NB_REPORT_INST_MAX            (8)

/// Maximal length of Report Map Char. Value
#define HOGPRH_REPORT_MAP_MAX_LEN            (512)

/*
 * ENUMERATIONS
 ****************************************************************************************
 */
/// Content of HOGPRH dummy bit field
enum hogprh_dummy_bf
{
    /// BAS Instance
    HOGPRH_DUMMY_HIDS_INST_MASK = 0x00FF,
    HOGPRH_DUMMY_HIDS_INST_LSB  = 0,
    /// Value
    HOGPRH_DUMMY_VAL_ID_MASK   = 0xFF00,
    HOGPRH_DUMMY_VAL_ID_LSB    = 8,
};

/// Possible states of the HOGPRH task
enum hogprh_state
{
    /// Disconnected state
    HOGPRH_FREE,
    /// IDLE state
    HOGPRH_IDLE,
    /// Busy State
    HOGPRH_BUSY,
    /// Number of defined states.
    HOGPRH_STATE_MAX
};

/// Characteristics
enum hogprh_chars
{
    /// Report Map
    HOGPRH_CHAR_REPORT_MAP,
    /// HID Information
    HOGPRH_CHAR_HID_INFO,
    /// HID Control Point
    HOGPRH_CHAR_HID_CTNL_PT,
    /// Protocol Mode
    HOGPRH_CHAR_PROTOCOL_MODE,
    /// Report
    HOGPRH_CHAR_REPORT,

    HOGPRH_CHAR_MAX = HOGPRH_CHAR_REPORT + HOGPRH_NB_REPORT_INST_MAX,
};


/// Characteristic descriptors
enum hogprh_descs
{
    /// Report Map Char. External Report Reference Descriptor
    HOGPRH_DESC_REPORT_MAP_EXT_REP_REF,
    /// Report Char. Report Reference
    HOGPRH_DESC_REPORT_REF,
    /// Report Client Config
    HOGPRH_DESC_REPORT_CFG = HOGPRH_DESC_REPORT_REF + HOGPRH_NB_REPORT_INST_MAX,

    HOGPRH_DESC_MAX = HOGPRH_DESC_REPORT_CFG + HOGPRH_NB_REPORT_INST_MAX,
};

/// Peer HID service info that can be read/write
enum hogprh_info
{
    /// Protocol Mode
    HOGPRH_PROTO_MODE,
    /// Report Map
    HOGPRH_REPORT_MAP,
    /// Report Map Char. External Report Reference Descriptor
    HOGPRH_REPORT_MAP_EXT_REP_REF,

    /// HID Information
    HOGPRH_HID_INFO,
    /// HID Control Point
    HOGPRH_HID_CTNL_PT,
    /// Report
    HOGPRH_REPORT,
    /// Report Char. Report Reference
    HOGPRH_REPORT_REF,
    /// Report Notification config
    HOGPRH_REPORT_NTF_CFG,

    HOGPRH_INFO_MAX,
};

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */

///Structure containing the characteristics handles, value handles and descriptors
typedef struct hogprh_content
{
    /// Service info
    prf_svc_t svc;

    /// Included service info
    prf_incl_svc_t incl_svc;

    /// Characteristic info:
    prf_char_t chars[HOGPRH_CHAR_MAX];

    /// Descriptor handles:
    prf_desc_t descs[HOGPRH_DESC_MAX];

    /// Number of Report Char. that have been found
    uint8_t report_nb;
#if 0
    /// HID report Reference
    struct hogprh_report_ref report_ref[HOGPRH_NB_REPORT_INST_MAX];
    /// HID report map
    struct hogprh_report_map *report_map;
#endif
}hogprh_content_t;


/// Environment variable for each Connections
typedef struct hogprh_cnx_env
{
    ///HIDS characteristics
    hogprh_content_t hids[HOGPRH_NB_HIDS_INST_MAX];
    ///Number of HIDS instances found
    uint8_t nb_svc;
    /// True if discovery procedure is on-going
    bool    discover;
}hogprh_cnx_env_t;

/// HID Over Gatt Profile Boot Host environment variable
typedef struct hogprh_env
{
    /// profile environment
    prf_hdr_t prf_env;
    /// Environment variable pointer for each connections
    hogprh_cnx_env_t * p_env[HOGPRH_IDX_MAX];
    /// GATT User local identifier
    uint8_t              user_lid;
}hogprh_env_t;




/// Parameters of the @ref HOGPRH_ENABLE_REQ message
struct hogprh_enable_req
{
    /// Connection type
    uint8_t con_type;

    /// Number of HIDS instances
    uint8_t hids_nb;
    /// Existing handle values hids
    struct hogprh_content hids[HOGPRH_NB_HIDS_INST_MAX];
};

/// Parameters of the @ref HOGPRH_ENABLE_RSP message
struct hogprh_enable_rsp
{
    ///status
    uint8_t status;

    /// Number of HIDS instances
    uint8_t hids_nb;
    /// Existing handle values hids
    struct hogprh_content hids[HOGPRH_NB_HIDS_INST_MAX];
};


/// HID report info
struct hogprh_report
{
    /// Report Length
    uint8_t length;
    /// Report value
    uint8_t value[__ARRAY_EMPTY];
};

/// HID report Reference
struct hogprh_report_ref
{
    /// Report ID
    uint8_t id;
    /// Report Type
    uint8_t type;
};

/// HID report MAP info
struct hogprh_report_map
{
    /// Report MAP Length
    uint16_t length;
    /// Report MAP value
    uint8_t value[__ARRAY_EMPTY];
};

/// HID report MAP reference
struct hogprh_report_map_ref
{
    /// Reference UUID length
    uint8_t uuid_len;
    /// Reference UUID Value
    uint8_t uuid[__ARRAY_EMPTY];
};

/// Information data
union hogprh_data
{
    /// Protocol Mode
    ///  - info = HOGPRH_PROTO_MODE
    uint8_t proto_mode;

    /// HID Information value
    ///  - info = HOGPRH_HID_INFO
    struct hids_hid_info hid_info;

    /// HID Control Point value to write
    ///  - info = HOGPRH_HID_CTNL_PT
    uint8_t hid_ctnl_pt;

    /// Report information
    ///  - info = HOGPRH_REPORT
    struct hogprh_report report;

    ///Notification Configuration Value
    ///  - info = HOGPRH_REPORT_NTF_CFG
    uint16_t report_cfg;

    /// HID report Reference
    ///  - info = HOGPRH_REPORT_REF
    struct hogprh_report_ref report_ref;

    /// HID report MAP info
    ///  - info = HOGPRH_REPORT_MAP
    struct hogprh_report_map report_map;

    /// HID report MAP reference
    ///  - info = HOGPRH_REPORT_MAP_EXT_REP_REF
    struct hogprh_report_map_ref report_map_ref;
};



///Parameters of the @ref HOGPRH_READ_INFO_REQ message
struct hogprh_read_info_req
{
    ///Characteristic info @see enum hogprh_info
    uint8_t info;
    /// HID Service Instance - From 0 to HOGPRH_NB_HIDS_INST_MAX-1
    uint8_t hid_idx;
    /// HID Report Index: only relevant for:
    ///  - info = HOGPRH_REPORT
    ///  - info = HOGPRH_REPORT_REF
    ///  - info = HOGPRH_REPORT_NTF_CFG
    uint8_t report_idx;
};

///Parameters of the @ref HOGPRH_READ_INFO_RSP message
struct hogprh_read_info_rsp
{
    /// status of the request
    uint8_t status;
    ///Characteristic info @see enum hogprh_info
    uint8_t info;
    /// HID Service Instance - From 0 to HOGPRH_NB_HIDS_INST_MAX-1
    uint8_t hid_idx;
    /// HID Report Index: only relevant for:
    ///  - info = HOGPRH_REPORT
    ///  - info = HOGPRH_REPORT_REF
    ///  - info = HOGPRH_REPORT_NTF_CFG
    uint8_t report_idx;
    /// Information data
    union hogprh_data data;
};


///Parameters of the @ref HOGPRH_WRITE_REQ message
struct hogprh_write_req
{
    ///Characteristic info @see enum hogprh_info
    uint8_t info;
    /// HID Service Instance - From 0 to HOGPRH_NB_HIDS_INST_MAX-1
    uint8_t hid_idx;
    /// HID Report Index: only relevant for:
    ///  - info = HOGPRH_REPORT
    ///  - info = HOGPRH_REPORT_NTF_CFG
    uint8_t report_idx;
    /// Write type ( Write without Response True or Write Request)
    /// - only valid for HOGPRH_REPORT
    bool    wr_cmd;
    /// Information data
    union hogprh_data data;
};


///Parameters of the @ref HOGPRH_WRITE_RSP message
struct hogprh_write_rsp
{
    /// status of the request
    uint8_t status;
    ///Characteristic info @see enum hogprh_info
    uint8_t info;
    /// HID Service Instance - From 0 to HOGPRH_NB_HIDS_INST_MAX-1
    uint8_t hid_idx;
    /// HID Report Index: only relevant for:
    ///  - info = HOGPRH_REPORT
    ///  - info = HOGPRH_REPORT_REF
    ///  - info = HOGPRH_REPORT_NTF_CFG
    uint8_t report_idx;
};


///Parameters of the @ref HOGPRH_BOOT_REPORT_IND message
struct hogprh_report_ind
{
    /// HIDS Instance
    uint8_t hid_idx;
    /// HID Report Index
    uint8_t report_idx;
    /// Report data
    struct hogprh_report report;
};

/// Hogprh service client callback set for user.
typedef struct hogprh_cb
{
    /**
     ****************************************************************************************
     * @brief Completion of Enable procedure
     ****************************************************************************************
     */
    void (*cb_enable_cmp)(uint8_t conidx, uint16_t status, uint8_t hogprh_nb, const hogprh_content_t* p_hogprh);

    /**
     ****************************************************************************************
     * @brief Inform that Notification configuration read procedure is over
     ****************************************************************************************
     */
    void (*cb_read_att_val_cmp)(uint8_t conidx, uint16_t status, uint8_t hogprh_instance, uint8_t hogprh_idx, \
                                uint8_t report_idx, uint8_t *p_data, uint16_t data_len);

    /**
     ****************************************************************************************
     * @brief Inform that Notification configuration write procedure is over
     ****************************************************************************************
     */
    void (*cb_write_att_val_cmp)(uint8_t conidx, uint16_t status, uint8_t hogprh_instance, uint8_t hogprh_idx, uint8_t report_idx);
    /**
     ****************************************************************************************
     * @brief Inform that report update from peer device
     ****************************************************************************************
     */
    void (*cb_att_val_ntf_upd)(uint8_t conidx, uint8_t hogprh_instance, uint8_t hogprh_idx, uint8_t report_idx, uint8_t *p_data, uint16_t data_len);
} hogprh_cb_t;

/*
 * GLOBAL VARIABLE DECLARATIONS
 ****************************************************************************************
 */
uint16_t ble_hogprh_init(const hogprh_cb_t* p_cb);
uint16_t ble_hogprh_enable(uint8_t conidx, uint8_t con_type, const hogprh_content_t* p_hogprh);

/// @} HOGPRH

#endif /* _HOGPRH_H_ */

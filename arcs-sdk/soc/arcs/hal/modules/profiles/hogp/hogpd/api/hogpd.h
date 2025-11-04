#ifndef _HOGPD_H_
#define _HOGPD_H_

/**
 ****************************************************************************************
 * @addtogroup HOGPD HID Over GATT Profile Device Role
 * @ingroup HOGP
 * @brief HID Over GATT Profile Device Role
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#define BLE_HID_DEVICE 1
#if (BLE_HID_DEVICE)
#include "hogp_common.h"
#include "hogpd_msg.h"

#include "ble_gatt.h"
#include "ble_prf.h"


/*
 * DEFINES
 ****************************************************************************************
 */

///Maximum number of HID Over GATT Device task instances
#define HOGPD_IDX_MAX                       (0x01)
/// Maximal length of Report Char. Value
#define HOGPD_REPORT_MAX_LEN                (128)
/// Maximal length of Report Map Char. Value
#define HOGPD_REPORT_MAP_MAX_LEN            (512)
/// Length of Boot Report Char. Value Maximal Length
#define HOGPD_BOOT_REPORT_MAX_LEN           (8)
/// Boot KB Input Report Notification Configuration Bit Mask
#define HOGPD_BOOT_KB_IN_NTF_CFG_MASK       (0x40)
/// Boot KB Input Report Notification Configuration Bit Mask
#define HOGPD_BOOT_MOUSE_IN_NTF_CFG_MASK    (0x80)
/// Boot Report Notification Configuration Bit Mask
#define HOGPD_REPORT_NTF_CFG_MASK           (0x20)


///* define the HID report id */
enum
{
    HIDS_KB_INDEX,
    HIDS_MOUSE_INDEX,
    HIDS_MEDIA_INDEX,
    HIDS_VOICE_DATA_INDEX,
    HIDS_VOICE_CTRL_INDEX,
    HIDS_GDE_ACK_INDEX,
    HIDS_GDE_DATA_INDEX,
    HIDS_GDE_FEEDBACK_INDEX,

    HIDS_INDEX_NUM
};

#define HIDS_KB_REPORT_ID               0x01
#define HIDS_MOUSE_REPORT_ID            0x02
#define HIDS_MEDIA_REPORT_ID            0x03
#define HIDS_VOICE_DATA_IN_REPORT_ID    0xFC
#define HIDS_VOICE_CTRL_OUT_REPORT_ID   0xFB
#define HIDS_GDE_ACK_IN_REPORT_ID       0xF8
#define HIDS_GDE_DATA_OUT_REPORT_ID     0xFA
#define HIDS_GDE_FEEDBACK_IN_REPORT_ID  0xF9



/*
 * ENUMERATIONS
 ****************************************************************************************
 */

/// Possible states of the HOGPD task
enum hogpd_state
{
    /// Idle state
    HOGPD_IDLE,
    /// Request from application on-going
    HOGPD_REQ_BUSY  = (1 << 0),
    /// OPeration requested by peer device on-going
    HOGPD_OP_BUSY   = (1 << 1),
    /// Number of defined states.
    HOGPD_STATE_MAX
};


/// HID Service Attributes Indexes
enum
{
    HOGPD_IDX_SVC,

    // Included Service
    HOGPD_IDX_INCL_SVC,

    // HID Information
    HOGPD_IDX_HID_INFO_CHAR,
    HOGPD_IDX_HID_INFO_VAL,

    // HID Control Point
    HOGPD_IDX_HID_CTNL_PT_CHAR,
    HOGPD_IDX_HID_CTNL_PT_VAL,

    // Report Map
    HOGPD_IDX_REPORT_MAP_CHAR,
    HOGPD_IDX_REPORT_MAP_VAL,
    HOGPD_IDX_REPORT_MAP_EXT_REP_REF,

    // Protocol Mode
    HOGPD_IDX_PROTO_MODE_CHAR,
    HOGPD_IDX_PROTO_MODE_VAL,

    // Boot Keyboard Input Report
    HOGPD_IDX_BOOT_KB_IN_REPORT_CHAR,
    HOGPD_IDX_BOOT_KB_IN_REPORT_VAL,
    HOGPD_IDX_BOOT_KB_IN_REPORT_NTF_CFG,

    // Boot Keyboard Output Report
    HOGPD_IDX_BOOT_KB_OUT_REPORT_CHAR,
    HOGPD_IDX_BOOT_KB_OUT_REPORT_VAL,

    // Boot Mouse Input Report
    HOGPD_IDX_BOOT_MOUSE_IN_REPORT_CHAR,
    HOGPD_IDX_BOOT_MOUSE_IN_REPORT_VAL,
    HOGPD_IDX_BOOT_MOUSE_IN_REPORT_NTF_CFG,

    /// number of attributes that are uniq in the service
    HOGPD_ATT_UNIQ_NB,

    // Report
    HOGPD_IDX_REPORT_CHAR = HOGPD_ATT_UNIQ_NB,
    HOGPD_IDX_REPORT_VAL,
    HOGPD_IDX_REPORT_REP_REF,
    HOGPD_IDX_REPORT_NTF_CFG,

    HOGPD_IDX_NB,

    // maximum number of attribute that can be present in the HID service
    HOGPD_ATT_MAX = HOGPD_ATT_UNIQ_NB + (4* (HOGPD_NB_REPORT_INST_MAX)),
};



/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */
/// Map Data
typedef struct hogpd_report_map
{
    /// Callback API - meaning specific to the profile
    uint16_t             size;
    uint16_t             remain_size;
    uint8_t *            rep_map;
} hogpd_report_map_t;

/// HIDS service cfg
struct hogpd_svc_cfg
{
    /// Service Features (@see enum hogpd_cfg)
    uint16_t features;
    /// Notification configuration
    uint16_t ntf_cfg[BLE_CONNECTION_MAX];
    /// Number of attribute present in service
    uint8_t  nb_att;
    /// Number of Report Char. instances to add in the database
    uint8_t  nb_report;
    /// Handle offset where report are available - to enhance handle search
    uint8_t  report_hdl_offset;
    /// Current Protocol Mode
    uint8_t  proto_mode;

    /// Report Char. Configuration (@see enum hogpd_report_cfg)
    uint8_t report_char_cfg[HOGPD_NB_REPORT_INST_MAX];
    /// Report id number
    uint8_t report_id[HOGPD_NB_REPORT_INST_MAX];
    /// Report map length
    hogpd_report_map_t report_map_info;
    /// HID Information Char. Values
    struct hids_hid_info hid_info;
    /// External Report Reference
    struct hogpd_ext_ref ext_ref;

    //Todu: Add report reference define
    struct hids_report_ref report_ref[HOGPD_NB_REPORT_INST_MAX];
};

/// HIDS on-going operation
struct hogpd_operation
{
    /// Connection index impacted
    uint8_t  conidx;
    /// Operation type (@see enum hogpd_op)
    uint8_t  operation;
    /// Handle impacted by operation
    uint16_t handle;
};

/// HOGPD device callback set
typedef struct hogpd_cb
{
    /// gatt opreate complete.
    void (*cb_read_cmp) (uint32_t token, uint8_t val_id);
    void (*cb_notify_cmp) (uint32_t token, uint8_t val_id);
    void (*cb_write_cmp) (uint32_t token, uint8_t val_id);

    /// gatt opreate indicate
    void (*cb_read_ind) (uint8_t conidx, uint16_t index, uint16_t length, uint8_t *data);
    void (*cb_notify_ind) (uint8_t conidx, uint16_t index, uint16_t length, uint8_t *data);
    void (*cb_write_ind) (uint8_t conidx, uint16_t index, uint16_t length, uint16_t offset, uint8_t *data);
} hogpd_cb_t;


///// HID Over GATT Profile HID Device Role Environment variable
typedef struct hogpd_env
{
    hogpd_cb_t* p_cb;
    /// Supported Features
    struct hogpd_svc_cfg svcs[HOGPD_NB_HIDS_INST_MAX];
    /// HIDS Start Handles
    uint16_t start_hdl;
    /// On-going operation (requested by peer device)
    struct hogpd_operation op;
    /// Token used for
    uint16_t    hids_token;
    /// Number of HIDS added in the database
    uint8_t hids_nb;
    /// GATT user local identifier
    uint8_t user_lid;

} hogpd_env_t;

/// ongoing operation information
typedef struct hogpd_buf_meta
{
     /// Handle of the attribute to indicate/notify
     uint16_t handle;
     /// used to know on which device interval update has been requested, and to prevent
     /// indication to be triggered on this connection index
     uint8_t  conidx;
     /// Service index
     uint8_t  svc_idx;
} hogpd_buf_meta_t;


/*
 * GLOBAL VARIABLE DECLARATIONS
 ****************************************************************************************
 */


/*
 * FUNCTION DECLARATIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Initialization of the HOGPD module.
 * This function performs all the initializations of the Profile module.
 *  - Creation of database (if it's a service)
 *
 * @param[in|out] start_hdl  Service start handle (0 - dynamically allocated), only applies for services.
 * @param[in]     app_task   Application task number.
 * @param[in]     sec_lvl    Security level (AUTH, EKS and MI field of @see enum attm_value_perm_mask)
 * @param[in]     param      Configuration parameters of profile collector or service (32 bits aligned)
 *
 * @return status code to know if profile initialization succeed or not.
 ****************************************************************************************
 */
uint16_t hogpd_init(uint16_t *p_start_hdl, uint8_t sec_lvl, uint8_t user_prio, const struct hogpd_db_cfg *p_params, hogpd_cb_t* p_cb, hogpd_report_map_t* p_map);

/**
 ****************************************************************************************
 * @brief Initialization of the HOGPD report map.
 * This function performs all the initializations of the report map.
 *
 * @param[in]     hids_nb    Number of hids.
 * @param[in]     p_map      Pointer of map structure.
 *
 * @return status code to know if profile initialization succeed or not.
 ****************************************************************************************
 */
uint16_t hogpd_init_report_map(uint8_t hids_nb, hogpd_report_map_t * p_map);

/**
 ****************************************************************************************
 * @brief Initialization of the HOGPD module.
 * This function performs all the initializations of the Profile module.
 *  - Creation of database (if it's a service)
 *
 * @param[in]     svc_features      Service Features (@see enum hogpd_cfg)
 * @param[in]     report_char_cfg   Report Char. Configuration (@see enum hogpd_report_cfg)
 *
 * @return status code to know if profile initialization succeed or not.
 ****************************************************************************************
 */
uint16_t ble_hogpd_init(uint8_t svc_features, uint8_t report_char_cfg, hogpd_cb_t* p_cb, hogpd_report_map_t * p_map);

/**
 ****************************************************************************************
 * @brief Handles reception of the @ref HOGPD_REPORT_UPD_REQ message.
 *
 * @param[in] param Pointer to the parameters of the message @ref struct hogpd_report_upd_req.
 * @return Status Code to know if request succeed or not.
 ****************************************************************************************
 */
int hogpd_report_upd(struct hogpd_report_upd_req const *param);

/**
 ****************************************************************************************
 * @brief Send hogpd report
 *
 * @param[in] length send data length.
 * @param[in] value send data.
 * @return Status Code to know if request succeed or not.
 ****************************************************************************************
 */
int ble_hogpd_report_upd(uint8_t conidx, uint8_t report_idx, uint8_t length, uint8_t* value);

/**
 ****************************************************************************************
 * @brief The function enables the HID Over GATT Profile Device Role.
 * @param[in] param Pointer to the parameters of the message @ref struct hogpd_enable_req.
 ****************************************************************************************
 */
void hogpd_enable(struct hogpd_enable_req const *param);

/**
 ****************************************************************************************
 * @brief The function enables the HID Over GATT Profile Device Role.
 * @param[in] None.
 ****************************************************************************************
 */
void ble_hogpd_enable(uint8_t conidx);

/**
 ****************************************************************************************
 * @brief Destruction of the HOGPD module - due to a reset for instance.
 * This function clean-up allocated memory (attribute database is destroyed by another
 * procedure)
 *
 * @param[in]        reason     Detach reason
 ****************************************************************************************
 */
uint16_t hogpd_destroy(uint8_t reason);


/**
 ****************************************************************************************
 * @brief Handles Disconnection
 *
 * @param[in]        conidx     Connection index
 ****************************************************************************************
 */
void hogpd_cleanup(uint8_t conidx, uint16_t reason);


#endif /* #if (BLE_HID_DEVICE) */

/// @} HOGPD

#endif /* _HOGPD_H_ */

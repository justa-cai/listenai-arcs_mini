/**
 ****************************************************************************************
 *
 * @file htpt_msg.h
 *
 * @brief Header file - Health Thermometer Profile Thermometer - Message API.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

#ifndef HTPT_MSG_H_
#define HTPT_MSG_H_

/**
 ****************************************************************************************
 * @addtogroup HTPT
 * @brief Health Thermometer Profile Thermometer - Message API.
 *
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "btip_task.h" // Task definitions
#include "htp_common.h"

/*
 * DEFINES
 ****************************************************************************************
 */

/// All Features supported
#define HTPT_ALL_FEAT_SUP               0x1F

/*
 * ENUMERATIONS
 ****************************************************************************************
 */

/// Messages for Health Thermometer Profile Thermometer
/*@TRACE*/
enum htpt_msg_id
{
    /// Start the Health Thermometer Profile Thermometer profile - at connection
    HTPT_ENABLE_REQ             = MSG_ID(APP, 0x00),
    /// Enable confirmation
    HTPT_ENABLE_RSP             = MSG_ID(APP, 0x01),

    /// Send temperature value from APP
    HTPT_TEMP_SEND_REQ          = MSG_ID(APP, 0x02),
    /// Send temperature response
    HTPT_TEMP_SEND_RSP          = MSG_ID(APP, 0x03),

    /// Indicate Measurement Interval
    HTPT_MEAS_INTV_UPD_REQ      = MSG_ID(APP, 0x04),
    /// Send Measurement Interval response
    HTPT_MEAS_INTV_UPD_RSP      = MSG_ID(APP, 0x05),

    /// Inform APP of new measurement interval value requested by a peer device
    HTPT_MEAS_INTV_CHG_REQ_IND  = MSG_ID(APP, 0x06),
    /// APP Confirm message of new measurement interval value requested by a peer device
    /// If accepted, it triggers indication on measurement interval attribute
    HTPT_MEAS_INTV_CHG_CFM      = MSG_ID(APP, 0x07),

    /// Inform APP that Indication Configuration has been changed - use to update bond data
    HTPT_CFG_INDNTF_IND         = MSG_ID(APP, 0x08),
};

/// Database Feature Configuration Flags
enum htpt_features_bf
{
    /// Indicate if Temperature Type Char. is supported
    HTPT_TEMP_TYPE_CHAR_SUP_POS    = 0,
    HTPT_TEMP_TYPE_CHAR_SUP_BIT    = 0x0001,

    /// Indicate if Intermediate Temperature Char. is supported
    HTPT_INTERM_TEMP_CHAR_SUP_POS  = 1,
    HTPT_INTERM_TEMP_CHAR_SUP_BIT  = 0x0002,

    /// Indicate if Measurement Interval Char. is supported
    HTPT_MEAS_INTV_CHAR_SUP_POS    = 2,
    HTPT_MEAS_INTV_CHAR_SUP_BIT    = 0x0004,

    /// Indicate if Measurement Interval Char. supports indications
    HTPT_MEAS_INTV_IND_SUP_POS     = 3,
    HTPT_MEAS_INTV_IND_SUP_BIT     = 0x0008,

    /// Indicate if Measurement Interval Char. is writable
    HTPT_MEAS_INTV_WR_SUP_POS      = 4,
    HTPT_MEAS_INTV_WR_SUP_BIT      = 0x0010,
};

/// Notification and indication configuration
enum htpt_ntf_ind_cfg
{
    /// Stable measurement interval indication enabled
    HTPT_CFG_STABLE_MEAS_IND    = (1 << 0),
    /// Intermediate measurement notification enabled
    HTPT_CFG_INTERM_MEAS_NTF    = (1 << 1),
    /// Measurement interval indication
    HTPT_CFG_MEAS_INTV_IND      = (1 << 2),
};

/*
 * API MESSAGES STRUCTURES
 ****************************************************************************************
 */
/// Parameters of the Health thermometer service database
struct htpt_db_cfg
{
    /// Health thermometer Feature (@see enum htpt_features)
    uint8_t features;
    /// Temperature Type Value
    uint8_t temp_type;

    /// Measurement Interval Valid Range - Minimal Value
    uint16_t valid_range_min;
    /// Measurement Interval Valid Range - Maximal Value
    uint16_t valid_range_max;
    /// Measurement interval (latest known interval range)
    uint16_t meas_intv;
};

/// Parameters of the @ref HTPT_ENABLE_REQ message
struct htpt_enable_req
{
    /// Connection index
    uint8_t conidx;
    /// Notification configuration (Bond Data to restore: @see enum htpt_ntf_ind_cfg)
    uint8_t ntf_ind_cfg;
};

/// Parameters of the @ref HTPT_ENABLE_RSP message
struct htpt_enable_rsp
{
    /// Connection index
    uint8_t conidx;
    /// Status of enable request
    uint16_t status;
};

/// Parameters of the @ref HTPT_TEMP_SEND_REQ message
struct htpt_temp_send_req
{
    /// Temperature Measurement
    htp_temp_meas_t temp_meas;
    /// Stable or intermediary type of temperature (True stable meas, else false)
    bool stable_meas;
};

/// Parameters of the @ref HTPT_TEMP_SEND_RSP message
struct htpt_temp_send_rsp
{
    /// Status
    uint16_t status;
};

/// Parameters of the @ref HTPT_MEAS_INTV_UPD_REQ message
struct htpt_meas_intv_upd_req
{
    /// Measurement Interval value
    uint16_t meas_intv;
};

/// Parameters of the @ref HTPT_MEAS_INTV_UPD_RSP message
struct htpt_meas_intv_upd_rsp
{
    /// status
    uint16_t status;
};

/// Parameters of the @ref HTPT_MEAS_INTV_CHG_REQ_IND message
struct htpt_meas_intv_chg_req_ind
{
    /// Connection index
    uint8_t conidx;
    /// new measurement interval
    uint16_t intv;
};

/// Parameters of the @ref HTPT_MEAS_INTV_CHG_CFM message
struct htpt_meas_intv_chg_cfm
{
    /// Connection index
    uint8_t conidx;
    /// status of the request
    uint16_t status;
};

/// Parameters of the @ref HTPT_CFG_INDNTF_IND message
struct htpt_cfg_indntf_ind
{
    /// connection index
    uint8_t conidx;
    /// Notification Configuration (@see enum htpt_ntf_ind_cfg)
    uint8_t ntf_ind_cfg;
};

/// @} HTPT
#endif // HTPT_MSG_H_

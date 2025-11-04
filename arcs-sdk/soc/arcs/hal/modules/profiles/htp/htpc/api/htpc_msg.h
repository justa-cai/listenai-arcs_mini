/**
 ****************************************************************************************
 *
 * @file htpc_msg.h
 *
 * @brief Header file -  Health Thermometer Profile - Message API
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

#ifndef HTPC_MSG_H_
#define HTPC_MSG_H_

/**
 ****************************************************************************************
 * @addtogroup HTP Health Thermometer Profile
 * @ingroup Profile
 * @brief Health Thermometer Profile
 *
 * The HTP module is the responsible block for implementing the Health Thermometer Profile
 * functionalities in the BLE Host.
 *
 * The Health Thermometer Profile defines the functionality required in a device that allows
 * the user (Collector device) to configure and recover temperature measurements from a Thermometer device.
 *****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup HTPC Health Thermometer Profile Collector
 * @ingroup HTP
 * @brief Health Thermometer Profile Collector
 *
 * The HTPC is responsible for providing Health Thermometer Profile Collector functionalities to
 * upper layer module or application. The device using this profile takes the role
 * of Health Thermometer Profile Collector.
 *
 * Health Thermometer Profile Collector (HTPC): A HTPC (e.g. PC, phone, etc)
 * is the term used by this profile to describe a device that can set temperature
 * indication/notification intervals in a HTP Thermometer, interpreting them in a way
 * suitable to the user application.
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

///  Message id
/*@TRACE*/
enum htpc_msg_id
{
    /// Start the Health Thermometer Collector profile - at connection
    HTPC_ENABLE_REQ                 = MSG_ID(HTPC, 0x00),
    /// Confirm that cfg connection has finished with discovery results, or that normal cnx started
    HTPC_ENABLE_RSP                 = MSG_ID(HTPC, 0x01),

    /// Write Health Thermometer Notification Configuration Value request
    HTPC_HEALTH_TEMP_NTF_CFG_REQ    = MSG_ID(HTPC, 0x02),
    /// Write Health Thermometer Notification Configuration Value response
    HTPC_HEALTH_TEMP_NTF_CFG_RSP    = MSG_ID(HTPC, 0x03),

    /// APP request for measurement interval write
    HTPC_WR_MEAS_INTV_REQ           = MSG_ID(HTPC, 0x04),
    /// APP request for measurement interval write
    HTPC_WR_MEAS_INTV_RSP           = MSG_ID(HTPC, 0x05),

    /// Temperature value received from peer sensor
    HTPC_TEMP_IND                   = MSG_ID(HTPC, 0x06),
    /// Measurement interval update indication received from peer sensor
    HTPC_MEAS_INTV_IND              = MSG_ID(HTPC, 0x07),

    /// Generic message to read a HTP characteristic value
    HTPC_RD_CHAR_REQ                = MSG_ID(HTPC, 0x08),
    /// Read HTP characteristic value response
    HTPC_RD_CHAR_RSP                = MSG_ID(HTPC, 0x09),
};

/// Health Thermometer Service Characteristics - Char. Code
enum htpc_chars
{
    /// Temperature Measurement
    HTPC_CHAR_HTS_TEMP_MEAS     = 0x00,
    /// Temperature Type
    HTPC_CHAR_HTS_TEMP_TYPE     = 0x01,
    /// Intermediate Temperature
    HTPC_CHAR_HTS_INTM_TEMP     = 0x02,
    /// Measurement Interval
    HTPC_CHAR_HTS_MEAS_INTV     = 0x03,

    HTPC_CHAR_HTS_MAX,
};

/// Health Thermometer Service Characteristic Descriptors
enum htpc_descs
{
    /// Temp. Meas. Client Config
    HTPC_DESC_HTS_TEMP_MEAS_CLI_CFG,
    /// Intm. Meas. Client Config
    HTPC_DESC_HTS_INTM_MEAS_CLI_CFG,
    /// Meas. Intv. Client Config
    HTPC_DESC_HTS_MEAS_INTV_CLI_CFG,
    /// Meas. Intv. Valid Range,
    HTPC_DESC_HTS_MEAS_INTV_VAL_RGE,

    HTPC_DESC_HTS_MAX,
};

/// Internal codes for reading a HTS characteristic with one single request
enum htpc_val_id
{
    /// Read HTS Temp. Type
    HTPC_VAL_TEMP_TYPE = 0,
    /// Read HTS Measurement Interval
    HTPC_VAL_MEAS_INTV,

    /// Read HTS Temperature Measurement Client Cfg. Desc
    HTPC_VAL_TEMP_MEAS_CLI_CFG,
    /// Read HTS Intermediate Temperature Client Cfg. Desc
    HTPC_VAL_INTM_TEMP_CLI_CFG,
    /// Read HTS Measurement Interval Client Cfg. Desc
    HTPC_VAL_MEAS_INTV_CLI_CFG,
    /// Read HTS Measurement Interval Client Cfg. Desc
    HTPC_VAL_MEAS_INTV_VAL_RGE,
};

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */

/// Structure containing the characteristics handles, value handles and descriptors for the Health Thermometer Service
typedef struct htpc_hts_content
{
    /// service info
    prf_svc_t           svc;
    /// Characteristic info:
    prf_char_t      chars[HTPC_CHAR_HTS_MAX];
    /// Descriptor handles:
    prf_desc_t descs[HTPC_DESC_HTS_MAX];
} htpc_hts_content_t;

/// Parameters of the @ref HTPC_ENABLE_REQ message
struct htpc_enable_req
{
    /// Connection Index
    uint8_t            conidx;
    /// Connection type
    uint8_t            con_type;
    /// HTS existing handle values
    htpc_hts_content_t hts;
};

/// Parameters of the @ref HTPC_ENABLE_RSP message
struct htpc_enable_rsp
{
    /// Connection Index
    uint8_t            conidx;
    /// Status
    uint16_t           status;
    /// HTS handle values and characteristic properties
    htpc_hts_content_t hts;
};

/// Parameters of the @ref HTPC_HEALTH_TEMP_NTF_CFG_REQ message
struct htpc_health_temp_ntf_cfg_req
{
    /// Connection Index
    uint8_t  conidx;
    /// Stop/notify/indicate value to configure into the peer characteristic
    uint16_t cfg_val;
    /// Own code for differentiating between Temperature Measurement,
    /// Intermediate Temperature and Measurement Interval chars (@see htpc_chars)
    uint8_t  char_code;
};

/// Parameters of the @ref HTPC_HEALTH_TEMP_NTF_CFG_RSP message
struct htpc_health_temp_ntf_cfg_rsp
{
    /// Connection Index
    uint8_t  conidx;
    /// Status
    uint16_t status;
};

/// Parameters of the @ref HTPC_WR_MEAS_INTV_REQ message
struct htpc_wr_meas_intv_req
{
    /// Connection Index
    uint8_t  conidx;
    /// Interval value in seconds
    uint16_t intv;
};

/// Parameters of the @ref HTPC_WR_MEAS_INTV_RSP message
struct htpc_wr_meas_intv_rsp
{
    /// Connection Index
    uint8_t  conidx;
    /// Status
    uint16_t status;
};

/// Parameters of the @ref HTPC_TEMP_IND message
struct htpc_temp_ind
{
    /// Connection Index
    uint8_t         conidx;
    /// Temperature Measurement Structure
    htp_temp_meas_t temp_meas;
    /// Stable or intermediary type of temperature
    bool            stable_meas;
};

/// Parameters of @ref HTPC_MEAS_INTV_IND message
struct htpc_meas_intv_ind
{
    /// Connection Index
    uint8_t  conidx;
    /// Interval in seconds
    uint16_t intv;
};

/// Parameters of the @ref HTPC_RD_CHAR_REQ message
struct htpc_rd_char_req
{
    /// Connection Index
    uint8_t  conidx;
    /// Value identifier (@see htpc_val_id)
    uint8_t  val_id;
};

/// Parameters of the @ref HTPC_RD_CHAR_RSP message
struct htpc_rd_char_rsp
{
    /// Connection Index
    uint8_t  conidx;
    /// Value identifier (@see htpc_val_id)
    uint8_t  val_id;
    /// Status of the request
    uint16_t status;
    /// Attribute length
    uint16_t length;
    /// Attribute value
    uint8_t  value[__ARRAY_EMPTY];
};
/// @} HTPC

#endif // HTPC_MSG_H_

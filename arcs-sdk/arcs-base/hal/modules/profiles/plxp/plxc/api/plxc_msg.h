/**
 ****************************************************************************************
 *
 * @file plxc_msg.h
 *
 * @brief Header file - Pulse Oximeter Profile Collector/Client Role - Message API.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 ****************************************************************************************
 */

#ifndef _PLXC_MSG_H_
#define _PLXC_MSG_H_

/**
 ****************************************************************************************
 * @addtogroup PLXC
 * @ingroup Profile
 * @brief  Pulse Oximeter Profile Collector  - Message API
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "btip_task.h" // Task definitions
#include "plxp_common.h"
/*
 * ENUMERATIONS
 ****************************************************************************************
 */


/// Message IDs
enum plxc_msg_ids
{
    /// Enable the Profile Collector task - at connection
    PLXC_ENABLE_REQ         = MSG_ID(PLXC, 0x00),
    /// Response to Enable the Profile Collector task - at connection
    PLXC_ENABLE_RSP         = MSG_ID(PLXC, 0x01),
    /// Read Characteristic
    PLXC_READ_CMD           = MSG_ID(PLXC, 0x02),
    /// Configure Characteristic's CCC descriptor
    PLXC_CFG_CCC_CMD        = MSG_ID(PLXC, 0x03),
    /// Write Command to the Control Point
    PLXC_WRITE_RACP_CMD     = MSG_ID(PLXC, 0x04),
    /// Receive the Spot-check Measurement or Measurement Record Indication SPOT_MEAS
    /// *   Receive Continuous Measurement Notification CONT_MEAS
    /// *   Receive Control Point Response Indication RACP_RESP
    PLXC_VALUE_IND          = MSG_ID(PLXC, 0x05),
    /// Read CCC value of specific characteristic
    PLXC_RD_CHAR_CCC_IND    = MSG_ID(PLXC, 0x06),

    /// Complete event for the Application commands
    PLXC_CMP_EVT            = MSG_ID(PLXC, 0x07),
};


/// Pulse Oximeter Service Characteristics
enum plxc_char_ids
{
    /// PLXP SPOT-Measurement Characteristic
    PLXC_CHAR_SPOT_MEASUREMENT = 0,
    /// PLXP Continuous Measurement Characteristic
    PLXC_CHAR_CONT_MEASUREMENT,
    /// PLXP Features Characteristic
    PLXC_CHAR_FEATURES,
    /// PLXP Record Access Control Point Characteristic
    PLXC_CHAR_RACP,

    PLXC_CHAR_MAX,
};

/// Pulse Oximeter Service Characteristic Descriptors
enum plxc_desc_ids
{
    /// PLXP SPOT-Measurement Characteristic Client Configuration
    PLXC_DESC_SPOT_MEASUREMENT_CCC = 0,
    /// PLXP Continuous Measurement Characteristic Client Configuration
    PLXC_DESC_CONT_MEASUREMENT_CCC,
    /// PLXP Record Access Control Point Characteristic Client Configuration
    PLXC_DESC_RACP_CCC,

    PLXC_DESC_MAX,
};

/// Pulse Oximeter value identifier
enum plxc_val_id
{
    /// Sensor features
    PLXC_VAL_FEATURES            = 0,
    /// Spot-Check Measurement
    PLXC_VAL_SPOT_CHECK_MEAS     = 1,
    /// Continuous Measurement
    PLXC_VAL_CONTINUOUS_MEAS     = 2,
    /// RACP Response
    PLXC_VAL_RACP_RSP            = 3,

    /// Spot-Check Measurement CCC value
    PLXC_VAL_SPOT_CHECK_MEAS_CFG = 4,
    /// Continuous Measurement CCC value
    PLXC_VAL_CONTINUOUS_MEAS_CFG = 5,
    /// RACP CCC value
    PLXC_VAL_RACP_CFG            = 6,
};

/// Define command operation
enum plxc_op_codes
{
    /// Null Operation
    PLXC_NO_OP                       = 0,
    /// Read Operation
    PLXC_OP_CODE_READ                = 1,
    /// Write Client Characteristic Configuration Operation
    PLXC_OP_CODE_WRITE_CCC           = 2,
    /// Send RACP Request Operation
    PLXC_OP_CODE_WRITE_RACP          = 3,
};

/*
 * API MESSAGE STRUCTURES
 ****************************************************************************************
 */

/**
 * Structure containing the characteristics handles, value handles and descriptors for
 * the Pulse Oximeter Service
 */
typedef struct plxc_plxp_content
{
    /// service info
    prf_svc_t           svc;

    /// Characteristic info:
    ///  - Feature
    ///  - Measurement
    prf_char_t      chars[PLXC_CHAR_MAX];

    /// Descriptor handles:
    ///  - Client cfg
    prf_desc_t descs[PLXC_DESC_MAX];
} plxc_plxp_content_t;

/// Parameters of the @see PLXC_ENABLE_REQ message
struct plxc_enable_req
{
    /// Connection index
    uint8_t             conidx;
    /// Connection type
    uint8_t             con_type;
    /// Existing handle values
    plxc_plxp_content_t plx;
};

/// Parameters of the @see PLXC_ENABLE_RSP message
struct plxc_enable_rsp
{
    /// Connection index
    uint8_t             conidx;
    /// status
    uint16_t            status;
    /// Existing handle values
    plxc_plxp_content_t plx;
};

///*** PLXC CHARACTERISTIC READ REQUESTS
/// Parameters of the @see PLXC_READ_CMD message
/// operation
struct plxc_read_cmd
{
    /// Connection index
    uint8_t conidx;
    /// Value Identifier (@see enum plxc_val_id)
    uint8_t val_id;
};


///*** PLXC CHARACTERISTIC/CCC DESCRIPTOR  WRITE REQUESTS
/// Parameters of the @see PLXC_CFG_CCC_CMD message
/// operation
struct plxc_cfg_ccc_cmd
{
    /// Connection index
    uint8_t  conidx;
    /// Value Identifier (@see enum plxc_val_id)
    ///  - PLXC_VAL_SPOT_CHECK_MEAS_CFG
    ///  - PLXC_VAL_CONTINUOUS_MEAS_CFG
    ///  - PLXC_VAL_RACP_CFG
    uint8_t  val_id;
    /// The Client Characteristic Configuration Value
    uint16_t ccc;
};

/// Write Operation Command to the Control Point forward to Application
/// Parameters of the @see PLXC_WRITE_RACP_CMD,
struct plxc_write_racp_cmd
{
    /// Connection index
    uint8_t conidx;
    /// Control Point OpCode @see enum plxp_cp_opcodes_id
    uint8_t cp_opcode;
    /// Operator  @see enum plxp_cp_operator_id
    uint8_t cp_operator;
};



/// Indication the Record Access Control Point response
/// Indication the Spot-check Measurement or Measurement Record
/// Indication Continuous Measurement
/// Parameters of the @see PLXC_VALUE_IND,
struct plxc_value_ind
{
    /// Connection index
    uint8_t conidx;
    /// Value Identifier (@see enum plxc_val_id)
    ///    - PLXC_VAL_FEATURES
    ///    - PLXC_VAL_SPOT_CHECK_MEAS
    ///    - PLXC_VAL_CONTINUOUS_MEAS
    ///    - PLXC_VAL_RACP_RSP
    uint8_t val_id;

    union plxc_meas_value_tag
    {
        /// Spot-Check Measurement
        plxp_spot_meas_t spot_meas;
        /// Continuous Measurement
        plxp_cont_meas_t cont_meas;
        /// Record Access Control Point response
        plxp_racp_rsp_t  racp_rsp;
        /// Read Features request command value
        plxp_features_t  features;
    } value;
};

/// Inform Application about the Characteristic's CCC descriptor
/// Parameters of the @see PLXC_RD_CHAR_CCC_IND,
struct plxc_rd_char_ccc_ind
{
    /// Connection index
    uint8_t  conidx;
    /// Value Identifier (@see enum plxc_val_id)
    ///  - PLXC_VAL_SPOT_CHECK_MEAS_CFG
    ///  - PLXC_VAL_CONTINUOUS_MEAS_CFG
    ///  - PLXC_VAL_RACP_CFG
    uint8_t  val_id;
    /// Char. Client Characteristic Configuration
    uint16_t ind_cfg;
};

/// Complete event for the Application commands
///    PLXC_CMP_EVT,
/// Parameters of the @see PLXC_CMP_EVT message
/// Complete Event Information
struct  plxc_cmp_evt
{
    /// Connection index
    uint8_t  conidx;
    /// Operation  @see enum plxc_op_codes
    uint8_t  operation;
    /// Status
    uint16_t status;
};

/// @} PLXC

#endif //(_PLXC_MSG_H_)

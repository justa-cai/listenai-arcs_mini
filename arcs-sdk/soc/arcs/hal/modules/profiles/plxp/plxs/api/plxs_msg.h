/**
 ****************************************************************************************
 *
 * @file plxs_msg.h
 *
 * @brief Header file - Pulse Oximeter Profile Service - Message API.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 ****************************************************************************************
 */

#ifndef _PLXS_MSG_H_
#define _PLXS_MSG_H_

/**
 ****************************************************************************************
 * @addtogroup PLXS Task
 * @ingroup Profile
 * @brief Pulse Oximeter Profile- Message API.
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

/// Messages for Pulse Oximeter Profile Service
enum plxs_msg_id
{
    /// Enable the PLXP Sensor task for a connection
    PLXS_ENABLE_REQ         = MSG_ID(PLXS, 0x00),
    /// Enable the PLXP Sensor task for a connection
    PLXS_ENABLE_RSP         = MSG_ID(PLXS, 0x01),
    /// Send the Spot-check Measurement or Measurement Record
    /// Send Continuous Measurement
    PLXS_MEAS_VALUE_CMD     = MSG_ID(PLXS, 0x02),
    /// Send Control Point Response
    PLXS_RACP_RESP_SEND_CMD = MSG_ID(PLXS, 0x03),
    /// Inform Application on the Characteristics CCC descriptor changes
    PLXS_CFG_INDNTF_IND     = MSG_ID(PLXS, 0x04),
    /// Write to the Control Point forward to Application
    PLXS_RACP_REQ_RECV_IND  = MSG_ID(PLXS, 0x05),
    /// Complete event for the Application commands
    PLXS_CMP_EVT            = MSG_ID(PLXS, 0x06),
};

/// type_of_operation - Additional to Specification to select the type of operation
enum plxs_optype_id
{
    PLXS_OPTYPE_SPOT_CHECK_AND_CONTINUOUS   = 0, 
    PLXS_OPTYPE_SPOT_CHECK_ONLY             = 1, 
    PLXS_OPTYPE_CONTINUOUS_ONLY             = 2
};

/// Define command operation
enum plxs_op_codes
{
    PLXS_SPOT_CHECK_MEAS_CMD_OP_CODE = 1,
    PLXS_CONTINUOUS_MEAS_CMD_OP_CODE = 2,
    PLXS_RACP_CMD_OP_CODE            = 3,
};

/// Define characteristic type
enum plxs_char_types
{
    PLXS_SPOT_CHECK_MEAS_CODE    = 1,
    PLXS_CONTINUOUS_MEAS_CODE    = 2,
    PLXS_RACP_CODE               = 3,
};

/// Indication/notification configuration (put in feature flag to optimize memory usage)
enum plxs_evt_cfg_bf
{
    /// Bit used to know if Spot-Check measurement indication is enabled
    PLXS_MEAS_SPOT_IND_CFG_BIT  = CO_BIT(0),
    PLXS_MEAS_SPOT_IND_CFG_POS  = 0,
    /// Bit used to know if Continuous measurement notification is enabled
    PLXS_MEAS_CONT_NTF_CFG_BIT  = CO_BIT(1),
    PLXS_MEAS_CONT_NTF_CFG_POS  = 1,
    /// Bit used to know if record access control point indication is enabled
    PLXS_RACP_IND_CFG_BIT       = CO_BIT(2),
    PLXS_RACP_IND_CFG_POS       = 2,
};

/*
 * STRUCTURES
 ****************************************************************************************
 */

/// Parameters of the initialization function
struct plxs_db_cfg
{
    /// Define the Type of Operation @see enum plxs_optype_id
    uint8_t optype;
    /// Supported Features @see common enum plxp_sup_feat_bf
    uint16_t sup_feat;
    /// If enabled in  Supported Features @see enum plxp_meas_status_sup_bf
    uint16_t meas_stat_sup;
    /// If enabled in  Supported Features @see enum plxp_dev_sensor_status_bf
    uint32_t dev_stat_sup;
};

/// Parameters of the @see PLXS_ENABLE_REQ message
struct plxs_enable_req
{
    /// Connection index
    uint8_t conidx;
	/// Indication/notification configuration (@see enum plxs_evt_cfg_bf)
    uint8_t evt_cfg;
};

/// Parameters of the @see PLXS_ENABLE_RSP message
struct plxs_enable_rsp
{
    /// Connection index
    uint8_t  conidx;
    /// Status
    uint16_t status;
};

/// Send the Spot-check Measurement or Measurement Record
/// Send Continuous Measurement
/// Parameters of the @see PLXS_MEAS_VALUE_CMD,
struct plxs_meas_value_cmd
{
    /// Connection index
    uint8_t  conidx;
    /// Operation @see enum plxs_op_codes
    uint8_t  operation;

    union plxs_meas
    {
        /// Spot-Check Measurement
        plxp_spot_meas_t spot_meas;
        /// Continuous Measurement
        plxp_cont_meas_t cont_meas;
    } value;
};

/// Send Control Point Response
/// Parameters of the @see PLXS_RACP_RESP_SEND_CMD,
struct plxs_racp_rsp_send_cmd
{
    /// Connection index
    uint8_t  conidx;
    /// Operation = PLXS_RASP_CMD_OP_CODE @see enum plxs_op_codes
    uint8_t  operation;
    /// Request Control Point OpCode @see enum plxp_cp_opcodes_id
    uint8_t  req_cp_opcode;
    /// Response Code @see enum plxp_cp_resp_code_id
    uint8_t  rsp_code;
    /// Number of Records
    uint16_t rec_num;
};

/// Write to the Control Point forward to Application
/// Parameters of the @see PLXS_RACP_REQ_RECV_IND,
struct plxs_racp_req_recv_ind
{
    /// Connection index
    uint8_t  conidx;
    /// Control Point OpCode @see enum plxp_cp_opcodes_id
    uint8_t  cp_opcode;
    /// Operator  @see enum plxp_cp_operator_id
    uint8_t  cp_operator;
};

/// Inform Application on the Characteristic's CCC descriptor changes
/// Parameters of the @see PLXS_CFG_INDNTF_IND,
struct plxs_cfg_indntf_ind
{
    /// Connection index
    uint8_t  conidx;
    /// Indication/notification configuration (@see enum plxs_evt_cfg_bf)
    uint8_t evt_cfg;
};

/// Parameters of the @see PLXS_CMP_EVT message
struct plxs_cmp_evt
{
    /// Connection index
    uint8_t  conidx;
    /// Operation @see enum plxs_op_codes
    uint8_t  operation;
    /// Operation Status
    uint16_t status;
};

/// @} PLXS

#endif //(_PLXS_MSG_H_)

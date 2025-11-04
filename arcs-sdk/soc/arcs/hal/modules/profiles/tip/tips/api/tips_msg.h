/**
 ****************************************************************************************
 *
 * @file tips_msg.h
 *
 * @brief Header file - Time Profile Server  - Message API
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

#ifndef TIPS_MSG_H_
#define TIPS_MSG_H_

/**
 ****************************************************************************************
 * @addtogroup TIPS
 * @ingroup Profile
 * @brief Time Profile Server  - Message API
 *
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "tip_common.h"
#include "btip_task.h" // Task definitions

/*
 * DEFINES
 ****************************************************************************************
 */


/// Messages for Time Profile Server
/*@TRACE*/
enum tips_msg_id
{
    /// Start the Time Profile Server Role - at connection
    TIPS_ENABLE_REQ = TASK_FIRST_MSG(TASK_ID_TIPS),
    /// Inform the application about the task creation result
    TIPS_ENABLE_RSP,

    /// Update Current Time Request from APP
    TIPS_UPD_CURR_TIME_CMD,

    /// Inform APP about a requested read
    TIPS_RD_REQ_IND,
    /// Read request Response
    TIPS_RD_CFM,

    /// Inform APP about modification of Current Time Characteristic Client. Charact. Cfg
    TIPS_CURRENT_TIME_CCC_IND,
    /// Inform APP about modification of Time Update Control Point Characteristic Value
    TIPS_TIME_UPD_CTNL_PT_IND,
    /// Complete Event Information
    TIPS_CMP_EVT
};

/// Operation codes
enum tips_op_codes
{
    /// Database Creation Procedure
    TIPS_RESERVED_OP_CODE = 0,
    /// Current Time notify Operation Code
    TIPS_UPD_CURR_TIME_CMD_OP_CODE = 1,
};

/*
 * STRUCTURES
 ****************************************************************************************
 */
/// Parameters of the profile creation
struct tips_db_cfg
{
    /// Database configuration
    uint8_t features;
};

/// Parameters of the @ref TIPS_ENABLE_REQ message
struct tips_enable_req
{
    /// Connection index
    uint8_t  conidx;
    /// Current Time notification configuration
    uint16_t current_time_ntf_en;
};

/// Parameters of the @ref TIPS_ENABLE_RSP message
struct tips_enable_rsp
{
    /// Connection index
    uint8_t  conidx;
    /// Status
    uint16_t status;
};

/// Parameters of the @ref TIPS_UPD_CURR_TIME_CMD message
struct tips_upd_curr_time_cmd
{
    /// Connection index
    uint8_t         conidx;
    /// Current Time
    tip_curr_time_t current_time;
    /**
     * Indicate if the new Current Time value can be sent if the current_time_ntf_en parameter is
     * set to 1.
     * (0 - Disable; 1- Enable)
     *
     * If the time of the Current Time Server is changed because of reference time update (adjust reason)
     * then no notifications shall be sent to the Current Time Service Client within the 15 minutes from
     * the last notification, unless one of both of the two statements below are true :
     *         - The new time information differs by more than 1min from the Current Time Server
     *         time previous to the update
     *         - The update was caused by the client (interacting with another service)
     */
    uint8_t         enable_ntf_send;
};

/// Parameters of the @ref TIPS_RD_REQ_IND message
struct tips_rd_req_ind
{
    /// Connection index
    uint8_t         conidx;
    /// Value identifier (@see enum tip_value_id)
    uint8_t         val_id;
    /// Token value that must be provided in confirmation
    uint32_t        token;
};

/// Parameters of the @ref TIPS_RD_CFM message
struct tips_rd_cfm
{
    /// Connection index
    uint8_t         conidx;
    /// Value identifier (@see enum tip_value_id)
    uint8_t         val_id;
    /// Token value provided in request
    uint32_t        token;
    /// Read value data
    union tip_value value;
};

/// Parameters of the @ref TIPS_CURRENT_TIME_CCC_IND message
struct tips_current_time_ccc_ind
{
    /// Connection index
    uint8_t  conidx;
    /// Configuration Value
    uint16_t cfg_val;
};

/// Parameters of the @ref TIPS_TIME_UPD_CTNL_PT_IND message
struct tips_time_upd_ctnl_pt_ind
{
    /// Connection index
    uint8_t                 conidx;
    /// Time Update Control Point Value
    tip_time_upd_contr_pt_t value;
};

/// Parameters of the @ref TIPS_CMP_EVT message
struct tips_cmp_evt
{
    /// Connection index
    uint8_t  conidx;
    /// Operation
    uint8_t  operation;
    /// Status
    uint16_t status;
};


/// @} TIPS
#endif // TIPS_MSG_H_

/**
 ****************************************************************************************
 *
 * @file findl_msg.h
 *
 * @brief Header file - Find Me Locator - Message API
 *
 * Copyright (C) ListenAI 2020-2099
 *
 ****************************************************************************************
 */

#ifndef FINDL_MSG_H_
#define FINDL_MSG_H_

/**
 ****************************************************************************************
 * @addtogroup FINDL Find Me Locator
 * @ingroup Profile
 * @brief Find Me Profile Locator
 *
 * The FINDL is responsible for providing Find Me profile Locator functionalities to
 * upper layer module or application. The device using this profile takes the role
 * of find me locator.
 *
 * Find Me Locator (LOC): A LOC (e.g. PC, phone, etc)
 * is the term used by this profile to describe a device that can set an alarm level value
 * in the peer Find Me target Device (TG), causing the TG to start a sound or flashing light
 * or other type of signal allowing it to be located.
 *
 * The interface of this role to the Application is:
 *  - Enable the profile role (from APP)
 *  - Disable the profile role (from APP)
 *  - Discover Immediate Alert Service range(from APP) and Send result (to APP)
 *  - Discover Alert Level Characteristic in the IAS range(from APP) and Send Result (to APP)
 *  - Set alert level in Target (from APP)
 *  - Error indications (to APP)
 *
 *  The application should remember the discovered IAS range and Alert Level Char. handles
 *  for the next connection to a known peer (that may also advertise it supports Find Me Profile.)
 *  This allows the setting of the alert level to be faster.
 *
 *  The enable/disable of the profile and devices disconnection is handled in the application,
 *  depending on its User Input.
 *
 * @{
 ****************************************************************************************
 */
/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "btip_task.h" // Task definitions
#include "ble_prf.h"

/*
 * DEFINES
 ****************************************************************************************
 */

///Find Me Locator API messages
/*@TRACE*/
enum findl_msg_id
{
    /// Start the find me locator profile - at connection
    FINDL_ENABLE_REQ        = MSG_ID(FINDL, 0x00),
    /// Enable confirm message, containing IAS attribute details if discovery connection type
    FINDL_ENABLE_RSP        = MSG_ID(FINDL, 0x01),

    /// Alert level set request
    FINDL_SET_ALERT_CMD     = MSG_ID(FINDL, 0x02),
    /// Complete Event Information
    FINDL_CMP_EVT           = MSG_ID(FINDL, 0x03),
};

/// Operation codes
enum findl_op_codes
{
    /// Database Creation Procedure
    FINDL_RESERVED_OP_CODE      = 0,
    /// Set Alert level Operation Code
    FINDL_SET_ALERT_CMD_OP_CODE = 1,
};

/// Alert levels
enum findl_alert_lvl
{
    /// No alert
    FINDL_ALERT_NONE    = 0x00,
    /// Mild alert
    FINDL_ALERT_MILD    = 0x01,
    /// High alert
    FINDL_ALERT_HIGH    = 0x02,
};

/// Immediate Alert service details container
typedef struct ias_content
{
    /// Service info
    prf_svc_t      svc;

    /// Characteristic info:
    /// - Alert Level
    prf_char_t alert_lvl_char;
} ias_content_t;

/// Parameters of the @ref FINDL_ENABLE_REQ message
struct findl_enable_req
{
    /// Connection index
    uint8_t       conidx;
    ///Connection type
    uint8_t       con_type;
    ///Discovered IAS details if any
    ias_content_t ias;
};

/// Parameters of the @ref FINDL_ENABLE_RSP message
struct findl_enable_rsp
{
    /// Connection index
    uint8_t       conidx;
    ///Status
    uint16_t      status;
    ///Discovered IAS details if any
    ias_content_t ias;
};

/// Parameters of the @ref FINDL_SET_ALERT_CMD message
struct findl_set_alert_cmd
{
    /// Connection index
    uint8_t conidx;
    /// Alert level
    uint8_t alert_lvl;
};

///Parameters of the @ref FINDL_CMP_EVT message
struct findl_cmp_evt
{
    /// Connection index
    uint8_t  conidx;
    /// Operation
    uint8_t  operation;
    /// Status
    uint16_t status;
};

/// @} FINDL

#endif // FINDL_MSG_H_

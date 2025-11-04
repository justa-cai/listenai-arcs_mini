/**
 ****************************************************************************************
 *
 * @file proxm_msg.h
 *
 * @brief Header file - Proximity Monitor - Message API.
 *
 * Copyright (C) ListenAI 2020-2099
 ****************************************************************************************
 */

#ifndef PROXM_MSG_H_
#define PROXM_MSG_H_

/**
 ****************************************************************************************
 * @addtogroup PROXM
 * @ingroup Profile
 * @brief Proximity Profile Monitor - Message API
 *
 * The PROXM is responsible for providing proximity profile monitor functionalities to
 * upper layer module or application. The device using this profile takes the role
 * of a proximity monitor role.
 *
 * Proximity Monitor (PM): A PM (e.g. PC, phone, electronic door entry system, etc)
 * is the term used by this profile to describe a device that monitors the distance
 * between itself and the connected PR device. The profile on the PM device constantly
 * monitors the path loss between itself and the communicating Proximity Reporter
 * device. The profile provides indications to an application which can cause an alert
 * to the user.
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

/// Max number of characteristics for all services
#define PROXM_CHAR_NB_MAX       1

/*
 * ENUMERATIONS
 ****************************************************************************************
 */

/// Proximity Monitor API messages
/*@TRACE*/
enum proxm_msg_id
{
    /// Proximity Monitor role enable request from application.
    PROXM_ENABLE_REQ       = MSG_ID(PROXM, 0x00),
    /// Proximity Monitor role enable confirmation to application.
    PROXM_ENABLE_RSP       = MSG_ID(PROXM, 0x01),

    /// Read LLS Alert or TX Power Level - request
    PROXM_RD_CMD           = MSG_ID(PROXM, 0x02),
    /// Read LLS Alert or TX Power Level - response
    PROXM_RD_IND           = MSG_ID(PROXM, 0x03),

    /// Set Alert level
    PROXM_WR_ALERT_LVL_CMD = MSG_ID(PROXM, 0x04),

    /// Complete Event Information
    PROXM_CMP_EVT          = MSG_ID(PROXM, 0x05),
};

/// Service identifier
enum prox_svc_id
{
    /// Link Loss
    PROXM_LK_LOSS_SVC,
    /// Immediate Alert Service
    PROXM_IAS_SVC,
    /// TX Power Service
    PROXM_TX_POWER_SVC,

    PROXM_SVC_NB
};

/// Operation codes
enum proxm_op_codes
{
    /// Database Creation Procedure
    PROXM_RESERVED_OP_CODE     = 0,
    /// Read Operation Code
    PROXM_RD_OP_CODE           = 1,
    /// Set Alert Level Operation Code
    PROXM_WR_ALERT_LVL_OP_CODE = 2,
};

/// Link Loss or Immediate Alert code for setting alert through one message
enum proxm_wr_char_code
{
    /// Code for LLS Alert Level Char.
    PROXM_SET_LK_LOSS_ALERT = 0x00,
    /// Code for IAS Alert Level Char.
    PROXM_SET_IMMDT_ALERT,
};

/// Read Characteristic Code
enum proxm_rd_char_code
{
    /// Read Link Loss Service Alert Level Characteristic Value
    PROXM_RD_LL_ALERT_LVL = 0x00,
    /// Read TX Power Service TX Power Level Characteristic Value
    PROXM_RD_TX_POWER_LVL,
};

/// Alert Level Values
enum proxm_alert_lvl
{
    PROXM_ALERT_NONE    = 0x00,
    PROXM_ALERT_MILD,
    PROXM_ALERT_HIGH,
};

/*
 * API Messages Structures
 ****************************************************************************************
 */

/// Service information
typedef struct svc_content
{
    /// Service info
    prf_svc_t      svc;

    /// Characteristic info:
    /// - Alert Level for IAS and LLS
    /// - TX Power Level for TXPS
    prf_char_t chars[PROXM_CHAR_NB_MAX];
} svc_content_t;

/// Proximity monitor enable command structure
struct proxm_enable_req
{
    /// Connection index
    uint8_t       conidx;
    /// Connection type
    uint8_t       con_type;
    /// Reporter LLS details kept in APP
    svc_content_t lls;
    /// Reporter IAS details kept in APP
    svc_content_t ias;
    /// Reporter TPS details kept in APP
    svc_content_t txps;
};

/// Proximity monitor enable confirm structure
struct proxm_enable_rsp
{
    /// Connection index
    uint8_t       conidx;
    /// Status
    uint16_t      status;
    /// Reporter LLS details to keep in APP
    svc_content_t lls;
    /// Reporter IAS details to keep in APP
    svc_content_t ias;
    /// Reporter TPS details to keep in APP
    svc_content_t txps;
};

/// Parameters of the @ref PROXM_WR_ALERT_LVL_CMD message
struct proxm_wr_alert_lvl_cmd
{
    /// Connection index
    uint8_t conidx;
    /// 0=LLS or 1=IAS, code for the service in which the alert level should be written
    uint8_t svc_code;
    /// Alert level
    uint8_t lvl;
};

/// Parameters of the @ref PROXM_RD_CMD message
struct proxm_rd_cmd
{
    /// Connection index
    uint8_t conidx;
    /// 0=LLS or 1=TXPS, code for the service in which the alert level should be read @see enum proxm_rd_char_code
    uint8_t svc_code;
};

/// Parameters of the @ref PROXM_RD_IND message
struct proxm_rd_ind
{
    /// Connection index
    uint8_t conidx;
    /// 0=LLS or 1=TXPS, code for the service in which the alert level should be read
    uint8_t svc_code;
    /// Value
    uint8_t value;
};

/// Parameters of the @ref PROXM_CMP_EVT message
struct proxm_cmp_evt
{
    /// Connection index
    uint8_t  conidx;
    /// Operation
    uint8_t  operation;
    /// Status
    uint16_t status;
};

/// @} PROXM

#endif // PROXM_MSG_H_

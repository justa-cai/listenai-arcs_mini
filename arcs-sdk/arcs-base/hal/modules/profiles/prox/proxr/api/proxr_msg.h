/**
 ****************************************************************************************
 *
 * @file proxr_msg.h
 *
 * @brief Header file - Proximity Profile Reporter - Message API.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 ****************************************************************************************
 */

#ifndef PROXR_MSG_H_
#define PROXR_MSG_H_

/**
 ****************************************************************************************
 * @addtogroup PROXR
 * @ingroup Profile
 * @brief Proximity Profile Reporter - Message API
 *
 * The PROXR is responsible for providing proximity reporter functionalities to
 * upper layer module or application. The device using this profile takes the role
 * of a proximity reporter role.
 *
 * Proximity Reporter (PR): A PR (e.g. a watch, key fob, etc) is the term used by
 * this profile to describe a personal device that a user carries with them and that
 * has low power requirement (i.e. it is operating with a button cell), allowing another
 * device to monitor their connection. The device may have a simple user alert
 * functionality, for example a blinking LED or audible output.
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "btip_task.h" // Task definitions

/*
 * DEFINES
 ****************************************************************************************
 */

/*
 * ENUMERATIONS
 ****************************************************************************************
 */

/// Messages for Proximity Reporter
/*@TRACE*/
enum proxr_msg_id
{
    /// LLS/IAS Alert Level Indication
    PROXR_ALERT_IND = MSG_ID(PROXR, 0x00),
};

/// Proximity reporter feature bit field
enum proxr_feature
{
    /// immediate alert and TX Power services are not present
    PROXR_IAS_TXPS_NOT_SUP  = 0,
    /// immediate alert and TX Power services are present
    PROXR_IAS_TXPS_SUP      = 1,
};

/// Characteristics Code for Write Indications
enum proxr_char_id
{
    PROXR_ERR_CHAR  = 0,
    PROXR_LLS_CHAR  = 1,
    PROXR_IAS_CHAR  = 2,
};

/// Alert Level Values
enum proxr_alert_lvl
{
    PROXR_ALERT_NONE = 0,
    PROXR_ALERT_MILD = 1,
    PROXR_ALERT_HIGH = 2,
};

/*
 * API MESSAGES STRUCTURES
 ****************************************************************************************
 */
/// Parameters of the Proximity service database
struct proxr_db_cfg
{
    /// Proximity Feature (@see enum proxr_feature)
    uint16_t features;
};

/// Parameters of the @ref PROXR_ALERT_IND message
struct proxr_alert_ind
{
    /// Connection index
    uint8_t conidx;
    /// Alert level
    uint8_t alert_lvl;
    /// Char Code - Indicate if LLS or IAS
    uint8_t char_code;
};

/// @} PROXR

#endif // PROXR_MSG_H_

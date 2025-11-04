/**
 ****************************************************************************************
 *
 * @file findt_msg.h
 *
 * @brief Header file - Find Me Target - Message API.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 ****************************************************************************************
 */

#ifndef FINDT_MSG_H_
#define FINDT_MSG_H_

/**
 ****************************************************************************************
 * @addtogroup FINDT Find Me Target
 * @ingroup Profile
 *
 * @brief Find Me Profile Target.
 *
 * In the Find Me BLE Profile, the device that is in Target role will act as GATT server.
 *
 * The Target will react to alert levels written by the Locator in the Alert Level
 * Characteristic of the Immediate Alert Service(IAS) present in the ATT DB of the device.
 *
 * The interface of this role to the Application is:
 *  - Enable the profile role (from APP)
 *  - Disable the profile role (from APP)
 *  - Indicate that the alert level has been written by the Locator. (to APP)
 *
 * There shall be only one IAS instance on the device. There shall be only one
 * Alert Level Characteristic in the IAS. The characteristic properties shall be
 * Write No Response only.
 *
 *  The enable/disable of the profile and devices disconnection is handled in the application,
 *  depending on its User Input.
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

/// Messages for Find Me Target
/*@TRACE*/
enum findt_msg_id
{
    /// Alert indication
    FINDT_ALERT_IND = MSG_ID(FINDT, 0x00),
};


/// Alert levels
enum findt_alert_lvl
{
    /// No alert
    FINDT_ALERT_NONE    = 0x00,
    /// Mild alert
    FINDT_ALERT_MILD    = 0x01,
    /// High alert
    FINDT_ALERT_HIGH    = 0x02,
};


/// Parameters of the @ref FINDT_ALERT_IND message
struct findt_alert_ind
{
    /// Connection index
    uint8_t conidx;
    /// Alert level
    uint8_t alert_lvl;
};

/*
 * API MESSAGES STRUCTURES
 ****************************************************************************************
 */
/// Parameters for the database creation
struct findt_db_cfg
{
    /// Database configuration
    uint8_t dummy;
};

/// @} FINDT
#endif // FINDT_MSG_H_

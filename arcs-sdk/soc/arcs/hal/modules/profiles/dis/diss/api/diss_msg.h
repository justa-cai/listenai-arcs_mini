/**
 ****************************************************************************************
 *
 * @file diss_msg.h
 *
 * @brief Header file - Device Information Service Server - Message API.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

#ifndef DISS_MSG_H_
#define DISS_MSG_H_

/**
 ****************************************************************************************
 * @addtogroup DISS Device Information Service Server
 * @brief Device Information Service Server - Message API
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "ble_prf.h"
#include "ble_plf_config.h"


/*
 * DEFINES
 ****************************************************************************************
 */

///All features are supported
#define DIS_ALL_FEAT_SUP                     (0x01FF)


/*
 * ENUMERATIONS
 ****************************************************************************************
 */

/// Attribute Table Indexes
enum diss_val_id
{
    /// Manufacturer Name
    DIS_VAL_MANUFACTURER_NAME,
    /// Model Number
    DIS_VAL_MODEL_NB_STR,
    /// Serial Number
    DIS_VAL_SERIAL_NB_STR,
    /// HW Revision Number
    DIS_VAL_HARD_REV_STR,
    /// FW Revision Number
    DIS_VAL_FIRM_REV_STR,
    /// SW Revision Number
    DIS_VAL_SW_REV_STR,
    /// System Identifier Name
    DIS_VAL_SYSTEM_ID,
    /// IEEE Certificate
    DIS_VAL_IEEE,
    /// Plug and Play Identifier
    DIS_VAL_PNP_ID,

    DIS_VAL_MAX,
};

/// Database Configuration Flags (not used)
enum diss_features_bf
{
    ///Indicate if Manufacturer Name String Char. is supported
    DIS_MANUFACTURER_NAME_CHAR_SUP_POS          = 0,
    DIS_MANUFACTURER_NAME_CHAR_SUP_BIT          = 1<<(DIS_MANUFACTURER_NAME_CHAR_SUP_POS),

    ///Indicate if Model Number String Char. is supported
    DIS_MODEL_NB_STR_CHAR_SUP_POS               = 1,
    DIS_MODEL_NB_STR_CHAR_SUP_BIT               = 1<<(DIS_MODEL_NB_STR_CHAR_SUP_POS),

    ///Indicate if Serial Number String Char. is supported
    DIS_SERIAL_NB_STR_CHAR_SUP_POS              = 2,
    DIS_SERIAL_NB_STR_CHAR_SUP_BIT              = 1<<(DIS_SERIAL_NB_STR_CHAR_SUP_POS),

    ///Indicate if Hardware Revision String Char. supports indications
    DIS_HARD_REV_STR_CHAR_SUP_POS               = 3,
    DIS_HARD_REV_STR_CHAR_SUP_BIT               = 1<<(DIS_HARD_REV_STR_CHAR_SUP_POS),

    ///Indicate if Firmware Revision String Char. is writable
    DIS_FIRM_REV_STR_CHAR_SUP_POS               = 4,
    DIS_FIRM_REV_STR_CHAR_SUP_BIT               = 1<<(DIS_FIRM_REV_STR_CHAR_SUP_POS),

    ///Indicate if Software Revision String Char. is writable
    DIS_SW_REV_STR_CHAR_SUP_POS                 = 5,
    DIS_SW_REV_STR_CHAR_SUP_BIT                 = 1<<(DIS_SW_REV_STR_CHAR_SUP_POS),

    ///Indicate if System ID Char. is writable
    DIS_SYSTEM_ID_CHAR_SUP_POS                  = 6,
    DIS_SYSTEM_ID_CHAR_SUP_BIT                  = 1<<(DIS_SYSTEM_ID_CHAR_SUP_POS),

    ///Indicate if IEEE Char. is writable
    DIS_IEEE_CHAR_SUP_POS                       = 7,
    DIS_IEEE_CHAR_SUP_BIT                       = 1<<(DIS_IEEE_CHAR_SUP_POS),

    ///Indicate if PnP ID Char. is writable
    DIS_PNP_ID_CHAR_SUP_POS                     = 8,
    DIS_PNP_ID_CHAR_SUP_BIT                     = 1<<(DIS_PNP_ID_CHAR_SUP_POS),
};

/*
 * API MESSAGES STRUCTURES
 ****************************************************************************************
 */

/// Parameters for the database creation
struct diss_db_cfg
{
    /// Database configuration @see enum diss_features_bf
    uint16_t features;
};

/// @} DISS
#endif // DISS_MSG_H_

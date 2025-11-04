/**
 ****************************************************************************************
 *
 * @file bass_msg.h
 *
 * @brief Header file - Battery Service Server Role - Message API.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 ****************************************************************************************
 */


#ifndef _BASS_MSG_H_
#define _BASS_MSG_H_

/**
 ****************************************************************************************
 * @addtogroup BASS
 * @ingroup Profile
 * @brief  Battery Service Server Role - Message API.
 *
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

#define BAS_BATTERY_LVL_MAX               (100)
///Maximal number of BAS that can be added in the DB
#define BASS_NB_BAS_INSTANCES_MAX         (2)
/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */

/// Features Flag Masks
enum bass_features
{
    /// Battery Level Characteristic doesn't support notifications
    BAS_BATT_LVL_NTF_NOT_SUP,
    /// Battery Level Characteristic support notifications
    BAS_BATT_LVL_NTF_SUP,
};

/*
 * APIs Structures
 ****************************************************************************************
 */

/// Parameters for the database creation
struct bass_db_cfg
{
    /// Number of BAS to add
    uint8_t             bas_nb;
    /// Features of each BAS instance
    uint8_t             features[BASS_NB_BAS_INSTANCES_MAX];
    /// Battery Level Characteristic Presentation Format - Should not change during connection
//    prf_char_pres_fmt_t batt_level_pres_format[BASS_NB_BAS_INSTANCES_MAX];
};


/// @} BASS

#endif /* _BASS_MSG_H_ */

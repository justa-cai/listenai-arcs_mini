/**
 ****************************************************************************************
 *
 * @file proxr.h
 *
 * @brief Header file - Proximity Profile Reporter - Native API.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 ****************************************************************************************
 */

#ifndef PROXR_H_
#define PROXR_H_

/**
 ****************************************************************************************
 * @addtogroup PROXR
 * @ingroup Profile
 * @brief Proximity Profile Reporter - Native API
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "proxr_msg.h"

/*
 * DEFINES
 ****************************************************************************************
 */


/*
 * TYPES DEFINITIONS
 ****************************************************************************************
 */

/// Proximity reporter server callback set
typedef struct proxr_cb
{
    /**
     ****************************************************************************************
     * @brief This function is called when a Find me locator update the Alert level
     *
     * @param[in] conidx        Connection Index
     * @param[in] char_code     Char Code - Indicate if LLS or IAS (@see enum proxr_alert_lvl)
     * @param[in] alert_lvl     Alert Level (@see enum proxr_alert_lvl)
     ****************************************************************************************
     */
    void (*cb_alert_upd) (uint8_t conidx, uint8_t char_code, uint8_t alert_lvl);
} proxr_cb_t;

/*
 * API
 ****************************************************************************************
 */

/// @} PROXR

#endif // PROXR_H_

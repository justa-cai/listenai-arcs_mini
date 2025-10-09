/**
 ****************************************************************************************
 *
 * @file findt.h
 *
 * @brief Header file - Find Me Target - Native API.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 ****************************************************************************************
 */

#ifndef FINDT_H_
#define FINDT_H_

/**
 ****************************************************************************************
 * @addtogroup FINDT
 * @ingroup Profile
 *
 * @brief Find Me Profile Target - Native API
 * @{
 ****************************************************************************************
 */
/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "findt_msg.h"

/*
 * DEFINES
 ****************************************************************************************
 */


/*
 * TYPES DEFINITIONS
 ****************************************************************************************
 */

/// Find Me Target server callback set
typedef struct findt_cb
{
    /**
     ****************************************************************************************
     * @brief This function is called when a Find me locator update the Alert level
     *
     * @param[in] conidx        Connection Index
     * @param[in] alert_lvl     Alert Level (@see enum findt_alert_lvl)
     ****************************************************************************************
     */
    void (*cb_alert_upd) (uint8_t conidx, uint8_t alert_lvl);
} findt_cb_t;

/*
 * API
 ****************************************************************************************
 */


/// @} FINDT
#endif // FINDT_H_

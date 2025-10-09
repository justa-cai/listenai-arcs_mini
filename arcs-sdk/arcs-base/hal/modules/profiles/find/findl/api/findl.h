/**
 ****************************************************************************************
 *
 * @file findl.h
 *
 * @brief Header file - Find Me Locator - Native API
 *
 * Copyright (C) ListenAI 2020-2099
 *
 ****************************************************************************************
 */

#ifndef FINDL_H_
#define FINDL_H_

/**
 ****************************************************************************************
 * @addtogroup FINDL
 * @ingroup Profile
 * @brief Find Me Profile Locator - Native API
 * @{
 ****************************************************************************************
 */
/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "findl_msg.h"

/*
 * DEFINES
 ****************************************************************************************
 */

/*
 * ENUMERATIONS
 ****************************************************************************************
 */

/*
 * TYPES DEFINITIONS
 ****************************************************************************************
 */
/// Find Me client callback set
typedef struct findl_cb
{
    /**
     ****************************************************************************************
     * @brief This function is called when GATT server user has initiated event send to peer
     *        device or if an error occurs.
     *
     * @param[in] conidx        Connection index
     * @param[in] status        Client Enable status (@see enum hl_err)
     * @param[in] p_ias         Pointer to bond data information that describe peer database
     ****************************************************************************************
     */
    void (*cb_enable_cmp) (uint8_t conidx, uint16_t status, const ias_content_t* p_ias);

    /**
     ****************************************************************************************
     * @brief This function is called when Alert update procedure is over.
     *
     * @param[in] conidx        Connection index
     * @param[in] status        Read status (@see enum hl_err)
     ****************************************************************************************
     */
    void (*cb_alert_upd_cmp) (uint8_t conidx, uint16_t status);
} findl_cb_t;

/*
 * API
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Enable client profile by doing a discovery or restoring bond data
 *
 * @param[in] conidx        Connection index
 * @param[in] con_type      Connection type (@see enum prf_con_type)
 * @param[in] p_ias         Pointer to bond data information (valid if con_type == PRF_CON_NORMAL)
 *
 * @return Function execution status (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t findl_enable(uint8_t conidx, uint8_t con_type, const ias_content_t* p_ias);

/**
 ****************************************************************************************
 * @brief Update peer device alert level
 *
 * @param[in] conidx     Connection index
 * @param[in] alert_lvl  Alert level (see enum findl_alert_lvl)
 *
 * @return Function execution status (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t findl_alert_upd(uint8_t conidx, uint8_t alert_lvl);

/// @} FINDL

#endif // FINDL_H_

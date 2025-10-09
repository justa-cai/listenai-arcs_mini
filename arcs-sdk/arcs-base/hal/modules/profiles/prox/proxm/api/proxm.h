/**
 ****************************************************************************************
 *
 * @file proxm_msg.h
 *
 * @brief Header file - Proximity Monitor - Native API.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 ****************************************************************************************
 */

#ifndef PROXM_H_
#define PROXM_H_

/**
 ****************************************************************************************
 * @addtogroup PROXM
 * @ingroup Profile
 * @brief Proximity Profile Monitor - Native API
 *
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "proxm_msg.h"

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
/// Proximity Monitor client callback set
typedef struct proxm_cb
{
    /**
     ****************************************************************************************
     * @brief This function is called when GATT server user has initiated event send to peer
     *        device or if an error occurs.
     *
     * @param[in] conidx        Connection index
     * @param[in] status        Client Enable status (@see enum hl_err)
     * @param[in] p_lls         Pointer to bond data information that describe link loss peer service
     * @param[in] p_ias         Pointer to bond data information that describe immediate alert peer service
     * @param[in] p_txps        Pointer to bond data information that describe TX power peer service
     ****************************************************************************************
     */
    void (*cb_enable_cmp) (uint8_t conidx, uint16_t status, const svc_content_t* p_lls, const svc_content_t* p_ias,
                           const svc_content_t* p_txps);

    /**
     ****************************************************************************************
     * @brief This function is called when Alert update procedure is over.
     *
     * @param[in] conidx        Connection index
     * @param[in] svc_code      0=LLS or 1=IAS, code for the service in which the alert level should be written
     *                          (@see enum proxm_wr_char_code)
     * @param[in] status        Read status (@see enum hl_err)
     ****************************************************************************************
     */
    void (*cb_alert_upd_cmp) (uint8_t conidx, uint8_t svc_code, uint16_t status);

    /**
     ****************************************************************************************
     * @brief This function is called when read link loss alert level procedure is over.
     *
     * @param[in] conidx        Connection index
     * @param[in] status        Client Enable status (@see enum hl_err)
     * @param[in] alert_lvl     Alert level (see enum proxm_alert_lvl)
     ****************************************************************************************
     */
    void (*cb_read_lls_alert_lvl_cmp) (uint8_t conidx, uint16_t status, uint8_t alert_lvl);

    /**
     ****************************************************************************************
     * @brief This function is called when read peer ADV TX power level procedure is over.
     *
     * @param[in] conidx        Connection index
     * @param[in] status        Client Enable status (@see enum hl_err)
     * @param[in] power_lvl     Advertising channel TX power level
     ****************************************************************************************
     */
    void (*cb_read_tx_pwr_lvl_cmp) (uint8_t conidx, uint16_t status, int8_t power_lvl);
} proxm_cb_t;

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
 * @param[in] p_lls         Pointer to bond data information that describe link loss peer service
 * @param[in] p_ias         Pointer to bond data information that describe immediate alert peer service
 * @param[in] p_txps        Pointer to bond data information that describe TX power peer service
 *
 * @return Function execution status (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t proxm_enable(uint8_t conidx, uint8_t con_type, const svc_content_t* p_lls, const svc_content_t* p_ias,
                      const svc_content_t* p_txps);

/**
 ****************************************************************************************
 * @brief Update peer device alert level
 *
 * @param[in] conidx      Connection index
 * @param[in] svc_code    0=LLS or 1=IAS, code for the service in which the alert level should be written
 *                        (@see enum proxm_wr_char_code)
 * @param[in] alert_lvl   Alert level (see enum proxm_alert_lvl)
 *
 * @return Function execution status (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t proxm_alert_upd(uint8_t conidx, uint8_t svc_code, uint8_t alert_lvl);

/**
 ****************************************************************************************
 * @brief Read peer Link loss alert level
 *
 * @param[in] conidx   Connection index
 *
 * @return Function execution status (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t proxm_read_lls_alert_lvl(uint8_t conidx);

/**
 ****************************************************************************************
 * @brief Read peer ADV TX power level
 *
 * @param[in] conidx   Connection index
 *
 * @return Function execution status (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t proxm_read_tx_pwr_lvl(uint8_t conidx);

/// @} PROXM

#endif // PROXM_H_

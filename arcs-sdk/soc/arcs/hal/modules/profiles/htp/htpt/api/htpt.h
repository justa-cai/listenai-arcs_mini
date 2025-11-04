/**
 ****************************************************************************************
 *
 * @file htpc.h
 *
 * @brief Header file - Health Thermometer Profile Thermometer - Native API.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 ****************************************************************************************
 */

#ifndef HTPT_H_
#define HTPT_H_

/**
 ****************************************************************************************
 * @addtogroup HTPT
 * @brief Health Thermometer Profile Thermometer - Native API.
 *
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "htpt_msg.h"

/*
 * DEFINES
 ****************************************************************************************
 */


/*
 * ENUMERATIONS
 ****************************************************************************************
 */

/*
 * NATIVE API CALLBACKS
 ****************************************************************************************
 */

/// Health Thermometer Profile server callback set
typedef struct htpt_cb
{
    /**
     ****************************************************************************************
     * @brief Completion of Send temperature procedure
     *
     * @param[in] status Status of the procedure execution (@see enum hl_err)
     ****************************************************************************************
     */
    void (*cb_temp_send_cmp)(uint16_t status);

    /**
     ****************************************************************************************
     * @brief Inform that peer device want to update of measurement interval value
     *
     * @param[in] conidx    Connection index
     * @param[in] meas_intv Measurement Interval value in seconds
     ****************************************************************************************
     */
    void (*cb_meas_intv_chg_req)(uint8_t conidx, uint16_t meas_intv);

    /**
     ****************************************************************************************
     * @brief Inform that Bond data updated for the connection.
     *
     * @param[in] conidx        Connection index
     * @param[in] ntf_ind_cfg   Notification Configuration (@see enum htpt_ntf_ind_cfg)
     ****************************************************************************************
     */
    void (*cb_bond_data_upd)(uint8_t conidx, uint8_t ntf_ind_cfg);
} htpt_cb_t;

/*
 * NATIVE API FUNCTIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Restore bond data of a known peer device (at connection establishment)
 *
 * @param[in] conidx        Connection index
 * @param[in] ntf_ind_cfg   Notification configuration (Bond Data to restore: @see enum htpt_ntf_ind_cfg)
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t htpt_enable(uint8_t conidx, uint8_t ntf_ind_cfg);

/**
 ****************************************************************************************
 * @brief Send temperature value from APP
 *
 * Wait for @see cb_temp_send_cmp execution before sending a new measurement
 *
 * @param[in] p_temp_meas   Pointer to Temperature Measurement information
 * @param[in] stable_meas   Stable or intermediary type of temperature (True stable meas, else false)
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t htpt_temp_send(const htp_temp_meas_t* p_temp_meas, bool stable_meas);

/**
 ****************************************************************************************
 * @brief Update the measurement interval value
 *
 * @param[in] meas_intv Measurement Interval value in seconds
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t htpt_meas_intv_upd(uint16_t meas_intv);

/**
 ****************************************************************************************
 * @brief Confirm or not update of measurement interval value
 *
 * @param[in] conidx  Connection index
 * @param[in] status  Status of the request
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t htpt_meas_intv_chg_cfm(uint8_t conidx, uint16_t status);



/// @} HTPT
#endif // HTPT_H_

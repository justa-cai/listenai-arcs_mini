/**
 ****************************************************************************************
 *
 * @file htpc.h
 *
 * @brief Header file - Health Thermometer Profile Collector - Native API.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 ****************************************************************************************
 */

#ifndef HTPT_C_
#define HTPT_C_

/**
 ****************************************************************************************
 * @addtogroup HTPC
 * @brief Health Thermometer Profile Collector - Native API.
 *
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "htpc_msg.h"

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

/// Health Thermometer Profile client callback set
typedef struct htpc_cb
{
    /**
     ****************************************************************************************
     * @brief Completion of Enable procedure
     *
     * @param[in] conidx        Connection index
     * @param[in] status        Status of the procedure execution (@see enum hl_err)
     * @param[in] p_hts         Pointer to peer database description bond data
     ****************************************************************************************
     */
    void (*cb_enable_cmp)(uint8_t conidx, uint16_t status, const htpc_hts_content_t* p_hts);

    /**
     ****************************************************************************************
     * @brief Inform that update of Notification configuration is over
     *
     * @param[in] conidx    Connection index
     * @param[in] status    Status of the procedure execution (@see enum hl_err)
     ****************************************************************************************
     */
    void (*cb_health_temp_ntf_cfg_cmp)(uint8_t conidx, uint16_t status);

    /**
     ****************************************************************************************
     * @brief Inform that update of measurement interval is over
     *
     * @param[in] conidx    Connection index
     * @param[in] status    Status of the procedure execution (@see enum hl_err)
     ****************************************************************************************
     */
    void (*cb_wr_meas_intv_cmp)(uint8_t conidx, uint16_t status);

    /**
     ****************************************************************************************
     * @brief Inform that characteristic information has been received
     *
     * @param[in] conidx    Connection index
     * @param[in] status    Status of the procedure execution (@see enum hl_err)
     * @param[in] val_id    Value identifier (@see htpc_val_id)
     * @param[in] length    Value length
     * @param[in] p_data    Pointer to data value
     ****************************************************************************************
     */
    void (*cb_rd_char_cmp)(uint8_t conidx, uint16_t status, uint8_t val_id, uint16_t length, const uint8_t* p_data);

    /**
     ****************************************************************************************
     * @brief Inform that new temperature data has been received
     *
     * @param[in] conidx       Connection index
     * @param[in] p_temp_meas  Pointer to Temperature Measurement value
     * @param[in] stable_meas  Stable or intermediary type of temperature
     ****************************************************************************************
     */
    void (*cb_temp_ind)(uint8_t conidx, const htp_temp_meas_t* p_temp_meas, bool stable_meas);

    /**
     ****************************************************************************************
     * @brief Inform that update of measurement interval has been received
     *
     * @param[in] conidx       Connection index
     * @param[in] meas_intv    Measurement interval in seconds
     ****************************************************************************************
     */
    void (*cb_meas_intv_ind)(uint8_t conidx, uint16_t meas_intv);
} htpc_cb_t;

/*
 * NATIVE API FUNCTIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Restore bond data of a known peer device (at connection establishment)
 *
 * @param[in] conidx        Connection index
 * @param[in] con_type      Connection type
 * @param[in] p_hts         Pointer to peer database description bond data
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t htpc_enable(uint8_t conidx, uint8_t con_type, const htpc_hts_content_t* p_hts);

/**
 ****************************************************************************************
 * @brief Write Health Thermometer Notification Configuration Value
 *
 * @param[in] conidx        Connection index
 * @param[in] char_code     Own code for differentiating between Temperature Measurement,
 *                          Intermediate Temperature and Measurement Interval chars (@see htpc_chars)
 * @param[in] cfg_val       Stop/notify/indicate value to configure into the peer characteristic
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t htpc_health_temp_ntf_cfg(uint8_t conidx, uint8_t char_code, uint16_t cfg_val);

/**
 ****************************************************************************************
 * @brief Update Measurement interval
 *
 * @param[in] conidx        Connection index
 * @param[in] meas_intv     Measurement interval in seconds
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t htpc_wr_meas_intv(uint8_t conidx, uint16_t meas_intv);

/**
 ****************************************************************************************
 * @brief Read characteristic value
 *
 * @param[in] conidx        Connection index
 * @param[in] val_id        Value identifier (@see htpc_val_id)
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t htpc_rd_char(uint8_t conidx, uint8_t val_id);

/// @} HTPC
#endif // HTPC_H_

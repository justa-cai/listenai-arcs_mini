/**
 ****************************************************************************************
 *
 * @file tips.h
 *
 * @brief Header file - Time Profile Server - Native API
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

#ifndef TIPS_H_
#define TIPS_H_

/**
 ****************************************************************************************
 * @addtogroup TIPS
 * @ingroup Profile
 * @brief Time Profile Server - Native API
 *
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "tips_msg.h"


/*
 * DEFINES
 ****************************************************************************************
 */


/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */


/*
 * NATIVE API CALLBACKS
 ****************************************************************************************
 */

/// Time profile server callback set
typedef struct tips_cb
{
    /**
     ****************************************************************************************
     * @brief Inform that bond data updated for the connection.
     *
     * @param[in] conidx        Connection index
     * @param[in] cfg_val       Char. Client Characteristic Configuration
     ****************************************************************************************
     */
    void (*cb_bond_data_upd)(uint8_t conidx, uint16_t cfg_val);

    /**
     ****************************************************************************************
     * @brief Completion of current time update procedure
     *
     * @param[in] conidx        Connection index
     * @param[in] status        Status of the procedure execution (@see enum hl_err)
     ****************************************************************************************
     */
    void (*cb_curr_time_upd_cmp)(uint8_t conidx, uint16_t status);

    /**
     ****************************************************************************************
     * @brief Read a time information
     *
     * Time information must be returned by application using @see tips_rd_info_cfm
     *
     * @param[in] conidx        Connection index
     * @param[in] token         Token information that must be returned in confirmation
     * @param[in] val_id        Value identifier (@see enum tip_value_id)
     ****************************************************************************************
     */
    void (*cb_rd_info_req)(uint8_t conidx, uint32_t token, uint8_t val_id);

    /**
     ****************************************************************************************
     * @brief Command received on  control point
     *
     * @param[in] conidx          Connection index
     * @param[in] value           Time Update Control Point Value
     ****************************************************************************************
     */
    void (*cb_ctnl_pt)(uint8_t conidx, tip_time_upd_contr_pt_t value);
} tips_cb_t;

/*
 * NATIVE API FUNCTIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Restore bond data of a known peer device (at connection establishment)
 *
 * @param[in] conidx       Connection index
 * @param[in] ntf_cfg      Current Time notification configuration
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t tips_enable(uint8_t conidx, uint16_t ntf_cfg);

/**
 ****************************************************************************************
 * @brief Send Current time value information
 *
 * @param[in] conidx            Connection index
 * @param[in] enable_ntf_send   Indicate if the new Current Time value can be sent if the
 *                              current_time_ntf_en parameter is enabled
 * @param[in] p_current_time    Pointer to current time value
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t tips_curr_time_upd(uint8_t conidx, bool enable_ntf_send, const tip_curr_time_t* p_current_time);

/**
 ****************************************************************************************
 * @brief Send back time information
 *
 * @param[in] conidx        Connection index
 * @param[in] token         Token information received in request
 * @param[in] val_id        Value identifier (@see enum tip_value_id)
 * @param[in] p_value       Pointer to time value information
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t tips_rd_info_cfm(uint8_t conidx, uint32_t token, uint8_t val_id, const union tip_value* p_value);


/// @} TIPS
#endif // TIPS_H_

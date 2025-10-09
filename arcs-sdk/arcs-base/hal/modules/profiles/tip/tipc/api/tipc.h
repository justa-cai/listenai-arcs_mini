/**
 ****************************************************************************************
 *
 * @file tipc.h
 *
 * @brief Header file - Time Profile Client - Native API
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

#ifndef _TIPC_H_
#define _TIPC_H_

/**
 ****************************************************************************************
 * @addtogroup TIPC
 * @ingroup Profile
 * @brief Time Profile Client - Native API
 *
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "tipc_msg.h"

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */

/*
 * NATIVE API CALLBACKS
 ****************************************************************************************
 */

/// Phone Alert service client callback set
typedef struct tipc_cb
{
    /**
     ****************************************************************************************
     * @brief Completion of enable procedure
     *
     * @param[in] conidx        Connection index
     * @param[in] status        Status of the procedure execution (@see enum hl_err)
     * @param[in] p_cts         Pointer to peer CTS database description bond data
     * @param[in] p_ndcs        Pointer to peer NDCS database description bond data
     * @param[in] p_rtus        Pointer to peer RTUS database description bond data
     ****************************************************************************************
     */
    void (*cb_enable_cmp)(uint8_t conidx, uint16_t status, const tipc_cts_content_t* p_cts,
                          const tipc_ndcs_content_t* p_ndcs, const tipc_rtus_content_t* p_rtus);

    /**
     ****************************************************************************************
     * @brief Completion of read alert status procedure.
     *
     * @param[in] conidx        Connection index
     * @param[in] status        Status of the procedure execution (@see enum hl_err)
     * @param[in] val_id        Value identifier (@see enum tip_value_id)
     * @param[in] p_value       Pointer to time value information
     *
     ****************************************************************************************
     */
    void (*cb_read_cmp)(uint8_t conidx, uint16_t status, uint8_t val_id, const union tip_value* p_value);

    /**
     ****************************************************************************************
     * @brief Completion of write Characteristic Configuration procedure.
     *
     * @param[in] conidx        Connection index
     * @param[in] status        Status of the procedure execution (@see enum hl_err)
     *
     ****************************************************************************************
     */
    void (*cb_write_cfg_cmp)(uint8_t conidx, uint16_t status);

    /**
     ****************************************************************************************
     * @brief Function called when current time update is received
     *
     * @param[in] conidx         Connection index
     * @param[in] p_current_time Pointer to current time update value
     ****************************************************************************************
     */
    void (*cb_curr_time)(uint8_t conidx, const tip_curr_time_t* p_current_time);

    /**
     ****************************************************************************************
     * @brief Completion of control point request procedure
     *
     * @param[in] conidx        Connection index
     * @param[in] status        Status of the Request Send (@see enum hl_err)
     ****************************************************************************************
     */
    void (*cb_ctnl_pt_req_cmp)(uint8_t conidx, uint16_t status);
} tipc_cb_t;

/*
 * NATIVE API FUNCTIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Restore bond data of a known peer device (at connection establishment)
 *
 * Wait for @see cb_enable_cmp execution before starting a new procedure
 *
 * @param[in] conidx        Connection index
 * @param[in] con_type      Connection type
 * @param[in] p_cts         Pointer to peer CTS database description bond data
 * @param[in] p_ndcs        Pointer to peer NDCS database description bond data
 * @param[in] p_rtus        Pointer to peer RTUS database description bond data
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t tipc_enable(uint8_t conidx, uint8_t con_type, const tipc_cts_content_t* p_cts,
                     const tipc_ndcs_content_t* p_ndcs, const tipc_rtus_content_t* p_rtus);

/**
 ****************************************************************************************
 * @brief Perform a read procedure.
 *
 * Wait for @see cb_read_cmp execution before starting a new procedure
 *
 * @param[in] conidx        Connection index
 * @param[in] val_id        Value identifier (@see enum tip_value_id)
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t tipc_read(uint8_t conidx, uint8_t val_id);


/**
 ****************************************************************************************
 * @brief Perform a write Characteristic Configuration procedure.
 *
 * Wait for @see cb_write_cfg_cmp execution before starting a new procedure
 *
 * @param[in] conidx        Connection index
 * @param[in] cfg_val       Configuration value
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t tipc_write_cfg(uint8_t conidx, uint16_t cfg_val);

/**
 ****************************************************************************************
 * @brief Function called to send a control point request
 *
 * Wait for @see cb_ctnl_pt_req_cmp execution before starting a new procedure
 *
 * @param[in] conidx         Connection index
 * @param[in] value          Control Point value
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t tipc_ctnl_pt_req(uint8_t conidx, tip_time_upd_contr_pt_t value);

/// @} TIPC
#endif /* _TIPC_H_ */

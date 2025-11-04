/**
 ****************************************************************************************
 *
 * @file plxs.h
 *
 * @brief Header file - Pulse Oximeter Profile Service - Native API.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 ****************************************************************************************
 */

#ifndef _PLXS_H_
#define _PLXS_H_

/**
 ****************************************************************************************
 * @addtogroup PLXS Task
 * @ingroup Profile
 * @brief Pulse Oximeter Profile- Native API.
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "plxs_msg.h"


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

/// Pulse oximeter sensor server callback set
typedef struct plxs_cb
{
    /**
     ****************************************************************************************
     * @brief Completion of Spot-Check measurement transmission
     *
     * @param[in] conidx        Connection index
     * @param[in] status        Status of the procedure execution (@see enum hl_err)
     ****************************************************************************************
     */
    void (*cb_spot_meas_send_cmp)(uint8_t conidx, uint16_t status);
    /**
     ****************************************************************************************
     * @brief Completion of Continuous measurement transmission
     *
     * @param[in] conidx        Connection index
     * @param[in] status        Status of the procedure execution (@see enum hl_err)
     ****************************************************************************************
     */
    void (*cb_cont_meas_send_cmp)(uint8_t conidx, uint16_t status);

    /**
     ****************************************************************************************
     * @brief Inform that bond data updated for the connection.
     *
     * @param[in] conidx        Connection index
     * @param[in] evt_cfg       Indication/notification configuration (@see enum plxs_evt_cfg_bf)
     ****************************************************************************************
     */
    void (*cb_bond_data_upd)(uint8_t conidx, uint8_t evt_cfg);

    /**
     ****************************************************************************************
     * @brief Inform that peer device requests an action using record access control point
     *
     * @note control point request must be answered using @see plxs_racp_rsp_send function
     *
     * @param[in] conidx        Connection index
     * @param[in] op_code       Operation Code (@see plxp_cp_opcodes_id)
     * @param[in] func_operator Function operator (see enum plxp_cp_operator_id)
     ****************************************************************************************
     */
    void (*cb_racp_req)(uint8_t conidx, uint8_t op_code, uint8_t func_operator);

    /**
     ****************************************************************************************
     * @brief Completion of record access control point response send procedure
     *
     * @param[in] conidx        Connection index
     * @param[in] status        Status of the procedure execution (@see enum hl_err)
     ****************************************************************************************
     */
    void (*cb_racp_rsp_send_cmp)(uint8_t conidx, uint16_t status);

} plxs_cb_t;

/*
 * NATIVE API FUNCTIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Restore bond data of a known peer device (at connection establishment)
 *
 * @param[in] conidx        Connection index
 * @param[in] evt_cfg       Indication/notification configuration (@see enum plxs_evt_cfg_bf)
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t plxs_enable(uint8_t conidx, uint8_t evt_cfg);

/**
 ****************************************************************************************
 * @brief Send a Spot-Check measurement to registered peer devices
 *
 * Wait for @see cb_spot_meas_send_cmp execution before starting a new procedure
 *
 * @param[in] conidx           Connection index
 * @param[in] seq_num          Measurement sequence number
 * @param[in] p_spot_meas      Pointer to Spot-Check measurement information
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t plxs_spot_meas_send(uint8_t conidx, const plxp_spot_meas_t* p_spot_meas);

/**
 ****************************************************************************************
 * @brief Send a continuous measurement to registered peer devices
 *
 * Wait for @see cb_cont_meas_send_cmp execution before starting a new procedure
 *
 * @param[in] conidx           Connection index
 * @param[in] seq_num          Measurement sequence number
 * @param[in] p_cont_meas      Pointer to Spot-Check measurement information
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t plxs_cont_meas_send(uint8_t conidx, const plxp_cont_meas_t* p_cont_meas);

/**
 ****************************************************************************************
 * @brief Send record access control point response.
 *
 * Wait for @see cb_ctnl_pt_rsp_send_cmp execution before starting a new procedure
 *
 * @param[in] conidx        Connection index
 * @param[in] op_code       Requested Operation Code (@see enum plxp_cp_opcodes_id)
 * @param[in] racp_status   Record access control point execution status (@see enum plxp_cp_resp_code_id)
 * @param[in] num_of_record Number of record (meaningful for PLXP_OPCODE_REPORT_NUMBER_OF_STORED_RECORDS operation)
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t plxs_racp_rsp_send(uint8_t conidx, uint8_t op_code, uint8_t racp_status, uint16_t num_of_record);


/// @} PLXS

#endif //(_PLXS_H_)

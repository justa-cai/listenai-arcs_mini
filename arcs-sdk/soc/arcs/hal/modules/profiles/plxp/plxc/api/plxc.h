/**
 ****************************************************************************************
 *
 * @file plxc.h
 *
 * @brief Header file - Pulse Oximeter Profile Collector/Client Role - Native API.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 ****************************************************************************************
 */

#ifndef _PLXC_H_
#define _PLXC_H_

/**
 ****************************************************************************************
 * @addtogroup PLXC
 * @ingroup Profile
 * @brief  Pulse Oximeter Profile Collector  - Native API
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "plxc_msg.h"


/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */

/*
 * NATIVE API CALLBACKS
 ****************************************************************************************
 */

/// Pulse oximeter sensor client callback set
typedef struct plxc_cb
{
    /**
     ****************************************************************************************
     * @brief Completion of enable procedure
     *
     * @param[in] conidx        Connection index
     * @param[in] status        Status of the procedure execution (@see enum hl_err)
     * @param[in] p_plx         Pointer to peer database description bond data
     *
     ****************************************************************************************
     */
    void (*cb_enable_cmp)(uint8_t conidx, uint16_t status, const plxc_plxp_content_t* p_plx);

    /**
     ****************************************************************************************
     * @brief Completion of read feature procedure.
     *
     * @param[in] conidx        Connection index
     * @param[in] status        Status of the procedure execution (@see enum hl_err)
     * @param[in] p_features    Pointer to sensor features information
     *
     ****************************************************************************************
     */
    void (*cb_read_features_cmp)(uint8_t conidx, uint16_t status, const plxp_features_t* p_features);

    /**
     ****************************************************************************************
     * @brief Completion of read Characteristic Configuration procedure.
     *
     * @param[in] conidx        Connection index
     * @param[in] status        Status of the procedure execution (@see enum hl_err)
     * @param[in] val_id        Value identifier (@see enum plxc_val_id)
     *                               - PLXC_VAL_SPOT_CHECK_MEAS_CFG
     *                               - PLXC_VAL_CONTINUOUS_MEAS_CFG
     *                               - PLXC_VAL_RACP_CFG
     * @param[in] cfg_val       Configuration value
     *
     ****************************************************************************************
     */
    void (*cb_read_cfg_cmp)(uint8_t conidx, uint16_t status, uint8_t val_id, uint16_t cfg_val);

    /**
     ****************************************************************************************
     * @brief Completion of sensor notification / indication configuration procedure.
     *
     * @param[in] conidx        Connection index
     * @param[in] status        Status of the procedure execution (@see enum hl_err)
     * @param[in] val_id        Value identifier (@see enum plxc_val_id)
     *                               - PLXC_VAL_SPOT_CHECK_MEAS_CFG
     *                               - PLXC_VAL_CONTINUOUS_MEAS_CFG
     *                               - PLXC_VAL_RACP_CFG
     *
     ****************************************************************************************
     */
    void (*cb_write_cfg_cmp)(uint8_t conidx, uint16_t status, uint8_t val_id);

    /**
     ****************************************************************************************
     * @brief Function called when Spot-Check measurement information is received
     *
     * @param[in] conidx         Connection index
     * @param[in] p_spot_meas    Pointer to Spot-Check measurement information
     *
     ****************************************************************************************
     */
    void (*cb_spot_meas)(uint8_t conidx, const plxp_spot_meas_t* p_spot_meas);


    /**
     ****************************************************************************************
     * @brief Function called when Continuous measurement information is received
     *
     * @param[in] conidx         Connection index
     * @param[in] p_cont_meas    Pointer to continuous measurement information
     *
     ****************************************************************************************
     */
    void (*cb_cont_meas)(uint8_t conidx, const plxp_cont_meas_t* p_cont_meas);

    /**
     ****************************************************************************************
     * @brief Completion of record access control point request send
     *
     * @param[in] conidx        Connection index
     * @param[in] status        Status of the Request Send (@see enum hl_err)
     * @param[in] req_op_code   Requested Operation Code (@see enum plxp_cp_operator_id)
     *
     ****************************************************************************************
     */
    void (*cb_racp_req_cmp)(uint8_t conidx, uint16_t status, uint8_t req_op_code);

    /**
     ****************************************************************************************
     * @brief Function called when record access point response is received
     *
     * @param[in] conidx        Connection index
     * @param[in] req_op_code   Requested Operation Code (@see enum plxp_cp_operator_id)
     * @param[in] racp_status   Record access control point execution status (@see enum plxp_cp_resp_code_id)
     * @param[in] num_of_record Number of record (meaningful for GLP_REQ_REP_NUM_OF_STRD_RECS operation)
     *
     ****************************************************************************************
     */
    void (*cb_racp_rsp_recv)(uint8_t conidx, uint8_t req_op_code, uint8_t racp_status, uint16_t num_of_record);
} plxc_cb_t;

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
 * @param[in] p_plxc        Pointer to peer database description bond data
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t plxc_enable(uint8_t conidx, uint8_t con_type, const plxc_plxp_content_t* p_plx);

/**
 ****************************************************************************************
 * @brief Perform a read sensor features procedure.
 *
 * Wait for @see cb_read_features_cmp execution before starting a new procedure
 *
 * @param[in] conidx        Connection index
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t plxc_read_features(uint8_t conidx);

/**
 ****************************************************************************************
 * @brief Perform a read Characteristic Configuration procedure.
 *
 * Wait for @see cb_read_cfg_cmp execution before starting a new procedure
 *
 * @param[in] conidx        Connection index
 * @param[in] val_id        Value identifier (@see enum plxc_val_id)
 *                               - PLXC_VAL_SPOT_CHECK_MEAS_CFG
 *                               - PLXC_VAL_CONTINUOUS_MEAS_CFG
 *                               - PLXC_VAL_RACP_CFG
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t plxc_read_cfg(uint8_t conidx, uint8_t val_id);

/**
 ****************************************************************************************
 * @brief Configure sensor notification and indication configuration.
 *
 * Wait for @see cb_write_cfg_cmp execution before starting a new procedure
 *
 * @param[in] conidx        Connection index
 * @param[in] val_id        Value identifier (@see enum plxc_val_id)
 *                               - PLXC_VAL_SPOT_CHECK_MEAS_CFG
 *                               - PLXC_VAL_CONTINUOUS_MEAS_CFG
 *                               - PLXC_VAL_RACP_CFG
 * @param[in] ccc           Client Characteristic Configuration value
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t plxc_write_cfg(uint8_t conidx, uint8_t val_id, uint16_t ccc);

/**
 ****************************************************************************************
 * @brief Function called to send a record access control point request
 *
 * Wait for @see cb_racp_req_cmp execution before starting a new procedure
 *
 * @param[in] conidx        Connection index
 * @param[in] req_op_code   Requested Operation Code (@see plxp_cp_operator_id)
 * @param[in] func_operator Function operator (see enum glp_racp_operator)
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t plxc_racp_req(uint8_t conidx, uint8_t req_op_code, uint8_t func_operator);

/// @} PLXC

#endif //(_PLXC_H_)

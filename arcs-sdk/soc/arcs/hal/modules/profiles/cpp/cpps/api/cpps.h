/**
 ****************************************************************************************
 *
 * @file cpps.h
 *
 * @brief Header file - Cycling Power Profile Sensor - Native API
 *
 * Copyright (C) ListenAI 2020-2099
 *
 ****************************************************************************************
 */

#ifndef _CPPS_H_
#define _CPPS_H_

/**
 ****************************************************************************************
 * @addtogroup CPPS
 * @ingroup Profile
 * @brief Cycling Power Profile  - Native API.
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "cpps_msg.h"

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

/// Cycling power service server callback set
typedef struct cpps_cb
{
    /**
     ****************************************************************************************
     * @brief Completion of measurement transmission
     *
     * @param[in] status Status of the procedure execution (@see enum hl_err)
     ****************************************************************************************
     */
    void (*cb_meas_send_cmp)(uint16_t status);

    /**
     ****************************************************************************************
     * @brief Completion of vector transmission
     *
     * @param[in] status Status of the procedure execution (@see enum hl_err)
     ****************************************************************************************
     */
    void (*cb_vector_send_cmp)(uint16_t status);

    /**
     ****************************************************************************************
     * @brief Inform that bond data updated for the connection.
     *
     * @param[in] conidx        Connection index
     * @param[in] char_code     Characteristic Code (CP Measurement or Control Point)
     * @param[in] cfg_val       Stop/notify/indicate value to configure into the peer characteristic
     ****************************************************************************************
     */
    void (*cb_bond_data_upd)(uint8_t conidx, uint8_t char_code, uint16_t cfg_val);

    /**
     ****************************************************************************************
     * @brief Inform that peer device requests an action using control point
     *
     * @note control point request must be answered using @see cpps_ctnl_pt_rsp_send function
     *
     * @param[in] conidx        Connection index
     * @param[in] op_code       Operation Code (@see enum cpp_ctnl_pt_code)
     * @param[in] p_value       Pointer to control point request value
     ****************************************************************************************
     */
    void (*cb_ctnl_pt_req)(uint8_t conidx, uint8_t op_code, const union cpp_ctnl_pt_req_val* p_value);

    /**
     ****************************************************************************************
     * @brief Completion of control point response send procedure
     *
     * @param[in] status Status of the procedure execution (@see enum hl_err)
     ****************************************************************************************
     */
    void (*cb_ctnl_pt_rsp_send_cmp)(uint8_t conidx, uint16_t status);

} cpps_cb_t;

/*
 * NATIVE API FUNCTIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Restore bond data of a known peer device (at connection establishment)
 *
 * @param[in] conidx            Connection index
 * @param[in] ntf_ind_cfg       Profile characteristic configuration:
 *                                   - Bit 0: Measurement Characteristic notification config
 *                                   - Bit 1: Measurement Characteristic broadcast config
 *                                   - Bit 2: Vector Characteristic notification config
 *                                   - Bit 3: Control Point Characteristic indication config
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t cpps_enable(uint8_t conidx, uint8_t ntf_ind_cfg);

/**
 ****************************************************************************************
 * @brief Pack measurement information for advertising
 *
 * @param[in] p_buf   Pointer to output buffer - Buffer tail length must support a value up
 *                    to CPP_CP_MEAS_ADV_MAX_LEN
 * @param[in] p_meas  Pointer of CPP measurement
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t cpps_adv_data_pack(co_buf_t* p_buf, const cpp_cp_meas_t* p_meas);

/**
 ****************************************************************************************
 * @brief Send a cycling power measurement to registered peer devices
 *
 * Wait for @see cb_meas_send_cmp execution before starting a new procedure
 *
 * @param[in] conidx_bf        Bit field of connection index on which measurement will be send
 *                             will be mask according to client that supports measurement reception
 * @param[in] cumul_wheel_rev  Cumulative Wheel Revolutions
 * @param[in] p_meas           Pointer to the cycling power measurement
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t cpps_meas_send(uint32_t conidx_bf, int16_t cumul_wheel_rev, const cpp_cp_meas_t* p_meas);

/**
 ****************************************************************************************
 * @brief Send a cycling power vector to registered peer devices
 *
 * Wait for @see cb_vector_send_cmp execution before starting a new procedure
 *
 * @param[in] conidx_bf      Bit field of connection index on which measurement will be send
 *                           will be mask according to client that supports measurement reception
 * @param[in] p_vector       Pointer to the cycling power vector information
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t cpps_vector_send(uint32_t conidx_bf, const cpp_cp_vector_t* p_vector);


/**
 ****************************************************************************************
 * @brief Send control point response.
 *
 * Wait for @see cb_ctnl_pt_rsp_send_cmp execution before starting a new procedure
 *
 * @param[in] conidx        Connection index
 * @param[in] op_code       Operation Code (@see enum cpp_ctnl_pt_code)
 * @param[in] resp_val      Control point response value (@see enum cpp_ctnl_pt_resp_val)
 * @param[in] p_value       Pointer to control point response value
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t cpps_ctnl_pt_rsp_send(uint8_t conidx, uint8_t op_code, uint8_t resp_val, const union cpp_ctnl_pt_rsp_val* p_value);

/// @} CPPS

#endif //(_CPPS_H_)

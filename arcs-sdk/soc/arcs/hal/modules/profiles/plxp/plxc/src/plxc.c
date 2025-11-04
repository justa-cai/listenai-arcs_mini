/**
 ****************************************************************************************
 *
 * @file plxc.c
 *
 * @brief Pulse Oximeter Profile Collector implementation.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 ****************************************************************************************
 */


/**
 ****************************************************************************************
 * @addtogroup PLXC
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "btip_config.h"

#if (BLE_PLX_CLIENT)

#include "plxc.h"
#include "gap.h"
#include "gatt.h"
#include "ble_prf.h"

#include "co_utils.h"
#include "co_endian.h"
#include "co_time.h"

#include <string.h>
#include "ke_mem.h"


/*
 * DEFINES
 ****************************************************************************************
 */

/// Maximum number of Client task instances
/// Record access control point timer (30s in milliseconds)
#define PLXC_RACP_TIMEOUT   (30000)
/// Invalid descriptor
#define PLXC_DESC_INVALID   (0xFFFF)

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */


/// Environment variable for each Connections
typedef struct plxc_cnx_env
{
    /// Control point timer
    co_time_timer_t     timer;
    /// Pulse Oximeter Service Characteristics
    plxc_plxp_content_t plx;
    /// counter used to check service uniqueness
    uint8_t             nb_svc;
    /// Client is in discovering state
    bool                discover;
    /// Control point operation on-going (@see enum plx_racp_op_code)
    uint8_t             racp_op_code;
} plxc_cnx_env_t;

/// Client environment variable
typedef struct plxc_env
{
    /// profile environment
    prf_hdr_t            prf_env;
    /// Environment variable pointer for each connections
    plxc_cnx_env_t*      p_env[BLE_CONNECTION_MAX];
    /// GATT User local identifier
    uint8_t              user_lid;
} plxc_env_t;


/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */

/// State machine used to retrieve Pulse Oximeter Service characteristics information
const prf_char_def_t plxc_plx_char[PLXC_CHAR_MAX] =
{
    [PLXC_CHAR_SPOT_MEASUREMENT] = { GATT_CHAR_PLX_SPOT_CHECK_MEASUREMENT_LOC, ATT_REQ(PRES, OPT),  (PROP(I))            },
    [PLXC_CHAR_CONT_MEASUREMENT] = { GATT_CHAR_PLX_CONTINUOUS_MEASUREMENT_LOC, ATT_REQ(PRES, OPT),  (PROP(N))            },
    [PLXC_CHAR_FEATURES]         = { GATT_CHAR_PLX_FEATURES_LOC,               ATT_REQ(PRES, MAND), (PROP(RD))           },
    [PLXC_CHAR_RACP]             = { GATT_CHAR_REC_ACCESS_CTRL_PT,             ATT_REQ(PRES, OPT),  (PROP(WR) | PROP(I)) },
};


/// State machine used to retrieve Pulse Oximeter Service characteristic Description information
const prf_desc_def_t plxc_plx_char_desc[PLXC_DESC_MAX] =
{
    /// Client config
    [PLXC_DESC_SPOT_MEASUREMENT_CCC] = { GATT_DESC_CLIENT_CHAR_CFG, ATT_REQ(PRES, OPT), PLXC_CHAR_SPOT_MEASUREMENT },
    /// Client config
    [PLXC_DESC_CONT_MEASUREMENT_CCC] = { GATT_DESC_CLIENT_CHAR_CFG, ATT_REQ(PRES, OPT), PLXC_CHAR_CONT_MEASUREMENT },
    /// Client config
    [PLXC_DESC_RACP_CCC]             = { GATT_DESC_CLIENT_CHAR_CFG, ATT_REQ(PRES, OPT), PLXC_CHAR_RACP             },
};
/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Send discovery results to application.
 *
 * @param p_plxc_env    Client Role task environment
 * @param conidx        Connection index
 * @param status        Response status code
 *****************************************************************************************
 */
__STATIC void plxc_enable_cmp(plxc_env_t* p_plxc_env, uint8_t conidx, uint16_t status)
{
    const plxc_cb_t* p_cb = (const plxc_cb_t*) p_plxc_env->prf_env.p_cb;

    if(p_plxc_env != NULL)
    {
        plxc_cnx_env_t* p_con_env = p_plxc_env->p_env[conidx];
        p_cb->cb_enable_cmp(conidx, status, &(p_con_env->plx));

        if (status != GAP_ERR_NO_ERROR)
        {
            // clean-up environment variable allocated for task instance
            ke_free(p_con_env);
            p_plxc_env->p_env[conidx] = NULL;
        }
        else
        {
             p_con_env->discover = false;

             // Register profile handle to catch gatt indications
             gatt_cli_event_register(conidx, p_plxc_env->user_lid, p_con_env->plx.svc.shdl,
                                     p_con_env->plx.svc.ehdl);
        }
    }
}

/**
 ****************************************************************************************
 * @brief Send read result to application,.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the request
 * @param[in] val_id        Value Identifier (@see enum plxc_val_id)
 * @param[in] p_data        Pointer of data value
 ****************************************************************************************
 */
__STATIC void plxc_read_val_cmp(uint8_t conidx, uint16_t status, uint8_t val_id, co_buf_t* p_data)
{
    plxc_env_t* p_plxc_env = PRF_ENV_GET(PLXC, plxc);

    if(p_plxc_env != NULL)
    {
        const plxc_cb_t* p_cb = (const plxc_cb_t*) p_plxc_env->prf_env.p_cb;
        switch (val_id)
        {
            // Read sensor Feature Characteristic value
            case (PLXC_VAL_FEATURES):
            {
                plxp_features_t features;
                memset(&features, 0, sizeof(plxp_features_t));

                if(status == GAP_ERR_NO_ERROR)
                {
                    features.sup_feat = co_btohs(co_read16p(co_buf_data(p_data)));
                    co_buf_head_release(p_data, 2);

                    if (GETB(features.sup_feat, PLXP_FEAT_MEAS_STATUS_SUP) && (co_buf_data_len(p_data) >= 2))
                    {
                        features.meas_stat_sup = co_btohs(co_read16p(co_buf_data(p_data)));
                        co_buf_head_release(p_data, 2);
                    }

                    if (GETB(features.sup_feat, PLXP_FEAT_DEV_SENSOR_STATUS_SUP) && (co_buf_data_len(p_data) >= 3))
                    {
                        features.dev_stat_sup = co_btoh24(co_read24p(co_buf_data(p_data)));
                    }

                }
                p_cb->cb_read_features_cmp(conidx, status, &features);
            } break;

            // Read Client Characteristic Configuration Descriptor value
            case (PLXC_VAL_SPOT_CHECK_MEAS_CFG):
            case (PLXC_VAL_CONTINUOUS_MEAS_CFG):
            case (PLXC_VAL_RACP_CFG):
            {
                uint16_t cfg_val = 0;

                if(status == GAP_ERR_NO_ERROR)
                {
                    cfg_val = co_btohs(co_read16p(co_buf_data(p_data)));
                }

                p_cb->cb_read_cfg_cmp(conidx, status, val_id, cfg_val);
            } break;

            default:
            {
                ASSERT_ERR(0);
            } break;
        }
    }
}

/**
 ****************************************************************************************
 * @brief Perform Value read procedure.
 *
 * @param[in] conidx        Connection index
 * @param[in] val_id        Value Identifier (@see enum plxc_info)
 ****************************************************************************************
 */
__STATIC uint16_t plxc_read_val(uint8_t conidx, uint16_t val_id)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    plxc_env_t* p_plxc_env = PRF_ENV_GET(PLXC, plxc);

    if(p_plxc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_plxc_env->p_env[conidx] != NULL) && (!p_plxc_env->p_env[conidx]->discover))
        {
            plxc_cnx_env_t* p_con_env = p_plxc_env->p_env[conidx];
            uint16_t hdl;
            plxc_plxp_content_t* p_plx = &(p_con_env->plx);

            switch(val_id)
            {
                case PLXC_VAL_FEATURES:            { hdl = p_plx->chars[PLXC_CHAR_FEATURES].val_hdl;              } break;
                case PLXC_VAL_SPOT_CHECK_MEAS_CFG: { hdl = p_plx->descs[PLXC_DESC_SPOT_MEASUREMENT_CCC].desc_hdl; } break;
                case PLXC_VAL_CONTINUOUS_MEAS_CFG: { hdl = p_plx->descs[PLXC_DESC_CONT_MEASUREMENT_CCC].desc_hdl; } break;
                case PLXC_VAL_RACP_CFG:            { hdl = p_plx->descs[PLXC_DESC_RACP_CCC].desc_hdl;             } break;
                default:                           { hdl = GATT_INVALID_HDL;                                      } break;
            }

            if(hdl == GATT_INVALID_HDL)
            {
                status = PRF_ERR_INEXISTENT_HDL;
            }
            else
            {
                // perform read request
                status = gatt_cli_read(conidx, p_plxc_env->user_lid, val_id, hdl, 0, 0);
            }
        }
    }

    return (status);
}


/**
 ****************************************************************************************
 * @brief Packs Record access Control Point data
 *
 * @param[in] p_buf         Pointer to output buffer
 * @param[in] req_op_code   Requested Operation Code (@see plx_racp_op_code)
 * @param[in] func_operator Function operator (see enum plx_racp_operator)
 * @param[in] filter_type   Filter type (@see enum plx_racp_filter)
 * @param[in] p_filter      Pointer to filter information
 *
 * @return Function execution status
 ****************************************************************************************
 */
__STATIC uint16_t plxc_pack_racp_req(co_buf_t* p_buf, uint8_t op_code, uint8_t func_operator)
{
    uint16_t status = GAP_ERR_NO_ERROR;

    // command op code
    co_buf_tail(p_buf)[0] = op_code;
    co_buf_tail_reserve(p_buf, 1);

    // operator of the function
    co_buf_tail(p_buf)[0] = func_operator;
    co_buf_tail_reserve(p_buf, 1);

    return (status);
}


/**
 ****************************************************************************************
 * @brief Unpacks Spot-check measurement data and sends the indication
 * @param[in] p_plxc_env    Environment variable
 * @param[in] conidx        Connection index
 * @param[in] p_buf         Pointer of input buffer received
 ****************************************************************************************
 */
__STATIC void plxc_unpack_spot_meas(plxc_env_t* p_plxc_env, uint8_t conidx, co_buf_t* p_buf)
{
    const plxc_cb_t* p_cb = (const plxc_cb_t*) p_plxc_env->prf_env.p_cb;
    plxp_spot_meas_t spot_meas;
    memset(&spot_meas, 0, sizeof(plxp_spot_meas_t));

    do
    {
        if(co_buf_data_len(p_buf) < 1) break;

        // Flags
        spot_meas.spot_flags = co_buf_data(p_buf)[0];
        co_buf_head_release(p_buf, 1);

        if(co_buf_data_len(p_buf) < 4) break;

        // Percentage with a resolution of 1
        spot_meas.sp_o2 = co_btohs(co_read16p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 2);
        // Period
        spot_meas.pr = co_btohs(co_read16p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 2);

        // Timestamp (if present)
        if (GETB(spot_meas.spot_flags, PLXP_SPOT_MEAS_FLAGS_TIMESTAMP))
        {
            if(co_buf_data_len(p_buf) < 7) break;
            prf_unpack_date_time(p_buf, &(spot_meas.timestamp));
        }

        // Measurement Status (bitfield) @see common measurement_status_supported
        if (GETB(spot_meas.spot_flags, PLXP_SPOT_MEAS_FLAGS_MEAS_STATUS))
        {
            if(co_buf_data_len(p_buf) < 2) break;
            spot_meas.meas_stat = co_btohs(co_read16p(co_buf_data(p_buf)));
            co_buf_head_release(p_buf, 2);
        }

        // Device and Sensor Status (bitfield) @see common device_status_supported
        if (GETB(spot_meas.spot_flags, PLXP_SPOT_MEAS_FLAGS_DEV_SENSOR_STATUS))
        {
            if(co_buf_data_len(p_buf) < 3) break;
            spot_meas.dev_sensor_stat = co_btoh24(co_read24p(co_buf_data(p_buf)));
            co_buf_head_release(p_buf, 3);
        }

        // Pulse Amplitude Index - Unit is percentage with a resolution of 1
        if (GETB(spot_meas.spot_flags, PLXP_SPOT_MEAS_FLAGS_PULSE_AMPLITUDE))
        {
            if(co_buf_data_len(p_buf) < 2) break;
            spot_meas.pulse_ampl = co_btohs(co_read16p(co_buf_data(p_buf)));
            co_buf_head_release(p_buf, 2);
        }
    } while(0);

    // Inform application about received measurement
    p_cb->cb_spot_meas(conidx, &spot_meas);
}

/**
 ****************************************************************************************
 * @brief Unpacks continous measurement data and sends the indication
 * @param[in] p_plxc_env    Environment variable
 * @param[in] conidx        Connection index
 * @param[in] p_buf         Pointer of input buffer received
 ****************************************************************************************
 */
__STATIC void plxc_unpack_cont_meas(plxc_env_t* p_plxc_env, uint8_t conidx, co_buf_t* p_buf)
{
    const plxc_cb_t* p_cb = (const plxc_cb_t*) p_plxc_env->prf_env.p_cb;
    plxp_cont_meas_t cont_meas;
    memset(&cont_meas, 0, sizeof(plxp_cont_meas_t));

    do
    {
        if(co_buf_data_len(p_buf) < 1) break;

        // Flags
        cont_meas.cont_flags = co_buf_data(p_buf)[0];
        co_buf_head_release(p_buf, 1);

        if(co_buf_data_len(p_buf) < 4) break;

        // SpO2 - PR measurements - Normal
        cont_meas.normal.sp_o2 = co_btohs(co_read16p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 2);
        cont_meas.normal.pr = co_btohs(co_read16p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 2);

        // SpO2 - PR measurements - Fast
        if (GETB(cont_meas.cont_flags, PLXP_CONT_MEAS_FLAGS_SPO2PR_FAST))
        {
            if(co_buf_data_len(p_buf) < 4) break;
            cont_meas.fast.sp_o2 = co_btohs(co_read16p(co_buf_data(p_buf)));
            co_buf_head_release(p_buf, 2);
            cont_meas.fast.pr = co_btohs(co_read16p(co_buf_data(p_buf)));
            co_buf_head_release(p_buf, 2);
        }
        // SpO2 - PR measurements - Slow
        if (GETB(cont_meas.cont_flags, PLXP_CONT_MEAS_FLAGS_SPO2PR_SLOW))
        {
            if(co_buf_data_len(p_buf) < 4) break;
            cont_meas.slow.sp_o2 = co_btohs(co_read16p(co_buf_data(p_buf)));
            co_buf_head_release(p_buf, 2);
            cont_meas.slow.pr = co_btohs(co_read16p(co_buf_data(p_buf)));
            co_buf_head_release(p_buf, 2);
        }
        // Measurement Status (bitfield) @see common measurement_status_supported
        if (GETB(cont_meas.cont_flags, PLXP_CONT_MEAS_FLAGS_MEAS_STATUS))
        {
            if(co_buf_data_len(p_buf) < 2) break;
            cont_meas.meas_stat = co_btohs(co_read16p(co_buf_data(p_buf)));
            co_buf_head_release(p_buf, 2);
        }

        // Device and Sensor Status (bitfield) @see common device_status_supported
        if (GETB(cont_meas.cont_flags, PLXP_CONT_MEAS_FLAGS_DEV_SENSOR_STATUS))
        {
            if(co_buf_data_len(p_buf) < 3) break;
            cont_meas.dev_sensor_stat = co_btoh24(co_read24p(co_buf_data(p_buf)));
            co_buf_head_release(p_buf, 3);
        }

        // Pulse Amplitude Index - Unit is percentage with a resolution of 1
        if (GETB(cont_meas.cont_flags, PLXP_CONT_MEAS_FLAGS_PULSE_AMPL))
        {
            if(co_buf_data_len(p_buf) < 2) break;
            cont_meas.pulse_ampl = co_btohs(co_read16p(co_buf_data(p_buf)));
            co_buf_head_release(p_buf, 2);
        }
    } while(0);

    // Inform application about received measurement
    p_cb->cb_cont_meas(conidx, &cont_meas);
}

/**
 ****************************************************************************************
 * @brief Unpacks Control Point data and sends the indication
 * @param[in] p_plxc_env    Environment variable
 * @param[in] conidx        Connection index
 * @param[in] p_buf         Pointer of input buffer received
 ****************************************************************************************
 */
__STATIC void plxc_unpack_racp_rsp(plxc_env_t* p_plxc_env, uint8_t conidx, co_buf_t* p_buf)
{
    bool valid = (co_buf_data_len(p_buf) >= 4);

    uint8_t op_code;
    uint8_t req_op_code;
    uint8_t racp_status;
    uint16_t num_of_record = 0;

    // Response Op code
    op_code = co_buf_data(p_buf)[0];
    co_buf_head_release(p_buf, 1);

    // Operator value (can be ignored)
    co_buf_head_release(p_buf, 1);

    if(op_code == PLXP_OPCODE_RESPONSE_CODE)
    {
        // Requested operation code
        req_op_code = co_buf_data(p_buf)[0];
        co_buf_head_release(p_buf, 1);

        // RACP Status value
        racp_status = co_buf_data(p_buf)[0];
        co_buf_head_release(p_buf, 1);
    }
    else if(op_code == PLXP_OPCODE_NUMBER_OF_STORED_RECORDS_RESP)
    {
        req_op_code = PLXP_OPCODE_REPORT_NUMBER_OF_STORED_RECORDS;
        racp_status = PLXP_RESP_SUCCESS;

        num_of_record = co_btohs(co_read16p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 2);
    }
    else
    {
        valid = false;
    }

    if(valid && ((req_op_code == p_plxc_env->p_env[conidx]->racp_op_code) || (req_op_code == PLXP_OPCODE_ABORT_OPERATION)))
    {
        const plxc_cb_t* p_cb = (const plxc_cb_t*) p_plxc_env->prf_env.p_cb;

        if(req_op_code != PLXP_OPCODE_ABORT_OPERATION)
        {
            p_plxc_env->p_env[conidx]->racp_op_code = PLXP_OPCODE_RESERVED;
        }

        // stop timer
        co_time_timer_stop(&(p_plxc_env->p_env[conidx]->timer));

        // provide control point response
        p_cb->cb_racp_rsp_recv(conidx, req_op_code, racp_status, num_of_record);
    }
}

/**
 ****************************************************************************************
 * @brief Function to called once timer expires
 *
 * @param[in] conidx Connection index
 ****************************************************************************************
 */
__STATIC void plxc_timer_handler(uint32_t conidx)
{
    // Get the address of the environment
    plxc_env_t *p_plxc_env = PRF_ENV_GET(PLXC, plxc);

    if (p_plxc_env != NULL)
    {
        plxc_cnx_env_t* p_con_env = p_plxc_env->p_env[conidx];
        ASSERT_ERR(p_con_env != NULL);
        if(p_con_env->racp_op_code != PLXP_OPCODE_RESERVED)
        {
            const plxc_cb_t* p_cb = (const plxc_cb_t*) p_plxc_env->prf_env.p_cb;
            uint8_t op_code = p_con_env->racp_op_code;
            p_con_env->racp_op_code = PLXP_OPCODE_RESERVED;

            p_cb->cb_racp_req_cmp((uint8_t)conidx, PRF_ERR_PROC_TIMEOUT, op_code);
        }
    }
}

/*
 * GATT USER CLIENT HANDLERS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief This function is called when a full service has been found during a discovery procedure.
 *
 * @param[in] conidx        Connection index
 * @param[in] user_lid      GATT user local identifier
 * @param[in] dummy         Dummy parameter provided by upper layer for command execution
 * @param[in] hdl           First handle value of following list
 * @param[in] disc_info     Discovery information (@see enum gatt_svc_disc_info)
 * @param[in] nb_att        Number of attributes
 * @param[in] p_atts        Pointer to attribute information present in a service
 ****************************************************************************************
 */
__STATIC void plxc_svc_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t hdl, uint8_t disc_info,
                          uint8_t nb_att, const gatt_svc_att_t* p_atts)
{
    // Get the address of the environment
    plxc_env_t* p_plxc_env = PRF_ENV_GET(PLXC, plxc);

    if(p_plxc_env != NULL)
    {
        plxc_cnx_env_t* p_con_env = p_plxc_env->p_env[conidx];

        ASSERT_INFO(p_con_env != NULL, conidx, user_lid);

        if (p_con_env->nb_svc == 0)
        {
            //Even if we get multiple responses we only store 1 range
            if((disc_info == GATT_SVC_CMPLT) || (disc_info == GATT_SVC_START))
            {
                p_con_env->plx.svc.shdl = hdl;
            }

            if((disc_info == GATT_SVC_CMPLT) || (disc_info == GATT_SVC_END))
            {
                p_con_env->plx.svc.ehdl = hdl + nb_att -1;
            }

            // Retrieve characteristics
            prf_extract_svc_info(hdl, nb_att, p_atts,
                                 PLXC_CHAR_MAX, &plxc_plx_char[0],      &(p_con_env->plx.chars[0]),
                                 PLXC_DESC_MAX, &plxc_plx_char_desc[0], &(p_con_env->plx.descs[0]));
        }

        if((disc_info == GATT_SVC_CMPLT) || (disc_info == GATT_SVC_END))
        {
            p_con_env->nb_svc++;
        }
    }
}

/**
 ****************************************************************************************
 * @brief This function is called when GATT client user discovery procedure is over.
 *
 * @param[in] conidx        Connection index
 * @param[in] user_lid      GATT user local identifier
 * @param[in] dummy         Dummy parameter provided by upper layer for command execution
 * @param[in] status        Status of the procedure (@see enum hl_err)
 ****************************************************************************************
 */
__STATIC void plxc_discover_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    // Get the address of the environment
    plxc_env_t* p_plxc_env = PRF_ENV_GET(PLXC, plxc);

    if(p_plxc_env != NULL)
    {
        plxc_cnx_env_t* p_con_env = p_plxc_env->p_env[conidx];

        if (p_con_env->nb_svc ==  1)
        {
            status = prf_check_svc_validity(PLXC_CHAR_MAX, p_con_env->plx.chars, plxc_plx_char,
                                            PLXC_DESC_MAX, p_con_env->plx.descs, plxc_plx_char_desc);
        }
        // too much services
        else if (p_con_env->nb_svc > 1)
        {
            status = PRF_ERR_MULTIPLE_SVC;
        }
        // no services found
        else
        {
            status = PRF_ERR_STOP_DISC_CHAR_MISSING;
        }

        plxc_enable_cmp(p_plxc_env, conidx, status);
    }
}

/**
 ****************************************************************************************
 * @brief This function is called during a read procedure when attribute value is retrieved
 *        form peer device.
 *
 * @param[in] conidx        Connection index
 * @param[in] user_lid      GATT user local identifier
 * @param[in] dummy         Dummy parameter provided by upper layer for command execution
 * @param[in] hdl           Attribute handle
 * @param[in] offset        Data offset
 * @param[in] p_data        Pointer to buffer that contains attribute value starting from offset
 ****************************************************************************************
 */
__STATIC void plxc_att_val_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t hdl, uint16_t offset,
                              co_buf_t* p_data)
{
    plxc_read_val_cmp(conidx, GAP_ERR_NO_ERROR, (uint8_t) dummy, p_data);
}


/**
 ****************************************************************************************
 * @brief This function is called when GATT client user read procedure is over.
 *
 * @param[in] conidx        Connection index
 * @param[in] user_lid      GATT user local identifier
 * @param[in] dummy         Dummy parameter provided by upper layer for command execution
 * @param[in] status        Status of the procedure (@see enum hl_err)
 ****************************************************************************************
 */
__STATIC void plxc_read_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    if(status != GAP_ERR_NO_ERROR)
    {
        plxc_read_val_cmp(conidx, status, (uint8_t) dummy, NULL);
    }
}

/**
 ****************************************************************************************
 * @brief This function is called when GATT client user write procedure is over.
 *
 * @param[in] conidx        Connection index
 * @param[in] user_lid      GATT user local identifier
 * @param[in] dummy         Dummy parameter provided by upper layer for command execution
 * @param[in] status        Status of the procedure (@see enum hl_err)
 ****************************************************************************************
 */
__STATIC void plxc_write_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    plxc_env_t* p_plxc_env = PRF_ENV_GET(PLXC, plxc);

    if(p_plxc_env != NULL)
    {
        const plxc_cb_t* p_cb = (const plxc_cb_t*) p_plxc_env->prf_env.p_cb;

        switch(dummy)
        {
            // Config control
            case PLXC_VAL_SPOT_CHECK_MEAS_CFG:
            case PLXC_VAL_CONTINUOUS_MEAS_CFG:
            case PLXC_VAL_RACP_CFG:            { p_cb->cb_write_cfg_cmp(conidx, status, (uint8_t) dummy); } break;
            // RACP
            case PLXC_VAL_RACP_RSP:
            {
                uint8_t opcode = p_plxc_env->p_env[conidx]->racp_op_code;
                p_cb->cb_racp_req_cmp(conidx, status, opcode);

                if(status == GAP_ERR_NO_ERROR)
                {
                    // Start Timeout Procedure - wait for Indication reception
                    co_time_timer_set(&(p_plxc_env->p_env[conidx]->timer), PLXC_RACP_TIMEOUT);
                }
                else
                {
                    p_plxc_env->p_env[conidx]->racp_op_code = PLXP_OPCODE_RESERVED;
                }
            } break;
            default: { /* Nothing to do */ } break;
        }
    }
}


/**
 ****************************************************************************************
 * @brief This function is called when a notification or an indication is received onto
 *        register handle range (@see gatt_cli_event_register).
 *
 *        @see gatt_cli_val_event_cfm must be called to confirm event reception.
 *
 * @param[in] conidx        Connection index
 * @param[in] user_lid      GATT user local identifier
 * @param[in] token         Procedure token that must be returned in confirmation function
 * @param[in] evt_type      Event type triggered (@see enum gatt_evt_type)
 * @param[in] complete      True if event value if complete value has been received
 *                          False if data received is equals to maximum attribute protocol value.
 *                          In such case GATT Client User should perform a read procedure.
 * @param[in] hdl           Attribute handle
 * @param[in] p_data        Pointer to buffer that contains attribute value
 ****************************************************************************************
 */
__STATIC void plxc_att_val_evt_cb(uint8_t conidx, uint8_t user_lid, uint16_t token, uint8_t evt_type, bool complete,
                        uint16_t hdl, co_buf_t* p_data)
{
    plxc_env_t* p_plxc_env = PRF_ENV_GET(PLXC, plxc);

    if(p_plxc_env != NULL)
    {
        plxc_cnx_env_t* p_con_env = p_plxc_env->p_env[conidx];
        plxc_plxp_content_t* p_plx = &(p_con_env->plx);

        if (hdl == p_plx->chars[PLXC_CHAR_SPOT_MEASUREMENT].val_hdl)
        {
            //Unpack measurement
            plxc_unpack_spot_meas(p_plxc_env, conidx, p_data);
        }
        else if (hdl == p_plx->chars[PLXC_CHAR_CONT_MEASUREMENT].val_hdl)
        {
            //Unpack measurement
            plxc_unpack_cont_meas(p_plxc_env, conidx, p_data);

            if(p_con_env->racp_op_code != PLXP_OPCODE_RESERVED)
            {
                co_time_timer_set(&(p_plxc_env->p_env[conidx]->timer), PLXC_RACP_TIMEOUT);
            }
        }
        else if (hdl == p_plx->chars[PLXC_CHAR_RACP].val_hdl)
        {
            // Unpack control point
            plxc_unpack_racp_rsp(p_plxc_env, conidx, p_data);
        }
    }

    // confirm event handling
    gatt_cli_att_event_cfm(conidx, user_lid, token);
}

/**
 ****************************************************************************************
 * @brief Event triggered when a service change has been received or if an attribute
 *        transaction triggers an out of sync error.
 *
 * @param[in] conidx        Connection index
 * @param[in] user_lid      GATT user local identifier
 * @param[in] out_of_sync   True if an out of sync error has been received
 * @param[in] start_hdl     Service start handle
 * @param[in] end_hdl       Service end handle
 ****************************************************************************************
 */
__STATIC void plxc_svc_changed_cb(uint8_t conidx, uint8_t user_lid, bool out_of_sync, uint16_t start_hdl, uint16_t end_hdl)
{
    // Do Nothing
}

/// Client callback hander
__STATIC const gatt_cli_cb_t plxc_cb =
{
    .cb_discover_cmp    = plxc_discover_cmp_cb,
    .cb_read_cmp        = plxc_read_cmp_cb,
    .cb_write_cmp       = plxc_write_cmp_cb,
    .cb_att_val_get     = NULL,
    .cb_svc             = plxc_svc_cb,
    .cb_svc_info        = NULL,
    .cb_inc_svc         = NULL,
    .cb_char            = NULL,
    .cb_desc            = NULL,
    .cb_att_val         = plxc_att_val_cb,
    .cb_att_val_evt     = plxc_att_val_evt_cb,
    .cb_svc_changed     = plxc_svc_changed_cb,
};

/*
 * PROFILE NATIVE API
 ****************************************************************************************
 */

uint16_t plxc_enable(uint8_t conidx, uint8_t con_type, const plxc_plxp_content_t* p_plx)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    plxc_env_t* p_plxc_env = PRF_ENV_GET(PLXC, plxc);

    if(p_plxc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_plxc_env->p_env[conidx] == NULL))
        {
            // allocate environment variable for task instance
            p_plxc_env->p_env[conidx] = (struct plxc_cnx_env *) ke_malloc(sizeof(struct plxc_cnx_env), KE_MEM_ATT_DB);

            if(p_plxc_env->p_env[conidx] != NULL)
            {
                memset(p_plxc_env->p_env[conidx], 0, sizeof(struct plxc_cnx_env));
                co_time_timer_init(&(p_plxc_env->p_env[conidx]->timer), (co_time_timer_cb)plxc_timer_handler,
                                   (uint8_t*) ((uint32_t) conidx));

                // Config connection, start discovering
                if (con_type == PRF_CON_DISCOVERY)
                {
                    uint16_t gatt_svc_uuid = GATT_SVC_PULSE_OXIMETER;

                    // start discovery
                    status = gatt_cli_discover_svc(conidx, p_plxc_env->user_lid, 0, GATT_DISCOVER_SVC_PRIMARY_BY_UUID, true,
                                                   GATT_MIN_HDL, GATT_MAX_HDL, GATT_UUID_16, (uint8_t*) &gatt_svc_uuid);

                    // Go to DISCOVERING state
                    p_plxc_env->p_env[conidx]->discover     = true;
                    p_plxc_env->p_env[conidx]->racp_op_code = PLXP_OPCODE_RESERVED;
                }
                // normal connection, get saved att details
                else
                {
                    memcpy(&(p_plxc_env->p_env[conidx]->plx), p_plx, sizeof(plxc_plxp_content_t));
                    status = GAP_ERR_NO_ERROR;

                    // send APP confirmation that can start normal connection to TH
                    plxc_enable_cmp(p_plxc_env, conidx, GAP_ERR_NO_ERROR);
                }
            }
            else
            {
                status = GAP_ERR_INSUFF_RESOURCES;
            }
        }
    }

    return (status);
}

uint16_t plxc_read_features(uint8_t conidx)
{
    uint16_t status = plxc_read_val(conidx, PLXC_VAL_FEATURES);
    return (status);
}

uint16_t plxc_read_cfg(uint8_t conidx, uint8_t val_id)
{
    uint16_t status;

    switch(val_id)
    {
        case PLXC_VAL_SPOT_CHECK_MEAS_CFG:
        case PLXC_VAL_CONTINUOUS_MEAS_CFG:
        case PLXC_VAL_RACP_CFG    :        { status = plxc_read_val(conidx, val_id);  } break;
        default:                           { status = PRF_ERR_INEXISTENT_HDL;             } break;
    }

    return (status);
}

uint16_t plxc_write_cfg(uint8_t conidx, uint8_t val_id, uint16_t ccc)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    plxc_env_t* p_plxc_env = PRF_ENV_GET(PLXC, plxc);

    if(p_plxc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_plxc_env->p_env[conidx] != NULL) && (!p_plxc_env->p_env[conidx]->discover))
        {
            plxc_cnx_env_t* p_con_env = p_plxc_env->p_env[conidx];
            uint16_t hdl;
            uint16_t cfg_en_val = 0;
            plxc_plxp_content_t* p_plx = &(p_con_env->plx);

            switch(val_id)
            {
                case PLXC_VAL_SPOT_CHECK_MEAS_CFG: { hdl        = p_plx->descs[PLXC_DESC_SPOT_MEASUREMENT_CCC].desc_hdl;
                                                     cfg_en_val =  PRF_CLI_START_IND;                                     } break;
                case PLXC_VAL_CONTINUOUS_MEAS_CFG: { hdl        = p_plx->descs[PLXC_DESC_CONT_MEASUREMENT_CCC].desc_hdl;
                                                     cfg_en_val =  PRF_CLI_START_NTF;                                     } break;
                case PLXC_VAL_RACP_CFG:            { hdl        = p_plx->descs[PLXC_DESC_RACP_CCC].desc_hdl;
                                                     cfg_en_val =  PRF_CLI_START_IND;                                     } break;
                default:                           { hdl = GATT_INVALID_HDL;                                              } break;
            }

            if(hdl == GATT_INVALID_HDL)
            {
                status = PRF_ERR_INEXISTENT_HDL;
            }
            else if((ccc != PRF_CLI_STOP_NTFIND) && (ccc != cfg_en_val))
            {
                status = PRF_ERR_INVALID_PARAM;
            }
            else
            {
                // Force endianess
                ccc = co_htobs(ccc);
                status = prf_gatt_write(conidx, p_plxc_env->user_lid, val_id, GATT_WRITE,
                                        hdl, sizeof(uint16_t), (uint8_t *)&ccc);
            }
        }
    }

    return (status);
}

uint16_t plxc_racp_req(uint8_t conidx, uint8_t req_op_code, uint8_t func_operator)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    plxc_env_t* p_plxc_env = PRF_ENV_GET(PLXC, plxc);

    if(p_plxc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_plxc_env->p_env[conidx] != NULL) && (!p_plxc_env->p_env[conidx]->discover))
        {
            plxc_cnx_env_t* p_con_env = p_plxc_env->p_env[conidx];
            plxc_plxp_content_t* p_plx = &(p_con_env->plx);
            uint16_t hdl = p_plx->chars[PLXC_CHAR_RACP].val_hdl;

            if(hdl == GATT_INVALID_HDL)
            {
                status = PRF_ERR_INEXISTENT_HDL;
            }
            // reject if there is an ongoing record access control point operation
            else if((p_con_env->racp_op_code != PLXP_OPCODE_RESERVED) && (req_op_code != PLXP_OPCODE_ABORT_OPERATION))
            {
                status = PRF_PROC_IN_PROGRESS;
            }
            else
            {
                co_buf_t* p_buf = NULL;

                // allocate buffer for event transmission
                if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, 0, PLXP_RACP_SIZE_MAX + GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
                {
                    status = plxc_pack_racp_req(p_buf, req_op_code, func_operator);

                    if(status == GAP_ERR_NO_ERROR)
                    {
                        status = gatt_cli_write(conidx, p_plxc_env->user_lid, PLXC_VAL_RACP_RSP, GATT_WRITE, hdl, 0, p_buf);
                        if((status == GAP_ERR_NO_ERROR) && (req_op_code != PLXP_OPCODE_ABORT_OPERATION))
                        {
                            // save on-going operation
                            p_con_env->racp_op_code = req_op_code;
                        }
                    }
                    co_buf_release(p_buf);
                }
                else
                {
                    status = GAP_ERR_INSUFF_RESOURCES;
                }

            }
        }
    }

    return (status);
}


#if (BLE_HL_MSG_API)
/*
 * PROFILE MSG HANDLERS
 ****************************************************************************************
 */


/**
 ****************************************************************************************
 * @brief Send a PLXC_CMP_EVT message to the task which enabled the profile
 * @param[in] conidx Connection index
 * @param[in] operation Operation
 * @param[in] status Satus
 ****************************************************************************************
 */
__STATIC void plxc_send_cmp_evt(uint8_t conidx, uint8_t operation, uint16_t status)
{
    struct plxc_cmp_evt *p_evt;

    // Send the message
    p_evt = KE_MSG_ALLOC(PLXC_CMP_EVT, PRF_DST_TASK(PLXC), PRF_SRC_TASK(PLXC), plxc_cmp_evt);
    if(p_evt)
    {
        p_evt->conidx     = conidx;
        p_evt->operation  = operation;
        p_evt->status     = status;

        ke_msg_send(p_evt);
    }
}

/**
 ****************************************************************************************
 * @brief  Message handler example
 *
 * @param[in] msgid Id of the message received (probably unused).
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int plxc_enable_req_handler(ke_msg_id_t const msgid, struct plxc_enable_req const *p_param,
                                     ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = plxc_enable(p_param->conidx, p_param->con_type, &(p_param->plx));

    // send an error if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct plxc_enable_rsp *p_rsp = KE_MSG_ALLOC(PLXC_ENABLE_RSP, src_id, dest_id, plxc_enable_rsp);
        if(p_rsp != NULL)
        {
            p_rsp->conidx = p_param->conidx;
            p_rsp->status = status;
            ke_msg_send(p_rsp);
        }
    }

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief  Message Handler example
 * @param[in] msgid Id of the message received (probably unused).
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int plxc_read_cmd_handler(ke_msg_id_t const msgid, struct plxc_read_cmd const *p_param,
                                   ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status;

    switch(p_param->val_id)
    {
        case PLXC_VAL_FEATURES:             { status = plxc_read_features(p_param->conidx);             } break;
        case PLXC_VAL_SPOT_CHECK_MEAS_CFG:
        case PLXC_VAL_CONTINUOUS_MEAS_CFG:
        case PLXC_VAL_RACP_CFG:             { status = plxc_read_cfg(p_param->conidx, p_param->val_id); } break;
        default:                            { status = PRF_ERR_INEXISTENT_HDL;                          } break;
    }

    // send error response if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct plxc_cmp_evt *p_evt = KE_MSG_ALLOC(PLXC_CMP_EVT, src_id, dest_id, plxc_cmp_evt);
        if(p_evt)
        {
            p_evt->conidx     = p_param->conidx;
            p_evt->operation  = PLXC_OP_CODE_READ;
            p_evt->status     = status;

            ke_msg_send(p_evt);
        }
    }

    return (KE_MSG_CONSUMED);
}


/**
 ****************************************************************************************
 * @brief Handles reception of the @see PLXC_WRITE_RACP_CMD message from the application.
 * @brief To write command to the Records Access Control Point in the peer server.
 * @param[in] msgid Id of the message received (probably unused).
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int plxc_write_racp_cmd_handler(ke_msg_id_t const msgid, struct plxc_write_racp_cmd *p_param,
                                         ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = plxc_racp_req(p_param->conidx, p_param->cp_opcode, p_param->cp_operator);

    // send error response if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct plxc_cmp_evt *p_evt = KE_MSG_ALLOC(PLXC_CMP_EVT, src_id, dest_id, plxc_cmp_evt);
        if(p_evt)
        {
            p_evt->conidx     = p_param->conidx;
            p_evt->operation  = PLXC_OP_CODE_WRITE_RACP;
            p_evt->status     = status;

            ke_msg_send(p_evt);
        }
    }

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief Handles reception of the @see PLXC_CFG_CCC_CMD message.
 * Allows the application to write new CCC values to a Alert Characteristic in the peer server
 * @param[in] msgid Id of the message received.
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int plxc_cfg_ccc_cmd_handler(ke_msg_id_t const msgid, struct plxc_cfg_ccc_cmd *p_param,
                                      ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status;

    switch(p_param->val_id)
    {
        case PLXC_VAL_SPOT_CHECK_MEAS_CFG:
        case PLXC_VAL_CONTINUOUS_MEAS_CFG:
        case PLXC_VAL_RACP_CFG:            { status = plxc_write_cfg(p_param->conidx, p_param->val_id, p_param->ccc); } break;
        default:                           { status = PRF_ERR_INVALID_PARAM;                                          } break;
    }

    // send error response if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct plxc_cmp_evt *p_evt = KE_MSG_ALLOC(PLXC_CMP_EVT, src_id, dest_id, plxc_cmp_evt);
        if(p_evt)
        {
            p_evt->conidx     = p_param->conidx;
            p_evt->operation  = PLXC_OP_CODE_WRITE_CCC;
            p_evt->status     = status;

            ke_msg_send(p_evt);
        }
    }

    return (KE_MSG_CONSUMED);
}

/// Default State handlers definition
KE_MSG_HANDLER_TAB(plxc)
{
    // Note: all messages must be sorted in ID ascending order

    { PLXC_ENABLE_REQ,               (ke_msg_func_t) plxc_enable_req_handler         },
    { PLXC_READ_CMD,                 (ke_msg_func_t) plxc_read_cmd_handler           },
    { PLXC_CFG_CCC_CMD,              (ke_msg_func_t) plxc_cfg_ccc_cmd_handler        },
    { PLXC_WRITE_RACP_CMD,           (ke_msg_func_t) plxc_write_racp_cmd_handler     },
};


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
__STATIC void plxc_cb_enable_cmp(uint8_t conidx, uint16_t status, const plxc_plxp_content_t* p_plx)
{
    // Send APP the details of the discovered attributes on PLXC
    struct plxc_enable_rsp *p_rsp = KE_MSG_ALLOC(PLXC_ENABLE_RSP, PRF_DST_TASK(PLXC), PRF_SRC_TASK(PLXC),
                                                 plxc_enable_rsp);
    if(p_rsp != NULL)
    {
        p_rsp->conidx = conidx;
        p_rsp->status = status;
        memcpy(&(p_rsp->plx), p_plx, sizeof(plxc_plxp_content_t));
        ke_msg_send(p_rsp);
    }

}

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
__STATIC void plxc_cb_read_features_cmp(uint8_t conidx, uint16_t status, const plxp_features_t* p_features)
{
    ke_task_id_t src_id  = PRF_SRC_TASK(PLXC);
    ke_task_id_t dest_id = PRF_DST_TASK(PLXC);

    if(status == GAP_ERR_NO_ERROR)
    {
        struct plxc_value_ind *p_ind = KE_MSG_ALLOC(PLXC_VALUE_IND, dest_id, src_id, plxc_value_ind);
        if(p_ind != NULL)
        {
            p_ind->conidx         = conidx;
            p_ind->val_id      = PLXC_VAL_FEATURES;
            memcpy(&(p_ind->value.features), p_features, sizeof(plxp_features_t));;
            ke_msg_send(p_ind);
        }
    }

    plxc_send_cmp_evt(conidx, PLXC_OP_CODE_READ, status);
}

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
__STATIC void plxc_cb_read_cfg_cmp(uint8_t conidx, uint16_t status, uint8_t val_id, uint16_t cfg_val)
{
    ke_task_id_t src_id  = PRF_SRC_TASK(PLXC);
    ke_task_id_t dest_id = PRF_DST_TASK(PLXC);

    if(status == GAP_ERR_NO_ERROR)
    {
        struct plxc_rd_char_ccc_ind *p_ind = KE_MSG_ALLOC(PLXC_RD_CHAR_CCC_IND, dest_id, src_id, plxc_rd_char_ccc_ind);
        if(p_ind != NULL)
        {
            p_ind->conidx  = conidx;
            p_ind->val_id  = val_id;
            p_ind->ind_cfg = cfg_val;
            ke_msg_send(p_ind);
        }
    }

    plxc_send_cmp_evt(conidx, PLXC_OP_CODE_READ, status);
}

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
__STATIC void plxc_cb_write_cfg_cmp(uint8_t conidx, uint16_t status, uint8_t val_id)
{
    plxc_send_cmp_evt(conidx, PLXC_OP_CODE_WRITE_CCC, status);
}

/**
 ****************************************************************************************
 * @brief Function called when Spot-Check measurement information is received
 *
 * @param[in] conidx         Connection index
 * @param[in] p_spot_meas    Pointer to Spot-Check measurement information
 *
 ****************************************************************************************
 */
__STATIC void plxc_cb_spot_meas(uint8_t conidx, const plxp_spot_meas_t* p_spot_meas)
{
    ke_task_id_t src_id  = PRF_SRC_TASK(PLXC);
    ke_task_id_t dest_id = PRF_DST_TASK(PLXC);

    struct plxc_value_ind *p_ind = KE_MSG_ALLOC(PLXC_VALUE_IND, dest_id, src_id, plxc_value_ind);
    if(p_ind != NULL)
    {
        p_ind->conidx      = conidx;
        p_ind->val_id      = PLXC_VAL_SPOT_CHECK_MEAS;
        memcpy(&(p_ind->value.spot_meas), p_spot_meas, sizeof(plxp_spot_meas_t));;
        ke_msg_send(p_ind);
    }
}


/**
 ****************************************************************************************
 * @brief Function called when Continuous measurement information is received
 *
 * @param[in] conidx         Connection index
 * @param[in] p_cont_meas    Pointer to continuous measurement information
 *
 ****************************************************************************************
 */
__STATIC void plxc_cb_cont_meas(uint8_t conidx, const plxp_cont_meas_t* p_cont_meas)
{
    ke_task_id_t src_id  = PRF_SRC_TASK(PLXC);
    ke_task_id_t dest_id = PRF_DST_TASK(PLXC);

    struct plxc_value_ind *p_ind = KE_MSG_ALLOC(PLXC_VALUE_IND, dest_id, src_id, plxc_value_ind);
    if(p_ind != NULL)
    {
        p_ind->conidx      = conidx;
        p_ind->val_id      = PLXC_VAL_CONTINUOUS_MEAS;
        memcpy(&(p_ind->value.cont_meas), p_cont_meas, sizeof(plxp_cont_meas_t));;
        ke_msg_send(p_ind);
    }
}

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
__STATIC void plxc_cb_racp_req_cmp(uint8_t conidx, uint16_t status, uint8_t req_op_code)
{
    plxc_send_cmp_evt(conidx, PLXC_OP_CODE_WRITE_RACP, status);
}

/**
 ****************************************************************************************
 * @brief Reception of record access point response.
 *
 * @param[in] conidx        Connection index
 * @param[in] req_op_code   Requested Operation Code (@see enum plxp_cp_operator_id)
 * @param[in] racp_status   Record access control point execution status (@see enum plxp_cp_resp_code_id)
 * @param[in] num_of_record Number of record (meaningful for GLP_REQ_REP_NUM_OF_STRD_RECS operation)
 *
 ****************************************************************************************
 */
__STATIC void plxc_cb_racp_rsp_recv(uint8_t conidx, uint8_t req_op_code, uint8_t racp_status, uint16_t num_of_record)
{
    ke_task_id_t src_id  = PRF_SRC_TASK(PLXC);
    ke_task_id_t dest_id = PRF_DST_TASK(PLXC);


    struct plxc_value_ind *p_ind = KE_MSG_ALLOC(PLXC_VALUE_IND, dest_id, src_id, plxc_value_ind);
    if(p_ind != NULL)
    {
        p_ind->conidx      = conidx;
        p_ind->val_id      = PLXC_VAL_RACP_RSP;
        p_ind->value.racp_rsp.req_cp_opcode = req_op_code;
        p_ind->value.racp_rsp.rsp_code      = racp_status;
        p_ind->value.racp_rsp.rec_num       = num_of_record;
        ke_msg_send(p_ind);
    }
}


/// Default Message handle
__STATIC const plxc_cb_t plxc_msg_cb =
{
    .cb_enable_cmp        = plxc_cb_enable_cmp,
    .cb_read_features_cmp = plxc_cb_read_features_cmp,
    .cb_read_cfg_cmp      = plxc_cb_read_cfg_cmp,
    .cb_write_cfg_cmp     = plxc_cb_write_cfg_cmp,
    .cb_spot_meas         = plxc_cb_spot_meas,
    .cb_cont_meas         = plxc_cb_cont_meas,
    .cb_racp_req_cmp      = plxc_cb_racp_req_cmp,
    .cb_racp_rsp_recv     = plxc_cb_racp_rsp_recv,
};
#endif // (BLE_HL_MSG_API)


/*
 * PROFILE DEFAULT HANDLERS
 ****************************************************************************************
 */
/**
 ****************************************************************************************
 * @brief Initialization of the Client module.
 * This function performs all the initializations of the Profile module.
 *  - Creation of database (if it's a service)
 *  - Allocation of profile required memory
 *  - Initialization of task descriptor to register application
 *      - Task State array
 *      - Number of tasks
 *      - Default task handler
 *
 * @param[out]    p_env        Collector or Service allocated environment data.
 * @param[in|out] p_start_hdl  Service start handle (0 - dynamically allocated), only applies for services.
 * @param[in]     sec_lvl      Security level (@see enum gatt_svc_info_bf)
 * @param[in]     user_prio    GATT User priority
 * @param[in]     p_param      Configuration parameters of profile collector or service (32 bits aligned)
 * @param[in]     p_cb         Callback structure that handles event from profile
 *
 * @return status code to know if profile initialization succeed or not.
 ****************************************************************************************
 */
__STATIC uint16_t plxc_init(prf_data_t* p_env, uint16_t* p_start_hdl, uint8_t sec_lvl, uint8_t user_prio,
                          const void* p_params, const plxc_cb_t* p_cb)
{
    uint8_t conidx;
    // DB Creation Status
    uint16_t status = GAP_ERR_NO_ERROR;
    uint8_t user_lid = GATT_INVALID_USER_LID;

    do
    {
        plxc_env_t* p_plxc_env;

        #if (BLE_HL_MSG_API)
        if(p_cb == NULL)
        {
            p_cb = &(plxc_msg_cb);
        }
        #endif // (BLE_HL_MSG_API)

        if(   (p_params == NULL) || (p_cb == NULL) || (p_cb->cb_enable_cmp == NULL) || (p_cb->cb_read_features_cmp == NULL)
           || (p_cb->cb_read_cfg_cmp == NULL) || (p_cb->cb_write_cfg_cmp == NULL) || (p_cb->cb_spot_meas == NULL)
           || (p_cb->cb_cont_meas == NULL)  || (p_cb->cb_racp_req_cmp == NULL)  || (p_cb->cb_racp_rsp_recv == NULL))
        {
            status = GAP_ERR_INVALID_PARAM;
            break;
        }

        // register PLXC user
        status = gatt_user_cli_register(L2CAP_LE_MTU_MIN, user_prio, &plxc_cb, &user_lid);
        if(status != GAP_ERR_NO_ERROR) break;

        //-------------------- allocate memory required for the profile  ---------------------
        p_plxc_env = (plxc_env_t*) ke_malloc(sizeof(plxc_env_t), KE_MEM_ATT_DB);

        if(p_plxc_env != NULL)
        {
            // allocate PLXC required environment variable
            p_env->p_env = (prf_hdr_t *) p_plxc_env;

            // initialize environment variable
            p_plxc_env->prf_env.p_cb    = p_cb;
            #if (BLE_HL_MSG_API)
            p_env->desc.msg_handler_tab = plxc_msg_handler_tab;
            p_env->desc.msg_cnt         = ARRAY_LEN(plxc_msg_handler_tab);
            #endif // (BLE_HL_MSG_API)

            p_plxc_env->user_lid = user_lid;
            for (conidx = 0; conidx < BLE_CONNECTION_MAX; conidx++)
            {
                p_plxc_env->p_env[conidx] = NULL;
            }
        }
    } while(0);


    if((status != GAP_ERR_NO_ERROR) && (user_lid != GATT_INVALID_USER_LID))
    {
        gatt_user_unregister(user_lid);
    }

    return (status);
}

/**
 ****************************************************************************************
 * @brief Destruction of the profile module - due to a reset or profile remove.
 *
 * This function clean-up allocated memory.
 *
 * @param[in|out]    p_env        Collector or Service allocated environment data.
 * @param[in]        reason       Destroy reason (@see enum prf_destroy_reason)
 *
 * @return status of the destruction, if fails, profile considered not removed.
 ****************************************************************************************
 */
__STATIC uint16_t plxc_destroy(prf_data_t *p_env, uint8_t reason)
{
    uint16_t status = GAP_ERR_NO_ERROR;
    plxc_env_t* p_plxc_env = (plxc_env_t*) p_env->p_env;

    if(reason != PRF_DESTROY_RESET)
    {
        status = gatt_user_unregister(p_plxc_env->user_lid);
    }

    if(status == GAP_ERR_NO_ERROR)
    {
        uint8_t conidx;

        // cleanup environment variable for each task instances
        for (conidx = 0; conidx < BLE_CONNECTION_MAX; conidx++)
        {
            if (p_plxc_env->p_env[conidx] != NULL)
            {
                if(reason != PRF_DESTROY_RESET)
                {
                    co_time_timer_stop(&(p_plxc_env->p_env[conidx]->timer));
                }

                ke_free(p_plxc_env->p_env[conidx]);
            }
        }

        // free profile environment variables
        p_env->p_env = NULL;
        ke_free(p_plxc_env);
    }

    return (status);
}

/**
 ****************************************************************************************
 * @brief Handles Connection creation
 *
 * @param[in|out]    env          Collector or Service allocated environment data.
 * @param[in]        conidx       Connection index
 * @param[in]        p_con_param  Pointer to connection parameters information
 ****************************************************************************************
 */
__STATIC void plxc_con_create(prf_data_t *p_env, uint8_t conidx, const gap_con_param_t* p_con_param)
{
    // Nothing to do
}

/**
 ****************************************************************************************
 * @brief Handles Disconnection
 *
 * @param[in|out]    p_env      Collector or Service allocated environment data.
 * @param[in]        conidx     Connection index
 * @param[in]        reason     Detach reason
 ****************************************************************************************
 */
__STATIC void plxc_con_cleanup(prf_data_t *p_env, uint8_t conidx, uint16_t reason)
{
    plxc_env_t* p_plxc_env = (plxc_env_t*) p_env->p_env;

    // clean-up environment variable allocated for task instance
    if (p_plxc_env->p_env[conidx] != NULL)
    {
        co_time_timer_stop(&(p_plxc_env->p_env[conidx]->timer));
        ke_free(p_plxc_env->p_env[conidx]);
        p_plxc_env->p_env[conidx] = NULL;
    }
}

/// PLXC Task interface required by profile manager
const prf_task_cbs_t plxc_itf =
{
    .cb_init          = (prf_init_cb) plxc_init,
    .cb_destroy       = plxc_destroy,
    .cb_con_create    = plxc_con_create,
    .cb_con_cleanup   = plxc_con_cleanup,
    .cb_con_upd       = NULL,
};

/**
 ****************************************************************************************
 * @brief Retrieve client profile interface
 *
 * @return Client profile interface
 ****************************************************************************************
 */
const prf_task_cbs_t* plxc_prf_itf_get(void)
{
    return &plxc_itf;
}

#endif //(BLE_PLX_CLIENT)

/// @} PLXC

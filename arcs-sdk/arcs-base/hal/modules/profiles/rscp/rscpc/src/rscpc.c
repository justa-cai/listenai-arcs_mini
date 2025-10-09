/**
 ****************************************************************************************
 *
 * @file rscpc.c
 *
 * @brief Running Speed and Cadence Profile Collector implementation.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 ****************************************************************************************
 */


/**
 ****************************************************************************************
 * @addtogroup RSCPC
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "btip_config.h"
#if (BLE_RSC_COLLECTOR)
#include "rscpc.h"
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
/// RSCP Control point procedure timeout (30s in milliseconds)
#define RSCP_SP_TIMEOUT  (30000)

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */


/// Environment variable for each Connections
typedef struct rscpc_cnx_env
{
    /// Control point timer
    co_time_timer_t       timer;
    /// Peer database discovered handle mapping
    rscpc_rscs_content_t  rscs;
    /// counter used to check service uniqueness
    uint8_t               nb_svc;
    /// Client is in discovering state
    bool                  discover;
    /// Control point operation on-going (@see enum rscp_sc_ctnl_pt_op_code)
    uint8_t               ctrl_pt_op;
} rscpc_cnx_env_t;

/// Client environment variable
typedef struct rscpc_env
{
    /// profile environment
    prf_hdr_t            prf_env;
    /// Environment variable pointer for each connections
    rscpc_cnx_env_t*     p_env[BLE_CONNECTION_MAX];
    /// GATT User local identifier
    uint8_t              user_lid;
} rscpc_env_t;


/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */

/// State machine used to retrieve Running Speed an Cadence service characteristics information
const prf_char_def_t rscpc_rscs_char[RSCP_RSCS_CHAR_MAX] =
{
    // RSC Measurement
    [RSCP_RSCS_RSC_MEAS_CHAR]   = { GATT_CHAR_RSC_MEAS,   ATT_REQ(PRES, MAND), PROP(N)             },
    // RSC Feature
    [RSCP_RSCS_RSC_FEAT_CHAR]   = { GATT_CHAR_RSC_FEAT,   ATT_REQ(PRES, MAND), PROP(RD)            },
    // Sensor Location
    [RSCP_RSCS_SENSOR_LOC_CHAR] = { GATT_CHAR_SENSOR_LOC, ATT_REQ(PRES, OPT),  PROP(RD)            },
    // SP Control Point
    [RSCP_RSCS_SC_CTNL_PT_CHAR] = { GATT_CHAR_SC_CNTL_PT, ATT_REQ(PRES, OPT),  PROP(WR) | PROP(I)  },
};

/// State machine used to retrieve Running Speed an Cadence service characteristic descriptor information
const prf_desc_def_t rscpc_rscs_char_desc[RSCPC_DESC_MAX] =
{
    // RSC Measurement Char. - Client Characteristic Configuration
    [RSCPC_DESC_RSC_MEAS_CL_CFG]   = { GATT_DESC_CLIENT_CHAR_CFG,   ATT_REQ(PRES, MAND), RSCP_RSCS_RSC_MEAS_CHAR   },
    // SC Control Point Char. - Client Characteristic Configuration
    [RSCPC_DESC_SC_CTNL_PT_CL_CFG] = { GATT_DESC_CLIENT_CHAR_CFG,   ATT_REQ(PRES, OPT),  RSCP_RSCS_SC_CTNL_PT_CHAR },
};

/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Send discovery results to application.
 *
 * @param p_rscpc_env    Client Role task environment
 * @param conidx        Connection index
 * @param status        Response status code
 *****************************************************************************************
 */
__STATIC void rscpc_enable_cmp(rscpc_env_t* p_rscpc_env, uint8_t conidx, uint16_t status)
{
    const rscpc_cb_t* p_cb = (const rscpc_cb_t*) p_rscpc_env->prf_env.p_cb;

    if(p_rscpc_env != NULL)
    {
        rscpc_cnx_env_t* p_con_env = p_rscpc_env->p_env[conidx];
        p_cb->cb_enable_cmp(conidx, status, &(p_con_env->rscs));

        if (status != GAP_ERR_NO_ERROR)
        {
            // clean-up environment variable allocated for task instance
            ke_free(p_con_env);
            p_rscpc_env->p_env[conidx] = NULL;
        }
        else
        {
             p_con_env->discover = false;

             // Register profile handle to catch gatt indications
             gatt_cli_event_register(conidx, p_rscpc_env->user_lid, p_con_env->rscs.svc.shdl,
                                     p_con_env->rscs.svc.ehdl);
        }
    }
}

/**
 ****************************************************************************************
 * @brief Send read result to application,.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the request
 * @param[in] val_id        Value Identifier (@see enum rscpc_val_id)
 * @param[in] length        Length of data value)
 * @param[in] p_data        Pointer of data value
 ****************************************************************************************
 */
__STATIC void rscpc_read_val_cmp(uint8_t conidx, uint16_t status, uint8_t val_id, uint16_t length, const uint8_t* p_data)
{
    rscpc_env_t* p_rscpc_env = PRF_ENV_GET(RSCPC, rscpc);

    if(p_rscpc_env != NULL)
    {
        const rscpc_cb_t* p_cb = (const rscpc_cb_t*) p_rscpc_env->prf_env.p_cb;
        switch (val_id)
        {
            // Read RSC Feature Characteristic value
            case (RSCPC_RD_RSC_FEAT):
            {
                uint16_t sensor_feat = 0;
                if(status == GAP_ERR_NO_ERROR)
                {
                    sensor_feat = co_btohs(co_read16p(p_data));
                }
                p_cb->cb_read_sensor_feat_cmp(conidx, status, sensor_feat);
            } break;

            // Read Sensor Location Characteristic value
            case (RSCPC_RD_SENSOR_LOC):
            {
                uint8_t sensor_loc = 0;

                if(status == GAP_ERR_NO_ERROR)
                {
                    sensor_loc = p_data[0];
                }

                p_cb->cb_read_sensor_loc_cmp(conidx, status, sensor_loc);

            } break;

            // Read Client Characteristic Configuration Descriptor value
            case (RSCPC_RD_WR_RSC_MEAS_CFG):
            case (RSCPC_RD_WR_SC_CTNL_PT_CFG):
            {
                uint16_t cfg_val = 0;

                if(status == GAP_ERR_NO_ERROR)
                {
                    cfg_val = co_btohs(co_read16p(p_data));
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
 * @param[in] val_id        Value Identifier (@see enum rscpc_info)
 ****************************************************************************************
 */
__STATIC uint16_t rscpc_read_val(uint8_t conidx, uint16_t val_id)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    rscpc_env_t* p_rscpc_env = PRF_ENV_GET(RSCPC, rscpc);

    if(p_rscpc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_rscpc_env->p_env[conidx] != NULL) && (!p_rscpc_env->p_env[conidx]->discover))
        {
            rscpc_cnx_env_t* p_con_env = p_rscpc_env->p_env[conidx];
            uint16_t hdl;
            rscpc_rscs_content_t* p_rscs = &(p_con_env->rscs);

            switch(val_id)
            {
                case RSCPC_RD_RSC_FEAT:          { hdl = p_rscs->chars[RSCP_RSCS_RSC_FEAT_CHAR].val_hdl;       } break;
                case RSCPC_RD_SENSOR_LOC:        { hdl = p_rscs->chars[RSCP_RSCS_SENSOR_LOC_CHAR].val_hdl;     } break;
                case RSCPC_RD_WR_RSC_MEAS_CFG:   { hdl = p_rscs->descs[RSCPC_DESC_RSC_MEAS_CL_CFG].desc_hdl;   } break;
                case RSCPC_RD_WR_SC_CTNL_PT_CFG: { hdl = p_rscs->descs[RSCPC_DESC_SC_CTNL_PT_CL_CFG].desc_hdl; } break;
                default:                         { hdl = GATT_INVALID_HDL;                                     } break;
            }

            if(hdl == GATT_INVALID_HDL)
            {
                status = PRF_ERR_INEXISTENT_HDL;
            }
            else
            {
                // perform read request
                status = gatt_cli_read(conidx, p_rscpc_env->user_lid, val_id, hdl, 0, 0);
            }
        }
    }

    return (status);
}


/**
 ****************************************************************************************
 * @brief Packs Control Point data
 * @param[in]  p_buf    Pointer to output buffer
 * @param[in]  op_code  Control point operation code
 * @param[in]  p_value  Control point request value
 * @return length
 ****************************************************************************************
 */
__STATIC uint16_t rscpc_pack_ctnl_pt_req(co_buf_t* p_buf, uint8_t op_code, const union rscp_sc_ctnl_pt_req_val* p_value)
{
    uint16_t status = GAP_ERR_NO_ERROR;
    // Set the operation code
    co_buf_tail(p_buf)[0] = op_code;
    co_buf_tail_reserve(p_buf, 1);

    // Fulfill the message according to the operation code
    switch (op_code)
    {
        case (RSCP_CTNL_PT_OP_SET_CUMUL_VAL):
        {
            // Set the cumulative value
            co_write32p(co_buf_tail(p_buf), co_htobl(p_value->cumul_val));
            co_buf_tail_reserve(p_buf, 4);
        } break;

        case (RSCP_CTNL_PT_OP_UPD_LOC):
        {
            // Set the sensor location
            co_buf_tail(p_buf)[0] = p_value->sensor_loc;
            co_buf_tail_reserve(p_buf, 1);
        } break;

        case (RSCP_CTNL_PT_OP_RESERVED):
        case (RSCP_CTNL_PT_OP_START_CALIB):
        case (RSCP_CTNL_PT_OP_REQ_SUPP_LOC):
        {
            // Nothing more to do
        } break;

        default:
        {
            status = PRF_ERR_INVALID_PARAM;
        } break;
    }

    return (status);
}


/**
 ****************************************************************************************
 * @brief Unpacks measurement data and sends the indication
 * @param[in] p_rscpc_env    Environment variable
 * @param[in] conidx        Connection index
 * @param[in] p_buf         Pointer of input buffer received
 ****************************************************************************************
 */
__STATIC void rscpc_unpack_meas(rscpc_env_t* p_rscpc_env, uint8_t conidx, co_buf_t* p_buf)
{
    const rscpc_cb_t* p_cb = (const rscpc_cb_t*) p_rscpc_env->prf_env.p_cb;
    rscp_rsc_meas_t meas;
    memset(&meas, 0, sizeof(rscp_rsc_meas_t));

    // Flags
    meas.flags = co_buf_data(p_buf)[0];
    co_buf_head_release(p_buf, 1);

    // Instantaneous Speed
    meas.inst_speed = co_btohs(co_read16p(co_buf_data(p_buf)));
    co_buf_head_release(p_buf, 2);
    // Instantaneous Cadence
    meas.inst_cad   = co_buf_data(p_buf)[0];
    co_buf_head_release(p_buf, 1);

    // Instantaneous Stride Length
    if (GETB(meas.flags, RSCP_MEAS_INST_STRIDE_LEN_PRESENT))
    {
        meas.inst_stride_len = co_btohs(co_read16p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 2);
    }

    // Total Distance
    if (GETB(meas.flags, RSCP_MEAS_TOTAL_DST_MEAS_PRESENT))
    {
        meas.total_dist = co_btohl(co_read32p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 4);
    }

    // Inform application about received measurement
    p_cb->cb_meas(conidx, &meas);
}


/**
 ****************************************************************************************
 * @brief Unpacks Control Point data and sends the indication
 * @param[in] p_rscpc_env    Environment variable
 * @param[in] conidx        Connection index
 * @param[in] p_buf         Pointer of input buffer received
 ****************************************************************************************
 */
__STATIC void rscpc_unpack_ctln_pt_rsp(rscpc_env_t* p_rscpc_env, uint8_t conidx, co_buf_t* p_buf)
{
    bool valid = (co_buf_data_len(p_buf) >= RSCP_SC_CNTL_PT_RSP_MIN_LEN);

    uint8_t op_code;
    uint8_t req_op_code;
    uint8_t resp_value;
    union rscp_sc_ctnl_pt_rsp_val value;
    memset(&value, 0, sizeof(union rscp_sc_ctnl_pt_rsp_val));

    // Response Op code
    op_code = co_buf_data(p_buf)[0];
    co_buf_head_release(p_buf, 1);

    // Requested operation code
    req_op_code = co_buf_data(p_buf)[0];
    co_buf_head_release(p_buf, 1);

    // Response value
    resp_value = co_buf_data(p_buf)[0];
    co_buf_head_release(p_buf, 1);

    if(valid && (op_code == RSCP_CTNL_PT_RSP_CODE) && (req_op_code == p_rscpc_env->p_env[conidx]->ctrl_pt_op))
    {
        const rscpc_cb_t* p_cb = (const rscpc_cb_t*) p_rscpc_env->prf_env.p_cb;

        if (resp_value == RSCP_CTNL_PT_RESP_SUCCESS)
        {
            switch (req_op_code)
            {
                case (RSCP_CTNL_PT_OP_REQ_SUPP_LOC):
                {
                   // Location
                   value.supp_sensor_loc = 0;
                   while(co_buf_data_len(p_buf) > 0)
                   {
                       value.supp_sensor_loc |= (1 << co_buf_data(p_buf)[0]);
                       co_buf_head_release(p_buf, 1);
                   }
                } break;

                case (RSCP_CTNL_PT_OP_SET_CUMUL_VAL):
                case (RSCP_CTNL_PT_OP_START_CALIB):
                case (RSCP_CTNL_PT_OP_UPD_LOC):
                {
                    // No parameters
                } break;

                default:
                {

                } break;
            }
        } // else Response value is not success

        p_rscpc_env->p_env[conidx]->ctrl_pt_op = RSCP_CTNL_PT_OP_RESERVED;
        // stop timer
        co_time_timer_stop(&(p_rscpc_env->p_env[conidx]->timer));
        // provide control point response
        p_cb->cb_ctnl_pt_req_cmp(conidx, GAP_ERR_NO_ERROR, req_op_code, resp_value, &value);
    }
}

/**
 ****************************************************************************************
 * @brief Function to called once timer expires
 *
 * @param[in] conidx Connection index
 ****************************************************************************************
 */
__STATIC void rscpc_timer_handler(uint32_t conidx)
{
    // Get the address of the environment
    rscpc_env_t *p_rscpc_env = PRF_ENV_GET(RSCPC, rscpc);

    if (p_rscpc_env != NULL)
    {
        rscpc_cnx_env_t* p_con_env = p_rscpc_env->p_env[conidx];
        ASSERT_ERR(p_con_env != NULL);
        if(p_con_env->ctrl_pt_op != RSCP_CTNL_PT_OP_RESERVED)
        {
            const rscpc_cb_t* p_cb = (const rscpc_cb_t*) p_rscpc_env->prf_env.p_cb;
            uint8_t op_code = p_con_env->ctrl_pt_op;
            p_con_env->ctrl_pt_op = RSCP_CTNL_PT_OP_RESERVED;

            p_cb->cb_ctnl_pt_req_cmp((uint8_t)conidx, PRF_ERR_PROC_TIMEOUT, op_code, 0, NULL);
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
__STATIC void rscpc_svc_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t hdl, uint8_t disc_info,
                          uint8_t nb_att, const gatt_svc_att_t* p_atts)
{
    // Get the address of the environment
    rscpc_env_t* p_rscpc_env = PRF_ENV_GET(RSCPC, rscpc);

    if(p_rscpc_env != NULL)
    {
        rscpc_cnx_env_t* p_con_env = p_rscpc_env->p_env[conidx];

        ASSERT_INFO(p_con_env != NULL, conidx, user_lid);

        if (p_con_env->nb_svc == 0)
        {
            //Even if we get multiple responses we only store 1 range
            if((disc_info == GATT_SVC_CMPLT) || (disc_info == GATT_SVC_START))
            {
                p_con_env->rscs.svc.shdl = hdl;
            }

            if((disc_info == GATT_SVC_CMPLT) || (disc_info == GATT_SVC_END))
            {
                p_con_env->rscs.svc.ehdl = hdl + nb_att -1;
            }

            // Retrieve characteristics
            prf_extract_svc_info(hdl, nb_att, p_atts,
                                 RSCP_RSCS_CHAR_MAX, &rscpc_rscs_char[0],      &(p_con_env->rscs.chars[0]),
                                 RSCPC_DESC_MAX,     &rscpc_rscs_char_desc[0], &(p_con_env->rscs.descs[0]));
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
__STATIC void rscpc_discover_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    // Get the address of the environment
    rscpc_env_t* p_rscpc_env = PRF_ENV_GET(RSCPC, rscpc);

    if(p_rscpc_env != NULL)
    {
        rscpc_cnx_env_t* p_con_env = p_rscpc_env->p_env[conidx];

        if (p_con_env->nb_svc ==  1)
        {
            status = prf_check_svc_validity(RSCP_RSCS_CHAR_MAX, p_con_env->rscs.chars, rscpc_rscs_char,
                                            RSCPC_DESC_MAX, p_con_env->rscs.descs, rscpc_rscs_char_desc);
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

        rscpc_enable_cmp(p_rscpc_env, conidx, status);
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
__STATIC void rscpc_att_val_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t hdl, uint16_t offset,
                              co_buf_t* p_data)
{
    rscpc_read_val_cmp(conidx, GAP_ERR_NO_ERROR, (uint8_t) dummy, co_buf_data_len(p_data), co_buf_data(p_data));
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
__STATIC void rscpc_read_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    if(status != GAP_ERR_NO_ERROR)
    {
        rscpc_read_val_cmp(conidx, status, (uint8_t) dummy, 0, NULL);
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
__STATIC void rscpc_write_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    rscpc_env_t* p_rscpc_env = PRF_ENV_GET(RSCPC, rscpc);

    if(p_rscpc_env != NULL)
    {
        const rscpc_cb_t* p_cb = (const rscpc_cb_t*) p_rscpc_env->prf_env.p_cb;

        switch(dummy)
        {
            // Config control
            case RSCPC_RD_WR_RSC_MEAS_CFG:
            case RSCPC_RD_WR_SC_CTNL_PT_CFG:
            {
                p_cb->cb_write_cfg_cmp(conidx, status, dummy);
            } break;
            // Control point commands
            case RSCPC_IND_SC_CTNL_PT:
            {
                if(status != GAP_ERR_NO_ERROR)
                {
                    uint8_t opcode = p_rscpc_env->p_env[conidx]->ctrl_pt_op;
                    p_rscpc_env->p_env[conidx]->ctrl_pt_op = RSCP_CTNL_PT_OP_RESERVED;
                    p_cb->cb_ctnl_pt_req_cmp(conidx, status, opcode, 0, NULL);
                }
                else
                {
                    // Start Timeout Procedure - wait for Indication reception
                    co_time_timer_set(&(p_rscpc_env->p_env[conidx]->timer), RSCP_SP_TIMEOUT);
                }
            } break;

            default: { /* Nothing to do */} break;
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
__STATIC void rscpc_att_val_evt_cb(uint8_t conidx, uint8_t user_lid, uint16_t token, uint8_t evt_type, bool complete,
                        uint16_t hdl, co_buf_t* p_data)
{
    rscpc_env_t* p_rscpc_env = PRF_ENV_GET(RSCPC, rscpc);

    if(p_rscpc_env != NULL)
    {
        rscpc_cnx_env_t* p_con_env = p_rscpc_env->p_env[conidx];
        rscpc_rscs_content_t* p_rscs = &(p_con_env->rscs);

        if (hdl == p_rscs->chars[RSCP_RSCS_RSC_MEAS_CHAR].val_hdl)
        {
            //Unpack measurement
            rscpc_unpack_meas(p_rscpc_env, conidx, p_data);
        }
        else if (hdl == p_rscs->chars[RSCP_RSCS_SC_CTNL_PT_CHAR].val_hdl)
        {
            // Unpack control point
            rscpc_unpack_ctln_pt_rsp(p_rscpc_env, conidx, p_data);
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
__STATIC void rscpc_svc_changed_cb(uint8_t conidx, uint8_t user_lid, bool out_of_sync, uint16_t start_hdl, uint16_t end_hdl)
{
    // Do Nothing
}

/// Client callback hander
__STATIC const gatt_cli_cb_t rscpc_cb =
{
    .cb_discover_cmp    = rscpc_discover_cmp_cb,
    .cb_read_cmp        = rscpc_read_cmp_cb,
    .cb_write_cmp       = rscpc_write_cmp_cb,
    .cb_att_val_get     = NULL,
    .cb_svc             = rscpc_svc_cb,
    .cb_svc_info        = NULL,
    .cb_inc_svc         = NULL,
    .cb_char            = NULL,
    .cb_desc            = NULL,
    .cb_att_val         = rscpc_att_val_cb,
    .cb_att_val_evt     = rscpc_att_val_evt_cb,
    .cb_svc_changed     = rscpc_svc_changed_cb,
};

/*
 * PROFILE NATIVE API
 ****************************************************************************************
 */

uint16_t rscpc_enable(uint8_t conidx, uint8_t con_type, const rscpc_rscs_content_t* p_rscs)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    rscpc_env_t* p_rscpc_env = PRF_ENV_GET(RSCPC, rscpc);

    if(p_rscpc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_rscpc_env->p_env[conidx] == NULL))
        {
            // allocate environment variable for task instance
            p_rscpc_env->p_env[conidx] = (struct rscpc_cnx_env *) ke_malloc(sizeof(struct rscpc_cnx_env), KE_MEM_ATT_DB);

            if(p_rscpc_env->p_env[conidx] != NULL)
            {
                memset(p_rscpc_env->p_env[conidx], 0, sizeof(struct rscpc_cnx_env));
                co_time_timer_init(&(p_rscpc_env->p_env[conidx]->timer), (co_time_timer_cb)rscpc_timer_handler,
                                   (uint8_t*) ((uint32_t) conidx));

                // Config connection, start discovering
                if (con_type == PRF_CON_DISCOVERY)
                {
                    uint16_t gatt_svc_uuid = GATT_SVC_RUNNING_SPEED_CADENCE;

                    // start discovery
                    status = gatt_cli_discover_svc(conidx, p_rscpc_env->user_lid, 0, GATT_DISCOVER_SVC_PRIMARY_BY_UUID, true,
                                                   GATT_MIN_HDL, GATT_MAX_HDL, GATT_UUID_16, (uint8_t*) &gatt_svc_uuid);

                    // Go to DISCOVERING state
                    p_rscpc_env->p_env[conidx]->discover   = true;
                    p_rscpc_env->p_env[conidx]->ctrl_pt_op = RSCP_CTNL_PT_OP_RESERVED;
                }
                // normal connection, get saved att details
                else
                {
                    memcpy(&(p_rscpc_env->p_env[conidx]->rscs), p_rscs, sizeof(rscpc_rscs_content_t));
                    status = GAP_ERR_NO_ERROR;

                    // send APP confirmation that can start normal connection to TH
                    rscpc_enable_cmp(p_rscpc_env, conidx, GAP_ERR_NO_ERROR);
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

uint16_t rscpc_read_sensor_feat(uint8_t conidx)
{
    uint16_t status = rscpc_read_val(conidx, RSCPC_RD_RSC_FEAT);
    return (status);
}

uint16_t rscpc_read_sensor_loc(uint8_t conidx)
{
    uint16_t status = rscpc_read_val(conidx, RSCPC_RD_SENSOR_LOC);
    return (status);
}

uint16_t rscpc_read_cfg(uint8_t conidx, uint8_t desc_code)
{
    uint16_t status;

    switch(desc_code)
    {
        case RSCPC_RD_WR_RSC_MEAS_CFG:
        case RSCPC_RD_WR_SC_CTNL_PT_CFG: { status = rscpc_read_val(conidx, desc_code);  } break;
        default:                         { status = PRF_ERR_INEXISTENT_HDL;             } break;
    }

    return (status);
}

uint16_t rscpc_write_cfg(uint8_t conidx, uint8_t desc_code, uint16_t cfg_val)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    rscpc_env_t* p_rscpc_env = PRF_ENV_GET(RSCPC, rscpc);

    if(p_rscpc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_rscpc_env->p_env[conidx] != NULL) && (!p_rscpc_env->p_env[conidx]->discover))
        {
            rscpc_cnx_env_t* p_con_env = p_rscpc_env->p_env[conidx];
            uint16_t hdl;
            uint16_t cfg_en_val = 0;
            rscpc_rscs_content_t* p_rscs = &(p_con_env->rscs);

            switch(desc_code)
            {
                case RSCPC_RD_WR_RSC_MEAS_CFG:   { hdl        = p_rscs->descs[RSCPC_DESC_RSC_MEAS_CL_CFG].desc_hdl;
                                                   cfg_en_val =  PRF_CLI_START_NTF;                                   } break;
                case RSCPC_RD_WR_SC_CTNL_PT_CFG: { hdl        = p_rscs->descs[RSCPC_DESC_SC_CTNL_PT_CL_CFG].desc_hdl;
                                                   cfg_en_val =  PRF_CLI_START_IND;                                   } break;
                default:                         { hdl = GATT_INVALID_HDL;                                            } break;
            }

            if(hdl == GATT_INVALID_HDL)
            {
                status = PRF_ERR_INEXISTENT_HDL;
            }
            else if((cfg_val != PRF_CLI_STOP_NTFIND) && (cfg_val != cfg_en_val))
            {
                status = PRF_ERR_INVALID_PARAM;
            }
            else
            {
                // Force endianess
                cfg_val = co_htobs(cfg_val);
                status = prf_gatt_write(conidx, p_rscpc_env->user_lid, desc_code, GATT_WRITE,
                                        hdl, sizeof(uint16_t), (uint8_t *)&cfg_val);
            }
        }
    }

    return (status);
}

uint16_t rscpc_ctnl_pt_req(uint8_t conidx, uint8_t req_op_code, const union rscp_sc_ctnl_pt_req_val* p_value)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    rscpc_env_t* p_rscpc_env = PRF_ENV_GET(RSCPC, rscpc);

    if(p_value == NULL)
    {
        status = PRF_ERR_INVALID_PARAM;
    }
    else if(p_rscpc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_rscpc_env->p_env[conidx] != NULL) && (!p_rscpc_env->p_env[conidx]->discover))
        {
            rscpc_cnx_env_t* p_con_env = p_rscpc_env->p_env[conidx];
            rscpc_rscs_content_t* p_rscs = &(p_con_env->rscs);
            uint16_t hdl = p_rscs->chars[RSCP_RSCS_SC_CTNL_PT_CHAR].val_hdl;

            if(hdl == GATT_INVALID_HDL)
            {
                status = PRF_ERR_INEXISTENT_HDL;
            }
            // reject if there is an ongoing control point operation
            else if(p_con_env->ctrl_pt_op != RSCP_CTNL_PT_OP_RESERVED)
            {
                status = PRF_ERR_REQ_DISALLOWED;
            }
            else
            {
                co_buf_t* p_buf = NULL;

                // allocate buffer for event transmission
                if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, 0, RSCP_SC_CNTL_PT_REQ_MAX_LEN + GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
                {
                    status = rscpc_pack_ctnl_pt_req(p_buf, req_op_code, p_value);

                    if(status == GAP_ERR_NO_ERROR)
                    {
                        status = gatt_cli_write(conidx, p_rscpc_env->user_lid, RSCPC_IND_SC_CTNL_PT, GATT_WRITE, hdl, 0, p_buf);
                        if(status == GAP_ERR_NO_ERROR)
                        {
                            // save on-going operation
                            p_con_env->ctrl_pt_op = req_op_code;
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
 * @brief Send a RSCPC_CMP_EVT message to the task which enabled the profile
 * @param[in] conidx Connection index
 * @param[in] operation Operation
 * @param[in] status Satus
 ****************************************************************************************
 */
__STATIC void rscpc_send_cmp_evt(uint8_t conidx, uint8_t operation, uint16_t status)
{
    struct rscpc_cmp_evt *p_evt;

    // Send the message
    p_evt = KE_MSG_ALLOC(RSCPC_CMP_EVT, PRF_DST_TASK(RSCPC), PRF_SRC_TASK(RSCPC), rscpc_cmp_evt);
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
__STATIC int rscpc_enable_req_handler(ke_msg_id_t const msgid, struct rscpc_enable_req const *p_param,
                                     ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = rscpc_enable(p_param->conidx, p_param->con_type, &(p_param->rscs));

    // send an error if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct rscpc_enable_rsp *p_rsp = KE_MSG_ALLOC(RSCPC_ENABLE_RSP, src_id, dest_id, rscpc_enable_rsp);
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
__STATIC int rscpc_read_cmd_handler(ke_msg_id_t const msgid, struct rscpc_read_cmd const *p_param,
                                   ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status;

    switch(p_param->read_code)
    {
        case RSCPC_RD_RSC_FEAT:          { status = rscpc_read_sensor_feat(p_param->conidx);             } break;
        case RSCPC_RD_SENSOR_LOC:        { status = rscpc_read_sensor_loc(p_param->conidx);              } break;
        case RSCPC_RD_WR_RSC_MEAS_CFG:
        case RSCPC_RD_WR_SC_CTNL_PT_CFG: { status = rscpc_read_cfg(p_param->conidx, p_param->read_code); } break;
        default:                         { status = PRF_ERR_INEXISTENT_HDL;                              } break;
    }

    // send error response if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct rscpc_cmp_evt *p_evt = KE_MSG_ALLOC(RSCPC_CMP_EVT, src_id, dest_id, rscpc_cmp_evt);
        if(p_evt)
        {
            p_evt->conidx     = p_param->conidx;
            p_evt->operation  = RSCPC_READ_OP_CODE;
            p_evt->status     = status;

            ke_msg_send(p_evt);
        }
    }

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief Handles reception of the @ref RSCPC_CFG_NTFIND_CMD message.
 * @param[in] msgid Id of the message received.
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int rscpc_cfg_ntfind_cmd_handler(ke_msg_id_t const msgid,
                                        struct rscpc_cfg_ntfind_cmd *p_param,
                                        ke_task_id_t const dest_id,
                                        ke_task_id_t const src_id)
{
    uint16_t status = rscpc_write_cfg(p_param->conidx, p_param->desc_code, p_param->ntfind_cfg);

    // send error response if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct rscpc_cmp_evt *p_evt = KE_MSG_ALLOC(RSCPC_CMP_EVT, src_id, dest_id, rscpc_cmp_evt);
        if(p_evt)
        {
            p_evt->conidx     = p_param->conidx;
            p_evt->operation  = RSCPC_CFG_NTF_IND_OP_CODE;
            p_evt->status     = status;

            ke_msg_send(p_evt);
        }
    }

    return (KE_MSG_CONSUMED);
}


/**
 ****************************************************************************************
 * @brief Handles reception of the @ref RSCPC_CTNL_PT_CFG_CMD message.
 * @param[in] msgid Id of the message received.
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int rscpc_ctnl_pt_cfg_cmd_handler(ke_msg_id_t const msgid, struct rscpc_ctnl_pt_cfg_cmd *p_param,
                                          ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = rscpc_ctnl_pt_req(p_param->conidx, p_param->op_code, &(p_param->value));

    // send error response if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct rscpc_cmp_evt *p_evt = KE_MSG_ALLOC(RSCPC_CMP_EVT, src_id, dest_id, rscpc_cmp_evt);
        if(p_evt)
        {
            p_evt->conidx     = p_param->conidx;
            p_evt->operation  = RSCPC_CTNL_PT_OP_CODE;
            p_evt->status     = status;

            ke_msg_send(p_evt);
        }
    }

    return (KE_MSG_CONSUMED);
}

/// Default State handlers definition
KE_MSG_HANDLER_TAB(rscpc)
{
    // Note: all messages must be sorted in ID ascending order

    { RSCPC_ENABLE_REQ,               (ke_msg_func_t) rscpc_enable_req_handler         },
    { RSCPC_READ_CMD,                 (ke_msg_func_t) rscpc_read_cmd_handler           },
    { RSCPC_CFG_NTFIND_CMD,           (ke_msg_func_t) rscpc_cfg_ntfind_cmd_handler     },
    { RSCPC_CTNL_PT_CFG_CMD,          (ke_msg_func_t) rscpc_ctnl_pt_cfg_cmd_handler    },
};


/**
 ****************************************************************************************
 * @brief Completion of enable procedure
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 * @param[in] p_rscs         Pointer to peer database description bond data
 ****************************************************************************************
 */
__STATIC void rscpc_cb_enable_cmp(uint8_t conidx, uint16_t status, const rscpc_rscs_content_t* p_rscs)
{
    // Send APP the details of the discovered attributes on RSCPC
    struct rscpc_enable_rsp *p_rsp = KE_MSG_ALLOC(RSCPC_ENABLE_RSP, PRF_DST_TASK(RSCPC), PRF_SRC_TASK(RSCPC),
                                                 rscpc_enable_rsp);
    if(p_rsp != NULL)
    {
        p_rsp->conidx = conidx;
        p_rsp->status = status;
        memcpy(&(p_rsp->rscs), p_rscs, sizeof(rscpc_rscs_content_t));
        ke_msg_send(p_rsp);
    }
}

/**
 ****************************************************************************************
 * @brief Completion of read sensor feature procedure.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 * @param[in] sensor_feat   RSC sensor feature
 *
 ****************************************************************************************
 */
__STATIC void rscpc_cb_read_sensor_feat_cmp(uint8_t conidx, uint16_t status, uint16_t sensor_feat)
{
    ke_task_id_t src_id  = PRF_SRC_TASK(RSCPC);
    ke_task_id_t dest_id = PRF_DST_TASK(RSCPC);

    if(status == GAP_ERR_NO_ERROR)
    {
        struct rscpc_value_ind *p_ind = KE_MSG_ALLOC(RSCPC_VALUE_IND, dest_id, src_id, rscpc_value_ind);
        if(p_ind != NULL)
        {
            p_ind->conidx            = conidx;
            p_ind->att_code          = RSCPC_RD_RSC_FEAT;
            p_ind->value.sensor_feat = sensor_feat;
            ke_msg_send(p_ind);
        }
    }

    rscpc_send_cmp_evt(conidx, RSCPC_READ_OP_CODE, status);
}

/**
 ****************************************************************************************
 * @brief Completion of read sensor location procedure.
 *
 * Wait for @see cb_read_loc_cmp execution before starting a new procedure
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 * @param[in] sensor_loc    Sensor Location
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
__STATIC void rscpc_cb_read_sensor_loc_cmp(uint8_t conidx, uint16_t status, uint8_t sensor_loc)
{
    ke_task_id_t src_id  = PRF_SRC_TASK(RSCPC);
    ke_task_id_t dest_id = PRF_DST_TASK(RSCPC);

    if(status == GAP_ERR_NO_ERROR)
    {
        struct rscpc_value_ind *p_ind = KE_MSG_ALLOC(RSCPC_VALUE_IND, dest_id, src_id, rscpc_value_ind);
        if(p_ind != NULL)
        {
            p_ind->conidx            = conidx;
            p_ind->att_code          = RSCPC_RD_SENSOR_LOC;
            p_ind->value.sensor_loc  = sensor_loc;
            ke_msg_send(p_ind);
        }
    }

    rscpc_send_cmp_evt(conidx, RSCPC_READ_OP_CODE, status);
}

/**
 ****************************************************************************************
 * @brief Completion of read Characteristic Configuration procedure.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 * @param[in] desc_code     Descriptor Code (@see enum rscpc_codes)
 *                              - RSCPC_RD_WR_RSC_MEAS_CFG: RSC Measurement Client Char. Configuration
 *                              - RSCPC_RD_WR_SC_CTNL_PT_CFG: SC Control Point Client Char. Configuration
 * @param[in] cfg_val       Configuration value
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
__STATIC void rscpc_cb_read_cfg_cmp(uint8_t conidx, uint16_t status, uint8_t desc_code, uint16_t cfg_val)
{
    ke_task_id_t src_id  = PRF_SRC_TASK(RSCPC);
    ke_task_id_t dest_id = PRF_DST_TASK(RSCPC);

    if(status == GAP_ERR_NO_ERROR)
    {
        struct rscpc_value_ind *p_ind = KE_MSG_ALLOC(RSCPC_VALUE_IND, dest_id, src_id, rscpc_value_ind);
        if(p_ind != NULL)
        {
            p_ind->conidx            = conidx;
            p_ind->att_code          = desc_code;
            p_ind->value.ntf_cfg     = cfg_val;
            ke_msg_send(p_ind);
        }
    }

    rscpc_send_cmp_evt(conidx, RSCPC_READ_OP_CODE, status);
}

/**
 ****************************************************************************************
 * @brief Completion of write Characteristic Configuration procedure.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 * @param[in] desc_code     Descriptor Code (@see enum rscpc_codes)
 *                              - RSCPC_RD_WR_RSC_MEAS_CFG: RSC Measurement Client Char. Configuration
 *                              - RSCPC_RD_WR_SC_CTNL_PT_CFG: SC Control Point Client Char. Configuration
 *
 ****************************************************************************************
 */
__STATIC void rscpc_cb_write_cfg_cmp(uint8_t conidx, uint16_t status, uint8_t desc_code)
{
    rscpc_send_cmp_evt(conidx, RSCPC_CFG_NTF_IND_OP_CODE, status);
}

/**
 ****************************************************************************************
 * @brief Function called when RSC measurement information is received
 *
 * @param[in] conidx         Connection index
 * @param[in] p_meas         Pointer to RSC measurement information
 ****************************************************************************************
 */
__STATIC void rscpc_cb_meas(uint8_t conidx, const rscp_rsc_meas_t* p_meas)
{
    ke_task_id_t src_id  = PRF_SRC_TASK(RSCPC);
    ke_task_id_t dest_id = PRF_DST_TASK(RSCPC);

    struct rscpc_value_ind *p_ind = KE_MSG_ALLOC(RSCPC_VALUE_IND, dest_id, src_id, rscpc_value_ind);
    if(p_ind != NULL)
    {
        p_ind->conidx            = conidx;
        p_ind->att_code          = RSCPC_NTF_RSC_MEAS;
        memcpy(&(p_ind->value.rsc_meas), p_meas, sizeof(rscp_rsc_meas_t));
        ke_msg_send(p_ind);
    }
}

/**
 ****************************************************************************************
 * @brief Completion of control point request procedure
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the Request Send (@see enum hl_err)
 * @param[in] req_op_code   Requested Operation Code @see enum rscp_sc_ctnl_pt_op_code
 * @param[in] resp_value    Response Value @see enum rscp_ctnl_pt_resp_val
 * @param[in] p_value       Pointer to response data content
 ****************************************************************************************
 */
__STATIC void rscpc_cb_ctnl_pt_req_cmp(uint8_t conidx, uint16_t status, uint8_t req_op_code, uint8_t resp_value,
        const union rscp_sc_ctnl_pt_rsp_val* p_value)
{
    ke_task_id_t src_id  = PRF_SRC_TASK(RSCPC);
    ke_task_id_t dest_id = PRF_DST_TASK(RSCPC);

    if(status == GAP_ERR_NO_ERROR)
    {
        struct rscpc_ctnl_pt_ind *p_ind = KE_MSG_ALLOC(RSCPC_CTNL_PT_IND, dest_id, src_id, rscpc_ctnl_pt_ind);
        if(p_ind != NULL)
        {
            p_ind->conidx            = conidx;
            p_ind->cp_opcode         = RSCP_CTNL_PT_RSP_CODE;
            p_ind->req_op_code       = req_op_code;
            p_ind->resp_value        = resp_value;
            p_ind->supp_sensor_loc   = p_value->supp_sensor_loc;
            ke_msg_send(p_ind);
        }
    }

    rscpc_send_cmp_evt(conidx, RSCPC_CTNL_PT_OP_CODE, status);
}

/// Default Message handle
__STATIC const rscpc_cb_t rscpc_msg_cb =
{
     .cb_enable_cmp           = rscpc_cb_enable_cmp,
     .cb_read_sensor_feat_cmp = rscpc_cb_read_sensor_feat_cmp,
     .cb_read_sensor_loc_cmp  = rscpc_cb_read_sensor_loc_cmp,
     .cb_read_cfg_cmp         = rscpc_cb_read_cfg_cmp,
     .cb_write_cfg_cmp        = rscpc_cb_write_cfg_cmp,
     .cb_meas                 = rscpc_cb_meas,
     .cb_ctnl_pt_req_cmp      = rscpc_cb_ctnl_pt_req_cmp,
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
__STATIC uint16_t rscpc_init(prf_data_t* p_env, uint16_t* p_start_hdl, uint8_t sec_lvl, uint8_t user_prio,
                          const void* p_params, const rscpc_cb_t* p_cb)
{
    uint8_t conidx;
    // DB Creation Status
    uint16_t status = GAP_ERR_NO_ERROR;
    uint8_t user_lid = GATT_INVALID_USER_LID;

    do
    {
        rscpc_env_t* p_rscpc_env;

        #if (BLE_HL_MSG_API)
        if(p_cb == NULL)
        {
            p_cb = &(rscpc_msg_cb);
        }
        #endif // (BLE_HL_MSG_API)

        if(   (p_params == NULL) || (p_cb == NULL) || (p_cb->cb_enable_cmp == NULL) || (p_cb->cb_read_sensor_feat_cmp == NULL)
           || (p_cb->cb_read_sensor_loc_cmp == NULL) || (p_cb->cb_read_cfg_cmp == NULL) || (p_cb->cb_write_cfg_cmp == NULL)
           || (p_cb->cb_meas == NULL) || (p_cb->cb_ctnl_pt_req_cmp == NULL))
        {
            status = GAP_ERR_INVALID_PARAM;
            break;
        }

        // register RSCPC user
        status = gatt_user_cli_register(L2CAP_LE_MTU_MIN, user_prio, &rscpc_cb, &user_lid);
        if(status != GAP_ERR_NO_ERROR) break;

        //-------------------- allocate memory required for the profile  ---------------------
        p_rscpc_env = (rscpc_env_t*) ke_malloc(sizeof(rscpc_env_t), KE_MEM_ATT_DB);

        if(p_rscpc_env != NULL)
        {
            // allocate RSCPC required environment variable
            p_env->p_env = (prf_hdr_t *) p_rscpc_env;

            // initialize environment variable
            p_rscpc_env->prf_env.p_cb    = p_cb;
            #if (BLE_HL_MSG_API)
            p_env->desc.msg_handler_tab = rscpc_msg_handler_tab;
            p_env->desc.msg_cnt         = ARRAY_LEN(rscpc_msg_handler_tab);
            #endif // (BLE_HL_MSG_API)

            p_rscpc_env->user_lid = user_lid;
            for (conidx = 0; conidx < BLE_CONNECTION_MAX; conidx++)
            {
                p_rscpc_env->p_env[conidx] = NULL;
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
__STATIC uint16_t rscpc_destroy(prf_data_t *p_env, uint8_t reason)
{
    uint16_t status = GAP_ERR_NO_ERROR;
    rscpc_env_t* p_rscpc_env = (rscpc_env_t*) p_env->p_env;

    if(reason != PRF_DESTROY_RESET)
    {
        status = gatt_user_unregister(p_rscpc_env->user_lid);
    }

    if(status == GAP_ERR_NO_ERROR)
    {
        uint8_t conidx;

        // cleanup environment variable for each task instances
        for (conidx = 0; conidx < BLE_CONNECTION_MAX; conidx++)
        {
            if (p_rscpc_env->p_env[conidx] != NULL)
            {
                if(reason != PRF_DESTROY_RESET)
                {
                    co_time_timer_stop(&(p_rscpc_env->p_env[conidx]->timer));
                }

                ke_free(p_rscpc_env->p_env[conidx]);
            }
        }

        // free profile environment variables
        p_env->p_env = NULL;
        ke_free(p_rscpc_env);
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
__STATIC void rscpc_con_create(prf_data_t *p_env, uint8_t conidx, const gap_con_param_t* p_con_param)
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
__STATIC void rscpc_con_cleanup(prf_data_t *p_env, uint8_t conidx, uint16_t reason)
{
    rscpc_env_t* p_rscpc_env = (rscpc_env_t*) p_env->p_env;

    // clean-up environment variable allocated for task instance
    if (p_rscpc_env->p_env[conidx] != NULL)
    {
        co_time_timer_stop(&(p_rscpc_env->p_env[conidx]->timer));
        ke_free(p_rscpc_env->p_env[conidx]);
        p_rscpc_env->p_env[conidx] = NULL;
    }
}

/// RSCPC Task interface required by profile manager
const prf_task_cbs_t rscpc_itf =
{
    .cb_init          = (prf_init_cb) rscpc_init,
    .cb_destroy       = rscpc_destroy,
    .cb_con_create    = rscpc_con_create,
    .cb_con_cleanup   = rscpc_con_cleanup,
    .cb_con_upd       = NULL,
};

/**
 ****************************************************************************************
 * @brief Retrieve client profile interface
 *
 * @return Client profile interface
 ****************************************************************************************
 */
const prf_task_cbs_t* rscpc_prf_itf_get(void)
{
    return &rscpc_itf;
}

#endif //(BLE_RSC_COLLECTOR)
/// @} RSCP

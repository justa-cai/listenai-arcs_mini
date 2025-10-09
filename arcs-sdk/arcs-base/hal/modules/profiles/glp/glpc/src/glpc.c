/**
 ****************************************************************************************
 *
 * @file glpc.c
 *
 * @brief Glucose Profile Collector implementation.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */


/**
 ****************************************************************************************
 * @addtogroup GLPC
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "btip_config.h"

#if 1 //(BLE_GL_COLLECTOR)

#include "../api/glpc.h"
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
#define GLPC_RACP_TIMEOUT   (30000)
/// Invalid descriptor
#define GLPC_DESC_INVALID   (0xFFFF)

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */


/// Environment variable for each Connections
typedef struct glpc_cnx_env
{
    /// Control point timer
    co_time_timer_t     timer;
    /// Glucose Service Characteristics
    gls_content_t       gls;
    /// counter used to check service uniqueness
    uint8_t             nb_svc;
    /// Client is in discovering state
    bool                discover;
    /// Control point operation on-going (@see enum glp_racp_op_code)
    uint8_t             racp_op_code;
} glpc_cnx_env_t;

/// Client environment variable
typedef struct glpc_env
{
    /// profile environment
    prf_hdr_t            prf_env;
    /// Environment variable pointer for each connections
    glpc_cnx_env_t*      p_env[BLE_CONNECTION_MAX];
    /// GATT User local identifier
    uint8_t              user_lid;
} glpc_env_t;


/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */

/// State machine used to retrieve Glucose service characteristics information
const prf_char_def_t glpc_gls_char[GLPC_CHAR_MAX] =
{
    // Glucose Measurement
    [GLPC_CHAR_MEAS]     = { GATT_CHAR_GLUCOSE_MEAS,       ATT_REQ(PRES, MAND), PROP(N)            },
    // Glucose Measurement Context
    [GLPC_CHAR_MEAS_CTX] = { GATT_CHAR_GLUCOSE_MEAS_CTX,   ATT_REQ(PRES, OPT),  PROP(N)            },
    // Glucose Feature
    [GLPC_CHAR_FEATURE]  = { GATT_CHAR_GLUCOSE_FEATURE,    ATT_REQ(PRES, MAND), PROP(RD)           },
    // Record Access control point
    [GLPC_CHAR_RACP]     = { GATT_CHAR_REC_ACCESS_CTRL_PT, ATT_REQ(PRES, MAND), PROP(WR) | PROP(I) },
};

/// State machine used to retrieve Glucose service characteristic descriptor information
const prf_desc_def_t glpc_gls_char_desc[GLPC_DESC_MAX] =
{
    // Glucose Measurement client config
    [GLPC_DESC_MEAS_CLI_CFG]     = { GATT_DESC_CLIENT_CHAR_CFG, ATT_REQ(PRES, MAND), GLPC_CHAR_MEAS     },
    // Glucose Measurement context client config
    [GLPC_DESC_MEAS_CTX_CLI_CFG] = { GATT_DESC_CLIENT_CHAR_CFG, ATT_REQ(PRES, MAND), GLPC_CHAR_MEAS_CTX },
    // Record Access control point client config
    [GLPC_DESC_RACP_CLI_CFG]     = { GATT_DESC_CLIENT_CHAR_CFG, ATT_REQ(PRES, MAND), GLPC_CHAR_RACP     },
};

/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Send discovery results to application.
 *
 * @param p_glpc_env    Client Role task environment
 * @param conidx        Connection index
 * @param status        Response status code
 *****************************************************************************************
 */
__STATIC void glpc_enable_cmp(glpc_env_t* p_glpc_env, uint8_t conidx, uint16_t status)
{
    const glpc_cb_t* p_cb = (const glpc_cb_t*) p_glpc_env->prf_env.p_cb;

    if(p_glpc_env != NULL)
    {
        glpc_cnx_env_t* p_con_env = p_glpc_env->p_env[conidx];
        p_cb->cb_enable_cmp(conidx, status, &(p_con_env->gls));

        if (status != GAP_ERR_NO_ERROR)
        {
            // clean-up environment variable allocated for task instance
            ke_free(p_con_env);
            p_glpc_env->p_env[conidx] = NULL;
        }
        else
        {
             p_con_env->discover = false;

             // Register profile handle to catch gatt indications
             gatt_cli_event_register(conidx, p_glpc_env->user_lid, p_con_env->gls.svc.shdl,
                                     p_con_env->gls.svc.ehdl);
        }
    }
}

/**
 ****************************************************************************************
 * @brief Send read result to application,.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the request
 * @param[in] val_id        Value Identifier (@see enum glpc_val_id)
 * @param[in] length        Length of data value)
 * @param[in] p_data        Pointer of data value
 ****************************************************************************************
 */
__STATIC void glpc_read_val_cmp(uint8_t conidx, uint16_t status, uint8_t val_id, uint16_t length, const uint8_t* p_data)
{
    glpc_env_t* p_glpc_env = PRF_ENV_GET(GLPC, glpc);

    if(p_glpc_env != NULL)
    {
        const glpc_cb_t* p_cb = (const glpc_cb_t*) p_glpc_env->prf_env.p_cb;
        switch (val_id)
        {
            // Read Glucose sensor Feature Characteristic value
            case (GLPC_RD_FEAT):
            {
                uint16_t sensor_feat = 0;
                if(status == GAP_ERR_NO_ERROR)
                {
                    sensor_feat = co_btohs(co_read16p(p_data));
                }
                p_cb->cb_read_sensor_feat_cmp(conidx, status, sensor_feat);
            } break;

            // Read Client Characteristic Configuration Descriptor value
            case (GLPC_RD_DESC_MEAS_CLI_CFG):
            case (GLPC_RD_DESC_MEAS_CTX_CLI_CFG):
            case (GLPC_RD_DESC_RACP_CLI):
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
 * @param[in] val_id        Value Identifier (@see enum glpc_info)
 ****************************************************************************************
 */
__STATIC uint16_t glpc_read_val(uint8_t conidx, uint16_t val_id)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    glpc_env_t* p_glpc_env = PRF_ENV_GET(GLPC, glpc);

    if(p_glpc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_glpc_env->p_env[conidx] != NULL) && (!p_glpc_env->p_env[conidx]->discover))
        {
            glpc_cnx_env_t* p_con_env = p_glpc_env->p_env[conidx];
            uint16_t hdl;
            gls_content_t* p_gls = &(p_con_env->gls);

            switch(val_id)
            {
                case GLPC_RD_FEAT:                  { hdl = p_gls->chars[GLPC_CHAR_FEATURE].val_hdl;           } break;
                case GLPC_RD_DESC_MEAS_CLI_CFG:     { hdl = p_gls->descs[GLPC_DESC_MEAS_CLI_CFG].desc_hdl;     } break;
                case GLPC_RD_DESC_MEAS_CTX_CLI_CFG: { hdl = p_gls->descs[GLPC_DESC_MEAS_CTX_CLI_CFG].desc_hdl; } break;
                case GLPC_RD_DESC_RACP_CLI:         { hdl = p_gls->descs[GLPC_DESC_RACP_CLI_CFG].desc_hdl;     } break;
                default:                            { hdl = GATT_INVALID_HDL;                                  } break;
            }

            if(hdl == GATT_INVALID_HDL)
            {
                status = PRF_ERR_INEXISTENT_HDL;
            }
            else
            {
                // perform read request
                status = gatt_cli_read(conidx, p_glpc_env->user_lid, val_id, hdl, 0, 0);
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
 * @param[in] req_op_code   Requested Operation Code (@see glp_racp_op_code)
 * @param[in] func_operator Function operator (see enum glp_racp_operator)
 * @param[in] filter_type   Filter type (@see enum glp_racp_filter)
 * @param[in] p_filter      Pointer to filter information
 *
 * @return Function execution status
 ****************************************************************************************
 */
__STATIC uint16_t glpc_pack_racp_req(co_buf_t* p_buf, uint8_t op_code, uint8_t func_operator,
                                     uint8_t filter_type, const union glp_filter* p_filter)
{
    uint16_t status = GAP_ERR_NO_ERROR;

    // command op code
    co_buf_tail(p_buf)[0] = op_code;
    co_buf_tail_reserve(p_buf, 1);

    // operator of the function
    co_buf_tail(p_buf)[0] = func_operator;
    co_buf_tail_reserve(p_buf, 1);

    do
    {
        // Abort operation don't require any other parameter
        if (op_code == GLP_REQ_ABORT_OP) break;

        // check if request requires operand (filter)
        if ((func_operator >= GLP_OP_LT_OR_EQ) || (func_operator <= GLP_OP_WITHIN_RANGE_OF))
        {
            if(p_filter == NULL)
            {
                status = PRF_ERR_INVALID_PARAM;
                break;
            }

            // command filter type
            co_buf_tail(p_buf)[0] = filter_type;
            co_buf_tail_reserve(p_buf, 1);

            // filter uses sequence number
            if (filter_type == GLP_FILTER_SEQ_NUMBER)
            {
                // minimum value
                if ((func_operator == GLP_OP_GT_OR_EQ) || (func_operator == GLP_OP_WITHIN_RANGE_OF))
                {
                    // minimum value
                    co_write16p(co_buf_tail(p_buf), co_htobs(p_filter->seq_num.min));
                    co_buf_tail_reserve(p_buf, 2);
                }

                // maximum value
                if ((func_operator == GLP_OP_LT_OR_EQ) || (func_operator == GLP_OP_WITHIN_RANGE_OF))
                {
                    // maximum value
                    co_write16p(co_buf_tail(p_buf), co_htobs(p_filter->seq_num.max));
                    co_buf_tail_reserve(p_buf, 2);
                }
            }
            // filter uses user facing time
            else
            {
                // retrieve minimum value
                if ((func_operator == GLP_OP_GT_OR_EQ) || (func_operator == GLP_OP_WITHIN_RANGE_OF))
                {
                    // retrieve minimum facing time
                    prf_pack_date_time(p_buf, &(p_filter->time.facetime_min));
                }

                // retrieve maximum value
                if ((func_operator == GLP_OP_LT_OR_EQ) || (func_operator == GLP_OP_WITHIN_RANGE_OF))
                {
                    // retrieve maximum facing time
                    prf_pack_date_time(p_buf, &(p_filter->time.facetime_max));
                }
            }
        }
    } while (0);

    return (status);
}


/**
 ****************************************************************************************
 * @brief Unpacks measurement data and sends the indication
 * @param[in] p_glpc_env    Environment variable
 * @param[in] conidx        Connection index
 * @param[in] p_buf         Pointer of input buffer received
 ****************************************************************************************
 */
__STATIC void glpc_unpack_meas(glpc_env_t* p_glpc_env, uint8_t conidx, co_buf_t* p_buf)
{
    const glpc_cb_t* p_cb = (const glpc_cb_t*) p_glpc_env->prf_env.p_cb;
    uint16_t seq_num;
    glp_meas_t meas;
    memset(&meas, 0, sizeof(glp_meas_t));

    // Flags
    meas.flags = co_buf_data(p_buf)[0];
    co_buf_head_release(p_buf, 1);

    // Sequence Number
    seq_num = co_btohs(co_read16p(co_buf_data(p_buf)));
    co_buf_head_release(p_buf, 2);

    // Base Time
    prf_unpack_date_time(p_buf, &(meas.base_time));

    // Time Offset
    if (GETB(meas.flags, GLP_MEAS_TIME_OFF_PRES))
    {
        meas.time_offset = co_btohs(co_read16p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 2);
    }

    // Glucose Concentration, type and location
    if (GETB(meas.flags, GLP_MEAS_GL_CTR_TYPE_AND_SPL_LOC_PRES))
    {
        meas.concentration = co_btohs(co_read16p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 2);

        // type and location are 2 nibble values
        meas.location = co_buf_data(p_buf)[0] >> 4;
        meas.type     = co_buf_data(p_buf)[0] & 0xF;

        co_buf_head_release(p_buf, 1);
    }

    // Sensor Status Annunciation
    if (GETB(meas.flags, GLP_MEAS_SENS_STAT_ANNUN_PRES))
    {
        meas.sensor_status = co_btohs(co_read16p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 2);
    }


    // Inform application about received vector
    p_cb->cb_meas(conidx, seq_num, &meas);
}

/**
 ****************************************************************************************
 * @brief Unpacks measurement context  data and sends the indication
 * @param[in] p_glpc_env    Environment variable
 * @param[in] conidx        Connection index
 * @param[in] p_buf         Pointer of input buffer received
 ****************************************************************************************
 */
__STATIC void glpc_unpack_meas_ctx(glpc_env_t* p_glpc_env, uint8_t conidx, co_buf_t* p_buf)
{
    const glpc_cb_t* p_cb = (const glpc_cb_t*) p_glpc_env->prf_env.p_cb;
    glp_meas_ctx_t meas_ctx;
    uint16_t seq_num;
    memset(&meas_ctx, 0, sizeof(glp_meas_ctx_t));

    // Flags
    meas_ctx.flags = co_buf_data(p_buf)[0];
    co_buf_head_release(p_buf, 1);

    // Sequence Number
    seq_num = co_btohs(co_read16p(co_buf_data(p_buf)));
    co_buf_head_release(p_buf, 2);

    // Extended Flags
    if (GETB(meas_ctx.flags, GLP_CTX_EXTD_F_PRES) != 0)
    {
        meas_ctx.ext_flags = co_buf_data(p_buf)[0];
    co_buf_head_release(p_buf, 1);
    }

    // Carbohydrate ID And Carbohydrate Present
    if (GETB(meas_ctx.flags, GLP_CTX_CRBH_ID_AND_CRBH_PRES) != 0)
    {
        // Carbohydrate ID
        meas_ctx.carbo_id = co_buf_data(p_buf)[0];
        co_buf_head_release(p_buf, 1);
        // Carbohydrate Present
        meas_ctx.carbo_val = co_btohs(co_read16p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 2);
    }

    // Meal Present
    if (GETB(meas_ctx.flags, GLP_CTX_MEAL_PRES) != 0)
    {
        meas_ctx.meal = co_buf_data(p_buf)[0];
        co_buf_head_release(p_buf, 1);
    }

    // Tester-Health Present
    if (GETB(meas_ctx.flags, GLP_CTX_TESTER_HEALTH_PRES) != 0)
    {
        // Tester and Health are 2 nibble values
        meas_ctx.health = co_buf_data(p_buf)[0] >> 4;
        meas_ctx.tester = co_buf_data(p_buf)[0] & 0xF;
        co_buf_head_release(p_buf, 1);
    }

    // Exercise Duration & Exercise Intensity Present
    if (GETB(meas_ctx.flags, GLP_CTX_EXE_DUR_AND_EXE_INTENS_PRES) != 0)
    {
        // Exercise Duration
        meas_ctx.exercise_dur = co_btohs(co_read16p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 2);

        // Exercise Intensity
        meas_ctx.exercise_intens = co_buf_data(p_buf)[0];
        co_buf_head_release(p_buf, 1);
    }

    // Medication ID And Medication Present
    if (GETB(meas_ctx.flags, GLP_CTX_MEDIC_ID_AND_MEDIC_PRES) != 0)
    {
        // Medication ID
        meas_ctx.med_id = co_buf_data(p_buf)[0];
        co_buf_head_release(p_buf, 1);

        // Medication Present
        meas_ctx.med_val = co_btohs(co_read16p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 2);
    }

    // HbA1c Present
    if (GETB(meas_ctx.flags, GLP_CTX_HBA1C_PRES) != 0)
    {
        // HbA1c
        meas_ctx.hba1c_val = co_btohs(co_read16p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 2);
    }

    // Inform application about received vector
    p_cb->cb_meas_ctx(conidx, seq_num, &meas_ctx);
}

/**
 ****************************************************************************************
 * @brief Unpacks Control Point data and sends the indication
 * @param[in] p_glpc_env    Environment variable
 * @param[in] conidx        Connection index
 * @param[in] p_buf         Pointer of input buffer received
 ****************************************************************************************
 */
__STATIC void glpc_unpack_racp_rsp(glpc_env_t* p_glpc_env, uint8_t conidx, co_buf_t* p_buf)
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

    if(op_code == GLP_REQ_RSP_CODE)
    {
        // Requested operation code
        req_op_code = co_buf_data(p_buf)[0];
        co_buf_head_release(p_buf, 1);

        // RACP Status value
        racp_status = co_buf_data(p_buf)[0];
        co_buf_head_release(p_buf, 1);
    }
    else if(op_code == GLP_REQ_NUM_OF_STRD_RECS_RSP)
    {
        req_op_code = GLP_REQ_REP_NUM_OF_STRD_RECS;
        racp_status = GLP_RSP_SUCCESS;

        num_of_record = co_btohs(co_read16p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 2);
    }
    else
    {
        valid = false;
    }

    if(valid && ((req_op_code == p_glpc_env->p_env[conidx]->racp_op_code) || (req_op_code == GLP_REQ_ABORT_OP)))
    {
        const glpc_cb_t* p_cb = (const glpc_cb_t*) p_glpc_env->prf_env.p_cb;

        if(req_op_code != GLP_REQ_ABORT_OP)
        {
            p_glpc_env->p_env[conidx]->racp_op_code = GLP_REQ_RESERVED;
        }

        // stop timer
        co_time_timer_stop(&(p_glpc_env->p_env[conidx]->timer));

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
__STATIC void glpc_timer_handler(uint32_t conidx)
{
    // Get the address of the environment
    glpc_env_t *p_glpc_env = PRF_ENV_GET(GLPC, glpc);

    if (p_glpc_env != NULL)
    {
        glpc_cnx_env_t* p_con_env = p_glpc_env->p_env[conidx];
        ASSERT_ERR(p_con_env != NULL);
        if(p_con_env->racp_op_code != GLP_REQ_RESERVED)
        {
            const glpc_cb_t* p_cb = (const glpc_cb_t*) p_glpc_env->prf_env.p_cb;
            uint8_t op_code = p_con_env->racp_op_code;
            p_con_env->racp_op_code = GLP_REQ_RESERVED;

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
__STATIC void glpc_svc_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t hdl, uint8_t disc_info,
                          uint8_t nb_att, const gatt_svc_att_t* p_atts)
{
    // Get the address of the environment
    glpc_env_t* p_glpc_env = PRF_ENV_GET(GLPC, glpc);

    if(p_glpc_env != NULL)
    {
        glpc_cnx_env_t* p_con_env = p_glpc_env->p_env[conidx];

        ASSERT_INFO(p_con_env != NULL, conidx, user_lid);

        if (p_con_env->nb_svc == 0)
        {
            //Even if we get multiple responses we only store 1 range
            if((disc_info == GATT_SVC_CMPLT) || (disc_info == GATT_SVC_START))
            {
                p_con_env->gls.svc.shdl = hdl;
            }

            if((disc_info == GATT_SVC_CMPLT) || (disc_info == GATT_SVC_END))
            {
                p_con_env->gls.svc.ehdl = hdl + nb_att -1;
            }

            // Retrieve characteristics
            prf_extract_svc_info(hdl, nb_att, p_atts,
                                 GLPC_CHAR_MAX, &glpc_gls_char[0],      &(p_con_env->gls.chars[0]),
                                 GLPC_DESC_MAX, &glpc_gls_char_desc[0], &(p_con_env->gls.descs[0]));
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
__STATIC void glpc_discover_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    // Get the address of the environment
    glpc_env_t* p_glpc_env = PRF_ENV_GET(GLPC, glpc);

    if(p_glpc_env != NULL)
    {
        glpc_cnx_env_t* p_con_env = p_glpc_env->p_env[conidx];

        if (p_con_env->nb_svc ==  1)
        {
            status = prf_check_svc_validity(GLPC_CHAR_MAX, p_con_env->gls.chars, glpc_gls_char,
                                            GLPC_DESC_MAX, p_con_env->gls.descs, glpc_gls_char_desc);
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

        glpc_enable_cmp(p_glpc_env, conidx, status);
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
__STATIC void glpc_att_val_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t hdl, uint16_t offset,
                              co_buf_t* p_data)
{
    glpc_read_val_cmp(conidx, GAP_ERR_NO_ERROR, (uint8_t) dummy, co_buf_data_len(p_data), co_buf_data(p_data));
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
__STATIC void glpc_read_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    if(status != GAP_ERR_NO_ERROR)
    {
        glpc_read_val_cmp(conidx, status, (uint8_t) dummy, 0, NULL);
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
__STATIC void glpc_write_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    glpc_env_t* p_glpc_env = PRF_ENV_GET(GLPC, glpc);

    if(p_glpc_env != NULL)
    {
        const glpc_cb_t* p_cb = (const glpc_cb_t*) p_glpc_env->prf_env.p_cb;

        // Register procedure
        if(dummy != GLPC_DESC_INVALID)
        {
            if(status == GAP_ERR_NO_ERROR)
            {
                switch(dummy)
                {
                    // Config control
                    case GLPC_DESC_MEAS_CTX_CLI_CFG: { dummy = GLPC_DESC_MEAS_CLI_CFG; } break;
                    case GLPC_DESC_MEAS_CLI_CFG:     { dummy = GLPC_DESC_RACP_CLI_CFG; } break;
                    case GLPC_DESC_RACP_CLI_CFG:     { dummy = GLPC_DESC_INVALID;     } break;
                    default:                         { ASSERT_ERR(0);                  } break;
                }

                if(dummy != GLPC_DESC_INVALID)
                {
                    glpc_cnx_env_t* p_con_env = p_glpc_env->p_env[conidx];
                    gls_content_t* p_gls = &(p_con_env->gls);
                    // Force endianess
                    uint16_t cfg_val = co_htobs((dummy != GLPC_DESC_RACP_CLI_CFG) ? PRF_CLI_START_NTF : PRF_CLI_START_IND);

                    status = prf_gatt_write(conidx, p_glpc_env->user_lid, dummy, GATT_WRITE,
                                            p_gls->descs[dummy].desc_hdl, sizeof(uint16_t), (uint8_t *)&cfg_val);
                }
            }

            // procedure is over
            if((status != GAP_ERR_NO_ERROR) || (dummy == GLPC_DESC_INVALID))
            {
                p_cb->cb_register_cmp(conidx, status);
            }
        }
        // record access control point command
        else
        {
            uint8_t opcode = p_glpc_env->p_env[conidx]->racp_op_code;
            p_cb->cb_racp_req_cmp(conidx, status, opcode);

            if(status == GAP_ERR_NO_ERROR)
            {
                // Start Timeout Procedure - wait for Indication reception
                co_time_timer_set(&(p_glpc_env->p_env[conidx]->timer), GLPC_RACP_TIMEOUT);
            }
            else
            {
                p_glpc_env->p_env[conidx]->racp_op_code = GLP_REQ_RESERVED;
            }
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
__STATIC void glpc_att_val_evt_cb(uint8_t conidx, uint8_t user_lid, uint16_t token, uint8_t evt_type, bool complete,
                        uint16_t hdl, co_buf_t* p_data)
{
    glpc_env_t* p_glpc_env = PRF_ENV_GET(GLPC, glpc);

    if(p_glpc_env != NULL)
    {
        glpc_cnx_env_t* p_con_env = p_glpc_env->p_env[conidx];
        gls_content_t* p_gls = &(p_con_env->gls);

        if (hdl == p_gls->chars[GLPC_CHAR_MEAS].val_hdl)
        {
            //Unpack measurement
            glpc_unpack_meas(p_glpc_env, conidx, p_data);

            if(p_con_env->racp_op_code != GLP_REQ_RESERVED)
            {
                co_time_timer_set(&(p_glpc_env->p_env[conidx]->timer), GLPC_RACP_TIMEOUT);
            }
        }
        else if (hdl == p_gls->chars[GLPC_CHAR_MEAS_CTX].val_hdl)
        {
            //Unpack measurement
            glpc_unpack_meas_ctx(p_glpc_env, conidx, p_data);

            if(p_con_env->racp_op_code != GLP_REQ_RESERVED)
            {
                co_time_timer_set(&(p_glpc_env->p_env[conidx]->timer), GLPC_RACP_TIMEOUT);
            }
        }
        else if (hdl == p_gls->chars[GLPC_CHAR_RACP].val_hdl)
        {
            // Unpack control point
            glpc_unpack_racp_rsp(p_glpc_env, conidx, p_data);
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
__STATIC void glpc_svc_changed_cb(uint8_t conidx, uint8_t user_lid, bool out_of_sync, uint16_t start_hdl, uint16_t end_hdl)
{
    // Do Nothing
}

/// Client callback hander
__STATIC const gatt_cli_cb_t glpc_cb =
{
    .cb_discover_cmp    = glpc_discover_cmp_cb,
    .cb_read_cmp        = glpc_read_cmp_cb,
    .cb_write_cmp       = glpc_write_cmp_cb,
    .cb_att_val_get     = NULL,
    .cb_svc             = glpc_svc_cb,
    .cb_svc_info        = NULL,
    .cb_inc_svc         = NULL,
    .cb_char            = NULL,
    .cb_desc            = NULL,
    .cb_att_val         = glpc_att_val_cb,
    .cb_att_val_evt     = glpc_att_val_evt_cb,
    .cb_svc_changed     = glpc_svc_changed_cb,
};

/*
 * PROFILE NATIVE API
 ****************************************************************************************
 */

uint16_t glpc_enable(uint8_t conidx, uint8_t con_type, const gls_content_t* p_gls)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    glpc_env_t* p_glpc_env = PRF_ENV_GET(GLPC, glpc);

    if(p_glpc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_glpc_env->p_env[conidx] == NULL))
        {
            // allocate environment variable for task instance
            p_glpc_env->p_env[conidx] = (struct glpc_cnx_env *) ke_malloc(sizeof(struct glpc_cnx_env), KE_MEM_ATT_DB);

            if(p_glpc_env->p_env[conidx] != NULL)
            {
                memset(p_glpc_env->p_env[conidx], 0, sizeof(struct glpc_cnx_env));
                co_time_timer_init(&(p_glpc_env->p_env[conidx]->timer), (co_time_timer_cb)glpc_timer_handler,
                                   (uint8_t*) ((uint32_t) conidx));

                // Config connection, start discovering
                if (con_type == PRF_CON_DISCOVERY)
                {
                    uint16_t gatt_svc_uuid = GATT_SVC_GLUCOSE;

                    // start discovery
                    status = gatt_cli_discover_svc(conidx, p_glpc_env->user_lid, 0, GATT_DISCOVER_SVC_PRIMARY_BY_UUID, true,
                                                   GATT_MIN_HDL, GATT_MAX_HDL, GATT_UUID_16, (uint8_t*) &gatt_svc_uuid);

                    // Go to DISCOVERING state
                    p_glpc_env->p_env[conidx]->discover     = true;
                    p_glpc_env->p_env[conidx]->racp_op_code = GLP_REQ_RESERVED;
                }
                // normal connection, get saved att details
                else
                {
                    memcpy(&(p_glpc_env->p_env[conidx]->gls), p_gls, sizeof(gls_content_t));
                    status = GAP_ERR_NO_ERROR;

                    // send APP confirmation that can start normal connection to TH
                    glpc_enable_cmp(p_glpc_env, conidx, GAP_ERR_NO_ERROR);
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

uint16_t glpc_read_sensor_feat(uint8_t conidx)
{
    uint16_t status = glpc_read_val(conidx, GLPC_RD_FEAT);
    return (status);
}

uint16_t glpc_read_cfg(uint8_t conidx, uint8_t desc_code)
{
    uint16_t status;

    switch(desc_code)
    {
        case GLPC_RD_DESC_MEAS_CLI_CFG:
        case GLPC_RD_DESC_MEAS_CTX_CLI_CFG:
        case GLPC_RD_DESC_RACP_CLI:        { status = glpc_read_val(conidx, desc_code);  } break;
        default:                           { status = PRF_ERR_INEXISTENT_HDL;             } break;
    }

    return (status);
}

uint16_t glpc_register(uint8_t conidx, bool meas_ctx_en)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    glpc_env_t* p_glpc_env = PRF_ENV_GET(GLPC, glpc);

    if(p_glpc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_glpc_env->p_env[conidx] != NULL) && (!p_glpc_env->p_env[conidx]->discover))
        {
            glpc_cnx_env_t* p_con_env = p_glpc_env->p_env[conidx];
            uint8_t desc_code;
            gls_content_t* p_gls = &(p_con_env->gls);
            // Force endianess
            uint16_t cfg_val = co_htobs(PRF_CLI_START_NTF);

            // check if client characteristics are present
            if (   (p_gls->descs[GLPC_DESC_MEAS_CLI_CFG].desc_hdl == GATT_INVALID_HDL)
                || (p_gls->descs[GLPC_DESC_RACP_CLI_CFG].desc_hdl == GATT_INVALID_HDL)
                || ((meas_ctx_en) && (p_gls->descs[GLPC_DESC_MEAS_CTX_CLI_CFG].desc_hdl == GATT_INVALID_HDL)))
            {
                status = PRF_ERR_INEXISTENT_HDL;
            }

            desc_code = (meas_ctx_en) ? GLPC_DESC_MEAS_CTX_CLI_CFG : GLPC_DESC_MEAS_CLI_CFG;

            status = prf_gatt_write(conidx, p_glpc_env->user_lid, desc_code, GATT_WRITE,
                                    p_gls->descs[desc_code].desc_hdl, sizeof(uint16_t), (uint8_t *)&cfg_val);
        }
    }

    return (status);
}

uint16_t glpc_racp_req(uint8_t conidx, uint8_t req_op_code, uint8_t func_operator,
                       uint8_t filter_type, const union glp_filter* p_filter)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    glpc_env_t* p_glpc_env = PRF_ENV_GET(GLPC, glpc);

    if(p_glpc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_glpc_env->p_env[conidx] != NULL) && (!p_glpc_env->p_env[conidx]->discover))
        {
            glpc_cnx_env_t* p_con_env = p_glpc_env->p_env[conidx];
            gls_content_t* p_gls = &(p_con_env->gls);
            uint16_t hdl = p_gls->chars[GLPC_CHAR_RACP].val_hdl;

            if(hdl == GATT_INVALID_HDL)
            {
                status = PRF_ERR_INEXISTENT_HDL;
            }
            // reject if there is an ongoing record access control point operation
            else if((p_con_env->racp_op_code != GLP_REQ_RESERVED) && (req_op_code != GLP_REQ_ABORT_OP))
            {
                status = PRF_ERR_REQ_DISALLOWED;
            }
            else
            {
                co_buf_t* p_buf = NULL;

                // allocate buffer for event transmission
                if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, 0, GLP_REC_ACCESS_CTRL_MAX_LEN + GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
                {
                    status = glpc_pack_racp_req(p_buf, req_op_code, func_operator, filter_type, p_filter);

                    if(status == GAP_ERR_NO_ERROR)
                    {
                        status = gatt_cli_write(conidx, p_glpc_env->user_lid, GLPC_DESC_INVALID, GATT_WRITE, hdl, 0, p_buf);
                        if((status == GAP_ERR_NO_ERROR) && (req_op_code != GLP_REQ_ABORT_OP))
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
 * @brief Send a GLPC_CMP_EVT message to the task which enabled the profile
 * @param[in] conidx Connection index
 * @param[in] operation Operation
 * @param[in] status Satus
 ****************************************************************************************
 */
__STATIC void glpc_send_cmp_evt(uint8_t conidx, uint8_t operation, uint16_t status)
{
    struct glpc_cmp_evt *p_evt;

    // Send the message
    p_evt = KE_MSG_ALLOC(GLPC_CMP_EVT, PRF_DST_TASK(GLPC), PRF_SRC_TASK(GLPC), glpc_cmp_evt);
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
__STATIC int glpc_enable_req_handler(ke_msg_id_t const msgid, struct glpc_enable_req const *p_param,
                                     ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = glpc_enable(p_param->conidx, p_param->con_type, &(p_param->gls));

    // send an error if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct glpc_enable_rsp *p_rsp = KE_MSG_ALLOC(GLPC_ENABLE_RSP, src_id, dest_id, glpc_enable_rsp);
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
__STATIC int glpc_read_cmd_handler(ke_msg_id_t const msgid, struct glpc_read_cmd const *p_param,
                                   ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status;

    switch(p_param->read_code)
    {
        case GLPC_RD_FEAT:                  { status = glpc_read_sensor_feat(p_param->conidx);             } break;
        case GLPC_RD_DESC_MEAS_CLI_CFG:
        case GLPC_RD_DESC_MEAS_CTX_CLI_CFG:
        case GLPC_RD_DESC_RACP_CLI:         { status = glpc_read_cfg(p_param->conidx, p_param->read_code); } break;
        default:                            { status = PRF_ERR_INEXISTENT_HDL;                             } break;
    }

    // send error response if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct glpc_cmp_evt *p_evt = KE_MSG_ALLOC(GLPC_CMP_EVT, src_id, dest_id, glpc_cmp_evt);
        if(p_evt)
        {
            p_evt->conidx     = p_param->conidx;
            p_evt->operation  = GLPC_READ_CMD_OP_CODE;
            p_evt->status     = status;

            ke_msg_send(p_evt);
        }
    }

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief Handles reception of the @ref GLPC_REGISTER_CMD message.
 * When receiving this request, Glucose collector register to measurement notifications
 * and RACP indications.
 *
 * @param[in] msgid Id of the message received (probably unused).
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int glpc_register_cmd_handler(ke_msg_id_t const msgid, struct glpc_register_cmd const *p_param,
                                       ke_task_id_t const dest_id, ke_task_id_t const src_id)

{
    uint16_t status = glpc_register(p_param->conidx, p_param->meas_ctx_en);

    // send error response if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct glpc_cmp_evt *p_evt = KE_MSG_ALLOC(GLPC_CMP_EVT, src_id, dest_id, glpc_cmp_evt);
        if(p_evt)
        {
            p_evt->conidx     = p_param->conidx;
            p_evt->operation  = GLPC_REGISTER_CMD_OP_CODE;
            p_evt->status     = status;

            ke_msg_send(p_evt);
        }
    }

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief Handles reception of the @ref GLPC_RACP_CMD message.
 * When receiving this request, Glucose collector send a RACP command to Glucose sensor.
 *
 * @param[in] msgid Id of the message received (probably unused).
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int glpc_racp_cmd_handler(ke_msg_id_t const msgid, struct glpc_racp_cmd const *p_param,
                                   ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = glpc_racp_req(p_param->conidx, p_param->req.op_code, p_param->req.operator, p_param->req.filter_type,
                                    &(p_param->req.filter));

    // send error response if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct glpc_cmp_evt *p_evt = KE_MSG_ALLOC(GLPC_CMP_EVT, src_id, dest_id, glpc_cmp_evt);
        if(p_evt)
        {
            p_evt->conidx     = p_param->conidx;
            p_evt->operation  = GLPC_RACP_CMD_OP_CODE;
            p_evt->status     = status;

            ke_msg_send(p_evt);
        }
    }

    return (KE_MSG_CONSUMED);
}

/// Default State handlers definition
KE_MSG_HANDLER_TAB(glpc)
{
    // Note: all messages must be sorted in ID ascending order

    { GLPC_ENABLE_REQ,               (ke_msg_func_t) glpc_enable_req_handler         },
    { GLPC_REGISTER_CMD,             (ke_msg_func_t) glpc_register_cmd_handler       },
    { GLPC_READ_CMD,                 (ke_msg_func_t) glpc_read_cmd_handler           },
    { GLPC_RACP_CMD,                 (ke_msg_func_t) glpc_racp_cmd_handler           },
};


/**
 ****************************************************************************************
 * @brief Completion of enable procedure
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 * @param[in] p_gls         Pointer to peer database description bond data
 ****************************************************************************************
 */
__STATIC void glpc_cb_enable_cmp(uint8_t conidx, uint16_t status, const gls_content_t* p_gls)
{
    // Send APP the details of the discovered attributes on GLPC
    struct glpc_enable_rsp *p_rsp = KE_MSG_ALLOC(GLPC_ENABLE_RSP, PRF_DST_TASK(GLPC), PRF_SRC_TASK(GLPC),
                                                 glpc_enable_rsp);
    if(p_rsp != NULL)
    {
        p_rsp->conidx = conidx;
        p_rsp->status = status;
        memcpy(&(p_rsp->gls), p_gls, sizeof(gls_content_t));
        ke_msg_send(p_rsp);
    }
}

/**
 ****************************************************************************************
 * @brief Completion of read sensor feature procedure.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 * @param[in] features      Glucose sensor features
 *
 ****************************************************************************************
 */
__STATIC void glpc_cb_read_sensor_feat_cmp(uint8_t conidx, uint16_t status, uint16_t features)
{
    ke_task_id_t src_id  = PRF_SRC_TASK(GLPC);
    ke_task_id_t dest_id = PRF_DST_TASK(GLPC);

    if(status == GAP_ERR_NO_ERROR)
    {
        struct glpc_read_ind *p_ind = KE_MSG_ALLOC(GLPC_READ_IND, dest_id, src_id, glpc_read_ind);
        if(p_ind != NULL)
        {
            p_ind->conidx         = conidx;
            p_ind->read_code      = GLPC_RD_FEAT;
            p_ind->value.features = features;
            ke_msg_send(p_ind);
        }
    }

    glpc_send_cmp_evt(conidx, GLPC_READ_CMD_OP_CODE, status);
}

/**
 ****************************************************************************************
 * @brief Completion of read Characteristic Configuration procedure.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 * @param[in] read_code     Glucose sensor read characteristic code (@see enum glpc_read_code)
 * @param[in] cfg_val       Configuration value
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
__STATIC void glpc_cb_read_cfg_cmp(uint8_t conidx, uint16_t status, uint8_t read_code, uint16_t cfg_val)
{
    ke_task_id_t src_id  = PRF_SRC_TASK(GLPC);
    ke_task_id_t dest_id = PRF_DST_TASK(GLPC);

    if(status == GAP_ERR_NO_ERROR)
    {
        struct glpc_read_ind *p_ind = KE_MSG_ALLOC(GLPC_READ_IND, dest_id, src_id, glpc_read_ind);
        if(p_ind != NULL)
        {
            p_ind->conidx        = conidx;
            p_ind->read_code     = read_code;
            p_ind->value.ind_cfg = cfg_val;
            ke_msg_send(p_ind);
        }
    }

    glpc_send_cmp_evt(conidx, GLPC_READ_CMD_OP_CODE, status);
}

/**
 ****************************************************************************************
 * @brief Completion of glucose sensor notification register procedure.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 *
 ****************************************************************************************
 */
__STATIC void glpc_cb_register_cmp(uint8_t conidx, uint16_t status)
{
    glpc_send_cmp_evt(conidx, GLPC_REGISTER_CMD_OP_CODE, status);
}

/**
 ****************************************************************************************
 * @brief Function called when CSC measurement information is received
 *
 * @param[in] conidx         Connection index
 * @param[in] seq_num        Measurement sequence number
 * @param[in] p_meas         Pointer to glucose measurement information
 ****************************************************************************************
 */
__STATIC void glpc_cb_meas(uint8_t conidx, uint16_t seq_num, const glp_meas_t* p_meas)
{
    ke_task_id_t src_id  = PRF_SRC_TASK(GLPC);
    ke_task_id_t dest_id = PRF_DST_TASK(GLPC);

    struct glpc_meas_ind *p_ind = KE_MSG_ALLOC(GLPC_MEAS_IND, dest_id, src_id, glpc_meas_ind);
    if(p_ind != NULL)
    {
        p_ind->conidx            = conidx;
        p_ind->seq_num           = seq_num;
        memcpy(&(p_ind->meas_val), p_meas, sizeof(glp_meas_t));
        ke_msg_send(p_ind);
    }
}

/**
 ****************************************************************************************
 * @brief Function called when CSC measurement information is received
 *
 * @param[in] conidx         Connection index
 * @param[in] seq_num        Measurement sequence number
 * @param[in] p_ctx          Pointer to glucose measurement information context
 ****************************************************************************************
 */
__STATIC void glpc_cb_meas_ctx(uint8_t conidx, uint16_t seq_num, const glp_meas_ctx_t* p_ctx)
{
    ke_task_id_t src_id  = PRF_SRC_TASK(GLPC);
    ke_task_id_t dest_id = PRF_DST_TASK(GLPC);

    struct glpc_meas_ctx_ind *p_ind = KE_MSG_ALLOC(GLPC_MEAS_CTX_IND, dest_id, src_id, glpc_meas_ctx_ind);
    if(p_ind != NULL)
    {
        p_ind->conidx            = conidx;
        p_ind->seq_num           = seq_num;
        memcpy(&(p_ind->ctx),      p_ctx, sizeof(glp_meas_ctx_t));
        ke_msg_send(p_ind);
    }
}

/**
 ****************************************************************************************
 * @brief Completion of record access control point request send
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the Request Send (@see enum hl_err)
 * @param[in] req_op_code   Requested Operation Code (@see enum glp_racp_op_code)
 ****************************************************************************************
 */
__STATIC void glpc_cb_racp_req_cmp(uint8_t conidx, uint16_t status, uint8_t req_op_code)
{
    glpc_send_cmp_evt(conidx, GLPC_RACP_CMD_OP_CODE, status);
}



/**
 ****************************************************************************************
 * @brief Reception of record access point response.
 *
 * @param[in] conidx        Connection index
 * @param[in] req_op_code   Requested Operation Code (@see enum glp_racp_op_code)
 * @param[in] racp_status   Record access control point execution status (@see enum glp_racp_status)
 * @param[in] num_of_record Number of record (meaningful for GLP_REQ_REP_NUM_OF_STRD_RECS operation)
 ****************************************************************************************
 */
__STATIC void glpc_cb_racp_rsp_recv(uint8_t conidx, uint8_t req_op_code, uint8_t racp_status, uint16_t num_of_record)
{
    ke_task_id_t src_id  = PRF_SRC_TASK(GLPC);
    ke_task_id_t dest_id = PRF_DST_TASK(GLPC);

    struct glpc_racp_ind *p_ind = KE_MSG_ALLOC(GLPC_RACP_IND, dest_id, src_id, glpc_racp_ind);
    if(p_ind != NULL)
    {
        p_ind->conidx            = conidx;
        p_ind->op_code           = (req_op_code == GLP_REQ_REP_NUM_OF_STRD_RECS)
                                 ? GLP_REQ_NUM_OF_STRD_RECS_RSP : GLP_REQ_RSP_CODE;
        p_ind->req_op_code       = req_op_code;
        p_ind->racp_status       = racp_status;
        p_ind->num_of_record     = num_of_record;
        ke_msg_send(p_ind);
    }
}



/// Default Message handle
__STATIC const glpc_cb_t glpc_msg_cb =
{
     .cb_enable_cmp           = glpc_cb_enable_cmp,
     .cb_read_sensor_feat_cmp = glpc_cb_read_sensor_feat_cmp,
     .cb_read_cfg_cmp         = glpc_cb_read_cfg_cmp,
     .cb_register_cmp         = glpc_cb_register_cmp,
     .cb_meas                 = glpc_cb_meas,
     .cb_meas_ctx             = glpc_cb_meas_ctx,
     .cb_racp_req_cmp         = glpc_cb_racp_req_cmp,
     .cb_racp_rsp_recv        = glpc_cb_racp_rsp_recv,
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
__STATIC uint16_t glpc_init(prf_data_t* p_env, uint16_t* p_start_hdl, uint8_t sec_lvl, uint8_t user_prio,
                          const void* p_params, const glpc_cb_t* p_cb)
{
    uint8_t conidx;
    // DB Creation Status
    uint16_t status = GAP_ERR_NO_ERROR;
    uint8_t user_lid = GATT_INVALID_USER_LID;

    do
    {
        glpc_env_t* p_glpc_env;

        #if (BLE_HL_MSG_API)
        if(p_cb == NULL)
        {
            p_cb = &(glpc_msg_cb);
        }
        #endif // (BLE_HL_MSG_API)

        if(   (p_params == NULL) || (p_cb == NULL) || (p_cb->cb_enable_cmp == NULL) || (p_cb->cb_read_sensor_feat_cmp == NULL)
           || (p_cb->cb_read_cfg_cmp == NULL) || (p_cb->cb_register_cmp == NULL) || (p_cb->cb_meas == NULL)
           || (p_cb->cb_meas_ctx == NULL)  || (p_cb->cb_racp_req_cmp == NULL)  || (p_cb->cb_racp_rsp_recv == NULL))
        {
            status = GAP_ERR_INVALID_PARAM;
            break;
        }

        // register GLPC user
        status = gatt_user_cli_register(L2CAP_LE_MTU_MIN, user_prio, &glpc_cb, &user_lid);
        if(status != GAP_ERR_NO_ERROR) break;

        //-------------------- allocate memory required for the profile  ---------------------
        p_glpc_env = (glpc_env_t*) ke_malloc(sizeof(glpc_env_t), KE_MEM_ATT_DB);

        if(p_glpc_env != NULL)
        {
            // allocate GLPC required environment variable
            p_env->p_env = (prf_hdr_t *) p_glpc_env;

            // initialize environment variable
            p_glpc_env->prf_env.p_cb    = p_cb;
            #if (BLE_HL_MSG_API)
            p_env->desc.msg_handler_tab = glpc_msg_handler_tab;
            p_env->desc.msg_cnt         = ARRAY_LEN(glpc_msg_handler_tab);
            #endif // (BLE_HL_MSG_API)

            p_glpc_env->user_lid = user_lid;
            for (conidx = 0; conidx < BLE_CONNECTION_MAX; conidx++)
            {
                p_glpc_env->p_env[conidx] = NULL;
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
__STATIC uint16_t glpc_destroy(prf_data_t *p_env, uint8_t reason)
{
    uint16_t status = GAP_ERR_NO_ERROR;
    glpc_env_t* p_glpc_env = (glpc_env_t*) p_env->p_env;

    if(reason != PRF_DESTROY_RESET)
    {
        status = gatt_user_unregister(p_glpc_env->user_lid);
    }

    if(status == GAP_ERR_NO_ERROR)
    {
        uint8_t conidx;

        // cleanup environment variable for each task instances
        for (conidx = 0; conidx < BLE_CONNECTION_MAX; conidx++)
        {
            if (p_glpc_env->p_env[conidx] != NULL)
            {
                if(reason != PRF_DESTROY_RESET)
                {
                    co_time_timer_stop(&(p_glpc_env->p_env[conidx]->timer));
                }

                ke_free(p_glpc_env->p_env[conidx]);
            }
        }

        // free profile environment variables
        p_env->p_env = NULL;
        ke_free(p_glpc_env);
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
__STATIC void glpc_con_create(prf_data_t *p_env, uint8_t conidx, const gap_con_param_t* p_con_param)
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
__STATIC void glpc_con_cleanup(prf_data_t *p_env, uint8_t conidx, uint16_t reason)
{
    glpc_env_t* p_glpc_env = (glpc_env_t*) p_env->p_env;

    // clean-up environment variable allocated for task instance
    if (p_glpc_env->p_env[conidx] != NULL)
    {
        co_time_timer_stop(&(p_glpc_env->p_env[conidx]->timer));
        ke_free(p_glpc_env->p_env[conidx]);
        p_glpc_env->p_env[conidx] = NULL;
    }
}

/// GLPC Task interface required by profile manager
const prf_task_cbs_t glpc_itf =
{
    .cb_init          = (prf_init_cb) glpc_init,
    .cb_destroy       = glpc_destroy,
    .cb_con_create    = glpc_con_create,
    .cb_con_cleanup   = glpc_con_cleanup,
    .cb_con_upd       = NULL,
};

/**
 ****************************************************************************************
 * @brief Retrieve client profile interface
 *
 * @return Client profile interface
 ****************************************************************************************
 */
const prf_task_cbs_t* glpc_prf_itf_get(void)
{
    return &glpc_itf;
}

#endif /* (BLE_GL_COLLECTOR) */

/// @} GLPC

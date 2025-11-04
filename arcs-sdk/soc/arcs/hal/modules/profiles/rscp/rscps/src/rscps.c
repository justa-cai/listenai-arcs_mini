/**
 ****************************************************************************************
 *
 * @file rscps.c
 *
 * @brief Running Speed and Cadence Profile Sensor implementation.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup RSCPS
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "btip_config.h"

#if (BLE_RSC_SENSOR)
#include "rscps.h"
#include "gap.h"
#include "gatt.h"
#include "ble_prf.h"

#include "co_utils.h"
#include "co_endian.h"

#include <string.h>
#include "ke_mem.h"

/*
 * DEFINES
 ****************************************************************************************
 */

/********************************************
 ******* RSCPS Configuration Flag Masks ******
 ********************************************/

/// Mandatory Attributes (RSC Measurement + RSC Feature)
#define RSCPS_MANDATORY_MASK               (0x003F)
/// Sensor Location Attributes
#define RSCPS_SENSOR_LOC_MASK              (0x00C0)
/// SC Control Point Attributes
#define RSCPS_SC_CTNL_PT_MASK              (0x0700)


/// Running Speed and Cadence Service - Attribute List
enum rscps_rscs_att_list
{
    /// Running Speed and Cadence Service
    RSCS_IDX_SVC,
    /// RSC Measurement
    RSCS_IDX_RSC_MEAS_CHAR,
    RSCS_IDX_RSC_MEAS_VAL,
    RSCS_IDX_RSC_MEAS_NTF_CFG,
    /// RSC Feature
    RSCS_IDX_RSC_FEAT_CHAR,
    RSCS_IDX_RSC_FEAT_VAL,
    /// Sensor Location
    RSCS_IDX_SENSOR_LOC_CHAR,
    RSCS_IDX_SENSOR_LOC_VAL,
    /// SC Control Point
    RSCS_IDX_SC_CTNL_PT_CHAR,
    RSCS_IDX_SC_CTNL_PT_VAL,
    RSCS_IDX_SC_CTNL_PT_NTF_CFG,

    /// Number of attributes
    RSCS_IDX_NB,
};

/// Profile Configuration Additional Flags bit field
enum rscps_prf_cfg_flag_bf
{
    /// RSC Measurement - Client Char. Cfg
    RSCP_PRF_CFG_FLAG_RSC_MEAS_NTF_POS = 5,
    RSCP_PRF_CFG_FLAG_RSC_MEAS_NTF_BIT = CO_BIT(RSCP_PRF_CFG_FLAG_RSC_MEAS_NTF_POS),

    /// SC Control Point - Client Char. Cfg
    RSCP_PRF_CFG_FLAG_SC_CTNL_PT_IND_POS = 6,
    RSCP_PRF_CFG_FLAG_SC_CTNL_PT_IND_BIT = CO_BIT(RSCP_PRF_CFG_FLAG_SC_CTNL_PT_IND_POS),

    /// Bonded data configured
    RSCP_PRF_CFG_PERFORMED_OK_POS = 7,
    RSCP_PRF_CFG_PERFORMED_OK_BIT = CO_BIT(RSCP_PRF_CFG_PERFORMED_OK_POS),
};

/// Sensor Location Supported Flag
enum rscps_sensor_loc_supp
{
    /// Sensor Location Char. is not supported
    RSCPS_SENSOR_LOC_NOT_SUPP,
    /// Sensor Location Char. is supported
    RSCPS_SENSOR_LOC_SUPP,
};

/*
 * MACROS
 ****************************************************************************************
 */

#define RSCPS_IS_FEATURE_SUPPORTED(features, flag) ((features & flag) == flag)

#define RSCPS_HANDLE(idx) \
    (p_rscps_env->start_hdl + (idx) - \
        ((!(RSCPS_IS_FEATURE_SUPPORTED(p_rscps_env->prf_cfg, RSCPS_SENSOR_LOC_MASK)) && \
                ((idx) > RSCS_IDX_SENSOR_LOC_CHAR))? (1) : (0)))

/*
 * TYPES DEFINITION
 ****************************************************************************************
 */

/// ongoing operation information
typedef struct rscps_buf_meta
{
    /// meaningful for some operation
    uint32_t  conidx_bf;
    /// Operation
    uint8_t   operation;
    /// Connection index targeted
    uint8_t   conidx;
} rscps_buf_meta_t;

/// Running speed and cadence server environment variable
typedef struct rscps_env
{
    /// profile environment
    prf_hdr_t prf_env;
    /// Operation Event TX wait queue
    co_list_t wait_queue;
    /// Service Attribute Start Handle
    uint16_t  start_hdl;
    /// Services features
    uint16_t  features;
    /// profile configuration
    uint16_t  prf_cfg;
    /// Sensor location
    uint8_t   sensor_loc;
    /// GATT user local identifier
    uint8_t   user_lid;
    /// Control point operation on-going (@see enum rscp_sc_ctnl_pt_op_code)
    uint8_t   ctrl_pt_op;
    /// Operation On-going
    bool      op_ongoing;
    /// Prevent recursion in execute_operation function
    bool      in_exe_op;
    /// Environment variable pointer for each connections
    uint8_t   prfl_ntf_ind_cfg[BLE_CONNECTION_MAX];

} rscps_env_t;


/*
 * ATTRIBUTES DATABASE
 ****************************************************************************************
 */

/// Full Database Description - Used to add attributes into the database
const gatt_att16_desc_t rscps_att_db[RSCS_IDX_NB] =
{
    // Running Speed and Cadence Service Declaration
    [RSCS_IDX_SVC]                = { GATT_DECL_PRIMARY_SERVICE,  PROP(RD),               0                                            },

    // RSC Measurement Characteristic Declaration
    [RSCS_IDX_RSC_MEAS_CHAR]      = { GATT_DECL_CHARACTERISTIC,   PROP(RD),               0                                            },
    // RSC Measurement Characteristic Value
    [RSCS_IDX_RSC_MEAS_VAL]       = { GATT_CHAR_RSC_MEAS,         PROP(N),                RSCP_RSC_MEAS_MAX_LEN                        },
    // RSC Measurement Characteristic - Client Characteristic Configuration Descriptor
    [RSCS_IDX_RSC_MEAS_NTF_CFG]   = { GATT_DESC_CLIENT_CHAR_CFG,  PROP(RD) | PROP(WR),    OPT(NO_OFFSET)                               },

    // RSC Feature Characteristic Declaration
    [RSCS_IDX_RSC_FEAT_CHAR]      = { GATT_DECL_CHARACTERISTIC,   PROP(RD),               0                                            },
    // RSC Feature Characteristic Value
    [RSCS_IDX_RSC_FEAT_VAL]       = { GATT_CHAR_RSC_FEAT,         PROP(RD),               OPT(NO_OFFSET)                               },

    // Sensor Location Characteristic Declaration
    [RSCS_IDX_SENSOR_LOC_CHAR]    = { GATT_DECL_CHARACTERISTIC,   PROP(RD),               0                                            },
    // Sensor Location Characteristic Value
    [RSCS_IDX_SENSOR_LOC_VAL]     = { GATT_CHAR_SENSOR_LOC,       PROP(RD),               OPT(NO_OFFSET)                               },

    // SC Control Point Characteristic Declaration
    [RSCS_IDX_SC_CTNL_PT_CHAR]    = { GATT_DECL_CHARACTERISTIC,   PROP(RD),               0                                            },
    // SC Control Point Characteristic Value - The response has the maximal length
    [RSCS_IDX_SC_CTNL_PT_VAL]     = { GATT_CHAR_SC_CNTL_PT,       PROP(I) | PROP(WR),     OPT(NO_OFFSET) | RSCP_SC_CNTL_PT_RSP_MAX_LEN },
    // SC Control Point Characteristic - Client Characteristic Configuration Descriptor
    [RSCS_IDX_SC_CTNL_PT_NTF_CFG] = { GATT_DESC_CLIENT_CHAR_CFG,  PROP(RD) | PROP(WR),    OPT(NO_OFFSET)                               },
};

/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */

/// Get database attribute index
__STATIC uint8_t rscps_idx_get(rscps_env_t* p_rscps_env, uint16_t hdl)
{
    uint8_t att_idx = hdl - p_rscps_env->start_hdl;

    if(   (att_idx >= RSCS_IDX_SENSOR_LOC_CHAR)
       && !RSCPS_IS_FEATURE_SUPPORTED(p_rscps_env->prf_cfg, RSCPS_SENSOR_LOC_MASK))
    {
        att_idx += 2;
    }

    return att_idx;
}

/**
 ****************************************************************************************
 * @brief  This function fully manages notification of measurement and vector
 ****************************************************************************************
 */
__STATIC void rscps_exe_operation(rscps_env_t* p_rscps_env)
{
    if(!p_rscps_env->in_exe_op)
    {
        p_rscps_env->in_exe_op = true;

        while(!co_list_is_empty(&(p_rscps_env->wait_queue)) && !(p_rscps_env->op_ongoing))
        {
            uint16_t status = GAP_ERR_NO_ERROR;
            co_buf_t* p_buf = (co_buf_t*) co_list_pop_front(&(p_rscps_env->wait_queue));
            rscps_buf_meta_t* p_meta = (rscps_buf_meta_t*) co_buf_metadata(p_buf);
            uint8_t conidx;

            switch(p_meta->operation)
            {
                case RSCPS_SEND_RSC_MEAS_OP_CODE:
                {
                    uint32_t conidx_bf = 0;

                    // check connection that support notification reception
                    for(conidx = 0 ; conidx < BLE_CONNECTION_MAX ; conidx++)
                    {
                        if(GETB(p_rscps_env->prfl_ntf_ind_cfg[conidx], RSCP_PRF_CFG_FLAG_RSC_MEAS_NTF))
                        {
                            conidx_bf |= CO_BIT(conidx);
                        }
                    }

                    // send notification only on selected connections
                    conidx_bf &= p_meta->conidx_bf;

                    if(conidx_bf != 0)
                    {
                        // send multi-point notification
                        status = gatt_srv_event_mtp_send(conidx_bf, p_rscps_env->user_lid, RSCPS_SEND_RSC_MEAS_OP_CODE,
                                                         GATT_NOTIFY, RSCPS_HANDLE(RSCS_IDX_RSC_MEAS_VAL), p_buf, true);
                        if(status == GAP_ERR_NO_ERROR)
                        {
                            p_rscps_env->op_ongoing = true;
                        }
                    }

                    co_buf_release(p_buf);

                    if(!p_rscps_env->op_ongoing)
                    {
                        const rscps_cb_t* p_cb = (const rscps_cb_t*) p_rscps_env->prf_env.p_cb;
                        // Inform application that event has been sent
                        p_cb->cb_meas_send_cmp(status);
                    }
                } break;

                default:
                {
                    conidx = p_meta->conidx;

                    status = gatt_srv_event_send(conidx, p_rscps_env->user_lid, p_meta->operation, GATT_INDICATE,
                                                 RSCPS_HANDLE(RSCS_IDX_SC_CTNL_PT_VAL), p_buf);

                    if(status == GAP_ERR_NO_ERROR)
                    {
                        p_rscps_env->op_ongoing = true;
                    }
                    else
                    {
                        // Inform application that control point response has been sent
                        if (p_rscps_env->ctrl_pt_op != RSCP_CTNL_PT_RSP_CODE)
                        {
                            const rscps_cb_t* p_cb = (const rscps_cb_t*) p_rscps_env->prf_env.p_cb;
                            p_cb->cb_ctnl_pt_rsp_send_cmp(conidx, status);
                        }

                        // consider control point operation done
                        p_rscps_env->ctrl_pt_op = RSCP_CTNL_PT_OP_RESERVED;
                    }

                    co_buf_release(p_buf);
                } break;
            }
        }

        p_rscps_env->in_exe_op = false;
    }
}


/**
 ****************************************************************************************
 * @brief Packs measurement data
 *
 * @param[in]     p_rscps_env   Environment data
 * @param[in]     p_buf         Pointer to output buffer
 * @param[in]     p_meas        pointer to measurement information
 ****************************************************************************************
 */
__STATIC void rscps_pack_meas(rscps_env_t *p_rscps_env, co_buf_t* p_buf, const rscp_rsc_meas_t *p_meas)
{
    uint8_t meas_flags = p_meas->flags;

    // Check the provided flags value
    if (!GETB(p_rscps_env->prf_cfg, RSCP_FEAT_INST_STRIDE_LEN_SUPP))
    {
        // Force Measurement Instantaneous Stride Length Present to No (Not supported)
        SETB(meas_flags, RSCP_MEAS_INST_STRIDE_LEN_PRESENT, 0);
    }

    if (!GETB(p_rscps_env->prf_cfg, RSCP_FEAT_TOTAL_DST_MEAS_SUPP))
    {
        // Force Total Distance Measurement Present to No (Not supported)
        SETB(meas_flags, RSCP_MEAS_TOTAL_DST_MEAS_PRESENT, 0);
    }

    if (!GETB(p_rscps_env->prf_cfg, RSCP_FEAT_WALK_RUN_STATUS_SUPP))
    {
        // Force Walking or Running Status Measurement Present to No (Not supported)
        SETB(meas_flags, RSCP_MEAS_WALK_RUN_STATUS, 0);
    }

    // Force the unused bits of the flag value to 0
    co_buf_tail(p_buf)[0] =  meas_flags & RSCP_MEAS_ALL_PRESENT;
    co_buf_tail_reserve(p_buf, 1);

    // Instantaneous Speed
    co_write16p(co_buf_tail(p_buf), co_htobs(p_meas->inst_speed));
    co_buf_tail_reserve(p_buf, 2);

    // Instantaneous Cadence
    co_buf_tail(p_buf)[0] = p_meas->inst_cad;
    co_buf_tail_reserve(p_buf, 1);

    // Instantaneous Stride Length
    if (GETB(meas_flags, RSCP_MEAS_INST_STRIDE_LEN_PRESENT))
    {
        co_write16p(co_buf_tail(p_buf), co_htobs(p_meas->inst_stride_len));
        co_buf_tail_reserve(p_buf, 2);
    }

    // Total Distance
    if (GETB(meas_flags, RSCP_MEAS_TOTAL_DST_MEAS_PRESENT))
    {
        co_write32p(co_buf_tail(p_buf), co_htobl(p_meas->total_dist));
        co_buf_tail_reserve(p_buf, 4);
    }
}

/**
 ****************************************************************************************
 * @brief Unpack control point data and process it
 *
 * @param[in] p_rscps_env Environment
 * @param[in] conidx     connection index
 * @param[in] p_buf      pointer to input data
 ****************************************************************************************
 */
__STATIC uint16_t rscps_unpack_ctnl_point_req(rscps_env_t *p_rscps_env, uint8_t conidx, co_buf_t* p_buf)
{
    uint8_t op_code;
    uint8_t ctnl_pt_rsp_status = RSCP_CTNL_PT_RESP_NOT_SUPP;
    uint16_t status = GAP_ERR_NO_ERROR;
    union rscp_sc_ctnl_pt_req_val value;
    memset(&value, 0, sizeof(union rscp_sc_ctnl_pt_req_val));
    op_code =  co_buf_data(p_buf)[0];

    if(co_buf_head_release(p_buf, 1) == CO_BUF_ERR_NO_ERROR)
    {
        // Operation Code
        switch (op_code)
        {
            case (RSCP_CTNL_PT_OP_SET_CUMUL_VAL):
            {
                // Check if the Total Distance Measurement feature is supported
                if (GETB(p_rscps_env->features, RSCP_FEAT_TOTAL_DST_MEAS_SUPP))
                {
                    // Provided parameter in not within the defined range
                    ctnl_pt_rsp_status = RSCP_CTNL_PT_RESP_INV_PARAM;

                    if (co_buf_data_len(p_buf) == 4)
                    {
                        // The request can be handled
                        ctnl_pt_rsp_status = RSCP_CTNL_PT_RESP_SUCCESS;
                        // Cumulative value
                        value.cumul_val = co_btohl(co_read32p(co_buf_data(p_buf)));
                    }
                }
            } break;

            // Start Sensor Calibration
            case (RSCP_CTNL_PT_OP_START_CALIB):
            {
                // Check if the Calibration Procedure feature is supported
                if (GETB(p_rscps_env->features, RSCP_FEAT_CALIB_PROC_SUPP))
                {
                    // The request can be handled
                    ctnl_pt_rsp_status = RSCP_CTNL_PT_RESP_SUCCESS;
                }
            } break;

            case (RSCP_CTNL_PT_OP_UPD_LOC):
            {
                // Check if the Multiple Sensor Location feature is supported
                if (GETB(p_rscps_env->features, RSCP_FEAT_MULT_SENSOR_LOC_SUPP))
                {
                    // Provided parameter in not within the defined range
                    ctnl_pt_rsp_status = RSCP_CTNL_PT_RESP_INV_PARAM;

                    if (co_buf_data_len(p_buf) == 1)
                    {
                        uint8_t sensor_loc = co_buf_data(p_buf)[0];

                        // Check the sensor location value
                        if (sensor_loc < RSCP_LOC_MAX)
                        {
                            value.sensor_loc = sensor_loc;
                            // The request can be handled
                            ctnl_pt_rsp_status = RSCP_CTNL_PT_RESP_SUCCESS;
                        }
                    }
                }
            } break;

            case (RSCP_CTNL_PT_OP_REQ_SUPP_LOC):
            {
                // Check if the Multiple Sensor Location feature is supported
                if (GETB(p_rscps_env->features, RSCP_FEAT_MULT_SENSOR_LOC_SUPP))
                {
                    // The request can be handled
                    ctnl_pt_rsp_status = RSCP_CTNL_PT_RESP_SUCCESS;
                }
            } break;

            default:
            {
                // Operation Code is invalid, status is already RSCP_CTNL_PT_RESP_NOT_SUPP
            } break;
        }

        // If no error raised, inform the application about the request
        if (ctnl_pt_rsp_status == RSCP_CTNL_PT_RESP_SUCCESS)
        {
            const rscps_cb_t* p_cb  = (const rscps_cb_t*) p_rscps_env->prf_env.p_cb;
            p_rscps_env->ctrl_pt_op = op_code;
            // inform application about control point request
            p_cb->cb_ctnl_pt_req(conidx, op_code, &value);
        }
        else
        {
            co_buf_t* p_out_buf = NULL;

            if(co_buf_alloc(&p_out_buf, GATT_BUFFER_HEADER_LEN, 0, RSCP_SC_CNTL_PT_RSP_MIN_LEN + GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
            {
                rscps_buf_meta_t* p_meta = (rscps_buf_meta_t*)co_buf_metadata(p_out_buf);

                p_rscps_env->ctrl_pt_op   = RSCP_CTNL_PT_RSP_CODE;
                co_buf_tail(p_out_buf)[0] = RSCP_CTNL_PT_RSP_CODE;
                co_buf_tail_reserve(p_out_buf, 1);
                co_buf_tail(p_out_buf)[0] = op_code;
                co_buf_tail_reserve(p_out_buf, 1);
                co_buf_tail(p_out_buf)[0] = ctnl_pt_rsp_status;
                co_buf_tail_reserve(p_out_buf, 1);

                p_meta->conidx    = conidx;
                p_meta->operation = RSCPS_CTNL_PT_RSP_SEND_OP_CODE;

                // put event on wait queue
                co_list_push_back(&(p_rscps_env->wait_queue), &(p_out_buf->hdr));
                // execute operation
                rscps_exe_operation(p_rscps_env);
            }
            else
            {
                status = ATT_ERR_INSUFF_RESOURCE;
            }
        }
    }

    return (status);
}


/**
 ****************************************************************************************
 * @brief Packs control point response
 * @param[in]     p_rscps_env   Environment data
 * @param[in]     conidx       Connection Index
 * @param[in]     p_buf        Pointer to output buffer
 * @param[in]     op_code      Operation Code
 * @param[in]     resp_val     Operation result
 * @param[in]     p_value      Pointer to operation response parameters
 ****************************************************************************************
 */
void rscps_pack_ctnl_point_rsp(rscps_env_t *p_rscps_env, uint8_t conidx, co_buf_t* p_buf, uint8_t op_code, uint8_t resp_val,
                               const union rscp_sc_ctnl_pt_rsp_val* p_value)
{
    // Set the Response Code
    co_buf_tail(p_buf)[0] = RSCP_CTNL_PT_RSP_CODE;
    co_buf_tail_reserve(p_buf, 1);

    // Set the request operation code
    co_buf_tail(p_buf)[0] = p_rscps_env->ctrl_pt_op;
    co_buf_tail_reserve(p_buf, 1);

    if (resp_val == RSCP_CTNL_PT_RESP_SUCCESS)
    {
        co_buf_tail(p_buf)[0] = resp_val;
        co_buf_tail_reserve(p_buf, 1);

        switch (p_rscps_env->ctrl_pt_op)
        {
            case (RSCP_CTNL_PT_OP_UPD_LOC):
            {
                // Store the new value in the environment
                p_rscps_env->sensor_loc = p_value->sensor_loc;
            } break;

            case (RSCP_CTNL_PT_OP_REQ_SUPP_LOC):
            {
                // Set the list of supported location
                for (uint8_t counter = 0; counter < RSCP_LOC_MAX; counter++)
                {
                    if ((p_value->supp_sensor_loc >> counter) & 0x0001)
                    {
                        co_buf_tail(p_buf)[0] = counter;
                        co_buf_tail_reserve(p_buf, 1);
                    }
                }
            } break;
            default: { /* Nothing to do */ } break;
        }
    }
    else
    {
        // Set the Response Value
        co_buf_tail(p_buf)[0] = (resp_val > RSCP_CTNL_PT_RESP_FAILED) ? RSCP_CTNL_PT_RESP_FAILED : resp_val;
        co_buf_tail_reserve(p_buf, 1);
    }
}
/*
 * GATT USER SERVICE HANDLERS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief This function is called when peer want to read local attribute database value.
 *
 *        @see gatt_srv_att_read_get_cfm shall be called to provide attribute value
 *
 * @param[in] conidx        Connection index
 * @param[in] user_lid      GATT user local identifier
 * @param[in] token         Procedure token that must be returned in confirmation function
 * @param[in] hdl           Attribute handle
 * @param[in] offset        Data offset
 * @param[in] max_length    Maximum data length to return
 ****************************************************************************************
 */
__STATIC void rscps_cb_att_read_get(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset,
                                   uint16_t max_length)
{
    rscps_env_t *p_rscps_env = PRF_ENV_GET(RSCPS, rscps);
    // retrieve value attribute
    co_buf_t* p_buf       = NULL;
    uint16_t  status      = GAP_ERR_NO_ERROR;
    uint16_t  att_val_len = 0;

    if(p_rscps_env == NULL)
    {
        status = PRF_APP_ERROR;
    }
    else if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, 0,  sizeof(uint16_t) + GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
    {
        uint8_t att_idx = rscps_idx_get(p_rscps_env, hdl);

        switch (att_idx)
        {
            case RSCS_IDX_RSC_MEAS_NTF_CFG:
            {
                uint16_t ntf_cfg = GETB(p_rscps_env->prfl_ntf_ind_cfg[conidx], RSCP_PRF_CFG_FLAG_RSC_MEAS_NTF)
                                 ? PRF_CLI_START_NTF : PRF_CLI_STOP_NTFIND;

                co_write16p(co_buf_tail(p_buf), co_htobs(ntf_cfg));
                co_buf_tail_reserve(p_buf, 2);
            } break;

            case RSCS_IDX_SC_CTNL_PT_NTF_CFG:
            {
                uint16_t ind_cfg = GETB(p_rscps_env->prfl_ntf_ind_cfg[conidx], RSCP_PRF_CFG_FLAG_SC_CTNL_PT_IND)
                                 ? PRF_CLI_START_IND : PRF_CLI_STOP_NTFIND;

                co_write16p(co_buf_tail(p_buf), co_htobs(ind_cfg));
                co_buf_tail_reserve(p_buf, 2);
            } break;

            case RSCS_IDX_RSC_FEAT_VAL:
            {
                co_write16p(co_buf_tail(p_buf), co_htobs(p_rscps_env->features));
                co_buf_tail_reserve(p_buf, 2);
            } break;

            case RSCS_IDX_SENSOR_LOC_VAL:
            {
                co_buf_tail(p_buf)[0] = p_rscps_env->sensor_loc;
                co_buf_tail_reserve(p_buf, 1);
            } break;

            default:
            {
                status = ATT_ERR_REQUEST_NOT_SUPPORTED;
            } break;
        }

        att_val_len = co_buf_data_len(p_buf);
    }
    else
    {
        status = ATT_ERR_INSUFF_RESOURCE;
    }


    gatt_srv_att_read_get_cfm(conidx, user_lid, token, status, att_val_len, p_buf);
    if(p_buf != NULL)
    {
        co_buf_release(p_buf);
    }
}


/**
 ****************************************************************************************
 * @brief This function is called during a write procedure to modify attribute handle.
 *
 *        @see gatt_srv_att_val_set_cfm shall be called to accept or reject attribute
 *        update.
 *
 * @param[in] conidx        Connection index
 * @param[in] user_lid      GATT user local identifier
 * @param[in] token         Procedure token that must be returned in confirmation function
 * @param[in] hdl           Attribute handle
 * @param[in] offset        Value offset
 * @param[in] p_data        Pointer to buffer that contains data to write starting from offset
 ****************************************************************************************
 */
__STATIC void rscps_cb_att_val_set(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset,
                                   co_buf_t* p_data)
{
    rscps_env_t *p_rscps_env = PRF_ENV_GET(RSCPS, rscps);
    uint16_t  status      = PRF_APP_ERROR;

    if(p_rscps_env != NULL)
    {
        uint8_t cfg_upd_flag  = 0;
        uint8_t cfg_upd_char  = 0;
        uint16_t cfg_en_val = 0;

        switch (rscps_idx_get(p_rscps_env, hdl))
        {
            case RSCS_IDX_RSC_MEAS_NTF_CFG:
            {
                cfg_upd_char = RSCP_RSCS_RSC_MEAS_CHAR;
                cfg_upd_flag = RSCP_PRF_CFG_FLAG_RSC_MEAS_NTF_BIT;
                cfg_en_val   = PRF_CLI_START_NTF;
            } break;

            case RSCS_IDX_SC_CTNL_PT_NTF_CFG:
            {
                cfg_upd_char = RSCP_RSCS_SC_CTNL_PT_CHAR;
                cfg_upd_flag = RSCP_PRF_CFG_FLAG_SC_CTNL_PT_IND_BIT;
                cfg_en_val   = PRF_CLI_START_IND;
            } break;

            case RSCS_IDX_SC_CTNL_PT_VAL:
            {
                // Check if sending of indications has been enabled
                if (!GETB(p_rscps_env->prfl_ntf_ind_cfg[conidx], RSCP_PRF_CFG_FLAG_SC_CTNL_PT_IND))
                {
                    // CPP improperly configured
                    status = RSCP_ERROR_CCC_INVALID_PARAM;
                }
                else if (p_rscps_env->ctrl_pt_op != RSCP_CTNL_PT_OP_RESERVED)
                {
                    // A procedure is already in progress
                    status = RSCP_ERROR_PROC_IN_PROGRESS;
                }
                else
                {
                    // Unpack Control Point parameters
                    status = rscps_unpack_ctnl_point_req(p_rscps_env, conidx, p_data);
                }
            } break;

            default: { status = ATT_ERR_REQUEST_NOT_SUPPORTED; } break;
        }

        if(cfg_upd_flag != 0)
        {
            uint16_t cfg = co_btohs(co_read16p(co_buf_data(p_data)));

            // parameter check
            if(   (co_buf_data_len(p_data) == sizeof(uint16_t))
               && ((cfg == PRF_CLI_STOP_NTFIND) || (cfg == cfg_en_val)))
            {
                const rscps_cb_t* p_cb  = (const rscps_cb_t*) p_rscps_env->prf_env.p_cb;

                if(cfg == PRF_CLI_STOP_NTFIND)
                {
                    p_rscps_env->prfl_ntf_ind_cfg[conidx] &= ~cfg_upd_flag;
                }
                else
                {
                    p_rscps_env->prfl_ntf_ind_cfg[conidx] |= cfg_upd_flag;
                }

                // inform application about update
                p_cb->cb_bond_data_upd(conidx, cfg_upd_char, cfg);
                status = GAP_ERR_NO_ERROR;
            }
            else
            {
                status = PRF_CCCD_IMPR_CONFIGURED;
            }
        }
    }

    gatt_srv_att_val_set_cfm(conidx, user_lid, token, status);
}

/**
 ****************************************************************************************
 * @brief This function is called when GATT server user has initiated event send to peer
 *        device or if an error occurs.
 *
 * @param[in] conidx        Connection index
 * @param[in] user_lid      GATT user local identifier
 * @param[in] dummy         Dummy parameter provided by upper layer for command execution
 * @param[in] status        Status of the procedure (@see enum hl_err)
 ****************************************************************************************
 */
__STATIC void rscps_cb_event_sent(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    // Consider job done
    rscps_env_t *p_rscps_env = PRF_ENV_GET(RSCPS, rscps);
    if(p_rscps_env != NULL)
    {
        const rscps_cb_t* p_cb  = (const rscps_cb_t*) p_rscps_env->prf_env.p_cb;
        p_rscps_env->op_ongoing = false;

        switch(dummy)
        {
            case RSCPS_SEND_RSC_MEAS_OP_CODE:
            {
                p_cb->cb_meas_send_cmp(status);
            } break;
            case RSCPS_CTNL_PT_RSP_SEND_OP_CODE:
            {
                // Inform application that control point response has been sent
                if (p_rscps_env->ctrl_pt_op != RSCP_CTNL_PT_RSP_CODE)
                {
                    p_cb->cb_ctnl_pt_rsp_send_cmp(conidx, status);
                }

                p_rscps_env->ctrl_pt_op = RSCP_CTNL_PT_OP_RESERVED;
            } break;
            default: { /* Nothing to do */ } break;
        }

        // continue operation execution
        rscps_exe_operation(p_rscps_env);
    }
}

/// Service callback hander
__STATIC const gatt_srv_cb_t rscps_cb =
{
        .cb_event_sent    = rscps_cb_event_sent,
        .cb_att_read_get  = rscps_cb_att_read_get,
        .cb_att_event_get = NULL,
        .cb_att_info_get  = NULL,
        .cb_att_val_set   = rscps_cb_att_val_set,
};

/*
 * PROFILE NATIVE HANDLERS
 ****************************************************************************************
 */

uint16_t rscps_enable(uint8_t conidx, uint16_t rsc_meas_ntf_cfg, uint16_t sc_ctnl_pt_ntf_cfg)
{
    rscps_env_t* p_rscps_env = PRF_ENV_GET(RSCPS, rscps);
    uint16_t status = PRF_ERR_REQ_DISALLOWED;

    if(p_rscps_env != NULL)
    {
        // check state of the task
        if (gapc_get_conhdl(conidx) != GAP_INVALID_CONHDL)
        {
            // Check the provided value
            if (rsc_meas_ntf_cfg == PRF_CLI_START_NTF)
            {
                // Store the status
                SETB(p_rscps_env->prfl_ntf_ind_cfg[conidx], RSCP_PRF_CFG_FLAG_RSC_MEAS_NTF, 1);
            }

            if (RSCPS_IS_FEATURE_SUPPORTED(p_rscps_env->prf_cfg, RSCPS_SC_CTNL_PT_MASK))
            {
                // Check the provided value
                if (sc_ctnl_pt_ntf_cfg == PRF_CLI_START_IND)
                {
                    // Store the status
                    SETB(p_rscps_env->prfl_ntf_ind_cfg[conidx], RSCP_PRF_CFG_FLAG_SC_CTNL_PT_IND, 1);
                }
            }

            // Enable Bonded Data
            SETB(p_rscps_env->prfl_ntf_ind_cfg[conidx], RSCP_PRF_CFG_PERFORMED_OK, 1);


            status = GAP_ERR_NO_ERROR;
        }
    }

    return (status);

}

uint16_t rscps_meas_send(uint32_t conidx_bf, const rscp_rsc_meas_t* p_meas)
{
    rscps_env_t* p_rscps_env = PRF_ENV_GET(RSCPS, rscps);
    uint16_t status = PRF_ERR_REQ_DISALLOWED;

    if(p_meas == NULL)
    {
        status = GAP_ERR_INVALID_PARAM;
    }
    else if(p_rscps_env != NULL)
    {
        co_buf_t* p_buf;

        if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, 0, RSCP_RSC_MEAS_MAX_LEN + GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
        {
            rscps_buf_meta_t* p_buf_meta = (rscps_buf_meta_t*) co_buf_metadata(p_buf);
            p_buf_meta->operation = RSCPS_SEND_RSC_MEAS_OP_CODE;
            p_buf_meta->conidx    = GAP_INVALID_CONIDX;
            p_buf_meta->conidx_bf = conidx_bf & ((1 << BLE_CONNECTION_MAX) - 1);

            // pack measurement
            rscps_pack_meas(p_rscps_env, p_buf, p_meas);
            // put event on wait queue
            co_list_push_back(&(p_rscps_env->wait_queue), &(p_buf->hdr));
            // execute operation
            rscps_exe_operation(p_rscps_env);
            status = GAP_ERR_NO_ERROR;
        }
        else
        {
            status = GAP_ERR_INSUFF_RESOURCES;
        }
    }

    return (status);
}

uint16_t rscps_ctnl_pt_rsp_send(uint8_t conidx, uint8_t op_code, uint8_t resp_val,
                                const union rscp_sc_ctnl_pt_rsp_val* p_value)
{
    rscps_env_t* p_rscps_env = PRF_ENV_GET(RSCPS, rscps);
    uint16_t status = PRF_ERR_REQ_DISALLOWED;

    if(p_value == NULL)
    {
        status = GAP_ERR_INVALID_PARAM;
    }
    else if(p_rscps_env != NULL)
    {
        do
        {
            co_buf_t* p_buf = NULL;

            // Check the current operation
            if (p_rscps_env->ctrl_pt_op == RSCP_CTNL_PT_OP_RESERVED)
            {
                // The confirmation has been sent without request indication, ignore
                break;
            }

            // The CP Control Point Characteristic must be supported if we are here
            if (!RSCPS_IS_FEATURE_SUPPORTED(p_rscps_env->prf_cfg, RSCPS_SC_CTNL_PT_MASK))
            {
                status = PRF_ERR_REQ_DISALLOWED;
                break;
            }

            // Check if sending of indications has been enabled
            if (!GETB(p_rscps_env->prfl_ntf_ind_cfg[conidx], RSCP_PRF_CFG_FLAG_SC_CTNL_PT_IND))
            {
                // mark operation done
                p_rscps_env->ctrl_pt_op = RSCP_CTNL_PT_OP_RESERVED;
                // CPP improperly configured
                status = PRF_CCCD_IMPR_CONFIGURED;
                break;
            }

            if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, 0, RSCP_SC_CNTL_PT_RSP_MAX_LEN + GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
            {
                rscps_buf_meta_t* p_buf_meta = (rscps_buf_meta_t*) co_buf_metadata(p_buf);
                p_buf_meta->operation = RSCPS_CTNL_PT_RSP_SEND_OP_CODE;
                p_buf_meta->conidx    = conidx;

                // Pack structure
                rscps_pack_ctnl_point_rsp(p_rscps_env, conidx, p_buf, op_code, resp_val, p_value);
                // put event on wait queue
                co_list_push_back(&(p_rscps_env->wait_queue), &(p_buf->hdr));
                // execute operation
                rscps_exe_operation(p_rscps_env);
                status = GAP_ERR_NO_ERROR;
            }
            else
            {
                status = GAP_ERR_INSUFF_RESOURCES;
            }

        } while(0);
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
 * @brief Send a RSCPS_CMP_EVT message to the application.
 * @param[in] conidx connection index
 * @param[in] operation Operation completed
 * @param[in] status status of the operation
 ****************************************************************************************
 */
__STATIC void rscps_send_cmp_evt(uint8_t conidx, uint8_t operation, uint16_t status)
{
    struct rscps_cmp_evt *p_evt;

    // Send the message
    p_evt = KE_MSG_ALLOC(RSCPS_CMP_EVT, PRF_DST_TASK(RSCPS), PRF_SRC_TASK(RSCPS), rscps_cmp_evt);

    if(p_evt != NULL)
    {
        p_evt->conidx     = conidx;
        p_evt->operation  = operation;
        p_evt->status     = status;
        ke_msg_send(p_evt);
    }
}


/**
 ****************************************************************************************
 * @brief Handles reception of the @ref RSCPS_ENABLE_REQ message.
 * @param[in] msgid Id of the message received
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int rscps_enable_req_handler(ke_msg_id_t const msgid, struct rscps_enable_req *p_param,
                                      ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    struct rscps_enable_rsp *p_cmp_evt;
    uint16_t status = rscps_enable(p_param->conidx, p_param->rsc_meas_ntf_cfg, p_param->sc_ctnl_pt_ntf_cfg);

    // send completed information to APP task that contains error status
    p_cmp_evt = KE_MSG_ALLOC(RSCPS_ENABLE_RSP, src_id, dest_id, rscps_enable_rsp);

    if(p_cmp_evt)
    {
        p_cmp_evt->conidx     = p_param->conidx;
        p_cmp_evt->status     = status;
        ke_msg_send(p_cmp_evt);
    }


    return (KE_MSG_CONSUMED);
}


/**
 ****************************************************************************************
 * @brief Handles reception of the @ref RSCPS_NTF_RSC_MEAS_CMD message.
 * @param[in] msgid Id of the message received.
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int rscps_ntf_rsc_meas_cmd_handler(ke_msg_id_t const msgid, struct rscps_ntf_rsc_meas_cmd *p_param,
                                            ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = rscps_meas_send(p_param->conidx_bf, &(p_param->rsc_meas));

    if(status != GAP_ERR_NO_ERROR)
    {
        rscps_send_cmp_evt(GAP_INVALID_CONIDX, RSCPS_SEND_RSC_MEAS_OP_CODE, status);
    }

    return (KE_MSG_CONSUMED);
}


/**
 ****************************************************************************************
 * @brief Handles reception of the @ref RSCPS_SC_CTNL_PT_RSP_SEND_CMD message.
 * @param[in] msgid Id of the message received.
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int rscps_sc_ctnl_pt_rsp_send_cmd_handler(ke_msg_id_t const msgid, struct rscps_sc_ctnl_pt_rsp_send_cmd *p_param,
                                                   ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = rscps_ctnl_pt_rsp_send(p_param->conidx, p_param->op_code, p_param->resp_value, &(p_param->value));

    if(status != GAP_ERR_NO_ERROR)
    {
        rscps_send_cmp_evt(p_param->conidx, RSCPS_CTNL_PT_RSP_SEND_OP_CODE, status);
    }

    return (KE_MSG_CONSUMED);
}

/// Default State handlers definition
KE_MSG_HANDLER_TAB(rscps)
{
    // Note: all messages must be sorted in ID ascending order

    {RSCPS_ENABLE_REQ,              (ke_msg_func_t) rscps_enable_req_handler},
    {RSCPS_NTF_RSC_MEAS_CMD,        (ke_msg_func_t) rscps_ntf_rsc_meas_cmd_handler},
    {RSCPS_SC_CTNL_PT_RSP_SEND_CMD, (ke_msg_func_t) rscps_sc_ctnl_pt_rsp_send_cmd_handler},
};

/**
 ****************************************************************************************
 * @brief Completion of measurement transmission
 *
 * @param[in] status Status of the procedure execution (@see enum hl_err)
 ****************************************************************************************
 */
__STATIC void rscps_cb_meas_send_cmp(uint16_t status)
{
    rscps_send_cmp_evt(GAP_INVALID_CONIDX, RSCPS_SEND_RSC_MEAS_OP_CODE, status);
}

/**
 ****************************************************************************************
 * @brief Inform that bond data updated for the connection.
 *
 * @param[in] conidx        Connection index
 * @param[in] char_code     Characteristic Code (CP Measurement or Control Point)
 * @param[in] cfg_val       Stop/notify/indicate value to configure into the peer characteristic
 ****************************************************************************************
 */
__STATIC void rscps_cb_bond_data_upd(uint8_t conidx, uint8_t char_code, uint16_t cfg_val)
{
    struct rscps_cfg_ntfind_ind *p_evt;

    // Send the message
    p_evt = KE_MSG_ALLOC(RSCPS_CFG_NTFIND_IND, PRF_DST_TASK(RSCPS), PRF_SRC_TASK(RSCPS), rscps_cfg_ntfind_ind);

    if(p_evt != NULL)
    {
        p_evt->conidx     = conidx;
        p_evt->char_code  = char_code;
        p_evt->ntf_cfg    = cfg_val;
        ke_msg_send(p_evt);
    }
}

/**
 ****************************************************************************************
 * @brief Inform that peer device requests an action using control point
 *
 * @note control point request must be answered using @see rscps_ctnl_pt_rsp_send function
 *
 * @param[in] conidx        Connection index
 * @param[in] op_code       Operation Code (@see enum rscp_sc_ctnl_pt_code)
 * @param[in] p_value       Pointer to control point request value
 ****************************************************************************************
 */
__STATIC void rscps_cb_ctnl_pt_req(uint8_t conidx, uint8_t op_code, const union rscp_sc_ctnl_pt_req_val* p_value)
{
    struct rscps_sc_ctnl_pt_req_recv_ind *p_evt;

    // Send the message
    p_evt = KE_MSG_ALLOC(RSCPS_SC_CTNL_PT_REQ_RECV_IND, PRF_DST_TASK(RSCPS),
                         PRF_SRC_TASK(RSCPS), rscps_sc_ctnl_pt_req_recv_ind);

    if(p_evt != NULL)
    {
        p_evt->conidx     = conidx;
        p_evt->op_code    = op_code;
        memcpy(&(p_evt->value), p_value, sizeof(union rscp_sc_ctnl_pt_req_val));
        ke_msg_send(p_evt);
    }
}

/**
 ****************************************************************************************
 * @brief Completion of control point response send procedure
 *
 * @param[in] status Status of the procedure execution (@see enum hl_err)
 ****************************************************************************************
 */
__STATIC void rscps_cb_ctnl_pt_rsp_send_cmp(uint8_t conidx, uint16_t status)
{
    rscps_send_cmp_evt(conidx, RSCPS_CTNL_PT_RSP_SEND_OP_CODE, status);
}

/// Default Message handle
__STATIC const rscps_cb_t rscps_msg_cb =
{
        .cb_meas_send_cmp        = rscps_cb_meas_send_cmp,
        .cb_bond_data_upd        = rscps_cb_bond_data_upd,
        .cb_ctnl_pt_req          = rscps_cb_ctnl_pt_req,
        .cb_ctnl_pt_rsp_send_cmp = rscps_cb_ctnl_pt_rsp_send_cmp,
};
#endif // (BLE_HL_MSG_API)

/*
 * PROFILE DEFAULT HANDLERS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Initialization of the RSCPS module.
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
__STATIC uint16_t rscps_init(prf_data_t *p_env, uint16_t *p_start_hdl, uint8_t sec_lvl, uint8_t user_prio,
                          struct rscps_db_cfg *p_params, const rscps_cb_t* p_cb)
{
    //------------------ create the attribute database for the profile -------------------

    // DB Creation Status
    uint16_t status = GAP_ERR_NO_ERROR;
    uint8_t user_lid = GATT_INVALID_USER_LID;

    do
    {
        rscps_env_t* p_rscps_env;
        // Service content flag
        uint32_t cfg_flag = RSCPS_MANDATORY_MASK;

        #if (BLE_HL_MSG_API)
        if(p_cb == NULL)
        {
            p_cb = &(rscps_msg_cb);
        }
        #endif // (BLE_HL_MSG_API)

        if(   (p_params == NULL) || (p_start_hdl == NULL) || (p_cb == NULL) || (p_cb->cb_meas_send_cmp == NULL)
           || (p_cb->cb_bond_data_upd == NULL) || (p_cb->cb_ctnl_pt_req == NULL) || (p_cb->cb_ctnl_pt_rsp_send_cmp == NULL))
        {
            status = GAP_ERR_INVALID_PARAM;
            break;
        }

        // register RSCPS user
        status = gatt_user_srv_register(L2CAP_LE_MTU_MIN, user_prio, &rscps_cb, &user_lid);
        if(status != GAP_ERR_NO_ERROR) break;

        /*
         * Check if the Sensor Location characteristic shall be added.
         * Mandatory if the Multiple Sensor Location feature is supported, otherwise optional.
         */
        if (   (p_params->sensor_loc_supp == RSCPS_SENSOR_LOC_SUPP)
            || (GETB(p_params->rsc_feature, RSCP_FEAT_MULT_SENSOR_LOC_SUPP)))
        {
            cfg_flag |= RSCPS_SENSOR_LOC_MASK;
        }

        /*
         * Check if the SC Control Point characteristic shall be added
         * Mandatory if at least one SC Control Point procedure is supported, otherwise excluded.
         */
        if (GETB(p_params->rsc_feature, RSCP_FEAT_CALIB_PROC_SUPP) ||
            GETB(p_params->rsc_feature, RSCP_FEAT_MULT_SENSOR_LOC_SUPP) ||
            GETB(p_params->rsc_feature, RSCP_FEAT_TOTAL_DST_MEAS_SUPP))
        {
            cfg_flag |= RSCPS_SC_CTNL_PT_MASK;
        }


        // Add GAP service
        status = gatt_db_svc16_add(user_lid, sec_lvl, GATT_SVC_RUNNING_SPEED_CADENCE, RSCS_IDX_NB,
                                   (uint8_t *)&cfg_flag, &(rscps_att_db[0]), RSCS_IDX_NB, p_start_hdl);
        if(status != GAP_ERR_NO_ERROR) break;

        //-------------------- allocate memory required for the profile  ---------------------
        p_rscps_env = (rscps_env_t *) ke_malloc(sizeof(rscps_env_t), KE_MEM_ATT_DB);

        if(p_rscps_env != NULL)
        {
            // allocate RSCPS required environment variable
            p_env->p_env = (prf_hdr_t *) p_rscps_env;
            p_rscps_env->start_hdl       = *p_start_hdl;
            p_rscps_env->features        = p_params->rsc_feature;
            p_rscps_env->user_lid        = user_lid;
            p_rscps_env->prf_cfg         = cfg_flag;
            p_rscps_env->op_ongoing      = false;
            p_rscps_env->in_exe_op       = false;
            p_rscps_env->ctrl_pt_op      = RSCPS_RESERVED_OP_CODE;
            memset(p_rscps_env->prfl_ntf_ind_cfg, 0, sizeof(p_rscps_env->prfl_ntf_ind_cfg));
            co_list_init(&(p_rscps_env->wait_queue));

            if (RSCPS_IS_FEATURE_SUPPORTED(p_rscps_env->prf_cfg, RSCPS_SENSOR_LOC_MASK))
            {
                p_rscps_env->sensor_loc = (p_params->sensor_loc >= RSCP_LOC_MAX) ?
                        RSCP_LOC_OTHER : p_params->sensor_loc;
            }

            // initialize profile environment variable
            p_rscps_env->prf_env.p_cb     = p_cb;
            #if (BLE_HL_MSG_API)
            p_env->desc.msg_handler_tab  = rscps_msg_handler_tab;
            p_env->desc.msg_cnt          = ARRAY_LEN(rscps_msg_handler_tab);
            #endif // (BLE_HL_MSG_API)
        }
        else
        {
            status = GAP_ERR_INSUFF_RESOURCES;
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
__STATIC uint16_t rscps_destroy(prf_data_t *p_env, uint8_t reason)
{
    uint16_t status = GAP_ERR_NO_ERROR;
    rscps_env_t *p_rscps_env = (rscps_env_t *) p_env->p_env;

    if(reason != PRF_DESTROY_RESET)
    {
        status = gatt_user_unregister(p_rscps_env->user_lid);
    }

    if(status == GAP_ERR_NO_ERROR)
    {
        if(reason != PRF_DESTROY_RESET)
        {
            // remove buffer in wait queue
            while(!co_list_is_empty(&p_rscps_env->wait_queue))
            {
                co_buf_t* p_buf = (co_buf_t*) co_list_pop_front(&p_rscps_env->wait_queue);
                co_buf_release(p_buf);
            }
        }

        // free profile environment variables
        p_env->p_env = NULL;
        ke_free(p_rscps_env);
    }

    return (status);
}

/**
 ****************************************************************************************
 * @brief @brief Handles Connection creation
 *
 * @param[in|out]    env          Collector or Service allocated environment data.
 * @param[in]        conidx       Connection index
 * @param[in]        p_con_param  Pointer to connection parameters information
 ****************************************************************************************
 */
__STATIC void rscps_con_create(prf_data_t *p_env, uint8_t conidx, const gap_con_param_t* p_con_param)
{
    rscps_env_t *p_rscps_env = (rscps_env_t *) p_env->p_env;
    p_rscps_env->prfl_ntf_ind_cfg[conidx] = 0;
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
__STATIC void rscps_con_cleanup(prf_data_t *p_env, uint8_t conidx, uint16_t reason)
{
    rscps_env_t *p_rscps_env = (rscps_env_t *) p_env->p_env;
    p_rscps_env->prfl_ntf_ind_cfg[conidx] = 0;
}

/// RSCPS Task interface required by profile manager
const prf_task_cbs_t rscps_itf =
{
    .cb_init          = (prf_init_cb) rscps_init,
    .cb_destroy       = rscps_destroy,
    .cb_con_create    = rscps_con_create,
    .cb_con_cleanup   = rscps_con_cleanup,
    .cb_con_upd       = NULL,
};

/**
 ****************************************************************************************
 * @brief Retrieve service profile interface
 *
 * @return service profile interface
 ****************************************************************************************
 */
const prf_task_cbs_t *rscps_prf_itf_get(void)
{
    return &rscps_itf;
}

#endif //(BLE_RSC_SENSOR)

/// @} RSCPS

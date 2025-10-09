/**
 ****************************************************************************************
 *
 * @file cscps.c
 *
 * @brief Cycling Speed and Cadence Profile Sensor implementation.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup CSCPS
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "btip_config.h"

#if (BLE_CSC_SENSOR)
#include "cscp_common.h"

#include "cscps.h"
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
 ******* CSCPS Configuration Flag Masks ******
 ********************************************/

/// Mandatory Attributes (CSC Measurement + CSC Feature)
#define CSCPS_MANDATORY_MASK               (0x003F)
/// Sensor Location Attributes
#define CSCPS_SENSOR_LOC_MASK              (0x00C0)
/// SC Control Point Attributes
#define CSCPS_SC_CTNL_PT_MASK              (0x0700)


/// Cycling Speed and Cadence Service - Attribute List
enum cscps_cscs_att_list
{
    /// Cycling Speed and Cadence Service
    CSCS_IDX_SVC,
    /// CSC Measurement
    CSCS_IDX_CSC_MEAS_CHAR,
    CSCS_IDX_CSC_MEAS_VAL,
    CSCS_IDX_CSC_MEAS_NTF_CFG,
    /// CSC Feature
    CSCS_IDX_CSC_FEAT_CHAR,
    CSCS_IDX_CSC_FEAT_VAL,
    /// Sensor Location
    CSCS_IDX_SENSOR_LOC_CHAR,
    CSCS_IDX_SENSOR_LOC_VAL,
    /// SC Control Point
    CSCS_IDX_SC_CTNL_PT_CHAR,
    CSCS_IDX_SC_CTNL_PT_VAL,
    CSCS_IDX_SC_CTNL_PT_NTF_CFG,

    /// Number of attributes
    CSCS_IDX_NB,
};

/// Profile Configuration Additional Flags bit field
enum cscps_prf_cfg_flag_bf
{
    /// CSC Measurement - Client Char. Cfg
    CSCP_PRF_CFG_FLAG_CSC_MEAS_NTF_POS = 3,
    CSCP_PRF_CFG_FLAG_CSC_MEAS_NTF_BIT = CO_BIT(CSCP_PRF_CFG_FLAG_CSC_MEAS_NTF_POS),

    /// SC Control Point - Client Char. Cfg
    CSCP_PRF_CFG_FLAG_SC_CTNL_PT_IND_POS = 4,
    CSCP_PRF_CFG_FLAG_SC_CTNL_PT_IND_BIT = CO_BIT(CSCP_PRF_CFG_FLAG_SC_CTNL_PT_IND_POS),

    /// Bonded data used
    CSCP_PRF_CFG_PERFORMED_OK_POS = 7,
    CSCP_PRF_CFG_PERFORMED_OK_BIT = CO_BIT(CSCP_PRF_CFG_PERFORMED_OK_POS),
};

/// Sensor Location Supported Flag
enum cscps_sensor_loc_supp
{
    /// Sensor Location Char. is not supported
    CSCPS_SENSOR_LOC_NOT_SUPP,
    /// Sensor Location Char. is supported
    CSCPS_SENSOR_LOC_SUPP,
};

/*
 * MACROS
 ****************************************************************************************
 */

#define CSCPS_IS_FEATURE_SUPPORTED(features, flag) ((features & flag) == flag)

#define CSCPS_HANDLE(idx) \
    (p_cscps_env->start_hdl + (idx) - \
        ((!(CSCPS_IS_FEATURE_SUPPORTED(p_cscps_env->prfl_cfg, CSCPS_SENSOR_LOC_MASK)) && \
                ((idx) > CSCS_IDX_SENSOR_LOC_CHAR))? (1) : (0)))


/*
 * TYPES DEFINITION
 ****************************************************************************************
 */


/// ongoing operation information
typedef struct cscps_buf_meta
{
    /// meaningful for some operation
    uint32_t  conidx_bf;
    /// Operation
    uint8_t   operation;
    /// Connection index targeted
    uint8_t   conidx;
} cscps_buf_meta_t;

/// cycling speed and cadence server environment variable
typedef struct cscps_env
{
    /// profile environment
    prf_hdr_t prf_env;
    /// Operation Event TX wait queue
    co_list_t wait_queue;
    /// Wheel revolution
    uint32_t  tot_wheel_rev;
    /// Service Attribute Start Handle
    uint16_t  start_hdl;
    /// Services features
    uint16_t  features;
    /// profile configuration
    uint16_t  prfl_cfg;
    /// Sensor location
    uint8_t   sensor_loc;
    /// GATT user local identifier
    uint8_t   user_lid;
    /// Control point operation on-going (@see enum cscp_sc_ctnl_pt_op_code)
    uint8_t   ctrl_pt_op;
    /// Operation On-going
    bool      op_ongoing;
    /// Prevent recursion in execute_operation function
    bool      in_exe_op;
    /// Environment variable pointer for each connections
    uint8_t   prfl_ntf_ind_cfg[BLE_CONNECTION_MAX];

} cscps_env_t;


/*
 * ATTRIBUTES DATABASE
 ****************************************************************************************
 */

/// Full Database Description - Used to add attributes into the database
const gatt_att16_desc_t cscps_att_db[CSCS_IDX_NB] =
{
    // Cycling Speed and Cadence Service Declaration
    [CSCS_IDX_SVC]                = { GATT_DECL_PRIMARY_SERVICE,  PROP(RD),               0                                            },

    // CSC Measurement Characteristic Declaration
    [CSCS_IDX_CSC_MEAS_CHAR]      = { GATT_DECL_CHARACTERISTIC,   PROP(RD),               0                                            },
    // CSC Measurement Characteristic Value
    [CSCS_IDX_CSC_MEAS_VAL]       = { GATT_CHAR_CSC_MEAS,         PROP(N),                CSCP_CSC_MEAS_MAX_LEN                        },
    // CSC Measurement Characteristic - Client Characteristic Configuration Descriptor
    [CSCS_IDX_CSC_MEAS_NTF_CFG]   = { GATT_DESC_CLIENT_CHAR_CFG,  PROP(RD) | PROP(WR),    OPT(NO_OFFSET)                               },

    // CSC Feature Characteristic Declaration
    [CSCS_IDX_CSC_FEAT_CHAR]      = { GATT_DECL_CHARACTERISTIC,   PROP(RD),               0                                            },
    // CSC Feature Characteristic Value
    [CSCS_IDX_CSC_FEAT_VAL]       = { GATT_CHAR_CSC_FEAT,         PROP(RD),               OPT(NO_OFFSET)                               },

    // Sensor Location Characteristic Declaration
    [CSCS_IDX_SENSOR_LOC_CHAR]    = { GATT_DECL_CHARACTERISTIC,   PROP(RD),               0                                            },
    // Sensor Location Characteristic Value
    [CSCS_IDX_SENSOR_LOC_VAL]     = { GATT_CHAR_SENSOR_LOC,       PROP(RD),               OPT(NO_OFFSET)                               },

    // SC Control Point Characteristic Declaration
    [CSCS_IDX_SC_CTNL_PT_CHAR]    = { GATT_DECL_CHARACTERISTIC,   PROP(RD),               0                                            },
    // SC Control Point Characteristic Value - The response has the maximal length
    [CSCS_IDX_SC_CTNL_PT_VAL]     = { GATT_CHAR_SC_CNTL_PT,       PROP(I) | PROP(WR),     OPT(NO_OFFSET) | CSCP_SC_CNTL_PT_RSP_MAX_LEN },
    // SC Control Point Characteristic - Client Characteristic Configuration Descriptor
    [CSCS_IDX_SC_CTNL_PT_NTF_CFG] = { GATT_DESC_CLIENT_CHAR_CFG,  PROP(RD) | PROP(WR),    OPT(NO_OFFSET)                               },
};

/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */

/// Get database attribute index
__STATIC uint8_t cscps_idx_get(cscps_env_t* p_cscps_env, uint16_t hdl)
{
    uint8_t att_idx = hdl - p_cscps_env->start_hdl;

    if(   (att_idx >= CSCS_IDX_SENSOR_LOC_CHAR)
       && !CSCPS_IS_FEATURE_SUPPORTED(p_cscps_env->prfl_cfg, CSCPS_SENSOR_LOC_MASK))
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
__STATIC void cscps_exe_operation(cscps_env_t* p_cscps_env)
{
    if(!p_cscps_env->in_exe_op)
    {
        p_cscps_env->in_exe_op = true;

        while(!co_list_is_empty(&(p_cscps_env->wait_queue)) && !(p_cscps_env->op_ongoing))
        {
            uint16_t status = GAP_ERR_NO_ERROR;
            co_buf_t* p_buf = (co_buf_t*) co_list_pop_front(&(p_cscps_env->wait_queue));
            cscps_buf_meta_t* p_meta = (cscps_buf_meta_t*) co_buf_metadata(p_buf);
            uint8_t  conidx;

            switch(p_meta->operation)
            {
                case CSCPS_SEND_CSC_MEAS_OP_CODE:
                {
                    uint32_t conidx_bf = 0;

                    // check connection that support notification reception
                    for(conidx = 0 ; conidx < BLE_CONNECTION_MAX ; conidx++)
                    {
                        if(GETB(p_cscps_env->prfl_ntf_ind_cfg[conidx], CSCP_PRF_CFG_FLAG_CSC_MEAS_NTF))
                        {
                            conidx_bf |= CO_BIT(conidx);
                        }
                    }

                    // send notification only on selected connections
                    conidx_bf &= p_meta->conidx_bf;

                    if(conidx_bf != 0)
                    {
                        // send multi-point notification
                        status = gatt_srv_event_mtp_send(conidx_bf, p_cscps_env->user_lid, CSCPS_SEND_CSC_MEAS_OP_CODE,
                                                         GATT_NOTIFY, CSCPS_HANDLE(CSCS_IDX_CSC_MEAS_VAL), p_buf, true);
                        if(status == GAP_ERR_NO_ERROR)
                        {
                            p_cscps_env->op_ongoing = true;
                        }
                    }

                    co_buf_release(p_buf);

                    if(!p_cscps_env->op_ongoing)
                    {
                        const cscps_cb_t* p_cb = (const cscps_cb_t*) p_cscps_env->prf_env.p_cb;
                        // Inform application that event has been sent
                        p_cb->cb_meas_send_cmp(status);
                    }
                } break;

                default:
                {
                    conidx = p_meta->conidx;

                    status = gatt_srv_event_send(conidx, p_cscps_env->user_lid, p_meta->operation, GATT_INDICATE,
                                                 CSCPS_HANDLE(CSCS_IDX_SC_CTNL_PT_VAL), p_buf);

                    if(status == GAP_ERR_NO_ERROR)
                    {
                        p_cscps_env->op_ongoing = true;
                    }
                    else
                    {
                        // Inform application that control point response has been sent
                        if (p_cscps_env->ctrl_pt_op != CSCP_CTNL_PT_RSP_CODE)
                        {
                            const cscps_cb_t* p_cb = (const cscps_cb_t*) p_cscps_env->prf_env.p_cb;
                            p_cb->cb_ctnl_pt_rsp_send_cmp(conidx, status);
                        }

                        // consider control point operation done
                        p_cscps_env->ctrl_pt_op = CSCP_CTNL_PT_OP_RESERVED;
                    }

                    co_buf_release(p_buf);
                } break;
            }
        }

        p_cscps_env->in_exe_op = false;
    }
}


/**
 ****************************************************************************************
 * @brief Packs measurement data
 *
 * @param[in]     p_cscps_env   Environment data
 * @param[in]     p_buf         Pointer to output buffer
 * @param[in]     p_meas        pointer to measurement information
 ****************************************************************************************
 */
__STATIC void cscps_pack_meas(cscps_env_t *p_cscps_env, co_buf_t* p_buf, const cscp_csc_meas_t *p_meas)
{
    uint8_t meas_flags = p_meas->flags;

    // Check the provided flags value
    if (!GETB(p_cscps_env->prfl_cfg, CSCP_FEAT_WHEEL_REV_DATA_SUPP))
    {
        // Force Wheel Revolution Data to No (Not supported)
        SETB(meas_flags, CSCP_MEAS_WHEEL_REV_DATA_PRESENT, 0);
    }

    if (!GETB(p_cscps_env->prfl_cfg, CSCP_FEAT_CRANK_REV_DATA_SUPP))
    {
        // Force Crank Revolution Data Present to No (Not supported)
        SETB(meas_flags, CSCP_MEAS_CRANK_REV_DATA_PRESENT, 0);
    }

    // Force the unused bits of the flag value to 0
    co_buf_tail(p_buf)[0] =  meas_flags & CSCP_MEAS_ALL_PRESENT;
    co_buf_tail_reserve(p_buf, 1);

    // Cumulative Wheel Resolutions
    // Last Wheel Event Time
    if (GETB(meas_flags, CSCP_MEAS_WHEEL_REV_DATA_PRESENT))
    {
        // Cumulative Wheel Resolutions
        co_write32p(co_buf_tail(p_buf), co_htobl(p_cscps_env->tot_wheel_rev));
        co_buf_tail_reserve(p_buf, 4);
        // Last Wheel Event Time
        co_write16p(co_buf_tail(p_buf), co_htobs(p_meas->last_wheel_evt_time));
        co_buf_tail_reserve(p_buf, 2);
    }

    // Cumulative Crank Revolutions
    // Last Crank Event Time
    if (GETB(meas_flags, CSCP_MEAS_CRANK_REV_DATA_PRESENT))
    {
        // Cumulative Crank Revolutions
        co_write16p(co_buf_tail(p_buf), co_htobs(p_meas->cumul_crank_rev));
        co_buf_tail_reserve(p_buf, 2);

        // Last Crank Event Time
        co_write16p(co_buf_tail(p_buf), co_htobs(p_meas->last_crank_evt_time));
        co_buf_tail_reserve(p_buf, 2);
    }
}

/**
 ****************************************************************************************
 * @brief Unpack control point data and process it
 *
 * @param[in] p_cscps_env Environment
 * @param[in] conidx     connection index
 * @param[in] p_buf      pointer to input data
 ****************************************************************************************
 */
__STATIC uint16_t cscps_unpack_ctnl_point_req(cscps_env_t *p_cscps_env, uint8_t conidx, co_buf_t* p_buf)
{
    uint8_t op_code;
    uint8_t ctnl_pt_rsp_status = CSCP_CTNL_PT_RESP_NOT_SUPP;
    uint16_t status = GAP_ERR_NO_ERROR;
    union cscp_sc_ctnl_pt_req_val value;
    memset(&value, 0, sizeof(union cscp_sc_ctnl_pt_req_val));
    op_code =  co_buf_data(p_buf)[0];

    if(co_buf_head_release(p_buf, 1) == CO_BUF_ERR_NO_ERROR)
    {
        // Operation Code
        switch (op_code)
        {
            case (CSCP_CTNL_PT_OP_SET_CUMUL_VAL):
            {
                // Check if the Wheel Revolution Data feature is supported
                if (GETB(p_cscps_env->features, CSCP_FEAT_WHEEL_REV_DATA_SUPP))
                {
                    // Provided parameter in not within the defined range
                    ctnl_pt_rsp_status = CSCP_CTNL_PT_RESP_INV_PARAM;

                    if (co_buf_data_len(p_buf) == 4)
                    {
                        // The request can be handled
                        ctnl_pt_rsp_status = CSCP_CTNL_PT_RESP_SUCCESS;
                        // Update the environment
                        p_cscps_env->ctrl_pt_op = op_code;
                        // Cumulative value
                        value.cumul_val = co_btohl(co_read32p(co_buf_data(p_buf)));
                    }
                }
            } break;

            case (CSCP_CTNL_PT_OP_UPD_LOC):
            {
                // Check if the Multiple Sensor Location feature is supported
                if (GETB(p_cscps_env->features, CSCP_FEAT_MULT_SENSOR_LOC_SUPP))
                {
                    // Provided parameter in not within the defined range
                    ctnl_pt_rsp_status = CSCP_CTNL_PT_RESP_INV_PARAM;

                    if (co_buf_data_len(p_buf) == 1)
                    {
                        uint8_t sensor_loc = co_buf_data(p_buf)[0];

                        // Check the sensor location value
                        if (sensor_loc < CSCP_LOC_MAX)
                        {
                            value.sensor_loc = sensor_loc;
                            // The request can be handled
                            ctnl_pt_rsp_status = CSCP_CTNL_PT_RESP_SUCCESS;
                            // Update the environment
                            p_cscps_env->ctrl_pt_op = op_code;
                        }
                    }
                }
            } break;

            case (CSCP_CTNL_PT_OP_REQ_SUPP_LOC):
            {
                // Check if the Multiple Sensor Location feature is supported
                if (GETB(p_cscps_env->features, CSCP_FEAT_MULT_SENSOR_LOC_SUPP))
                {
                    // The request can be handled
                    ctnl_pt_rsp_status = CSCP_CTNL_PT_RESP_SUCCESS;
                    // Update the environment
                    p_cscps_env->ctrl_pt_op = op_code;
                }
            } break;

            default:
            {
                // Operation Code is invalid, status is already CSCP_CTNL_PT_RESP_NOT_SUPP
            } break;
        }

        // If no error raised, inform the application about the request
        if (ctnl_pt_rsp_status == CSCP_CTNL_PT_RESP_SUCCESS)
        {
            const cscps_cb_t* p_cb  = (const cscps_cb_t*) p_cscps_env->prf_env.p_cb;

            // inform application about control point request
            p_cb->cb_ctnl_pt_req(conidx, op_code, &value);
        }
        else
        {
            co_buf_t* p_out_buf = NULL;

            if(co_buf_alloc(&p_out_buf, GATT_BUFFER_HEADER_LEN, 0, CSCP_SC_CNTL_PT_RSP_MIN_LEN + GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
            {
                cscps_buf_meta_t* p_meta = (cscps_buf_meta_t*)co_buf_metadata(p_out_buf);

                p_cscps_env->ctrl_pt_op   = CSCP_CTNL_PT_RSP_CODE;
                co_buf_tail(p_out_buf)[0] = CSCP_CTNL_PT_RSP_CODE;
                co_buf_tail_reserve(p_out_buf, 1);
                co_buf_tail(p_out_buf)[0] = op_code;
                co_buf_tail_reserve(p_out_buf, 1);
                co_buf_tail(p_out_buf)[0] = ctnl_pt_rsp_status;
                co_buf_tail_reserve(p_out_buf, 1);

                p_meta->conidx    = conidx;
                p_meta->operation = CSCPS_CTNL_PT_RSP_OP_CODE;

                // put event on wait queue
                co_list_push_back(&(p_cscps_env->wait_queue), &(p_out_buf->hdr));
                // execute operation
                cscps_exe_operation(p_cscps_env);
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
 * @param[in]     p_cscps_env   Environment data
 * @param[in]     conidx       Connection Index
 * @param[in]     p_buf        Pointer to output buffer
 * @param[in]     op_code      Operation Code
 * @param[in]     resp_val     Operation result
 * @param[in]     p_value      Pointer to operation response parameters
 ****************************************************************************************
 */
void cscps_pack_ctnl_point_rsp(cscps_env_t *p_cscps_env, uint8_t conidx, co_buf_t* p_buf, uint8_t op_code, uint8_t resp_val,
                               const union cscp_sc_ctnl_pt_rsp_val* p_value)
{
    // Set the Response Code
    co_buf_tail(p_buf)[0] = CSCP_CTNL_PT_RSP_CODE;
    co_buf_tail_reserve(p_buf, 1);

    // Set the request operation code
    co_buf_tail(p_buf)[0] = p_cscps_env->ctrl_pt_op;
    co_buf_tail_reserve(p_buf, 1);

    if (resp_val == CSCP_CTNL_PT_RESP_SUCCESS)
    {
        co_buf_tail(p_buf)[0] = resp_val;
        co_buf_tail_reserve(p_buf, 1);

        switch (p_cscps_env->ctrl_pt_op)
        {
            case (CSCP_CTNL_PT_OP_SET_CUMUL_VAL):
                {
                // Save in the environment
                p_cscps_env->tot_wheel_rev = p_value->cumul_wheel_rev;
            } break;

            case (CSCP_CTNL_PT_OP_UPD_LOC):
            {
                // Store the new value in the environment
                p_cscps_env->sensor_loc = p_value->sensor_loc;
            } break;

            case (CSCP_CTNL_PT_OP_REQ_SUPP_LOC):
            {
                // Set the list of supported location
                for (uint8_t counter = 0; counter < CSCP_LOC_MAX; counter++)
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
        co_buf_tail(p_buf)[0] = (resp_val > CSCP_CTNL_PT_RESP_FAILED) ? CSCP_CTNL_PT_RESP_FAILED : resp_val;
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
__STATIC void cscps_cb_att_read_get(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset,
                                   uint16_t max_length)
{
    cscps_env_t *p_cscps_env = PRF_ENV_GET(CSCPS, cscps);
    // retrieve value attribute
    co_buf_t* p_buf       = NULL;
    uint16_t  status      = GAP_ERR_NO_ERROR;
    uint16_t  att_val_len = 0;

    if(p_cscps_env == NULL)
    {
        status = PRF_APP_ERROR;
    }
    else if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, 0,  sizeof(uint16_t) + GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
    {
        uint8_t att_idx = cscps_idx_get(p_cscps_env, hdl);

        switch (att_idx)
        {
            case CSCS_IDX_CSC_MEAS_NTF_CFG:
            {
                uint16_t ntf_cfg = GETB(p_cscps_env->prfl_ntf_ind_cfg[conidx], CSCP_PRF_CFG_FLAG_CSC_MEAS_NTF)
                                 ? PRF_CLI_START_NTF : PRF_CLI_STOP_NTFIND;

                co_write16p(co_buf_tail(p_buf), co_htobs(ntf_cfg));
                co_buf_tail_reserve(p_buf, 2);
            } break;

            case CSCS_IDX_SC_CTNL_PT_NTF_CFG:
            {
                uint16_t ind_cfg = GETB(p_cscps_env->prfl_ntf_ind_cfg[conidx], CSCP_PRF_CFG_FLAG_SC_CTNL_PT_IND)
                                 ? PRF_CLI_START_IND : PRF_CLI_STOP_NTFIND;

                co_write16p(co_buf_tail(p_buf), co_htobs(ind_cfg));
                co_buf_tail_reserve(p_buf, 2);
            } break;

            case CSCS_IDX_CSC_FEAT_VAL:
            {
                co_write16p(co_buf_tail(p_buf), co_htobs(p_cscps_env->features));
                co_buf_tail_reserve(p_buf, 2);
            } break;

            case CSCS_IDX_SENSOR_LOC_VAL:
            {
                co_buf_tail(p_buf)[0] = p_cscps_env->sensor_loc;
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
__STATIC void cscps_cb_att_val_set(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset,
                        co_buf_t* p_data)
{
    cscps_env_t *p_cscps_env = PRF_ENV_GET(CSCPS, cscps);
    uint16_t  status      = PRF_APP_ERROR;

    if(p_cscps_env != NULL)
    {
        uint8_t cfg_upd_flag  = 0;
        uint8_t cfg_upd_char  = 0;
        uint16_t cfg_en_val = 0;

        switch (cscps_idx_get(p_cscps_env, hdl))
        {
            case CSCS_IDX_CSC_MEAS_NTF_CFG:
            {
                cfg_upd_char = CSCP_CSCS_CSC_MEAS_CHAR;
                cfg_upd_flag = CSCP_PRF_CFG_FLAG_CSC_MEAS_NTF_BIT;
                cfg_en_val   = PRF_CLI_START_NTF;
            } break;

            case CSCS_IDX_SC_CTNL_PT_NTF_CFG:
            {
                cfg_upd_char = CSCP_CSCS_SC_CTNL_PT_CHAR;
                cfg_upd_flag = CSCP_PRF_CFG_FLAG_SC_CTNL_PT_IND_BIT;
                cfg_en_val   = PRF_CLI_START_IND;
            } break;

            case CSCS_IDX_SC_CTNL_PT_VAL:
            {
                // Check if sending of indications has been enabled
                if (!GETB(p_cscps_env->prfl_ntf_ind_cfg[conidx], CSCP_PRF_CFG_FLAG_SC_CTNL_PT_IND))
                {
                    // CPP improperly configured
                    status = CSCP_ERROR_CCC_INVALID_PARAM;
                }
                else if (p_cscps_env->ctrl_pt_op != CSCP_CTNL_PT_OP_RESERVED)
                {
                    // A procedure is already in progress
                    status = CSCP_ERROR_PROC_IN_PROGRESS;
                }
                else
                {
                    // Unpack Control Point parameters
                    status = cscps_unpack_ctnl_point_req(p_cscps_env, conidx, p_data);
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
                const cscps_cb_t* p_cb  = (const cscps_cb_t*) p_cscps_env->prf_env.p_cb;

                if(cfg == PRF_CLI_STOP_NTFIND)
                {
                    p_cscps_env->prfl_ntf_ind_cfg[conidx] &= ~cfg_upd_flag;
                }
                else
                {
                    p_cscps_env->prfl_ntf_ind_cfg[conidx] |= cfg_upd_flag;
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
__STATIC void cscps_cb_event_sent(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    // Consider job done
    cscps_env_t *p_cscps_env = PRF_ENV_GET(CSCPS, cscps);
    if(p_cscps_env != NULL)
    {
        const cscps_cb_t* p_cb  = (const cscps_cb_t*) p_cscps_env->prf_env.p_cb;
        p_cscps_env->op_ongoing = false;

        switch(dummy)
        {
            case CSCPS_SEND_CSC_MEAS_OP_CODE:
            {
                p_cb->cb_meas_send_cmp(status);
            } break;
            case CSCPS_CTNL_PT_RSP_OP_CODE:
            {
                // Inform application that control point response has been sent
                if (p_cscps_env->ctrl_pt_op != CSCP_CTNL_PT_RSP_CODE)
                {
                    p_cb->cb_ctnl_pt_rsp_send_cmp(conidx, status);
                }

                p_cscps_env->ctrl_pt_op = CSCP_CTNL_PT_OP_RESERVED;
            } break;
            default: { /* Nothing to do */ } break;
        }

        // continue operation execution
        cscps_exe_operation(p_cscps_env);
    }
}

/// Service callback hander
__STATIC const gatt_srv_cb_t cscps_cb =
{
        .cb_event_sent    = cscps_cb_event_sent,
        .cb_att_read_get  = cscps_cb_att_read_get,
        .cb_att_event_get = NULL,
        .cb_att_info_get  = NULL,
        .cb_att_val_set   = cscps_cb_att_val_set,
};

/*
 * PROFILE NATIVE HANDLERS
 ****************************************************************************************
 */

uint16_t cscps_enable(uint8_t conidx, uint16_t csc_meas_ntf_cfg, uint16_t sc_ctnl_pt_ntf_cfg)
{
    cscps_env_t* p_cscps_env = PRF_ENV_GET(CSCPS, cscps);
    uint16_t status = PRF_ERR_REQ_DISALLOWED;

    if(p_cscps_env != NULL)
    {
        // check state of the task
        if (gapc_get_conhdl(conidx) != GAP_INVALID_CONHDL)
        {
            // Check the provided value
            if (csc_meas_ntf_cfg == PRF_CLI_START_NTF)
            {
                // Store the status
                SETB(p_cscps_env->prfl_ntf_ind_cfg[conidx], CSCP_PRF_CFG_FLAG_CSC_MEAS_NTF, 1);
            }

            if (CSCPS_IS_FEATURE_SUPPORTED(p_cscps_env->prfl_cfg, CSCPS_SC_CTNL_PT_MASK))
            {
                // Check the provided value
                if (sc_ctnl_pt_ntf_cfg == PRF_CLI_START_IND)
                {
                    // Store the status
                    SETB(p_cscps_env->prfl_ntf_ind_cfg[conidx], CSCP_PRF_CFG_FLAG_SC_CTNL_PT_IND, 1);
                }
            }

            // Enable Bonded Data
            SETB(p_cscps_env->prfl_ntf_ind_cfg[conidx], CSCP_PRF_CFG_PERFORMED_OK, 1);


            status = GAP_ERR_NO_ERROR;
        }
    }

    return (status);

}

uint16_t cscps_meas_send(uint32_t conidx_bf, int16_t wheel_rev,  const cscp_csc_meas_t* p_meas)
{
    cscps_env_t* p_cscps_env = PRF_ENV_GET(CSCPS, cscps);
    uint16_t status = PRF_ERR_REQ_DISALLOWED;

    if(p_meas == NULL)
    {
        status = GAP_ERR_INVALID_PARAM;
    }
    else if(p_cscps_env != NULL)
    {
        co_buf_t* p_buf;
        // Should be updated just once
        if (   GETB(p_meas->flags, CSCP_MEAS_WHEEL_REV_DATA_PRESENT)
            && GETB(p_cscps_env->features, CSCP_FEAT_WHEEL_REV_DATA_SUPP))
        {
            // Update the cumulative wheel revolutions value stored in the environment
            if (wheel_rev < 0)
            {
                // The value shall not decrement below zero
                p_cscps_env->tot_wheel_rev = (co_abs(wheel_rev) > p_cscps_env->tot_wheel_rev)
                                           ? 0 : p_cscps_env->tot_wheel_rev + wheel_rev;
            }
            else
            {
                p_cscps_env->tot_wheel_rev += wheel_rev;
            }
        }

        if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, 0, CSCP_CSC_MEAS_MAX_LEN + GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
        {
            cscps_buf_meta_t* p_buf_meta = (cscps_buf_meta_t*) co_buf_metadata(p_buf);
            p_buf_meta->operation = CSCPS_SEND_CSC_MEAS_OP_CODE;
            p_buf_meta->conidx    = GAP_INVALID_CONIDX;
            p_buf_meta->conidx_bf = conidx_bf & ((1 << BLE_CONNECTION_MAX) - 1);

            // pack measurement
            cscps_pack_meas(p_cscps_env, p_buf, p_meas);
            // put event on wait queue
            co_list_push_back(&(p_cscps_env->wait_queue), &(p_buf->hdr));
            // execute operation
            cscps_exe_operation(p_cscps_env);
            status = GAP_ERR_NO_ERROR;
        }
        else
        {
            status = GAP_ERR_INSUFF_RESOURCES;
        }
    }

    return (status);
}

uint16_t cscps_ctnl_pt_rsp_send(uint8_t conidx, uint8_t op_code, uint8_t resp_val,
                                const union cscp_sc_ctnl_pt_rsp_val* p_value)
{
    cscps_env_t* p_cscps_env = PRF_ENV_GET(CSCPS, cscps);
    uint16_t status = PRF_ERR_REQ_DISALLOWED;

    if(p_value == NULL)
    {
        status = GAP_ERR_INVALID_PARAM;
    }
    else if(p_cscps_env != NULL)
    {
        do
        {
            co_buf_t* p_buf = NULL;

            // Check the current operation
            if (p_cscps_env->ctrl_pt_op == CSCP_CTNL_PT_OP_RESERVED)
            {
                // The confirmation has been sent without request indication, ignore
                break;
            }

            // The CP Control Point Characteristic must be supported if we are here
            if (!CSCPS_IS_FEATURE_SUPPORTED(p_cscps_env->prfl_cfg, CSCPS_SC_CTNL_PT_MASK))
            {
                status = PRF_ERR_REQ_DISALLOWED;
                break;
            }

            // Check if sending of indications has been enabled
            if (!GETB(p_cscps_env->prfl_ntf_ind_cfg[conidx], CSCP_PRF_CFG_FLAG_SC_CTNL_PT_IND))
            {
                // mark operation done
                p_cscps_env->ctrl_pt_op = CSCP_CTNL_PT_OP_RESERVED;
                // CPP improperly configured
                status = PRF_CCCD_IMPR_CONFIGURED;
                break;
            }

            if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, 0, CSCP_SC_CNTL_PT_RSP_MAX_LEN + GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
            {
                cscps_buf_meta_t* p_buf_meta = (cscps_buf_meta_t*) co_buf_metadata(p_buf);
                p_buf_meta->operation = CSCPS_CTNL_PT_RSP_OP_CODE;
                p_buf_meta->conidx    = conidx;

                // Pack structure
                cscps_pack_ctnl_point_rsp(p_cscps_env, conidx, p_buf, op_code, resp_val, p_value);
                // put event on wait queue
                co_list_push_back(&(p_cscps_env->wait_queue), &(p_buf->hdr));
                // execute operation
                cscps_exe_operation(p_cscps_env);
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
 * @brief Send a CSCPS_CMP_EVT message to the application.
 * @param[in] conidx connection index
 * @param[in] operation Operation completed
 * @param[in] status status of the operation
 ****************************************************************************************
 */
__STATIC void cscps_send_cmp_evt(uint8_t conidx, uint8_t operation, uint16_t status)
{
    struct cscps_cmp_evt *p_evt;

    // Send the message
    p_evt = KE_MSG_ALLOC(CSCPS_CMP_EVT, PRF_DST_TASK(CSCPS),
                         PRF_SRC_TASK(CSCPS), cscps_cmp_evt);

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
 * @brief Handles reception of the @ref CSCPS_ENABLE_REQ message.
 * @param[in] msgid Id of the message received
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int cscps_enable_req_handler(ke_msg_id_t const msgid, struct cscps_enable_req *p_param,
                                      ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    struct cscps_enable_rsp *p_cmp_evt;
    uint16_t status = cscps_enable(p_param->conidx, p_param->csc_meas_ntf_cfg, p_param->sc_ctnl_pt_ntf_cfg);

    // send completed information to APP task that contains error status
    p_cmp_evt = KE_MSG_ALLOC(CSCPS_ENABLE_RSP, src_id, dest_id, cscps_enable_rsp);

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
 * @brief Handles reception of the @ref CSCPS_NTF_CSC_MEAS_CMD message.
 * @param[in] msgid Id of the message received.
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int cscps_ntf_csc_meas_cmd_handler(ke_msg_id_t const msgid, struct cscps_ntf_csc_meas_cmd *p_param,
                                            ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = cscps_meas_send(p_param->conidx_bf, p_param->wheel_rev, &(p_param->csc_meas));

    if(status != GAP_ERR_NO_ERROR)
    {
        cscps_send_cmp_evt(GAP_INVALID_CONIDX, CSCPS_SEND_CSC_MEAS_OP_CODE, status);
    }

    return (KE_MSG_CONSUMED);
}


/**
 ****************************************************************************************
 * @brief Handles reception of the @ref CSCPS_SC_CTNL_PT_RSP_SEND_CMD message.
 * @param[in] msgid Id of the message received.
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int cscps_sc_ctnl_pt_rsp_send_cmd_handler(ke_msg_id_t const msgid, struct cscps_sc_ctnl_pt_rsp_send_cmd *p_param,
                                                   ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = cscps_ctnl_pt_rsp_send(p_param->conidx, p_param->op_code, p_param->resp_value, &(p_param->value));

    if(status != GAP_ERR_NO_ERROR)
    {
        cscps_send_cmp_evt(p_param->conidx, CSCPS_CTNL_PT_RSP_OP_CODE, status);
    }

    return (KE_MSG_CONSUMED);
}

/// Default State handlers definition
KE_MSG_HANDLER_TAB(cscps)
{
    // Note: all messages must be sorted in ID ascending order

    {CSCPS_ENABLE_REQ,              (ke_msg_func_t) cscps_enable_req_handler},
    {CSCPS_NTF_CSC_MEAS_CMD,        (ke_msg_func_t) cscps_ntf_csc_meas_cmd_handler},
    {CSCPS_SC_CTNL_PT_RSP_SEND_CMD, (ke_msg_func_t) cscps_sc_ctnl_pt_rsp_send_cmd_handler},
};

/**
 ****************************************************************************************
 * @brief Completion of measurement transmission
 *
 * @param[in] status Status of the procedure execution (@see enum hl_err)
 ****************************************************************************************
 */
__STATIC void cscps_cb_meas_send_cmp(uint16_t status)
{
    cscps_send_cmp_evt(GAP_INVALID_CONIDX, CSCPS_SEND_CSC_MEAS_OP_CODE, status);
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
__STATIC void cscps_cb_bond_data_upd(uint8_t conidx, uint8_t char_code, uint16_t cfg_val)
{
    struct cscps_cfg_ntfind_ind *p_evt;

    // Send the message
    p_evt = KE_MSG_ALLOC(CSCPS_CFG_NTFIND_IND, PRF_DST_TASK(CSCPS),
                         PRF_SRC_TASK(CSCPS), cscps_cfg_ntfind_ind);

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
 * @note control point request must be answered using @see cscps_ctnl_pt_rsp_send function
 *
 * @param[in] conidx        Connection index
 * @param[in] op_code       Operation Code (@see enum cscp_sc_ctnl_pt_code)
 * @param[in] p_value       Pointer to control point request value
 ****************************************************************************************
 */
__STATIC void cscps_cb_ctnl_pt_req(uint8_t conidx, uint8_t op_code, const union cscp_sc_ctnl_pt_req_val* p_value)
{
    struct cscps_sc_ctnl_pt_req_recv_ind *p_evt;

    // Send the message
    p_evt = KE_MSG_ALLOC(CSCPS_SC_CTNL_PT_REQ_RECV_IND, PRF_DST_TASK(CSCPS),
                         PRF_SRC_TASK(CSCPS), cscps_sc_ctnl_pt_req_recv_ind);

    if(p_evt != NULL)
    {
        p_evt->conidx     = conidx;
        p_evt->op_code    = op_code;
        memcpy(&(p_evt->value), p_value, sizeof(union cscp_sc_ctnl_pt_req_val));
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
__STATIC void cscps_cb_ctnl_pt_rsp_send_cmp(uint8_t conidx, uint16_t status)
{
    cscps_send_cmp_evt(conidx, CSCPS_CTNL_PT_RSP_OP_CODE, status);
}

/// Default Message handle
__STATIC const cscps_cb_t cscps_msg_cb =
{
        .cb_meas_send_cmp        = cscps_cb_meas_send_cmp,
        .cb_bond_data_upd        = cscps_cb_bond_data_upd,
        .cb_ctnl_pt_req          = cscps_cb_ctnl_pt_req,
        .cb_ctnl_pt_rsp_send_cmp = cscps_cb_ctnl_pt_rsp_send_cmp,
};
#endif // (BLE_HL_MSG_API)

/*
 * PROFILE DEFAULT HANDLERS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Initialization of the CSCPS module.
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
__STATIC uint16_t cscps_init(prf_data_t *p_env, uint16_t *p_start_hdl, uint8_t sec_lvl, uint8_t user_prio,
                          struct cscps_db_cfg *p_params, const cscps_cb_t* p_cb)
{
    //------------------ create the attribute database for the profile -------------------

    // DB Creation Status
    uint16_t status = GAP_ERR_NO_ERROR;
    uint8_t user_lid = GATT_INVALID_USER_LID;

    do
    {
        cscps_env_t* p_cscps_env;
        // Service content flag
        uint32_t cfg_flag = CSCPS_MANDATORY_MASK;

        #if (BLE_HL_MSG_API)
        if(p_cb == NULL)
        {
            p_cb = &(cscps_msg_cb);
        }
        #endif // (BLE_HL_MSG_API)

        if(   (p_params == NULL) || (p_start_hdl == NULL) || (p_cb == NULL) || (p_cb->cb_meas_send_cmp == NULL)
           || (p_cb->cb_bond_data_upd == NULL) || (p_cb->cb_ctnl_pt_req == NULL) || (p_cb->cb_ctnl_pt_rsp_send_cmp == NULL))
        {
            status = GAP_ERR_INVALID_PARAM;
            break;
        }

        // register CSCPS user
        status = gatt_user_srv_register(L2CAP_LE_MTU_MIN, user_prio, &cscps_cb, &user_lid);
        if(status != GAP_ERR_NO_ERROR) break;

        /*
         * Check if the Sensor Location characteristic shall be added.
         * Mandatory if the Multiple Sensor Location feature is supported, otherwise optional.
         */
        if (   (p_params->sensor_loc_supp == CSCPS_SENSOR_LOC_SUPP)
            || (GETB(p_params->csc_feature, CSCP_FEAT_MULT_SENSOR_LOC_SUPP)))
        {
            cfg_flag |= CSCPS_SENSOR_LOC_MASK;
        }

        /*
         * Check if the SC Control Point characteristic shall be added
         * Mandatory if at least one SC Control Point procedure is supported, otherwise excluded.
         */
        if (   GETB(p_params->csc_feature, CSCP_FEAT_WHEEL_REV_DATA_SUPP)
            || GETB(p_params->csc_feature, CSCP_FEAT_MULT_SENSOR_LOC_SUPP))
        {
            cfg_flag |= CSCPS_SC_CTNL_PT_MASK;
        }


        // Add GAP service
        status = gatt_db_svc16_add(user_lid, sec_lvl, GATT_SVC_CYCLING_SPEED_CADENCE, CSCS_IDX_NB,
                                   (uint8_t *)&cfg_flag, &(cscps_att_db[0]), CSCS_IDX_NB, p_start_hdl);
        if(status != GAP_ERR_NO_ERROR) break;

        //-------------------- allocate memory required for the profile  ---------------------
        p_cscps_env = (cscps_env_t *) ke_malloc(sizeof(cscps_env_t), KE_MEM_ATT_DB);

        if(p_cscps_env != NULL)
        {
            // allocate CSCPS required environment variable
            p_env->p_env = (prf_hdr_t *) p_cscps_env;
            p_cscps_env->start_hdl       = *p_start_hdl;
            p_cscps_env->features        = p_params->csc_feature;
            p_cscps_env->user_lid        = user_lid;
            p_cscps_env->prfl_cfg        = cfg_flag;
            p_cscps_env->tot_wheel_rev   = p_params->wheel_rev;
            p_cscps_env->op_ongoing      = false;
            p_cscps_env->in_exe_op       = false;
            p_cscps_env->ctrl_pt_op      = CSCPS_RESERVED_OP_CODE;
            memset(p_cscps_env->prfl_ntf_ind_cfg, 0, sizeof(p_cscps_env->prfl_ntf_ind_cfg));
            co_list_init(&(p_cscps_env->wait_queue));

            if (CSCPS_IS_FEATURE_SUPPORTED(p_cscps_env->prfl_cfg, CSCPS_SENSOR_LOC_MASK))
            {
                p_cscps_env->sensor_loc = (p_params->sensor_loc >= CSCP_LOC_MAX) ?
                        CSCP_LOC_OTHER : p_params->sensor_loc;
            }

            // initialize profile environment variable
            p_cscps_env->prf_env.p_cb     = p_cb;
            #if (BLE_HL_MSG_API)
            p_env->desc.msg_handler_tab  = cscps_msg_handler_tab;
            p_env->desc.msg_cnt          = ARRAY_LEN(cscps_msg_handler_tab);
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
__STATIC uint16_t cscps_destroy(prf_data_t *p_env, uint8_t reason)
{
    uint16_t status = GAP_ERR_NO_ERROR;
    cscps_env_t *p_cscps_env = (cscps_env_t *) p_env->p_env;

    if(reason != PRF_DESTROY_RESET)
    {
        status = gatt_user_unregister(p_cscps_env->user_lid);
    }

    if(status == GAP_ERR_NO_ERROR)
    {
        if(reason != PRF_DESTROY_RESET)
        {
            // remove buffer in wait queue
            while(!co_list_is_empty(&p_cscps_env->wait_queue))
            {
                co_buf_t* p_buf = (co_buf_t*) co_list_pop_front(&p_cscps_env->wait_queue);
                co_buf_release(p_buf);
            }
        }

        // free profile environment variables
        p_env->p_env = NULL;
        ke_free(p_cscps_env);
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
__STATIC void cscps_con_create(prf_data_t *p_env, uint8_t conidx, const gap_con_param_t* p_con_param)
{
    cscps_env_t *p_cscps_env = (cscps_env_t *) p_env->p_env;
    p_cscps_env->prfl_ntf_ind_cfg[conidx] = 0;
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
__STATIC void cscps_con_cleanup(prf_data_t *p_env, uint8_t conidx, uint16_t reason)
{
    cscps_env_t *p_cscps_env = (cscps_env_t *) p_env->p_env;
    p_cscps_env->prfl_ntf_ind_cfg[conidx] = 0;
}

/// CSCPS Task interface required by profile manager
const prf_task_cbs_t cscps_itf =
{
    .cb_init          = (prf_init_cb) cscps_init,
    .cb_destroy       = cscps_destroy,
    .cb_con_create    = cscps_con_create,
    .cb_con_cleanup   = cscps_con_cleanup,
    .cb_con_upd       = NULL,
};

/**
 ****************************************************************************************
 * @brief Retrieve service profile interface
 *
 * @return service profile interface
 ****************************************************************************************
 */
const prf_task_cbs_t *cscps_prf_itf_get(void)
{
    return &cscps_itf;
}


#endif //(BLE_CSC_SENSOR)

/// @} CSCPS

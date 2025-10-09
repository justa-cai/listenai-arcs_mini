/**
 ****************************************************************************************
 *
 * @file plxs.c
 *
 * @brief Pulse Oximeter Profile Service implementation.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup PLXS
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "btip_config.h"

#if (BLE_PLX_SERVER)

#include "plxs.h"
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
 ******* PLXS Configuration Flag Masks ******
 ********************************************/
/// PLXP Configuration Flag Masks
#define PLXS_MANDATORY_MASK                (0x1F8F)
#define PLXS_MEAS_CTX_PRES_MASK            (0x0070)

#define PLXS_FILTER_USER_FACING_TIME_SIZE (7)

/// Pulse Oximeter Service - Attribute List
enum plxs_att_list
{
    /// Pulse Oximeter Service
    PLXS_IDX_SVC,
    /// PLXP SPOT-Measurement Characteristic
    PLXS_IDX_SPOT_MEASUREMENT_CHAR,
    PLXS_IDX_SPOT_MEASUREMENT_VAL,
    /// CCC Descriptor
    PLXS_IDX_SPOT_MEASUREMENT_CCC,
    /// PLXP Continuous Measurement Characteristic
    PLXS_IDX_CONT_MEASUREMENT_CHAR,
    PLXS_IDX_CONT_MEASUREMENT_VAL,
    /// CCC Descriptor
    PLXS_IDX_CONT_MEASUREMENT_CCC,
    /// PLXP Features Characteristic
    PLXS_IDX_FEATURES_CHAR,
    PLXS_IDX_FEATURES_VAL,
    /// PLXP Record Access Control Point Characteristic
    PLXS_IDX_RACP_CHAR,
    PLXS_IDX_RACP_VAL,
    /// CCC Descriptor
    PLXS_IDX_RACP_CCC,

    /// Number of attributes
    PLXS_IDX_NB,
};


/*
 * MACROS
 ****************************************************************************************
 */


/*
 * TYPES DEFINITION
 ****************************************************************************************
 */

/// ongoing operation information
typedef struct plxs_buf_meta
{
    /// Attribute handle
    uint16_t  hdl;
    /// Event type
    uint16_t  evt_type;
    /// Operation
    uint8_t   operation;
    /// Connection index targeted
    uint8_t   conidx;
} plxs_buf_meta_t;


/// Service server environment variable
typedef struct plxs_env
{
    /// profile environment
    prf_hdr_t       prf_env;
    /// Operation Event TX wait queue
    co_list_t       wait_queue;
    /// Service Attribute Start Handle
    uint16_t        start_hdl;
    /// Device features
    plxp_features_t features;
    /// Type of Operation @see enum plxs_optype_id
    uint8_t         optype;
    /// GATT user local identifier
    uint8_t         user_lid;
    /// Control point operation on-going (@see enum glp_racp_op_code)
    uint8_t         racp_op_code;
    /// Operation On-going
    bool            op_ongoing;
    /// Prevent recursion in execute_operation function
    bool            in_exe_op;
    /// Event (notification/indication) configuration
    uint8_t         evt_cfg[BLE_CONNECTION_MAX];

} plxs_env_t;


/*
 * ATTRIBUTES DATABASE
 ****************************************************************************************
 */

/// Default read perm
#define RD_P        (PROP(RD) | SEC_LVL(RP,  NOT_ENC))
/// Default write perm
#define WR_P        (PROP(WR) | SEC_LVL(WP,  NOT_ENC))
/// Default notify perm
#define NTF_P       (PROP(N)  | SEC_LVL(NIP, NOT_ENC))
/// Ind perm
#define IND_P       (PROP(I)  | SEC_LVL(NIP, NOT_ENC))

/// Full PLXS Database Description - Used to add attributes into the database
__STATIC const gatt_att16_desc_t plxs_att_db[PLXS_IDX_NB] =
{
    // ATT Index                       | ATT UUID                                | Permission  | EXT PERM / MAX ATT SIZE
    // service attribute
    [PLXS_IDX_SVC]                   = { GATT_DECL_PRIMARY_SERVICE,                PROP(RD),     0                                   },
    // PLXP SPOT-Measurement Characteristic
    [PLXS_IDX_SPOT_MEASUREMENT_CHAR] = { GATT_DECL_CHARACTERISTIC,                 PROP(RD),     0                                   },
    [PLXS_IDX_SPOT_MEASUREMENT_VAL]  = { GATT_CHAR_PLX_SPOT_CHECK_MEASUREMENT_LOC, IND_P,        0                                   },
    [PLXS_IDX_SPOT_MEASUREMENT_CCC]  = { GATT_DESC_CLIENT_CHAR_CFG,                RD_P | WR_P,  OPT(NO_OFFSET)                      },
    // PLXP Continuous Measurement Characteristic
    [PLXS_IDX_CONT_MEASUREMENT_CHAR] = { GATT_DECL_CHARACTERISTIC,                 PROP(RD),     0                                   },
    [PLXS_IDX_CONT_MEASUREMENT_VAL]  = { GATT_CHAR_PLX_CONTINUOUS_MEASUREMENT_LOC, NTF_P,        0                                   },
    [PLXS_IDX_CONT_MEASUREMENT_CCC]  = { GATT_DESC_CLIENT_CHAR_CFG,                RD_P | WR_P,  OPT(NO_OFFSET)                      },
    // PLXP Features Characteristic
    [PLXS_IDX_FEATURES_CHAR]         = { GATT_DECL_CHARACTERISTIC,                 PROP(RD),     0                                   },
    [PLXS_IDX_FEATURES_VAL]          = { GATT_CHAR_PLX_FEATURES_LOC,               RD_P,         OPT(NO_OFFSET)                      },
    // PLXP Record Access Control Point Characteristic
    [PLXS_IDX_RACP_CHAR]             = { GATT_DECL_CHARACTERISTIC,                 PROP(RD),     0                                   },
    [PLXS_IDX_RACP_VAL]              = { GATT_CHAR_REC_ACCESS_CTRL_PT,             WR_P | IND_P, OPT(NO_OFFSET) | PLXP_RACP_SIZE_MAX },
    [PLXS_IDX_RACP_CCC]              = { GATT_DESC_CLIENT_CHAR_CFG,                RD_P | WR_P,  OPT(NO_OFFSET)                      },
};

/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */


/**
 ****************************************************************************************
 * @brief Convert raw handle to the attribute index from @see enum plxs_att_list
 * @param[in] hdl     PLXS raw handle
 * @return PLXS Attribute
 ****************************************************************************************
 */
__STATIC uint8_t plxs_idx_get(uint16_t hdl)
{
    // Get the address of the environment
    plxs_env_t *p_plxs_env = PRF_ENV_GET(PLXS, plxs);
    uint8_t att_idx = hdl - p_plxs_env->start_hdl;

    if (PLXS_OPTYPE_SPOT_CHECK_ONLY == p_plxs_env->optype)
    {
        // if >4 then +3 (4-->7)
        if (att_idx > 4)
        {
            att_idx += PLXS_IDX_FEATURES_CHAR -PLXS_IDX_CONT_MEASUREMENT_CHAR;
        }
    }
    else if (PLXS_OPTYPE_CONTINUOUS_ONLY == p_plxs_env->optype)
    {
        // if >0 then +3 (1-->4)
        if (att_idx > 0)
        {
            att_idx += PLXS_IDX_CONT_MEASUREMENT_CHAR -PLXS_IDX_SPOT_MEASUREMENT_CHAR;
        }
    }

    return att_idx;
}


/**
 ****************************************************************************************
 * @brief Convert attribute to the raw handle index from @see enum plxs_att_list
 * @param[in] att_idx     PLXS Attribute
 * @return PLXS Handle index
 ****************************************************************************************
 */
uint16_t plxs_hdl_get(uint8_t att_idx)
{
    // Get the address of the environment
    plxs_env_t *p_plxs_env = PRF_ENV_GET(PLXS, plxs);
    uint16_t hdl = p_plxs_env->start_hdl + att_idx;

    if (PLXS_OPTYPE_SPOT_CHECK_ONLY == p_plxs_env->optype)
    {
        // adjust by not existent CONT_MEAS attributes
        if (att_idx >= PLXS_IDX_FEATURES_CHAR)
        {
            hdl -= PLXS_IDX_FEATURES_CHAR - PLXS_IDX_CONT_MEASUREMENT_CHAR;
        }
    }
    else if (PLXS_OPTYPE_CONTINUOUS_ONLY == p_plxs_env->optype)
    {
        // adjust by not existent SPOT_MEASUREMENT attributes
        if (att_idx >= PLXS_IDX_CONT_MEASUREMENT_CHAR)
        {
            att_idx += PLXS_IDX_CONT_MEASUREMENT_CHAR - PLXS_IDX_SPOT_MEASUREMENT_CHAR;
        }
    }

    return hdl;
}

/**
 ****************************************************************************************
 * @brief  This function fully manages GATT event transmission
 ****************************************************************************************
 */
__STATIC void plxs_exe_operation(plxs_env_t* p_plxs_env)
{
    if(!p_plxs_env->in_exe_op)
    {
        p_plxs_env->in_exe_op = true;

        while(!co_list_is_empty(&(p_plxs_env->wait_queue)) && !(p_plxs_env->op_ongoing))
        {
            uint16_t status = GAP_ERR_NO_ERROR;
            co_buf_t* p_buf = (co_buf_t*) co_list_pop_front(&(p_plxs_env->wait_queue));
            plxs_buf_meta_t* p_meta = (plxs_buf_meta_t*) co_buf_metadata(p_buf);
            uint8_t  operation = p_meta->operation;
            uint8_t  conidx    = p_meta->conidx;

            // send GATT event
            status = gatt_srv_event_send(conidx, p_plxs_env->user_lid, operation, p_meta->evt_type, p_meta->hdl, p_buf);

            if(status == GAP_ERR_NO_ERROR)
            {
                p_plxs_env->op_ongoing = true;
            }

            co_buf_release(p_buf);

            if(!p_plxs_env->op_ongoing)
            {
                const plxs_cb_t* p_cb = (const plxs_cb_t*) p_plxs_env->prf_env.p_cb;

                switch(operation)
                {
                    case PLXS_RACP_CMD_OP_CODE:
                    {
                        // Inform application that control point response has been sent
                        if (p_plxs_env->racp_op_code != PLXP_OPCODE_RESPONSE_CODE)
                        {
                            p_cb->cb_racp_rsp_send_cmp(conidx, status);
                        }

                        // consider control point operation done
                        p_plxs_env->racp_op_code = PLXP_OPCODE_RESERVED;
                    } break;
                    case PLXS_SPOT_CHECK_MEAS_CMD_OP_CODE:
                    {
                        // Inform application that event has been sent
                        p_cb->cb_spot_meas_send_cmp(conidx, status);
                    }break;
                    case PLXS_CONTINUOUS_MEAS_CMD_OP_CODE:
                    {
                        // Inform application that event has been sent
                        p_cb->cb_cont_meas_send_cmp(conidx, status);
                    }break;

                    default: { /* Nothing to do */ } break;
                }
            }
        }

        p_plxs_env->in_exe_op = false;
    }
}


/**
 ****************************************************************************************
 * @brief Packs Spot-Check measurement data
 *
 * @param[in]     p_plxs_env   Environment data
 * @param[in]     p_buf        Pointer to output buffer
 * @param[in]     p_spot_meas  pointer to measurement information
 ****************************************************************************************
 */
__STATIC void plxs_pack_spot_meas(plxs_env_t *p_plxs_env, co_buf_t* p_buf,  const plxp_spot_meas_t* p_spot_meas)
{
    uint8_t meas_flags = p_spot_meas->spot_flags & PLXP_SPOT_MEAS_FLAGS_VALID_MASK;

    // check the sup_feat vs Flags settings
    // mask off unsupported features
    if (!GETB(p_plxs_env->features.sup_feat, PLXP_FEAT_TIMESTAMP_SUP))
    {
        SETB(meas_flags, PLXP_SPOT_MEAS_FLAGS_TIMESTAMP, 0);
    }
    if (!GETB(p_plxs_env->features.sup_feat, PLXP_FEAT_MEAS_STATUS_SUP))
    {
        SETB(meas_flags, PLXP_SPOT_MEAS_FLAGS_MEAS_STATUS, 0);
    }
    if (!GETB(p_plxs_env->features.sup_feat, PLXP_FEAT_DEV_SENSOR_STATUS_SUP))
    {
        SETB(meas_flags, PLXP_SPOT_MEAS_FLAGS_DEV_SENSOR_STATUS, 0);
    }
    if (!GETB(p_plxs_env->features.sup_feat, PLXP_FEAT_PULSE_AMPL_SUP))
    {
        SETB(meas_flags, PLXP_SPOT_MEAS_FLAGS_PULSE_AMPLITUDE, 0);
    }

    // Flags
    co_buf_tail(p_buf)[0] =  meas_flags;
    co_buf_tail_reserve(p_buf, 1);

    // SP O2 data
    co_write16p(co_buf_tail(p_buf), co_htobs(p_spot_meas->sp_o2));
    co_buf_tail_reserve(p_buf, 2);

    // Period
    co_write16p(co_buf_tail(p_buf), co_htobs(p_spot_meas->pr));
    co_buf_tail_reserve(p_buf, 2);

    // Timestamp
    if (GETB(meas_flags, PLXP_SPOT_MEAS_FLAGS_TIMESTAMP))
    {
        prf_pack_date_time(p_buf, &(p_spot_meas->timestamp));
    }

    // Measurement Status
    if (GETB(meas_flags, PLXP_SPOT_MEAS_FLAGS_MEAS_STATUS))
    {
        // If bit of the Measurement Status field is not supported in the Feature,
        // it shall always be set to 0 in its characteristic field
        uint16_t meas_stat = p_spot_meas->meas_stat & p_plxs_env->features.meas_stat_sup;
        co_write16p(co_buf_tail(p_buf), co_htobs(meas_stat));
        co_buf_tail_reserve(p_buf, 2);
    }

    // Device & Sensor Status
    if (GETB(meas_flags, PLXP_SPOT_MEAS_FLAGS_DEV_SENSOR_STATUS))
    {
        // If bit of the Device and Sensor Status field is not supported in the Feature,
        // it shall always be set to 0 in its characteristic field
        uint32_t dev_sensor_stat = p_spot_meas->dev_sensor_stat & p_plxs_env->features.dev_stat_sup;
        co_write24p(co_buf_tail(p_buf), co_htob24(dev_sensor_stat));
        co_buf_tail_reserve(p_buf, 3);
    }

    // Pulse Amplitude Index
    if (GETB(meas_flags, PLXP_SPOT_MEAS_FLAGS_PULSE_AMPLITUDE))
    {
        co_write16p(co_buf_tail(p_buf), co_htobs(p_spot_meas->pulse_ampl));
        co_buf_tail_reserve(p_buf, 2);
    }
}

/**
 ****************************************************************************************
 * @brief Packs Continuous measurement data
 *
 * @param[in]     p_plxs_env   Environment data
 * @param[in]     p_buf        Pointer to output buffer
 * @param[in]     p_cont_meas  Pointer to measurement information
 ****************************************************************************************
 */
__STATIC void plxs_pack_cont_meas(plxs_env_t *p_plxs_env, co_buf_t* p_buf, const plxp_cont_meas_t* p_cont_meas)
{
    uint8_t meas_flags = p_cont_meas->cont_flags & PLXP_CONT_MEAS_FLAGS_VALID_MASK;

    // check the sup_feat vs Flags settings
    // mask off unsupported features
    if (!GETB(p_plxs_env->features.sup_feat, PLXP_FEAT_SPO2PR_FAST_SUP))
    {
        SETB(meas_flags, PLXP_CONT_MEAS_FLAGS_SPO2PR_FAST, 0);
    }
    if (!GETB(p_plxs_env->features.sup_feat, PLXP_FEAT_SPO2PR_SLOW_SUP))
    {
        SETB(meas_flags, PLXP_CONT_MEAS_FLAGS_SPO2PR_SLOW, 0);
    }
    if (!GETB(p_plxs_env->features.sup_feat, PLXP_FEAT_MEAS_STATUS_SUP))
    {
        SETB(meas_flags, PLXP_CONT_MEAS_FLAGS_MEAS_STATUS, 0);
    }
    if (!GETB(p_plxs_env->features.sup_feat, PLXP_FEAT_DEV_SENSOR_STATUS_SUP))
    {
        SETB(meas_flags, PLXP_CONT_MEAS_FLAGS_DEV_SENSOR_STATUS, 0);
    }
    if (!GETB(p_plxs_env->features.sup_feat, PLXP_FEAT_PULSE_AMPL_SUP))
    {
        SETB(meas_flags, PLXP_CONT_MEAS_FLAGS_PULSE_AMPL, 0);
    }

    // Flags
    co_buf_tail(p_buf)[0] = meas_flags;
    co_buf_tail_reserve(p_buf, 1);

    // SpO2PR-Normal
    co_write16p(co_buf_tail(p_buf), co_htobs(p_cont_meas->normal.sp_o2));
    co_buf_tail_reserve(p_buf, 2);
    co_write16p(co_buf_tail(p_buf), co_htobs(p_cont_meas->normal.pr));
    co_buf_tail_reserve(p_buf, 2);

    // SpO2PR-Fast
    if (GETB(meas_flags, PLXP_CONT_MEAS_FLAGS_SPO2PR_FAST))
    {
        co_write16p(co_buf_tail(p_buf), co_htobs(p_cont_meas->fast.sp_o2));
        co_buf_tail_reserve(p_buf, 2);
        co_write16p(co_buf_tail(p_buf), co_htobs(p_cont_meas->fast.pr));
        co_buf_tail_reserve(p_buf, 2);
    }
    // SpO2PR-Slow
    if (GETB(meas_flags, PLXP_CONT_MEAS_FLAGS_SPO2PR_SLOW))
    {
        co_write16p(co_buf_tail(p_buf), co_htobs(p_cont_meas->slow.sp_o2));
        co_buf_tail_reserve(p_buf, 2);
        co_write16p(co_buf_tail(p_buf), co_htobs(p_cont_meas->slow.pr));
        co_buf_tail_reserve(p_buf, 2);
    }

    // Measurement Status
    if (GETB(meas_flags, PLXP_CONT_MEAS_FLAGS_MEAS_STATUS))
    {
        // If bit of the Measurement Status field is not supported in the Feature,
        // it shall always be set to 0 in its characteristic field
        uint16_t meas_stat = p_cont_meas->meas_stat & p_plxs_env->features.meas_stat_sup;
        co_write16p(co_buf_tail(p_buf), co_htobs(meas_stat));
        co_buf_tail_reserve(p_buf, 2);
    }

    // Device & Sensor Status
    if (GETB(meas_flags, PLXP_CONT_MEAS_FLAGS_DEV_SENSOR_STATUS))
    {
        // If bit of the Device and Sensor Status field is not supported in the Feature,
        // it shall always be set to 0 in its characteristic field
        uint32_t dev_sensor_stat = p_cont_meas->dev_sensor_stat & p_plxs_env->features.dev_stat_sup;
        co_write24p(co_buf_tail(p_buf), co_htob24(dev_sensor_stat));
        co_buf_tail_reserve(p_buf, 3);
    }

    // Pulse Amplitude Index
    if (GETB(meas_flags, PLXP_CONT_MEAS_FLAGS_PULSE_AMPL))
    {
        co_write16p(co_buf_tail(p_buf), co_htobs(p_cont_meas->pulse_ampl));
        co_buf_tail_reserve(p_buf, 2);
    }
}

/**
 ****************************************************************************************
 * @brief Unpack control point data and process it
 *
 * @param[in] p_plxs_env Environment
 * @param[in] conidx     connection index
 * @param[in] p_buf      pointer to input data
 ****************************************************************************************
 */
__STATIC uint16_t plxs_unpack_racp_req(plxs_env_t *p_plxs_env, uint8_t conidx, co_buf_t* p_buf)
{
    uint16_t status = GAP_ERR_NO_ERROR;
    uint8_t  racp_rsp_status = PLXP_RESP_INVALID_OPERATOR;
    uint8_t  op_code = 0;
    uint8_t  func_operator = 0;

    do
    {
        // verify that enough data present to load operation filter
        if (co_buf_data_len(p_buf) < 2)
        {
            status = ATT_ERR_UNLIKELY_ERR;
            break;
        }

        op_code = co_buf_data(p_buf)[0];
        co_buf_head_release(p_buf, 1);
        func_operator = co_buf_data(p_buf)[0];
        co_buf_head_release(p_buf, 1);


        // Abort operation don't require any other parameter
        if (op_code == PLXP_OPCODE_ABORT_OPERATION)
        {
            if(p_plxs_env->racp_op_code == PLXP_OPCODE_RESERVED)
            {
                // do nothing since a procedure already in progress
                racp_rsp_status = PLXP_RESP_ABORT_UNSUCCESSFUL;
            }
            else
            {
                // Handle abort, no need to extract other info
                racp_rsp_status = PLXP_RESP_SUCCESS;
            }
            break;
        }
        else if(p_plxs_env->racp_op_code != PLXP_OPCODE_RESERVED)
        {
            // do nothing since a procedure already in progress
            status = PRF_PROC_IN_PROGRESS;
            break;
        }

        // check if opcode is supported
        if ((op_code < PLXP_OPCODE_REPORT_STORED_RECORDS) || (op_code > PLXP_OPCODE_REPORT_NUMBER_OF_STORED_RECORDS))
        {
            racp_rsp_status = PLXP_RESP_OP_CODE_NOT_SUPPORTED;
            break;
        }

        // check if operator is valid
        if (func_operator < PLXP_OPERATOR_ALL_RECORDS)
        {
            racp_rsp_status = PLXP_RESP_INVALID_OPERATOR;
            break;
        }
        // check if operator is supported
        else if (func_operator > PLXP_OPERATOR_ALL_RECORDS)
        {
            racp_rsp_status = PLXP_RESP_OPERATOR_NOT_SUPPORTED;
            break;
        }

        // consider that data extraction is a sucess
        racp_rsp_status = PLXP_RESP_SUCCESS;
    } while(0);


    if(status == GAP_ERR_NO_ERROR)
    {
        // If no error raised, inform the application about the request
        if (racp_rsp_status == PLXP_RESP_SUCCESS)
        {
            const plxs_cb_t* p_cb  = (const plxs_cb_t*) p_plxs_env->prf_env.p_cb;

            p_plxs_env->racp_op_code  = op_code;

            // inform application about control point request
            p_cb->cb_racp_req(conidx, op_code, func_operator);
        }
        else
        {
            co_buf_t* p_out_buf = NULL;

            if(co_buf_alloc(&p_out_buf, GATT_BUFFER_HEADER_LEN, 0, L2CAP_LE_MTU_MIN + GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
            {
                plxs_buf_meta_t* p_meta = (plxs_buf_meta_t*)co_buf_metadata(p_out_buf);

                p_plxs_env->racp_op_code  = PLXP_OPCODE_RESPONSE_CODE;
                co_buf_tail(p_out_buf)[0] = PLXP_OPCODE_RESPONSE_CODE;
                co_buf_tail_reserve(p_out_buf, 1);
                co_buf_tail(p_out_buf)[0] = PLXP_OPERATOR_NULL;
                co_buf_tail_reserve(p_out_buf, 1);
                co_buf_tail(p_out_buf)[0] = op_code;
                co_buf_tail_reserve(p_out_buf, 1);
                co_buf_tail(p_out_buf)[0] = racp_rsp_status;
                co_buf_tail_reserve(p_out_buf, 1);

                p_meta->conidx    = conidx;
                p_meta->operation = PLXS_RACP_CMD_OP_CODE;
                p_meta->evt_type  = GATT_INDICATE;
                p_meta->hdl       = plxs_hdl_get(PLXS_IDX_RACP_VAL);

                // put event on wait queue
                co_list_push_back(&(p_plxs_env->wait_queue), &(p_out_buf->hdr));
                // execute operation
                plxs_exe_operation(p_plxs_env);
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
 * @param[in] p_plxs_env    Environment data
 * @param[in] conidx        Connection Index
 * @param[in] p_buf         Pointer to output buffer
 * @param[in] op_code       Requested Operation Code (@see enum glp_racp_op_code)
 * @param[in] racp_status   Record access control point execution status (@see enum glp_racp_status)
 * @param[in] num_of_record Number of record (meaningful for PLXP_OPCODE_REPORT_NUMBER_OF_STORED_RECORDS operation)
 ****************************************************************************************
 */
void plxs_pack_racp_rsp(plxs_env_t *p_plxs_env, uint8_t conidx, co_buf_t* p_buf, uint8_t op_code, uint8_t racp_status,
                        uint16_t num_of_record)
{
    bool num_recs_rsp = ((op_code == PLXP_OPCODE_REPORT_NUMBER_OF_STORED_RECORDS) && (racp_status == PLXP_RESP_SUCCESS));

    // Set the Response Code
    co_buf_tail(p_buf)[0] = num_recs_rsp ? PLXP_OPCODE_NUMBER_OF_STORED_RECORDS_RESP : PLXP_OPCODE_RESPONSE_CODE;
    co_buf_tail_reserve(p_buf, 1);

    // set operator (null)
    co_buf_tail(p_buf)[0] = PLXP_OPERATOR_NULL;
    co_buf_tail_reserve(p_buf, 1);

    if(num_recs_rsp)
    {
        co_write16p(co_buf_tail(p_buf), co_htobs(num_of_record));
        co_buf_tail_reserve(p_buf, 2);
    }
    else
    {
        // requested opcode
        co_buf_tail(p_buf)[0] = op_code;
        co_buf_tail_reserve(p_buf, 1);
        // command status
        co_buf_tail(p_buf)[0] = racp_status;
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
__STATIC void plxs_cb_att_read_get(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset,
                                   uint16_t max_length)
{
    plxs_env_t *p_plxs_env = PRF_ENV_GET(PLXS, plxs);
    // retrieve value attribute
    co_buf_t* p_buf       = NULL;
    uint16_t  status      = GAP_ERR_NO_ERROR;
    uint16_t  att_val_len = 0;

    if(p_plxs_env == NULL)
    {
        status = PRF_APP_ERROR;
    }
    else if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, 0,  PLXP_FEAT_SIZE_MAX + GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
    {
        uint8_t att_idx = plxs_idx_get(hdl);

        switch (att_idx)
        {
            case PLXS_IDX_SPOT_MEASUREMENT_CCC:
            {
                uint16_t ntf_cfg = GETB(p_plxs_env->evt_cfg[conidx], PLXS_MEAS_SPOT_IND_CFG)
                                 ? PRF_CLI_START_IND : PRF_CLI_STOP_NTFIND;

                co_write16p(co_buf_tail(p_buf), co_htobs(ntf_cfg));
                co_buf_tail_reserve(p_buf, 2);
            } break;

            case PLXS_IDX_CONT_MEASUREMENT_CCC:
            {
                uint16_t ntf_cfg = GETB(p_plxs_env->evt_cfg[conidx], PLXS_MEAS_CONT_NTF_CFG)
                                 ? PRF_CLI_START_NTF : PRF_CLI_STOP_NTFIND;

                co_write16p(co_buf_tail(p_buf), co_htobs(ntf_cfg));
                co_buf_tail_reserve(p_buf, 2);
            } break;

            case PLXS_IDX_RACP_CCC:
            {
                uint16_t ind_cfg = GETB(p_plxs_env->evt_cfg[conidx], PLXS_RACP_IND_CFG)
                                 ? PRF_CLI_START_IND : PRF_CLI_STOP_NTFIND;

                co_write16p(co_buf_tail(p_buf), co_htobs(ind_cfg));
                co_buf_tail_reserve(p_buf, 2);
            } break;

            case PLXS_IDX_FEATURES_VAL:
            {
                co_write16p(co_buf_tail(p_buf), co_htobs(p_plxs_env->features.sup_feat));
                co_buf_tail_reserve(p_buf, 2);

                if (GETB(p_plxs_env->features.sup_feat, PLXP_FEAT_MEAS_STATUS_SUP))
                {
                    co_write16p(co_buf_tail(p_buf), co_htobs(p_plxs_env->features.meas_stat_sup));
                    co_buf_tail_reserve(p_buf, 2);
                }
                if (GETB(p_plxs_env->features.sup_feat, PLXP_FEAT_DEV_SENSOR_STATUS_SUP))
                {
                    co_write24p(co_buf_tail(p_buf), co_htob24(p_plxs_env->features.dev_stat_sup));
                    co_buf_tail_reserve(p_buf, 3);
                }
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
__STATIC void plxs_cb_att_val_set(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset,
                        co_buf_t* p_data)
{
    plxs_env_t *p_plxs_env = PRF_ENV_GET(PLXS, plxs);
    uint16_t  status      = PRF_APP_ERROR;

    if(p_plxs_env != NULL)
    {
        uint8_t cfg_upd_flag  = 0;
        uint16_t cfg_en_val = 0;

        switch (plxs_idx_get(hdl))
        {
            case PLXS_IDX_SPOT_MEASUREMENT_CCC:
            {
                cfg_upd_flag = PLXS_MEAS_SPOT_IND_CFG_BIT;
                cfg_en_val   = PRF_CLI_START_IND;
            } break;

            case PLXS_IDX_CONT_MEASUREMENT_CCC:
            {
                cfg_upd_flag = PLXS_MEAS_CONT_NTF_CFG_BIT;
                cfg_en_val   = PRF_CLI_START_NTF;
            } break;

            case PLXS_IDX_RACP_CCC:
            {
                cfg_upd_flag = PLXS_RACP_IND_CFG_BIT;
                cfg_en_val   = PRF_CLI_START_IND;
            } break;

            case PLXS_IDX_RACP_VAL:
            {
                // Check if sending of indications has been enabled
                if (!GETB(p_plxs_env->evt_cfg[conidx], PLXS_RACP_IND_CFG))
                {
                    // CPP improperly configured
                    status = PRF_CCCD_IMPR_CONFIGURED;
                }
                else
                {
                    // Unpack Control Point parameters
                    status = plxs_unpack_racp_req(p_plxs_env, conidx, p_data);
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
                const plxs_cb_t* p_cb  = (const plxs_cb_t*) p_plxs_env->prf_env.p_cb;

                if(cfg == PRF_CLI_STOP_NTFIND)
                {
                    p_plxs_env->evt_cfg[conidx] &= ~cfg_upd_flag;
                }
                else
                {
                    p_plxs_env->evt_cfg[conidx] |= cfg_upd_flag;
                }

                // inform application about update
                p_cb->cb_bond_data_upd(conidx, p_plxs_env->evt_cfg[conidx]);
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
__STATIC void plxs_cb_event_sent(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    // Consider job done
    plxs_env_t *p_plxs_env = PRF_ENV_GET(PLXS, plxs);
    if(p_plxs_env != NULL)
    {
        const plxs_cb_t* p_cb  = (const plxs_cb_t*) p_plxs_env->prf_env.p_cb;
        p_plxs_env->op_ongoing = false;

        switch(dummy)
        {
            case PLXS_RACP_CMD_OP_CODE:
            {
                // Inform application that control point response has been sent
                if (p_plxs_env->racp_op_code != PLXP_OPCODE_RESPONSE_CODE)
                {
                    p_cb->cb_racp_rsp_send_cmp(conidx, status);
                }

                // consider control point operation done
                p_plxs_env->racp_op_code = PLXP_OPCODE_RESERVED;
            } break;
            case PLXS_SPOT_CHECK_MEAS_CMD_OP_CODE:
            {
                // Inform application that event has been sent
                p_cb->cb_spot_meas_send_cmp(conidx, status);
            }break;
            case PLXS_CONTINUOUS_MEAS_CMD_OP_CODE:
            {
                // Inform application that event has been sent
                p_cb->cb_cont_meas_send_cmp(conidx, status);
            }break;

            default: { /* Nothing to do */ } break;
        }

        // continue operation execution
        plxs_exe_operation(p_plxs_env);
    }
}

/// Service callback hander
__STATIC const gatt_srv_cb_t plxs_cb =
{
        .cb_event_sent    = plxs_cb_event_sent,
        .cb_att_read_get  = plxs_cb_att_read_get,
        .cb_att_event_get = NULL,
        .cb_att_info_get  = NULL,
        .cb_att_val_set   = plxs_cb_att_val_set,
};

/*
 * PROFILE NATIVE HANDLERS
 ****************************************************************************************
 */

uint16_t plxs_enable(uint8_t conidx, uint8_t evt_cfg)
{
    plxs_env_t* p_plxs_env = PRF_ENV_GET(PLXS, plxs);
    uint16_t status = PRF_ERR_REQ_DISALLOWED;

    if(p_plxs_env != NULL)
    {
        // check state of the task
        if (gapc_get_conhdl(conidx) != GAP_INVALID_CONHDL)
        {
            p_plxs_env->evt_cfg[conidx] = evt_cfg;

            status = GAP_ERR_NO_ERROR;
        }
    }

    return (status);
}

uint16_t plxs_spot_meas_send(uint8_t conidx, const plxp_spot_meas_t* p_spot_meas)
{
    plxs_env_t* p_plxs_env = PRF_ENV_GET(PLXS, plxs);
    uint16_t status = PRF_ERR_REQ_DISALLOWED;

    if(p_spot_meas == NULL)
    {
        status = GAP_ERR_INVALID_PARAM;
    }
    else if((p_plxs_env != NULL) && (gapc_get_conhdl(conidx) != GAP_INVALID_CONHDL))
    {
        co_buf_t* p_buf_meas;
        plxs_buf_meta_t* p_buf_meta;

        // check if indication enabled
        if(   !GETB(p_plxs_env->evt_cfg[conidx], PLXS_MEAS_SPOT_IND_CFG)
           || (p_plxs_env->optype == PLXS_OPTYPE_CONTINUOUS_ONLY))
        {
            // Not allowed to send measurement if Notifications not enabled.
            status = (PRF_ERR_IND_DISABLED);
        }
        else if(co_buf_alloc(&p_buf_meas, GATT_BUFFER_HEADER_LEN, 0, L2CAP_LE_MTU_MIN + GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
        {
            p_buf_meta = (plxs_buf_meta_t*) co_buf_metadata(p_buf_meas);
            p_buf_meta->operation = PLXS_SPOT_CHECK_MEAS_CMD_OP_CODE;
            p_buf_meta->conidx    = conidx;
            p_buf_meta->evt_type  = GATT_INDICATE;
            p_buf_meta->hdl       = plxs_hdl_get(PLXS_IDX_SPOT_MEASUREMENT_VAL);

            // pack measurement
            plxs_pack_spot_meas(p_plxs_env, p_buf_meas, p_spot_meas);
            status = GAP_ERR_NO_ERROR;
        }
        else
        {
            status = GAP_ERR_INSUFF_RESOURCES;
        }

        if(status == GAP_ERR_NO_ERROR)
        {
            // put event(s) on wait queue
            co_list_push_back(&(p_plxs_env->wait_queue), &(p_buf_meas->hdr));
            // execute operation
            plxs_exe_operation(p_plxs_env);
        }
    }

    return (status);
}

uint16_t plxs_cont_meas_send(uint8_t conidx, const plxp_cont_meas_t* p_cont_meas)
{
    plxs_env_t* p_plxs_env = PRF_ENV_GET(PLXS, plxs);
    uint16_t status = PRF_ERR_REQ_DISALLOWED;

    if(p_cont_meas == NULL)
    {
        status = GAP_ERR_INVALID_PARAM;
    }
    else if((p_plxs_env != NULL) && (gapc_get_conhdl(conidx) != GAP_INVALID_CONHDL))
    {
        co_buf_t* p_buf_meas;
        plxs_buf_meta_t* p_buf_meta;


        // check if notifications enabled
        if(   !GETB(p_plxs_env->evt_cfg[conidx], PLXS_MEAS_CONT_NTF_CFG)
           || (p_plxs_env->optype == PLXS_OPTYPE_SPOT_CHECK_ONLY))
        {
            // Not allowed to send measurement if Notifications not enabled.
            status = (PRF_ERR_NTF_DISABLED);
        }
        else if(co_buf_alloc(&p_buf_meas, GATT_BUFFER_HEADER_LEN, 0, L2CAP_LE_MTU_MIN + GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
        {
            p_buf_meta = (plxs_buf_meta_t*) co_buf_metadata(p_buf_meas);
            p_buf_meta->operation = PLXS_CONTINUOUS_MEAS_CMD_OP_CODE;
            p_buf_meta->conidx    = conidx;
            p_buf_meta->evt_type  = GATT_NOTIFY;
            p_buf_meta->hdl       = plxs_hdl_get(PLXS_IDX_CONT_MEASUREMENT_VAL);

            // pack measurement
            plxs_pack_cont_meas(p_plxs_env, p_buf_meas, p_cont_meas);
            status = GAP_ERR_NO_ERROR;
        }
        else
        {
            status = GAP_ERR_INSUFF_RESOURCES;
        }

        if(status == GAP_ERR_NO_ERROR)
        {
            // put event(s) on wait queue
            co_list_push_back(&(p_plxs_env->wait_queue), &(p_buf_meas->hdr));
            // execute operation
            plxs_exe_operation(p_plxs_env);
        }
    }

    return (status);
}

uint16_t plxs_racp_rsp_send(uint8_t conidx, uint8_t op_code, uint8_t racp_status, uint16_t num_of_record)
{
    plxs_env_t* p_plxs_env = PRF_ENV_GET(PLXS, plxs);
    uint16_t status = PRF_ERR_REQ_DISALLOWED;

    if(p_plxs_env != NULL)
    {
        do
        {
            co_buf_t* p_buf = NULL;

            // check if op code valid
            if ((op_code < PLXP_OPCODE_REPORT_STORED_RECORDS) || (op_code > PLXP_OPCODE_REPORT_NUMBER_OF_STORED_RECORDS))
            {
                // Wrong op code
                status = PRF_ERR_INVALID_PARAM;
                break;
            }
            // check if RACP on going
            else if ((op_code != PLXP_OPCODE_ABORT_OPERATION) && (p_plxs_env->racp_op_code != op_code))
            {
                // Cannot send response since no RACP on going
                break;
            }

            // Check the current operation
            if (p_plxs_env->racp_op_code == PLXP_OPCODE_RESERVED)
            {
                // The confirmation has been sent without request indication, ignore
                break;
            }

            // Check if sending of indications has been enabled
            if (!GETB(p_plxs_env->evt_cfg[conidx], PLXS_RACP_IND_CFG))
            {
                // mark operation done
                p_plxs_env->racp_op_code = PLXP_OPCODE_RESERVED;
                // CPP improperly configured
                status = PRF_ERR_IND_DISABLED;
                break;
            }

            if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, 0, L2CAP_LE_MTU_MIN + GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
            {
                plxs_buf_meta_t* p_buf_meta = (plxs_buf_meta_t*) co_buf_metadata(p_buf);
                p_buf_meta->operation = PLXS_RACP_CMD_OP_CODE;
                p_buf_meta->conidx    = conidx;
                p_buf_meta->hdl       = plxs_hdl_get(PLXS_IDX_RACP_VAL);
                p_buf_meta->evt_type  = GATT_INDICATE;

                // Pack structure
                plxs_pack_racp_rsp(p_plxs_env, conidx, p_buf, op_code, racp_status, num_of_record);
                // put event on wait queue
                co_list_push_back(&(p_plxs_env->wait_queue), &(p_buf->hdr));
                // execute operation
                plxs_exe_operation(p_plxs_env);
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
 * @brief Send a PLXS_CMP_EVT message to the application.
 * @param[in] conidx    Connection index
 * @param[in] operation Completed operation (@see enum plxs_op_codes)
 * @param[in] status    Status of the operation
 ****************************************************************************************
 */
__STATIC void plxs_send_cmp_evt(uint8_t conidx, uint8_t operation, uint16_t status)
{
    struct plxs_cmp_evt *p_evt;

    // Send the message
    p_evt = KE_MSG_ALLOC(PLXS_CMP_EVT, PRF_DST_TASK(PLXS), PRF_SRC_TASK(PLXS), plxs_cmp_evt);

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
 * @brief Handles reception of the @ref PLXS_ENABLE_REQ message.
 * @param[in] msgid Id of the message received
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int plxs_enable_req_handler(ke_msg_id_t const msgid, struct plxs_enable_req *p_param,
                                     ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    struct plxs_enable_rsp *p_cmp_evt;
    uint16_t status = plxs_enable(p_param->conidx, p_param->evt_cfg);

    // send completed information to APP task that contains error status
    p_cmp_evt = KE_MSG_ALLOC(PLXS_ENABLE_RSP, src_id, dest_id, plxs_enable_rsp);

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
 * @brief Handles reception of the @see PLXS_MEAS_VALUE_CMD message.
 * @brief Send MEASUREMENT INDICATION/NOTIFICATION to the connected peer case if CCC enabled
 * @param[in] msgid Id of the message received.
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int plxs_meas_value_cmd_handler(ke_msg_id_t const msgid, struct plxs_meas_value_cmd *p_param,
                                         ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status;

    switch(p_param->operation)
    {
        case PLXS_SPOT_CHECK_MEAS_CMD_OP_CODE: { status = plxs_spot_meas_send(p_param->conidx, &(p_param->value.spot_meas)); } break;
        case PLXS_CONTINUOUS_MEAS_CMD_OP_CODE: { status = plxs_cont_meas_send(p_param->conidx, &(p_param->value.cont_meas)); } break;
        default:                               { status = PRF_ERR_INVALID_PARAM;                                             } break;
    }

    if(status != GAP_ERR_NO_ERROR)
    {
        plxs_send_cmp_evt(p_param->conidx, p_param->operation, status);
    }

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief Handles reception of the @see PLXS_RACP_RESP_SEND_CMD message.
 * @brief Send MEASUREMENT INDICATION to the connected peer case if CCC enabled
 * @param[in] msgid Id of the message received.
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int plxs_racp_rsp_send_cmd_handler(ke_msg_id_t const msgid, struct plxs_racp_rsp_send_cmd *p_param,
                                          ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = plxs_racp_rsp_send(p_param->conidx, p_param->req_cp_opcode, p_param->rsp_code, p_param->rec_num);

    if(status != GAP_ERR_NO_ERROR)
    {
        plxs_send_cmp_evt(p_param->conidx, PLXS_RACP_CMD_OP_CODE, status);
    }

    return (KE_MSG_CONSUMED);
}

/// Default State handlers definition
KE_MSG_HANDLER_TAB(plxs)
{
    // Note: all messages must be sorted in ID ascending order

    { PLXS_ENABLE_REQ,          (ke_msg_func_t) plxs_enable_req_handler        },
    { PLXS_MEAS_VALUE_CMD,      (ke_msg_func_t) plxs_meas_value_cmd_handler    },
    { PLXS_RACP_RESP_SEND_CMD,  (ke_msg_func_t) plxs_racp_rsp_send_cmd_handler },
};


/**
 ****************************************************************************************
 * @brief Completion of Spot-Check measurement transmission
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 ****************************************************************************************
 */
__STATIC void plxs_cb_spot_meas_send_cmp(uint8_t conidx, uint16_t status)
{
    plxs_send_cmp_evt(conidx, PLXS_SPOT_CHECK_MEAS_CMD_OP_CODE, status);
}
/**
 ****************************************************************************************
 * @brief Completion of Continuous measurement transmission
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 ****************************************************************************************
 */
__STATIC void plxs_cb_cont_meas_send_cmp(uint8_t conidx, uint16_t status)
{
    plxs_send_cmp_evt(conidx, PLXS_CONTINUOUS_MEAS_CMD_OP_CODE, status);
}

/**
 ****************************************************************************************
 * @brief Inform that bond data updated for the connection.
 *
 * @param[in] conidx        Connection index
 * @param[in] evt_cfg       Indication/notification configuration (@see enum plxs_evt_cfg_bf)
 ****************************************************************************************
 */
__STATIC void plxs_cb_bond_data_upd(uint8_t conidx, uint8_t evt_cfg)
{
    struct plxs_cfg_indntf_ind *p_evt;

    // Send the message
    p_evt = KE_MSG_ALLOC(PLXS_CFG_INDNTF_IND, PRF_DST_TASK(PLXS),
                         PRF_SRC_TASK(PLXS), plxs_cfg_indntf_ind);

    if(p_evt != NULL)
    {
        p_evt->conidx     = conidx;
        p_evt->evt_cfg    = evt_cfg;
        ke_msg_send(p_evt);
    }
}

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
__STATIC void plxs_cb_racp_req(uint8_t conidx, uint8_t op_code, uint8_t func_operator)
{
    struct plxs_racp_req_recv_ind *p_evt;

    // Send the message
    p_evt = KE_MSG_ALLOC(PLXS_RACP_REQ_RECV_IND, PRF_DST_TASK(PLXS),
                         PRF_SRC_TASK(PLXS), plxs_racp_req_recv_ind);

    if(p_evt != NULL)
    {
        p_evt->conidx      = conidx;
        p_evt->cp_opcode   = op_code;
        p_evt->cp_operator = func_operator;
        ke_msg_send(p_evt);
    }
}

/**
 ****************************************************************************************
 * @brief Completion of record access control point response send procedure
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 ****************************************************************************************
 */
__STATIC void plxs_cb_racp_rsp_send_cmp(uint8_t conidx, uint16_t status)
{
    plxs_send_cmp_evt(conidx, PLXS_RACP_CMD_OP_CODE, status);
}


/// Default Message handle
__STATIC const plxs_cb_t plxs_msg_cb =
{
    .cb_spot_meas_send_cmp = plxs_cb_spot_meas_send_cmp,
    .cb_cont_meas_send_cmp = plxs_cb_cont_meas_send_cmp,
    .cb_bond_data_upd      = plxs_cb_bond_data_upd,
    .cb_racp_req           = plxs_cb_racp_req,
    .cb_racp_rsp_send_cmp  = plxs_cb_racp_rsp_send_cmp,
};
#endif // (BLE_HL_MSG_API)

/*
 * PROFILE DEFAULT HANDLERS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Initialization of the PLXS module.
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
__STATIC uint16_t plxs_init(prf_data_t *p_env, uint16_t *p_start_hdl, uint8_t sec_lvl, uint8_t user_prio,
                          struct plxs_db_cfg *p_params, const plxs_cb_t* p_cb)
{
    //------------------ create the attribute database for the profile -------------------

    // DB Creation Status
    uint16_t status = GAP_ERR_NO_ERROR;
    uint8_t user_lid = GATT_INVALID_USER_LID;

    do
    {
        uint8_t optype;
        plxs_env_t* p_plxs_env;
        uint8_t cfg_flag[] = {0xFF,0x0F};

        #if (BLE_HL_MSG_API)
        if(p_cb == NULL)
        {
            p_cb = &(plxs_msg_cb);
        }
        #endif // (BLE_HL_MSG_API)

        if(   (p_params == NULL) || (p_start_hdl == NULL) || (p_cb == NULL)
           || (p_cb->cb_spot_meas_send_cmp == NULL) || (p_cb->cb_cont_meas_send_cmp == NULL)
           || (p_cb->cb_bond_data_upd == NULL) || (p_cb->cb_racp_req == NULL) || (p_cb->cb_racp_rsp_send_cmp == NULL))
        {
            status = GAP_ERR_INVALID_PARAM;
            break;
        }

        // register PLXS user
        status = gatt_user_srv_register(L2CAP_LE_MTU_MIN, user_prio, &plxs_cb, &user_lid);
        if(status != GAP_ERR_NO_ERROR) break;

        optype = p_params->optype;

        if (optype == PLXS_OPTYPE_SPOT_CHECK_ONLY)
        {
            // mask off Continuous Measurement Characteristic
            // mask off Records Access Control Point Characteristic
            cfg_flag[0] = 0x8F;
            cfg_flag[1] = 0x01;
        }
        else if (optype == PLXS_OPTYPE_CONTINUOUS_ONLY)
        {
            // mask off SPOT-Measurement Characteristic
            cfg_flag[0] = 0xF1;
        }
        else
        {
            optype = 0;
        }

        // Add GAP service
        status = gatt_db_svc16_add(user_lid, sec_lvl, GATT_SVC_PULSE_OXIMETER, PLXS_IDX_NB,
                                   cfg_flag, &(plxs_att_db[0]), PLXS_IDX_NB, p_start_hdl);
        if(status != GAP_ERR_NO_ERROR) break;

        //-------------------- allocate memory required for the profile  ---------------------
        p_plxs_env = (plxs_env_t *) ke_malloc(sizeof(plxs_env_t), KE_MEM_ATT_DB);

        if(p_plxs_env != NULL)
        {
            // allocate PLXS required environment variable
            p_env->p_env = (prf_hdr_t *) p_plxs_env;
            p_plxs_env->start_hdl              = *p_start_hdl;
            p_plxs_env->optype                 = optype;
            p_plxs_env->features.sup_feat      = p_params->sup_feat;
            p_plxs_env->features.meas_stat_sup = p_params->meas_stat_sup;
            p_plxs_env->features.dev_stat_sup  = p_params->dev_stat_sup;
            p_plxs_env->optype                 = optype;
            p_plxs_env->optype                 = optype;
            p_plxs_env->user_lid               = user_lid;
            p_plxs_env->op_ongoing             = false;
            p_plxs_env->in_exe_op              = false;
            p_plxs_env->racp_op_code           = PLXP_OPCODE_RESERVED;
            memset(p_plxs_env->evt_cfg, 0,       sizeof(p_plxs_env->evt_cfg));
            co_list_init(&(p_plxs_env->wait_queue));

            // initialize profile environment variable
            p_plxs_env->prf_env.p_cb     = p_cb;
            #if (BLE_HL_MSG_API)
            p_env->desc.msg_handler_tab  = plxs_msg_handler_tab;
            p_env->desc.msg_cnt          = ARRAY_LEN(plxs_msg_handler_tab);
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
__STATIC uint16_t plxs_destroy(prf_data_t *p_env, uint8_t reason)
{
    uint16_t status = GAP_ERR_NO_ERROR;
    plxs_env_t *p_plxs_env = (plxs_env_t *) p_env->p_env;

    if(reason != PRF_DESTROY_RESET)
    {
        status = gatt_user_unregister(p_plxs_env->user_lid);
    }

    if(status == GAP_ERR_NO_ERROR)
    {
        if(reason != PRF_DESTROY_RESET)
        {
            // remove buffer in wait queue
            while(!co_list_is_empty(&p_plxs_env->wait_queue))
            {
                co_buf_t* p_buf = (co_buf_t*) co_list_pop_front(&p_plxs_env->wait_queue);
                co_buf_release(p_buf);
            }
        }

        // free profile environment variables
        p_env->p_env = NULL;
        ke_free(p_plxs_env);
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
__STATIC void plxs_con_create(prf_data_t *p_env, uint8_t conidx, const gap_con_param_t* p_con_param)
{
    plxs_env_t *p_plxs_env = (plxs_env_t *) p_env->p_env;
    p_plxs_env->evt_cfg[conidx] = 0;
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
__STATIC void plxs_con_cleanup(prf_data_t *p_env, uint8_t conidx, uint16_t reason)
{
    plxs_env_t *p_plxs_env = (plxs_env_t *) p_env->p_env;
    p_plxs_env->evt_cfg[conidx] = 0;
}

/// PLXS Task interface required by profile manager
const prf_task_cbs_t plxs_itf =
{
    .cb_init          = (prf_init_cb) plxs_init,
    .cb_destroy       = plxs_destroy,
    .cb_con_create    = plxs_con_create,
    .cb_con_cleanup   = plxs_con_cleanup,
    .cb_con_upd       = NULL,
};

/**
 ****************************************************************************************
 * @brief Retrieve service profile interface
 *
 * @return service profile interface
 ****************************************************************************************
 */
const prf_task_cbs_t *plxs_prf_itf_get(void)
{
    return &plxs_itf;
}
#endif //(BLE_PLX_SERVER)

/// @} PLXS

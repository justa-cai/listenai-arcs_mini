/**
 ****************************************************************************************
 *
 * @file glps.c
 *
 * @brief Glucose Profile Sensor implementation.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup GLPS
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "btip_config.h"

#if 1//(BLE_GL_SENSOR)
#include "../api/glps.h"
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
 ******* GLPS Configuration Flag Masks ******
 ********************************************/
/// GLS Configuration Flag Masks
#define GLPS_MANDATORY_MASK                (0x1F8F)
#define GLPS_MEAS_CTX_PRES_MASK            (0x0070)

#define GLPS_FILTER_USER_FACING_TIME_SIZE (7)

/// Cycling Speed and Cadence Service - Attribute List
enum glps_gls_att_list
{
    /// Glucose Service
    GLS_IDX_SVC,
    /// Glucose Measurement
    GLS_IDX_MEAS_CHAR,
    GLS_IDX_MEAS_VAL,
    GLS_IDX_MEAS_NTF_CFG,
    /// Glucose Measurement Context
    GLS_IDX_MEAS_CTX_CHAR,
    GLS_IDX_MEAS_CTX_VAL,
    GLS_IDX_MEAS_CTX_NTF_CFG,
    /// Glucose Feature
    GLS_IDX_FEATURE_CHAR,
    GLS_IDX_FEATURE_VAL,
    /// Record Access Control Point
    GLS_IDX_REC_ACCESS_CTRL_CHAR,
    GLS_IDX_REC_ACCESS_CTRL_VAL,
    GLS_IDX_REC_ACCESS_CTRL_IND_CFG,

    GLS_IDX_NB,
};


/// Characteristic Codes
enum glps_char_code
{
    /// Glucose Measurement
    GLS_MEAS_CHAR,
    /// Glucose Measurement Context
    GLS_MEAS_CTX_CHAR,
    /// Glucose Feature
    GLS_FEATURE_CHAR,
    /// Record Access Control Point
    GLS_REC_ACCESS_CTRL_CHAR,
};


/// Type of operation
enum glps_op_type
{
    /// Send Measurement
    GLPS_OP_MEAS_SEND,
    /// Send Measurement - Context data following
    GLPS_OP_MEAS_SEND_WITH_CTX,
    /// Send Measurement Context
    GLPS_OP_MEAS_CTX_SEND,
    /// Record Access Control Point Response Indication
    GLPS_OP_RACP_RSP_SEND
};

/// State Flag Bit field
enum glps_flag_bf
{
    /// True: Bond data set by application
    GPLS_BOND_DATA_PRESENT_BIT = 0x01,
    GPLS_BOND_DATA_PRESENT_POS = 0,
    /// True: Module is sending measurement data
    GPLS_SENDING_MEAS_BIT      = 0x02,
    GPLS_SENDING_MEAS_POS      = 1,
};
/*
 * MACROS
 ****************************************************************************************
 */

/// Get database attribute handle
#define GLPS_HANDLE(idx) \
    (p_glps_env->start_hdl + (idx) - \
        ((!(p_glps_env->meas_ctx_supported) && ((idx) > GLS_IDX_MEAS_CTX_NTF_CFG))? (3) : (0)))

/// Get database attribute index
#define GLPS_IDX(hdl) \
    (((((hdl) - p_glps_env->start_hdl) > GLS_IDX_MEAS_CTX_NTF_CFG) && !(p_glps_env->meas_ctx_supported)) ?\
        ((hdl) - p_glps_env->start_hdl + 3) : ((hdl) - p_glps_env->start_hdl))


/*
 * TYPES DEFINITION
 ****************************************************************************************
 */

/// ongoing operation information
typedef struct glps_buf_meta
{
    /// Attribute handle
    uint16_t  hdl;
    /// Event type
    uint16_t  evt_type;
    /// Operation
    uint8_t   operation;
    /// Connection index targeted
    uint8_t   conidx;
} glps_buf_meta_t;


/// Glucose Profile Sensor environment variable per connection
typedef struct glps_cnx_env
{
    /// Glucose service processing flags
    uint8_t flags;
    /// Event (notification/indication) configuration
    uint8_t evt_cfg;
} glps_cnx_env_t;

/// Glucose service server environment variable
typedef struct glps_env
{
    /// profile environment
    prf_hdr_t prf_env;
    /// Operation Event TX wait queue
    co_list_t wait_queue;
    /// Service Attribute Start Handle
    uint16_t  start_hdl;
    /// Glucose Feature
    uint16_t  features;
    /// Measurement context supported
    uint8_t   meas_ctx_supported;
    /// GATT user local identifier
    uint8_t   user_lid;
    /// Control point operation on-going (@see enum glp_racp_op_code)
    uint8_t   racp_op_code;
    /// Operation On-going
    bool      op_ongoing;
    /// Prevent recursion in execute_operation function
    bool      in_exe_op;
    /// Environment variable pointer for each connections
    glps_cnx_env_t env[BLE_CONNECTION_MAX];

} glps_env_t;


/*
 * ATTRIBUTES DATABASE
 ****************************************************************************************
 */

/// Full Database Description - Used to add attributes into the database
const gatt_att16_desc_t glps_att_db[GLS_IDX_NB] =
{
    // Glucose Service Declaration
    [GLS_IDX_SVC]                     = { GATT_DECL_PRIMARY_SERVICE,    PROP(RD),                                  0                                            },

    // Glucose Measurement Characteristic Declaration
    [GLS_IDX_MEAS_CHAR]               = { GATT_DECL_CHARACTERISTIC,     PROP(RD),                                  0                                            },
    // Glucose Measurement Characteristic Value
    [GLS_IDX_MEAS_VAL]                = { GATT_CHAR_GLUCOSE_MEAS,       PROP(N),                                   OPT(NO_OFFSET)                               },
    // Glucose Measurement Characteristic - Client Characteristic Configuration Descriptor
    [GLS_IDX_MEAS_NTF_CFG]            = { GATT_DESC_CLIENT_CHAR_CFG,    PROP(RD)|PROP(WR),                         OPT(NO_OFFSET)                               },

    // Glucose Measurement Context Characteristic Declaration
    [GLS_IDX_MEAS_CTX_CHAR]           = { GATT_DECL_CHARACTERISTIC,     PROP(RD),                                  0                                            },
    // Glucose Measurement Context Characteristic Value
    [GLS_IDX_MEAS_CTX_VAL]            = { GATT_CHAR_GLUCOSE_MEAS_CTX,   PROP(N),                                   OPT(NO_OFFSET)                               },
    // Glucose Measurement Context Characteristic - Client Characteristic Configuration Descriptor
    [GLS_IDX_MEAS_CTX_NTF_CFG]        = { GATT_DESC_CLIENT_CHAR_CFG,    PROP(RD)|PROP(WR),                         OPT(NO_OFFSET)                               },

    // Glucose Features Characteristic Declaration
    [GLS_IDX_FEATURE_CHAR]            = { GATT_DECL_CHARACTERISTIC,     PROP(RD),                                  0                                            },
    // Glucose Features Characteristic Value
    [GLS_IDX_FEATURE_VAL]             = { GATT_CHAR_GLUCOSE_FEATURE,    PROP(RD),                                  OPT(NO_OFFSET)                               },

    // Record Access Control Point characteristic Declaration
    [GLS_IDX_REC_ACCESS_CTRL_CHAR]    = { GATT_DECL_CHARACTERISTIC,     PROP(RD),                                  0                                            },
    // Record Access Control Point characteristic Value
    [GLS_IDX_REC_ACCESS_CTRL_VAL]     = { GATT_CHAR_REC_ACCESS_CTRL_PT, PROP(I) | SEC_LVL(WP, NO_AUTH) | PROP(WR), OPT(NO_OFFSET) | GLP_REC_ACCESS_CTRL_MAX_LEN },
    // Record Access Control Point characteristic - Client Characteristic Configuration Descriptor
    [GLS_IDX_REC_ACCESS_CTRL_IND_CFG] = { GATT_DESC_CLIENT_CHAR_CFG,    PROP(RD)|PROP(WR),                         OPT(NO_OFFSET)                               },
};

/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief  This function fully manages GATT event transmission
 ****************************************************************************************
 */
__STATIC void glps_exe_operation(glps_env_t* p_glps_env)
{
    if(!p_glps_env->in_exe_op)
    {
        p_glps_env->in_exe_op = true;

        while(!co_list_is_empty(&(p_glps_env->wait_queue)) && !(p_glps_env->op_ongoing))
        {
            uint16_t status = GAP_ERR_NO_ERROR;
            co_buf_t* p_buf = (co_buf_t*) co_list_pop_front(&(p_glps_env->wait_queue));
            glps_buf_meta_t* p_meta = (glps_buf_meta_t*) co_buf_metadata(p_buf);
            uint8_t  operation = p_meta->operation;
            uint8_t  conidx    = p_meta->conidx;

            // send GATT event
            status = gatt_srv_event_send(conidx, p_glps_env->user_lid, operation, p_meta->evt_type, p_meta->hdl, p_buf);

            if(status == GAP_ERR_NO_ERROR)
            {
                p_glps_env->op_ongoing = true;
            }

            co_buf_release(p_buf);

            if(!p_glps_env->op_ongoing)
            {
                if(operation == GLPS_OP_RACP_RSP_SEND)
                {
                    // Inform application that control point response has been sent
                    if (p_glps_env->racp_op_code != GLP_REQ_RSP_CODE)
                    {
                        const glps_cb_t* p_cb = (const glps_cb_t*) p_glps_env->prf_env.p_cb;
                        p_cb->cb_racp_rsp_send_cmp(conidx, status);
                    }

                    // consider control point operation done
                    p_glps_env->racp_op_code = GLP_REQ_RESERVED;
                }
                else
                {
                    const glps_cb_t* p_cb = (const glps_cb_t*) p_glps_env->prf_env.p_cb;
                    SETB(p_glps_env->env[conidx].flags, GPLS_SENDING_MEAS, false);

                    // drop context data not yet send
                    if(operation == GLPS_OP_MEAS_SEND_WITH_CTX)
                    {
                        p_buf = (co_buf_t*) co_list_pop_front(&(p_glps_env->wait_queue));
                        ASSERT_ERR(p_buf != NULL);
                        co_buf_release(p_buf);
                    }

                    // Inform application that event has been sent
                    p_cb->cb_meas_send_cmp(conidx, status);
                }
            }
        }

        p_glps_env->in_exe_op = false;
    }
}


/**
 ****************************************************************************************
 * @brief Packs measurement data
 *
 * @param[in]     p_glps_env   Environment data
 * @param[in]     p_buf        Pointer to output buffer
 * @param[in]     seq_num      Glucose measurement sequence number
 * @param[in]     p_meas       pointer to measurement information
 ****************************************************************************************
 */
__STATIC void glps_pack_meas(glps_env_t *p_glps_env, co_buf_t* p_buf, uint16_t seq_num, const glp_meas_t *p_meas)
{
    uint8_t meas_flags = p_meas->flags;

    // Flags
    co_buf_tail(p_buf)[0] =  meas_flags;
    co_buf_tail_reserve(p_buf, 1);

    // Sequence Number
    co_write16p(co_buf_tail(p_buf), co_htobs(seq_num));
    co_buf_tail_reserve(p_buf, 2);

    // Base Time
    prf_pack_date_time(p_buf, &(p_meas->base_time));

    // Time Offset
    if (GETB(meas_flags, GLP_MEAS_TIME_OFF_PRES))
    {
        co_write16p(co_buf_tail(p_buf), co_htobs(p_meas->time_offset));
        co_buf_tail_reserve(p_buf, 2);
    }

    // Glucose Concentration, type and location
    if (GETB(meas_flags, GLP_MEAS_GL_CTR_TYPE_AND_SPL_LOC_PRES))
    {
        co_write16p(co_buf_tail(p_buf), co_htobs(p_meas->concentration));
        co_buf_tail_reserve(p_buf, 2);
        // type and location are 2 nibble values
        co_buf_tail(p_buf)[0] = (p_meas->location << 4) | (p_meas->type);
        co_buf_tail_reserve(p_buf, 1);
    }

    // Sensor Status Annunciation
    if (GETB(meas_flags, GLP_MEAS_SENS_STAT_ANNUN_PRES))
    {
        // Use a non-const value
        uint16_t sensor_status = p_meas->sensor_status;

        // If feature not supported, corresponding Flag in the Sensor Status Annunciation Field
        // shall be set to default of 0

        // Low Battery Detection During Measurement Support Bit
        if (!GETB(p_glps_env->features, GLP_FET_LOW_BAT_DET_DUR_MEAS_SUPP))
        {
            SETB(sensor_status, GLP_MEAS_STATE_DEV_BAT_LOW, 0);
        }

        // Sensor Malfunction Detection Support Bit
        if (!GETB(p_glps_env->features, GLP_FET_SENS_MFNC_DET_SUPP))
        {
            SETB(sensor_status, GLP_MEAS_STATE_SENS_MFNC_OR_FLTING, 0);
        }

        // Sensor Sample Size Support Bit
        if (!GETB(p_glps_env->features, GLP_FET_SENS_SPL_SIZE_SUPP))
        {
            SETB(sensor_status, GLP_MEAS_STATE_SPL_SIZE_INSUFF, 0);
        }

        // Sensor Strip Insertion Error Detection Support Bit
        if (!GETB(p_glps_env->features, GLP_FET_SENS_STRIP_INSERT_ERR_DET_SUPP))
        {
            SETB(sensor_status, GLP_MEAS_STATE_STRIP_INSERT_ERR, 0);
        }

        // Sensor Result High-Low Detection Support Bit
        if (!GETB(p_glps_env->features, GLP_FET_SENS_RES_HIGH_LOW_DET_SUPP))
        {
            SETB(sensor_status, GLP_MEAS_STATE_SENS_RES_HIGHER, 0);
            SETB(sensor_status, GLP_MEAS_STATE_SENS_RES_LOWER, 0);
        }

        // Sensor Temperature High-Low Detection Support Bit
        if (!GETB(p_glps_env->features, GLP_FET_SENS_TEMP_HIGH_LOW_DET_SUPP))
        {
            SETB(sensor_status, GLP_MEAS_STATE_SENS_TEMP_TOO_HIGH, 0);
            SETB(sensor_status, GLP_MEAS_STATE_SENS_TEMP_TOO_LOW, 0);
        }

        // Sensor Read Interrupt Detection Support Bit
        if (!GETB(p_glps_env->features, GLP_FET_SENS_RD_INT_DET_SUPP))
        {
            SETB(sensor_status, GLP_MEAS_STATE_SENS_RD_INTED, 0);
        }

        // General Device Fault Support Bit
        if (!GETB(p_glps_env->features, GLP_FET_GEN_DEV_FLT_SUPP))
        {
            SETB(sensor_status, GLP_MEAS_STATE_GEN_DEV_FLT, 0);
        }

        // Time Fault Support Bit
        if (!GETB(p_glps_env->features, GLP_FET_TIME_FLT_SUPP))
        {
            SETB(sensor_status, GLP_MEAS_STATE_TIME_FLT, 0);
        }

        // Multiple Bond Support Bit
        if (!GETB(p_glps_env->features, GLP_FET_MUL_BOND_SUPP))
        {
            // can determine that the Glucose Sensor supports only a single bond
        }
        else
        {
            // Collector can determine that the Glucose supports multiple bonds
        }

        co_write16p(co_buf_tail(p_buf), co_htobs(sensor_status));
        co_buf_tail_reserve(p_buf, 2);
    }
}

/**
 ****************************************************************************************
 * @brief Packs context data
 *
 * @param[in]     p_glps_env   Environment data
 * @param[in]     p_buf        Pointer to output buffer
 * @param[in]     seq_num      Glucose measurement sequence number
 * @param[in]     p_ctx        Pointer to measurement context information
 ****************************************************************************************
 */
__STATIC void glps_pack_meas_ctx(glps_env_t *p_glps_env, co_buf_t* p_buf, uint16_t seq_num, const glp_meas_ctx_t* p_ctx)
{
    uint8_t meas_flags = p_ctx->flags;

    // Flags
    co_buf_tail(p_buf)[0] = meas_flags;
    co_buf_tail_reserve(p_buf, 1);

    // Sequence Number
    co_write16p(co_buf_tail(p_buf), co_htobs(seq_num));
    co_buf_tail_reserve(p_buf, 2);

    // Extended Flags
    if (GETB(meas_flags, GLP_CTX_EXTD_F_PRES) != 0)
    {
        co_buf_tail(p_buf)[0] = p_ctx->ext_flags;
        co_buf_tail_reserve(p_buf, 1);
    }

    // Carbohydrate ID And Carbohydrate Present
    if (GETB(meas_flags, GLP_CTX_CRBH_ID_AND_CRBH_PRES) != 0)
    {
        // Carbohydrate ID
        co_buf_tail(p_buf)[0] = p_ctx->carbo_id;
        co_buf_tail_reserve(p_buf, 1);
        // Carbohydrate Present
        co_write16p(co_buf_tail(p_buf), co_htobs(p_ctx->carbo_val));
        co_buf_tail_reserve(p_buf, 2);
    }

    // Meal Present
    if (GETB(meas_flags, GLP_CTX_MEAL_PRES) != 0)
    {
        co_buf_tail(p_buf)[0] = p_ctx->meal;
        co_buf_tail_reserve(p_buf, 1);
    }

    // Tester-Health Present
    if (GETB(meas_flags, GLP_CTX_TESTER_HEALTH_PRES) != 0)
    {
        // Tester and Health are 2 nibble values
        co_buf_tail(p_buf)[0] = (p_ctx->health << 4) | (p_ctx->tester);
        co_buf_tail_reserve(p_buf, 1);
    }

    // Exercise Duration & Exercise Intensity Present
    if (GETB(meas_flags, GLP_CTX_EXE_DUR_AND_EXE_INTENS_PRES) != 0)
    {
        // Exercise Duration
        co_write16p(co_buf_tail(p_buf), co_htobs(p_ctx->exercise_dur));
        co_buf_tail_reserve(p_buf, 2);
        // Exercise Intensity
        co_buf_tail(p_buf)[0] = p_ctx->exercise_intens;
        co_buf_tail_reserve(p_buf, 1);
    }

    // Medication ID And Medication Present
    if (GETB(meas_flags, GLP_CTX_MEDIC_ID_AND_MEDIC_PRES) != 0)
    {
        // Medication ID
        co_buf_tail(p_buf)[0] = p_ctx->med_id;
        co_buf_tail_reserve(p_buf, 1);
        // Medication Present
        co_write16p(co_buf_tail(p_buf), co_htobs(p_ctx->med_val));
        co_buf_tail_reserve(p_buf, 2);
    }

    // HbA1c Present
    if (GETB(meas_flags, GLP_CTX_HBA1C_PRES) != 0)
    {
        // HbA1c
        co_write16p(co_buf_tail(p_buf), co_htobs(p_ctx->hba1c_val));
        co_buf_tail_reserve(p_buf, 2);
    }
}

/**
 ****************************************************************************************
 * @brief Unpack control point data and process it
 *
 * @param[in] p_glps_env Environment
 * @param[in] conidx     connection index
 * @param[in] p_buf      pointer to input data
 ****************************************************************************************
 */
__STATIC uint16_t glps_unpack_racp_req(glps_env_t *p_glps_env, uint8_t conidx, co_buf_t* p_buf)
{
    uint16_t status = GAP_ERR_NO_ERROR;
    uint8_t racp_rsp_status = GLP_RSP_INVALID_OPERATOR;
    union glp_filter filter;
    uint8_t op_code = 0;
    uint8_t func_operator = 0;
    uint8_t filter_type = 0;
    memset(&filter, 0, sizeof(union glp_filter));

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
        if (op_code == GLP_REQ_ABORT_OP)
        {
            if(p_glps_env->racp_op_code == GLP_REQ_RESERVED)
            {
                // do nothing since a procedure already in progress
                racp_rsp_status = GLP_RSP_ABORT_UNSUCCESSFUL;
            }
            else
            {
                // Handle abort, no need to extract other info
                racp_rsp_status = GLP_RSP_SUCCESS;
            }
            break;
        }
        else if(p_glps_env->racp_op_code != GLP_REQ_RESERVED)
        {
            // do nothing since a procedure already in progress
            status = GLP_ERR_PROC_ALREADY_IN_PROGRESS;
            break;
        }

        // check if opcode is supported
        if ((op_code < GLP_REQ_REP_STRD_RECS) || (op_code > GLP_REQ_REP_NUM_OF_STRD_RECS))
        {
            racp_rsp_status = GLP_RSP_OP_CODE_NOT_SUP;
            break;
        }

        // check if operator is valid
        if (func_operator < GLP_OP_ALL_RECS)
        {
            racp_rsp_status = GLP_RSP_INVALID_OPERATOR;
            break;
        }
        // check if operator is supported
        else if (func_operator > GLP_OP_LAST_REC)
        {
            racp_rsp_status = GLP_RSP_OPERATOR_NOT_SUP;
            break;
        }

        // check if request requires operand (filter)
        if ((func_operator < GLP_OP_LT_OR_EQ) || (func_operator > GLP_OP_WITHIN_RANGE_OF))
        {
            racp_rsp_status = GLP_RSP_SUCCESS;
            break;
        }

        filter_type =  co_buf_data(p_buf)[0];
        if(co_buf_head_release(p_buf, 1) != CO_BUF_ERR_NO_ERROR) break;

        // filter uses sequence number
        if (filter_type == GLP_FILTER_SEQ_NUMBER)
        {
            // retrieve minimum value
            if ((func_operator == GLP_OP_GT_OR_EQ) || (func_operator == GLP_OP_WITHIN_RANGE_OF))
            {
                // check sufficient data available
                if (co_buf_data_len(p_buf) < 2) break;

                // retrieve minimum value
                filter.seq_num.min = co_btohs(co_read16p(co_buf_data(p_buf)));
                co_buf_head_release(p_buf, 2);
            }

            // retrieve maximum value
            if ((func_operator == GLP_OP_LT_OR_EQ) || (func_operator == GLP_OP_WITHIN_RANGE_OF))
            {
                // check sufficient data available
                if (co_buf_data_len(p_buf) < 2) break;

                // retrieve maximum value
                filter.seq_num.max = co_btohs(co_read16p(co_buf_data(p_buf)));
                co_buf_head_release(p_buf, 2);
            }

            // check that range value is valid
            if ((func_operator == GLP_OP_WITHIN_RANGE_OF) && (filter.seq_num.min > filter.seq_num.max)) break;
        }
        // filter uses user facing time
        else if (filter_type == GLP_FILTER_USER_FACING_TIME)
        {
            // retrieve minimum value
            if ((func_operator == GLP_OP_GT_OR_EQ) || (func_operator == GLP_OP_WITHIN_RANGE_OF))
            {
                // check sufficient data available
                if (co_buf_data_len(p_buf) < GLPS_FILTER_USER_FACING_TIME_SIZE) break;

                // retrieve minimum facing time
                prf_unpack_date_time(p_buf, &(filter.time.facetime_min));
            }

            // retrieve maximum value
            if ((func_operator == GLP_OP_LT_OR_EQ) || (func_operator == GLP_OP_WITHIN_RANGE_OF))
            {
                if (co_buf_data_len(p_buf) < GLPS_FILTER_USER_FACING_TIME_SIZE) break;

                // retrieve maximum facing time
                prf_unpack_date_time(p_buf, &(filter.time.facetime_max));
            }
        }
        else
        {
            racp_rsp_status = GLP_RSP_OPERAND_NOT_SUP;
            break;
        }

        // consider that data extraction is a sucess
        racp_rsp_status = GLP_RSP_SUCCESS;
    } while(0);


    if(status == GAP_ERR_NO_ERROR)
    {
        // If no error raised, inform the application about the request
        if (racp_rsp_status == GLP_RSP_SUCCESS)
        {
            const glps_cb_t* p_cb  = (const glps_cb_t*) p_glps_env->prf_env.p_cb;

            p_glps_env->racp_op_code  = op_code;

            // inform application about control point request
            p_cb->cb_racp_req(conidx, op_code, func_operator, filter_type, &filter);
        }
        else
        {
            co_buf_t* p_out_buf = NULL;

            if(co_buf_alloc(&p_out_buf, GATT_BUFFER_HEADER_LEN, 0, GLP_REC_ACCESS_CTRL_MAX_LEN + GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
            {
                glps_buf_meta_t* p_meta = (glps_buf_meta_t*)co_buf_metadata(p_out_buf);

                p_glps_env->racp_op_code  = GLP_REQ_RSP_CODE;
                co_buf_tail(p_out_buf)[0] = GLP_REQ_RSP_CODE;
                co_buf_tail_reserve(p_out_buf, 1);
                co_buf_tail(p_out_buf)[0] = 0;
                co_buf_tail_reserve(p_out_buf, 1);
                co_buf_tail(p_out_buf)[0] = op_code;
                co_buf_tail_reserve(p_out_buf, 1);
                co_buf_tail(p_out_buf)[0] = racp_rsp_status;
                co_buf_tail_reserve(p_out_buf, 1);

                p_meta->conidx    = conidx;
                p_meta->operation = GLPS_OP_RACP_RSP_SEND;
                p_meta->evt_type  = GATT_INDICATE;
                p_meta->hdl       = GLPS_HANDLE(GLS_IDX_REC_ACCESS_CTRL_VAL);

                // put event on wait queue
                co_list_push_back(&(p_glps_env->wait_queue), &(p_out_buf->hdr));
                // execute operation
                glps_exe_operation(p_glps_env);
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
 * @param[in] p_glps_env    Environment data
 * @param[in] conidx        Connection Index
 * @param[in] p_buf         Pointer to output buffer
 * @param[in] op_code       Requested Operation Code (@see enum glp_racp_op_code)
 * @param[in] racp_status   Record access control point execution status (@see enum glp_racp_status)
 * @param[in] num_of_record Number of record (meaningful for GLP_REQ_REP_NUM_OF_STRD_RECS operation)
 ****************************************************************************************
 */
void glps_pack_racp_rsp(glps_env_t *p_glps_env, uint8_t conidx, co_buf_t* p_buf, uint8_t op_code, uint8_t racp_status,
                        uint16_t num_of_record)
{
    bool num_recs_rsp = ((op_code == GLP_REQ_REP_NUM_OF_STRD_RECS) && (racp_status == GLP_RSP_SUCCESS));

    // Set the Response Code
    co_buf_tail(p_buf)[0] = num_recs_rsp ? GLP_REQ_NUM_OF_STRD_RECS_RSP : GLP_REQ_RSP_CODE;
    co_buf_tail_reserve(p_buf, 1);

    // set operator (null)
    co_buf_tail(p_buf)[0] = 0;
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
__STATIC void glps_cb_att_read_get(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset,
                                   uint16_t max_length)
{
    glps_env_t *p_glps_env = PRF_ENV_GET(GLPS, glps);
    // retrieve value attribute
    co_buf_t* p_buf       = NULL;
    uint16_t  status      = GAP_ERR_NO_ERROR;
    uint16_t  att_val_len = 0;

    if(p_glps_env == NULL)
    {
        status = PRF_APP_ERROR;
    }
    else if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, 0,  sizeof(uint16_t) + GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
    {
        uint8_t att_idx = GLPS_IDX(hdl);

        switch (att_idx)
        {
            case GLS_IDX_MEAS_NTF_CFG:
            {
                uint16_t ntf_cfg = GETB(p_glps_env->env[conidx].evt_cfg, GLPS_MEAS_NTF_CFG)
                                 ? PRF_CLI_START_NTF : PRF_CLI_STOP_NTFIND;

                co_write16p(co_buf_tail(p_buf), co_htobs(ntf_cfg));
                co_buf_tail_reserve(p_buf, 2);
            } break;

            case GLS_IDX_MEAS_CTX_NTF_CFG:
            {
                uint16_t ntf_cfg = GETB(p_glps_env->env[conidx].evt_cfg, GLPS_MEAS_CTX_NTF_CFG)
                                 ? PRF_CLI_START_NTF : PRF_CLI_STOP_NTFIND;

                co_write16p(co_buf_tail(p_buf), co_htobs(ntf_cfg));
                co_buf_tail_reserve(p_buf, 2);
            } break;

            case GLS_IDX_REC_ACCESS_CTRL_IND_CFG:
            {
                uint16_t ind_cfg = GETB(p_glps_env->env[conidx].evt_cfg, GLPS_RACP_IND_CFG)
                                 ? PRF_CLI_START_IND : PRF_CLI_STOP_NTFIND;

                co_write16p(co_buf_tail(p_buf), co_htobs(ind_cfg));
                co_buf_tail_reserve(p_buf, 2);
            } break;

            case GLS_IDX_FEATURE_VAL:
            {
                co_write16p(co_buf_tail(p_buf), co_htobs(p_glps_env->features));
                co_buf_tail_reserve(p_buf, 2);
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
__STATIC void glps_cb_att_val_set(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset,
                        co_buf_t* p_data)
{
    glps_env_t *p_glps_env = PRF_ENV_GET(GLPS, glps);
    uint16_t  status      = PRF_APP_ERROR;

    if(p_glps_env != NULL)
    {
        uint8_t cfg_upd_flag  = 0;
        uint16_t cfg_en_val = 0;

        switch (GLPS_IDX(hdl))
        {
            case GLS_IDX_MEAS_NTF_CFG:
            {
                cfg_upd_flag = GLPS_MEAS_NTF_CFG_BIT;
                cfg_en_val   = PRF_CLI_START_NTF;
            } break;

            case GLS_IDX_MEAS_CTX_NTF_CFG:
            {
                cfg_upd_flag = GLPS_MEAS_CTX_NTF_CFG_BIT;
                cfg_en_val   = PRF_CLI_START_NTF;
            } break;

            case GLS_IDX_REC_ACCESS_CTRL_IND_CFG:
            {
                cfg_upd_flag = GLPS_RACP_IND_CFG_BIT;
                cfg_en_val   = PRF_CLI_START_IND;
            } break;

            case GLS_IDX_REC_ACCESS_CTRL_VAL:
            {
                // Check if sending of indications has been enabled
                if (!GETB(p_glps_env->env[conidx].evt_cfg, GLPS_RACP_IND_CFG))
                {
                    // CPP improperly configured
                    status = GLP_ERR_IMPROPER_CLI_CHAR_CFG;
                }
                else
                {
                    // Unpack Control Point parameters
                    status = glps_unpack_racp_req(p_glps_env, conidx, p_data);
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
                const glps_cb_t* p_cb  = (const glps_cb_t*) p_glps_env->prf_env.p_cb;

                if(cfg == PRF_CLI_STOP_NTFIND)
                {
                    p_glps_env->env[conidx].evt_cfg &= ~cfg_upd_flag;
                }
                else
                {
                    p_glps_env->env[conidx].evt_cfg |= cfg_upd_flag;
                }

                // inform application about update
                p_cb->cb_bond_data_upd(conidx, p_glps_env->env[conidx].evt_cfg);
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
__STATIC void glps_cb_event_sent(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    // Consider job done
    glps_env_t *p_glps_env = PRF_ENV_GET(GLPS, glps);
    if(p_glps_env != NULL)
    {
        const glps_cb_t* p_cb  = (const glps_cb_t*) p_glps_env->prf_env.p_cb;
        p_glps_env->op_ongoing = false;

        switch(dummy)
        {
            case GLPS_OP_MEAS_SEND_WITH_CTX:
            {
                if(status != GAP_ERR_NO_ERROR)
                {
                    co_buf_t* p_buf = (co_buf_t*) co_list_pop_front(&(p_glps_env->wait_queue));
                    ASSERT_ERR(p_buf != NULL);
                    co_buf_release(p_buf);
                }
                else
                {
                    break;
                }
            }
            // no break
            case GLPS_OP_MEAS_SEND:
            case GLPS_OP_MEAS_CTX_SEND:
            {
                SETB(p_glps_env->env[conidx].flags, GPLS_SENDING_MEAS, false);
                p_cb->cb_meas_send_cmp(conidx, status);
            } break;
            case GLPS_OP_RACP_RSP_SEND:
            {
                // Inform application that control point response has been sent
                if (p_glps_env->racp_op_code != GLP_REQ_RSP_CODE)
                {
                    p_cb->cb_racp_rsp_send_cmp(conidx, status);
                }

                p_glps_env->racp_op_code = GLP_REQ_RESERVED;
            } break;
            default: { /* Nothing to do */ } break;
        }

        // continue operation execution
        glps_exe_operation(p_glps_env);
    }
}

/// Service callback hander
__STATIC const gatt_srv_cb_t glps_cb =
{
        .cb_event_sent    = glps_cb_event_sent,
        .cb_att_read_get  = glps_cb_att_read_get,
        .cb_att_event_get = NULL,
        .cb_att_info_get  = NULL,
        .cb_att_val_set   = glps_cb_att_val_set,
};

/*
 * PROFILE NATIVE HANDLERS
 ****************************************************************************************
 */

uint16_t glps_enable(uint8_t conidx, uint8_t evt_cfg)
{
    glps_env_t* p_glps_env = PRF_ENV_GET(GLPS, glps);
    uint16_t status = PRF_ERR_REQ_DISALLOWED;

    if(p_glps_env != NULL)
    {
        // check state of the task
        if (gapc_get_conhdl(conidx) != GAP_INVALID_CONHDL)
        {
            if(!GETB(p_glps_env->env[conidx].flags, GPLS_BOND_DATA_PRESENT))
            {
                SETB(p_glps_env->env[conidx].flags, GPLS_BOND_DATA_PRESENT, true);
                p_glps_env->env[conidx].evt_cfg = evt_cfg;
            }

            status = GAP_ERR_NO_ERROR;
        }
    }

    return (status);
}

uint16_t glps_meas_send(uint8_t conidx, uint16_t seq_num, const glp_meas_t* p_meas, const glp_meas_ctx_t* p_ctx)
{
    glps_env_t* p_glps_env = PRF_ENV_GET(GLPS, glps);
    uint16_t status = PRF_ERR_REQ_DISALLOWED;

    if(p_meas == NULL)
    {
        status = GAP_ERR_INVALID_PARAM;
    }
    else if((p_glps_env != NULL) && (gapc_get_conhdl(conidx) != GAP_INVALID_CONHDL))
    {
        // Cannot send another measurement in parallel
        if(!GETB(p_glps_env->env[conidx].flags, GPLS_SENDING_MEAS))
        {
            co_buf_t* p_buf_meas;
            co_buf_t* p_buf_meas_ctx;
            glps_buf_meta_t* p_buf_meta;

            // check if context is supported
            if ((p_ctx != NULL) && !(p_glps_env->meas_ctx_supported))
            {
                // Context not supported
                status = PRF_ERR_FEATURE_NOT_SUPPORTED;
            }
            // check if notifications enabled
            else if (   !GETB(p_glps_env->env[conidx].evt_cfg, GLPS_MEAS_NTF_CFG)
                     || (!GETB(p_glps_env->env[conidx].evt_cfg, GLPS_MEAS_CTX_NTF_CFG) && (p_ctx != NULL)))
            {
                // Not allowed to send measurement if Notifications not enabled.
                status = (PRF_ERR_NTF_DISABLED);
            }
            else if(co_buf_alloc(&p_buf_meas, GATT_BUFFER_HEADER_LEN, 0, GLP_MEAS_MAX_LEN + GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
            {
                p_buf_meta = (glps_buf_meta_t*) co_buf_metadata(p_buf_meas);
                p_buf_meta->operation = (p_ctx != NULL) ? GLPS_OP_MEAS_SEND_WITH_CTX : GLPS_OP_MEAS_SEND;
                p_buf_meta->conidx    = conidx;
                p_buf_meta->evt_type  = GATT_NOTIFY;
                p_buf_meta->hdl       = GLPS_HANDLE(GLS_IDX_MEAS_VAL);

                // pack measurement
                glps_pack_meas(p_glps_env, p_buf_meas, seq_num, p_meas);
                status = GAP_ERR_NO_ERROR;

                if(p_ctx != NULL)
                {
                    if(co_buf_alloc(&p_buf_meas_ctx, GATT_BUFFER_HEADER_LEN, 0, GLP_MEAS_CTX_MAX_LEN + GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
                    {
                        p_buf_meta = (glps_buf_meta_t*) co_buf_metadata(p_buf_meas_ctx);
                        p_buf_meta->operation = GLPS_OP_MEAS_CTX_SEND;
                        p_buf_meta->conidx    = conidx;
                        p_buf_meta->evt_type  = GATT_NOTIFY;
                        p_buf_meta->hdl       = GLPS_HANDLE(GLS_IDX_MEAS_CTX_VAL);

                        // pack measurement
                        glps_pack_meas_ctx(p_glps_env, p_buf_meas_ctx, seq_num, p_ctx);
                    }
                    else
                    {
                        status = GAP_ERR_INSUFF_RESOURCES;
                        co_buf_release(p_buf_meas);
                    }
                }
            }
            else
            {
                status = GAP_ERR_INSUFF_RESOURCES;
            }

            if(status == GAP_ERR_NO_ERROR)
            {
                SETB(p_glps_env->env[conidx].flags, GPLS_SENDING_MEAS, true);

                // put event(s) on wait queue
                co_list_push_back(&(p_glps_env->wait_queue), &(p_buf_meas->hdr));
                if(p_ctx != NULL)
                {
                    co_list_push_back(&(p_glps_env->wait_queue), &(p_buf_meas_ctx->hdr));
                }
                // execute operation
                glps_exe_operation(p_glps_env);
            }
        }
    }

    return (status);
}

uint16_t glps_racp_rsp_send(uint8_t conidx, uint8_t op_code, uint8_t racp_status, uint16_t num_of_record)
{
    glps_env_t* p_glps_env = PRF_ENV_GET(GLPS, glps);
    uint16_t status = PRF_ERR_REQ_DISALLOWED;

    if(p_glps_env != NULL)
    {
        do
        {
            co_buf_t* p_buf = NULL;

            // check if op code valid
            if ((op_code < GLP_REQ_REP_STRD_RECS) || (op_code > GLP_REQ_REP_NUM_OF_STRD_RECS))
            {
                // Wrong op code
                status = PRF_ERR_INVALID_PARAM;
                break;
            }
            // check if RACP on going
            else if ((op_code != GLP_REQ_ABORT_OP) && (p_glps_env->racp_op_code != op_code))
            {
                // Cannot send response since no RACP on going
                break;
            }

            // Check the current operation
            if (p_glps_env->racp_op_code == GLP_REQ_RESERVED)
            {
                // The confirmation has been sent without request indication, ignore
                break;
            }

            // Check if sending of indications has been enabled
            if (!GETB(p_glps_env->env[conidx].evt_cfg, GLPS_RACP_IND_CFG))
            {
                // mark operation done
                p_glps_env->racp_op_code = GLP_REQ_RESERVED;
                // CPP improperly configured
                status = PRF_ERR_IND_DISABLED;
                break;
            }

            if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, 0, GLP_REC_ACCESS_CTRL_MAX_LEN + GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
            {
                glps_buf_meta_t* p_buf_meta = (glps_buf_meta_t*) co_buf_metadata(p_buf);
                p_buf_meta->operation = GLPS_OP_RACP_RSP_SEND;
                p_buf_meta->conidx    = conidx;
                p_buf_meta->hdl       = GLPS_HANDLE(GLS_IDX_REC_ACCESS_CTRL_VAL);
                p_buf_meta->evt_type  = GATT_INDICATE;

                // Pack structure
                glps_pack_racp_rsp(p_glps_env, conidx, p_buf, op_code, racp_status, num_of_record);
                // put event on wait queue
                co_list_push_back(&(p_glps_env->wait_queue), &(p_buf->hdr));
                // execute operation
                glps_exe_operation(p_glps_env);
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
 * @brief Send a GLPS_CMP_EVT message to the application.
 * @param[in] conidx    Connection index
 * @param[in] request   Completed request (@see enum glps_request_type)
 * @param[in] status    Status of the operation
 ****************************************************************************************
 */
__STATIC void glps_send_cmp_evt(uint8_t conidx, uint8_t request, uint16_t status)
{
    struct glps_cmp_evt *p_evt;

    // Send the message
    p_evt = KE_MSG_ALLOC(GLPS_CMP_EVT, PRF_DST_TASK(GLPS), PRF_SRC_TASK(GLPS), glps_cmp_evt);

    if(p_evt != NULL)
    {
        p_evt->conidx     = conidx;
        p_evt->request    = request;
        p_evt->status     = status;
        ke_msg_send(p_evt);
    }
}


/**
 ****************************************************************************************
 * @brief Handles reception of the @ref GLPS_ENABLE_REQ message.
 * @param[in] msgid Id of the message received
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int glps_enable_req_handler(ke_msg_id_t const msgid, struct glps_enable_req *p_param,
                                     ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    struct glps_enable_rsp *p_cmp_evt;
    uint16_t status = glps_enable(p_param->conidx, p_param->evt_cfg);

    // send completed information to APP task that contains error status
    p_cmp_evt = KE_MSG_ALLOC(GLPS_ENABLE_RSP, src_id, dest_id, glps_enable_rsp);

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
 * @brief Handles reception of the @ref GLPS_SEND_MEAS_WITHOUT_CTX_CMD message.
 * @param[in] msgid Id of the message received (probably unused).
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int glps_meas_send_without_ctx_cmd_handler(ke_msg_id_t const msgid,
                                                    struct glps_send_meas_without_ctx_cmd const *p_param,
                                                    ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = glps_meas_send(p_param->conidx, p_param->seq_num, &(p_param->meas), NULL);

    if(status != GAP_ERR_NO_ERROR)
    {
        glps_send_cmp_evt(p_param->conidx, GLPS_SEND_MEAS, status);
    }

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief Handles reception of the @ref GLPS_SEND_MEAS_WITHOUT_CTX_CMD message.
 * @param[in] msgid Id of the message received (probably unused).
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int glps_meas_send_with_ctx_cmd_handler(ke_msg_id_t const msgid,
                                                 struct glps_send_meas_with_ctx_cmd const *p_param,
                                                 ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = glps_meas_send(p_param->conidx, p_param->seq_num, &(p_param->meas), &(p_param->ctx));

    if(status != GAP_ERR_NO_ERROR)
    {
        glps_send_cmp_evt(p_param->conidx, GLPS_SEND_MEAS, status);
    }

    return (KE_MSG_CONSUMED);
}


/**
 ****************************************************************************************
 * @brief Handles reception of the @ref  message.
 * @param[in] msgid Id of the message received.
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int glps_send_racp_rsp_cmd_handler(ke_msg_id_t const msgid, struct glps_send_racp_rsp_cmd *p_param,
                                            ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = glps_racp_rsp_send(p_param->conidx, p_param->op_code, p_param->racp_status, p_param->num_of_record);

    if(status != GAP_ERR_NO_ERROR)
    {
        glps_send_cmp_evt(p_param->conidx, GLPS_SEND_RACP_RSP, status);
    }

    return (KE_MSG_CONSUMED);
}

/// Default State handlers definition
KE_MSG_HANDLER_TAB(glps)
{
    // Note: all messages must be sorted in ID ascending order

    {GLPS_ENABLE_REQ,                 (ke_msg_func_t) glps_enable_req_handler},
    {GLPS_SEND_RACP_RSP_CMD,          (ke_msg_func_t) glps_send_racp_rsp_cmd_handler},
    {GLPS_SEND_MEAS_WITH_CTX_CMD,     (ke_msg_func_t) glps_meas_send_with_ctx_cmd_handler},
    {GLPS_SEND_MEAS_WITHOUT_CTX_CMD,  (ke_msg_func_t) glps_meas_send_without_ctx_cmd_handler},
};

/**
 ****************************************************************************************
 * @brief Completion of measurement transmission
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 ****************************************************************************************
 */
__STATIC void glps_cb_meas_send_cmp(uint8_t conidx, uint16_t status)
{
    glps_send_cmp_evt(conidx, GLPS_SEND_MEAS, status);
}

/**
 ****************************************************************************************
 * @brief Inform that bond data updated for the connection.
 *
 * @param[in] conidx        Connection index
 * @param[in] evt_cfg       Glucose indication/notification configuration (@see enum glps_evt_cfg_bf)
 ****************************************************************************************
 */
__STATIC void glps_cb_bond_data_upd(uint8_t conidx, uint8_t evt_cfg)
{
    struct glps_cfg_indntf_ind *p_evt;

    // Send the message
    p_evt = KE_MSG_ALLOC(GLPS_CFG_INDNTF_IND, PRF_DST_TASK(GLPS),
                         PRF_SRC_TASK(GLPS), glps_cfg_indntf_ind);

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
 * @note control point request must be answered using @see glps_racp_rsp_send function
 *
 * @param[in] conidx        Connection index
 * @param[in] op_code       Operation Code (@see glp_racp_op_code)
 * @param[in] func_operator Function operator (see enum glp_racp_operator)
 * @param[in] filter_type   Filter type (@see enum glp_racp_filter)
 * @param[in] p_filter      Pointer to filter information
 ****************************************************************************************
 */
__STATIC void glps_cb_racp_req(uint8_t conidx, uint8_t op_code, uint8_t func_operator,
                               uint8_t filter_type, const union glp_filter* p_filter)
{
    struct glps_racp_req_rcv_ind *p_evt;

    // Send the message
    p_evt = KE_MSG_ALLOC(GLPS_RACP_REQ_RCV_IND, PRF_DST_TASK(GLPS),
                         PRF_SRC_TASK(GLPS), glps_racp_req_rcv_ind);

    if(p_evt != NULL)
    {
        p_evt->conidx      = conidx;
        p_evt->req.op_code     = op_code;
        p_evt->req.operator    = func_operator;
        p_evt->req.filter_type = filter_type;
        memcpy(&(p_evt->req.filter), p_filter, sizeof(union glp_filter));
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
__STATIC void glps_cb_racp_rsp_send_cmp(uint8_t conidx, uint16_t status)
{
    glps_send_cmp_evt(conidx, GLPS_SEND_RACP_RSP, status);
}


/// Default Message handle
__STATIC const glps_cb_t glps_msg_cb =
{
        .cb_meas_send_cmp        = glps_cb_meas_send_cmp,
        .cb_bond_data_upd        = glps_cb_bond_data_upd,
        .cb_racp_req             = glps_cb_racp_req,
        .cb_racp_rsp_send_cmp    = glps_cb_racp_rsp_send_cmp,
};
#endif // (BLE_HL_MSG_API)

/*
 * PROFILE DEFAULT HANDLERS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Initialization of the GLPS module.
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
__STATIC uint16_t glps_init(prf_data_t *p_env, uint16_t *p_start_hdl, uint8_t sec_lvl, uint8_t user_prio,
                          struct glps_db_cfg *p_params, const glps_cb_t* p_cb)
{
    //------------------ create the attribute database for the profile -------------------

    // DB Creation Status
    uint16_t status = GAP_ERR_NO_ERROR;
    uint8_t user_lid = GATT_INVALID_USER_LID;

    do
    {
        glps_env_t* p_glps_env;
        // Service content flag
        uint32_t cfg_flag = GLPS_MANDATORY_MASK;

        #if (BLE_HL_MSG_API)
        if(p_cb == NULL)
        {
            p_cb = &(glps_msg_cb);
        }
        #endif // (BLE_HL_MSG_API)

        if(   (p_params == NULL) || (p_start_hdl == NULL) || (p_cb == NULL) || (p_cb->cb_meas_send_cmp == NULL)
           || (p_cb->cb_bond_data_upd == NULL) || (p_cb->cb_racp_req == NULL) || (p_cb->cb_racp_rsp_send_cmp == NULL))
        {
            status = GAP_ERR_INVALID_PARAM;
            break;
        }

        // register GLPS user
        status = gatt_user_srv_register(L2CAP_LE_MTU_MIN, user_prio, &glps_cb, &user_lid);
        if(status != GAP_ERR_NO_ERROR) break;

        // Set Configuration Flag Value
        if (p_params->meas_ctx_supported)
        {
            cfg_flag |= GLPS_MEAS_CTX_PRES_MASK;
        }

        // Add GAP service
        status = gatt_db_svc16_add(user_lid, sec_lvl, GATT_SVC_GLUCOSE, GLS_IDX_NB,
                                   (uint8_t *)&cfg_flag, &(glps_att_db[0]), GLS_IDX_NB, p_start_hdl);
        if(status != GAP_ERR_NO_ERROR) break;

        //-------------------- allocate memory required for the profile  ---------------------
        p_glps_env = (glps_env_t *) ke_malloc(sizeof(glps_env_t), KE_MEM_ATT_DB);

        if(p_glps_env != NULL)
        {
            // allocate GLPS required environment variable
            p_env->p_env = (prf_hdr_t *) p_glps_env;
            p_glps_env->start_hdl          = *p_start_hdl;
            p_glps_env->features           = p_params->features;
            p_glps_env->user_lid           = user_lid;
            p_glps_env->meas_ctx_supported = p_params->meas_ctx_supported;
            p_glps_env->op_ongoing         = false;
            p_glps_env->in_exe_op          = false;
            p_glps_env->racp_op_code       = GLP_REQ_RESERVED;
            memset(p_glps_env->env, 0, sizeof(p_glps_env->env));
            co_list_init(&(p_glps_env->wait_queue));

            // initialize profile environment variable
            p_glps_env->prf_env.p_cb     = p_cb;
            #if (BLE_HL_MSG_API)
            p_env->desc.msg_handler_tab  = glps_msg_handler_tab;
            p_env->desc.msg_cnt          = ARRAY_LEN(glps_msg_handler_tab);
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
__STATIC uint16_t glps_destroy(prf_data_t *p_env, uint8_t reason)
{
    uint16_t status = GAP_ERR_NO_ERROR;
    glps_env_t *p_glps_env = (glps_env_t *) p_env->p_env;

    if(reason != PRF_DESTROY_RESET)
    {
        status = gatt_user_unregister(p_glps_env->user_lid);
    }

    if(status == GAP_ERR_NO_ERROR)
    {
        if(reason != PRF_DESTROY_RESET)
        {
            // remove buffer in wait queue
            while(!co_list_is_empty(&p_glps_env->wait_queue))
            {
                co_buf_t* p_buf = (co_buf_t*) co_list_pop_front(&p_glps_env->wait_queue);
                co_buf_release(p_buf);
            }
        }

        // free profile environment variables
        p_env->p_env = NULL;
        ke_free(p_glps_env);
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
__STATIC void glps_con_create(prf_data_t *p_env, uint8_t conidx, const gap_con_param_t* p_con_param)
{
    glps_env_t *p_glps_env = (glps_env_t *) p_env->p_env;
    memset(&(p_glps_env->env[conidx]), 0, sizeof(glps_cnx_env_t));
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
__STATIC void glps_con_cleanup(prf_data_t *p_env, uint8_t conidx, uint16_t reason)
{
    glps_env_t *p_glps_env = (glps_env_t *) p_env->p_env;
    memset(&(p_glps_env->env[conidx]), 0, sizeof(glps_cnx_env_t));
}

/// GLPS Task interface required by profile manager
const prf_task_cbs_t glps_itf =
{
    .cb_init          = (prf_init_cb) glps_init,
    .cb_destroy       = glps_destroy,
    .cb_con_create    = glps_con_create,
    .cb_con_cleanup   = glps_con_cleanup,
    .cb_con_upd       = NULL,
};

/**
 ****************************************************************************************
 * @brief Retrieve service profile interface
 *
 * @return service profile interface
 ****************************************************************************************
 */
const prf_task_cbs_t *glps_prf_itf_get(void)
{
    return &glps_itf;
}

#endif /* BLE_GL_SENSOR */

/// @} GLPS

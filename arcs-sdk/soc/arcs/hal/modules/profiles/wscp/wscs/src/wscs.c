/**
 ****************************************************************************************
 *
 * @file wscs.c
 *
 * @brief Weight SCale Profile Sensor implementation.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup WSCS
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "btip_config.h"

#if (BLE_WSC_SERVER)

#include "wscs.h"
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


/// default read perm
#define RD_P   (PROP(RD))
/// default write perm
#define WR_P   (PROP(WR))
/// ind perm
#define IND_P  (PROP(I))


/// Weight SCale Service - Attribute List
enum wscs_att_list
{
    /// Weight SCale Service
    WSCS_IDX_SVC,
    /// Included Body Composition Service
    WSCS_IDX_INC_SVC,
    /// Weight SCale Feature Characteristic
    WSCS_IDX_FEAT_CHAR,
    WSCS_IDX_FEAT_VAL,
    /// Weight SCale Measurement Characteristic
    WSCS_IDX_MEAS_CHAR,
    WSCS_IDX_MEAS_VAL,
    /// CCC Descriptor
    WSCS_IDX_MEAS_CCC,

    /// Number of attributes
    WSCS_IDX_NB,
};

/*
 * MACROS
 ****************************************************************************************
 */

#define WSCS_HANDLE(idx) \
    (p_wscs_env->start_hdl + (idx) - \
        ((!p_wscs_env->bcs_included && ((idx) > WSCS_IDX_INC_SVC))? (1) : (0)))

/*
 * TYPES DEFINITION
 ****************************************************************************************
 */

/// ongoing operation information
typedef struct wscs_buf_meta
{
    /// Operation
    uint8_t   operation;
    /// Connection index targeted
    uint8_t   conidx;
} wscs_buf_meta_t;

/// Running speed and cadence server environment variable
typedef struct wscs_env
{
    /// profile environment
    prf_hdr_t prf_env;
    /// Operation Event TX wait queue
    co_list_t wait_queue;
    /// Bit field that manage indication configuration for all connections
    uint32_t  ind_cfg_bf;
    /// Service Attribute Start Handle
    uint16_t  start_hdl;
    /// Feature configuration
    uint32_t  feature;
    /// BCS Service Included
    bool      bcs_included;
    /// GATT user local identifier
    uint8_t   user_lid;
    /// Operation On-going
    bool      op_ongoing;
    /// Prevent recursion in execute_operation function
    bool      in_exe_op;

} wscs_env_t;


/*
 * ATTRIBUTES DATABASE
 ****************************************************************************************
 */

/// Full WSCS Database Description - Used to add attributes into the database
__STATIC const gatt_att16_desc_t wscs_att_db[WSCS_IDX_NB] =
{
    /// ATT Index              | ATT UUID                     | Permission  | EXT_INFO
    // Weight SCale Service Declaration
    [WSCS_IDX_SVC]          = { GATT_DECL_PRIMARY_SERVICE,      RD_P,        0               },

    [WSCS_IDX_INC_SVC]      = { GATT_DECL_INCLUDE,              RD_P,        0               },
    // Weight SCale Feature
    [WSCS_IDX_FEAT_CHAR]    = { GATT_DECL_CHARACTERISTIC,       RD_P,        0               },
    // Descriptor Value Changed Characteristic Value
    [WSCS_IDX_FEAT_VAL]     = { GATT_CHAR_WEIGHT_SCALE_FEATURE, RD_P,        OPT(NO_OFFSET)  },

    // Weight SCale Measurement
    [WSCS_IDX_MEAS_CHAR]    = { GATT_DECL_CHARACTERISTIC,       RD_P,        0               },
    // Weight SCale Measurement Characteristic Value
    [WSCS_IDX_MEAS_VAL]     = { GATT_CHAR_WEIGHT_MEASUREMENT,   IND_P,       OPT(NO_OFFSET)  },
    // Weight SCale Measurement Characteristic - Client Characteristic Configuration Descriptor
    [WSCS_IDX_MEAS_CCC]     = { GATT_DESC_CLIENT_CHAR_CFG,      RD_P | WR_P, OPT(NO_OFFSET)  },
};


/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */

/// Get database attribute index
__STATIC uint8_t wscs_idx_get(wscs_env_t* p_wscs_env, uint16_t hdl)
{
    uint8_t att_idx = hdl - p_wscs_env->start_hdl;

    if((att_idx >= WSCS_IDX_INC_SVC) && !p_wscs_env->bcs_included)
    {
        att_idx += 1;
    }

    return att_idx;
}



/**
 ****************************************************************************************
 * @brief Get Weight Resolution from feature value
 *
 * @param[in] feature Weight Scale Feature value
 *
 * @return Mass Resolution
 ****************************************************************************************
 */
__STATIC uint8_t wscs_get_wght_resol(uint32_t feature)
{
    uint8_t mass_resol;

    // Imperial - Weight and Mass in units of pound (lb) and Height in units of inch (in)
    switch(GETF(feature, WSC_FEAT_WGHT_RESOL))
    {
        case WSC_WGHT_RESOL_05kg_1lb:      { mass_resol = WSC_WGHT_RESOL_05kg_1lb;      } break;
        case WSC_WGHT_RESOL_02kg_05lb:     { mass_resol = WSC_WGHT_RESOL_02kg_05lb;     } break;
        case WSC_WGHT_RESOL_01kg_02lb:     { mass_resol = WSC_WGHT_RESOL_01kg_02lb;     } break;
        case WSC_WGHT_RESOL_005kg_01lb:    { mass_resol = WSC_WGHT_RESOL_005kg_01lb;    } break;
        case WSC_WGHT_RESOL_002kg_005lb:   { mass_resol = WSC_WGHT_RESOL_002kg_005lb;   } break;
        case WSC_WGHT_RESOL_001kg_002lb:   { mass_resol = WSC_WGHT_RESOL_001kg_002lb;   } break;
        case WSC_WGHT_RESOL_0005kg_001lb:  { mass_resol = WSC_WGHT_RESOL_0005kg_001lb;  } break;
        case WSC_WGHT_RESOL_NOT_SPECIFIED:
        default :                          { mass_resol = WSC_WGHT_RESOL_NOT_SPECIFIED; } break;
    }

    return mass_resol;
}

/**
 ****************************************************************************************
 * @brief Get Height Resolution from feature value
 *
 * @param[in] feature Weight Scale Feature value
 *
 * @return Height Resolution
 ****************************************************************************************
 */
__STATIC uint8_t wscs_get_hght_resol(uint32_t feature)
{
    uint8_t hght_resol;

    // Imperial - Weight and Mass in units of pound (lb) and Height in units of inch (in)
    switch(GETF(feature, WSC_FEAT_HGHT_RESOL))
    {
        case WSC_HGHT_RESOL_001mtr_1inch:   { hght_resol = WSC_HGHT_RESOL_001mtr_1inch;   } break;
        case WSC_HGHT_RESOL_0005mtr_05inch: { hght_resol = WSC_HGHT_RESOL_0005mtr_05inch; } break;
        case WSC_HGHT_RESOL_0001mtr_01inch: { hght_resol = WSC_HGHT_RESOL_0001mtr_01inch; } break;
        case WSC_HGHT_RESOL_NOT_SPECIFIED:
        default:                            { hght_resol = WSC_HGHT_RESOL_NOT_SPECIFIED;  } break;
    }

    return hght_resol;
}

/**
 ****************************************************************************************
 * @brief  This function fully manages notification of measurement and vector
 ****************************************************************************************
 */
__STATIC void wscs_exe_operation(wscs_env_t* p_wscs_env)
{
    if(!p_wscs_env->in_exe_op)
    {
        p_wscs_env->in_exe_op = true;

        while(!co_list_is_empty(&(p_wscs_env->wait_queue)) && !(p_wscs_env->op_ongoing))
        {
            uint16_t status = GAP_ERR_NO_ERROR;
            co_buf_t* p_buf = (co_buf_t*) co_list_pop_front(&(p_wscs_env->wait_queue));
            wscs_buf_meta_t* p_meta = (wscs_buf_meta_t*) co_buf_metadata(p_buf);
            uint8_t conidx;

            switch(p_meta->operation)
            {
                case WSCS_MEAS_INDICATE_CMD_OP_CODE:
                {
                    conidx = p_meta->conidx;

                    // send indication to peer device
                    status = gatt_srv_event_send(conidx, p_wscs_env->user_lid, p_meta->operation, GATT_INDICATE,
                                                 WSCS_HANDLE(WSCS_IDX_MEAS_VAL), p_buf);
                    co_buf_release(p_buf);

                    if(status == GAP_ERR_NO_ERROR)
                    {
                        p_wscs_env->op_ongoing = true;
                    }
                    else
                    {
                        const wscs_cb_t* p_cb = (const wscs_cb_t*) p_wscs_env->prf_env.p_cb;
                        p_cb->cb_meas_send_cmp(conidx, status);
                    }

                } break;
                default: { ASSERT_ERR(0); } break;
            }
        }

        p_wscs_env->in_exe_op = false;
    }
}


/**
 ****************************************************************************************
 * @brief Packs measurement data
 *
 * @param[in]     p_wscs_env   Environment data
 * @param[in]     p_buf         Pointer to output buffer
 * @param[in]     p_meas        pointer to measurement information
 ****************************************************************************************
 */
__STATIC void wscs_pack_meas(wscs_env_t *p_wscs_env, co_buf_t* p_buf, const wsc_meas_t *p_meas)
{
    uint8_t meas_flags = p_meas->flags & WSC_MEAS_FLAGS_VALID;

    // If the Weight indicates Measurement Unsuccessful - 0xFFFF
    // The the flags field should not include BMI and Height
    if (p_meas->weight == WSC_MEASUREMENT_UNSUCCESSFUL)
    {
        // Disable all other optional fields other than Timestamp and User ID
        SETB(meas_flags, WSC_MEAS_FLAGS_BMI_PRESENT, 0);
    }

    // Time Stamp shall be included in flags field if the Server supports Time Stamp feature
    SETB(meas_flags, WSC_MEAS_FLAGS_TIMESTAMP_PRESENT, GETB(p_wscs_env->feature, WSC_FEAT_TIME_STAMP_SUPP));

    // User ID shall be included in flags field if the Server supports Multiple Users feature
    SETB(meas_flags, WSC_MEAS_FLAGS_USER_ID_PRESENT, GETB(p_wscs_env->feature, WSC_FEAT_MULTIPLE_USERS_SUPP));

    // BMI & Height if present and enabled in the features.
    if (!GETB(p_wscs_env->feature, WSC_FEAT_BMI_SUPP))
    {
        SETB(meas_flags, WSC_MEAS_FLAGS_BMI_PRESENT, 0);
    }

    // Force the unused bits of the flag value to 0
    co_buf_tail(p_buf)[0] = meas_flags;
    co_buf_tail_reserve(p_buf, 1);

    // Mandatory weight
    co_write16p(co_buf_tail(p_buf), co_htobs(p_meas->weight));
    co_buf_tail_reserve(p_buf, 2);

    // Get Weight Resolution
    co_buf_tail(p_buf)[0] = wscs_get_wght_resol(p_wscs_env->feature);
    co_buf_tail_reserve(p_buf, 1);

    // Measurement Units
    // 0 for SI and 1 for Imperial
    co_buf_tail(p_buf)[0] = GETB(meas_flags, WSC_MEAS_FLAGS_UNITS_IMPERIAL);
    co_buf_tail_reserve(p_buf, 1);

    // If Time-Stamp is supported
    if (GETB(p_wscs_env->feature, WSC_FEAT_TIME_STAMP_SUPP))
    {
        prf_pack_date_time(p_buf, &p_meas->time_stamp);
    }

    // If the Multiple users fields is enabled
    if (GETB(p_wscs_env->feature, WSC_FEAT_MULTIPLE_USERS_SUPP))
    {
        co_buf_tail(p_buf)[0] = (GETB(p_meas->flags, WSC_MEAS_FLAGS_USER_ID_PRESENT) ? p_meas->user_id
                                                                                     : WSC_MEAS_USER_ID_UNKNOWN_USER);
        co_buf_tail_reserve(p_buf, 1);
    }

    // If BMI is present
    if (GETB(p_wscs_env->feature, WSC_FEAT_BMI_SUPP))
    {
        co_write16p(co_buf_tail(p_buf), co_htobs(p_meas->bmi));
        co_buf_tail_reserve(p_buf, 2);

        co_write16p(co_buf_tail(p_buf), co_htobs(p_meas->height));
        co_buf_tail_reserve(p_buf, 2);

        co_buf_tail(p_buf)[0] = wscs_get_hght_resol(p_wscs_env->feature);
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
__STATIC void wscs_cb_att_read_get(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset,
                                   uint16_t max_length)
{
    wscs_env_t *p_wscs_env = PRF_ENV_GET(WSCS, wscs);
    // retrieve value attribute
    co_buf_t* p_buf       = NULL;
    uint16_t  status      = GAP_ERR_NO_ERROR;
    uint16_t  att_val_len = 0;

    if(p_wscs_env == NULL)
    {
        status = PRF_APP_ERROR;
    }
    else if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, 0, WSC_FEAT_VAL_SIZE + GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
    {
        uint8_t att_idx = wscs_idx_get(p_wscs_env, hdl);

        switch (att_idx)
        {
            case WSCS_IDX_MEAS_CCC:
            {
                uint16_t ind_cfg = CO_BIT_GET(&(p_wscs_env->ind_cfg_bf), conidx)
                                 ? PRF_CLI_START_IND : PRF_CLI_STOP_NTFIND;

                co_write16p(co_buf_tail(p_buf), co_htobs(ind_cfg));
                co_buf_tail_reserve(p_buf, 2);
            } break;

            case WSCS_IDX_FEAT_VAL:
            {
                co_write32p(co_buf_tail(p_buf), co_htobl(p_wscs_env->feature));
                co_buf_tail_reserve(p_buf, 4);
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
__STATIC void wscs_cb_att_val_set(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset,
                                   co_buf_t* p_data)
{
    wscs_env_t *p_wscs_env = PRF_ENV_GET(WSCS, wscs);
    uint16_t  status      = PRF_APP_ERROR;

    if(p_wscs_env != NULL)
    {
        switch (wscs_idx_get(p_wscs_env, hdl))
        {
            case WSCS_IDX_MEAS_CCC:
            {
                uint16_t cfg = co_btohs(co_read16p(co_buf_data(p_data)));

                // parameter check
                if(   (co_buf_data_len(p_data) == sizeof(uint16_t))
                   && ((cfg == PRF_CLI_STOP_NTFIND) || (cfg == PRF_CLI_START_IND)))
                {
                    const wscs_cb_t* p_cb  = (const wscs_cb_t*) p_wscs_env->prf_env.p_cb;

                    CO_BIT_SET(&(p_wscs_env->ind_cfg_bf), conidx, (cfg == PRF_CLI_START_IND));

                    // inform application about update
                    p_cb->cb_bond_data_upd(conidx, cfg);
                    status = GAP_ERR_NO_ERROR;
                }
                else
                {
                    status = PRF_CCCD_IMPR_CONFIGURED;
                }
            } break;

            default: { status = ATT_ERR_REQUEST_NOT_SUPPORTED; } break;
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
__STATIC void wscs_cb_event_sent(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    // Consider job done
    wscs_env_t *p_wscs_env = PRF_ENV_GET(WSCS, wscs);
    if(p_wscs_env != NULL)
    {
        const wscs_cb_t* p_cb  = (const wscs_cb_t*) p_wscs_env->prf_env.p_cb;
        p_wscs_env->op_ongoing = false;

        switch(dummy)
        {
            case WSCS_MEAS_INDICATE_CMD_OP_CODE:
            {
                p_cb->cb_meas_send_cmp(conidx, status);
            } break;
            default: { /* Nothing to do */ } break;
        }

        // continue operation execution
        wscs_exe_operation(p_wscs_env);
    }
}

/// Service callback hander
__STATIC const gatt_srv_cb_t wscs_cb =
{
        .cb_event_sent    = wscs_cb_event_sent,
        .cb_att_read_get  = wscs_cb_att_read_get,
        .cb_att_event_get = NULL,
        .cb_att_info_get  = NULL,
        .cb_att_val_set   = wscs_cb_att_val_set,
};

/*
 * PROFILE NATIVE HANDLERS
 ****************************************************************************************
 */

uint16_t wscs_enable(uint8_t conidx, uint16_t ind_cfg)
{
    wscs_env_t* p_wscs_env = PRF_ENV_GET(WSCS, wscs);
    uint16_t status = PRF_ERR_REQ_DISALLOWED;

    if(p_wscs_env != NULL)
    {
        // check state of the task
        if (gapc_get_conhdl(conidx) != GAP_INVALID_CONHDL)
        {
            CO_BIT_SET(&(p_wscs_env->ind_cfg_bf), conidx, (ind_cfg != 0));
            status = GAP_ERR_NO_ERROR;
        }
    }

    return (status);

}

uint16_t wscs_meas_send(uint8_t conidx, const wsc_meas_t* p_meas)
{
    wscs_env_t* p_wscs_env = PRF_ENV_GET(WSCS, wscs);
    uint16_t status = PRF_ERR_REQ_DISALLOWED;

    if(p_meas == NULL)
    {
        status = GAP_ERR_INVALID_PARAM;
    }
    else if(p_wscs_env != NULL)
    {
        co_buf_t* p_buf;

        // check if indication is enabled
        if(!CO_BIT_GET(&(p_wscs_env->ind_cfg_bf), conidx))
        {
            status = PRF_ERR_IND_DISABLED;
        }
        // If Body Composition Service is included then Mandatory to have
        // both the Height and BMI fields present (in flags) otherwise
        // reject command.
        else if ((p_wscs_env->bcs_included) && (!GETB(p_meas->flags, WSC_MEAS_FLAGS_BMI_PRESENT)))
        {
            status = PRF_ERR_INVALID_PARAM;
        }
        else if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, 0, WSC_MEAS_SIZE + GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
        {
            wscs_buf_meta_t* p_buf_meta = (wscs_buf_meta_t*) co_buf_metadata(p_buf);
            p_buf_meta->operation = WSCS_MEAS_INDICATE_CMD_OP_CODE;
            p_buf_meta->conidx    = conidx;

            // pack measurement
            wscs_pack_meas(p_wscs_env, p_buf, p_meas);
            // put event on wait queue
            co_list_push_back(&(p_wscs_env->wait_queue), &(p_buf->hdr));
            // execute operation
            wscs_exe_operation(p_wscs_env);
            status = GAP_ERR_NO_ERROR;
        }
        else
        {
            status = GAP_ERR_INSUFF_RESOURCES;
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
 * @brief Send a WSCS_CMP_EVT message to the application.
 * @param[in] conidx connection index
 * @param[in] operation Operation completed
 * @param[in] status status of the operation
 ****************************************************************************************
 */
__STATIC void wscs_send_cmp_evt(uint8_t conidx, uint8_t operation, uint16_t status)
{
    struct wscs_cmp_evt *p_evt;

    // Send the message
    p_evt = KE_MSG_ALLOC(WSCS_CMP_EVT, PRF_DST_TASK(WSCS), PRF_SRC_TASK(WSCS), wscs_cmp_evt);

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
 * @brief Handles reception of the @ref WSCS_ENABLE_REQ message.
 *
 * @param[in] msgid Id of the message received
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance
 * @param[in] src_id ID of the sending task instance.
 *
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int wscs_enable_req_handler(ke_msg_id_t const msgid, struct wscs_enable_req *p_param,
                                     ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    struct wscs_enable_rsp *p_cmp_evt;
    uint16_t status = wscs_enable(p_param->conidx, p_param->ind_cfg);

    // send completed information to APP task that contains error status
    p_cmp_evt = KE_MSG_ALLOC(WSCS_ENABLE_RSP, src_id, dest_id, wscs_enable_rsp);

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
 * @brief Handles reception of the @ref WSCS_MEAS_INDICATE_CMD message.
 * Send MEASUREMENT INDICATION to the connected peer case of CCC enabled
 *
 * @param[in] msgid Id of the message received.
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id ID of the sending task instance.
 *
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int wscs_meas_indicate_cmd_handler(ke_msg_id_t const msgid, struct wscs_meas_indicate_cmd *p_param,
                                            ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    wsc_meas_t meas = {
            .flags = p_param->flags,
            .weight = p_param->weight,
            .time_stamp = p_param->time_stamp,
            .user_id = p_param->user_id,
            .bmi = p_param->bmi,
            .height = p_param->height,
    };

    uint16_t status = wscs_meas_send(p_param->conidx, &(meas));

    if(status != GAP_ERR_NO_ERROR)
    {
        wscs_send_cmp_evt(p_param->conidx, WSCS_MEAS_INDICATE_CMD_OP_CODE, status);
    }

    return (KE_MSG_CONSUMED);
}


/// Default State handlers definition
KE_MSG_HANDLER_TAB(wscs)
{
    // Note: all messages must be sorted in ID ascending order

    {WSCS_ENABLE_REQ,          (ke_msg_func_t) wscs_enable_req_handler          },
    {WSCS_MEAS_INDICATE_CMD,   (ke_msg_func_t) wscs_meas_indicate_cmd_handler   },
};

/**
 ****************************************************************************************
 * @brief Completion of measurement transmission
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 ****************************************************************************************
 */
__STATIC void wscs_cb_bond_data_upd(uint8_t conidx, uint16_t cfg_val)
{
    struct wscs_meas_ccc_ind *p_evt;

    // Send the message
    p_evt = KE_MSG_ALLOC(WSCS_MEAS_CCC_IND, PRF_DST_TASK(WSCS), PRF_SRC_TASK(WSCS), wscs_meas_ccc_ind);

    if(p_evt != NULL)
    {
        p_evt->conidx     = conidx;
        p_evt->ind_cfg    = cfg_val;
        ke_msg_send(p_evt);
    }
}

/**
 ****************************************************************************************
 * @brief Completion of measurement transmission
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 ****************************************************************************************
 */
__STATIC void wscs_cb_meas_send_cmp(uint8_t conidx, uint16_t status)
{
    wscs_send_cmp_evt(conidx, WSCS_MEAS_INDICATE_CMD_OP_CODE, status);
}

/// Default Message handle
__STATIC const wscs_cb_t wscs_msg_cb =
{
        .cb_bond_data_upd        = wscs_cb_bond_data_upd,
        .cb_meas_send_cmp        = wscs_cb_meas_send_cmp,
};
#endif // (BLE_HL_MSG_API)

/*
 * PROFILE DEFAULT HANDLERS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Initialization of the WSCS module.
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
__STATIC uint16_t wscs_init(prf_data_t *p_env, uint16_t *p_start_hdl, uint8_t sec_lvl, uint8_t user_prio,
                          struct wscs_db_cfg *p_params, const wscs_cb_t* p_cb)
{
    //------------------ create the attribute database for the profile -------------------

    // DB Creation Status
    uint16_t status = GAP_ERR_NO_ERROR;
    uint8_t user_lid = GATT_INVALID_USER_LID;

    do
    {
        wscs_env_t* p_wscs_env;
        // Service content flag
        uint32_t cfg_flag = (1 << WSCS_IDX_NB) - 1;
        // Add service in the database
        gatt_att16_desc_t wscs_db[WSCS_IDX_NB];

        #if (BLE_HL_MSG_API)
        if(p_cb == NULL)
        {
            p_cb = &(wscs_msg_cb);
        }
        #endif // (BLE_HL_MSG_API)

        if(   (p_params == NULL) || (p_start_hdl == NULL) || (p_cb == NULL)
           || (p_cb->cb_meas_send_cmp == NULL) || (p_cb->cb_bond_data_upd == NULL))
        {
            status = GAP_ERR_INVALID_PARAM;
            break;
        }

        // register WSCS user
        status = gatt_user_srv_register(L2CAP_LE_MTU_MIN, user_prio, &wscs_cb, &user_lid);
        if(status != GAP_ERR_NO_ERROR) break;

        // Prepare database
        memcpy(wscs_db, wscs_att_db, sizeof(wscs_db));
        if (p_params->bcs_start_hdl != GATT_INVALID_HDL)
        {
            wscs_db[WSCS_IDX_INC_SVC].ext_info = p_params->bcs_start_hdl;
        }
        else
        {
            cfg_flag &= ~CO_BIT(WSCS_IDX_INC_SVC);
        }



        // Add GAP service
        status = gatt_db_svc16_add(user_lid, sec_lvl, GATT_SVC_WEIGHT_SCALE, WSCS_IDX_NB,
                                   (uint8_t *)&cfg_flag, &(wscs_db[0]), WSCS_IDX_NB, p_start_hdl);
        if(status != GAP_ERR_NO_ERROR) break;

        //-------------------- allocate memory required for the profile  ---------------------
        p_wscs_env = (wscs_env_t *) ke_malloc(sizeof(wscs_env_t), KE_MEM_ATT_DB);

        if(p_wscs_env != NULL)
        {
            // allocate WSCS required environment variable
            p_env->p_env = (prf_hdr_t *) p_wscs_env;
            p_wscs_env->start_hdl       = *p_start_hdl;
            p_wscs_env->feature         = p_params->feature;
            p_wscs_env->user_lid        = user_lid;
            p_wscs_env->op_ongoing      = false;
            p_wscs_env->in_exe_op       = false;
            p_wscs_env->ind_cfg_bf      = 0;
            co_list_init(&(p_wscs_env->wait_queue));
            p_wscs_env->bcs_included    = (p_params->bcs_start_hdl != GATT_INVALID_HDL);

            // initialize profile environment variable
            p_wscs_env->prf_env.p_cb     = p_cb;
            #if (BLE_HL_MSG_API)
            p_env->desc.msg_handler_tab  = wscs_msg_handler_tab;
            p_env->desc.msg_cnt          = ARRAY_LEN(wscs_msg_handler_tab);
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
__STATIC uint16_t wscs_destroy(prf_data_t *p_env, uint8_t reason)
{
    uint16_t status = GAP_ERR_NO_ERROR;
    wscs_env_t *p_wscs_env = (wscs_env_t *) p_env->p_env;

    if(reason != PRF_DESTROY_RESET)
    {
        status = gatt_user_unregister(p_wscs_env->user_lid);
    }

    if(status == GAP_ERR_NO_ERROR)
    {
        if(reason != PRF_DESTROY_RESET)
        {
            // remove buffer in wait queue
            while(!co_list_is_empty(&p_wscs_env->wait_queue))
            {
                co_buf_t* p_buf = (co_buf_t*) co_list_pop_front(&p_wscs_env->wait_queue);
                co_buf_release(p_buf);
            }
        }

        // free profile environment variables
        p_env->p_env = NULL;
        ke_free(p_wscs_env);
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
__STATIC void wscs_con_create(prf_data_t *p_env, uint8_t conidx, const gap_con_param_t* p_con_param)
{
    wscs_env_t *p_wscs_env = (wscs_env_t *) p_env->p_env;
    CO_BIT_SET(&(p_wscs_env->ind_cfg_bf), conidx, 0);
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
__STATIC void wscs_con_cleanup(prf_data_t *p_env, uint8_t conidx, uint16_t reason)
{
    wscs_env_t *p_wscs_env = (wscs_env_t *) p_env->p_env;
    CO_BIT_SET(&(p_wscs_env->ind_cfg_bf), conidx, 0);
}

/// WSCS Task interface required by profile manager
const prf_task_cbs_t wscs_itf =
{
    .cb_init          = (prf_init_cb) wscs_init,
    .cb_destroy       = wscs_destroy,
    .cb_con_create    = wscs_con_create,
    .cb_con_cleanup   = wscs_con_cleanup,
    .cb_con_upd       = NULL,
};

/**
 ****************************************************************************************
 * @brief Retrieve service profile interface
 *
 * @return service profile interface
 ****************************************************************************************
 */
const prf_task_cbs_t *wscs_prf_itf_get(void)
{
    return &wscs_itf;
}

#endif //(BLE_WSC_SERVER)

/// @} WSCS

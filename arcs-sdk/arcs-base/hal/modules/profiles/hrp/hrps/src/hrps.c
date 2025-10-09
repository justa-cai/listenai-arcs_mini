/**
 ****************************************************************************************
 *
 * @file hrps.c
 *
 * @brief Heart Rate Profile Sensor implementation.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup HRPS
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "btip_config.h"

#if (BLE_HR_SENSOR)

#include "hrp_common.h"

#include "hrps.h"
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
 ******* HRPS Configuration Flag Masks ******
 ********************************************/
#define HRPS_MANDATORY_MASK             (0x0F)
#define HRPS_BODY_SENSOR_LOC_MASK       (0x30)
#define HRPS_HR_CTNL_PT_MASK            (0xC0)

#define HRP_PRF_CFG_PERFORMED_OK        (0x80)

#define HRPS_HT_MEAS_MAX_LEN            (5 + (2 * HRS_MAX_RR_INTERVAL))

/// Service - Attribute List
enum hrps_hrs_att_list
{
    HRS_IDX_SVC,

    HRS_IDX_HR_MEAS_CHAR,
    HRS_IDX_HR_MEAS_VAL,
    HRS_IDX_HR_MEAS_NTF_CFG,

    HRS_IDX_BODY_SENSOR_LOC_CHAR,
    HRS_IDX_BODY_SENSOR_LOC_VAL,

    HRS_IDX_HR_CTNL_PT_CHAR,
    HRS_IDX_HR_CTNL_PT_VAL,

    HRS_IDX_NB,
};

enum
{
    HRPS_HR_MEAS_CHAR,
    HRPS_BODY_SENSOR_LOC_CHAR,
    HRPS_HR_CTNL_PT_CHAR,

    HRPS_CHAR_MAX,
};

/// Feature bit field
enum hrps_feat_bf
{
    /// Body Sensor Location Feature Supported
    HRPS_BODY_SENSOR_LOC_CHAR_SUP_POS = 0,
    HRPS_BODY_SENSOR_LOC_CHAR_SUP_BIT = CO_BIT(HRPS_BODY_SENSOR_LOC_CHAR_SUP_POS),

    /// Energy Expanded Feature Supported
    HRPS_ENGY_EXP_FEAT_SUP_POS = 1,
    HRPS_ENGY_EXP_FEAT_SUP_BIT = CO_BIT(HRPS_ENGY_EXP_FEAT_SUP_POS),

    /// Heart Rate Measurement Notification Supported
    HRPS_HR_MEAS_NTF_CFG_POS = 2,
    HRPS_HR_MEAS_NTF_CFG_BIT = CO_BIT(HRPS_HR_MEAS_NTF_CFG_POS),
};

/*
 * MACROS
 ****************************************************************************************
 */

#define HRPS_IS_SUPPORTED(features, mask) ((features & mask) == mask)

/*
 * TYPES DEFINITION
 ****************************************************************************************
 */


/// ongoing operation information
typedef struct hrps_buf_meta
{
    /// meaningful for some operation
    uint32_t  conidx_bf;
    /// Operation
    uint8_t   operation;
    /// Connection index targeted
    uint8_t   conidx;
} hrps_buf_meta_t;

/// Heart rate sensor server environment variable
typedef struct hrps_env
{
    /// profile environment
    prf_hdr_t prf_env;
    /// Operation Event TX wait queue
    co_list_t wait_queue;
    /// Service Attribute Start Handle
    uint16_t  start_hdl;
    /// Services features
    uint16_t  features;
    /// Sensor location
    uint8_t   sensor_loc;
    /// GATT user local identifier
    uint8_t   user_lid;
    /// Operation On-going
    bool      op_ongoing;
    /// Prevent recursion in execute_operation function
    bool      in_exe_op;
    /// Measurement notification config
    uint16_t  hr_meas_ntf[BLE_CONNECTION_MAX];

} hrps_env_t;


/*
 * ATTRIBUTES DATABASE
 ****************************************************************************************
 */

/// Full Database Description - Used to add attributes into the database
const gatt_att16_desc_t hrps_att_db[HRS_IDX_NB] =
{
    // Heart Rate Service Declaration
    [HRS_IDX_SVC]                  =   { GATT_DECL_PRIMARY_SERVICE,       PROP(RD),            0                                },

    // Heart Rate Measurement Characteristic Declaration
    [HRS_IDX_HR_MEAS_CHAR]         =   { GATT_DECL_CHARACTERISTIC,        PROP(RD),            0                                },
    // Heart Rate Measurement Characteristic Value
    [HRS_IDX_HR_MEAS_VAL]          =   { GATT_CHAR_HEART_RATE_MEAS,       PROP(N),             OPT(NO_OFFSET)                   },
    // Heart Rate Measurement Characteristic - Client Characteristic Configuration Descriptor
    [HRS_IDX_HR_MEAS_NTF_CFG]      =   { GATT_DESC_CLIENT_CHAR_CFG,       PROP(RD) | PROP(WR), OPT(NO_OFFSET)                   },

    // Body Sensor Location Characteristic Declaration
    [HRS_IDX_BODY_SENSOR_LOC_CHAR] =   { GATT_DECL_CHARACTERISTIC,        PROP(RD),            0                                },
    // Body Sensor Location Characteristic Value
    [HRS_IDX_BODY_SENSOR_LOC_VAL]  =   { GATT_CHAR_BODY_SENSOR_LOCATION,  PROP(RD),            OPT(NO_OFFSET)                   },

    // Heart Rate Control Point Characteristic Declaration
    [HRS_IDX_HR_CTNL_PT_CHAR]      =   { GATT_DECL_CHARACTERISTIC,        PROP(RD),            0                                },
    // Heart Rate Control Point Characteristic Value
    [HRS_IDX_HR_CTNL_PT_VAL]       =   { GATT_CHAR_HEART_RATE_CNTL_POINT, PROP(WR),            OPT(NO_OFFSET) | sizeof(uint8_t) },
};

/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */

/// Get database attribute index
__STATIC uint8_t hrps_idx_get(hrps_env_t* p_hrps_env, uint16_t hdl)
{
    uint8_t att_idx = hdl - p_hrps_env->start_hdl;

    if(   (att_idx >= HRS_IDX_BODY_SENSOR_LOC_CHAR)
       && !GETB(p_hrps_env->features, HRPS_BODY_SENSOR_LOC_CHAR_SUP))
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
__STATIC void hrps_exe_operation(hrps_env_t* p_hrps_env)
{
    if(!p_hrps_env->in_exe_op)
    {
        p_hrps_env->in_exe_op = true;

        while(!co_list_is_empty(&(p_hrps_env->wait_queue)) && !(p_hrps_env->op_ongoing))
        {
            uint16_t status = GAP_ERR_NO_ERROR;
            co_buf_t* p_buf = (co_buf_t*) co_list_pop_front(&(p_hrps_env->wait_queue));
            hrps_buf_meta_t* p_meta = (hrps_buf_meta_t*) co_buf_metadata(p_buf);
            uint8_t  conidx;

            switch(p_meta->operation)
            {
                case HRPS_MEAS_SEND_CMD_OP_CODE:
                {
                    uint32_t conidx_bf = 0;

                    // check connection that support notification reception
                    for(conidx = 0 ; conidx < BLE_CONNECTION_MAX ; conidx++)
                    {
                        if ((p_hrps_env->hr_meas_ntf[conidx] & ~HRP_PRF_CFG_PERFORMED_OK) == PRF_CLI_START_NTF)
                        {
                            conidx_bf |= CO_BIT(conidx);
                        }
                    }

                    // send notification only on selected connections
                    conidx_bf &= p_meta->conidx_bf;

                    if(conidx_bf != 0)
                    {
                        // send multi-point notification
                        status = gatt_srv_event_mtp_send(conidx_bf, p_hrps_env->user_lid, HRPS_MEAS_SEND_CMD_OP_CODE,
                                                         GATT_NOTIFY, p_hrps_env->start_hdl + HRS_IDX_HR_MEAS_VAL, p_buf, true);
                        if(status == GAP_ERR_NO_ERROR)
                        {
                            p_hrps_env->op_ongoing = true;
                        }
                    }

                    co_buf_release(p_buf);

                    if(!p_hrps_env->op_ongoing)
                    {
                        const hrps_cb_t* p_cb = (const hrps_cb_t*) p_hrps_env->prf_env.p_cb;
                        // Inform application that event has been sent
                        p_cb->cb_meas_send_cmp(status);
                    }
                } break;
                default: { ASSERT_ERR(0); } break;
            }
        }

        p_hrps_env->in_exe_op = false;
    }
}


/**
 ****************************************************************************************
 * @brief Packs measurement data
 *
 * @param[in]     p_hrps_env   Environment data
 * @param[in]     p_buf         Pointer to output buffer
 * @param[in]     p_meas        pointer to measurement information
 ****************************************************************************************
 */
__STATIC void hrps_pack_meas(hrps_env_t *p_hrps_env, co_buf_t* p_buf, const hrs_hr_meas_t *p_meas)
{
    uint8_t meas_flags = p_meas->flags;
    co_buf_tail(p_buf)[0] =  meas_flags;
    co_buf_tail_reserve(p_buf, 1);

    // Heart Rate Measurement Value 16 bits
    if (GETB(meas_flags, HRS_FLAG_HR_VALUE_FORMAT))
    {
        co_write16p(co_buf_tail(p_buf), co_htobs(p_meas->heart_rate));
        co_buf_tail_reserve(p_buf, 2);
    }
    // Heart Rate Measurement Value 8 bits
    else
    {
        co_buf_tail(p_buf)[0] = p_meas->heart_rate;
        co_buf_tail_reserve(p_buf, 1);
    }

    // Energy Expended present
    if (GETB(p_meas->flags, HRS_FLAG_ENERGY_EXPENDED_PRESENT))
    {
        co_write16p(co_buf_tail(p_buf), co_htobs(p_meas->energy_expended));
        co_buf_tail_reserve(p_buf, 2);
    }

    // RR-Intervals
    if (GETB(p_meas->flags, HRS_FLAG_RR_INTERVAL_PRESENT))
    {
        uint8_t cursor;
        for (cursor = 0; (cursor < (p_meas->nb_rr_interval)) && (cursor < (HRS_MAX_RR_INTERVAL)); cursor++)
        {
            co_write16p(co_buf_tail(p_buf), co_htobs(p_meas->rr_intervals[cursor]));
            co_buf_tail_reserve(p_buf, 2);
        }
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
__STATIC void hrps_cb_att_read_get(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset,
                                   uint16_t max_length)
{
    hrps_env_t *p_hrps_env = PRF_ENV_GET(HRPS, hrps);
    // retrieve value attribute
    co_buf_t* p_buf       = NULL;
    uint16_t  status      = GAP_ERR_NO_ERROR;
    uint16_t  att_val_len = 0;

    if(p_hrps_env == NULL)
    {
        status = PRF_APP_ERROR;
    }
    else if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, 0,  sizeof(uint16_t) + GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
    {
        uint8_t att_idx = hdl - p_hrps_env->start_hdl;

        switch (att_idx)
        {
            case HRS_IDX_HR_MEAS_NTF_CFG:
            {
                uint16_t ntf_cfg = p_hrps_env->hr_meas_ntf[conidx] & ~HRP_PRF_CFG_PERFORMED_OK;

                co_write16p(co_buf_tail(p_buf), co_htobs(ntf_cfg));
                co_buf_tail_reserve(p_buf, 2);
            } break;

            case HRS_IDX_BODY_SENSOR_LOC_VAL:
            {
                co_buf_tail(p_buf)[0] = p_hrps_env->sensor_loc;
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
__STATIC void hrps_cb_att_val_set(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset,
                        co_buf_t* p_data)
{
    hrps_env_t *p_hrps_env = PRF_ENV_GET(HRPS, hrps);
    uint16_t  status      = PRF_APP_ERROR;

    if(p_hrps_env != NULL)
    {
        const hrps_cb_t* p_cb  = (const hrps_cb_t*) p_hrps_env->prf_env.p_cb;

        switch (hrps_idx_get(p_hrps_env, hdl))
        {
            case HRS_IDX_HR_MEAS_NTF_CFG:
            {
                uint16_t cfg = co_btohs(co_read16p(co_buf_data(p_data)));

                // parameter check
                if(   (co_buf_data_len(p_data) == sizeof(uint16_t))
                   && ((cfg == PRF_CLI_STOP_NTFIND) || (cfg == PRF_CLI_START_NTF)))
                {
                    p_hrps_env->hr_meas_ntf[conidx] = (cfg | HRP_PRF_CFG_PERFORMED_OK);

                    // inform application about update
                    p_cb->cb_bond_data_upd(conidx, cfg);
                    status = GAP_ERR_NO_ERROR;
                }
                else
                {
                    status = PRF_APP_ERROR;
                }
            } break;

            case HRS_IDX_HR_CTNL_PT_VAL:
            {
                if (GETB(p_hrps_env->features, HRPS_ENGY_EXP_FEAT_SUP))
                {
                    uint8_t value = co_buf_data(p_data)[0];
                    if((value == 0x01) && (co_buf_data_len(p_data) >= 0))
                    {
                        // inform application that energy reset is requested
                        p_cb->cb_energy_exp_reset(conidx);
                        status = GAP_ERR_NO_ERROR;
                    }
                    else
                    {
                        status = HRS_ERR_HR_CNTL_POINT_NOT_SUPPORTED;
                    }
                }
                else
                {
                    // Allowed to send Application Error if value inconvenient
                    status = HRS_ERR_HR_CNTL_POINT_NOT_SUPPORTED;
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
__STATIC void hrps_cb_event_sent(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    // Consider job done
    hrps_env_t *p_hrps_env = PRF_ENV_GET(HRPS, hrps);
    if(p_hrps_env != NULL)
    {
        const hrps_cb_t* p_cb  = (const hrps_cb_t*) p_hrps_env->prf_env.p_cb;
        p_hrps_env->op_ongoing = false;
        // inform application that measurement has been sent
        p_cb->cb_meas_send_cmp(status);
        // continue operation execution
        hrps_exe_operation(p_hrps_env);
    }
}

/// Service callback hander
__STATIC const gatt_srv_cb_t hrps_cb =
{
        .cb_event_sent    = hrps_cb_event_sent,
        .cb_att_read_get  = hrps_cb_att_read_get,
        .cb_att_event_get = NULL,
        .cb_att_info_get  = NULL,
        .cb_att_val_set   = hrps_cb_att_val_set,
};

/*
 * PROFILE NATIVE HANDLERS
 ****************************************************************************************
 */

uint16_t hrps_enable(uint8_t conidx, uint16_t hr_meas_ntf)
{
    hrps_env_t* p_hrps_env = PRF_ENV_GET(HRPS, hrps);
    uint16_t status = PRF_ERR_REQ_DISALLOWED;

    if(p_hrps_env != NULL)
    {
        // check state of the task
        if (gapc_get_conhdl(conidx) != GAP_INVALID_CONHDL)
        {
            // Bonded data was not used before
            if (!HRPS_IS_SUPPORTED(p_hrps_env->hr_meas_ntf[conidx], HRP_PRF_CFG_PERFORMED_OK))
            {
                // Save notification config
                p_hrps_env->hr_meas_ntf[conidx] = hr_meas_ntf;
                // Enable Bonded Data
                p_hrps_env->hr_meas_ntf[conidx] |= HRP_PRF_CFG_PERFORMED_OK;
                status = GAP_ERR_NO_ERROR;
            }

            status = GAP_ERR_NO_ERROR;
        }
    }

    return (status);
}

uint16_t hrps_meas_send(uint32_t conidx_bf, const hrs_hr_meas_t* p_meas)
{
    hrps_env_t* p_hrps_env = PRF_ENV_GET(HRPS, hrps);
    uint16_t status = PRF_ERR_REQ_DISALLOWED;

    if((p_meas == NULL) || (p_meas->nb_rr_interval > HRS_MAX_RR_INTERVAL))
    {
        status = GAP_ERR_INVALID_PARAM;
    }
    else if(p_hrps_env != NULL)
    {
        co_buf_t* p_buf;

        if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, 0, HRPS_HT_MEAS_MAX_LEN + GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
        {
            hrps_buf_meta_t* p_buf_meta = (hrps_buf_meta_t*) co_buf_metadata(p_buf);
            p_buf_meta->operation = HRPS_MEAS_SEND_CMD_OP_CODE;
            p_buf_meta->conidx    = GAP_INVALID_CONIDX;
            p_buf_meta->conidx_bf = conidx_bf & ((1 << BLE_CONNECTION_MAX) - 1);

            // pack measurement
            hrps_pack_meas(p_hrps_env, p_buf, p_meas);
            // put event on wait queue
            co_list_push_back(&(p_hrps_env->wait_queue), &(p_buf->hdr));
            // execute operation
            hrps_exe_operation(p_hrps_env);
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
 * @brief Send a HRPS_CMP_EVT message to the application.
 * @param[in] conidx connection index
 * @param[in] operation Operation completed
 * @param[in] status status of the operation
 ****************************************************************************************
 */
__STATIC void hrps_send_cmp_evt(uint8_t operation, uint16_t status)
{
    struct hrps_cmp_evt *p_evt;

    // Send the message
    p_evt = KE_MSG_ALLOC(HRPS_CMP_EVT, PRF_DST_TASK(HRPS), PRF_SRC_TASK(HRPS), hrps_cmp_evt);

    if(p_evt != NULL)
    {
        p_evt->operation  = operation;
        p_evt->status     = status;
        ke_msg_send(p_evt);
    }
}

/**
 ****************************************************************************************
 * @brief Handles reception of the @ref HRPS_ENABLE_REQ message.
 * @param[in] msgid Id of the message received
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int hrps_enable_req_handler(ke_msg_id_t const msgid, struct hrps_enable_req *p_param,
                                      ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    struct hrps_enable_rsp *p_cmp_evt;
    uint16_t status = hrps_enable(p_param->conidx, p_param->hr_meas_ntf);

    // send completed information to APP task that contains error status
    p_cmp_evt = KE_MSG_ALLOC(HRPS_ENABLE_RSP, src_id, dest_id, hrps_enable_rsp);

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
 * @brief Handles reception of the @ref HRPS_MEAS_SEND_CMD message.
 * @param[in] msgid Id of the message received.
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int hrps_meas_send_cmd_handler(ke_msg_id_t const msgid, struct hrps_meas_send_cmd *p_param,
                                        ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = hrps_meas_send(p_param->conidx_bf, &(p_param->meas_val));

    if(status != GAP_ERR_NO_ERROR)
    {
        hrps_send_cmp_evt(HRPS_MEAS_SEND_CMD_OP_CODE, status);
    }

    return (KE_MSG_CONSUMED);
}

/// Default State handlers definition
KE_MSG_HANDLER_TAB(hrps)
{
    // Note: all messages must be sorted in ID ascending order

    { HRPS_ENABLE_REQ,              (ke_msg_func_t) hrps_enable_req_handler    },
    { HRPS_MEAS_SEND_CMD,           (ke_msg_func_t) hrps_meas_send_cmd_handler },
};

/**
 ****************************************************************************************
 * @brief Completion of measurement transmission
 *
 * @param[in] status Status of the procedure execution (@see enum hl_err)
 ****************************************************************************************
 */
__STATIC void hrps_cb_meas_send_cmp(uint16_t status)
{
    hrps_send_cmp_evt(HRPS_MEAS_SEND_CMD_OP_CODE, status);
}

/**
 ****************************************************************************************
 * @brief Inform that bond data updated for the connection.
 *
 * @param[in] conidx        Connection index
 * @param[in] cfg_val       Stop/notify/indicate value to configure into the peer characteristic
 ****************************************************************************************
 */
__STATIC void hrps_cb_bond_data_upd(uint8_t conidx, uint16_t cfg_val)
{
    struct hrps_cfg_indntf_ind *p_evt;

    // Send the message
    p_evt = KE_MSG_ALLOC(HRPS_CFG_INDNTF_IND, PRF_DST_TASK(HRPS), PRF_SRC_TASK(HRPS), hrps_cfg_indntf_ind);

    if(p_evt != NULL)
    {
        p_evt->conidx     = conidx;
        p_evt->cfg_val    = cfg_val;
        ke_msg_send(p_evt);
    }
}

/**
 ****************************************************************************************
 * @brief Inform APP that Energy Expanded must be reset value
 *
 * @param[in] conidx        Connection index
 ****************************************************************************************
 */
__STATIC void hrps_cb_energy_exp_reset(uint8_t conidx)
{
    struct hrps_energy_exp_reset_ind *p_evt;

    // Send the message
    p_evt = KE_MSG_ALLOC(HRPS_ENERGY_EXP_RESET_IND, PRF_DST_TASK(HRPS), PRF_SRC_TASK(HRPS), hrps_energy_exp_reset_ind);

    if(p_evt != NULL)
    {
        p_evt->conidx     = conidx;
        ke_msg_send(p_evt);
    }
}

/// Default Message handle
__STATIC const hrps_cb_t hrps_msg_cb =
{
        .cb_meas_send_cmp        = hrps_cb_meas_send_cmp,
        .cb_bond_data_upd        = hrps_cb_bond_data_upd,
        .cb_energy_exp_reset     = hrps_cb_energy_exp_reset,
};
#endif // (BLE_HL_MSG_API)

/*
 * PROFILE DEFAULT HANDLERS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Initialization of the HRPS module.
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
__STATIC uint16_t hrps_init(prf_data_t *p_env, uint16_t *p_start_hdl, uint8_t sec_lvl, uint8_t user_prio,
                          struct hrps_db_cfg *p_params, const hrps_cb_t* p_cb)
{
    //------------------ create the attribute database for the profile -------------------

    // DB Creation Status
    uint16_t status = GAP_ERR_NO_ERROR;
    uint8_t user_lid = GATT_INVALID_USER_LID;

    do
    {
        hrps_env_t* p_hrps_env;
        // Service content flag
        uint32_t cfg_flag = HRPS_MANDATORY_MASK;

        #if (BLE_HL_MSG_API)
        if(p_cb == NULL)
        {
            p_cb = &(hrps_msg_cb);
        }
        #endif // (BLE_HL_MSG_API)

        if(   (p_params == NULL) || (p_start_hdl == NULL) || (p_cb == NULL) || (p_cb->cb_meas_send_cmp == NULL)
           || (p_cb->cb_bond_data_upd == NULL) || (p_cb->cb_energy_exp_reset == NULL))
        {
            status = GAP_ERR_INVALID_PARAM;
            break;
        }

        // register HRPS user
        status = gatt_user_srv_register(L2CAP_LE_MTU_MIN, user_prio, &hrps_cb, &user_lid);
        if(status != GAP_ERR_NO_ERROR) break;

        // Set Configuration Flag Value
        if (GETB(p_params->features, HRPS_BODY_SENSOR_LOC_CHAR_SUP))
        {
            cfg_flag |= HRPS_BODY_SENSOR_LOC_MASK;
        }
        if (GETB(p_params->features, HRPS_ENGY_EXP_FEAT_SUP))
        {
            cfg_flag |= HRPS_HR_CTNL_PT_MASK;
        }

        // Add GAP service
        status = gatt_db_svc16_add(user_lid, sec_lvl, GATT_SVC_HEART_RATE, HRS_IDX_NB,
                                   (uint8_t *)&cfg_flag, &(hrps_att_db[0]), HRS_IDX_NB, p_start_hdl);
        if(status != GAP_ERR_NO_ERROR) break;

        //-------------------- allocate memory required for the profile  ---------------------
        p_hrps_env = (hrps_env_t *) ke_malloc(sizeof(hrps_env_t), KE_MEM_ATT_DB);

        if(p_hrps_env != NULL)
        {
            // allocate HRPS required environment variable
            p_env->p_env = (prf_hdr_t *) p_hrps_env;
            p_hrps_env->start_hdl       = *p_start_hdl;
            p_hrps_env->features        = p_params->features;
            p_hrps_env->sensor_loc      = p_params->body_sensor_loc;
            p_hrps_env->user_lid        = user_lid;
            p_hrps_env->op_ongoing      = false;
            p_hrps_env->in_exe_op       = false;
            memset(p_hrps_env->hr_meas_ntf, 0, sizeof(p_hrps_env->hr_meas_ntf));
            co_list_init(&(p_hrps_env->wait_queue));

            // initialize profile environment variable
            p_hrps_env->prf_env.p_cb     = p_cb;
            #if (BLE_HL_MSG_API)
            p_env->desc.msg_handler_tab  = hrps_msg_handler_tab;
            p_env->desc.msg_cnt          = ARRAY_LEN(hrps_msg_handler_tab);
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
__STATIC uint16_t hrps_destroy(prf_data_t *p_env, uint8_t reason)
{
    uint16_t status = GAP_ERR_NO_ERROR;
    hrps_env_t *p_hrps_env = (hrps_env_t *) p_env->p_env;

    if(reason != PRF_DESTROY_RESET)
    {
        status = gatt_user_unregister(p_hrps_env->user_lid);
    }

    if(status == GAP_ERR_NO_ERROR)
    {
        if(reason != PRF_DESTROY_RESET)
        {
            // remove buffer in wait queue
            while(!co_list_is_empty(&p_hrps_env->wait_queue))
            {
                co_buf_t* p_buf = (co_buf_t*) co_list_pop_front(&p_hrps_env->wait_queue);
                co_buf_release(p_buf);
            }
        }

        // free profile environment variables
        p_env->p_env = NULL;
        ke_free(p_hrps_env);
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
__STATIC void hrps_con_create(prf_data_t *p_env, uint8_t conidx, const gap_con_param_t* p_con_param)
{
    hrps_env_t *p_hrps_env = (hrps_env_t *) p_env->p_env;
    p_hrps_env->hr_meas_ntf[conidx] = 0;
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
__STATIC void hrps_con_cleanup(prf_data_t *p_env, uint8_t conidx, uint16_t reason)
{
    hrps_env_t *p_hrps_env = (hrps_env_t *) p_env->p_env;
    p_hrps_env->hr_meas_ntf[conidx] = 0;
}

/// HRPS Task interface required by profile manager
const prf_task_cbs_t hrps_itf =
{
    .cb_init          = (prf_init_cb) hrps_init,
    .cb_destroy       = hrps_destroy,
    .cb_con_create    = hrps_con_create,
    .cb_con_cleanup   = hrps_con_cleanup,
    .cb_con_upd       = NULL,
};

/**
 ****************************************************************************************
 * @brief Retrieve service profile interface
 *
 * @return service profile interface
 ****************************************************************************************
 */
const prf_task_cbs_t *hrps_prf_itf_get(void)
{
    return &hrps_itf;
}


#endif /* BLE_HR_SENSOR */

/// @} HRPS

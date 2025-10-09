/**
 ****************************************************************************************
 *
 * @file pasps.c
 *
 * @brief Phone Alert Status Profile Server implementation.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup PASPS
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "btip_config.h"
#if (BLE_PAS_SERVER)

#include "pasps.h"
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
/// Number of parallel instances of the task
/// Database Configuration Flag
#define PASPS_DB_CFG_FLAG                   (0x01FF)

/// Notification States Flags
#define PASPS_FLAG_ALERT_STATUS_CFG         (0x01)
#define PASPS_FLAG_RINGER_SETTING_CFG       (0x02)
///  Bonded data used
#define PASPS_FLAG_CFG_PERFORMED_OK         (0x80)

/// Attributes State Machine
enum pasps_pass_att_list
{
    PASS_IDX_SVC,

    PASS_IDX_ALERT_STATUS_CHAR,
    PASS_IDX_ALERT_STATUS_VAL,
    PASS_IDX_ALERT_STATUS_CFG,

    PASS_IDX_RINGER_SETTING_CHAR,
    PASS_IDX_RINGER_SETTING_VAL,
    PASS_IDX_RINGER_SETTING_CFG,

    PASS_IDX_RINGER_CTNL_PT_CHAR,
    PASS_IDX_RINGER_CTNL_PT_VAL,

    PASS_IDX_NB,
};

/*
 * MACROS
 ****************************************************************************************
 */

#define PASPS_IS_NTF_ENABLED(conidx, idx_env, flag) ((idx_env->p_env[conidx]->ntf_state & flag) == flag)

/*
 * TYPES DEFINITION
 ****************************************************************************************
 */


/// ongoing operation information
typedef struct pasps_buf_meta
{
    /// meaningful for some operation
    uint32_t  conidx_bf;
    /// Attribute handle to notify
    uint16_t  hdl;
    /// Operation
    uint8_t   operation;
    /// Connection index targeted
    uint8_t   conidx;
    /// Notification configuration flag
    uint8_t   ntf_flag;
} pasps_buf_meta_t;


/// Phone Alert Status Profile Server Connection Dependent Environment Variable
typedef struct pasps_cnx_env
{
    /// Ringer State
    uint8_t ringer_state;
    /**
     * Ringer State + Notification State
     *     Bit 0: Alert Status notification configuration
     *     Bit 1: Ringer setting notification configuration
     */
    uint8_t ntf_state;
} pasps_cnx_env_t;

/// Phone alert server environment variable
typedef struct pasps_env
{
    /// profile environment
    prf_hdr_t       prf_env;
    /// Operation Event TX wait queue
    co_list_t       wait_queue;
    /// Service Attribute Start Handle
    uint16_t        start_hdl;
    /// Alert Status Char. Value
    uint8_t         alert_status;
    /// Ringer Settings Char. Value
    uint8_t         ringer_setting;
    /// GATT user local identifier
    uint8_t         user_lid;
    /// Operation On-going
    bool            op_ongoing;
    /// Prevent recursion in execute_operation function
    bool            in_exe_op;
    /// Environment variable pointer for each connections
    pasps_cnx_env_t env[BLE_CONNECTION_MAX];

} pasps_env_t;


/*
 * ATTRIBUTES DATABASE
 ****************************************************************************************
 */

/// Full Database Description - Used to add attributes into the database
const gatt_att16_desc_t pasps_att_db[PASS_IDX_NB] =
{
    // Phone Alert Status Service Declaration
    [PASS_IDX_SVC]                 = { GATT_DECL_PRIMARY_SERVICE,   PROP(RD),            0                                },

    // Alert Status Characteristic Declaration
    [PASS_IDX_ALERT_STATUS_CHAR]   = { GATT_DECL_CHARACTERISTIC,    PROP(RD),            0                                },
    // Alert Status Characteristic Value
    [PASS_IDX_ALERT_STATUS_VAL]    = { GATT_CHAR_ALERT_STATUS,      PROP(RD) | PROP(N),  OPT(NO_OFFSET)                   },
    // Alert Status Characteristic - Client Characteristic Configuration Descriptor
    [PASS_IDX_ALERT_STATUS_CFG]    = { GATT_DESC_CLIENT_CHAR_CFG,   PROP(RD) | PROP(WR), OPT(NO_OFFSET)                   },

    // Ringer Setting Characteristic Declaration
    [PASS_IDX_RINGER_SETTING_CHAR] = { GATT_DECL_CHARACTERISTIC,    PROP(RD),            0                                },
    // Ringer Settings Characteristic Value
    [PASS_IDX_RINGER_SETTING_VAL]  = { GATT_CHAR_RINGER_SETTING,    PROP(RD) | PROP(N),  OPT(NO_OFFSET)                   },
    // Ringer Settings Characteristic - Client Characteristic Configuration Descriptor
    [PASS_IDX_RINGER_SETTING_CFG]  = { GATT_DESC_CLIENT_CHAR_CFG,   PROP(RD) | PROP(WR), OPT(NO_OFFSET)                   },

    // Ringer Control Point Characteristic Declaration
    [PASS_IDX_RINGER_CTNL_PT_CHAR] = { GATT_DECL_CHARACTERISTIC,    PROP(RD),            0                                },
    // Ringer Control Point Characteristic Value
    [PASS_IDX_RINGER_CTNL_PT_VAL]  = { GATT_CHAR_RINGER_CNTL_POINT, PROP(WC),            OPT(NO_OFFSET) | sizeof(uint8_t) },
};

/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief  This function fully manages notifications
 ****************************************************************************************
 */
__STATIC void pasps_exe_operation(pasps_env_t* p_pasps_env)
{
    if(!p_pasps_env->in_exe_op)
    {
        p_pasps_env->in_exe_op = true;

        while(!co_list_is_empty(&(p_pasps_env->wait_queue)) && !(p_pasps_env->op_ongoing))
        {
            const pasps_cb_t* p_cb = (const pasps_cb_t*) p_pasps_env->prf_env.p_cb;
            uint16_t status = PRF_ERR_NTF_DISABLED;
            co_buf_t* p_buf = (co_buf_t*) co_list_pop_front(&(p_pasps_env->wait_queue));
            pasps_buf_meta_t* p_meta = (pasps_buf_meta_t*) co_buf_metadata(p_buf);
            uint8_t  conidx;
            uint32_t conidx_bf = 0;
            uint8_t  operation = p_meta->operation;

            // check connection that support notification reception
            for(conidx = 0 ; conidx < BLE_CONNECTION_MAX ; conidx++)
            {
                if((p_pasps_env->env[conidx].ntf_state & p_meta->ntf_flag) != 0)
                {
                    conidx_bf |= CO_BIT(conidx);
                }
            }

            // send notification only on selected connections
            conidx_bf &= p_meta->conidx_bf;

            if(conidx_bf != 0)
            {
                // send multi-point notification
                status = gatt_srv_event_mtp_send(conidx_bf, p_pasps_env->user_lid, p_meta->operation,
                                                 GATT_NOTIFY, p_meta->hdl, p_buf, true);
                if(status == GAP_ERR_NO_ERROR)
                {
                    p_pasps_env->op_ongoing = true;
                }
            }
            co_buf_release(p_buf);

            if(!p_pasps_env->op_ongoing)
            {
                switch(operation)
                {
                    case PASPS_UPD_ALERT_STATUS_OP_CODE:
                    {
                        p_cb->cb_alert_status_upd_cmp(status);
                    } break;
                    case PASPS_UPD_RINGER_SETTING_OP_CODE:
                    {
                        p_cb->cb_ringer_setting_upd_cmp(status);
                    } break;
                    default: { /* Nothing to do */ } break;
                }
            }
        }

        p_pasps_env->in_exe_op = false;
    }
}


/**
 ****************************************************************************************
 * @brief Unpack control point data and process it
 *
 * @param[in] p_pasps_env Environment
 * @param[in] conidx     connection index
 * @param[in] p_buf      pointer to input data
 ****************************************************************************************
 */
__STATIC uint16_t pasps_unpack_ctnl_point_req(pasps_env_t *p_pasps_env, uint8_t conidx, co_buf_t* p_buf)
{
    uint8_t ringer_ctnl_pt = co_buf_data(p_buf)[0];
    uint16_t status = GAP_ERR_NO_ERROR;

    if(co_buf_data_len(p_buf) > 0)
    {
        // Inform the HL ?
        bool inform_hl = false;

        // Check the received value
        switch (ringer_ctnl_pt)
        {
            case PASP_SILENT_MODE:
            {
                // Ignore if ringer is already silent
                if (p_pasps_env->env[conidx].ringer_state == PASP_RINGER_NORMAL)
                {
                    inform_hl = true;
                    // Set to Ringer Silent Mode
                    p_pasps_env->env[conidx].ringer_state = PASP_RINGER_SILENT;
                }
            } break;

            case PASP_CANCEL_SILENT_MODE:
            {
                // Ignore if ringer is not silent
                if (p_pasps_env->env[conidx].ringer_state == PASP_RINGER_SILENT)
                {
                    inform_hl = true;
                    // Set to Ringer Normal Mode
                    p_pasps_env->env[conidx].ringer_state = PASP_RINGER_NORMAL;
                }
            } break;

            case PASP_MUTE_ONCE: {  inform_hl = true;  } break;
            default:             { /* Nothing to do */ } break;
        }

        if (inform_hl)
        {
            const pasps_cb_t* p_cb  = (const pasps_cb_t*) p_pasps_env->prf_env.p_cb;

            // inform application about control point update
            p_cb->cb_ctnl_pt(conidx, ringer_ctnl_pt);
        }
    }
    else
    {
        status = PRF_APP_ERROR;
    }

    return (status);
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
__STATIC void pasps_cb_att_read_get(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset,
                                   uint16_t max_length)
{
    pasps_env_t *p_pasps_env = PRF_ENV_GET(PASPS, pasps);
    // retrieve value attribute
    co_buf_t* p_buf       = NULL;
    uint16_t  status      = GAP_ERR_NO_ERROR;
    uint16_t  att_val_len = 0;

    if(p_pasps_env == NULL)
    {
        status = PRF_APP_ERROR;
    }
    else if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, 0,  sizeof(uint16_t) + GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
    {
        uint8_t att_idx = hdl - p_pasps_env->start_hdl;

        switch (att_idx)
        {
            case PASS_IDX_ALERT_STATUS_CFG:
            {
                uint16_t ntf_cfg = (p_pasps_env->env[conidx].ntf_state & PASPS_FLAG_ALERT_STATUS_CFG)
                                 ? PRF_CLI_START_NTF : PRF_CLI_STOP_NTFIND;

                co_write16p(co_buf_tail(p_buf), co_htobs(ntf_cfg));
                co_buf_tail_reserve(p_buf, 2);
            } break;

            case PASS_IDX_RINGER_SETTING_CFG:
            {
                uint16_t ntf_cfg = (p_pasps_env->env[conidx].ntf_state & PASPS_FLAG_RINGER_SETTING_CFG)
                                 ? PRF_CLI_START_NTF : PRF_CLI_STOP_NTFIND;

                co_write16p(co_buf_tail(p_buf), co_htobs(ntf_cfg));
                co_buf_tail_reserve(p_buf, 2);
            } break;

            case PASS_IDX_ALERT_STATUS_VAL:
            {
                co_buf_tail(p_buf)[0] = p_pasps_env->alert_status;
                co_buf_tail_reserve(p_buf, 1);
            } break;

            case PASS_IDX_RINGER_SETTING_VAL:
            {
                co_buf_tail(p_buf)[0] = p_pasps_env->ringer_setting;
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
__STATIC void pasps_cb_att_val_set(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset,
                                   co_buf_t* p_data)
{
    pasps_env_t *p_pasps_env = PRF_ENV_GET(PASPS, pasps);
    uint16_t  status         = PRF_APP_ERROR;

    if(p_pasps_env != NULL)
    {
        uint8_t  cfg_upd_flag  = 0;
        uint8_t  cfg_upd_char  = 0;
        uint16_t cfg_en_val = 0;

        switch (hdl - p_pasps_env->start_hdl)
        {
            case PASS_IDX_ALERT_STATUS_CFG:
            {
                cfg_upd_char = PASPS_ALERT_STATUS_NTF_CFG;
                cfg_upd_flag = PASPS_FLAG_ALERT_STATUS_CFG;
                cfg_en_val   = PRF_CLI_START_NTF;
            } break;

            case PASS_IDX_RINGER_SETTING_CFG:
            {
                cfg_upd_char = PASPS_RINGER_SETTING_NTF_CFG;
                cfg_upd_flag = PASPS_FLAG_RINGER_SETTING_CFG;
                cfg_en_val   = PRF_CLI_START_NTF;
            } break;

            case PASS_IDX_RINGER_CTNL_PT_VAL:
            {
                // Unpack Control Point parameters
                status = pasps_unpack_ctnl_point_req(p_pasps_env, conidx, p_data);
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
                const pasps_cb_t* p_cb  = (const pasps_cb_t*) p_pasps_env->prf_env.p_cb;

                if(cfg == PRF_CLI_STOP_NTFIND)
                {
                    p_pasps_env->env[conidx].ntf_state &= ~cfg_upd_flag;
                }
                else
                {
                    p_pasps_env->env[conidx].ntf_state |= cfg_upd_flag;
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
__STATIC void pasps_cb_event_sent(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    // Consider job done
    pasps_env_t *p_pasps_env = PRF_ENV_GET(PASPS, pasps);
    if(p_pasps_env != NULL)
    {
        const pasps_cb_t* p_cb  = (const pasps_cb_t*) p_pasps_env->prf_env.p_cb;
        p_pasps_env->op_ongoing = false;

        switch(dummy)
        {
            case PASPS_UPD_ALERT_STATUS_OP_CODE:
            {
                p_cb->cb_alert_status_upd_cmp(status);
            } break;
            case PASPS_UPD_RINGER_SETTING_OP_CODE:
            {
                p_cb->cb_ringer_setting_upd_cmp(status);
            } break;
            default: { /* Nothing to do */ } break;
        }

        // continue operation execution
        pasps_exe_operation(p_pasps_env);
    }
}

/// Service callback hander
__STATIC const gatt_srv_cb_t pasps_cb =
{
        .cb_event_sent    = pasps_cb_event_sent,
        .cb_att_read_get  = pasps_cb_att_read_get,
        .cb_att_event_get = NULL,
        .cb_att_info_get  = NULL,
        .cb_att_val_set   = pasps_cb_att_val_set,
};

/*
 * PROFILE NATIVE HANDLERS
 ****************************************************************************************
 */

uint16_t pasps_enable(uint8_t conidx, uint16_t alert_status_ntf_cfg, uint16_t ringer_setting_ntf_cfg)
{
    pasps_env_t* p_pasps_env = PRF_ENV_GET(PASPS, pasps);
    uint16_t status = PRF_ERR_REQ_DISALLOWED;

    if(p_pasps_env != NULL)
    {
        // check state of the task
        if (gapc_get_conhdl(conidx) != GAP_INVALID_CONHDL)
        {
            uint8_t ntf_state = p_pasps_env->env[conidx].ntf_state;

            // Bonded data was not used before
            if (!(ntf_state & PASPS_FLAG_CFG_PERFORMED_OK))
            {
                status = GAP_ERR_NO_ERROR;
                if (alert_status_ntf_cfg != PRF_CLI_STOP_NTFIND)
                {
                    ntf_state |= PASPS_FLAG_ALERT_STATUS_CFG;
                }

                if (ringer_setting_ntf_cfg != PRF_CLI_STOP_NTFIND)
                {
                    ntf_state |= PASPS_FLAG_RINGER_SETTING_CFG;
                }

                // Enable Bonded Data
                ntf_state |= PASPS_FLAG_CFG_PERFORMED_OK;
                p_pasps_env->env[conidx].ntf_state = ntf_state;
            }
        }
    }

    return (status);
}

uint16_t pasps_alert_status_upd(uint32_t conidx_bf, uint8_t value)
{
    pasps_env_t* p_pasps_env = PRF_ENV_GET(PASPS, pasps);
    uint16_t status = PRF_ERR_REQ_DISALLOWED;

    if (value > PASP_ALERT_STATUS_VAL_MAX)
    {
        status = PRF_ERR_INVALID_PARAM;
    }
    else if(p_pasps_env != NULL)
    {
        co_buf_t* p_buf;

        // Update the alert status
        p_pasps_env->alert_status = value;

        if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, sizeof(uint8_t), GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
        {
            pasps_buf_meta_t* p_buf_meta = (pasps_buf_meta_t*) co_buf_metadata(p_buf);
            p_buf_meta->operation = PASPS_UPD_ALERT_STATUS_OP_CODE;
            p_buf_meta->conidx    = GAP_INVALID_CONIDX;
            p_buf_meta->conidx_bf = conidx_bf & ((1 << BLE_CONNECTION_MAX) - 1);
            p_buf_meta->hdl       = p_pasps_env->start_hdl + PASS_IDX_ALERT_STATUS_VAL;
            p_buf_meta->ntf_flag  = PASPS_FLAG_ALERT_STATUS_CFG;


            co_buf_data(p_buf)[0] = value;

            // put event on wait queue
            co_list_push_back(&(p_pasps_env->wait_queue), &(p_buf->hdr));
            // execute operation
            pasps_exe_operation(p_pasps_env);
            status = GAP_ERR_NO_ERROR;
        }
        else
        {
            status = GAP_ERR_INSUFF_RESOURCES;
        }
    }

    return (status);
}

uint16_t pasps_ringer_setting_upd(uint32_t conidx_bf, uint8_t value)
{
    pasps_env_t* p_pasps_env = PRF_ENV_GET(PASPS, pasps);
    uint16_t status = PRF_ERR_REQ_DISALLOWED;

    if (value > PASP_RINGER_NORMAL)
    {
        status = PRF_ERR_INVALID_PARAM;
    }
    else if(p_pasps_env != NULL)
    {
        co_buf_t* p_buf;

        // Update the ringer state value
        p_pasps_env->ringer_setting = value;

        if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, sizeof(uint8_t), GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
        {
            pasps_buf_meta_t* p_buf_meta = (pasps_buf_meta_t*) co_buf_metadata(p_buf);
            p_buf_meta->operation = PASPS_UPD_RINGER_SETTING_OP_CODE;
            p_buf_meta->conidx    = GAP_INVALID_CONIDX;
            p_buf_meta->conidx_bf = conidx_bf & ((1 << BLE_CONNECTION_MAX) - 1);
            p_buf_meta->hdl       = p_pasps_env->start_hdl + PASS_IDX_RINGER_SETTING_VAL;
            p_buf_meta->ntf_flag  = PASPS_FLAG_RINGER_SETTING_CFG;

            co_buf_data(p_buf)[0] = value;

            // put event on wait queue
            co_list_push_back(&(p_pasps_env->wait_queue), &(p_buf->hdr));
            // execute operation
            pasps_exe_operation(p_pasps_env);
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
 * @brief Send a PASPS_CMP_EVT message to the application.
 * @param[in] conidx connection index
 * @param[in] operation Operation completed
 * @param[in] status status of the operation
 ****************************************************************************************
 */
__STATIC void pasps_send_cmp_evt(uint8_t conidx, uint8_t operation, uint16_t status)
{
    struct pasps_cmp_evt *p_evt;

    // Send the message
    p_evt = KE_MSG_ALLOC(PASPS_CMP_EVT, PRF_DST_TASK(PASPS), PRF_SRC_TASK(PASPS), pasps_cmp_evt);

    if(p_evt != NULL)
    {
        p_evt->operation  = operation;
        p_evt->status     = status;
        ke_msg_send(p_evt);
    }
}


/**
 ****************************************************************************************
 * @brief Handles reception of the @ref PASPS_ENABLE_REQ message.
 * @param[in] msgid Id of the message received
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int pasps_enable_req_handler(ke_msg_id_t const msgid, struct pasps_enable_req *p_param,
                                      ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    struct pasps_enable_rsp *p_cmp_evt;
    uint16_t status = pasps_enable(p_param->conidx, p_param->alert_status_ntf_cfg, p_param->ringer_setting_ntf_cfg);

    // send completed information to APP task that contains error status
    p_cmp_evt = KE_MSG_ALLOC(PASPS_ENABLE_RSP, src_id, dest_id, pasps_enable_rsp);

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
 * @brief Handles reception of the @ref PASPS_UPDATE_CHAR_VAL_REQ message.
 * @param[in] msgid Id of the message received.
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int pasps_update_char_val_cmd_handler(ke_msg_id_t const msgid, struct pasps_update_char_val_cmd *p_param,
                                               ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status;

    switch(p_param->operation)
    {
        // Alert Status Characteristic
        case PASPS_UPD_ALERT_STATUS_OP_CODE:   { status = pasps_alert_status_upd(p_param->conidx_bf, p_param->value);   } break;
        // Ringer Setting Characteristic
        case PASPS_UPD_RINGER_SETTING_OP_CODE: { status = pasps_ringer_setting_upd(p_param->conidx_bf, p_param->value); } break;
        default:                               { status = PRF_ERR_INVALID_PARAM;                                        } break;
    }

    if(status != GAP_ERR_NO_ERROR)
    {
        pasps_send_cmp_evt(GAP_INVALID_CONIDX, p_param->operation, status);
    }

    return (KE_MSG_CONSUMED);
}



/// Default State handlers definition
KE_MSG_HANDLER_TAB(pasps)
{
    // Note: all messages must be sorted in ID ascending order

    { PASPS_ENABLE_REQ,              (ke_msg_func_t) pasps_enable_req_handler             },
    { PASPS_UPDATE_CHAR_VAL_CMD,     (ke_msg_func_t) pasps_update_char_val_cmd_handler    },
};


/**
 ****************************************************************************************
 * @brief Inform that bond data updated for the connection.
 *
 * @param[in] conidx        Connection index
 * @param[in] char_code     Characteristic Code
 *                              - PASPS_ALERT_STATUS_NTF_CFG
 *                              - PASPS_RINGER_SETTING_NTF_CFG
 * @param[in] cfg_val       Stop/notify/indicate value to configure into the peer characteristic
 ****************************************************************************************
 */
__STATIC void pasps_cb_bond_data_upd(uint8_t conidx, uint8_t char_code, uint16_t cfg_val)
{
    struct pasps_written_char_val_ind *p_evt;

    // Send the message
    p_evt = KE_MSG_ALLOC(PASPS_WRITTEN_CHAR_VAL_IND, PRF_DST_TASK(PASPS),
                         PRF_SRC_TASK(PASPS), pasps_written_char_val_ind);

    if(p_evt != NULL)
    {
        p_evt->conidx                     = conidx;
        p_evt->att_code                   = char_code;
        p_evt->value.alert_status_ntf_cfg = cfg_val;
        ke_msg_send(p_evt);
    }
}


/**
 ****************************************************************************************
 * @brief Completion of alert status update (and registered peer device notification)
 *
 * @param[in] status Status of the procedure execution (@see enum hl_err)
 ****************************************************************************************
 */
__STATIC void pasps_cb_alert_status_upd_cmp(uint16_t status)
{
    pasps_send_cmp_evt(GAP_INVALID_CONIDX, PASPS_UPD_ALERT_STATUS_OP_CODE, status);
}

/**
 ****************************************************************************************
 * @brief Completion of ringer setting update (and registered peer device notification)
 *
 * @param[in] status Status of the procedure execution (@see enum hl_err)
 ****************************************************************************************
 */
__STATIC void pasps_cb_ringer_setting_upd_cmp(uint16_t status)
{
    pasps_send_cmp_evt(GAP_INVALID_CONIDX, PASPS_UPD_RINGER_SETTING_OP_CODE, status);
}

/**
 ****************************************************************************************
 * @brief Inform that peer device requests an action using control point
 *
 * @param[in] conidx         Connection index
 * @param[in] ringer_ctnl_pt Ringer Control Point value
 ****************************************************************************************
 */
__STATIC void pasps_cb_ctnl_pt(uint8_t conidx, uint8_t ringer_ctnl_pt)
{
    struct pasps_written_char_val_ind *p_evt;

    // Send the message
    p_evt = KE_MSG_ALLOC(PASPS_WRITTEN_CHAR_VAL_IND, PRF_DST_TASK(PASPS),
                         PRF_SRC_TASK(PASPS), pasps_written_char_val_ind);

    if(p_evt != NULL)
    {
        p_evt->conidx               = conidx;
        p_evt->att_code             = PASPS_RINGER_CTNL_PT_CHAR_VAL;
        p_evt->value.ringer_ctnl_pt = ringer_ctnl_pt;
        ke_msg_send(p_evt);
    }
}


/// Default Message handle
__STATIC const pasps_cb_t pasps_msg_cb =
{
        .cb_bond_data_upd          = pasps_cb_bond_data_upd,
        .cb_alert_status_upd_cmp   = pasps_cb_alert_status_upd_cmp,
        .cb_ringer_setting_upd_cmp = pasps_cb_ringer_setting_upd_cmp,
        .cb_ctnl_pt                = pasps_cb_ctnl_pt,
};
#endif // (BLE_HL_MSG_API)

/*
 * PROFILE DEFAULT HANDLERS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Initialization of the PASPS module.
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
__STATIC uint16_t pasps_init(prf_data_t *p_env, uint16_t *p_start_hdl, uint8_t sec_lvl, uint8_t user_prio,
                           struct pasps_db_cfg *p_params, const pasps_cb_t* p_cb)
{
    //------------------ create the attribute database for the profile -------------------

    // DB Creation Status
    uint16_t status = GAP_ERR_NO_ERROR;
    uint8_t user_lid = GATT_INVALID_USER_LID;

    do
    {
        pasps_env_t* p_pasps_env;
        // Service content flag
        uint32_t cfg_flag = PASPS_DB_CFG_FLAG;

        #if (BLE_HL_MSG_API)
        if(p_cb == NULL)
        {
            p_cb = &(pasps_msg_cb);
        }
        #endif // (BLE_HL_MSG_API)

        if(   (p_params == NULL) || (p_start_hdl == NULL) || (p_cb == NULL) || (p_cb->cb_alert_status_upd_cmp == NULL)
           || (p_cb->cb_bond_data_upd == NULL) || (p_cb->cb_ctnl_pt == NULL) || (p_cb->cb_ringer_setting_upd_cmp == NULL))
        {
            status = GAP_ERR_INVALID_PARAM;
            break;
        }

        // register PASPS user
        status = gatt_user_srv_register(L2CAP_LE_MTU_MIN, user_prio, &pasps_cb, &user_lid);
        if(status != GAP_ERR_NO_ERROR) break;


        // Add GAP service
        status = gatt_db_svc16_add(user_lid, sec_lvl, GATT_SVC_PHONE_ALERT_STATUS, PASS_IDX_NB,
                                   (uint8_t *)&cfg_flag, &(pasps_att_db[0]), PASS_IDX_NB, p_start_hdl);
        if(status != GAP_ERR_NO_ERROR) break;

        //-------------------- allocate memory required for the profile  ---------------------
        p_pasps_env = (pasps_env_t *) ke_malloc(sizeof(pasps_env_t), KE_MEM_ATT_DB);

        if(p_pasps_env != NULL)
        {
            // allocate PASPS required environment variable
            p_env->p_env = (prf_hdr_t *) p_pasps_env;
            p_pasps_env->start_hdl       = *p_start_hdl;
            p_pasps_env->user_lid        = user_lid;
            p_pasps_env->op_ongoing      = false;
            p_pasps_env->in_exe_op       = false;
            memset(p_pasps_env->env, 0, sizeof(p_pasps_env->env));
            co_list_init(&(p_pasps_env->wait_queue));

            p_pasps_env->alert_status   = p_params->alert_status;
            p_pasps_env->ringer_setting = p_params->ringer_setting;

            // initialize profile environment variable
            p_pasps_env->prf_env.p_cb     = p_cb;
            #if (BLE_HL_MSG_API)
            p_env->desc.msg_handler_tab  = pasps_msg_handler_tab;
            p_env->desc.msg_cnt          = ARRAY_LEN(pasps_msg_handler_tab);
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
__STATIC uint16_t pasps_destroy(prf_data_t *p_env, uint8_t reason)
{
    uint16_t status = GAP_ERR_NO_ERROR;
    pasps_env_t *p_pasps_env = (pasps_env_t *) p_env->p_env;

    if(reason != PRF_DESTROY_RESET)
    {
        status = gatt_user_unregister(p_pasps_env->user_lid);
    }

    if(status == GAP_ERR_NO_ERROR)
    {
        if(reason != PRF_DESTROY_RESET)
        {
            // remove buffer in wait queue
            while(!co_list_is_empty(&p_pasps_env->wait_queue))
            {
                co_buf_t* p_buf = (co_buf_t*) co_list_pop_front(&p_pasps_env->wait_queue);
                co_buf_release(p_buf);
            }
        }

        // free profile environment variables
        p_env->p_env = NULL;
        ke_free(p_pasps_env);
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
__STATIC void pasps_con_create(prf_data_t *p_env, uint8_t conidx, const gap_con_param_t* p_con_param)
{
    pasps_env_t *p_pasps_env = (pasps_env_t *) p_env->p_env;
    p_pasps_env->env[conidx].ntf_state    = 0;
    p_pasps_env->env[conidx].ringer_state = p_pasps_env->ringer_setting;
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
__STATIC void pasps_con_cleanup(prf_data_t *p_env, uint8_t conidx, uint16_t reason)
{
    pasps_env_t *p_pasps_env = (pasps_env_t *) p_env->p_env;
    memset(&(p_pasps_env->env[conidx]), 0, sizeof(struct pasps_cnx_env));
}

/// PASPS Task interface required by profile manager
const prf_task_cbs_t pasps_itf =
{
    .cb_init          = (prf_init_cb) pasps_init,
    .cb_destroy       = pasps_destroy,
    .cb_con_create    = pasps_con_create,
    .cb_con_cleanup   = pasps_con_cleanup,
    .cb_con_upd       = NULL,
};

/**
 ****************************************************************************************
 * @brief Retrieve service profile interface
 *
 * @return service profile interface
 ****************************************************************************************
 */
const prf_task_cbs_t *pasps_prf_itf_get(void)
{
    return &pasps_itf;
}
#endif //(BLE_PASP_SERVER)

/// @} PASPS

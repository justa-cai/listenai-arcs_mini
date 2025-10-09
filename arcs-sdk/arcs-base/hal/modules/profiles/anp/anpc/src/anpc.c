/**
 ****************************************************************************************
 *
 * @file anpc.c
 *
 * @brief Alert Notification Profile Client implementation.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 ****************************************************************************************
 */


/**
 ****************************************************************************************
 * @addtogroup ANPC
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "btip_config.h"

#if (BLE_AN_CLIENT)
#include "anpc.h"
#include "anp_common.h"

#include "gap.h"
#include "gatt.h"

#include "ke_mem.h"
#include "co_utils.h"
#include "co_endian.h"
#include <string.h>

/*
 * DEFINES
 ****************************************************************************************
 */

/// Maximum number of Alert Notification Client task instances

/*
 * ENUMERATIONS
 ****************************************************************************************
 */


/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */

/// Connection environment data
typedef struct anpc_cnx_env
{
    /// Phone Alert Status Service Characteristics
    anpc_ans_content_t ans;
    /// Counter used to check service uniqueness
    uint8_t            nb_svc;
    /// Discovery procedure on-going
    bool               discover;
} anpc_cnx_env_t;

/// Alert Notification Profile Client environment variable
typedef struct anpc_env
{
    /// profile environment
    prf_hdr_t       prf_env;
    /// Environment variable pointer for each connections
    anpc_cnx_env_t* p_env[BLE_CONNECTION_MAX];
    /// GATT User local identifier
    uint8_t         user_lid;
} anpc_env_t;

/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */

/// State machine used to retrieve Alert Notification service characteristics information
const prf_char_def_t anpc_ans_char[ANPC_CHAR_MAX] =
{
    // Supported New Alert Category
    [ANPC_CHAR_SUP_NEW_ALERT_CAT]      = {GATT_CHAR_SUP_NEW_ALERT_CAT,    ATT_REQ(PRES, MAND), PROP(RD) },
    // New Alert
    [ANPC_CHAR_NEW_ALERT]              = {GATT_CHAR_NEW_ALERT,            ATT_REQ(PRES, MAND), PROP(N)  },
    // Supported Unread Alert Category
    [ANPC_CHAR_SUP_UNREAD_ALERT_CAT]   = {GATT_CHAR_SUP_UNREAD_ALERT_CAT, ATT_REQ(PRES, MAND), PROP(RD) },
    // Unread Alert Status
    [ANPC_CHAR_UNREAD_ALERT_STATUS]    = {GATT_CHAR_UNREAD_ALERT_STATUS,  ATT_REQ(PRES, MAND), PROP(N)  },
    // Alert Notification Control Point
    [ANPC_CHAR_ALERT_NTF_CTNL_PT]      = {GATT_CHAR_ALERT_NTF_CTNL_PT,    ATT_REQ(PRES, MAND), PROP(WR) },
};

/// State machine used to retrieve Phone Alert Status service characteristic descriptor information
const prf_desc_def_t anpc_ans_char_desc[ANPC_DESC_MAX] =
{
    // New Alert Char. - Client Characteristic Configuration
    [ANPC_DESC_NEW_ALERT_CL_CFG]           = { GATT_DESC_CLIENT_CHAR_CFG, ATT_REQ(PRES, MAND), ANPC_CHAR_NEW_ALERT           },
    // Unread Alert Status Char. - Client Characteristic Configuration
    [ANPC_DESC_UNREAD_ALERT_STATUS_CL_CFG] = { GATT_DESC_CLIENT_CHAR_CFG, ATT_REQ(PRES, MAND), ANPC_CHAR_UNREAD_ALERT_STATUS },
};

/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Send discovery results to application.
 *
 * @param p_anpc_env    Client Role task environment
 * @param conidx        Connection index
 * @param status        Response status code
 *****************************************************************************************
 */
__STATIC void anpc_enable_cmp(anpc_env_t* p_anpc_env, uint8_t conidx, uint16_t status)
{
    const anpc_cb_t* p_cb = (const anpc_cb_t*) p_anpc_env->prf_env.p_cb;

    if(p_anpc_env != NULL)
    {
        p_cb->cb_enable_cmp(conidx, status, &(p_anpc_env->p_env[conidx]->ans));

        if (status != GAP_ERR_NO_ERROR)
        {
            // clean-up environment variable allocated for task instance
            ke_free(p_anpc_env->p_env[conidx]);
            p_anpc_env->p_env[conidx] = NULL;
        }
        else
        {
             p_anpc_env->p_env[conidx]->discover = false;

             // Register profile handle to catch gatt indications
             gatt_cli_event_register(conidx, p_anpc_env->user_lid, p_anpc_env->p_env[conidx]->ans.svc.shdl,
                                     p_anpc_env->p_env[conidx]->ans.svc.ehdl);

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
__STATIC void anpc_svc_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t hdl, uint8_t disc_info,
                          uint8_t nb_att, const gatt_svc_att_t* p_atts)
{
    // Get the address of the environment
    anpc_env_t* p_anpc_env = PRF_ENV_GET(ANPC, anpc);

    if(p_anpc_env != NULL)
    {
        ASSERT_INFO(p_anpc_env->p_env[conidx] != NULL, conidx, user_lid);

        if (p_anpc_env->p_env[conidx]->nb_svc == 0)
        {
            //Even if we get multiple responses we only store 1 range
            if((disc_info == GATT_SVC_CMPLT) || (disc_info == GATT_SVC_START))
            {
                p_anpc_env->p_env[conidx]->ans.svc.shdl = hdl;
            }

            if((disc_info == GATT_SVC_CMPLT) || (disc_info == GATT_SVC_END))
            {
                p_anpc_env->p_env[conidx]->ans.svc.ehdl = hdl + nb_att -1;
            }

            // Retrieve characteristics
            prf_extract_svc_info(hdl, nb_att, p_atts,
                                 ANPC_CHAR_MAX, &anpc_ans_char[0],      &(p_anpc_env->p_env[conidx]->ans.chars[0]),
                                 ANPC_DESC_MAX, &anpc_ans_char_desc[0], &(p_anpc_env->p_env[conidx]->ans.descs[0]));
        }

        if((disc_info == GATT_SVC_CMPLT) || (disc_info == GATT_SVC_END))
        {
            p_anpc_env->p_env[conidx]->nb_svc++;
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
__STATIC void anpc_discover_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    // Get the address of the environment
    anpc_env_t* p_anpc_env = PRF_ENV_GET(ANPC, anpc);

    if(p_anpc_env != NULL)
    {
        if (p_anpc_env->p_env[conidx]->nb_svc ==  1)
        {
            status = prf_check_svc_validity(ANPC_CHAR_MAX, p_anpc_env->p_env[conidx]->ans.chars, anpc_ans_char,
                                            ANPC_DESC_MAX, p_anpc_env->p_env[conidx]->ans.descs, anpc_ans_char_desc);
            p_anpc_env->p_env[conidx]->nb_svc = 0;
        }
        // too much services
        else if (p_anpc_env->p_env[conidx]->nb_svc > 1)
        {
            status = PRF_ERR_MULTIPLE_SVC;
        }
        // no services found
        else
        {
            status = PRF_ERR_STOP_DISC_CHAR_MISSING;
        }

        anpc_enable_cmp(p_anpc_env, conidx, status);
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
__STATIC void anpc_att_val_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t hdl, uint16_t offset,
                              co_buf_t* p_data)
{
    anpc_env_t* p_anpc_env = PRF_ENV_GET(ANPC, anpc);

    if((p_anpc_env != NULL) && (p_anpc_env->p_env[conidx] != NULL))
    {
        const anpc_cb_t* p_cb = (const anpc_cb_t*) p_anpc_env->prf_env.p_cb;

        switch(dummy)
        {
            case ANPC_RD_SUP_NEW_ALERT_CAT:
            case ANPC_RD_SUP_UNREAD_ALERT_CAT:
            {
                uint8_t cat_id_mask_0 = co_buf_data(p_data)[0];
                uint8_t cat_id_mask_1 = co_buf_data(p_data)[1];
                p_cb->cb_supp_cat(conidx, dummy, cat_id_mask_0, cat_id_mask_1);
            } break;

            case ANPC_RD_WR_NEW_ALERT_CFG:
            case ANPC_RD_WR_UNREAD_ALERT_STATUS_CFG:
            {
                uint16_t ntf_cfg = co_btohs(co_read16p(co_buf_data(p_data)));
                p_cb->cb_ntf_cfg(conidx, dummy, ntf_cfg);
            } break;
            default: {/* Nothing to do */ } break;
        }
    }
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
__STATIC void anpc_read_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    anpc_env_t* p_anpc_env = PRF_ENV_GET(ANPC, anpc);

    if(p_anpc_env != NULL)
    {
        const anpc_cb_t* p_cb = (const anpc_cb_t*) p_anpc_env->prf_env.p_cb;

        p_cb->cb_read_cmp(conidx, status, dummy);
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
__STATIC void anpc_write_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    anpc_env_t* p_anpc_env = PRF_ENV_GET(ANPC, anpc);

    if(p_anpc_env != NULL)
    {
        const anpc_cb_t* p_cb = (const anpc_cb_t*) p_anpc_env->prf_env.p_cb;

        p_cb->cb_write_cmp(conidx, status, dummy);
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
__STATIC void anpc_att_val_evt_cb(uint8_t conidx, uint8_t user_lid, uint16_t token, uint8_t evt_type, bool complete,
                                  uint16_t hdl, co_buf_t* p_data)
{
    anpc_env_t* p_anpc_env = PRF_ENV_GET(ANPC, anpc);

    if((p_anpc_env != NULL) && (p_anpc_env->p_env[conidx] != NULL))
    {
        anpc_ans_content_t* p_ans = &(p_anpc_env->p_env[conidx]->ans);
        const anpc_cb_t* p_cb = (const anpc_cb_t*) p_anpc_env->prf_env.p_cb;

        if(co_buf_data_len(p_data) >= 2)
        {
            uint8_t cat_id, nb_alert;

            cat_id   = co_buf_data(p_data)[0];
            co_buf_head_release(p_data, 1);
            nb_alert = co_buf_data(p_data)[0];
            co_buf_head_release(p_data, 1);

            if(hdl == p_ans->chars[ANPC_CHAR_NEW_ALERT].val_hdl)
            {
                p_cb->cb_new_alert_upd(conidx, cat_id, nb_alert, co_buf_data_len(p_data), co_buf_data(p_data));
            }
            else if (hdl == p_ans->chars[ANPC_CHAR_UNREAD_ALERT_STATUS].val_hdl)
            {
                p_cb->cb_unread_alert_upd(conidx, cat_id, nb_alert);
            }
            // else do nothing
        }
    }

    // confirm handling of notification
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
__STATIC void anpc_svc_changed_cb(uint8_t conidx, uint8_t user_lid, bool out_of_sync, uint16_t start_hdl, uint16_t end_hdl)
{
    // Nothing to do
}

/// Client callback hander
__STATIC const gatt_cli_cb_t anpc_cb =
{
    .cb_discover_cmp    = anpc_discover_cmp_cb,
    .cb_read_cmp        = anpc_read_cmp_cb,
    .cb_write_cmp       = anpc_write_cmp_cb,
    .cb_att_val_get     = NULL,
    .cb_svc             = anpc_svc_cb,
    .cb_svc_info        = NULL,
    .cb_inc_svc         = NULL,
    .cb_char            = NULL,
    .cb_desc            = NULL,
    .cb_att_val         = anpc_att_val_cb,
    .cb_att_val_evt     = anpc_att_val_evt_cb,
    .cb_svc_changed     = anpc_svc_changed_cb,
};

/*
 * PROFILE NATIVE API
 ****************************************************************************************
 */

uint16_t anpc_enable(uint8_t conidx, uint8_t con_type, const anpc_ans_content_t* p_ans)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    anpc_env_t* p_anpc_env = PRF_ENV_GET(ANPC, anpc);

    if(p_anpc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_anpc_env->p_env[conidx] == NULL))
        {
            // allocate environment variable for task instance
            p_anpc_env->p_env[conidx] = (struct anpc_cnx_env *) ke_malloc(sizeof(struct anpc_cnx_env), KE_MEM_ATT_DB);

            if(p_anpc_env->p_env[conidx] != NULL)
            {
                // Config connection, start discovering
                if (con_type == PRF_CON_DISCOVERY)
                {
                    uint16_t gatt_svc_uuid = GATT_SVC_ALERT_NTF;
                    memset(p_anpc_env->p_env[conidx], 0, sizeof(struct anpc_cnx_env));

                    // start discovery
                    status = gatt_cli_discover_svc(conidx, p_anpc_env->user_lid, 0, GATT_DISCOVER_SVC_PRIMARY_BY_UUID, true,
                                                   GATT_MIN_HDL, GATT_MAX_HDL, GATT_UUID_16, (uint8_t*) &gatt_svc_uuid);

                    // Go to DISCOVERING state
                    p_anpc_env->p_env[conidx]->discover = true;
                }
                // normal connection, get saved att details
                else
                {
                    memcpy(&(p_anpc_env->p_env[conidx]->ans), p_ans, sizeof(anpc_ans_content_t));
                    status = GAP_ERR_NO_ERROR;

                    // send APP confirmation that can start normal connection to TH
                    anpc_enable_cmp(p_anpc_env, conidx, GAP_ERR_NO_ERROR);
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

uint16_t anpc_read(uint8_t conidx, uint8_t read_code)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    anpc_env_t* p_anpc_env = PRF_ENV_GET(ANPC, anpc);

    if(p_anpc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_anpc_env->p_env[conidx] != NULL) && !(p_anpc_env->p_env[conidx]->discover))
        {
            // Attribute Handle
            uint16_t handle = GATT_INVALID_HDL;
            anpc_ans_content_t* p_ans = &(p_anpc_env->p_env[conidx]->ans);
            status = PRF_ERR_INEXISTENT_HDL;

            switch (read_code)
            {
                // Read Supported New Alert
                case ANPC_RD_SUP_NEW_ALERT_CAT:          { handle = p_ans->chars[ANPC_CHAR_SUP_NEW_ALERT_CAT].val_hdl;           } break;
                // Read Supported Unread Alert
                case ANPC_RD_SUP_UNREAD_ALERT_CAT:       { handle = p_ans->chars[ANPC_CHAR_SUP_UNREAD_ALERT_CAT].val_hdl;        } break;
                // Read New Alert Characteristic Client Char. Cfg. Descriptor Value
                case ANPC_RD_WR_NEW_ALERT_CFG:           { handle = p_ans->descs[ANPC_DESC_NEW_ALERT_CL_CFG].desc_hdl;           } break;
                // Read Unread Alert Characteristic Client Char. Cfg. Descriptor Value
                case ANPC_RD_WR_UNREAD_ALERT_STATUS_CFG: { handle = p_ans->descs[ANPC_DESC_UNREAD_ALERT_STATUS_CL_CFG].desc_hdl; } break;
                default:                                 { status = PRF_ERR_INVALID_PARAM;                                       } break;
            }

            if(handle != GATT_INVALID_HDL)
            {
                // read attribute value
                status = gatt_cli_read(conidx, p_anpc_env->user_lid, read_code, handle, 0, 0);
            }
        }
    }

    return (status);
}

uint16_t anpc_write_new_alert_ntf_cfg(uint8_t conidx, uint16_t ntf_cfg)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    anpc_env_t* p_anpc_env = PRF_ENV_GET(ANPC, anpc);

    if (ntf_cfg > PRF_CLI_START_NTF)
    {
        status = PRF_ERR_INVALID_PARAM;
    }
    else if(p_anpc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_anpc_env->p_env[conidx] != NULL) && !(p_anpc_env->p_env[conidx]->discover))
        {
            uint16_t handle = p_anpc_env->p_env[conidx]->ans.descs[ANPC_DESC_NEW_ALERT_CL_CFG].desc_hdl;

            if(handle != GATT_INVALID_HDL)
            {
                ntf_cfg = co_btohs(ntf_cfg); // force Little Endian

                // Write control point
                status = prf_gatt_write(conidx, p_anpc_env->user_lid, ANPC_RD_WR_NEW_ALERT_CFG,
                                        GATT_WRITE, handle, sizeof(uint16_t), (uint8_t*) &ntf_cfg);
            }
            else
            {
                status = PRF_ERR_INEXISTENT_HDL;
            }
        }
    }

    return (status);
}

uint16_t anpc_write_unread_alert_ntf_cfg(uint8_t conidx, uint16_t ntf_cfg)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    anpc_env_t* p_anpc_env = PRF_ENV_GET(ANPC, anpc);

    if (ntf_cfg > PRF_CLI_START_NTF)
    {
        status = PRF_ERR_INVALID_PARAM;
    }
    else if(p_anpc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_anpc_env->p_env[conidx] != NULL) && !(p_anpc_env->p_env[conidx]->discover))
        {
            uint16_t handle = p_anpc_env->p_env[conidx]->ans.descs[ANPC_DESC_UNREAD_ALERT_STATUS_CL_CFG].desc_hdl;

            if(handle != GATT_INVALID_HDL)
            {
                ntf_cfg = co_btohs(ntf_cfg); // force Little Endian

                // Write control point
                status = prf_gatt_write(conidx, p_anpc_env->user_lid, ANPC_RD_WR_UNREAD_ALERT_STATUS_CFG,
                                        GATT_WRITE, handle, sizeof(uint16_t), (uint8_t*) &ntf_cfg);
            }
            else
            {
                status = PRF_ERR_INEXISTENT_HDL;
            }
        }
    }

    return (status);
}

uint16_t anpc_write_ctnl_pt(uint8_t conidx, uint8_t cmd_id, uint8_t cat_id)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    anpc_env_t* p_anpc_env = PRF_ENV_GET(ANPC, anpc);

    if ((cmd_id >= CMD_ID_NB) || ((cat_id >= CAT_ID_NB) && (cat_id != CAT_ID_ALL_SUPPORTED_CAT)))
    {
        status = PRF_ERR_INVALID_PARAM;
    }
    else  if(p_anpc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_anpc_env->p_env[conidx] != NULL) && !(p_anpc_env->p_env[conidx]->discover))
        {
            uint16_t handle = p_anpc_env->p_env[conidx]->ans.chars[ANPC_CHAR_ALERT_NTF_CTNL_PT].val_hdl;

            if(handle != GATT_INVALID_HDL)
            {
                uint8_t value[2];
                value[0] = cmd_id;
                value[1] = cat_id;

                // Write control point
                status = prf_gatt_write(conidx, p_anpc_env->user_lid, ANPC_CHAR_ALERT_NTF_CTNL_PT,
                                        GATT_WRITE, handle, 2, value);
            }
            else
            {
                status = PRF_ERR_INEXISTENT_HDL;
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
 * @brief  Message handler example
 *
 * @param[in] msgid Id of the message received (probably unused).
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int anpc_enable_req_handler(ke_msg_id_t const msgid, struct anpc_enable_req const *p_param,
                                     ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = anpc_enable(p_param->conidx, p_param->con_type, &(p_param->ans));

    // send an error if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct anpc_enable_rsp *p_rsp = KE_MSG_ALLOC(ANPC_ENABLE_RSP, src_id, dest_id, anpc_enable_rsp);
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
 * @brief Handles reception of the @ref ANPC_READ_CMD message.
 * @param[in] msgid Id of the message received.
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int anpc_read_cmd_handler(ke_msg_id_t const msgid, struct anpc_read_cmd *p_param,
                                   ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = anpc_read(p_param->conidx, p_param->read_code);

    // send an error if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct anpc_cmp_evt *p_cmp = KE_MSG_ALLOC(ANPC_CMP_EVT, src_id, dest_id, anpc_cmp_evt);
        if(p_cmp != NULL)
        {
            p_cmp->conidx    = p_param->conidx;
            p_cmp->operation = ANPC_READ_OP_CODE;
            p_cmp->status    = status;
            ke_msg_send(p_cmp);
        }
    }

    return (KE_MSG_CONSUMED);
}


/**
 ****************************************************************************************
 * @brief Handles reception of the @ref ANPC_WRITE_CMD message.
 * @param[in] msgid Id of the message received.
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int anpc_write_cmd_handler(ke_msg_id_t const msgid, struct anpc_write_cmd *p_param,
                                    ke_task_id_t const dest_id,  ke_task_id_t const src_id)
{
    // Status
    uint16_t status = GAP_ERR_NO_ERROR;

    switch(p_param->write_code)
    {
        case ANPC_WR_ALERT_NTF_CTNL_PT:
        {
            status = anpc_write_ctnl_pt(p_param->conidx, p_param->value.ctnl_pt.cmd_id, p_param->value.ctnl_pt.cat_id);
        } break;
        case ANPC_RD_WR_NEW_ALERT_CFG:
        {
            status = anpc_write_new_alert_ntf_cfg(p_param->conidx, p_param->value.new_alert_ntf_cfg);
        } break;
        case ANPC_RD_WR_UNREAD_ALERT_STATUS_CFG:
        {
            status = anpc_write_unread_alert_ntf_cfg(p_param->conidx, p_param->value.unread_alert_status_ntf_cfg);
        } break;
        default: { status = PRF_ERR_INVALID_PARAM; } break;
    }

    // send an error if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct anpc_cmp_evt *p_cmp = KE_MSG_ALLOC(ANPC_CMP_EVT, src_id, dest_id, anpc_cmp_evt);
        if(p_cmp != NULL)
        {
            p_cmp->conidx    = p_param->conidx;
            p_cmp->operation = ANPC_WRITE_OP_CODE;
            p_cmp->status    = status;
            ke_msg_send(p_cmp);
        }
    }

    return (KE_MSG_CONSUMED);
}



/// Default State handlers definition
KE_MSG_HANDLER_TAB(anpc)
{
    // Note: all messages must be sorted in ID ascending order

    {ANPC_ENABLE_REQ,               (ke_msg_func_t)anpc_enable_req_handler},
    {ANPC_READ_CMD,                 (ke_msg_func_t)anpc_read_cmd_handler},
    {ANPC_WRITE_CMD,                (ke_msg_func_t)anpc_write_cmd_handler},
};


/**
 ****************************************************************************************
 * @brief Completion client enable
 *
 * @param[in] conidx Connection index
 * @param[in] status Status of the procedure execution (@see enum hl_err)
 * @param[in] p_ans  Pointer to peer database description (Bond Data)
 ****************************************************************************************
 */
__STATIC void anpc_cb_enable_cmp(uint8_t conidx, uint16_t status, const anpc_ans_content_t* p_ans)
{
    // Send APP the details of the discovered attributes on ANPC
    struct anpc_enable_rsp *p_rsp = KE_MSG_ALLOC(ANPC_ENABLE_RSP, PRF_DST_TASK(ANPC), PRF_SRC_TASK(ANPC),
                                                 anpc_enable_rsp);
    if(p_rsp != NULL)
    {
        p_rsp->conidx = conidx;
        p_rsp->status = status;
        memcpy(&(p_rsp->ans), p_ans, sizeof(anpc_ans_content_t));
        ke_msg_send(p_rsp);
    }
}

/**
 ****************************************************************************************
 * @brief Completion of Read procedure
 *
 * @param[in] conidx    Connection index
 * @param[in] status    Status of the procedure execution (@see enum hl_err)
 * @param[in] read_code Read code (@see enum anpc_rd_wr_ntf_codes)
 ****************************************************************************************
 */
__STATIC void anpc_cb_read_cmp(uint8_t conidx, uint16_t status, uint8_t read_code)
{
    struct anpc_cmp_evt *p_cmp = KE_MSG_ALLOC(ANPC_CMP_EVT, PRF_DST_TASK(ANPC), PRF_SRC_TASK(ANPC), anpc_cmp_evt);
    if(p_cmp != NULL)
    {
        p_cmp->conidx    = conidx;
        p_cmp->operation = ANPC_READ_OP_CODE;
        p_cmp->status    = status;
        ke_msg_send(p_cmp);
    }
}

/**
 ****************************************************************************************
 * @brief Completion of Read procedure
 *
 * @param[in] conidx     Connection index
 * @param[in] status     Status of the procedure execution (@see enum hl_err)
 * @param[in] write_code Write code (@see enum anpc_rd_wr_ntf_codes)
 ****************************************************************************************
 */
__STATIC void anpc_cb_write_cmp(uint8_t conidx, uint16_t status, uint8_t write_code)
{
    struct anpc_cmp_evt *p_cmp = KE_MSG_ALLOC(ANPC_CMP_EVT, PRF_DST_TASK(ANPC), PRF_SRC_TASK(ANPC), anpc_cmp_evt);
    if(p_cmp != NULL)
    {
        p_cmp->conidx    = conidx;
        p_cmp->operation = ANPC_WRITE_OP_CODE;
        p_cmp->status    = status;
        ke_msg_send(p_cmp);
    }
}

/**
 ****************************************************************************************
 * @brief Receive list of supported categories
 *
 * @param[in] conidx        Connection index
 * @param[in] char_code     Characteristic code (@see enum anpc_rd_wr_ntf_codes)
 * @param[in] cat_id_mask_0 Category ID Bit Mask 0
 * @param[in] cat_id_mask_1 Category ID Bit Mask 1
 ****************************************************************************************
 */
__STATIC void anpc_cb_supp_cat(uint8_t conidx, uint8_t char_code, uint8_t cat_id_mask_0, uint8_t cat_id_mask_1)
{
    struct anpc_value_ind *p_ind = KE_MSG_ALLOC(ANPC_VALUE_IND, PRF_DST_TASK(ANPC), PRF_SRC_TASK(ANPC), anpc_value_ind);
    if(p_ind != NULL)
    {
        p_ind->conidx                             = conidx;
        p_ind->att_code                           = char_code;
        p_ind->value.supp_cat.cat_id_mask_0       = cat_id_mask_0;
        p_ind->value.supp_cat.cat_id_mask_1       = cat_id_mask_1;

        ke_msg_send(p_ind);
    }
}

/**
 ****************************************************************************************
 * @brief Receive Update of Unread alert
 *
 * @param[in] conidx      Connection index
 * @param[in] char_code   Characteristic code (@see enum anpc_rd_wr_ntf_codes)
 * @param[in] cat_id      Category ID
 * @param[in] nb_alert    Number of alerts
 ****************************************************************************************
 */
__STATIC void anpc_cb_new_alert_upd(uint8_t conidx, uint8_t cat_id, uint8_t nb_alert, uint8_t info_len, const uint8_t* p_info)
{
    struct anpc_value_ind *p_ind = KE_MSG_ALLOC_DYN(ANPC_VALUE_IND, PRF_DST_TASK(ANPC),
                                                PRF_SRC_TASK(ANPC), anpc_value_ind, info_len);
    if(p_ind != NULL)
    {
        p_ind->conidx                         = conidx;
        p_ind->att_code                       = ANPC_NTF_NEW_ALERT;
        p_ind->value.new_alert.cat_id         = cat_id;
        p_ind->value.new_alert.nb_new_alert   = nb_alert;
        p_ind->value.new_alert.info_str_len   = info_len;
        memcpy(p_ind->value.new_alert.str_info, p_info, info_len);

        ke_msg_send(p_ind);
    }


}

/**
 ****************************************************************************************
 * @brief Receive Update of new alert
 *
 * @param[in] conidx       Connection index
 * @param[in] cat_id       Category ID
 * @param[in] nb_alert     Number of alerts
 * @param[in] info_len     Text String Information length
 * @param[in] p_info       Text String Information
 ****************************************************************************************
 */
__STATIC void anpc_cb_unread_alert_upd(uint8_t conidx, uint8_t cat_id, uint8_t nb_alert)
{
    struct anpc_value_ind *p_ind = KE_MSG_ALLOC(ANPC_VALUE_IND, PRF_DST_TASK(ANPC), PRF_SRC_TASK(ANPC), anpc_value_ind);
    if(p_ind != NULL)
    {
        p_ind->conidx                             = conidx;
        p_ind->att_code                           = ANPC_NTF_UNREAD_ALERT;
        p_ind->value.unread_alert.cat_id          = cat_id;
        p_ind->value.unread_alert.nb_unread_alert = nb_alert;

        ke_msg_send(p_ind);
    }
}

/**
 ****************************************************************************************
 * @brief Receive value of Notification configuration
 *
 * @param[in] conidx      Connection index
 * @param[in] char_code   Characteristic code (@see enum anpc_rd_wr_ntf_codes)
 * @param[in] ntf_cfg     Notification configuration value
 ****************************************************************************************
 */
__STATIC void anpc_cb_ntf_cfg(uint8_t conidx, uint8_t char_code, uint16_t ntf_cfg)
{
    struct anpc_value_ind *p_ind = KE_MSG_ALLOC(ANPC_VALUE_IND, PRF_DST_TASK(ANPC), PRF_SRC_TASK(ANPC), anpc_value_ind);
    if(p_ind != NULL)
    {
        p_ind->conidx        = conidx;
        p_ind->att_code      = char_code;
        p_ind->value.ntf_cfg = ntf_cfg;

        ke_msg_send(p_ind);
    }
}

/// Default Message handle
__STATIC const anpc_cb_t anpc_msg_cb =
{
    .cb_enable_cmp       = anpc_cb_enable_cmp,
    .cb_read_cmp         = anpc_cb_read_cmp,
    .cb_write_cmp        = anpc_cb_write_cmp,
    .cb_supp_cat         = anpc_cb_supp_cat,
    .cb_new_alert_upd    = anpc_cb_new_alert_upd,
    .cb_unread_alert_upd = anpc_cb_unread_alert_upd,
    .cb_ntf_cfg          = anpc_cb_ntf_cfg,
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
__STATIC uint16_t anpc_init(prf_data_t* p_env, uint16_t* p_start_hdl, uint8_t sec_lvl, uint8_t user_prio,
                          const void* p_params, const anpc_cb_t* p_cb)
{
    uint8_t conidx;
    // DB Creation Status
    uint16_t status = GAP_ERR_NO_ERROR;
    uint8_t user_lid = GATT_INVALID_USER_LID;

    do
    {
        anpc_env_t* p_anpc_env;
        #if (BLE_HL_MSG_API)
        if(p_cb == NULL)
        {
            p_cb = &(anpc_msg_cb);
        }
        #endif // (BLE_HL_MSG_API)

        if(   (p_params == NULL) || (p_cb == NULL) || (p_cb->cb_enable_cmp == NULL) || (p_cb->cb_enable_cmp == NULL)
           || (p_cb->cb_read_cmp == NULL)  || (p_cb->cb_write_cmp == NULL)  || (p_cb->cb_supp_cat == NULL)
           || (p_cb->cb_new_alert_upd == NULL)  || (p_cb->cb_unread_alert_upd == NULL) || (p_cb->cb_ntf_cfg == NULL))
        {
            status = GAP_ERR_INVALID_PARAM;
            break;
        }

        // register ANPC user
        status = gatt_user_cli_register(L2CAP_LE_MTU_MIN, user_prio, &anpc_cb, &user_lid);
        if(status != GAP_ERR_NO_ERROR) break;

        //-------------------- allocate memory required for the profile  ---------------------
        p_anpc_env = (anpc_env_t*) ke_malloc(sizeof(anpc_env_t), KE_MEM_ATT_DB);

        if(p_anpc_env != NULL)
        {
            // allocate ANPC required environment variable
            p_env->p_env = (prf_hdr_t *) p_anpc_env;

            // initialize environment variable
            p_anpc_env->prf_env.p_cb    = p_cb;
            #if (BLE_HL_MSG_API)
            p_env->desc.msg_handler_tab = anpc_msg_handler_tab;
            p_env->desc.msg_cnt         = ARRAY_LEN(anpc_msg_handler_tab);
            #endif // (BLE_HL_MSG_API)

            p_anpc_env->user_lid = user_lid;
            for (conidx = 0; conidx < BLE_CONNECTION_MAX; conidx++)
            {
                p_anpc_env->p_env[conidx] = NULL;
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
__STATIC uint16_t anpc_destroy(prf_data_t *p_env, uint8_t reason)
{
    uint16_t status = GAP_ERR_NO_ERROR;
    anpc_env_t* p_anpc_env = (anpc_env_t*) p_env->p_env;

    if(reason != PRF_DESTROY_RESET)
    {
        status = gatt_user_unregister(p_anpc_env->user_lid);
    }

    if(status == GAP_ERR_NO_ERROR)
    {
        uint8_t idx;

        // cleanup environment variable for each task instances
        for (idx = 0; idx < BLE_CONNECTION_MAX; idx++)
        {
            if (p_anpc_env->p_env[idx] != NULL)
            {
                ke_free(p_anpc_env->p_env[idx]);
            }
        }

        // free profile environment variables
        p_env->p_env = NULL;
        ke_free(p_anpc_env);
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
__STATIC void anpc_con_create(prf_data_t *p_env, uint8_t conidx, const gap_con_param_t* p_con_param)
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
__STATIC void anpc_con_cleanup(prf_data_t *p_env, uint8_t conidx, uint16_t reason)
{
    anpc_env_t* p_anpc_env = (anpc_env_t*) p_env->p_env;

    // clean-up environment variable allocated for task instance
    if (p_anpc_env->p_env[conidx] != NULL)
    {
        ke_free(p_anpc_env->p_env[conidx]);
        p_anpc_env->p_env[conidx] = NULL;
    }
}

/// ANPC Task interface required by profile manager
const prf_task_cbs_t anpc_itf =
{
    .cb_init          = (prf_init_cb) anpc_init,
    .cb_destroy       = anpc_destroy,
    .cb_con_create    = anpc_con_create,
    .cb_con_cleanup   = anpc_con_cleanup,
    .cb_con_upd       = NULL,
};

/**
 ****************************************************************************************
 * @brief Retrieve client profile interface
 *
 * @return Client profile interface
 ****************************************************************************************
 */
const prf_task_cbs_t* anpc_prf_itf_get(void)
{
    return &anpc_itf;
}


#endif //(BLE_AN_CLIENT)

/// @} ANPC

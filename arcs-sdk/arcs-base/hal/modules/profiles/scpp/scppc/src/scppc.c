/**
 ****************************************************************************************
 *
 * @file scppc.c
 *
 * @brief Scan Parameters Profile Client implementation.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */


/**
 ****************************************************************************************
 * @addtogroup SCPPC
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "btip_config.h"

#if (BLE_SP_CLIENT)

#include "scppc.h"
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

/// Maximum number of Client task instances

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */


/// Environment variable for each Connections
typedef struct scppc_cnx_env
{
    /// Peer database discovered handle mapping
    scps_content_t      scps;
    /// counter used to check service uniqueness
    uint8_t             nb_svc;
    /// Client is in discovering state
    bool                discover;
} scppc_cnx_env_t;

/// Client environment variable
typedef struct scppc_env
{
    /// profile environment
    prf_hdr_t            prf_env;
    /// Environment variable pointer for each connections
    scppc_cnx_env_t*     p_env[BLE_CONNECTION_MAX];
    /// GATT User local identifier
    uint8_t              user_lid;
} scppc_env_t;


/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */

/// State machine used to retrieve Scan Parameters Service characteristics information
const prf_char_def_t scppc_scps_char[SCPPC_CHAR_MAX] =
{
    // Scan Interval Window
    [SCPPC_CHAR_SCAN_INTV_WD]      = { GATT_CHAR_SCAN_INTV_WD, ATT_REQ(PRES, MAND), PROP(WC) },
    // Scan Refresh
    [SCPPC_CHAR_SCAN_REFRESH]      = { GATT_CHAR_SCAN_REFRESH, ATT_REQ(PRES, OPT), PROP(N)   },
};

/// State machine used to retrieve Scan Parameters Service characteristic description information
const prf_desc_def_t scppc_scps_char_desc[SCPPC_DESC_MAX] =
{
    // Boot Keyboard Input Report Client Config
    [SCPPC_DESC_SCAN_REFRESH_CFG]  = { GATT_DESC_CLIENT_CHAR_CFG, ATT_REQ(PRES, MAND), SCPPC_CHAR_SCAN_REFRESH },
};

/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Send discovery results to application.
 *
 * @param p_scppc_env    Client Role task environment
 * @param conidx        Connection index
 * @param status        Response status code
 *****************************************************************************************
 */
__STATIC void scppc_enable_cmp(scppc_env_t* p_scppc_env, uint8_t conidx, uint16_t status)
{
    const scppc_cb_t* p_cb = (const scppc_cb_t*) p_scppc_env->prf_env.p_cb;

    if(p_scppc_env != NULL)
    {
        scppc_cnx_env_t* p_con_env = p_scppc_env->p_env[conidx];
        p_cb->cb_enable_cmp(conidx, status, &(p_con_env->scps));

        if (status != GAP_ERR_NO_ERROR)
        {
            // clean-up environment variable allocated for task instance
            ke_free(p_con_env);
            p_scppc_env->p_env[conidx] = NULL;
        }
        else
        {
             p_con_env->discover = false;

             // Register profile handle to catch gatt indications
             gatt_cli_event_register(conidx, p_scppc_env->user_lid, p_con_env->scps.svc.shdl,
                                     p_con_env->scps.svc.ehdl);
        }
    }
}

/**
 ****************************************************************************************
 * @brief Perform Value read procedure.
 *
 * @param[in] conidx        Connection index
 * @param[in] val_id        Value Identifier
 ****************************************************************************************
 */
__STATIC uint16_t scppc_read_val(uint8_t conidx, uint16_t val_id)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    scppc_env_t* p_scppc_env = PRF_ENV_GET(SCPPC, scppc);

    if(p_scppc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_scppc_env->p_env[conidx] != NULL) && (!p_scppc_env->p_env[conidx]->discover))
        {
            scppc_cnx_env_t* p_con_env = p_scppc_env->p_env[conidx];
            uint16_t hdl;
            scps_content_t* p_scps = &(p_con_env->scps);

            switch(val_id)
            {
                case SCPPC_DESC_SCAN_REFRESH_CFG: { hdl = p_scps->descs[SCPPC_DESC_SCAN_REFRESH_CFG].desc_hdl; } break;
                default:                          { hdl = GATT_INVALID_HDL;                                    } break;
            }

            if(hdl == GATT_INVALID_HDL)
            {
                status = PRF_ERR_INEXISTENT_HDL;
            }
            else
            {
                // perform read request
                status = gatt_cli_read(conidx, p_scppc_env->user_lid, val_id, hdl, 0, 0);
            }
        }
    }

    return (status);
}

/**
 ****************************************************************************************
 * @brief Send read result to application,.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the request
 * @param[in] val_id        Value Identifier (@see enum scppc_val_id)
 * @param[in] length        Length of data value)
 * @param[in] p_data        Pointer of data value
 ****************************************************************************************
 */
__STATIC void scppc_read_val_cmp(uint8_t conidx, uint16_t status, uint8_t val_id, uint16_t length, const uint8_t* p_data)
{
    scppc_env_t* p_scppc_env = PRF_ENV_GET(SCPPC, scppc);

    if(p_scppc_env != NULL)
    {
        const scppc_cb_t* p_cb = (const scppc_cb_t*) p_scppc_env->prf_env.p_cb;
        switch (val_id)
        {
            // Read Client Characteristic Configuration Descriptor value
            case (SCPPC_DESC_SCAN_REFRESH_CFG):
            {
                uint16_t cfg_val = 0;

                if(status == GAP_ERR_NO_ERROR)
                {
                    cfg_val = co_btohs(co_read16p(p_data));
                }

                p_cb->cb_read_cfg_cmp(conidx, status, cfg_val);
            } break;

            default:
            {
                ASSERT_ERR(0);
            } break;
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
__STATIC void scppc_svc_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t hdl, uint8_t disc_info,
                          uint8_t nb_att, const gatt_svc_att_t* p_atts)
{
    // Get the address of the environment
    scppc_env_t* p_scppc_env = PRF_ENV_GET(SCPPC, scppc);

    if(p_scppc_env != NULL)
    {
        scppc_cnx_env_t* p_con_env = p_scppc_env->p_env[conidx];

        ASSERT_INFO(p_con_env != NULL, conidx, user_lid);

        if (p_con_env->nb_svc == 0)
        {
            //Even if we get multiple responses we only store 1 range
            if((disc_info == GATT_SVC_CMPLT) || (disc_info == GATT_SVC_START))
            {
                p_con_env->scps.svc.shdl = hdl;
            }

            if((disc_info == GATT_SVC_CMPLT) || (disc_info == GATT_SVC_END))
            {
                p_con_env->scps.svc.ehdl = hdl + nb_att -1;
            }

            // Retrieve characteristics
            prf_extract_svc_info(hdl, nb_att, p_atts,
                                 SCPPC_CHAR_MAX, &scppc_scps_char[0],      &(p_con_env->scps.chars[0]),
                                 SCPPC_DESC_MAX, &scppc_scps_char_desc[0], &(p_con_env->scps.descs[0]));
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
__STATIC void scppc_discover_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    // Get the address of the environment
    scppc_env_t* p_scppc_env = PRF_ENV_GET(SCPPC, scppc);

    if(p_scppc_env != NULL)
    {
        scppc_cnx_env_t* p_con_env = p_scppc_env->p_env[conidx];

        if (p_con_env->nb_svc ==  1)
        {
            status = prf_check_svc_validity(SCPPC_CHAR_MAX, p_con_env->scps.chars, scppc_scps_char,
                                            SCPPC_DESC_MAX, p_con_env->scps.descs, scppc_scps_char_desc);
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

        scppc_enable_cmp(p_scppc_env, conidx, status);
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
__STATIC void scppc_att_val_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t hdl, uint16_t offset,
                              co_buf_t* p_data)
{
    scppc_read_val_cmp(conidx, GAP_ERR_NO_ERROR, (uint8_t) dummy, co_buf_data_len(p_data), co_buf_data(p_data));
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
__STATIC void scppc_read_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    if(status != GAP_ERR_NO_ERROR)
    {
        scppc_read_val_cmp(conidx, status, (uint8_t) dummy, 0, NULL);
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
__STATIC void scppc_write_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    scppc_env_t* p_scppc_env = PRF_ENV_GET(SCPPC, scppc);

    if(p_scppc_env != NULL)
    {
        const scppc_cb_t* p_cb = (const scppc_cb_t*) p_scppc_env->prf_env.p_cb;
        switch (dummy)
        {
            case SCPPC_SCAN_INTV_WD_WR_CMD_OP_CODE:      { p_cb->cb_scan_intv_wd_upd_cmp(conidx, status); } break;
            case SCPPC_SCAN_REFRESH_NTF_CFG_CMD_OP_CODE: { p_cb->cb_write_cfg_cmp(conidx, status);        } break;
            default:                                     { ASSERT_ERR(0);                                 } break;
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
__STATIC void scppc_att_val_evt_cb(uint8_t conidx, uint8_t user_lid, uint16_t token, uint8_t evt_type, bool complete,
                                   uint16_t hdl, co_buf_t* p_data)
{
    scppc_env_t* p_scppc_env = PRF_ENV_GET(SCPPC, scppc);

    if(p_scppc_env != NULL)
    {
        scppc_cnx_env_t* p_con_env = p_scppc_env->p_env[conidx];
        scps_content_t* p_scps = &(p_con_env->scps);

        if (hdl == p_scps->chars[SCPPC_CHAR_SCAN_REFRESH].val_hdl)
        {
            const scppc_cb_t* p_cb = (const scppc_cb_t*) p_scppc_env->prf_env.p_cb;
            p_cb->cb_scan_refresh(conidx);
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
__STATIC void scppc_svc_changed_cb(uint8_t conidx, uint8_t user_lid, bool out_of_sync, uint16_t start_hdl, uint16_t end_hdl)
{
    // Do Nothing
}

/// Client callback hander
__STATIC const gatt_cli_cb_t scppc_cb =
{
    .cb_discover_cmp    = scppc_discover_cmp_cb,
    .cb_read_cmp        = scppc_read_cmp_cb,
    .cb_write_cmp       = scppc_write_cmp_cb,
    .cb_att_val_get     = NULL,
    .cb_svc             = scppc_svc_cb,
    .cb_svc_info        = NULL,
    .cb_inc_svc         = NULL,
    .cb_char            = NULL,
    .cb_desc            = NULL,
    .cb_att_val         = scppc_att_val_cb,
    .cb_att_val_evt     = scppc_att_val_evt_cb,
    .cb_svc_changed     = scppc_svc_changed_cb,
};

/*
 * PROFILE NATIVE API
 ****************************************************************************************
 */

uint16_t scppc_enable(uint8_t conidx, uint8_t con_type, const scps_content_t* p_scps)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    scppc_env_t* p_scppc_env = PRF_ENV_GET(SCPPC, scppc);

    if(p_scppc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_scppc_env->p_env[conidx] == NULL))
        {
            // allocate environment variable for task instance
            p_scppc_env->p_env[conidx] = (struct scppc_cnx_env *) ke_malloc(sizeof(struct scppc_cnx_env), KE_MEM_ATT_DB);

            if(p_scppc_env->p_env[conidx] != NULL)
            {
                // Config connection, start discovering
                if (con_type == PRF_CON_DISCOVERY)
                {
                    uint16_t gatt_svc_uuid = GATT_SVC_SCAN_PARAMETERS;
                    memset(p_scppc_env->p_env[conidx], 0, sizeof(struct scppc_cnx_env));

                    // start discovery
                    status = gatt_cli_discover_svc(conidx, p_scppc_env->user_lid, 0, GATT_DISCOVER_SVC_PRIMARY_BY_UUID, true,
                                                   GATT_MIN_HDL, GATT_MAX_HDL, GATT_UUID_16, (uint8_t*) &gatt_svc_uuid);

                    // Go to DISCOVERING state
                    p_scppc_env->p_env[conidx]->discover = true;
                }
                // normal connection, get saved att details
                else
                {
                    memcpy(&(p_scppc_env->p_env[conidx]->scps), p_scps, sizeof(scps_content_t));
                    status = GAP_ERR_NO_ERROR;

                    // send APP confirmation that can start normal connection to TH
                    scppc_enable_cmp(p_scppc_env, conidx, GAP_ERR_NO_ERROR);
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

uint16_t scppc_read_cfg(uint8_t conidx)
{
    return (scppc_read_val(conidx, SCPPC_DESC_SCAN_REFRESH_CFG));
}

uint16_t scppc_write_cfg(uint8_t conidx, uint16_t ntf_cfg)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    scppc_env_t* p_scppc_env = PRF_ENV_GET(SCPPC, scppc);

    if(p_scppc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_scppc_env->p_env[conidx] != NULL) && (!p_scppc_env->p_env[conidx]->discover))
        {
            scppc_cnx_env_t* p_con_env = p_scppc_env->p_env[conidx];
            uint16_t hdl;
            scps_content_t* p_scps = &(p_con_env->scps);

            hdl = p_scps->descs[SCPPC_DESC_SCAN_REFRESH_CFG].desc_hdl;

            if(hdl == GATT_INVALID_HDL)
            {
                status = PRF_ERR_INEXISTENT_HDL;
            }
            else if((ntf_cfg != PRF_CLI_STOP_NTFIND) && (ntf_cfg != PRF_CLI_START_NTF))
            {
                status = PRF_ERR_INVALID_PARAM;
            }
            else
            {
                // Force endianess
                ntf_cfg = co_htobs(ntf_cfg);
                status = prf_gatt_write(conidx, p_scppc_env->user_lid, SCPPC_SCAN_REFRESH_NTF_CFG_CMD_OP_CODE,
                                        GATT_WRITE, hdl, sizeof(uint16_t), (uint8_t *)&ntf_cfg);
            }
        }
    }

    return (status);
}

uint16_t scppc_scan_intv_wd_upd(uint8_t conidx, const scpp_scan_intv_wd_t* p_scan_intv_wd)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    scppc_env_t* p_scppc_env = PRF_ENV_GET(SCPPC, scppc);

    if(p_scan_intv_wd == NULL)
    {
        status = PRF_ERR_INVALID_PARAM;
    }
    else if(p_scppc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_scppc_env->p_env[conidx] != NULL) && (!p_scppc_env->p_env[conidx]->discover))
        {
            scppc_cnx_env_t* p_con_env = p_scppc_env->p_env[conidx];
            scps_content_t* p_scps = &(p_con_env->scps);
            uint16_t hdl = p_scps->chars[SCPPC_CHAR_SCAN_INTV_WD].val_hdl;

            if(hdl == GATT_INVALID_HDL)
            {
                status = PRF_ERR_INEXISTENT_HDL;
            }
            else
            {
                co_buf_t* p_buf = NULL;

                // allocate buffer for event transmission
                if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, 0, sizeof(scpp_scan_intv_wd_t) + GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
                {
                    // Pack data
                    co_write16p(co_buf_tail(p_buf), co_htobs(p_scan_intv_wd->le_scan_intv));
                    co_buf_tail_reserve(p_buf, 2);
                    co_write16p(co_buf_tail(p_buf), co_htobs(p_scan_intv_wd->le_scan_window));
                    co_buf_tail_reserve(p_buf, 2);

                    status = gatt_cli_write(conidx, p_scppc_env->user_lid, SCPPC_SCAN_INTV_WD_WR_CMD_OP_CODE,
                                            GATT_WRITE_NO_RESP, hdl, 0, p_buf);
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
 * @brief Send a SCPPC_CMP_EVT message to the task which enabled the profile
 * @param[in] conidx Connection index
 * @param[in] operation Operation
 * @param[in] status Satus
 ****************************************************************************************
 */
__STATIC void scppc_send_cmp_evt(uint8_t conidx, uint8_t operation, uint16_t status)
{
    struct scppc_cmp_evt *p_evt;

    // Send the message
    p_evt = KE_MSG_ALLOC(SCPPC_CMP_EVT, PRF_DST_TASK(SCPPC), PRF_SRC_TASK(SCPPC), scppc_cmp_evt);
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
 * @brief Handles reception of the @ref SCPPC_ENABLE_REQ message.
 * The handler enables the Scan Parameters Profile Client Role.
 * @param[in] msgid Id of the message received (probably unused).
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int scppc_enable_req_handler(ke_msg_id_t const msgid, struct scppc_enable_req const *p_param,
                                     ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = scppc_enable(p_param->conidx, p_param->con_type, &(p_param->scps));

    // send an error if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct scppc_enable_rsp *p_rsp = KE_MSG_ALLOC(SCPPC_ENABLE_RSP, src_id, dest_id, scppc_enable_rsp);
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
 * @brief Handles reception of the @ref SCPPC_SCAN_INTV_WD_WR_CMD message.
 * @param[in] msgid Id of the message received (probably unused).
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int scppc_scan_intv_wd_wr_cmd_handler(ke_msg_id_t const msgid,
                                               struct scppc_scan_intv_wd_wr_cmd const *p_param,
                                               ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = scppc_scan_intv_wd_upd(p_param->conidx, &(p_param->scan_intv_wd));

    // send error response if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct scppc_cmp_evt *p_evt = KE_MSG_ALLOC(SCPPC_CMP_EVT, src_id, dest_id, scppc_cmp_evt);

        if(p_evt != NULL)
        {
            p_evt->conidx      = p_param->conidx;
            p_evt->operation   = SCPPC_SCAN_INTV_WD_WR_CMD_OP_CODE;
            p_evt->status      = status;

            ke_msg_send(p_evt);
        }
    }

    return (KE_MSG_CONSUMED);
}


/**
 ****************************************************************************************
 * @brief Handles reception of the @ref SCPPC_SCAN_REFRESH_NTF_CFG_RD_CMD message.
 * @param[in] msgid Id of the message received (probably unused).
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int scppc_scan_refresh_ntf_cfg_rd_cmd_handler(ke_msg_id_t const msgid, struct
                                                       scppc_scan_refresh_ntf_cfg_rd_cmd const *p_param,
                                                       ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = scppc_read_cfg(p_param->conidx);

    // send error response if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct scppc_cmp_evt *p_evt = KE_MSG_ALLOC(SCPPC_CMP_EVT, src_id, dest_id, scppc_cmp_evt);

        if(p_evt != NULL)
        {
            p_evt->conidx      = p_param->conidx;
            p_evt->operation   = SCPPC_SCAN_REFRESH_NTF_CFG_RD_CMD_OP_CODE;
            p_evt->status      = status;

            ke_msg_send(p_evt);
        }
    }

    return (KE_MSG_CONSUMED);
}


/**
 ****************************************************************************************
 * @brief Handles reception of the @ref SCPPC_SCAN_REFRESH_NTF_CFG_CMD message.
 * It allows configuration of the peer ntf/stop characteristic for a specified characteristic.
 * Will return an error code if that cfg char does not exist.
 * @param[in] msgid Id of the message received (probably unused).
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int scppc_scan_refresh_ntf_cfg_cmd_handler(ke_msg_id_t const msgid, struct scppc_scan_refresh_ntf_cfg_cmd const *p_param,
                                                    ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = scppc_write_cfg(p_param->conidx, p_param->ntf_cfg);

    // send error response if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct scppc_cmp_evt *p_evt = KE_MSG_ALLOC(SCPPC_CMP_EVT, src_id, dest_id, scppc_cmp_evt);

        if(p_evt != NULL)
        {
            p_evt->conidx      = p_param->conidx;
            p_evt->operation   = SCPPC_SCAN_REFRESH_NTF_CFG_CMD_OP_CODE;
            p_evt->status      = status;

            ke_msg_send(p_evt);
        }
    }

    return (KE_MSG_CONSUMED);
}

/// Default State handlers definition
KE_MSG_HANDLER_TAB(scppc)
{
    // Note: all messages must be sorted in ID ascending order

    { SCPPC_ENABLE_REQ,                      (ke_msg_func_t)scppc_enable_req_handler                  },
    { SCPPC_SCAN_INTV_WD_WR_CMD,             (ke_msg_func_t)scppc_scan_intv_wd_wr_cmd_handler         },
    { SCPPC_SCAN_REFRESH_NTF_CFG_RD_CMD,     (ke_msg_func_t)scppc_scan_refresh_ntf_cfg_rd_cmd_handler },
    { SCPPC_SCAN_REFRESH_NTF_CFG_CMD,        (ke_msg_func_t)scppc_scan_refresh_ntf_cfg_cmd_handler    },
};

/**
 ****************************************************************************************
 * @brief Completion of enable procedure
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 * @param[in] p_scps        Pointer to peer database description bond data
 ****************************************************************************************
 */
__STATIC void scppc_cb_enable_cmp(uint8_t conidx, uint16_t status, const scps_content_t* p_scps)
{
    // Send APP the details of the discovered attributes on SCPPC
    struct scppc_enable_rsp *p_rsp = KE_MSG_ALLOC(SCPPC_ENABLE_RSP, PRF_DST_TASK(SCPPC), PRF_SRC_TASK(SCPPC),
                                                 scppc_enable_rsp);
    if(p_rsp != NULL)
    {
        p_rsp->conidx = conidx;
        p_rsp->status = status;
        memcpy(&(p_rsp->scps), p_scps, sizeof(scps_content_t));
        ke_msg_send(p_rsp);
    }
}

/**
 ****************************************************************************************
 * @brief Completion of read Characteristic Configuration procedure.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 * @param[in] ntf_cfg       Notification Configuration Value
 ****************************************************************************************
 */
__STATIC void scppc_cb_read_cfg_cmp(uint8_t conidx, uint16_t status, uint16_t ntf_cfg)
{
    if(status == GAP_ERR_NO_ERROR)
    {
        // Send APP the details of the discovered attributes on SCPPC
        struct scppc_scan_refresh_ntf_cfg_rd_ind *p_ind = KE_MSG_ALLOC(SCPPC_SCAN_REFRESH_NTF_CFG_RD_IND,
                                                                       PRF_DST_TASK(SCPPC),
                                                                       PRF_SRC_TASK(SCPPC),
                                                                       scppc_scan_refresh_ntf_cfg_rd_ind);
        if(p_ind != NULL)
        {
            p_ind->conidx = conidx;
            p_ind->ntf_cfg = ntf_cfg;
            ke_msg_send(p_ind);
        }
    }

    // send operation complete
    scppc_send_cmp_evt(conidx, SCPPC_SCAN_REFRESH_NTF_CFG_RD_CMD_OP_CODE, status);
}

/**
 ****************************************************************************************
 * @brief Completion of write Characteristic Configuration procedure.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 *
 ****************************************************************************************
 */
__STATIC void scppc_cb_write_cfg_cmp(uint8_t conidx, uint16_t status)
{
    // send operation complete
    scppc_send_cmp_evt(conidx, SCPPC_SCAN_REFRESH_NTF_CFG_CMD_OP_CODE, status);
}

/**
 ****************************************************************************************
 * @brief Completion of scan interval and window update procedure.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 ****************************************************************************************
 */
__STATIC void scppc_cb_scan_intv_wd_upd_cmp(uint8_t conidx, uint16_t status)
{
    // send operation complete
    scppc_send_cmp_evt(conidx, SCPPC_SCAN_INTV_WD_WR_CMD_OP_CODE, status);
}

/**
 ****************************************************************************************
 * @brief Function called when peer device ask for a scan parameter refresh
 *
 * @param[in] conidx        Connection index
 ****************************************************************************************
 */
__STATIC void scppc_cb_scan_refresh(uint8_t conidx)
{
    // Inform that peer device requests a Scan parameter refresh
    struct scppc_scan_refresh_ind *p_ind = KE_MSG_ALLOC(SCPPC_SCAN_REFRESH_IND, PRF_DST_TASK(SCPPC), PRF_SRC_TASK(SCPPC),
                                                        scppc_scan_refresh_ind);
    if(p_ind != NULL)
    {
        p_ind->conidx = conidx;
        ke_msg_send(p_ind);
    }
}

/// Default Message handle
__STATIC const scppc_cb_t scppc_msg_cb =
{
    .cb_enable_cmp           = scppc_cb_enable_cmp,
    .cb_read_cfg_cmp         = scppc_cb_read_cfg_cmp,
    .cb_write_cfg_cmp        = scppc_cb_write_cfg_cmp,
    .cb_scan_intv_wd_upd_cmp = scppc_cb_scan_intv_wd_upd_cmp,
    .cb_scan_refresh         = scppc_cb_scan_refresh,
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
 * @param[out]    p_env      Collector or Service allocated environment data.
 * @param[in|out] p_start_hdl  Service start handle (0 - dynamically allocated), only applies for services.
 * @param[in]     sec_lvl    Security level (@see enum gatt_svc_info_bf)
 * @param[in]     p_param    Configuration parameters of profile collector or service (32 bits aligned)
 * @param[in]     p_cb       Callback structure that handles event from profile
 *
 * @return status code to know if profile initialization succeed or not.
 ****************************************************************************************
 */
__STATIC uint16_t scppc_init(prf_data_t* p_env, uint16_t* p_start_hdl, uint8_t sec_lvl, uint8_t user_prio,
                          const void* p_params, const scppc_cb_t* p_cb)
{
    uint8_t conidx;
    // DB Creation Status
    uint16_t status = GAP_ERR_NO_ERROR;
    uint8_t user_lid = GATT_INVALID_USER_LID;

    do
    {
        scppc_env_t* p_scppc_env;

        #if (BLE_HL_MSG_API)
        if(p_cb == NULL)
        {
            p_cb = &(scppc_msg_cb);
        }
        #endif // (BLE_HL_MSG_API)

        if(   (p_params == NULL) || (p_cb == NULL) || (p_cb->cb_enable_cmp == NULL) || (p_cb->cb_read_cfg_cmp == NULL)
           || (p_cb->cb_write_cfg_cmp == NULL)  || (p_cb->cb_scan_intv_wd_upd_cmp == NULL)
           || (p_cb->cb_scan_refresh == NULL))
        {
            status = GAP_ERR_INVALID_PARAM;
            break;
        }

        // register SCPPC user
        status = gatt_user_cli_register(L2CAP_LE_MTU_MIN, user_prio, &scppc_cb, &user_lid);
        if(status != GAP_ERR_NO_ERROR) break;

        //-------------------- allocate memory required for the profile  ---------------------
        p_scppc_env = (scppc_env_t*) ke_malloc(sizeof(scppc_env_t), KE_MEM_ATT_DB);

        if(p_scppc_env != NULL)
        {
            // allocate SCPPC required environment variable
            p_env->p_env = (prf_hdr_t *) p_scppc_env;

            // initialize environment variable
            p_scppc_env->prf_env.p_cb    = p_cb;
            #if (BLE_HL_MSG_API)
            p_env->desc.msg_handler_tab = scppc_msg_handler_tab;
            p_env->desc.msg_cnt         = ARRAY_LEN(scppc_msg_handler_tab);
            #endif // (BLE_HL_MSG_API)

            p_scppc_env->user_lid = user_lid;
            for (conidx = 0; conidx < BLE_CONNECTION_MAX; conidx++)
            {
                p_scppc_env->p_env[conidx] = NULL;
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
__STATIC uint16_t scppc_destroy(prf_data_t *p_env, uint8_t reason)
{
    uint16_t status = GAP_ERR_NO_ERROR;
    scppc_env_t* p_scppc_env = (scppc_env_t*) p_env->p_env;

    if(reason != PRF_DESTROY_RESET)
    {
        status = gatt_user_unregister(p_scppc_env->user_lid);
    }

    if(status == GAP_ERR_NO_ERROR)
    {
        uint8_t idx;

        // cleanup environment variable for each task instances
        for (idx = 0; idx < BLE_CONNECTION_MAX; idx++)
        {
            if (p_scppc_env->p_env[idx] != NULL)
            {
                ke_free(p_scppc_env->p_env[idx]);
            }
        }

        // free profile environment variables
        p_env->p_env = NULL;
        ke_free(p_scppc_env);
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
__STATIC void scppc_con_create(prf_data_t *p_env, uint8_t conidx, const gap_con_param_t* p_con_param)
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
__STATIC void scppc_con_cleanup(prf_data_t *p_env, uint8_t conidx, uint16_t reason)
{
    scppc_env_t* p_scppc_env = (scppc_env_t*) p_env->p_env;

    // clean-up environment variable allocated for task instance
    if (p_scppc_env->p_env[conidx] != NULL)
    {
        ke_free(p_scppc_env->p_env[conidx]);
        p_scppc_env->p_env[conidx] = NULL;
    }
}

/// SCPPC Task interface required by profile manager
const prf_task_cbs_t scppc_itf =
{
    .cb_init          = (prf_init_cb) scppc_init,
    .cb_destroy       = scppc_destroy,
    .cb_con_create    = scppc_con_create,
    .cb_con_cleanup   = scppc_con_cleanup,
    .cb_con_upd       = NULL,
};

/**
 ****************************************************************************************
 * @brief Retrieve client profile interface
 *
 * @return Client profile interface
 ****************************************************************************************
 */
const prf_task_cbs_t* scppc_prf_itf_get(void)
{
    return &scppc_itf;
}

#endif /* (BLE_SP_CLIENT) */

/// @} SCPPC

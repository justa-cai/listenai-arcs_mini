/**
 ****************************************************************************************
 *
 * @file paspc.c
 *
 * @brief Phone Alert Status Profile Client implementation.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 ****************************************************************************************
 */


/**
 ****************************************************************************************
 * @addtogroup PASPC
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "btip_config.h"

#if (BLE_PAS_CLIENT)

#include "paspc.h"
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

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */


/// Environment variable for each Connections
typedef struct paspc_cnx_env
{
    /// Peer database discovered handle mapping
    paspc_pass_content_t  pass;
    /// counter used to check service uniqueness
    uint8_t               nb_svc;
    /// Client is in discovering state
    bool                  discover;
} paspc_cnx_env_t;

/// Client environment variable
typedef struct paspc_env
{
    /// profile environment
    prf_hdr_t            prf_env;
    /// Environment variable pointer for each connections
    paspc_cnx_env_t*     p_env[BLE_CONNECTION_MAX];
    /// GATT User local identifier
    uint8_t              user_lid;
} paspc_env_t;


/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */

/// State machine used to retrieve Phone Alert Status service characteristics information
const prf_char_def_t paspc_pass_char[PASPC_CHAR_MAX] =
{
    // Alert Status
    [PASPC_CHAR_ALERT_STATUS]   = { GATT_CHAR_ALERT_STATUS,      ATT_REQ(PRES, MAND), PROP(RD) | PROP(N) },
    // Ringer Setting
    [PASPC_CHAR_RINGER_SETTING] = { GATT_CHAR_RINGER_SETTING,    ATT_REQ(PRES, MAND), PROP(RD) | PROP(N) },
    // Ringer Control Point
    [PASPC_CHAR_RINGER_CTNL_PT] = { GATT_CHAR_RINGER_CNTL_POINT, ATT_REQ(PRES, MAND), PROP(WC)           },
};

/// State machine used to retrieve Phone Alert Status service characteristic descriptor information
const prf_desc_def_t paspc_pass_char_desc[PASPC_DESC_MAX] =
{
    // Alert Status Client Characteristic Configuration
    [PASPC_DESC_ALERT_STATUS_CL_CFG]   = { GATT_DESC_CLIENT_CHAR_CFG, ATT_REQ(PRES, MAND), PASPC_CHAR_ALERT_STATUS   },
    // Ringer Setting Client Characteristic Configuration
    [PASPC_DESC_RINGER_SETTING_CL_CFG] = { GATT_DESC_CLIENT_CHAR_CFG, ATT_REQ(PRES, MAND), PASPC_CHAR_RINGER_SETTING },
};

/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Send discovery results to application.
 *
 * @param p_paspc_env    Client Role task environment
 * @param conidx        Connection index
 * @param status        Response status code
 *****************************************************************************************
 */
__STATIC void paspc_enable_cmp(paspc_env_t* p_paspc_env, uint8_t conidx, uint16_t status)
{
    const paspc_cb_t* p_cb = (const paspc_cb_t*) p_paspc_env->prf_env.p_cb;

    if(p_paspc_env != NULL)
    {
        paspc_cnx_env_t* p_con_env = p_paspc_env->p_env[conidx];
        p_cb->cb_enable_cmp(conidx, status, &(p_con_env->pass));

        if (status != GAP_ERR_NO_ERROR)
        {
            // clean-up environment variable allocated for task instance
            ke_free(p_con_env);
            p_paspc_env->p_env[conidx] = NULL;
        }
        else
        {
             p_con_env->discover = false;

             // Register profile handle to catch gatt indications
             gatt_cli_event_register(conidx, p_paspc_env->user_lid, p_con_env->pass.svc.shdl,
                                     p_con_env->pass.svc.ehdl);
        }
    }
}

/**
 ****************************************************************************************
 * @brief Send read result to application,.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the request
 * @param[in] val_id        Value Identifier (@see enum paspc_val_id)
 * @param[in] length        Length of data value)
 * @param[in] p_data        Pointer of data value
 ****************************************************************************************
 */
__STATIC void paspc_read_val_cmp(uint8_t conidx, uint16_t status, uint8_t val_id, uint16_t length, const uint8_t* p_data)
{
    paspc_env_t* p_paspc_env = PRF_ENV_GET(PASPC, paspc);

    if(p_paspc_env != NULL)
    {
        const paspc_cb_t* p_cb = (const paspc_cb_t*) p_paspc_env->prf_env.p_cb;
        switch (val_id)
        {
            // Read PAS Alert Status
            case (PASPC_RD_ALERT_STATUS):
            {
                uint8_t alert_status = 0;
                if(status == GAP_ERR_NO_ERROR)
                {
                    alert_status = p_data[0];
                }
                p_cb->cb_read_alert_status_cmp(conidx, status, alert_status);
            } break;

            // Read PAS Ringer Setting
            case (PASPC_RD_RINGER_SETTING):
            {
                uint8_t ringer_setting = 0;

                if(status == GAP_ERR_NO_ERROR)
                {
                    ringer_setting = p_data[0];
                }

                p_cb->cb_read_ringer_setting_cmp(conidx, status, ringer_setting);

            } break;

            // Read Client Characteristic Configuration Descriptor value
            case (PASPC_RD_WR_ALERT_STATUS_CFG):
            case (PASPC_RD_WR_RINGER_SETTING_CFG):
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
 * @param[in] val_id        Value Identifier (@see enum paspc_info)
 ****************************************************************************************
 */
__STATIC uint16_t paspc_read_val(uint8_t conidx, uint16_t val_id)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    paspc_env_t* p_paspc_env = PRF_ENV_GET(PASPC, paspc);

    if(p_paspc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_paspc_env->p_env[conidx] != NULL) && (!p_paspc_env->p_env[conidx]->discover))
        {
            paspc_cnx_env_t* p_con_env = p_paspc_env->p_env[conidx];
            uint16_t hdl;
            paspc_pass_content_t* p_pass = &(p_con_env->pass);

            switch(val_id)
            {
                case PASPC_RD_ALERT_STATUS:          { hdl = p_pass->chars[PASPC_CHAR_ALERT_STATUS].val_hdl;           } break;
                case PASPC_RD_RINGER_SETTING:        { hdl = p_pass->chars[PASPC_CHAR_RINGER_SETTING].val_hdl;         } break;
                case PASPC_RD_WR_ALERT_STATUS_CFG:   { hdl = p_pass->descs[PASPC_DESC_ALERT_STATUS_CL_CFG].desc_hdl;   } break;
                case PASPC_RD_WR_RINGER_SETTING_CFG: { hdl = p_pass->descs[PASPC_DESC_RINGER_SETTING_CL_CFG].desc_hdl; } break;
                default:                             { hdl = GATT_INVALID_HDL;                                         } break;
            }

            if(hdl == GATT_INVALID_HDL)
            {
                status = PRF_ERR_INEXISTENT_HDL;
            }
            else
            {
                // perform read request
                status = gatt_cli_read(conidx, p_paspc_env->user_lid, val_id, hdl, 0, 0);
            }
        }
    }

    return (status);
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
__STATIC void paspc_svc_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t hdl, uint8_t disc_info,
                          uint8_t nb_att, const gatt_svc_att_t* p_atts)
{
    // Get the address of the environment
    paspc_env_t* p_paspc_env = PRF_ENV_GET(PASPC, paspc);

    if(p_paspc_env != NULL)
    {
        paspc_cnx_env_t* p_con_env = p_paspc_env->p_env[conidx];

        ASSERT_INFO(p_con_env != NULL, conidx, user_lid);

        if (p_con_env->nb_svc == 0)
        {
            //Even if we get multiple responses we only store 1 range
            if((disc_info == GATT_SVC_CMPLT) || (disc_info == GATT_SVC_START))
            {
                p_con_env->pass.svc.shdl = hdl;
            }

            if((disc_info == GATT_SVC_CMPLT) || (disc_info == GATT_SVC_END))
            {
                p_con_env->pass.svc.ehdl = hdl + nb_att -1;
            }

            // Retrieve characteristics
            prf_extract_svc_info(hdl, nb_att, p_atts,
                                 PASPC_CHAR_MAX, &paspc_pass_char[0],      &(p_con_env->pass.chars[0]),
                                 PASPC_DESC_MAX, &paspc_pass_char_desc[0], &(p_con_env->pass.descs[0]));
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
__STATIC void paspc_discover_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    // Get the address of the environment
    paspc_env_t* p_paspc_env = PRF_ENV_GET(PASPC, paspc);

    if(p_paspc_env != NULL)
    {
        paspc_cnx_env_t* p_con_env = p_paspc_env->p_env[conidx];

        if (p_con_env->nb_svc ==  1)
        {
            status = prf_check_svc_validity(PASPC_CHAR_MAX, p_con_env->pass.chars, paspc_pass_char,
                                            PASPC_DESC_MAX, p_con_env->pass.descs, paspc_pass_char_desc);
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

        paspc_enable_cmp(p_paspc_env, conidx, status);
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
__STATIC void paspc_att_val_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t hdl, uint16_t offset,
                              co_buf_t* p_data)
{
    paspc_read_val_cmp(conidx, GAP_ERR_NO_ERROR, (uint8_t) dummy, co_buf_data_len(p_data), co_buf_data(p_data));
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
__STATIC void paspc_read_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    if(status != GAP_ERR_NO_ERROR)
    {
        paspc_read_val_cmp(conidx, status, (uint8_t) dummy, 0, NULL);
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
__STATIC void paspc_write_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    paspc_env_t* p_paspc_env = PRF_ENV_GET(PASPC, paspc);

    if(p_paspc_env != NULL)
    {
        const paspc_cb_t* p_cb = (const paspc_cb_t*) p_paspc_env->prf_env.p_cb;

        switch(dummy)
        {
            // Config control
            case PASPC_RD_WR_ALERT_STATUS_CFG:
            case PASPC_RD_WR_RINGER_SETTING_CFG:
            {
                p_cb->cb_write_cfg_cmp(conidx, status, dummy);
            } break;
            // Control point commands
            case PASPC_WR_RINGER_CTNL_PT:
            {
                p_cb->cb_ctnl_pt_req_cmp(conidx, status);
            } break;

            default: { /* Nothing to do */} break;
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
__STATIC void paspc_att_val_evt_cb(uint8_t conidx, uint8_t user_lid, uint16_t token, uint8_t evt_type, bool complete,
                        uint16_t hdl, co_buf_t* p_data)
{
    paspc_env_t* p_paspc_env = PRF_ENV_GET(PASPC, paspc);

    if(p_paspc_env != NULL)
    {
        paspc_cnx_env_t* p_con_env = p_paspc_env->p_env[conidx];
        paspc_pass_content_t* p_pass = &(p_con_env->pass);
        const paspc_cb_t* p_cb = (const paspc_cb_t*) p_paspc_env->prf_env.p_cb;

        if (hdl == p_pass->chars[PASPC_CHAR_ALERT_STATUS].val_hdl)
        {
            uint8_t alert_status = co_buf_data(p_data)[0];
            p_cb->cb_alert_status(conidx, alert_status);
        }
        else if (hdl == p_pass->chars[PASPC_CHAR_RINGER_SETTING].val_hdl)
        {
            uint8_t ringer_setting = co_buf_data(p_data)[0];
            p_cb->cb_ringer_setting(conidx, ringer_setting);
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
__STATIC void paspc_svc_changed_cb(uint8_t conidx, uint8_t user_lid, bool out_of_sync, uint16_t start_hdl, uint16_t end_hdl)
{
    // Do Nothing
}

/// Client callback hander
__STATIC const gatt_cli_cb_t paspc_cb =
{
    .cb_discover_cmp    = paspc_discover_cmp_cb,
    .cb_read_cmp        = paspc_read_cmp_cb,
    .cb_write_cmp       = paspc_write_cmp_cb,
    .cb_att_val_get     = NULL,
    .cb_svc             = paspc_svc_cb,
    .cb_svc_info        = NULL,
    .cb_inc_svc         = NULL,
    .cb_char            = NULL,
    .cb_desc            = NULL,
    .cb_att_val         = paspc_att_val_cb,
    .cb_att_val_evt     = paspc_att_val_evt_cb,
    .cb_svc_changed     = paspc_svc_changed_cb,
};

/*
 * PROFILE NATIVE API
 ****************************************************************************************
 */

uint16_t paspc_enable(uint8_t conidx, uint8_t con_type, const paspc_pass_content_t* p_pass)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    paspc_env_t* p_paspc_env = PRF_ENV_GET(PASPC, paspc);

    if(p_paspc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_paspc_env->p_env[conidx] == NULL))
        {
            // allocate environment variable for task instance
            p_paspc_env->p_env[conidx] = (struct paspc_cnx_env *) ke_malloc(sizeof(struct paspc_cnx_env), KE_MEM_ATT_DB);

            if(p_paspc_env->p_env[conidx] != NULL)
            {
                memset(p_paspc_env->p_env[conidx], 0, sizeof(struct paspc_cnx_env));

                // Config connection, start discovering
                if (con_type == PRF_CON_DISCOVERY)
                {
                    uint16_t gatt_svc_uuid = GATT_SVC_PHONE_ALERT_STATUS;

                    // start discovery
                    status = gatt_cli_discover_svc(conidx, p_paspc_env->user_lid, 0, GATT_DISCOVER_SVC_PRIMARY_BY_UUID, true,
                                                   GATT_MIN_HDL, GATT_MAX_HDL, GATT_UUID_16, (uint8_t*) &gatt_svc_uuid);

                    // Go to DISCOVERING state
                    p_paspc_env->p_env[conidx]->discover   = true;
                }
                // normal connection, get saved att details
                else
                {
                    memcpy(&(p_paspc_env->p_env[conidx]->pass), p_pass, sizeof(paspc_pass_content_t));
                    status = GAP_ERR_NO_ERROR;

                    // send APP confirmation that can start normal connection to TH
                    paspc_enable_cmp(p_paspc_env, conidx, GAP_ERR_NO_ERROR);
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

uint16_t paspc_read_alert_status(uint8_t conidx)
{
    uint16_t status = paspc_read_val(conidx, PASPC_RD_ALERT_STATUS);
    return (status);
}

uint16_t paspc_read_ringer_setting(uint8_t conidx)
{
    uint16_t status = paspc_read_val(conidx, PASPC_RD_RINGER_SETTING);
    return (status);
}

uint16_t paspc_read_cfg(uint8_t conidx, uint8_t desc_code)
{
    uint16_t status;

    switch(desc_code)
    {
        case PASPC_RD_WR_ALERT_STATUS_CFG:
        case PASPC_RD_WR_RINGER_SETTING_CFG: { status = paspc_read_val(conidx, desc_code);  } break;
        default:                             { status = PRF_ERR_INEXISTENT_HDL;             } break;
    }

    return (status);
}

uint16_t paspc_write_cfg(uint8_t conidx, uint8_t desc_code, uint16_t cfg_val)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    paspc_env_t* p_paspc_env = PRF_ENV_GET(PASPC, paspc);

    if(p_paspc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_paspc_env->p_env[conidx] != NULL) && (!p_paspc_env->p_env[conidx]->discover))
        {
            paspc_cnx_env_t* p_con_env = p_paspc_env->p_env[conidx];
            uint16_t hdl;
            paspc_pass_content_t* p_pass = &(p_con_env->pass);

            switch(desc_code)
            {
                case PASPC_RD_WR_ALERT_STATUS_CFG:   { hdl        = p_pass->descs[PASPC_DESC_ALERT_STATUS_CL_CFG].desc_hdl;   } break;
                case PASPC_RD_WR_RINGER_SETTING_CFG: { hdl        = p_pass->descs[PASPC_DESC_RINGER_SETTING_CL_CFG].desc_hdl; } break;
                default:                             { hdl = GATT_INVALID_HDL;                                                } break;
            }

            if(hdl == GATT_INVALID_HDL)
            {
                status = PRF_ERR_INEXISTENT_HDL;
            }
            else if((cfg_val != PRF_CLI_STOP_NTFIND) && (cfg_val != PRF_CLI_START_NTF))
            {
                status = PRF_ERR_INVALID_PARAM;
            }
            else
            {
                // Force endianess
                cfg_val = co_htobs(cfg_val);
                status = prf_gatt_write(conidx, p_paspc_env->user_lid, desc_code, GATT_WRITE,
                                        hdl, sizeof(uint16_t), (uint8_t *)&cfg_val);
            }
        }
    }

    return (status);
}

uint16_t paspc_ctnl_pt_req(uint8_t conidx, uint8_t ringer_ctnl_pt)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    paspc_env_t* p_paspc_env = PRF_ENV_GET(PASPC, paspc);

    if((ringer_ctnl_pt < PASP_SILENT_MODE) || (ringer_ctnl_pt > PASP_CANCEL_SILENT_MODE))
    {
        status = PRF_ERR_INVALID_PARAM;
    }
    else if(p_paspc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_paspc_env->p_env[conidx] != NULL) && (!p_paspc_env->p_env[conidx]->discover))
        {
            paspc_cnx_env_t* p_con_env = p_paspc_env->p_env[conidx];
            paspc_pass_content_t* p_pass = &(p_con_env->pass);
            uint16_t hdl = p_pass->chars[PASPC_CHAR_RINGER_CTNL_PT].val_hdl;

            if(hdl == GATT_INVALID_HDL)
            {
                status = PRF_ERR_INEXISTENT_HDL;
            }
            else
            {
                co_buf_t* p_buf = NULL;

                // allocate buffer for event transmission
                if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, sizeof(uint8_t), GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
                {
                    co_buf_data(p_buf)[0] = ringer_ctnl_pt;

                    status = gatt_cli_write(conidx, p_paspc_env->user_lid, PASPC_WR_RINGER_CTNL_PT, GATT_WRITE_NO_RESP,
                                            hdl, 0, p_buf);
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
 * @brief Send a PASPC_CMP_EVT message to the task which enabled the profile
 * @param[in] conidx Connection index
 * @param[in] operation Operation
 * @param[in] status Satus
 ****************************************************************************************
 */
__STATIC void paspc_send_cmp_evt(uint8_t conidx, uint8_t operation, uint16_t status)
{
    struct paspc_cmp_evt *p_evt;

    // Send the message
    p_evt = KE_MSG_ALLOC(PASPC_CMP_EVT, PRF_DST_TASK(PASPC), PRF_SRC_TASK(PASPC), paspc_cmp_evt);
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
__STATIC int paspc_enable_req_handler(ke_msg_id_t const msgid, struct paspc_enable_req const *p_param,
                                     ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = paspc_enable(p_param->conidx, p_param->con_type, &(p_param->pass));

    // send an error if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct paspc_enable_rsp *p_rsp = KE_MSG_ALLOC(PASPC_ENABLE_RSP, src_id, dest_id, paspc_enable_rsp);
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
 * @brief Handles reception of the @ref PASPC_READ_CMD message.
 * @param[in] msgid Id of the message received.
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int paspc_read_cmd_handler(ke_msg_id_t const msgid, struct paspc_read_cmd const *p_param,
                                    ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status;

    switch(p_param->read_code)
    {
        case PASPC_RD_ALERT_STATUS:          { status = paspc_read_alert_status(p_param->conidx);            } break;
        case PASPC_RD_RINGER_SETTING:        { status = paspc_read_ringer_setting(p_param->conidx);          } break;
        case PASPC_RD_WR_ALERT_STATUS_CFG:
        case PASPC_RD_WR_RINGER_SETTING_CFG: { status = paspc_read_cfg(p_param->conidx, p_param->read_code); } break;
        default:                             { status = PRF_ERR_INEXISTENT_HDL;                              } break;
    }

    // send error response if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct paspc_cmp_evt *p_evt = KE_MSG_ALLOC(PASPC_CMP_EVT, src_id, dest_id, paspc_cmp_evt);
        if(p_evt)
        {
            p_evt->conidx     = p_param->conidx;
            p_evt->operation  = PASPC_READ_OP_CODE;
            p_evt->status     = status;

            ke_msg_send(p_evt);
        }
    }

    return (KE_MSG_CONSUMED);
}



/**
 ****************************************************************************************
 * @brief Handles reception of the @ref PASPC_WRITE_CMD message.
 * @param[in] msgid Id of the message received.
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int paspc_write_cmd_handler(ke_msg_id_t const msgid, struct paspc_write_cmd const *p_param,
                                     ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status;
    uint8_t conidx = p_param->conidx;
    uint8_t write_code = p_param->write_code;

    switch(write_code)
    {
        case PASPC_WR_RINGER_CTNL_PT:        { status = paspc_ctnl_pt_req(conidx, p_param->value.ringer_ctnl_pt);                 } break;
        case PASPC_RD_WR_ALERT_STATUS_CFG:
        case PASPC_RD_WR_RINGER_SETTING_CFG: { status = paspc_write_cfg(conidx, write_code, p_param->value.alert_status_ntf_cfg); } break;
        default:                             { status = PRF_ERR_INEXISTENT_HDL;                                                   } break;
    }

    // send error response if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct paspc_cmp_evt *p_evt = KE_MSG_ALLOC(PASPC_CMP_EVT, src_id, dest_id, paspc_cmp_evt);
        if(p_evt)
        {
            p_evt->conidx     = p_param->conidx;
            p_evt->operation  = PASPC_WRITE_OP_CODE;
            p_evt->status     = status;

            ke_msg_send(p_evt);
        }
    }

    return (KE_MSG_CONSUMED);
}



/// Default State handlers definition
KE_MSG_HANDLER_TAB(paspc)
{
    // Note: all messages must be sorted in ID ascending order

    { PASPC_ENABLE_REQ,               (ke_msg_func_t) paspc_enable_req_handler         },
    { PASPC_READ_CMD,                 (ke_msg_func_t) paspc_read_cmd_handler           },
    { PASPC_WRITE_CMD,                (ke_msg_func_t) paspc_write_cmd_handler          },
};

/**
 ****************************************************************************************
 * @brief Completion of enable procedure
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 * @param[in] p_pass        Pointer to peer database description bond data
 ****************************************************************************************
 */
__STATIC void paspc_cb_enable_cmp(uint8_t conidx, uint16_t status, const paspc_pass_content_t* p_pass)
{
    // Send APP the details of the discovered attributes on PASPC
    struct paspc_enable_rsp *p_rsp = KE_MSG_ALLOC(PASPC_ENABLE_RSP,
                                                 PRF_DST_TASK(PASPC),
                                                 PRF_SRC_TASK(PASPC),
                                                 paspc_enable_rsp);
    if(p_rsp != NULL)
    {
        p_rsp->conidx = conidx;
        p_rsp->status = status;
        memcpy(&(p_rsp->pass), p_pass, sizeof(paspc_pass_content_t));
        ke_msg_send(p_rsp);
    }
}

/**
 ****************************************************************************************
 * @brief Completion of read sensor feature procedure.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 * @param[in] value  Current alert status value
 *
 ****************************************************************************************
 */
__STATIC void paspc_cb_read_alert_status_cmp(uint8_t conidx, uint16_t status, uint8_t value)
{
    ke_task_id_t src_id  = PRF_SRC_TASK(PASPC);
    ke_task_id_t dest_id = PRF_DST_TASK(PASPC);

    if(status == GAP_ERR_NO_ERROR)
    {
        struct paspc_value_ind *p_ind = KE_MSG_ALLOC(PASPC_VALUE_IND, dest_id, src_id, paspc_value_ind);
        if(p_ind != NULL)
        {
            p_ind->conidx             = conidx;
            p_ind->att_code           = PASPC_RD_ALERT_STATUS;
            p_ind->value.alert_status = value;
            ke_msg_send(p_ind);
        }
    }

    paspc_send_cmp_evt(conidx, PASPC_READ_OP_CODE, status);
}

    /**
     ****************************************************************************************
     * @brief Completion of read sensor location procedure.
     *
     * @param[in] conidx         Connection index
     * @param[in] status         Status of the procedure execution (@see enum hl_err)
     * @param[in] value          Current ringer setting value
     *
     ****************************************************************************************
     */
__STATIC void paspc_cb_read_ringer_setting_cmp(uint8_t conidx, uint16_t status, uint8_t value)
{
    ke_task_id_t src_id  = PRF_SRC_TASK(PASPC);
    ke_task_id_t dest_id = PRF_DST_TASK(PASPC);

    if(status == GAP_ERR_NO_ERROR)
    {
        struct paspc_value_ind *p_ind = KE_MSG_ALLOC(PASPC_VALUE_IND, dest_id, src_id, paspc_value_ind);
        if(p_ind != NULL)
        {
            p_ind->conidx               = conidx;
            p_ind->att_code             = PASPC_RD_RINGER_SETTING;
            p_ind->value.ringer_setting = value;
            ke_msg_send(p_ind);
        }
    }

    paspc_send_cmp_evt(conidx, PASPC_READ_OP_CODE, status);
}

/**
 ****************************************************************************************
 * @brief Completion of read Characteristic Configuration procedure.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 * @param[in] desc_code     Descriptor Code (@see enum paspc_rd_wr_codes)
 *                              - PASPC_RD_WR_ALERT_STATUS_CFG: PAS Alert Status CCC Descriptor
 *                              - PASPC_RD_WR_RINGER_SETTING_CFG: PAS Ringer Setting CCC Descriptor
 * @param[in] cfg_val       Configuration value
 *
 ****************************************************************************************
 */
__STATIC void paspc_cb_read_cfg_cmp(uint8_t conidx, uint16_t status, uint8_t desc_code, uint16_t cfg_val)
{
    ke_task_id_t src_id  = PRF_SRC_TASK(PASPC);
    ke_task_id_t dest_id = PRF_DST_TASK(PASPC);

    if(status == GAP_ERR_NO_ERROR)
    {
        struct paspc_value_ind *p_ind = KE_MSG_ALLOC(PASPC_VALUE_IND, dest_id, src_id, paspc_value_ind);
        if(p_ind != NULL)
        {
            p_ind->conidx                     = conidx;
            p_ind->att_code                   = desc_code;
            p_ind->value.alert_status_ntf_cfg = cfg_val;
            ke_msg_send(p_ind);
        }
    }

    paspc_send_cmp_evt(conidx, PASPC_READ_OP_CODE, status);
}

/**
 ****************************************************************************************
 * @brief Completion of write Characteristic Configuration procedure.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 * @param[in] desc_code     Descriptor Code (@see enum paspc_rd_wr_codes)
 *                              - PASPC_RD_WR_ALERT_STATUS_CFG: PAS Alert Status CCC Descriptor
 *                              - PASPC_RD_WR_RINGER_SETTING_CFG: PAS Ringer Setting CCC Descriptor
 *
 ****************************************************************************************
 */
__STATIC void paspc_cb_write_cfg_cmp(uint8_t conidx, uint16_t status, uint8_t desc_code)
{
    paspc_send_cmp_evt(conidx, PASPC_WRITE_OP_CODE, status);
}

/**
 ****************************************************************************************
 * @brief Function called when alert status update is received
 *
 * @param[in] conidx         Connection index
 * @param[in] value          Current alert status value
 ****************************************************************************************
 */
__STATIC void paspc_cb_alert_status(uint8_t conidx, uint8_t value)
{
    ke_task_id_t src_id  = PRF_SRC_TASK(PASPC);
    ke_task_id_t dest_id = PRF_DST_TASK(PASPC);

    struct paspc_value_ind *p_ind = KE_MSG_ALLOC(PASPC_VALUE_IND, dest_id, src_id, paspc_value_ind);
    if(p_ind != NULL)
    {
        p_ind->conidx             = conidx;
        p_ind->att_code           = PASPC_RD_ALERT_STATUS;
        p_ind->value.alert_status = value;
        ke_msg_send(p_ind);
    }
}

/**
 ****************************************************************************************
 * @brief Function called when ringer setting update is received
 *
 * @param[in] conidx         Connection index
 * @param[in] value          Current ringer setting value
 ****************************************************************************************
 */
__STATIC void paspc_cb_ringer_setting(uint8_t conidx, uint8_t value)
{
    ke_task_id_t src_id  = PRF_SRC_TASK(PASPC);
    ke_task_id_t dest_id = PRF_DST_TASK(PASPC);
    struct paspc_value_ind *p_ind = KE_MSG_ALLOC(PASPC_VALUE_IND, dest_id, src_id, paspc_value_ind);
    if(p_ind != NULL)
    {
        p_ind->conidx             = conidx;
        p_ind->att_code           = PASPC_RD_RINGER_SETTING;
        p_ind->value.alert_status = value;
        ke_msg_send(p_ind);
    }
}


/**
 ****************************************************************************************
 * @brief Completion of control point request procedure
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the Request Send (@see enum hl_err)
 ****************************************************************************************
 */
__STATIC void paspc_cb_ctnl_pt_req_cmp(uint8_t conidx, uint16_t status)
{
    paspc_send_cmp_evt(conidx, PASPC_WRITE_OP_CODE, status);
}

/// Default Message handle
__STATIC const paspc_cb_t paspc_msg_cb =
{
     .cb_enable_cmp              = paspc_cb_enable_cmp,
     .cb_read_alert_status_cmp   = paspc_cb_read_alert_status_cmp,
     .cb_read_ringer_setting_cmp = paspc_cb_read_ringer_setting_cmp,
     .cb_read_cfg_cmp            = paspc_cb_read_cfg_cmp,
     .cb_write_cfg_cmp           = paspc_cb_write_cfg_cmp,
     .cb_alert_status            = paspc_cb_alert_status,
     .cb_ringer_setting          = paspc_cb_ringer_setting,
     .cb_ctnl_pt_req_cmp         = paspc_cb_ctnl_pt_req_cmp,
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
__STATIC uint16_t paspc_init(prf_data_t* p_env, uint16_t* p_start_hdl, uint8_t sec_lvl, uint8_t user_prio,
                          const void* p_params, const paspc_cb_t* p_cb)
{
    uint8_t conidx;
    // DB Creation Status
    uint16_t status = GAP_ERR_NO_ERROR;
    uint8_t user_lid = GATT_INVALID_USER_LID;

    do
    {
        paspc_env_t* p_paspc_env;

        #if (BLE_HL_MSG_API)
        if(p_cb == NULL)
        {
            p_cb = &(paspc_msg_cb);
        }
        #endif // (BLE_HL_MSG_API)

        if(   (p_params == NULL) || (p_cb == NULL) || (p_cb->cb_enable_cmp == NULL) || (p_cb->cb_read_alert_status_cmp == NULL)
           || (p_cb->cb_read_ringer_setting_cmp == NULL) || (p_cb->cb_read_cfg_cmp == NULL)
           || (p_cb->cb_write_cfg_cmp == NULL) || (p_cb->cb_alert_status == NULL) || (p_cb->cb_ringer_setting == NULL)
           || (p_cb->cb_ctnl_pt_req_cmp == NULL))
        {
            status = GAP_ERR_INVALID_PARAM;
            break;
        }

        // register PASPC user
        status = gatt_user_cli_register(L2CAP_LE_MTU_MIN, user_prio, &paspc_cb, &user_lid);
        if(status != GAP_ERR_NO_ERROR) break;

        //-------------------- allocate memory required for the profile  ---------------------
        p_paspc_env = (paspc_env_t*) ke_malloc(sizeof(paspc_env_t), KE_MEM_ATT_DB);

        if(p_paspc_env != NULL)
        {
            // allocate PASPC required environment variable
            p_env->p_env = (prf_hdr_t *) p_paspc_env;

            // initialize environment variable
            p_paspc_env->prf_env.p_cb    = p_cb;
            #if (BLE_HL_MSG_API)
            p_env->desc.msg_handler_tab = paspc_msg_handler_tab;
            p_env->desc.msg_cnt         = ARRAY_LEN(paspc_msg_handler_tab);
            #endif // (BLE_HL_MSG_API)

            p_paspc_env->user_lid = user_lid;
            for (conidx = 0; conidx < BLE_CONNECTION_MAX; conidx++)
            {
                p_paspc_env->p_env[conidx] = NULL;
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
__STATIC uint16_t paspc_destroy(prf_data_t *p_env, uint8_t reason)
{
    uint16_t status = GAP_ERR_NO_ERROR;
    paspc_env_t* p_paspc_env = (paspc_env_t*) p_env->p_env;

    if(reason != PRF_DESTROY_RESET)
    {
        status = gatt_user_unregister(p_paspc_env->user_lid);
    }

    if(status == GAP_ERR_NO_ERROR)
    {
        uint8_t conidx;

        // cleanup environment variable for each task instances
        for (conidx = 0; conidx < BLE_CONNECTION_MAX; conidx++)
        {
            if (p_paspc_env->p_env[conidx] != NULL)
            {
                ke_free(p_paspc_env->p_env[conidx]);
            }
        }

        // free profile environment variables
        p_env->p_env = NULL;
        ke_free(p_paspc_env);
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
__STATIC void paspc_con_create(prf_data_t *p_env, uint8_t conidx, const gap_con_param_t* p_con_param)
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
__STATIC void paspc_con_cleanup(prf_data_t *p_env, uint8_t conidx, uint16_t reason)
{
    paspc_env_t* p_paspc_env = (paspc_env_t*) p_env->p_env;

    // clean-up environment variable allocated for task instance
    if (p_paspc_env->p_env[conidx] != NULL)
    {
        ke_free(p_paspc_env->p_env[conidx]);
        p_paspc_env->p_env[conidx] = NULL;
    }
}

/// PASPC Task interface required by profile manager
const prf_task_cbs_t paspc_itf =
{
    .cb_init          = (prf_init_cb) paspc_init,
    .cb_destroy       = paspc_destroy,
    .cb_con_create    = paspc_con_create,
    .cb_con_cleanup   = paspc_con_cleanup,
    .cb_con_upd       = NULL,
};

/**
 ****************************************************************************************
 * @brief Retrieve client profile interface
 *
 * @return Client profile interface
 ****************************************************************************************
 */
const prf_task_cbs_t* paspc_prf_itf_get(void)
{
    return &paspc_itf;
}
#endif //(BLE_PAS_CLIENT)

/// @} PASPC

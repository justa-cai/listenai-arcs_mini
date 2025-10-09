/**
 ****************************************************************************************
 *
 * @file wscc.c
 *
 * @brief Weight SCale Profile Collector implementation.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 ****************************************************************************************
 */


/**
 ****************************************************************************************
 * @addtogroup WSCC
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "btip_config.h"

#if (BLE_WSC_CLIENT)
#include "wscc.h"
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
typedef struct wscc_cnx_env
{
    /// Peer database discovered handle mapping
    wscc_wss_content_t    wss;
    /// counter used to check service uniqueness
    uint8_t               nb_svc;
    /// Client is in discovering state
    bool                  discover;
} wscc_cnx_env_t;

/// Client environment variable
typedef struct wscc_env
{
    /// profile environment
    prf_hdr_t            prf_env;
    /// Environment variable pointer for each connections
    wscc_cnx_env_t*      p_env[BLE_CONNECTION_MAX];
    /// GATT User local identifier
    uint8_t              user_lid;
} wscc_env_t;


/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */

/// State machine used to retrieve Weight Scale Service characteristics information
const prf_char_def_t wscc_wss_char[WSCC_CHAR_WSS_MAX] =
{
    [WSCC_CHAR_WSS_FEATURE] = { GATT_CHAR_WEIGHT_SCALE_FEATURE, ATT_REQ(PRES, MAND), (PROP(RD)) },

    [WSCC_CHAR_WSS_MEAS]    = { GATT_CHAR_WEIGHT_MEASUREMENT,   ATT_REQ(PRES, MAND), (PROP(I))  },
};

/// State machine used to retrieve Weight Scale Service characteristic description information
const prf_desc_def_t wscc_wss_char_desc[WSCC_DESC_WSS_MAX] =
{
    /// Client config
    [WSCC_DESC_WSS_MEAS_CCC] = { GATT_DESC_CLIENT_CHAR_CFG, ATT_REQ(PRES, MAND), WSCC_CHAR_WSS_MEAS },
};

/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Send discovery results to application.
 *
 * @param p_wscc_env    Client Role task environment
 * @param conidx        Connection index
 * @param status        Response status code
 *****************************************************************************************
 */
__STATIC void wscc_enable_cmp(wscc_env_t* p_wscc_env, uint8_t conidx, uint16_t status)
{
    const wscc_cb_t* p_cb = (const wscc_cb_t*) p_wscc_env->prf_env.p_cb;

    if(p_wscc_env != NULL)
    {
        wscc_cnx_env_t* p_con_env = p_wscc_env->p_env[conidx];
        p_cb->cb_enable_cmp(conidx, status, &(p_con_env->wss));

        if (status != GAP_ERR_NO_ERROR)
        {
            // clean-up environment variable allocated for task instance
            ke_free(p_con_env);
            p_wscc_env->p_env[conidx] = NULL;
        }
        else
        {
             p_con_env->discover = false;

             // Register profile handle to catch gatt indications
             gatt_cli_event_register(conidx, p_wscc_env->user_lid, p_con_env->wss.svc.shdl,
                                     p_con_env->wss.svc.ehdl);
        }
    }
}

/**
 ****************************************************************************************
 * @brief Send read result to application,.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the request
 * @param[in] val_id        Value Identifier (@see enum wscc_val_id)
 * @param[in] length        Length of data value)
 * @param[in] p_data        Pointer of buffer that contains data value
 ****************************************************************************************
 */
__STATIC void wscc_read_val_cmp(uint8_t conidx, uint16_t status, uint8_t val_id, co_buf_t* p_data)
{
    wscc_env_t* p_wscc_env = PRF_ENV_GET(WSCC, wscc);

    if(p_wscc_env != NULL)
    {
        const wscc_cb_t* p_cb = (const wscc_cb_t*) p_wscc_env->prf_env.p_cb;
        switch (val_id)
        {
            // Read RSC Feature Characteristic value
            case (WSCC_READ_FEAT_OP_CODE):
            {
                uint32_t features = 0;
                if(status == GAP_ERR_NO_ERROR)
                {
                    features = co_btohl(co_read32p(co_buf_data(p_data)));
                }
                p_cb->cb_read_feat_cmp(conidx, status, features);
            } break;

            // Read Client Characteristic Configuration Descriptor value
            case (WSCC_READ_CCC_OP_CODE):
            {
                uint16_t cfg_val = 0;

                if(status == GAP_ERR_NO_ERROR)
                {
                    cfg_val = co_btohs(co_read16p(co_buf_data(p_data)));
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

/**
 ****************************************************************************************
 * @brief Perform Value read procedure.
 *
 * @param[in] conidx        Connection index
 * @param[in] val_id        Value Identifier (@see enum wscc_info)
 ****************************************************************************************
 */
__STATIC uint16_t wscc_read_val(uint8_t conidx, uint16_t val_id)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    wscc_env_t* p_wscc_env = PRF_ENV_GET(WSCC, wscc);

    if(p_wscc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_wscc_env->p_env[conidx] != NULL) && (!p_wscc_env->p_env[conidx]->discover))
        {
            wscc_cnx_env_t* p_con_env = p_wscc_env->p_env[conidx];
            uint16_t hdl;
            wscc_wss_content_t* p_wss = &(p_con_env->wss);

            switch(val_id)
            {
                case WSCC_READ_FEAT_OP_CODE: { hdl = p_wss->chars[WSCC_CHAR_WSS_FEATURE].val_hdl;   } break;
                case WSCC_READ_CCC_OP_CODE:  { hdl = p_wss->descs[WSCC_DESC_WSS_MEAS_CCC].desc_hdl; } break;
                default:                     { hdl = GATT_INVALID_HDL;                              } break;
            }

            if(hdl == GATT_INVALID_HDL)
            {
                status = PRF_ERR_INEXISTENT_HDL;
            }
            else
            {
                // perform read request
                status = gatt_cli_read(conidx, p_wscc_env->user_lid, val_id, hdl, 0, 0);
            }
        }
    }

    return (status);
}

/**
 ****************************************************************************************
 * @brief Unpacks measurement data and sends the indication
 * @param[in] p_wscc_env    Environment variable
 * @param[in] conidx        Connection index
 * @param[in] p_buf         Pointer of input buffer received
 ****************************************************************************************
 */
__STATIC void wscc_unpack_meas(wscc_env_t* p_wscc_env, uint8_t conidx, co_buf_t* p_buf)
{
    const wscc_cb_t* p_cb = (const wscc_cb_t*) p_wscc_env->prf_env.p_cb;
    wsc_meas_t meas;
    wsc_meas_prop_t meas_prop;
    memset(&meas,      0, sizeof(wsc_meas_t));
    memset(&meas_prop, 0, sizeof(wsc_meas_prop_t));

    // Flags
    meas.flags = co_buf_data(p_buf)[0] & WSC_MEAS_FLAGS_VALID;
    co_buf_head_release(p_buf, 1);


    // Mandatory weight
    meas.weight = co_btohs(co_read16p(co_buf_data(p_buf)));
    co_buf_head_release(p_buf, 2);

    // Weight resolution
    meas_prop.wght_resol = co_buf_data(p_buf)[0];
    co_buf_head_release(p_buf, 1);

    // Measurement Units
    meas_prop.meas_u = co_buf_data(p_buf)[0];
    co_buf_head_release(p_buf, 1);

    // Timestamp if present
    if (GETB(meas.flags, WSC_MEAS_FLAGS_TIMESTAMP_PRESENT))
    {
        prf_unpack_date_time(p_buf, &(meas.time_stamp));
    }

    // User Id if present
    if (GETB(meas.flags, WSC_MEAS_FLAGS_USER_ID_PRESENT))
    {
        meas.user_id = co_buf_data(p_buf)[0];
        co_buf_head_release(p_buf, 1);
    }

    // BMI & Height if present
    if (GETB(meas.flags, WSC_MEAS_FLAGS_BMI_PRESENT))
    {
        // BMI
        meas.bmi = co_btohs(co_read16p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 2);
        // Height
        meas.height = co_btohs(co_read16p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 2);
        // Height Resolution
        meas_prop.hght_resol = co_buf_data(p_buf)[0];
        co_buf_head_release(p_buf, 1);
    }

    // Inform application about received measurement
    p_cb->cb_meas(conidx, &meas, &meas_prop);
}


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
__STATIC void wscc_svc_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t hdl, uint8_t disc_info,
                          uint8_t nb_att, const gatt_svc_att_t* p_atts)
{
    // Get the address of the environment
    wscc_env_t* p_wscc_env = PRF_ENV_GET(WSCC, wscc);

    if(p_wscc_env != NULL)
    {
        wscc_cnx_env_t* p_con_env = p_wscc_env->p_env[conidx];

        ASSERT_INFO(p_con_env != NULL, conidx, user_lid);

        if (p_con_env->nb_svc == 0)
        {
            uint8_t att_cursor;

            //Even if we get multiple responses we only store 1 range
            if((disc_info == GATT_SVC_CMPLT) || (disc_info == GATT_SVC_START))
            {
                p_con_env->wss.svc.shdl = hdl;
            }

            if((disc_info == GATT_SVC_CMPLT) || (disc_info == GATT_SVC_END))
            {
                p_con_env->wss.svc.ehdl = hdl + nb_att -1;
            }

            // Retrieve characteristics
            prf_extract_svc_info(hdl, nb_att, p_atts,
                                 WSCC_CHAR_WSS_MAX, &wscc_wss_char[0],      &(p_con_env->wss.chars[0]),
                                 WSCC_DESC_WSS_MAX,     &wscc_wss_char_desc[0], &(p_con_env->wss.descs[0]));


            // Search for an included service
            for (att_cursor = 0; att_cursor < nb_att; att_cursor++)
            {
                if(   (p_atts[att_cursor].att_type == GATT_ATT_INCL_SVC)
                   && (gatt_uuid16_comp(p_atts[att_cursor].uuid, p_atts[att_cursor].uuid_type, GATT_SVC_BODY_COMPOSITION)))
                {
                    p_con_env->wss.incl_svc.handle    = hdl + att_cursor;
                    p_con_env->wss.incl_svc.start_hdl = p_atts[att_cursor].info.svc.start_hdl;
                    p_con_env->wss.incl_svc.end_hdl   = p_atts[att_cursor].info.svc.end_hdl;
                    p_con_env->wss.incl_svc.uuid_len  = GATT_UUID_16_LEN;
                    // Copy UUID data
                    memcpy(p_con_env->wss.incl_svc.uuid, p_atts[att_cursor].uuid, GATT_UUID_16_LEN);
                }
            }
        }

        if((disc_info == GATT_SVC_CMPLT) || (disc_info == GATT_SVC_END))
        {
            p_con_env->nb_svc++;
        }
    }
}

/*
 * GATT USER CLIENT HANDLERS
 ****************************************************************************************
 */


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
__STATIC void wscc_discover_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    // Get the address of the environment
    wscc_env_t* p_wscc_env = PRF_ENV_GET(WSCC, wscc);

    if(p_wscc_env != NULL)
    {
        wscc_cnx_env_t* p_con_env = p_wscc_env->p_env[conidx];

        if (p_con_env->nb_svc ==  1)
        {
            status = prf_check_svc_validity(WSCC_CHAR_WSS_MAX, p_con_env->wss.chars, wscc_wss_char,
                                            WSCC_DESC_WSS_MAX, p_con_env->wss.descs, wscc_wss_char_desc);
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

        wscc_enable_cmp(p_wscc_env, conidx, status);
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
__STATIC void wscc_att_val_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t hdl, uint16_t offset,
                              co_buf_t* p_data)
{
    wscc_read_val_cmp(conidx, GAP_ERR_NO_ERROR, (uint8_t) dummy, p_data);
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
__STATIC void wscc_read_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    if(status != GAP_ERR_NO_ERROR)
    {
        wscc_read_val_cmp(conidx, status, (uint8_t) dummy, NULL);
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
__STATIC void wscc_write_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    wscc_env_t* p_wscc_env = PRF_ENV_GET(WSCC, wscc);

    if(p_wscc_env != NULL)
    {
        const wscc_cb_t* p_cb = (const wscc_cb_t*) p_wscc_env->prf_env.p_cb;

        switch(dummy)
        {
            // Config control
            case WSCC_WRITE_CCC_OP_CODE:
            {
                p_cb->cb_write_cfg_cmp(conidx, status);
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
__STATIC void wscc_att_val_evt_cb(uint8_t conidx, uint8_t user_lid, uint16_t token, uint8_t evt_type, bool complete,
                        uint16_t hdl, co_buf_t* p_data)
{
    wscc_env_t* p_wscc_env = PRF_ENV_GET(WSCC, wscc);

    if(p_wscc_env != NULL)
    {
        wscc_cnx_env_t* p_con_env = p_wscc_env->p_env[conidx];
        wscc_wss_content_t* p_wss = &(p_con_env->wss);

        if (hdl == p_wss->chars[WSCC_CHAR_WSS_MEAS].val_hdl)
        {
            //Unpack measurement
            wscc_unpack_meas(p_wscc_env, conidx, p_data);
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
__STATIC void wscc_svc_changed_cb(uint8_t conidx, uint8_t user_lid, bool out_of_sync, uint16_t start_hdl, uint16_t end_hdl)
{
    // Do Nothing
}

/// Client callback hander
__STATIC const gatt_cli_cb_t wscc_cb =
{
    .cb_discover_cmp    = wscc_discover_cmp_cb,
    .cb_read_cmp        = wscc_read_cmp_cb,
    .cb_write_cmp       = wscc_write_cmp_cb,
    .cb_att_val_get     = NULL,
    .cb_svc             = wscc_svc_cb,
    .cb_svc_info        = NULL,
    .cb_inc_svc         = NULL,
    .cb_char            = NULL,
    .cb_desc            = NULL,
    .cb_att_val         = wscc_att_val_cb,
    .cb_att_val_evt     = wscc_att_val_evt_cb,
    .cb_svc_changed     = wscc_svc_changed_cb,
};

/*
 * PROFILE NATIVE API
 ****************************************************************************************
 */

uint16_t wscc_enable(uint8_t conidx, uint8_t con_type, const wscc_wss_content_t* p_wss)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    wscc_env_t* p_wscc_env = PRF_ENV_GET(WSCC, wscc);

    if(p_wscc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_wscc_env->p_env[conidx] == NULL))
        {
            // allocate environment variable for task instance
            p_wscc_env->p_env[conidx] = (struct wscc_cnx_env *) ke_malloc(sizeof(struct wscc_cnx_env), KE_MEM_ATT_DB);

            if(p_wscc_env->p_env[conidx] != NULL)
            {
                memset(p_wscc_env->p_env[conidx], 0, sizeof(struct wscc_cnx_env));

                // Config connection, start discovering
                if (con_type == PRF_CON_DISCOVERY)
                {
                    uint16_t gatt_svc_uuid = GATT_SVC_WEIGHT_SCALE;

                    // start discovery
                    status = gatt_cli_discover_svc(conidx, p_wscc_env->user_lid, 0, GATT_DISCOVER_SVC_PRIMARY_BY_UUID, true,
                                                   GATT_MIN_HDL, GATT_MAX_HDL, GATT_UUID_16, (uint8_t*) &gatt_svc_uuid);

                    // Go to DISCOVERING state
                    p_wscc_env->p_env[conidx]->discover   = true;
                }
                // normal connection, get saved att details
                else
                {
                    memcpy(&(p_wscc_env->p_env[conidx]->wss), p_wss, sizeof(wscc_wss_content_t));
                    status = GAP_ERR_NO_ERROR;

                    // send APP confirmation that can start normal connection to TH
                    wscc_enable_cmp(p_wscc_env, conidx, GAP_ERR_NO_ERROR);
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

uint16_t wscc_read_feat(uint8_t conidx)
{
    uint16_t status = wscc_read_val(conidx, WSCC_READ_FEAT_OP_CODE);
    return (status);
}

uint16_t wscc_read_cfg(uint8_t conidx)
{
    uint16_t status = wscc_read_val(conidx, WSCC_READ_CCC_OP_CODE);
    return (status);
}

uint16_t wscc_write_cfg(uint8_t conidx, uint16_t cfg_val)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    wscc_env_t* p_wscc_env = PRF_ENV_GET(WSCC, wscc);

    if(p_wscc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_wscc_env->p_env[conidx] != NULL) && (!p_wscc_env->p_env[conidx]->discover))
        {
            wscc_cnx_env_t* p_con_env = p_wscc_env->p_env[conidx];
            uint16_t hdl;
            wscc_wss_content_t* p_wss = &(p_con_env->wss);

            hdl = p_wss->descs[WSCC_DESC_WSS_MEAS_CCC].desc_hdl;

            if(hdl == GATT_INVALID_HDL)
            {
                status = PRF_ERR_INEXISTENT_HDL;
            }
            else if((cfg_val != PRF_CLI_STOP_NTFIND) && (cfg_val != PRF_CLI_START_IND))
            {
                status = PRF_ERR_INVALID_PARAM;
            }
            else
            {
                // Force endianess
                cfg_val = co_htobs(cfg_val);
                status = prf_gatt_write(conidx, p_wscc_env->user_lid, WSCC_WRITE_CCC_OP_CODE, GATT_WRITE,
                                        hdl, sizeof(uint16_t), (uint8_t *)&cfg_val);
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
 * @brief Send a WSCC_CMP_EVT message to the task which enabled the profile
 * @param[in] conidx Connection index
 * @param[in] operation Operation
 * @param[in] status Satus
 ****************************************************************************************
 */
__STATIC void wscc_send_cmp_evt(uint8_t conidx, uint8_t operation, uint16_t status)
{
    struct wscc_cmp_evt *p_evt;

    // Send the message
    p_evt = KE_MSG_ALLOC(WSCC_CMP_EVT, PRF_DST_TASK(WSCC), PRF_SRC_TASK(WSCC), wscc_cmp_evt);
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
__STATIC int wscc_enable_req_handler(ke_msg_id_t const msgid, struct wscc_enable_req const *p_param,
                                     ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = wscc_enable(p_param->conidx, p_param->con_type, &(p_param->wss));

    // send an error if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct wscc_enable_rsp *p_rsp = KE_MSG_ALLOC(WSCC_ENABLE_RSP, src_id, dest_id, wscc_enable_rsp);
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
 * @brief Handles reception of the @ref WSCC_RD_FEAT_CMD message from the application.
 * @brief To read the Feature Characteristic in the peer server.
 *
 * @param[in] msgid Id of the message received (probably unused).
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 *
 * @return If the message was consumed or not.
 ****************************************************************************************
 */

__STATIC int wscc_rd_feat_cmd_handler(ke_msg_id_t const msgid, struct wscc_rd_feat_cmd *p_param,
                                      ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = wscc_read_feat(p_param->conidx);

    // send error response if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct wscc_cmp_evt *p_evt = KE_MSG_ALLOC(WSCC_CMP_EVT, src_id, dest_id, wscc_cmp_evt);
        if(p_evt)
        {
            p_evt->conidx     = p_param->conidx;
            p_evt->operation  = WSCC_READ_FEAT_OP_CODE;
            p_evt->status     = status;

            ke_msg_send(p_evt);
        }
    }

    return (KE_MSG_CONSUMED);
}


/**
 ****************************************************************************************
 * @brief Handles reception of the @ref WSCC_RD_MEAS_CCC_CMD  message from the application.
 * @brief To read the CCC value of the Measurement characteristic in the peer server.
 *
 * @param[in] msgid Id of the message received (probably unused).
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 *
 * @return If the message was consumed or not.
 ****************************************************************************************
 */

__STATIC int wscc_rd_meas_ccc_cmd_handler(ke_msg_id_t const msgid,
        struct wscc_rd_meas_ccc_cmd *p_param,
        ke_task_id_t const dest_id,
        ke_task_id_t const src_id)
{
    uint16_t status = wscc_read_cfg(p_param->conidx);

    // send error response if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct wscc_cmp_evt *p_evt = KE_MSG_ALLOC(WSCC_CMP_EVT, src_id, dest_id, wscc_cmp_evt);
        if(p_evt)
        {
            p_evt->conidx     = p_param->conidx;
            p_evt->operation  = WSCC_READ_CCC_OP_CODE;
            p_evt->status     = status;

            ke_msg_send(p_evt);
        }
    }

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief Handles reception of the @ref WSCC_WR_MEAS_CCC_CMD message.
 * Allows the application to write new CCC values to a Characteristic in the peer server
 *
 * @param[in] msgid Id of the message received.
 * @param[in] p_cmd Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id ID of the sending task instance.
 *
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int wscc_wr_meas_ccc_cmd_handler(ke_msg_id_t const msgid, struct wscc_wr_meas_ccc_cmd *p_param,
                                          ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = wscc_write_cfg(p_param->conidx, p_param->ccc);

    // send error response if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct wscc_cmp_evt *p_evt = KE_MSG_ALLOC(WSCC_CMP_EVT, src_id, dest_id, wscc_cmp_evt);
        if(p_evt)
        {
            p_evt->conidx     = p_param->conidx;
            p_evt->operation  = WSCC_WRITE_CCC_OP_CODE;
            p_evt->status     = status;

            ke_msg_send(p_evt);
        }
    }

    return (KE_MSG_CONSUMED);
}

/// Default State handlers definition
KE_MSG_HANDLER_TAB(wscc)
{
    // Note: all messages must be sorted in ID ascending order

    {WSCC_ENABLE_REQ,                   (ke_msg_func_t) wscc_enable_req_handler      },
    {WSCC_RD_FEAT_CMD,                  (ke_msg_func_t) wscc_rd_feat_cmd_handler     },
    {WSCC_RD_MEAS_CCC_CMD,              (ke_msg_func_t) wscc_rd_meas_ccc_cmd_handler },
    {WSCC_WR_MEAS_CCC_CMD,              (ke_msg_func_t) wscc_wr_meas_ccc_cmd_handler },
};


/**
 ****************************************************************************************
 * @brief Completion of enable procedure
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 * @param[in] p_wss         Pointer to peer database description bond data
 ****************************************************************************************
 */
__STATIC void wscc_cb_enable_cmp(uint8_t conidx, uint16_t status, const wscc_wss_content_t* p_wss)
{
    // Send APP the details of the discovered attributes on WSCC
    struct wscc_enable_rsp *p_rsp = KE_MSG_ALLOC(WSCC_ENABLE_RSP, PRF_DST_TASK(WSCC), PRF_SRC_TASK(WSCC),
                                                 wscc_enable_rsp);
    if(p_rsp != NULL)
    {
        p_rsp->conidx = conidx;
        p_rsp->status = status;
        memcpy(&(p_rsp->wss), p_wss, sizeof(wscc_wss_content_t));
        ke_msg_send(p_rsp);
    }
}

/**
 ****************************************************************************************
 * @brief Completion of read feature procedure.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 * @param[in] feature       Weight Scale Feature read value
 *
 ****************************************************************************************
 */
__STATIC void wscc_cb_read_feat_cmp(uint8_t conidx, uint16_t status, uint32_t feature)
{
    ke_task_id_t src_id  = PRF_SRC_TASK(WSCC);
    ke_task_id_t dest_id = PRF_DST_TASK(WSCC);

    if(status == GAP_ERR_NO_ERROR)
    {
        struct wscc_feat_ind *p_ind = KE_MSG_ALLOC(WSCC_FEAT_IND, dest_id, src_id, wscc_feat_ind);
        if(p_ind != NULL)
        {
            p_ind->conidx   = conidx;
            p_ind->feature  = feature;
            ke_msg_send(p_ind);
        }
    }

    wscc_send_cmp_evt(conidx, WSCC_READ_FEAT_OP_CODE, status);
}

/**
 ****************************************************************************************
 * @brief Completion of read Characteristic Configuration procedure.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 * @param[in] cfg_val       Configuration value
 *
 ****************************************************************************************
 */
__STATIC void wscc_cb_read_cfg_cmp(uint8_t conidx, uint16_t status, uint16_t cfg_val)
{
    ke_task_id_t src_id  = PRF_SRC_TASK(WSCC);
    ke_task_id_t dest_id = PRF_DST_TASK(WSCC);

    if(status == GAP_ERR_NO_ERROR)
    {
        struct wscc_meas_ccc_ind *p_ind = KE_MSG_ALLOC(WSCC_MEAS_CCC_IND, dest_id, src_id, wscc_meas_ccc_ind);
        if(p_ind != NULL)
        {
            p_ind->conidx  = conidx;
            p_ind->ccc     = cfg_val;
            ke_msg_send(p_ind);
        }
    }

    wscc_send_cmp_evt(conidx, WSCC_READ_CCC_OP_CODE, status);
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
__STATIC void wscc_cb_write_cfg_cmp(uint8_t conidx, uint16_t status)
{
    wscc_send_cmp_evt(conidx, WSCC_WRITE_CCC_OP_CODE, status);
}
/**
 ****************************************************************************************
 * @brief Function called when weight measurement information is received
 *
 * @param[in] conidx         Connection index
 * @param[in] p_meas         Pointer to weight measurement data
 * @param[in] p_meas_prop    Pointer to measurement properties
 ****************************************************************************************
 */
__STATIC void wscc_cb_meas(uint8_t conidx, const wsc_meas_t* p_meas, const wsc_meas_prop_t* p_meas_prop)
{
    ke_task_id_t src_id  = PRF_SRC_TASK(WSCC);
    ke_task_id_t dest_id = PRF_DST_TASK(WSCC);

    struct wscc_meas_ind *p_ind = KE_MSG_ALLOC(WSCC_MEAS_IND, dest_id, src_id, wscc_meas_ind);
    if(p_ind != NULL)
    {
        p_ind->conidx            = conidx;
        p_ind->flags             = p_meas->flags;
        p_ind->weight            = p_meas->weight;
        p_ind->wght_resol        = p_meas_prop->wght_resol;
        p_ind->meas_u            = p_meas_prop->meas_u;
        p_ind->time_stamp        = p_meas->time_stamp;
        p_ind->user_id           = p_meas->user_id;
        p_ind->bmi               = p_meas->bmi;
        p_ind->height            = p_meas->height;
        p_ind->hght_resol        = p_meas_prop->hght_resol;
        ke_msg_send(p_ind);
    }
}

/// Default Message handle
__STATIC const wscc_cb_t wscc_msg_cb =
{
     .cb_enable_cmp           = wscc_cb_enable_cmp,
     .cb_read_feat_cmp        = wscc_cb_read_feat_cmp,
     .cb_read_cfg_cmp         = wscc_cb_read_cfg_cmp,
     .cb_write_cfg_cmp        = wscc_cb_write_cfg_cmp,
     .cb_meas                 = wscc_cb_meas,
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
__STATIC uint16_t wscc_init(prf_data_t* p_env, uint16_t* p_start_hdl, uint8_t sec_lvl, uint8_t user_prio,
                          const void* p_params, const wscc_cb_t* p_cb)
{
    uint8_t conidx;
    // DB Creation Status
    uint16_t status = GAP_ERR_NO_ERROR;
    uint8_t user_lid = GATT_INVALID_USER_LID;

    do
    {
        wscc_env_t* p_wscc_env;

        #if (BLE_HL_MSG_API)
        if(p_cb == NULL)
        {
            p_cb = &(wscc_msg_cb);
        }
        #endif // (BLE_HL_MSG_API)

        if(   (p_params == NULL) || (p_cb == NULL) || (p_cb->cb_enable_cmp == NULL) || (p_cb->cb_read_feat_cmp == NULL)
           || (p_cb->cb_read_cfg_cmp == NULL) || (p_cb->cb_write_cfg_cmp == NULL) || (p_cb->cb_meas == NULL))
        {
            status = GAP_ERR_INVALID_PARAM;
            break;
        }

        // register WSCC user
        status = gatt_user_cli_register(L2CAP_LE_MTU_MIN, user_prio, &wscc_cb, &user_lid);
        if(status != GAP_ERR_NO_ERROR) break;

        //-------------------- allocate memory required for the profile  ---------------------
        p_wscc_env = (wscc_env_t*) ke_malloc(sizeof(wscc_env_t), KE_MEM_ATT_DB);

        if(p_wscc_env != NULL)
        {
            // allocate WSCC required environment variable
            p_env->p_env = (prf_hdr_t *) p_wscc_env;

            // initialize environment variable
            p_wscc_env->prf_env.p_cb    = p_cb;
            #if (BLE_HL_MSG_API)
            p_env->desc.msg_handler_tab = wscc_msg_handler_tab;
            p_env->desc.msg_cnt         = ARRAY_LEN(wscc_msg_handler_tab);
            #endif // (BLE_HL_MSG_API)

            p_wscc_env->user_lid = user_lid;
            for (conidx = 0; conidx < BLE_CONNECTION_MAX; conidx++)
            {
                p_wscc_env->p_env[conidx] = NULL;
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
__STATIC uint16_t wscc_destroy(prf_data_t *p_env, uint8_t reason)
{
    uint16_t status = GAP_ERR_NO_ERROR;
    wscc_env_t* p_wscc_env = (wscc_env_t*) p_env->p_env;

    if(reason != PRF_DESTROY_RESET)
    {
        status = gatt_user_unregister(p_wscc_env->user_lid);
    }

    if(status == GAP_ERR_NO_ERROR)
    {
        uint8_t conidx;

        // cleanup environment variable for each task instances
        for (conidx = 0; conidx < BLE_CONNECTION_MAX; conidx++)
        {
            if (p_wscc_env->p_env[conidx] != NULL)
            {
                ke_free(p_wscc_env->p_env[conidx]);
            }
        }

        // free profile environment variables
        p_env->p_env = NULL;
        ke_free(p_wscc_env);
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
__STATIC void wscc_con_create(prf_data_t *p_env, uint8_t conidx, const gap_con_param_t* p_con_param)
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
__STATIC void wscc_con_cleanup(prf_data_t *p_env, uint8_t conidx, uint16_t reason)
{
    wscc_env_t* p_wscc_env = (wscc_env_t*) p_env->p_env;

    // clean-up environment variable allocated for task instance
    if (p_wscc_env->p_env[conidx] != NULL)
    {
        ke_free(p_wscc_env->p_env[conidx]);
        p_wscc_env->p_env[conidx] = NULL;
    }
}

/// WSCC Task interface required by profile manager
const prf_task_cbs_t wscc_itf =
{
    .cb_init          = (prf_init_cb) wscc_init,
    .cb_destroy       = wscc_destroy,
    .cb_con_create    = wscc_con_create,
    .cb_con_cleanup   = wscc_con_cleanup,
    .cb_con_upd       = NULL,
};

/**
 ****************************************************************************************
 * @brief Retrieve client profile interface
 *
 * @return Client profile interface
 ****************************************************************************************
 */
const prf_task_cbs_t* wscc_prf_itf_get(void)
{
    return &wscc_itf;
}
#endif //(BLE_WSC_CLIENT)

/// @} WSCC

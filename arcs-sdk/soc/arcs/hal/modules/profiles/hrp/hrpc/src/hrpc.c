/**
 ****************************************************************************************
 *
 * @file hrpc.c
 *
 * @brief Heart Rate Profile Collector implementation.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup HRPC
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "btip_config.h"

#if (BLE_HR_COLLECTOR)


#include "hrp_common.h"

#include "hrpc.h"
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
typedef struct hrpc_cnx_env
{
    /// Peer database discovered handle mapping
    hrs_content_t       hrs;
    /// counter used to check service uniqueness
    uint8_t             nb_svc;
    /// Client is in discovering state
    bool                discover;
} hrpc_cnx_env_t;

/// Client environment variable
typedef struct hrpc_env
{
    /// profile environment
    prf_hdr_t            prf_env;
    /// Environment variable pointer for each connections
    hrpc_cnx_env_t*      p_env[BLE_CONNECTION_MAX];
    /// GATT User local identifier
    uint8_t              user_lid;
} hrpc_env_t;


/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */

/// State machine used to retrieve heart rate service characteristics information
const prf_char_def_t hrpc_hrs_char[HRPC_CHAR_MAX] =
{
    // Heart Rate Measurement
    [HRPC_CHAR_HR_MEAS]              = { GATT_CHAR_HEART_RATE_MEAS,       ATT_REQ(PRES, MAND), PROP(N)  },
    // Body Sensor Location
    [HRPC_CHAR_BODY_SENSOR_LOCATION] = { GATT_CHAR_BODY_SENSOR_LOCATION,  ATT_REQ(PRES, OPT),  PROP(RD) },
    // Heart Rate Control Point
    [HRPC_CHAR_HR_CNTL_POINT]        = { GATT_CHAR_HEART_RATE_CNTL_POINT, ATT_REQ(PRES, OPT),  PROP(WR) },
};

/// State machine used to retrieve heart rate service characteristic description information
const prf_desc_def_t hrpc_hrs_char_desc[HRPC_DESC_MAX] =
{
    // Heart Rate Measurement client config
    [HRPC_DESC_HR_MEAS_CLI_CFG] = { GATT_DESC_CLIENT_CHAR_CFG, ATT_REQ(PRES, MAND), HRPC_CHAR_HR_MEAS },
};
/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Send discovery results to application.
 *
 * @param p_hrpc_env    Client Role task environment
 * @param conidx        Connection index
 * @param status        Response status code
 *****************************************************************************************
 */
__STATIC void hrpc_enable_cmp(hrpc_env_t* p_hrpc_env, uint8_t conidx, uint16_t status)
{
    const hrpc_cb_t* p_cb = (const hrpc_cb_t*) p_hrpc_env->prf_env.p_cb;

    if(p_hrpc_env != NULL)
    {
        hrpc_cnx_env_t* p_con_env = p_hrpc_env->p_env[conidx];
        p_cb->cb_enable_cmp(conidx, status, &(p_con_env->hrs));

        if (status != GAP_ERR_NO_ERROR)
        {
            // clean-up environment variable allocated for task instance
            ke_free(p_con_env);
            p_hrpc_env->p_env[conidx] = NULL;
        }
        else
        {
             p_con_env->discover = false;

             // Register profile handle to catch gatt indications
             gatt_cli_event_register(conidx, p_hrpc_env->user_lid, p_con_env->hrs.svc.shdl,
                                     p_con_env->hrs.svc.ehdl);
        }
    }
}

/**
 ****************************************************************************************
 * @brief Send read result to application,.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the request
 * @param[in] val_id        Value Identifier (@see enum hrpc_val_id)
 * @param[in] length        Length of data value)
 * @param[in] p_data        Pointer of data value
 ****************************************************************************************
 */
__STATIC void hrpc_read_val_cmp(uint8_t conidx, uint16_t status, uint8_t val_id, uint16_t length, const uint8_t* p_data)
{
    hrpc_env_t* p_hrpc_env = PRF_ENV_GET(HRPC, hrpc);

    if(p_hrpc_env != NULL)
    {
        const hrpc_cb_t* p_cb = (const hrpc_cb_t*) p_hrpc_env->prf_env.p_cb;
        switch (val_id)
        {
            // Read Sensor Location Characteristic value
            case (HRPC_VAL_BODY_SENSOR_LOCATION):
            {
                uint8_t sensor_loc = 0;

                if(status == GAP_ERR_NO_ERROR)
                {
                    sensor_loc = p_data[0];
                }

                p_cb->cb_read_sensor_loc_cmp(conidx, status, sensor_loc);

            } break;

            // Read Client Characteristic Configuration Descriptor value
            case (HRPC_VAL_HR_MEAS_CLI_CFG):
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

/**
 ****************************************************************************************
 * @brief Perform Value read procedure.
 *
 * @param[in] conidx        Connection index
 * @param[in] val_id        Value Identifier (@see enum hrpc_info)
 ****************************************************************************************
 */
__STATIC uint16_t hrpc_read_val(uint8_t conidx, uint16_t val_id)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    hrpc_env_t* p_hrpc_env = PRF_ENV_GET(HRPC, hrpc);

    if(p_hrpc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_hrpc_env->p_env[conidx] != NULL) && (!p_hrpc_env->p_env[conidx]->discover))
        {
            hrpc_cnx_env_t* p_con_env = p_hrpc_env->p_env[conidx];
            uint16_t hdl;
            hrs_content_t* p_hrs = &(p_con_env->hrs);

            switch(val_id)
            {
                case HRPC_VAL_BODY_SENSOR_LOCATION: { hdl = p_hrs->chars[HRPC_CHAR_BODY_SENSOR_LOCATION].val_hdl; } break;
                case HRPC_VAL_HR_MEAS_CLI_CFG:      { hdl = p_hrs->descs[HRPC_DESC_HR_MEAS_CLI_CFG].desc_hdl;     } break;
                default:                            { hdl = GATT_INVALID_HDL;                                     } break;
            }

            if(hdl == GATT_INVALID_HDL)
            {
                status = PRF_ERR_INEXISTENT_HDL;
            }
            else
            {
                // perform read request
                status = gatt_cli_read(conidx, p_hrpc_env->user_lid, val_id, hdl, 0, 0);
            }
        }
    }

    return (status);
}

/**
 ****************************************************************************************
 * @brief Unpacks measurement data and sends the indication
 * @param[in] p_hrpc_env    Environment variable
 * @param[in] conidx        Connection index
 * @param[in] p_buf         Pointer of input buffer received
 ****************************************************************************************
 */
__STATIC void hrpc_unpack_meas(hrpc_env_t* p_hrpc_env, uint8_t conidx, co_buf_t* p_buf)
{
    const hrpc_cb_t* p_cb = (const hrpc_cb_t*) p_hrpc_env->prf_env.p_cb;
    hrs_hr_meas_t meas;
    uint8_t i;
    memset(&meas, 0, sizeof(hrs_hr_meas_t));

    // Flags
    meas.flags = co_buf_data(p_buf)[0];
    co_buf_head_release(p_buf, 1);

    if (GETB(meas.flags, HRS_FLAG_HR_VALUE_FORMAT))
    {
        // Heart Rate Measurement Value 16 bits
        meas.heart_rate = co_btohs(co_read16p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 2);
    }
    else
    {
        // Heart Rate Measurement Value 8 bits
        meas.heart_rate = co_buf_data(p_buf)[0];
        co_buf_head_release(p_buf, 1);
    }

    if (GETB(meas.flags, HRS_FLAG_ENERGY_EXPENDED_PRESENT))
    {
        // Energy Expended present
        meas.energy_expended = co_btohs(co_read16p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 2);
    }

    // retrieve number of rr intervals
    meas.nb_rr_interval = co_min(co_buf_data_len(p_buf) / 2, HRS_MAX_RR_INTERVAL);

    for (i = 0; i < meas.nb_rr_interval; i++)
    {
        // RR-Intervals
        meas.rr_intervals[i] = co_btohs(co_read16p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 2);
    }

    // Inform application about received vector
    p_cb->cb_meas(conidx, &meas);
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
__STATIC void hrpc_svc_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t hdl, uint8_t disc_info,
                          uint8_t nb_att, const gatt_svc_att_t* p_atts)
{
    // Get the address of the environment
    hrpc_env_t* p_hrpc_env = PRF_ENV_GET(HRPC, hrpc);

    if(p_hrpc_env != NULL)
    {
        hrpc_cnx_env_t* p_con_env = p_hrpc_env->p_env[conidx];

        ASSERT_INFO(p_con_env != NULL, conidx, user_lid);

        if (p_con_env->nb_svc == 0)
        {
            //Even if we get multiple responses we only store 1 range
            if((disc_info == GATT_SVC_CMPLT) || (disc_info == GATT_SVC_START))
            {
                p_con_env->hrs.svc.shdl = hdl;
            }

            if((disc_info == GATT_SVC_CMPLT) || (disc_info == GATT_SVC_END))
            {
                p_con_env->hrs.svc.ehdl = hdl + nb_att -1;
            }

            // Retrieve characteristics
            prf_extract_svc_info(hdl, nb_att, p_atts,
                                 HRPC_CHAR_MAX, &hrpc_hrs_char[0],      &(p_con_env->hrs.chars[0]),
                                 HRPC_DESC_MAX, &hrpc_hrs_char_desc[0], &(p_con_env->hrs.descs[0]));
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
__STATIC void hrpc_discover_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    // Get the address of the environment
    hrpc_env_t* p_hrpc_env = PRF_ENV_GET(HRPC, hrpc);

    if(p_hrpc_env != NULL)
    {
        hrpc_cnx_env_t* p_con_env = p_hrpc_env->p_env[conidx];

        if (p_con_env->nb_svc == 1)
        {
            status = prf_check_svc_validity(HRPC_CHAR_MAX, p_con_env->hrs.chars, hrpc_hrs_char,
                                            HRPC_DESC_MAX, p_con_env->hrs.descs, hrpc_hrs_char_desc);
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

        hrpc_enable_cmp(p_hrpc_env, conidx, status);
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
__STATIC void hrpc_att_val_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t hdl, uint16_t offset,
                              co_buf_t* p_data)
{
    hrpc_read_val_cmp(conidx, GAP_ERR_NO_ERROR, (uint8_t) dummy, co_buf_data_len(p_data), co_buf_data(p_data));
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
__STATIC void hrpc_read_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    if(status != GAP_ERR_NO_ERROR)
    {
        hrpc_read_val_cmp(conidx, status, (uint8_t) dummy, 0, NULL);
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
__STATIC void hrpc_write_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    hrpc_env_t* p_hrpc_env = PRF_ENV_GET(HRPC, hrpc);

    if(p_hrpc_env != NULL)
    {
        const hrpc_cb_t* p_cb = (const hrpc_cb_t*) p_hrpc_env->prf_env.p_cb;

        switch(dummy)
        {
            // Config control
            case HRPC_CFG_INDNTF_CMD_OP_CODE:
            {
                p_cb->cb_write_cfg_cmp(conidx, status);
            } break;
            // Control point commands
            case HRPC_WR_CNTL_POINT_CMD_OP_CODE:
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
__STATIC void hrpc_att_val_evt_cb(uint8_t conidx, uint8_t user_lid, uint16_t token, uint8_t evt_type, bool complete,
                        uint16_t hdl, co_buf_t* p_data)
{
    hrpc_env_t* p_hrpc_env = PRF_ENV_GET(HRPC, hrpc);

    if(p_hrpc_env != NULL)
    {
        hrpc_cnx_env_t* p_con_env = p_hrpc_env->p_env[conidx];
        hrs_content_t* p_hrs = &(p_con_env->hrs);

        if (hdl == p_hrs->chars[HRPC_CHAR_HR_MEAS].val_hdl)
        {
            //Unpack measurement
            hrpc_unpack_meas(p_hrpc_env, conidx, p_data);
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
__STATIC void hrpc_svc_changed_cb(uint8_t conidx, uint8_t user_lid, bool out_of_sync, uint16_t start_hdl, uint16_t end_hdl)
{
    // Do Nothing
}

/// Client callback hander
__STATIC const gatt_cli_cb_t hrpc_cb =
{
    .cb_discover_cmp    = hrpc_discover_cmp_cb,
    .cb_read_cmp        = hrpc_read_cmp_cb,
    .cb_write_cmp       = hrpc_write_cmp_cb,
    .cb_att_val_get     = NULL,
    .cb_svc             = hrpc_svc_cb,
    .cb_svc_info        = NULL,
    .cb_inc_svc         = NULL,
    .cb_char            = NULL,
    .cb_desc            = NULL,
    .cb_att_val         = hrpc_att_val_cb,
    .cb_att_val_evt     = hrpc_att_val_evt_cb,
    .cb_svc_changed     = hrpc_svc_changed_cb,
};

/*
 * PROFILE NATIVE API
 ****************************************************************************************
 */

uint16_t hrpc_enable(uint8_t conidx, uint8_t con_type, const hrs_content_t* p_hrs)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    hrpc_env_t* p_hrpc_env = PRF_ENV_GET(HRPC, hrpc);

    if(p_hrpc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_hrpc_env->p_env[conidx] == NULL))
        {
            // allocate environment variable for task instance
            p_hrpc_env->p_env[conidx] = (struct hrpc_cnx_env *) ke_malloc(sizeof(struct hrpc_cnx_env), KE_MEM_ATT_DB);

            if(p_hrpc_env->p_env[conidx] != NULL)
            {
                memset(p_hrpc_env->p_env[conidx], 0, sizeof(struct hrpc_cnx_env));

                // Config connection, start discovering
                if (con_type == PRF_CON_DISCOVERY)
                {
                    uint16_t gatt_svc_uuid = GATT_SVC_HEART_RATE;

                    // start discovery
                    status = gatt_cli_discover_svc(conidx, p_hrpc_env->user_lid, 0, GATT_DISCOVER_SVC_PRIMARY_BY_UUID, true,
                                                   GATT_MIN_HDL, GATT_MAX_HDL, GATT_UUID_16, (uint8_t*) &gatt_svc_uuid);

                    // Go to DISCOVERING state
                    p_hrpc_env->p_env[conidx]->discover   = true;
                }
                // normal connection, get saved att details
                else
                {
                    memcpy(&(p_hrpc_env->p_env[conidx]->hrs), p_hrs, sizeof(hrs_content_t));
                    status = GAP_ERR_NO_ERROR;

                    // send APP confirmation that can start normal connection to TH
                    hrpc_enable_cmp(p_hrpc_env, conidx, GAP_ERR_NO_ERROR);
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

uint16_t hrpc_read_sensor_loc(uint8_t conidx)
{
    uint16_t status = hrpc_read_val(conidx, HRPC_VAL_BODY_SENSOR_LOCATION);
    return (status);
}

uint16_t hrpc_read_cfg(uint8_t conidx)
{
    uint16_t status = hrpc_read_val(conidx, HRPC_VAL_HR_MEAS_CLI_CFG);
    return (status);
}

uint16_t hrpc_write_cfg(uint8_t conidx, uint16_t cfg_val)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    hrpc_env_t* p_hrpc_env = PRF_ENV_GET(HRPC, hrpc);

    if(p_hrpc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_hrpc_env->p_env[conidx] != NULL) && (!p_hrpc_env->p_env[conidx]->discover))
        {
            hrpc_cnx_env_t* p_con_env = p_hrpc_env->p_env[conidx];
            hrs_content_t* p_hrs = &(p_con_env->hrs);
            uint16_t hdl = p_hrs->descs[HRPC_DESC_HR_MEAS_CLI_CFG].desc_hdl;

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
                status = prf_gatt_write(conidx, p_hrpc_env->user_lid, HRPC_CFG_INDNTF_CMD_OP_CODE, GATT_WRITE,
                                        hdl, sizeof(uint16_t), (uint8_t *)&cfg_val);
            }
        }
    }

    return (status);
}

uint16_t hrpc_ctnl_pt_req(uint8_t conidx, uint8_t value)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    hrpc_env_t* p_hrpc_env = PRF_ENV_GET(HRPC, hrpc);

    if(p_hrpc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_hrpc_env->p_env[conidx] != NULL) && (!p_hrpc_env->p_env[conidx]->discover))
        {
            hrpc_cnx_env_t* p_con_env = p_hrpc_env->p_env[conidx];
            hrs_content_t* p_hrs = &(p_con_env->hrs);
            uint16_t hdl = p_hrs->chars[HRPC_CHAR_HR_CNTL_POINT].val_hdl;

            if(hdl == GATT_INVALID_HDL)
            {
                status = PRF_ERR_INEXISTENT_HDL;
            }
            //write not allowed, so no point in continuing
            else if((p_hrs->chars[HRPC_CHAR_HR_CNTL_POINT].prop & PROP(WR)) == 0)
            {
                status = PRF_ERR_NOT_WRITABLE;
            }
            else
            {
                co_buf_t* p_buf = NULL;

                // allocate buffer for event transmission
                if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, 1, GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
                {
                    co_buf_data(p_buf)[0] = value;

                    status = gatt_cli_write(conidx, p_hrpc_env->user_lid, HRPC_WR_CNTL_POINT_CMD_OP_CODE, GATT_WRITE, hdl, 0, p_buf);
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
 * @brief Send a HRPC_CMP_EVT message to the task which enabled the profile
 * @param[in] conidx Connection index
 * @param[in] operation Operation
 * @param[in] status Satus
 ****************************************************************************************
 */
__STATIC void hrpc_send_cmp_evt(uint8_t conidx, uint8_t operation, uint16_t status)
{
    struct hrpc_cmp_evt *p_evt;

    // Send the message
    p_evt = KE_MSG_ALLOC(HRPC_CMP_EVT, PRF_DST_TASK(HRPC), PRF_SRC_TASK(HRPC), hrpc_cmp_evt);
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
__STATIC int hrpc_enable_req_handler(ke_msg_id_t const msgid, struct hrpc_enable_req const *p_param,
                                     ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = hrpc_enable(p_param->conidx, p_param->con_type, &(p_param->hrs));

    // send an error if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct hrpc_enable_rsp *p_rsp = KE_MSG_ALLOC(HRPC_ENABLE_RSP, src_id, dest_id, hrpc_enable_rsp);
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
 * @brief  Message Handler example
 * @param[in] msgid Id of the message received (probably unused).
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int hrpc_rd_char_cmd_handler(ke_msg_id_t const msgid, struct hrpc_rd_char_cmd const *p_param,
                                      ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status;

    switch(p_param->val_id)
    {
        case HRPC_VAL_BODY_SENSOR_LOCATION: { status = hrpc_read_sensor_loc(p_param->conidx); } break;
        case HRPC_VAL_HR_MEAS_CLI_CFG:      { status = hrpc_read_cfg(p_param->conidx) ;       } break;
        default:                            { status = PRF_ERR_INEXISTENT_HDL;                              } break;
    }

    // send error response if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct hrpc_cmp_evt *p_evt = KE_MSG_ALLOC(HRPC_CMP_EVT, src_id, dest_id, hrpc_cmp_evt);
        if(p_evt)
        {
            p_evt->conidx     = p_param->conidx;
            p_evt->operation  = HRPC_RD_CHAR_CMD_OP_CODE;
            p_evt->status     = status;

            ke_msg_send(p_evt);
        }
    }

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief Handles reception of the @ref HRPC_CFG_INDNTF_CMD message.
 * @param[in] msgid Id of the message received.
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int hrpc_cfg_indntf_cmd_handler(ke_msg_id_t const msgid,
                                         struct hrpc_cfg_indntf_cmd *p_param,
                                         ke_task_id_t const dest_id,
                                         ke_task_id_t const src_id)
{
    uint16_t status = hrpc_write_cfg(p_param->conidx, p_param->cfg_val);

    // send error response if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct hrpc_cmp_evt *p_evt = KE_MSG_ALLOC(HRPC_CMP_EVT, src_id, dest_id, hrpc_cmp_evt);
        if(p_evt)
        {
            p_evt->conidx     = p_param->conidx;
            p_evt->operation  = HRPC_CFG_INDNTF_CMD_OP_CODE;
            p_evt->status     = status;

            ke_msg_send(p_evt);
        }
    }

    return (KE_MSG_CONSUMED);
}


/**
 ****************************************************************************************
 * @brief Handles reception of the @ref HRPC_WR_CNTL_POINT_CMD message.
 * @param[in] msgid Id of the message received.
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int hrpc_wr_cntl_point_cmd_handler(ke_msg_id_t const msgid, struct hrpc_wr_cntl_point_cmd *p_param,
                                            ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = hrpc_ctnl_pt_req(p_param->conidx, p_param->val);

    // send error response if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct hrpc_cmp_evt *p_evt = KE_MSG_ALLOC(HRPC_CMP_EVT, src_id, dest_id, hrpc_cmp_evt);
        if(p_evt)
        {
            p_evt->conidx     = p_param->conidx;
            p_evt->operation  = HRPC_WR_CNTL_POINT_CMD_OP_CODE;
            p_evt->status     = status;

            ke_msg_send(p_evt);
        }
    }

    return (KE_MSG_CONSUMED);
}

/// Default State handlers definition
KE_MSG_HANDLER_TAB(hrpc)
{
    // Note: all messages must be sorted in ID ascending order

    { HRPC_ENABLE_REQ,               (ke_msg_func_t) hrpc_enable_req_handler         },
    { HRPC_RD_CHAR_CMD,              (ke_msg_func_t) hrpc_rd_char_cmd_handler        },
    { HRPC_CFG_INDNTF_CMD,           (ke_msg_func_t) hrpc_cfg_indntf_cmd_handler     },
    { HRPC_WR_CNTL_POINT_CMD,        (ke_msg_func_t) hrpc_wr_cntl_point_cmd_handler  },
};

/**
 ****************************************************************************************
 * @brief Completion of enable procedure
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 * @param[in] p_hrs         Pointer to peer database description bond data
 ****************************************************************************************
 */
__STATIC void hrpc_cb_enable_cmp(uint8_t conidx, uint16_t status, const hrs_content_t* p_hrs)
{
    // Send APP the details of the discovered attributes on HRPC
    struct hrpc_enable_rsp *p_rsp = KE_MSG_ALLOC(HRPC_ENABLE_RSP, PRF_DST_TASK(HRPC), PRF_SRC_TASK(HRPC),
                                                 hrpc_enable_rsp);
    if(p_rsp != NULL)
    {
        p_rsp->conidx = conidx;
        p_rsp->status = status;
        memcpy(&(p_rsp->hrs), p_hrs, sizeof(hrs_content_t));
        ke_msg_send(p_rsp);
    }
}

/**
 ****************************************************************************************
 * @brief Completion of read sensor location procedure.
 *
 * Wait for @see cb_read_loc_cmp execution before starting a new procedure
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 * @param[in] sensor_loc    Sensor Location
 *
 ****************************************************************************************
 */
__STATIC void hrpc_cb_read_sensor_loc_cmp(uint8_t conidx, uint16_t status, uint8_t sensor_loc)
{
    ke_task_id_t src_id  = PRF_SRC_TASK(HRPC);
    ke_task_id_t dest_id = PRF_DST_TASK(HRPC);

    if(status == GAP_ERR_NO_ERROR)
    {
        struct hrpc_rd_char_ind *p_ind = KE_MSG_ALLOC_DYN(HRPC_RD_CHAR_IND, dest_id, src_id, hrpc_rd_char_ind, 1);
        if(p_ind != NULL)
        {
            p_ind->conidx    = conidx;
            p_ind->val_id    = HRPC_VAL_BODY_SENSOR_LOCATION;
            p_ind->length    = 1;
            p_ind->value[0]  = sensor_loc;
            ke_msg_send(p_ind);
        }
    }

    hrpc_send_cmp_evt(conidx, HRPC_RD_CHAR_CMD_OP_CODE, status);
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
__STATIC void hrpc_cb_read_cfg_cmp(uint8_t conidx, uint16_t status, uint16_t cfg_val)
{
    ke_task_id_t src_id  = PRF_SRC_TASK(HRPC);
    ke_task_id_t dest_id = PRF_DST_TASK(HRPC);

    if(status == GAP_ERR_NO_ERROR)
    {
        struct hrpc_rd_char_ind *p_ind = KE_MSG_ALLOC_DYN(HRPC_RD_CHAR_IND, dest_id, src_id, hrpc_rd_char_ind, 1);
        if(p_ind != NULL)
        {
            p_ind->conidx    = conidx;
            p_ind->val_id    = HRPC_VAL_HR_MEAS_CLI_CFG;
            p_ind->length    = 2;
            co_write16p(p_ind->value, cfg_val);
            ke_msg_send(p_ind);
        }
    }

    hrpc_send_cmp_evt(conidx, HRPC_RD_CHAR_CMD_OP_CODE, status);
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
__STATIC void hrpc_cb_write_cfg_cmp(uint8_t conidx, uint16_t status)
{
    hrpc_send_cmp_evt(conidx, HRPC_CFG_INDNTF_CMD_OP_CODE, status);
}

/**
 ****************************************************************************************
 * @brief Function called when heart rate measurement information is received
 *
 * @param[in] conidx         Connection index
 * @param[in] p_meas         Pointer to heart rate measurement information
 ****************************************************************************************
 */
__STATIC void hrpc_cb_meas(uint8_t conidx, const hrs_hr_meas_t* p_meas)
{
    ke_task_id_t src_id  = PRF_SRC_TASK(HRPC);
    ke_task_id_t dest_id = PRF_DST_TASK(HRPC);

    struct hrpc_hr_meas_ind *p_ind = KE_MSG_ALLOC(HRPC_HR_MEAS_IND, dest_id, src_id, hrpc_hr_meas_ind);
    if(p_ind != NULL)
    {
        p_ind->conidx            = conidx;
        memcpy(&(p_ind->meas_val), p_meas, sizeof(hrs_hr_meas_t));
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
__STATIC void hrpc_cb_ctnl_pt_req_cmp(uint8_t conidx, uint16_t status)
{
    hrpc_send_cmp_evt(conidx, HRPC_WR_CNTL_POINT_CMD_OP_CODE, status);
}

/// Default Message handle
__STATIC const hrpc_cb_t hrpc_msg_cb =
{
     .cb_enable_cmp           = hrpc_cb_enable_cmp,
     .cb_read_sensor_loc_cmp  = hrpc_cb_read_sensor_loc_cmp,
     .cb_read_cfg_cmp         = hrpc_cb_read_cfg_cmp,
     .cb_write_cfg_cmp        = hrpc_cb_write_cfg_cmp,
     .cb_meas                 = hrpc_cb_meas,
     .cb_ctnl_pt_req_cmp      = hrpc_cb_ctnl_pt_req_cmp,
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
__STATIC uint16_t hrpc_init(prf_data_t* p_env, uint16_t* p_start_hdl, uint8_t sec_lvl, uint8_t user_prio,
                          const void* p_params, const hrpc_cb_t* p_cb)
{
    uint8_t conidx;
    // DB Creation Status
    uint16_t status = GAP_ERR_NO_ERROR;
    uint8_t user_lid = GATT_INVALID_USER_LID;

    do
    {
        hrpc_env_t* p_hrpc_env;

        #if (BLE_HL_MSG_API)
        if(p_cb == NULL)
        {
            p_cb = &(hrpc_msg_cb);
        }
        #endif // (BLE_HL_MSG_API)

        if(   (p_params == NULL) || (p_cb == NULL) || (p_cb->cb_enable_cmp == NULL)
           || (p_cb->cb_read_sensor_loc_cmp == NULL) || (p_cb->cb_read_cfg_cmp == NULL) || (p_cb->cb_write_cfg_cmp == NULL)
           || (p_cb->cb_meas == NULL) || (p_cb->cb_ctnl_pt_req_cmp == NULL))
        {
            status = GAP_ERR_INVALID_PARAM;
            break;
        }

        // register HRPC user
        status = gatt_user_cli_register(L2CAP_LE_MTU_MIN, user_prio, &hrpc_cb, &user_lid);
        if(status != GAP_ERR_NO_ERROR) break;

        //-------------------- allocate memory required for the profile  ---------------------
        p_hrpc_env = (hrpc_env_t*) ke_malloc(sizeof(hrpc_env_t), KE_MEM_ATT_DB);

        if(p_hrpc_env != NULL)
        {
            // allocate HRPC required environment variable
            p_env->p_env = (prf_hdr_t *) p_hrpc_env;

            // initialize environment variable
            p_hrpc_env->prf_env.p_cb    = p_cb;
            #if (BLE_HL_MSG_API)
            p_env->desc.msg_handler_tab = hrpc_msg_handler_tab;
            p_env->desc.msg_cnt         = ARRAY_LEN(hrpc_msg_handler_tab);
            #endif // (BLE_HL_MSG_API)

            p_hrpc_env->user_lid = user_lid;
            for (conidx = 0; conidx < BLE_CONNECTION_MAX; conidx++)
            {
                p_hrpc_env->p_env[conidx] = NULL;
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
__STATIC uint16_t hrpc_destroy(prf_data_t *p_env, uint8_t reason)
{
    uint16_t status = GAP_ERR_NO_ERROR;
    hrpc_env_t* p_hrpc_env = (hrpc_env_t*) p_env->p_env;

    if(reason != PRF_DESTROY_RESET)
    {
        status = gatt_user_unregister(p_hrpc_env->user_lid);
    }

    if(status == GAP_ERR_NO_ERROR)
    {
        uint8_t conidx;

        // cleanup environment variable for each task instances
        for (conidx = 0; conidx < BLE_CONNECTION_MAX; conidx++)
        {
            if (p_hrpc_env->p_env[conidx] != NULL)
            {
                ke_free(p_hrpc_env->p_env[conidx]);
            }
        }

        // free profile environment variables
        p_env->p_env = NULL;
        ke_free(p_hrpc_env);
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
__STATIC void hrpc_con_create(prf_data_t *p_env, uint8_t conidx, const gap_con_param_t* p_con_param)
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
__STATIC void hrpc_con_cleanup(prf_data_t *p_env, uint8_t conidx, uint16_t reason)
{
    hrpc_env_t* p_hrpc_env = (hrpc_env_t*) p_env->p_env;

    // clean-up environment variable allocated for task instance
    if (p_hrpc_env->p_env[conidx] != NULL)
    {
        ke_free(p_hrpc_env->p_env[conidx]);
        p_hrpc_env->p_env[conidx] = NULL;
    }
}

/// HRPC Task interface required by profile manager
const prf_task_cbs_t hrpc_itf =
{
    .cb_init          = (prf_init_cb) hrpc_init,
    .cb_destroy       = hrpc_destroy,
    .cb_con_create    = hrpc_con_create,
    .cb_con_cleanup   = hrpc_con_cleanup,
    .cb_con_upd       = NULL,
};

/**
 ****************************************************************************************
 * @brief Retrieve client profile interface
 *
 * @return Client profile interface
 ****************************************************************************************
 */
const prf_task_cbs_t* hrpc_prf_itf_get(void)
{
    return &hrpc_itf;
}


#endif /* (BLE_HR_COLLECTOR) */

/// @} HRPC

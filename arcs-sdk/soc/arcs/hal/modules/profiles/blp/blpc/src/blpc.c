/**
 ****************************************************************************************
 *
 * @file blpc.c
 *
 * @brief Blood Pressure Profile Collector implementation.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup BLPC Blood Pressure Profile Collector
 * @ingroup BLP
 * @brief Blood Pressure Profile Collector
 *
 * The BLPC is responsible for providing Blood Pressure Profile Collector functionalities
 * to upper layer module or application. The device using this profile takes the role
 * of Blood Pressure Profile Collector.
 *
 * Blood Pressure Profile Collector. (BLPC): A BLPC (e.g. PC, phone, etc)
 * is the term used by this profile to describe a device that can interpret blood pressure
 * measurement in a way suitable to the user application.
 *
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "btip_config.h"

#if (BLE_BP_COLLECTOR)

#include "blpc.h"
#include "gap.h"
#include "gatt.h"

#include "co_utils.h"
#include "co_endian.h"

#include <string.h>
#include "ke_mem.h"


/*
 * DEFINES
 ****************************************************************************************
 */

/// Maximum number of Client task instances

/// Internal codes for reading a BPS or DIS characteristic with one single request
enum
{
    ///Read BPS Blood pressure Measurement
    BLPC_RD_BPS_BP_MEAS          = BLPC_CHAR_BP_MEAS,
    ///Read BPS Intermdiate Cuff Pressure
    BLPC_RD_BPS_CP_MEAS          = BLPC_CHAR_CP_MEAS,
    ///Read BPS Blood pressure Features
    BLPC_RD_BPS_FEATURE          = BLPC_CHAR_BP_FEATURE,

    ///Read BPS Blood pressure Measurement Client Cfg. Desc
    BLPC_RD_BPS_BP_MEAS_CFG      = (BLPC_DESC_MASK | BLPC_DESC_BP_MEAS_CLI_CFG),
    ///Read BPS Intermdiate Cuff Pressure Client Cfg. Desc
    BLPC_RD_BPS_CP_MEAS_CFG      = (BLPC_DESC_MASK | BLPC_DESC_IC_MEAS_CLI_CFG),
};

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */


/// Environment variable for each Connections
typedef struct blpc_cnx_env
{
    /// Peer database discovered handle mapping
    bps_content_t        bps;
    /// counter used to check service uniqueness
    uint8_t             nb_svc;
    /// Client is in discovering state
    bool                discover;
} blpc_cnx_env_t;

/// Client environment variable
typedef struct blpc_env
{
    /// profile environment
    prf_hdr_t            prf_env;
    /// Environment variable pointer for each connections
    blpc_cnx_env_t*      p_env[BLE_CONNECTION_MAX];
    /// GATT User local identifier
    uint8_t              user_lid;
} blpc_env_t;


/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */

/// State machine used to retrieve blood pressure service characteristics information
const prf_char_def_t blpc_bps_char[BLPC_CHAR_MAX] =
{
    // Blood Pressure Measurement
    [BLPC_CHAR_BP_MEAS]    = { GATT_CHAR_BLOOD_PRESSURE_MEAS,        ATT_REQ(PRES, MAND), PROP(I)  },
    // Intermediate Cuff pressure
    [BLPC_CHAR_CP_MEAS]    = { GATT_CHAR_INTERMEDIATE_CUFF_PRESSURE, ATT_REQ(PRES, OPT),  PROP(N)  },
    // Blood Pressure Feature
    [BLPC_CHAR_BP_FEATURE] = { GATT_CHAR_BLOOD_PRESSURE_FEATURE,     ATT_REQ(PRES, MAND), PROP(RD) },
};

/// State machine used to retrieve blood pressure service characteristic description information
const prf_desc_def_t blpc_bps_char_desc[BLPC_DESC_MAX] =
{
    // Blood Pressure Measurement client config
    [BLPC_DESC_BP_MEAS_CLI_CFG] = { GATT_DESC_CLIENT_CHAR_CFG, ATT_REQ(PRES, MAND), BLPC_CHAR_BP_MEAS },
    // Intermediate Cuff pressure client config
    [BLPC_DESC_IC_MEAS_CLI_CFG] = { GATT_DESC_CLIENT_CHAR_CFG, ATT_REQ(PRES, MAND), BLPC_CHAR_CP_MEAS },
};

/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */


/**
 ****************************************************************************************
 * @brief Unpack Blood pressure measurement data into a comprehensive structure.
 *
 * @param[in]  p_buf      Pointer of data buffer received
 * @param[out] p_meas_val Pointer to Blood pressure measurement structure destination
 ****************************************************************************************
 */
__STATIC void blpc_unpack_meas_value(co_buf_t* p_buf, bps_bp_meas_t *p_meas_val)
{
    memset(p_meas_val, 0, sizeof(bps_bp_meas_t));

    // blood pressure measurement flags
    p_meas_val->flags = co_buf_data(p_buf)[0];
    co_buf_head_release(p_buf, 1);

    // Blood Pressure Measurement Compound Value - Systolic
    p_meas_val->systolic = co_btohs(co_read16p(co_buf_data(p_buf)));
    co_buf_head_release(p_buf, 2);

    // Blood Pressure Measurement Compound Value - Diastolic (mmHg)
    p_meas_val->diastolic = co_btohs(co_read16p(co_buf_data(p_buf)));
    co_buf_head_release(p_buf, 2);

    //  Blood Pressure Measurement Compound Value - Mean Arterial Pressure (mmHg)
    p_meas_val->mean_arterial_pressure = co_btohs(co_read16p(co_buf_data(p_buf)));
    co_buf_head_release(p_buf, 2);

    // time flag set
    if (GETB(p_meas_val->flags, BPS_MEAS_FLAG_TIME_STAMP))
    {
        prf_unpack_date_time(p_buf, &(p_meas_val->time_stamp));
    }

    // pulse rate flag set
    if (GETB(p_meas_val->flags, BPS_MEAS_PULSE_RATE))
    {
        p_meas_val->pulse_rate = co_btohs(co_read16p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 2);
    }

    // User ID flag set
    if (GETB(p_meas_val->flags, BPS_MEAS_USER_ID))
    {
        p_meas_val->user_id = co_buf_data(p_buf)[0];
        co_buf_head_release(p_buf, 1);
    }

    // measurement status flag set
    if (GETB(p_meas_val->flags, BPS_MEAS_MEAS_STATUS))
    {
        p_meas_val->meas_status = co_btohs(co_read16p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 2);
    }
}

/**
 ****************************************************************************************
 * @brief Send discovery results to application.
 *
 * @param p_blpc_env    Client Role task environment
 * @param conidx        Connection index
 * @param status        Response status code
 *****************************************************************************************
 */
__STATIC void blpc_enable_cmp(blpc_env_t* p_blpc_env, uint8_t conidx, uint16_t status)
{
    const blpc_cb_t* p_cb = (const blpc_cb_t*) p_blpc_env->prf_env.p_cb;

    if(p_blpc_env != NULL)
    {
        blpc_cnx_env_t* p_con_env = p_blpc_env->p_env[conidx];
        p_cb->cb_enable_cmp(conidx, status, &(p_con_env->bps));

        if (status != GAP_ERR_NO_ERROR)
        {
            // clean-up environment variable allocated for task instance
            ke_free(p_con_env);
            p_blpc_env->p_env[conidx] = NULL;
        }
        else
        {
             p_con_env->discover = false;

             // Register profile handle to catch gatt indications
             gatt_cli_event_register(conidx, p_blpc_env->user_lid, p_con_env->bps.svc.shdl,
                                     p_con_env->bps.svc.ehdl);
        }
    }
}

/**
 ****************************************************************************************
 * @brief Send read result to application,.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the request
 * @param[in] val_id        Value Identifier
 * @param[in] length        Length of data value)
 * @param[in] p_data        Pointer of data value
 ****************************************************************************************
 */
__STATIC void blpc_read_val_cmp(uint8_t conidx, uint16_t status, uint8_t val_id, uint16_t length, const uint8_t* p_data)
{
    blpc_env_t* p_blpc_env = PRF_ENV_GET(BLPC, blpc);

    if(p_blpc_env != NULL)
    {
        const blpc_cb_t* p_cb = (const blpc_cb_t*) p_blpc_env->prf_env.p_cb;

        p_cb->cb_read_char_cmp(conidx, status, val_id, length, p_data);
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
 * @param[in] bpsc_info     Discovery information (@see enum gatt_svc_disc_info)
 * @param[in] nb_att        Number of attributes
 * @param[in] p_atts        Pointer to attribute information present in a service
 ****************************************************************************************
 */
__STATIC void blpc_svc_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t hdl, uint8_t bpsc_info,
                          uint8_t nb_att, const gatt_svc_att_t* p_atts)
{
    // Get the address of the environment
    blpc_env_t* p_blpc_env = PRF_ENV_GET(BLPC, blpc);

    if(p_blpc_env != NULL)
    {
        blpc_cnx_env_t* p_con_env = p_blpc_env->p_env[conidx];

        ASSERT_INFO(p_con_env != NULL, conidx, user_lid);

        if (p_con_env->nb_svc == 0)
        {
            //Even if we get multiple responses we only store 1 range
            if((bpsc_info == GATT_SVC_CMPLT) || (bpsc_info == GATT_SVC_START))
            {
                p_con_env->bps.svc.shdl = hdl;
            }

            if((bpsc_info == GATT_SVC_CMPLT) || (bpsc_info == GATT_SVC_END))
            {
                p_con_env->bps.svc.ehdl = hdl + nb_att -1;
            }

            // Retrieve characteristics
            prf_extract_svc_info(hdl, nb_att, p_atts,
                                 BLPC_CHAR_MAX, &blpc_bps_char[0],      &(p_con_env->bps.chars[0]),
                                 BLPC_DESC_MAX, &blpc_bps_char_desc[0], &(p_con_env->bps.descs[0]));
        }

        if((bpsc_info == GATT_SVC_CMPLT) || (bpsc_info == GATT_SVC_END))
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
__STATIC void blpc_discover_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    // Get the address of the environment
    blpc_env_t* p_blpc_env = PRF_ENV_GET(BLPC, blpc);

    if(p_blpc_env != NULL)
    {
        blpc_cnx_env_t* p_con_env = p_blpc_env->p_env[conidx];

        if (p_con_env->nb_svc ==  1)
        {
            status = prf_check_svc_validity(BLPC_CHAR_MAX, p_con_env->bps.chars, blpc_bps_char,
                                            BLPC_DESC_MAX, p_con_env->bps.descs, blpc_bps_char_desc);
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

        blpc_enable_cmp(p_blpc_env, conidx, status);
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
__STATIC void blpc_att_val_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t hdl, uint16_t offset,
                              co_buf_t* p_data)
{
    blpc_read_val_cmp(conidx, GAP_ERR_NO_ERROR, (uint8_t) dummy, co_buf_data_len(p_data), co_buf_data(p_data));
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
__STATIC void blpc_read_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    if(status != GAP_ERR_NO_ERROR)
    {
        blpc_read_val_cmp(conidx, status, (uint8_t) dummy, 0, NULL);
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
__STATIC void blpc_write_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    blpc_env_t* p_blpc_env = PRF_ENV_GET(BLPC, blpc);

    if(p_blpc_env != NULL)
    {
        const blpc_cb_t* p_cb = (const blpc_cb_t*) p_blpc_env->prf_env.p_cb;
        p_cb->cb_write_ntf_ind_cfg_cmp(conidx, status, (uint8_t) dummy);
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
__STATIC void blpc_att_val_evt_cb(uint8_t conidx, uint8_t user_lid, uint16_t token, uint8_t evt_type, bool complete,
                        uint16_t hdl, co_buf_t* p_data)
{
    // Get the address of the environment
    blpc_env_t* p_blpc_env = PRF_ENV_GET(BLPC, blpc);

    if(p_blpc_env != NULL)
    {
        blpc_cnx_env_t* p_con_env = p_blpc_env->p_env[conidx];

        if(   (p_con_env != NULL)
           && (   (hdl == p_con_env->bps.chars[BLPC_CHAR_CP_MEAS].val_hdl)
               || (hdl == p_con_env->bps.chars[BLPC_CHAR_BP_MEAS].val_hdl)))
        {
            bps_bp_meas_t meas;
            bool  stable  = (hdl == p_con_env->bps.chars[BLPC_CHAR_BP_MEAS].val_hdl);
            const blpc_cb_t* p_cb = (const blpc_cb_t*) p_blpc_env->prf_env.p_cb;

            // unpack blood pressure measurement.
            blpc_unpack_meas_value(p_data, &(meas));

            // provide measurement to application
            p_cb->cb_meas(conidx, stable, &(meas));
        }
    }

    // confirm event reception
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
__STATIC void blpc_svc_changed_cb(uint8_t conidx, uint8_t user_lid, bool out_of_sync, uint16_t start_hdl, uint16_t end_hdl)
{
    // Do nothing
}

/// Client callback hander
__STATIC const gatt_cli_cb_t blpc_cb =
{
    .cb_discover_cmp    = blpc_discover_cmp_cb,
    .cb_read_cmp        = blpc_read_cmp_cb,
    .cb_write_cmp       = blpc_write_cmp_cb,
    .cb_att_val_get     = NULL,
    .cb_svc             = blpc_svc_cb,
    .cb_svc_info        = NULL,
    .cb_inc_svc         = NULL,
    .cb_char            = NULL,
    .cb_desc            = NULL,
    .cb_att_val         = blpc_att_val_cb,
    .cb_att_val_evt     = blpc_att_val_evt_cb,
    .cb_svc_changed     = blpc_svc_changed_cb,
};

/*
 * PROFILE NATIVE API
 ****************************************************************************************
 */

uint16_t blpc_enable(uint8_t conidx, uint8_t con_type, const bps_content_t* p_bps)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    blpc_env_t* p_blpc_env = PRF_ENV_GET(BLPC, blpc);

    if(p_blpc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_blpc_env->p_env[conidx] == NULL))
        {
            // allocate environment variable for task instance
            p_blpc_env->p_env[conidx] = (struct blpc_cnx_env *) ke_malloc(sizeof(struct blpc_cnx_env), KE_MEM_ATT_DB);

            if(p_blpc_env->p_env[conidx] != NULL)
            {
                // Config connection, start discovering
                if (con_type == PRF_CON_DISCOVERY)
                {
                    uint16_t gatt_svc_uuid = GATT_SVC_BLOOD_PRESSURE;
                    memset(p_blpc_env->p_env[conidx], 0, sizeof(struct blpc_cnx_env));

                    // start discovery
                    status = gatt_cli_discover_svc(conidx, p_blpc_env->user_lid, 0, GATT_DISCOVER_SVC_PRIMARY_BY_UUID, true,
                                                   GATT_MIN_HDL, GATT_MAX_HDL, GATT_UUID_16, (uint8_t*) &gatt_svc_uuid);

                    // Go to DISCOVERING state
                    p_blpc_env->p_env[conidx]->discover = true;
                }
                // normal connection, get saved att details
                else
                {
                    memcpy(&(p_blpc_env->p_env[conidx]->bps), p_bps, sizeof(bps_content_t));
                    status = GAP_ERR_NO_ERROR;

                    // send APP confirmation that can start normal connection to TH
                    blpc_enable_cmp(p_blpc_env, conidx, GAP_ERR_NO_ERROR);
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

uint16_t blpc_read_char(uint8_t conidx, uint8_t char_code)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    blpc_env_t* p_blpc_env = PRF_ENV_GET(BLPC, blpc);

    if(p_blpc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_blpc_env->p_env[conidx] != NULL) && !(p_blpc_env->p_env[conidx]->discover))
        {
            uint16_t search_hdl = GATT_INVALID_HDL;

            if ((char_code & BLPC_DESC_MASK) == BLPC_DESC_MASK)
            {
                search_hdl = p_blpc_env->p_env[conidx]->bps.descs[char_code & ~BLPC_DESC_MASK].desc_hdl;
            }
            else
            {
                search_hdl = p_blpc_env->p_env[conidx]->bps.chars[char_code].val_hdl;
            }

            //Check if handle is viable
            if (search_hdl != GATT_INVALID_HDL)
            {
                // perform read request
                status = gatt_cli_read(conidx, p_blpc_env->user_lid, char_code, search_hdl, 0, 0);
            }
            else
            {
                // invalid handle requested
                status = PRF_ERR_INEXISTENT_HDL;
            }
        }
    }

    return (status);
}

uint16_t blpc_write_ntf_ind_cfg(uint8_t conidx, uint8_t char_code, uint16_t cfg_val)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    blpc_env_t* p_blpc_env = PRF_ENV_GET(BLPC, blpc);

    if(p_blpc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_blpc_env->p_env[conidx] != NULL) && !(p_blpc_env->p_env[conidx]->discover))
        {
            uint16_t hdl = GATT_INVALID_HDL;

            // get handle of the configuration characteristic to set and check if value matches property
            switch (char_code)
            {
                case BLPC_CHAR_BP_MEAS://can only IND
                {
                    if ((cfg_val != PRF_CLI_STOP_NTFIND)  && (cfg_val != PRF_CLI_START_IND))
                    {
                        status = PRF_ERR_INVALID_PARAM;
                        break;
                    }
                    hdl = p_blpc_env->p_env[conidx]->bps.descs[BLPC_DESC_BP_MEAS_CLI_CFG].desc_hdl;
                } break;

                case BLPC_CHAR_CP_MEAS://can only NTF
                {
                    if ((cfg_val != PRF_CLI_STOP_NTFIND)  && (cfg_val != PRF_CLI_START_NTF))
                    {
                        status = PRF_ERR_INVALID_PARAM;
                        break;
                    }

                    hdl = p_blpc_env->p_env[conidx]->bps.descs[BLPC_DESC_IC_MEAS_CLI_CFG].desc_hdl;
                } break;

                default:
                {
                    status = PRF_ERR_INEXISTENT_HDL;
                } break;
            }

            //check if the handle value exists
            if (hdl != GATT_INVALID_HDL)
            {
                // Force endianess
                cfg_val = co_htobs(cfg_val);
                status = prf_gatt_write(conidx, p_blpc_env->user_lid, char_code, GATT_WRITE,
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
 * @brief Send an BLPC_CMP_EVT message to a requester.
 *
 * @param[in] conidx        Connection index
 * @param[in] operation     Code of the completed operation
 * @param[in] status        Status of the request
 ****************************************************************************************
 */
void blpc_send_cmp_evt(uint8_t conidx, uint8_t operation, uint16_t status)
{
    struct blpc_cmp_evt *p_evt;
    ke_task_id_t src_id = PRF_SRC_TASK(BLPC);
    ke_task_id_t dest_id = PRF_DST_TASK(BLPC);

    // Send the message to the application
    p_evt = KE_MSG_ALLOC(BLPC_CMP_EVT, dest_id, src_id, blpc_cmp_evt);

    if(p_evt != NULL)
    {
        p_evt->conidx    = conidx;
        p_evt->operation = operation;
        p_evt->status    = status;

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
__STATIC int blpc_enable_req_handler(ke_msg_id_t const msgid, struct blpc_enable_req const *p_param,
                                     ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = blpc_enable(p_param->conidx, p_param->con_type, &(p_param->bps));

    // send an error if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct blpc_enable_rsp *p_rsp = KE_MSG_ALLOC(BLPC_ENABLE_RSP, src_id, dest_id, blpc_enable_rsp);
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
 * @brief Handles reception of the @ref BLPC_CFG_INDNTF_CMD message.
 * It allows configuration of the peer ind/ntf/stop characteristic for a specified characteristic.
 * Will return an error code if that cfg char does not exist.
 * @param[in] msgid Id of the message received (probably unused).
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int blpc_cfg_indntf_cmd_handler(ke_msg_id_t const msgid, struct blpc_cfg_indntf_cmd const *p_param,
                                         ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = blpc_write_ntf_ind_cfg(p_param->conidx, p_param->char_code, p_param->cfg_val);

    if (status != GAP_ERR_NO_ERROR)
    {
        blpc_send_cmp_evt(p_param->conidx, BLPC_CFG_INDNTF_CMD_OP_CODE, status);
    }

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief Handles reception of the @ref BLPC_RD_CHAR_CMD message.
 * Check if the handle exists in profile(already discovered) and send request, otherwise
 * error to APP.
 * @param[in] msgid Id of the message received (probably unused).
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int blpc_rd_char_cmd_handler(ke_msg_id_t const msgid, struct blpc_rd_char_cmd const *p_param,
                                      ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = blpc_read_char(p_param->conidx, p_param->char_code);

    if (status != GAP_ERR_NO_ERROR)
    {
        blpc_send_cmp_evt(p_param->conidx, BLPC_RD_CHAR_CMD_OP_CODE, status);
    }

    return (KE_MSG_CONSUMED);
}

/// Default State handlers definition
KE_MSG_HANDLER_TAB(blpc)
{
    // Note: all messages must be sorted in ID ascending order

    {BLPC_ENABLE_REQ,               (ke_msg_func_t)blpc_enable_req_handler},
    {BLPC_RD_CHAR_CMD,              (ke_msg_func_t)blpc_rd_char_cmd_handler},
    {BLPC_CFG_INDNTF_CMD,           (ke_msg_func_t)blpc_cfg_indntf_cmd_handler},
};

/**
 ****************************************************************************************
 * @brief This function is called when GATT server user has initiated event send to peer
 *        device or if an error occurs.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Client Enable status (@see enum hl_err)
 * @param[in] p_bps         Pointer to bond data information that describe peer database
 ****************************************************************************************
 */
void blpc_enable_cmp_handler(uint8_t conidx, uint16_t status, const bps_content_t* p_bps)
{
    // Send APP the details of the discovered attributes on BLPC
    struct blpc_enable_rsp *p_rsp = KE_MSG_ALLOC(BLPC_ENABLE_RSP, PRF_DST_TASK(BLPC), PRF_SRC_TASK(BLPC),
                                                 blpc_enable_rsp);
    if(p_rsp != NULL)
    {
        p_rsp->conidx = conidx;
        p_rsp->status = status;
        memcpy(&(p_rsp->bps), p_bps, sizeof(bps_content_t));
        ke_msg_send(p_rsp);
    }
}

/**
 ****************************************************************************************
 * @brief Inform that characteristic read procedure is over
 *
 * @param[in] conidx      Connection index
 * @param[in] status      Status of the procedure execution (@see enum hl_err)
 * @param[in] char_code   Characteristic value code
 * @param[in] length      Value length
 * @param[in] p_value     Pointer to value data
 ****************************************************************************************
 */
void blpc_cb_read_char_cmp(uint8_t conidx, uint16_t status, uint8_t char_code, uint16_t length, const uint8_t* p_value)
{
    if(status == GAP_ERR_NO_ERROR)
    {
        ke_task_id_t src_id  = PRF_SRC_TASK(BLPC);
        ke_task_id_t dest_id = PRF_DST_TASK(BLPC);
        struct blpc_rd_char_ind *p_ind = KE_MSG_ALLOC_DYN(BLPC_RD_CHAR_IND, dest_id, src_id, blpc_rd_char_ind, length);

        if(p_ind != NULL)
        {
            p_ind->conidx      = conidx;
            p_ind->char_code   = char_code;
            p_ind->length      = length;
            memcpy(p_ind->value, p_value, length);
            ke_msg_send(p_ind);
        }
    }

    blpc_send_cmp_evt(conidx, BLPC_RD_CHAR_CMD_OP_CODE, status);
}

/**
 ****************************************************************************************
 * @brief Inform that indication configuration write procedure is over
 *
 * @param[in] conidx        Connection index
 * @param[in] char_code     Own code for differentiating between blood pressure and
 *                          intermediate cuff pressure measurements
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 ****************************************************************************************
 */
void blpc_cb_write_ntf_ind_cfg_cmp(uint8_t conidx, uint16_t status, uint8_t char_code)
{
    blpc_send_cmp_evt(conidx, BLPC_CFG_INDNTF_CMD_OP_CODE, status);
}

/**
 ****************************************************************************************
 * @brief Function called when new blood pressure measurement information is received
 *
 * @param[in] conidx         Connection index
 * @param[in] flag_interm_cp Flag indicating if it is a intermediary cuff pressure measurement (0) or
 *                           stable blood pressure measurement (1).
 * @param[in] p_meas         Pointer to body configuration measurement information
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Function called when new blood pressure measurement information is received
 *
 * @param[in] conidx         Connection index
 * @param[in] stable         Flag indicating if it is a intermediary cuff pressure measurement (false)
 *                           or stable blood pressure measurement (true).
 * @param[in] p_meas         Pointer to body configuration measurement information
 ****************************************************************************************
 */
void blpc_cb_meas(uint8_t conidx, bool stable, const bps_bp_meas_t* p_meas)
{
    ke_task_id_t src_id  = PRF_SRC_TASK(BLPC);
    ke_task_id_t dest_id = PRF_DST_TASK(BLPC);
    struct blpc_bp_meas_ind *p_ind = KE_MSG_ALLOC(BLPC_BP_MEAS_IND, dest_id, src_id, blpc_bp_meas_ind);

    if(p_ind != NULL)
    {
        p_ind->conidx            = conidx;
        p_ind->flag_interm_cp    = (stable ? BPS_STABLE_MEAS : BPS_INTERM_CP_MEAS);
        memcpy(&(p_ind->meas_val), p_meas, sizeof(bps_bp_meas_t));
        ke_msg_send(p_ind);
    }
}

/// Default Message handle
__STATIC const blpc_cb_t blpc_msg_cb =
{
    .cb_enable_cmp            = blpc_enable_cmp_handler,
    .cb_read_char_cmp         = blpc_cb_read_char_cmp,
    .cb_write_ntf_ind_cfg_cmp = blpc_cb_write_ntf_ind_cfg_cmp,
    .cb_meas                  = blpc_cb_meas,
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
__STATIC uint16_t blpc_init(prf_data_t* p_env, uint16_t* p_start_hdl, uint8_t sec_lvl, uint8_t user_prio,
                          const void* p_params, const blpc_cb_t* p_cb)
{
    uint8_t conidx;
    // DB Creation Status
    uint16_t status = GAP_ERR_NO_ERROR;
    uint8_t user_lid = GATT_INVALID_USER_LID;

    do
    {
        blpc_env_t* p_blpc_env;

        #if (BLE_HL_MSG_API)
        if(p_cb == NULL)
        {
            p_cb = &(blpc_msg_cb);
        }
        #endif // (BLE_HL_MSG_API)

        if(   (p_params == NULL) || (p_cb == NULL) || (p_cb->cb_enable_cmp == NULL) || (p_cb->cb_read_char_cmp == NULL)
           || (p_cb->cb_write_ntf_ind_cfg_cmp == NULL) || (p_cb->cb_meas == NULL))
        {
            status = GAP_ERR_INVALID_PARAM;
            break;
        }

        // register BLPC user
        status = gatt_user_cli_register(L2CAP_LE_MTU_MIN, user_prio, &blpc_cb, &user_lid);
        if(status != GAP_ERR_NO_ERROR) break;

        //-------------------- allocate memory required for the profile  ---------------------
        p_blpc_env = (blpc_env_t*) ke_malloc(sizeof(blpc_env_t), KE_MEM_ATT_DB);

        if(p_blpc_env != NULL)
        {
            // allocate BLPC required environment variable
            p_env->p_env = (prf_hdr_t *) p_blpc_env;

            // initialize environment variable
            p_blpc_env->prf_env.p_cb    = p_cb;
            #if (BLE_HL_MSG_API)
            p_env->desc.msg_handler_tab = blpc_msg_handler_tab;
            p_env->desc.msg_cnt         = ARRAY_LEN(blpc_msg_handler_tab);
            #endif // (BLE_HL_MSG_API)

            p_blpc_env->user_lid = user_lid;
            for (conidx = 0; conidx < BLE_CONNECTION_MAX; conidx++)
            {
                p_blpc_env->p_env[conidx] = NULL;
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
__STATIC uint16_t blpc_destroy(prf_data_t *p_env, uint8_t reason)
{
    uint16_t status = GAP_ERR_NO_ERROR;
    blpc_env_t* p_blpc_env = (blpc_env_t*) p_env->p_env;

    if(reason != PRF_DESTROY_RESET)
    {
        status = gatt_user_unregister(p_blpc_env->user_lid);
    }

    if(status == GAP_ERR_NO_ERROR)
    {
        uint8_t idx;

        // cleanup environment variable for each task instances
        for (idx = 0; idx < BLE_CONNECTION_MAX; idx++)
        {
            if (p_blpc_env->p_env[idx] != NULL)
            {
                ke_free(p_blpc_env->p_env[idx]);
            }
        }

        // free profile environment variables
        p_env->p_env = NULL;
        ke_free(p_blpc_env);
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
__STATIC void blpc_con_create(prf_data_t *p_env, uint8_t conidx, const gap_con_param_t* p_con_param)
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
__STATIC void blpc_con_cleanup(prf_data_t *p_env, uint8_t conidx, uint16_t reason)
{
    blpc_env_t* p_blpc_env = (blpc_env_t*) p_env->p_env;

    // clean-up environment variable allocated for task instance
    if (p_blpc_env->p_env[conidx] != NULL)
    {
        ke_free(p_blpc_env->p_env[conidx]);
        p_blpc_env->p_env[conidx] = NULL;
    }
}

/// BLPC Task interface required by profile manager
const prf_task_cbs_t blpc_itf =
{
    .cb_init          = (prf_init_cb) blpc_init,
    .cb_destroy       = blpc_destroy,
    .cb_con_create    = blpc_con_create,
    .cb_con_cleanup   = blpc_con_cleanup,
    .cb_con_upd       = NULL,
};

/**
 ****************************************************************************************
 * @brief Retrieve client profile interface
 *
 * @return Client profile interface
 ****************************************************************************************
 */
const prf_task_cbs_t* blpc_prf_itf_get(void)
{
    return &blpc_itf;
}

#endif /* (BLE_BP_COLLECTOR) */

/// @} BLPC

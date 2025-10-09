/**
 ****************************************************************************************
 *
 * @file htpc.c
 *
 * @brief Health Thermometer Profile Collector implementation.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup HTPC
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "btip_config.h"

#if (BLE_HT_COLLECTOR)
#include "htpc.h"
#include "gap.h"
#include "gapc.h"
#include "gatt.h"

#include "ke_mem.h"
#include "co_utils.h"
#include "co_endian.h"
#include "ke_mem.h"
#include <string.h>

/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */

/// Maximum number of task instances

#define HTPC_PACKED_TEMP_MIN_LEN        (5)
#define HTPC_PACKED_TEMP_MAX_LEN        (13)

/// Possible states of the HTPC task
enum htpc_state
{
    ///Idle state
    HTPC_IDLE,
    ///Discovering state
    HTPC_DISCOVERING,
    ///Number of defined states.
    HTPC_STATE_MAX
};

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */


/// Health Temperature Collector environment variable per connection
typedef struct htpc_cnx_env
{
    /// HTS handle values
    htpc_hts_content_t  hts;
    /// Last service for which something was discovered
    uint8_t             last_svc_req;
    /// counter used to check service uniqueness
    uint8_t             nb_svc;
    /// used to store if measurement context
    uint8_t             meas_ctx_en;
    /// Connection state
    uint8_t             state;
} htpc_cnx_env_t;

/// Client environment variable
typedef struct htpc_env
{
    /// profile environment
    prf_hdr_t           prf_env;
    /// Environment variable pointer for each connections
    htpc_cnx_env_t*     p_env[BLE_CONNECTION_MAX];
    /// GATT User local identifier
    uint8_t             user_lid;
} htpc_env_t;


/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */

/// State machine used to retrieve Health Thermometer service characteristics information
const prf_char_def_t htpc_hts_char[HTPC_CHAR_HTS_MAX] =
{
    /// Temperature Measurement
    [HTPC_CHAR_HTS_TEMP_MEAS]        = {GATT_CHAR_TEMPERATURE_MEAS,     ATT_REQ(PRES, MAND), PROP(I)},
    /// Temperature Type
    [HTPC_CHAR_HTS_TEMP_TYPE]        = {GATT_CHAR_TEMPERATURE_TYPE,     ATT_REQ(PRES, OPT),  PROP(RD)},
    /// Intermediate Temperature
    [HTPC_CHAR_HTS_INTM_TEMP]        = {GATT_CHAR_INTERMED_TEMPERATURE, ATT_REQ(PRES, OPT),  PROP(N)},
    /// Measurement Interval
    [HTPC_CHAR_HTS_MEAS_INTV]        = {GATT_CHAR_MEAS_INTERVAL,        ATT_REQ(PRES, OPT),  PROP(RD)},
};

/// State machine used to retrieve Health Thermometer service characteristics description information
const prf_desc_def_t htpc_hts_char_desc[HTPC_DESC_HTS_MAX] =
{
    /// Temperature Measurement Client Config
    [HTPC_DESC_HTS_TEMP_MEAS_CLI_CFG]        = {GATT_DESC_CLIENT_CHAR_CFG, ATT_REQ(PRES, MAND), HTPC_CHAR_HTS_TEMP_MEAS},
    /// Intermediate Temperature Client Config
    [HTPC_DESC_HTS_INTM_MEAS_CLI_CFG]        = {GATT_DESC_CLIENT_CHAR_CFG, ATT_REQ(PRES, OPT),  HTPC_CHAR_HTS_INTM_TEMP},
    /// Measurement Interval Client Config
    [HTPC_DESC_HTS_MEAS_INTV_CLI_CFG]        = {GATT_DESC_CLIENT_CHAR_CFG, ATT_REQ(PRES, OPT),  HTPC_CHAR_HTS_MEAS_INTV},
    /// Measurement Interval valid range
    [HTPC_DESC_HTS_MEAS_INTV_VAL_RGE]        = {GATT_DESC_VALID_RANGE,     ATT_REQ(PRES, OPT),  HTPC_CHAR_HTS_MEAS_INTV},
};

/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Send discovery results to application.
 *
 * @param p_htpc_env    Client Role task environment
 * @param conidx        Connection index
 * @param status        Response status code
 *****************************************************************************************
 */
__STATIC void htpc_enable_cmp(htpc_env_t* p_htpc_env, uint8_t conidx, uint16_t status)
{
    if(p_htpc_env != NULL)
    {
        const htpc_cb_t* p_cb = (const htpc_cb_t*) p_htpc_env->prf_env.p_cb;
        p_cb->cb_enable_cmp(conidx, status, &(p_htpc_env->p_env[conidx]->hts));

        if (status != GAP_ERR_NO_ERROR)
        {
            // clean-up environment variable allocated for task instance
            ke_free(p_htpc_env->p_env[conidx]);
            p_htpc_env->p_env[conidx] = NULL;
        }
        else
        {
             p_htpc_env->p_env[conidx]->state = HTPC_IDLE;

            // Register profile handle to catch gatt indications
            gatt_cli_event_register(conidx, p_htpc_env->user_lid, p_htpc_env->p_env[conidx]->hts.svc.shdl,
                                    p_htpc_env->p_env[conidx]->hts.svc.ehdl);
        }
    }
}


/**
 ****************************************************************************************
 * @brief Send read result to application,.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the request
 * @param[in] val_id        Value Identifier (@see enum htpcc_val_id)
 * @param[in] length        Length of data value)
 * @param[in] p_data        Pointer of data value
 ****************************************************************************************
 */
__STATIC void htpc_read_val_cmp(uint8_t conidx, uint16_t status, uint8_t val_id, uint16_t length, const uint8_t* p_data)
{
    htpc_env_t* p_htpc_env = PRF_ENV_GET(HTPC, htpc);

    if(p_htpc_env != NULL)
    {
        const htpc_cb_t* p_cb = (const htpc_cb_t*) p_htpc_env->prf_env.p_cb;

        p_cb->cb_rd_char_cmp(conidx, status, val_id, length, p_data);
    }
}


/**
 ****************************************************************************************
 * @brief Unpack the received temperature measurement value
 *
 * @param[in]  p_buf         Pointer to Input buffer
 * @param[out] p_temp_meas   Pointer to unpacked structure
 ****************************************************************************************
 */
__STATIC void htpc_unpack_temp(co_buf_t* p_buf, htp_temp_meas_t* p_temp_meas)
{
    // Unpack Temp Measurement
    p_temp_meas->flags = co_buf_data(p_buf)[0];
    co_buf_head_release(p_buf, 1);

    p_temp_meas->temp = co_btohl(co_read32p(co_buf_data(p_buf)));
    co_buf_head_release(p_buf, 4);

    // Time Flag Set
    if (GETB(p_temp_meas->flags, HTP_FLAG_TIME))
    {
        prf_unpack_date_time(p_buf, &(p_temp_meas->time_stamp));
    }
    else
    {
        memset(&(p_temp_meas->time_stamp), 0, sizeof(prf_date_time_t));
    }

    // Type Flag set
    if (GETB(p_temp_meas->flags, HTP_FLAG_TYPE))
    {
        p_temp_meas->type = co_buf_data(p_buf)[0];
        co_buf_head_release(p_buf, 1);
    }
    else
    {
        p_temp_meas->type = 0;
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
__STATIC void htpc_svc_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t hdl, uint8_t disc_info,
                          uint8_t nb_att, const gatt_svc_att_t* p_atts)
{
    // Get the address of the environment
    htpc_env_t* p_htpc_env = PRF_ENV_GET(HTPC, htpc);

    if(p_htpc_env != NULL)
    {
        ASSERT_INFO(p_htpc_env->p_env[conidx] != NULL, conidx, user_lid);

        if (p_htpc_env->p_env[conidx]->nb_svc == 0)
        {
            //Even if we get multiple responses we only store 1 range
            if((disc_info == GATT_SVC_CMPLT) || (disc_info == GATT_SVC_START))
            {
                p_htpc_env->p_env[conidx]->hts.svc.shdl = hdl;
            }

            if((disc_info == GATT_SVC_CMPLT) || (disc_info == GATT_SVC_END))
            {
                p_htpc_env->p_env[conidx]->hts.svc.ehdl = hdl + nb_att -1;
            }

            // Retrieve characteristics
            prf_extract_svc_info(hdl, nb_att, p_atts,
                                 HTPC_CHAR_HTS_MAX, &htpc_hts_char[0], &(p_htpc_env->p_env[conidx]->hts.chars[0]),
                                 HTPC_DESC_HTS_MAX, &htpc_hts_char_desc[0], &(p_htpc_env->p_env[conidx]->hts.descs[0]));

        }

        if((disc_info == GATT_SVC_CMPLT) || (disc_info == GATT_SVC_END))
        {
            p_htpc_env->p_env[conidx]->nb_svc++;
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
__STATIC void htpc_discover_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    // Get the address of the environment
    htpc_env_t* p_htpc_env = PRF_ENV_GET(HTPC, htpc);

    if(p_htpc_env != NULL)
    {
        if (p_htpc_env->p_env[conidx]->nb_svc ==  1)
        {
            status = prf_check_svc_char_validity(HTPC_CHAR_HTS_MAX, p_htpc_env->p_env[conidx]->hts.chars, htpc_hts_char);
            p_htpc_env->p_env[conidx]->nb_svc = 0;
        }
        // too much services
        else if (p_htpc_env->p_env[conidx]->nb_svc > 1)
        {
            status = PRF_ERR_MULTIPLE_SVC;
        }
        // no services found
        else
        {
            status = PRF_ERR_STOP_DISC_CHAR_MISSING;
        }

        htpc_enable_cmp(p_htpc_env, conidx, status);
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
__STATIC void htpc_att_val_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t hdl, uint16_t offset,
                              co_buf_t* p_data)
{
    htpc_read_val_cmp(conidx, GAP_ERR_NO_ERROR, (uint8_t) dummy, co_buf_data_len(p_data), co_buf_data(p_data));
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
__STATIC void htpc_read_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    if(status != GAP_ERR_NO_ERROR)
    {
        htpc_read_val_cmp(conidx, status, (uint8_t) dummy, 0, NULL);
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
__STATIC void htpc_write_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    htpc_env_t* p_htpc_env = PRF_ENV_GET(HTPC, htpc);

    if(p_htpc_env != NULL)
    {
        const htpc_cb_t* p_cb = (const htpc_cb_t*) p_htpc_env->prf_env.p_cb;

        switch(dummy)
        {
            case HTPC_HEALTH_TEMP_NTF_CFG_REQ:
            {
                p_cb->cb_health_temp_ntf_cfg_cmp(conidx, status);
            } break;

            case HTPC_WR_MEAS_INTV_REQ:
            {
                p_cb->cb_wr_meas_intv_cmp(conidx, status);
            } break;
            default: { /* Nothing to do */ } break;
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
__STATIC void htpc_att_val_evt_cb(uint8_t conidx, uint8_t user_lid, uint16_t token, uint8_t evt_type, bool complete,
                        uint16_t hdl, co_buf_t* p_data)
{
    htpc_env_t* p_htpc_env = PRF_ENV_GET(HTPC, htpc);

    if(p_htpc_env != NULL)
    {
        const htpc_cb_t* p_cb = (const htpc_cb_t*) p_htpc_env->prf_env.p_cb;

        // Measurement Interval Char.
        if (hdl == p_htpc_env->p_env[conidx]->hts.chars[HTPC_CHAR_HTS_MEAS_INTV].val_hdl)
        {
            uint16_t meas_intv = co_btohs(co_read16p(co_buf_data(p_data)));
            p_cb->cb_meas_intv_ind(conidx, meas_intv);
        }
        else if (   (hdl == p_htpc_env->p_env[conidx]->hts.chars[HTPC_CHAR_HTS_TEMP_MEAS].val_hdl)
                 || (hdl == p_htpc_env->p_env[conidx]->hts.chars[HTPC_CHAR_HTS_INTM_TEMP].val_hdl))
        {
            // Checked the length of the received value
            if (co_buf_data_len(p_data) >= HTPC_PACKED_TEMP_MIN_LEN)
            {
                htp_temp_meas_t temp_meas;
                bool stable_meas = (hdl == p_htpc_env->p_env[conidx]->hts.chars[HTPC_CHAR_HTS_TEMP_MEAS].val_hdl);

                // unpack temperature data
                htpc_unpack_temp(p_data, &temp_meas);

                p_cb->cb_temp_ind(conidx, &temp_meas, stable_meas);
            }
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
__STATIC void htpc_svc_changed_cb(uint8_t conidx, uint8_t user_lid, bool out_of_sync, uint16_t start_hdl, uint16_t end_hdl)
{
    // Ignore
}

/// Client callback hander
__STATIC const gatt_cli_cb_t htpc_cb =
{
    .cb_discover_cmp    = htpc_discover_cmp_cb,
    .cb_read_cmp        = htpc_read_cmp_cb,
    .cb_write_cmp       = htpc_write_cmp_cb,
    .cb_att_val_get     = NULL,
    .cb_svc             = htpc_svc_cb,
    .cb_svc_info        = NULL,
    .cb_inc_svc         = NULL,
    .cb_char            = NULL,
    .cb_desc            = NULL,
    .cb_att_val         = htpc_att_val_cb,
    .cb_att_val_evt     = htpc_att_val_evt_cb,
    .cb_svc_changed     = htpc_svc_changed_cb,
};

/*
 * PROFILE NATIVE API
 ****************************************************************************************
 */

uint16_t htpc_enable(uint8_t conidx, uint8_t con_type, const htpc_hts_content_t* p_hts)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    htpc_env_t* p_htpc_env = PRF_ENV_GET(HTPC, htpc);

    if(p_htpc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_htpc_env->p_env[conidx] == NULL))
        {
            // allocate environment variable for task instance
            p_htpc_env->p_env[conidx] = (struct htpc_cnx_env *) ke_malloc(sizeof(struct htpc_cnx_env), KE_MEM_ATT_DB);

            if(p_htpc_env->p_env[conidx] != NULL)
            {
                // Config connection, start discovering
                if (con_type == PRF_CON_DISCOVERY)
                {
                    uint16_t gatt_svc_uuid = GATT_SVC_HEALTH_THERMOM;
                    memset(p_htpc_env->p_env[conidx], 0, sizeof(struct htpc_cnx_env));

                    // start discovery
                    status = gatt_cli_discover_svc(conidx, p_htpc_env->user_lid, 0, GATT_DISCOVER_SVC_PRIMARY_BY_UUID, true,
                                                   GATT_MIN_HDL, GATT_MAX_HDL, GATT_UUID_16, (uint8_t*) &gatt_svc_uuid);

                    // Go to DISCOVERING state
                    p_htpc_env->p_env[conidx]->state = HTPC_DISCOVERING;
                }
                // normal connection, get saved att details
                else
                {
                    memcpy(&(p_htpc_env->p_env[conidx]->hts), p_hts, sizeof(htpc_hts_content_t));
                    status = GAP_ERR_NO_ERROR;

                    // send APP confirmation that can start normal connection to TH
                    htpc_enable_cmp(p_htpc_env, conidx, GAP_ERR_NO_ERROR);
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

uint16_t htpc_health_temp_ntf_cfg(uint8_t conidx, uint8_t char_code, uint16_t cfg_val)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    htpc_env_t* p_htpc_env = PRF_ENV_GET(HTPC, htpc);

    if(p_htpc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_htpc_env->p_env[conidx] != NULL) && (p_htpc_env->p_env[conidx]->state == HTPC_IDLE))
        {
            uint16_t cfg_hdl  = GATT_INVALID_HDL;
            status = GAP_ERR_NO_ERROR;

            switch(char_code)
            {
                case HTPC_CHAR_HTS_TEMP_MEAS: //can only IND
                {
                    cfg_hdl = p_htpc_env->p_env[conidx]->hts.descs[HTPC_DESC_HTS_TEMP_MEAS_CLI_CFG].desc_hdl;
                    if ((cfg_val != PRF_CLI_STOP_NTFIND) && (cfg_val != PRF_CLI_START_IND))
                    {
                        status = PRF_ERR_INVALID_PARAM;
                    }
                } break;

                case HTPC_CHAR_HTS_MEAS_INTV: //can only IND
                {
                    cfg_hdl = p_htpc_env->p_env[conidx]->hts.descs[HTPC_DESC_HTS_MEAS_INTV_CLI_CFG].desc_hdl;
                    if ((cfg_val != PRF_CLI_STOP_NTFIND) && (cfg_val != PRF_CLI_START_IND))
                    {
                        status = PRF_ERR_INVALID_PARAM;
                    }
                } break;

                case HTPC_CHAR_HTS_INTM_TEMP: //can only NTF
                {
                    cfg_hdl = p_htpc_env->p_env[conidx]->hts.descs[HTPC_DESC_HTS_INTM_MEAS_CLI_CFG].desc_hdl;
                    if ((cfg_val != PRF_CLI_STOP_NTFIND) && (cfg_val != PRF_CLI_START_NTF))
                    {
                        status = PRF_ERR_INVALID_PARAM;
                    }
                } break;

                default:
                {
                    status = PRF_ERR_INEXISTENT_HDL;
                } break;
            }

            // no error detected
            if (status == GAP_ERR_NO_ERROR)
            {
                // Force endianess
                cfg_val = co_htobs(cfg_val);
                status = prf_gatt_write(conidx, p_htpc_env->user_lid, HTPC_HEALTH_TEMP_NTF_CFG_REQ, GATT_WRITE,
                                        cfg_hdl, sizeof(uint16_t), (uint8_t *)&cfg_val);
            }
        }
    }

    return (status);
}

uint16_t htpc_wr_meas_intv(uint8_t conidx, uint16_t meas_intv)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    htpc_env_t* p_htpc_env = PRF_ENV_GET(HTPC, htpc);

    if(p_htpc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_htpc_env->p_env[conidx] != NULL) && (p_htpc_env->p_env[conidx]->state == HTPC_IDLE))
        {
            uint16_t val_hdl  = GATT_INVALID_HDL;

            // check if value is writable
            if (!GETB(p_htpc_env->p_env[conidx]->hts.chars[HTPC_CHAR_HTS_MEAS_INTV].prop, GATT_PROP_WR))
            {
                status = PRF_ERR_NOT_WRITABLE;
            }
            else
            {
                val_hdl  = p_htpc_env->p_env[conidx]->hts.chars[HTPC_CHAR_HTS_MEAS_INTV].val_hdl;

                if (val_hdl != GATT_INVALID_HDL)
                {
                    // Force endianess
                    meas_intv = co_htobs(meas_intv);
                    status = prf_gatt_write(conidx, p_htpc_env->user_lid, HTPC_WR_MEAS_INTV_REQ, GATT_WRITE,
                                            val_hdl, sizeof(uint16_t), (uint8_t *)&meas_intv);
                }
                else
                {
                    // invalid handle requested
                    status = PRF_ERR_INEXISTENT_HDL;
                }
            }
        }
    }

    return (status);
}

uint16_t htpc_rd_char(uint8_t conidx, uint8_t val_id)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    htpc_env_t* p_htpc_env = PRF_ENV_GET(HTPC, htpc);

    if(p_htpc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_htpc_env->p_env[conidx] != NULL) && (p_htpc_env->p_env[conidx]->state == HTPC_IDLE))
        {
            uint16_t search_hdl = GATT_INVALID_HDL;

            switch (val_id)
            {
                // Read HTS Temp. Type
                case HTPC_VAL_TEMP_TYPE:
                {
                    search_hdl = p_htpc_env->p_env[conidx]->hts.chars[HTPC_CHAR_HTS_TEMP_TYPE].val_hdl;
                } break;
                // Read HTS Measurement Interval
                case HTPC_VAL_MEAS_INTV:
                {
                    search_hdl = p_htpc_env->p_env[conidx]->hts.chars[HTPC_CHAR_HTS_MEAS_INTV].val_hdl;
                } break;
                // Read HTS Temperature Measurement Client Cfg. Desc
                case HTPC_VAL_TEMP_MEAS_CLI_CFG:
                {
                    search_hdl = p_htpc_env->p_env[conidx]->hts.descs[HTPC_DESC_HTS_TEMP_MEAS_CLI_CFG].desc_hdl;
                } break;
                // Read HTS Intermediate Temperature Client Cfg. Desc
                case HTPC_VAL_INTM_TEMP_CLI_CFG:
                {
                    search_hdl = p_htpc_env->p_env[conidx]->hts.descs[HTPC_DESC_HTS_INTM_MEAS_CLI_CFG].desc_hdl;
                } break;
                // Read HTS Measurement Interval Client Cfg. Desc
                case HTPC_VAL_MEAS_INTV_CLI_CFG:
                {
                    search_hdl = p_htpc_env->p_env[conidx]->hts.descs[HTPC_DESC_HTS_MEAS_INTV_CLI_CFG].desc_hdl;
                } break;
                // Read HTS Measurement Interval Client Cfg. Desc
                case HTPC_VAL_MEAS_INTV_VAL_RGE:
                {
                    search_hdl = p_htpc_env->p_env[conidx]->hts.descs[HTPC_DESC_HTS_MEAS_INTV_VAL_RGE].desc_hdl;
                } break;
                default:
                {
                    status = GAP_ERR_INVALID_PARAM;
                } break;
            }

            //Check if handle is viable
            if (search_hdl != GATT_INVALID_HDL)
            {
                // perform read request
                status = gatt_cli_read(conidx, p_htpc_env->user_lid, val_id, search_hdl, 0, 0);
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


#if (BLE_HL_MSG_API)
/*
 * PROFILE MSG HANDLERS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Handles reception of the @ref HTPC_ENABLE_REQ message.
 * The handler enables the Health Thermometer Profile Collector Role.
 * @param[in] msgid Id of the message received (probably unused).
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int htpc_enable_req_handler(ke_msg_id_t const msgid,
                                   struct htpc_enable_req const *p_param,
                                   ke_task_id_t const dest_id,
                                   ke_task_id_t const src_id)
{
    uint16_t status = htpc_enable(p_param->conidx, p_param->con_type, &(p_param->hts));

    // send an error if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct htpc_enable_rsp *p_rsp = KE_MSG_ALLOC(HTPC_ENABLE_RSP, src_id, dest_id, htpc_enable_rsp);
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
 * @brief Handles reception of the @ref HTPC_WR_MEAS_INTV_REQ message.
 * Check if the handle exists in profile(already discovered) and send request, otherwise
 * error to APP.
 * @param[in] msgid Id of the message received (probably unused).
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int htpc_wr_meas_intv_req_handler(ke_msg_id_t const msgid, struct htpc_wr_meas_intv_req const *p_param,
                                           ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = htpc_wr_meas_intv(p_param->conidx, p_param->intv);

    // send error response if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct htpc_wr_meas_intv_rsp *p_rsp = KE_MSG_ALLOC(HTPC_WR_MEAS_INTV_RSP, src_id, dest_id, htpc_wr_meas_intv_rsp);
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
 * @brief Handles reception of the @ref HTPC_HEALTH_TEMP_NTF_CFG_REQ message.
 * It allows configuration of the peer ind/ntf/stop characteristic for a specified characteristic.
 * Will return an error code if that cfg char does not exist.
 * @param[in] msgid Id of the message received (probably unused).
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int htpc_health_temp_ntf_cfg_req_handler(ke_msg_id_t const msgid, struct htpc_health_temp_ntf_cfg_req const *p_param,
                                                  ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = htpc_health_temp_ntf_cfg(p_param->conidx, p_param->char_code, p_param->cfg_val);

    // send error response if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct htpc_health_temp_ntf_cfg_rsp *p_rsp = KE_MSG_ALLOC(HTPC_HEALTH_TEMP_NTF_CFG_RSP,
                                                                src_id, dest_id,
                                                                htpc_health_temp_ntf_cfg_rsp);

        p_rsp->conidx = p_param->conidx;
        p_rsp->status = status;
        ke_msg_send(p_rsp);
    }

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief Handles reception of the @ref HTPC_RD_REQ message.
 * Check if the handle exists in profile(already discovered) and send request, otherwise
 * error to APP.
 * @param[in] msgid Id of the message received (probably unused).
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int htpc_rd_char_req_handler(ke_msg_id_t const msgid, struct htpc_rd_char_req const *p_param,
                                      ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = htpc_rd_char(p_param->conidx, p_param->val_id);

    // send error response if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct htpc_rd_char_rsp *p_ind =  KE_MSG_ALLOC(HTPC_RD_CHAR_RSP, src_id, dest_id, htpc_rd_char_rsp);
        if(p_ind != NULL)
        {
            p_ind->conidx = p_param->conidx;
            p_ind->status = status;
            p_ind->val_id = p_param->val_id;

            ke_msg_send(p_ind);
        }
    }

    return (KE_MSG_CONSUMED);
}

/* Default State handlers definition. */
KE_MSG_HANDLER_TAB(htpc)
{
    // Note: all messages must be sorted in ID ascending order

    {HTPC_ENABLE_REQ,               (ke_msg_func_t) htpc_enable_req_handler},
    {HTPC_HEALTH_TEMP_NTF_CFG_REQ,  (ke_msg_func_t) htpc_health_temp_ntf_cfg_req_handler},
    {HTPC_WR_MEAS_INTV_REQ,         (ke_msg_func_t) htpc_wr_meas_intv_req_handler},
    {HTPC_RD_CHAR_REQ,              (ke_msg_func_t) htpc_rd_char_req_handler},
};


/**
 ****************************************************************************************
 * @brief Completion of Enable procedure
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 * @param[in] p_hts         Pointer to peer database description bond data
 ****************************************************************************************
 */
__STATIC void htpc_cb_enable_cmp(uint8_t conidx, uint16_t status, const htpc_hts_content_t* p_hts)
{
    // Send APP the details of the discovered attributes on HTPC
    struct htpc_enable_rsp *p_rsp = KE_MSG_ALLOC(HTPC_ENABLE_RSP, PRF_DST_TASK(HTPC), PRF_SRC_TASK(HTPC),
                                                 htpc_enable_rsp);
    if(p_rsp != NULL)
    {
        p_rsp->conidx = conidx;
        p_rsp->status = status;
        memcpy(&(p_rsp->hts), p_hts, sizeof(htpc_hts_content_t));
        ke_msg_send(p_rsp);
    }
}

/**
 ****************************************************************************************
 * @brief Inform that update of Notification configuration is over
 *
 * @param[in] conidx    Connection index
 * @param[in] status    Status of the procedure execution (@see enum hl_err)
 ****************************************************************************************
 */
__STATIC void htpc_cb_health_temp_ntf_cfg_cmp(uint8_t conidx, uint16_t status)
{
    // send response of the notification configuration request
    struct htpc_health_temp_ntf_cfg_rsp *p_rsp =
            KE_MSG_ALLOC(HTPC_HEALTH_TEMP_NTF_CFG_RSP, PRF_DST_TASK(HTPC), PRF_SRC_TASK(HTPC), htpc_health_temp_ntf_cfg_rsp);

    if(p_rsp != NULL)
    {
        p_rsp->conidx = conidx;
        p_rsp->status = status;
        ke_msg_send(p_rsp);
    }
}

/**
 ****************************************************************************************
 * @brief Inform that update of measurement interval is over
 *
 * @param[in] conidx    Connection index
 * @param[in] status    Status of the procedure execution (@see enum hl_err)
 ****************************************************************************************
 */
__STATIC void htpc_cb_wr_meas_intv_cmp(uint8_t conidx, uint16_t status)
{
    struct htpc_wr_meas_intv_rsp *p_rsp =
            KE_MSG_ALLOC(HTPC_WR_MEAS_INTV_RSP, PRF_DST_TASK(HTPC), PRF_SRC_TASK(HTPC), htpc_wr_meas_intv_rsp);

    if(p_rsp != NULL)
    {
        p_rsp->conidx = conidx;
        p_rsp->status = status;
        ke_msg_send(p_rsp);
    }
}

/**
 ****************************************************************************************
 * @brief Inform that characteristic information has been received
 *
 * @param[in] conidx    Connection index
 * @param[in] status    Status of the procedure execution (@see enum hl_err)
 * @param[in] val_id    Value identifier (@see htpc_val_id)
 * @param[in] length    Value length
 * @param[in] p_data    Pointer to data value
 ****************************************************************************************
 */
__STATIC void htpc_cb_rd_char_cmp(uint8_t conidx, uint16_t status, uint8_t val_id, uint16_t length, const uint8_t* p_data)
{
    struct htpc_rd_char_rsp *p_ind =
            KE_MSG_ALLOC_DYN(HTPC_RD_CHAR_RSP, PRF_DST_TASK(HTPC), PRF_SRC_TASK(HTPC), htpc_rd_char_rsp, length);
    if(p_ind != NULL)
    {
        p_ind->conidx = conidx;
        p_ind->status = GAP_ERR_NO_ERROR;

        p_ind->val_id = val_id;

        if(status == GAP_ERR_NO_ERROR)
        {
            p_ind->length = length;
            memcpy(&(p_ind->value), p_data, length);
        }

        ke_msg_send(p_ind);
    }
}

/**
 ****************************************************************************************
 * @brief Inform that new temperature data has been received
 *
 * @param[in] conidx       Connection index
 * @param[in] p_temp_meas  Pointer to Temperature Measurement value
 * @param[in] stable_meas  Stable or intermediary type of temperature
 ****************************************************************************************
 */
__STATIC void htpc_cb_temp_ind(uint8_t conidx, const htp_temp_meas_t* p_temp_meas, bool stable_meas)
{
    struct htpc_temp_ind *p_ind = KE_MSG_ALLOC(HTPC_TEMP_IND, PRF_DST_TASK(HTPC), PRF_SRC_TASK(HTPC),
                                               htpc_temp_ind);

    if(p_ind != NULL)
    {
        p_ind->conidx      = conidx;
        p_ind->stable_meas = stable_meas;
        memcpy(&(p_ind->temp_meas), p_temp_meas, sizeof(htp_temp_meas_t));
        ke_msg_send(p_ind);
    }
}

/**
 ****************************************************************************************
 * @brief Inform that update of measurement interval has been received
 *
 * @param[in] conidx       Connection index
 * @param[in] meas_intv    Measurement interval in seconds
 ****************************************************************************************
 */
__STATIC void htpc_cb_meas_intv_ind(uint8_t conidx, uint16_t meas_intv)
{
    struct htpc_meas_intv_ind *p_ind = KE_MSG_ALLOC(HTPC_MEAS_INTV_IND, PRF_DST_TASK(HTPC), PRF_SRC_TASK(HTPC),
                                        htpc_meas_intv_ind);

    if(p_ind != NULL)
    {
        p_ind->conidx = conidx;
        p_ind->intv   = meas_intv;
        ke_msg_send(p_ind);
    }
}

/// Default Message handle
__STATIC const htpc_cb_t htpc_msg_cb =
{
    .cb_enable_cmp              = htpc_cb_enable_cmp,
    .cb_health_temp_ntf_cfg_cmp = htpc_cb_health_temp_ntf_cfg_cmp,
    .cb_wr_meas_intv_cmp        = htpc_cb_wr_meas_intv_cmp,
    .cb_rd_char_cmp             = htpc_cb_rd_char_cmp,
    .cb_temp_ind                = htpc_cb_temp_ind,
    .cb_meas_intv_ind           = htpc_cb_meas_intv_ind,
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
__STATIC uint16_t htpc_init(prf_data_t* p_env, uint16_t* p_start_hdl, uint8_t sec_lvl, uint8_t user_prio,
                          const void* p_params, const htpc_cb_t* p_cb)
{
    uint8_t conidx;
    // DB Creation Status
    uint16_t status = GAP_ERR_NO_ERROR;
    uint8_t user_lid = GATT_INVALID_USER_LID;

    do
    {
        htpc_env_t* p_htpc_env;

        #if (BLE_HL_MSG_API)
        if(p_cb == NULL)
        {
            p_cb = &(htpc_msg_cb);
        }
        #endif // (BLE_HL_MSG_API)

        if(   (p_params == NULL) || (p_cb == NULL)
           || (p_cb->cb_enable_cmp == NULL) || (p_cb->cb_health_temp_ntf_cfg_cmp == NULL) || (p_cb->cb_wr_meas_intv_cmp == NULL)
           || (p_cb->cb_rd_char_cmp == NULL) || (p_cb->cb_temp_ind == NULL) || (p_cb->cb_meas_intv_ind == NULL))
        {
            status = GAP_ERR_INVALID_PARAM;
            break;
        }

        // register HTPC user
        status = gatt_user_cli_register(L2CAP_LE_MTU_MIN, user_prio, &htpc_cb, &user_lid);
        if(status != GAP_ERR_NO_ERROR) break;

        //-------------------- allocate memory required for the profile  ---------------------
        p_htpc_env = (htpc_env_t*) ke_malloc(sizeof(htpc_env_t), KE_MEM_ATT_DB);

        if(p_htpc_env != NULL)
        {
            // allocate HTPC required environment variable
            p_env->p_env = (prf_hdr_t *) p_htpc_env;

            // initialize environment variable
            p_htpc_env->prf_env.p_cb    = p_cb;
            #if (BLE_HL_MSG_API)
            p_env->desc.msg_handler_tab = htpc_msg_handler_tab;
            p_env->desc.msg_cnt         = ARRAY_LEN(htpc_msg_handler_tab);
            #endif // (BLE_HL_MSG_API)

            p_htpc_env->user_lid = user_lid;
            for (conidx = 0; conidx < BLE_CONNECTION_MAX; conidx++)
            {
                p_htpc_env->p_env[conidx] = NULL;
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
__STATIC uint16_t htpc_destroy(prf_data_t *p_env, uint8_t reason)
{
    uint16_t status = GAP_ERR_NO_ERROR;
    htpc_env_t* p_htpc_env = (htpc_env_t*) p_env->p_env;

    if(reason != PRF_DESTROY_RESET)
    {
        status = gatt_user_unregister(p_htpc_env->user_lid);
    }

    if(status == GAP_ERR_NO_ERROR)
    {
        uint8_t idx;

        // cleanup environment variable for each task instances
        for (idx = 0; idx < BLE_CONNECTION_MAX; idx++)
        {
            if (p_htpc_env->p_env[idx] != NULL)
            {
                ke_free(p_htpc_env->p_env[idx]);
            }
        }

        // free profile environment variables
        p_env->p_env = NULL;
        ke_free(p_htpc_env);
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
__STATIC void htpc_con_create(prf_data_t *p_env, uint8_t conidx, const gap_con_param_t* p_con_param)
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
__STATIC void htpc_con_cleanup(prf_data_t *p_env, uint8_t conidx, uint16_t reason)
{
    htpc_env_t* p_htpc_env = (htpc_env_t*) p_env->p_env;

    // clean-up environment variable allocated for task instance
    if (p_htpc_env->p_env[conidx] != NULL)
    {
        ke_free(p_htpc_env->p_env[conidx]);
        p_htpc_env->p_env[conidx] = NULL;
    }
}

/// HTPC Task interface required by profile manager
const prf_task_cbs_t htpc_itf =
{
    .cb_init          = (prf_init_cb) htpc_init,
    .cb_destroy       = htpc_destroy,
    .cb_con_create    = htpc_con_create,
    .cb_con_cleanup   = htpc_con_cleanup,
    .cb_con_upd       = NULL,
};

/**
 ****************************************************************************************
 * @brief Retrieve client profile interface
 *
 * @return Client profile interface
 ****************************************************************************************
 */
const prf_task_cbs_t* htpc_prf_itf_get(void)
{
    return &htpc_itf;
}


#endif //BLE_HT_COLLECTOR
/// @} HTPC

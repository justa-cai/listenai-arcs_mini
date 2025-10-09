/**
 ****************************************************************************************
 *
 * @file tipc.c
 *
 * @brief Time Profile Client implementation.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */


/**
 ****************************************************************************************
 * @addtogroup TIPC
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "btip_config.h"

#if (BLE_TIP_CLIENT)

#include "tipc.h"
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

/// Internal Service identifier
enum tipc_svc_id
{
    TIPC_SVC_CTS,
    TIPC_SVC_NDCS,
    TIPC_SVC_RTUS,
    TIPC_SVC_MAX,
};


/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */


/// Environment variable for each Connections
typedef struct tipc_cnx_env
{
    /// Peer database discovered handle mapping CTS
    tipc_cts_content_t    cts;
    /// Peer database discovered handle mapping NDCS
    tipc_ndcs_content_t   ndcs;
    /// Peer database discovered handle mapping RTUS
    tipc_rtus_content_t   rtus;
    /// counter used to check service uniqueness
    uint8_t               nb_svc;
    /// Client is in discovering state
    bool                  discover;
} tipc_cnx_env_t;

/// Client environment variable
typedef struct tipc_env
{
    /// profile environment
    prf_hdr_t            prf_env;
    /// Environment variable pointer for each connections
    tipc_cnx_env_t*      p_env[BLE_CONNECTION_MAX];
    /// GATT User local identifier
    uint8_t              user_lid;
} tipc_env_t;


/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */

/// State machine used to retrieve Current Time service characteristics information
const prf_char_def_t tipc_cts_char[TIPC_CHAR_CTS_MAX] =
{
    // Current Time
    [TIPC_CHAR_CTS_CURR_TIME]        = { GATT_CHAR_CT_TIME,             ATT_REQ(PRES, MAND), PROP(RD)|PROP(N) },
    // Local Time Info
    [TIPC_CHAR_CTS_LOCAL_TIME_INFO]  = { GATT_CHAR_LOCAL_TIME_INFO,     ATT_REQ(PRES, OPT),  PROP(RD)         },
    // Reference Time Info
    [TIPC_CHAR_CTS_REF_TIME_INFO]    = { GATT_CHAR_REFERENCE_TIME_INFO, ATT_REQ(PRES, OPT),  PROP(RD)         },
};

/// State machine used to retrieve Current Time service characteristic description information
const prf_desc_def_t tipc_cts_char_desc[TIPC_DESC_CTS_MAX] =
{
    // Current Time client config
    [TIPC_DESC_CTS_CURR_TIME_CLI_CFG] = { GATT_DESC_CLIENT_CHAR_CFG, ATT_REQ(PRES, MAND), TIPC_CHAR_CTS_CURR_TIME },
};

/// State machine used to retrieve Next DST Change service characteristics information
const prf_char_def_t tipc_ndcs_char[TIPC_CHAR_NDCS_MAX] =
{
    // Current Time
    [TIPC_CHAR_NDCS_TIME_WITH_DST]    = { GATT_CHAR_TIME_WITH_DST, ATT_REQ(PRES, MAND), PROP(RD) },
};

/// State machine used to retrieve Reference Time Update service characteristics information
const prf_char_def_t tipc_rtus_char[TIPC_CHAR_RTUS_MAX] =
{
    // Time Update Control Point
    [TIPC_CHAR_RTUS_TIME_UPD_CTNL_PT] = { GATT_CHAR_TIME_UPDATE_CNTL_POINT, ATT_REQ(PRES, MAND), PROP(WC) },
    // Time Update State
    [TIPC_CHAR_RTUS_TIME_UPD_STATE]   = { GATT_CHAR_TIME_UPDATE_STATE,      ATT_REQ(PRES, MAND), PROP(RD) },
};

/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Unpack Current Time
 *
 * @param[in] p_buf      Pointer to input buffer
 * @param[out] p_value   Pointer to Current Time value
 ****************************************************************************************
 */
__STATIC void tipc_unpack_cts_current_time(co_buf_t* p_buf, tip_curr_time_t* p_value)
{
    // Date-Time
    prf_unpack_date_time(p_buf, &(p_value->date_time));

    //Day of Week
    p_value->day_of_week = co_buf_data(p_buf)[0];
    co_buf_head_release(p_buf, 1);

    //Fraction 256
    p_value->fraction_256 = co_buf_data(p_buf)[0];
    co_buf_head_release(p_buf, 1);

    //Adjust Reason
    p_value->adjust_reason = co_buf_data(p_buf)[0];
    co_buf_head_release(p_buf, 1);
}

/**
 ****************************************************************************************
 * @brief Unpack Local Time Information
 *
 * @param[in] p_buf      Pointer to input buffer
 * @param[out] p_value   Pointer to Local Time Information
 ****************************************************************************************
 */
__STATIC void tipc_unpack_cts_local_time_info(co_buf_t* p_buf, tip_loc_time_info_t* p_value)
{
    // Time Zone
    p_value->time_zone = co_buf_data(p_buf)[0];
    co_buf_head_release(p_buf, 1);

    //DST offset
    p_value->dst_offset = co_buf_data(p_buf)[0];
    co_buf_head_release(p_buf, 1);
}

/**
 ****************************************************************************************
 * @brief Unpack Reference Time Information
 *
 * @param[in] p_buf      Pointer to input buffer
 * @param[out] p_value   Pointer to Reference Time Information
 ****************************************************************************************
 */
__STATIC void tipc_unpack_cts_ref_time_info(co_buf_t* p_buf, tip_ref_time_info_t* p_value)
{
    // Time source
    p_value->time_source = co_buf_data(p_buf)[0];
    co_buf_head_release(p_buf, 1);

    // Time Accuracy
    p_value->time_accuracy = co_buf_data(p_buf)[0];
    co_buf_head_release(p_buf, 1);

    // Days since last update about Reference Source
    p_value->days_update = co_buf_data(p_buf)[0];
    co_buf_head_release(p_buf, 1);

    // Hours since update about Reference Source
    p_value->hours_update = co_buf_data(p_buf)[0];
    co_buf_head_release(p_buf, 1);

}

/**
 ****************************************************************************************
 * @brief Unpack Time With DST value
 *
 * @param[in] p_buf      Pointer to input buffer
 * @param[out] p_value   Pointer to Time With DST value
 ****************************************************************************************
 */
__STATIC void tipc_unpack_ndcs_time_dst(co_buf_t* p_buf, tip_time_with_dst_t* p_value)
{
    // Date-Time
    prf_unpack_date_time(p_buf, &(p_value->date_time));

    // DST Offset
    p_value->dst_offset = co_buf_data(p_buf)[0];
    co_buf_head_release(p_buf, 1);
}


/**
 ****************************************************************************************
 * @brief Unpack Time Update State value
 *
 * @param[in] p_buf      Pointer to input buffer
 * @param[out] p_value   Pointer to Time Update State
 ****************************************************************************************
 */
__STATIC void tipc_unpack_rtus_time_upd_state_val(co_buf_t* p_buf, tip_time_upd_state_t* p_value)
{
    // Current State
    p_value->current_state = co_buf_data(p_buf)[0];
    co_buf_head_release(p_buf, 1);

    // Result
    p_value->result = co_buf_data(p_buf)[0];
    co_buf_head_release(p_buf, 1);
}


/**
 ****************************************************************************************
 * @brief Send discovery results to application.
 *
 * @param p_tipc_env    Client Role task environment
 * @param conidx        Connection index
 * @param status        Response status code
 *****************************************************************************************
 */
__STATIC void tipc_enable_cmp(tipc_env_t* p_tipc_env, uint8_t conidx, uint16_t status)
{

    const tipc_cb_t* p_cb = (const tipc_cb_t*) p_tipc_env->prf_env.p_cb;

    if(p_tipc_env != NULL)
    {
        tipc_cnx_env_t* p_con_env = p_tipc_env->p_env[conidx];

        p_cb->cb_enable_cmp(conidx, status, &(p_con_env->cts), &(p_con_env->ndcs),
                            &(p_con_env->rtus));

        if (status != GAP_ERR_NO_ERROR)
        {
            // clean-up environment variable allocated for task instance
            ke_free(p_con_env);
            p_tipc_env->p_env[conidx] = NULL;
        }
        else
        {
             p_con_env->discover = false;
             // Register profile handle to catch gatt indications
             gatt_cli_event_register(conidx, p_tipc_env->user_lid, p_con_env->cts.svc.shdl,
                                     p_con_env->cts.svc.ehdl);
        }
    }
}

/**
 ****************************************************************************************
 * @brief Send read result to application,.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the request
 * @param[in] val_id        Value Identifier (@see enum tipc_val_id)
 * @param[in] length        Length of data value)
 * @param[in] p_data        Pointer of data value
 ****************************************************************************************
 */
__STATIC void tipc_read_val_cmp(uint8_t conidx, uint16_t status, uint8_t val_id, co_buf_t* p_data)
{
    tipc_env_t* p_tipc_env = PRF_ENV_GET(TIPC, tipc);

    if(p_tipc_env != NULL)
    {
        const tipc_cb_t* p_cb = (const tipc_cb_t*) p_tipc_env->prf_env.p_cb;
        union tip_value value;
        memset(&value, 0, sizeof(union tip_value));

        if(status == GAP_ERR_NO_ERROR)
        {
            // un pack time information
            switch (val_id)
            {
                case TIP_VAL_CTS_CURRENT_TIME:        { tipc_unpack_cts_current_time(p_data, &(value.curr_time));             } break;
                case TIP_VAL_CTS_LOCAL_TIME_INFO:     { tipc_unpack_cts_local_time_info(p_data, &(value.loc_time_info));      } break;
                case TIP_VAL_CTS_REF_TIME_INFO:       { tipc_unpack_cts_ref_time_info(p_data, &(value.ref_time_info));        } break;
                case TIP_VAL_NDCS_TIME_DST:           { tipc_unpack_ndcs_time_dst(p_data, &(value.time_with_dst));            } break;
                case TIP_VAL_RTUS_TIME_UPD_STATE_VAL: { tipc_unpack_rtus_time_upd_state_val(p_data, &(value.time_upd_state)); } break;
                case TIP_VAL_CTS_NTF_CCC_CFG:         { value.ntf_cfg = co_btohs(co_read16p(co_buf_data(p_data)));            } break;

                default:                              { /* Nothing to do */                                                   } break;
            }
        }

        p_cb->cb_read_cmp(conidx, status, val_id, &value);
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
__STATIC void tipc_svc_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t hdl, uint8_t disc_info,
                          uint8_t nb_att, const gatt_svc_att_t* p_atts)
{
    // Get the address of the environment
    tipc_env_t* p_tipc_env = PRF_ENV_GET(TIPC, tipc);

    if(p_tipc_env != NULL)
    {
        tipc_cnx_env_t* p_con_env = p_tipc_env->p_env[conidx];

        ASSERT_INFO(p_con_env != NULL, conidx, user_lid);

        if (p_con_env->nb_svc == 0)
        {
            const prf_char_def_t*      p_char_def = NULL;
            const prf_desc_def_t* p_desc_def = NULL;
            prf_svc_t*                 p_svc      = NULL;
            prf_char_t*            p_chars    = NULL;
            prf_desc_t*       p_desc     = NULL;
            uint8_t                    nb_char    = 0;
            uint8_t                    nb_desc    = 0;

            switch(dummy)
            {
                case TIPC_SVC_CTS:
                {
                    p_svc      = &(p_con_env->cts.svc);
                    p_chars    = &(p_con_env->cts.chars[0]);
                    p_desc     = &(p_con_env->cts.descs[0]);
                    nb_char    = TIPC_CHAR_CTS_MAX;
                    nb_desc    = TIPC_DESC_CTS_MAX;
                    p_char_def = &(tipc_cts_char[0]);
                    p_desc_def = &(tipc_cts_char_desc[0]);
                } break;
                case TIPC_SVC_NDCS:
                {
                    p_svc      = &(p_con_env->ndcs.svc);
                    p_chars    = &(p_con_env->ndcs.chars[0]);
                    nb_char    = TIPC_CHAR_NDCS_MAX;
                    p_char_def = &(tipc_ndcs_char[0]);
                } break;
                case TIPC_SVC_RTUS:
                {
                    p_svc      = &(p_con_env->rtus.svc);
                    p_chars    = &(p_con_env->rtus.chars[0]);
                    nb_char    = TIPC_CHAR_RTUS_MAX;
                    p_char_def = &(tipc_rtus_char[0]);
                } break;
                default: { ASSERT_ERR(0); } break;
            }

            //Even if we get multiple responses we only store 1 range
            if((disc_info == GATT_SVC_CMPLT) || (disc_info == GATT_SVC_START))
            {
                p_svc->shdl = hdl;
            }

            if((disc_info == GATT_SVC_CMPLT) || (disc_info == GATT_SVC_END))
            {
                 p_svc->ehdl = hdl + nb_att -1;
            }

            // Retrieve characteristics
            prf_extract_svc_info(hdl, nb_att, p_atts,
                                 nb_char, p_char_def, p_chars,
                                 nb_desc, p_desc_def, p_desc);
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
__STATIC void tipc_discover_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    // Get the address of the environment
    tipc_env_t* p_tipc_env = PRF_ENV_GET(TIPC, tipc);

    if(p_tipc_env != NULL)
    {
        tipc_cnx_env_t* p_con_env = p_tipc_env->p_env[conidx];

        if (p_con_env->nb_svc ==  1)
        {
            const prf_char_def_t*      p_char_def = NULL;
            prf_char_t*            p_chars    = NULL;
            uint8_t                    nb_char    = 0;

            switch(dummy)
            {
                case TIPC_SVC_CTS:
                {
                    p_chars    = &(p_con_env->cts.chars[0]);
                    nb_char    = TIPC_CHAR_CTS_MAX;
                    p_char_def = &(tipc_cts_char[0]);
                } break;
                case TIPC_SVC_NDCS:
                {
                    p_chars    = &(p_con_env->ndcs.chars[0]);
                    nb_char    = TIPC_CHAR_NDCS_MAX;
                    p_char_def = &(tipc_ndcs_char[0]);
                } break;
                case TIPC_SVC_RTUS:
                {
                    p_chars    = &(p_con_env->rtus.chars[0]);
                    nb_char    = TIPC_CHAR_RTUS_MAX;
                    p_char_def = &(tipc_rtus_char[0]);
                } break;
                default: { ASSERT_ERR(0); } break;
            }

            status = prf_check_svc_char_validity(nb_char, p_chars, p_char_def);

            // Check Descriptors (mandatory)
            if((status == GAP_ERR_NO_ERROR) && (dummy == TIPC_SVC_CTS))
            {
                status = prf_check_svc_desc_validity(TIPC_DESC_CTS_MAX, p_con_env->cts.descs, tipc_cts_char_desc,
                                                          p_con_env->cts.chars);
            }
        }
        // too much services
        else if (p_con_env->nb_svc > 1)
        {
            status = PRF_ERR_MULTIPLE_SVC;
        }
        // if an optional service is not present does not raise an error
        else if ((status == ATT_ERR_ATTRIBUTE_NOT_FOUND) && (dummy != TIPC_SVC_CTS))
        {
            status = GAP_ERR_NO_ERROR;
        }

        // check next service
        dummy += 1;
        if((status == GAP_ERR_NO_ERROR) && (dummy < TIPC_SVC_MAX))
        {
            uint16_t gatt_svc_uuid = GATT_SVC_CURRENT_TIME;
            switch (dummy)
            {
                case TIPC_SVC_NDCS: { gatt_svc_uuid = GATT_SVC_NEXT_DST_CHANGE; } break;
                case TIPC_SVC_RTUS: { gatt_svc_uuid = GATT_SVC_REF_TIME_UPDATE; } break;
                default:            { ASSERT_ERR(0);                            } break;
            }

            // start discovery
            p_con_env->nb_svc = 0;
            status = gatt_cli_discover_svc(conidx, p_tipc_env->user_lid, dummy,
                                           GATT_DISCOVER_SVC_PRIMARY_BY_UUID, true,
                                           GATT_MIN_HDL, GATT_MAX_HDL, GATT_UUID_16, (uint8_t*) &gatt_svc_uuid);
        }

        if((status != GAP_ERR_NO_ERROR) || (dummy == TIPC_SVC_MAX))
        {
            tipc_enable_cmp(p_tipc_env, conidx, status);
        }
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
__STATIC void tipc_att_val_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t hdl, uint16_t offset,
                              co_buf_t* p_data)
{
    tipc_read_val_cmp(conidx, GAP_ERR_NO_ERROR, (uint8_t) dummy, p_data);
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
__STATIC void tipc_read_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    if(status != GAP_ERR_NO_ERROR)
    {
        tipc_read_val_cmp(conidx, status, (uint8_t) dummy, NULL);
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
__STATIC void tipc_write_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    tipc_env_t* p_tipc_env = PRF_ENV_GET(TIPC, tipc);

    if(p_tipc_env != NULL)
    {
        const tipc_cb_t* p_cb = (const tipc_cb_t*) p_tipc_env->prf_env.p_cb;

        switch(dummy)
        {
            // Config control
            case TIPC_CT_NTF_CFG_CMD_OP_CODE:
            {
                p_cb->cb_write_cfg_cmp(conidx, status);
            } break;
            // Control point commands
            case TIPC_WR_TIME_UPD_CTNL_PT_CMD_OP_CODE:
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
__STATIC void tipc_att_val_evt_cb(uint8_t conidx, uint8_t user_lid, uint16_t token, uint8_t evt_type, bool complete,
                                  uint16_t hdl, co_buf_t* p_data)
{
    tipc_env_t* p_tipc_env = PRF_ENV_GET(TIPC, tipc);

    if(p_tipc_env != NULL)
    {
        tipc_cnx_env_t* p_con_env = p_tipc_env->p_env[conidx];
        tipc_cts_content_t* p_cts = &(p_con_env->cts);
        const tipc_cb_t* p_cb = (const tipc_cb_t*) p_tipc_env->prf_env.p_cb;

        if (hdl == p_cts->chars[TIPC_CHAR_CTS_CURR_TIME].val_hdl)
        {
            tip_curr_time_t current_time;
            memset(&current_time, 0, sizeof(tip_curr_time_t));

            tipc_unpack_cts_current_time(p_data, &current_time);

            p_cb->cb_curr_time(conidx, &current_time);
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
__STATIC void tipc_svc_changed_cb(uint8_t conidx, uint8_t user_lid, bool out_of_sync, uint16_t start_hdl, uint16_t end_hdl)
{
    // Do Nothing
}

/// Client callback hander
__STATIC const gatt_cli_cb_t tipc_cb =
{
    .cb_discover_cmp    = tipc_discover_cmp_cb,
    .cb_read_cmp        = tipc_read_cmp_cb,
    .cb_write_cmp       = tipc_write_cmp_cb,
    .cb_att_val_get     = NULL,
    .cb_svc             = tipc_svc_cb,
    .cb_svc_info        = NULL,
    .cb_inc_svc         = NULL,
    .cb_char            = NULL,
    .cb_desc            = NULL,
    .cb_att_val         = tipc_att_val_cb,
    .cb_att_val_evt     = tipc_att_val_evt_cb,
    .cb_svc_changed     = tipc_svc_changed_cb,
};

/*
 * PROFILE NATIVE API
 ****************************************************************************************
 */

uint16_t tipc_enable(uint8_t conidx, uint8_t con_type, const tipc_cts_content_t* p_cts,
                     const tipc_ndcs_content_t* p_ndcs, const tipc_rtus_content_t* p_rtus)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    tipc_env_t* p_tipc_env = PRF_ENV_GET(TIPC, tipc);

    if(p_tipc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_tipc_env->p_env[conidx] == NULL))
        {
            // allocate environment variable for task instance
            p_tipc_env->p_env[conidx] = (struct tipc_cnx_env *) ke_malloc(sizeof(struct tipc_cnx_env), KE_MEM_ATT_DB);

            if(p_tipc_env->p_env[conidx] != NULL)
            {
                memset(p_tipc_env->p_env[conidx], 0, sizeof(struct tipc_cnx_env));

                // Config connection, start discovering
                if (con_type == PRF_CON_DISCOVERY)
                {
                    uint16_t gatt_svc_uuid = GATT_SVC_CURRENT_TIME;

                    // start discovery
                    status = gatt_cli_discover_svc(conidx, p_tipc_env->user_lid, TIPC_SVC_CTS,
                                                   GATT_DISCOVER_SVC_PRIMARY_BY_UUID, true,
                                                   GATT_MIN_HDL, GATT_MAX_HDL, GATT_UUID_16, (uint8_t*) &gatt_svc_uuid);

                    // Go to DISCOVERING state
                    p_tipc_env->p_env[conidx]->discover = true;
                }
                // normal connection, get saved att details
                else
                {
                    if(p_cts != NULL)
                    {
                        memcpy(&(p_tipc_env->p_env[conidx]->cts),  p_cts,   sizeof(tipc_cts_content_t));
                    }

                    if(p_ndcs != NULL)
                    {
                        memcpy(&(p_tipc_env->p_env[conidx]->ndcs), p_ndcs,  sizeof(tipc_ndcs_content_t));
                    }

                    if(p_rtus != NULL)
                    {
                        memcpy(&(p_tipc_env->p_env[conidx]->rtus), p_rtus,  sizeof(tipc_rtus_content_t));
                    }

                    status = GAP_ERR_NO_ERROR;
                    // send APP confirmation that can start normal connection to TH
                    tipc_enable_cmp(p_tipc_env, conidx, GAP_ERR_NO_ERROR);
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

uint16_t tipc_read(uint8_t conidx, uint8_t val_id)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    tipc_env_t* p_tipc_env = PRF_ENV_GET(TIPC, tipc);

    if(p_tipc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_tipc_env->p_env[conidx] != NULL) && (!p_tipc_env->p_env[conidx]->discover))
        {
            tipc_cnx_env_t* p_con_env = p_tipc_env->p_env[conidx];
            uint16_t hdl;
            tipc_cts_content_t*  p_cts  = &(p_con_env->cts);
            tipc_ndcs_content_t* p_ndcs = &(p_con_env->ndcs);
            tipc_rtus_content_t* p_rtus = &(p_con_env->rtus);

            status = GAP_ERR_NO_ERROR;

            switch(val_id)
            {
                case TIP_VAL_CTS_CURRENT_TIME:        { hdl = p_cts->chars[TIPC_CHAR_CTS_CURR_TIME].val_hdl;          } break;
                case TIP_VAL_CTS_LOCAL_TIME_INFO:     { hdl = p_cts->chars[TIPC_CHAR_CTS_LOCAL_TIME_INFO].val_hdl;    } break;
                case TIP_VAL_CTS_REF_TIME_INFO:       { hdl = p_cts->chars[TIPC_CHAR_CTS_REF_TIME_INFO].val_hdl;      } break;
                case TIP_VAL_NDCS_TIME_DST:           { hdl = p_ndcs->chars[TIPC_CHAR_NDCS_TIME_WITH_DST].val_hdl;    } break;
                case TIP_VAL_RTUS_TIME_UPD_STATE_VAL: { hdl = p_rtus->chars[TIPC_CHAR_RTUS_TIME_UPD_STATE].val_hdl;   } break;
                case TIP_VAL_CTS_NTF_CCC_CFG:         { hdl = p_cts->descs[TIPC_DESC_CTS_CURR_TIME_CLI_CFG].desc_hdl; } break;
                default:                              { status = PRF_ERR_INVALID_PARAM;                               } break;
            }

            if(status == GAP_ERR_NO_ERROR)
            {
                if(hdl == GATT_INVALID_HDL)
                {
                    status = PRF_ERR_INEXISTENT_HDL;
                }
                else
                {
                    // perform read request
                    status = gatt_cli_read(conidx, p_tipc_env->user_lid, val_id, hdl, 0, 0);
                }
            }
        }
    }

    return (status);
}


uint16_t tipc_write_cfg(uint8_t conidx, uint16_t cfg_val)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    tipc_env_t* p_tipc_env = PRF_ENV_GET(TIPC, tipc);

    if(p_tipc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_tipc_env->p_env[conidx] != NULL) && (!p_tipc_env->p_env[conidx]->discover))
        {
            tipc_cnx_env_t* p_con_env = p_tipc_env->p_env[conidx];
            uint16_t hdl;
            tipc_cts_content_t*  p_cts  = &(p_con_env->cts);
            hdl = p_cts->descs[TIPC_DESC_CTS_CURR_TIME_CLI_CFG].desc_hdl;

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
                status = prf_gatt_write(conidx, p_tipc_env->user_lid, TIPC_CT_NTF_CFG_CMD_OP_CODE, GATT_WRITE,
                                        hdl, sizeof(uint16_t), (uint8_t *)&cfg_val);
            }
        }
    }

    return (status);
}

uint16_t tipc_ctnl_pt_req(uint8_t conidx, tip_time_upd_contr_pt_t value)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    tipc_env_t* p_tipc_env = PRF_ENV_GET(TIPC, tipc);

    if((value != TIPS_TIME_UPD_CTNL_PT_GET) && (value != TIPS_TIME_UPD_CTNL_PT_CANCEL))
    {
        status = PRF_ERR_INVALID_PARAM;
    }
    else if(p_tipc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_tipc_env->p_env[conidx] != NULL) && (!p_tipc_env->p_env[conidx]->discover))
        {
            tipc_cnx_env_t* p_con_env = p_tipc_env->p_env[conidx];
            tipc_rtus_content_t* p_rtus = &(p_con_env->rtus);
            uint16_t hdl = p_rtus->chars[TIPC_CHAR_RTUS_TIME_UPD_CTNL_PT].val_hdl;

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
                    co_buf_data(p_buf)[0] = value;

                    status = gatt_cli_write(conidx, p_tipc_env->user_lid, TIPC_WR_TIME_UPD_CTNL_PT_CMD_OP_CODE, GATT_WRITE_NO_RESP,
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
 * @brief Send a TIPC_CMP_EVT message to the task which enabled the profile
 * @param[in] conidx Connection index
 * @param[in] operation Operation
 * @param[in] status Satus
 ****************************************************************************************
 */
__STATIC void tipc_send_cmp_evt(uint8_t conidx, uint8_t operation, uint16_t status)
{
    struct tipc_cmp_evt *p_evt;

    // Send the message
    p_evt = KE_MSG_ALLOC(TIPC_CMP_EVT, PRF_DST_TASK(TIPC), PRF_SRC_TASK(TIPC), tipc_cmp_evt);
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
__STATIC int tipc_enable_req_handler(ke_msg_id_t const msgid, struct tipc_enable_req const *p_param,
                                     ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = tipc_enable(p_param->conidx, p_param->con_type, &(p_param->cts), &(p_param->ndcs), &(p_param->rtus));

    // send an error if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct tipc_enable_rsp *p_rsp = KE_MSG_ALLOC(TIPC_ENABLE_RSP, src_id, dest_id, tipc_enable_rsp);
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
 * @brief Handles reception of the @ref TIPC_RD_CHAR_CMD message.
 * Check if the handle exists in profile(already discovered) and send request, otherwise
 * error to APP.
 * @param[in] msgid Id of the message received (probably unused).
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int tipc_rd_char_cmd_handler(ke_msg_id_t const msgid, struct tipc_rd_char_cmd const *p_param,
                                      ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = tipc_read(p_param->conidx, p_param->val_id);

    // send error response if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct tipc_cmp_evt *p_evt = KE_MSG_ALLOC(TIPC_CMP_EVT, src_id, dest_id, tipc_cmp_evt);
        if(p_evt)
        {
            p_evt->conidx     = p_param->conidx;
            p_evt->operation  = TIPC_RD_CHAR_CMD_OP_CODE;
            p_evt->status     = status;

            ke_msg_send(p_evt);
        }
    }

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief Handles reception of the @ref TIPC_CT_NTF_CFG_CMD message.
 * It allows configuration of the peer ind/ntf/stop characteristic for a specified characteristic.
 * Will return an error code if that cfg char does not exist.
 * @param[in] msgid Id of the message received (probably unused).
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int tipc_ct_ntf_cfg_cmd_handler(ke_msg_id_t const msgid, struct tipc_ct_ntf_cfg_cmd const *p_param,
                                         ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = tipc_write_cfg(p_param->conidx, p_param->cfg_val);

    // send error response if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct tipc_cmp_evt *p_evt = KE_MSG_ALLOC(TIPC_CMP_EVT, src_id, dest_id, tipc_cmp_evt);
        if(p_evt)
        {
            p_evt->conidx     = p_param->conidx;
            p_evt->operation  = TIPC_CT_NTF_CFG_CMD_OP_CODE;
            p_evt->status     = status;

            ke_msg_send(p_evt);
        }
    }

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief Handles reception of the @ref TIPC_WR_TIME_UPD_CTNL_PT_CMD message.
 * Check if the handle exists in profile(already discovered) and send request, otherwise
 * error to APP.
 * @param[in] msgid Id of the message received (probably unused).
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int tipc_wr_time_upd_ctnl_pt_cmd_handler(ke_msg_id_t const msgid, struct tipc_wr_time_upd_ctnl_pt_cmd const *p_param,
                                                  ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = tipc_ctnl_pt_req(p_param->conidx, p_param->value);

    // send error response if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct tipc_cmp_evt *p_evt = KE_MSG_ALLOC(TIPC_CMP_EVT, src_id, dest_id, tipc_cmp_evt);
        if(p_evt)
        {
            p_evt->conidx     = p_param->conidx;
            p_evt->operation  = TIPC_WR_TIME_UPD_CTNL_PT_CMD_OP_CODE;
            p_evt->status     = status;

            ke_msg_send(p_evt);
        }
    }

    return (KE_MSG_CONSUMED);
}

/// Default State handlers definition
KE_MSG_HANDLER_TAB(tipc)
{
    // Note: all messages must be sorted in ID ascending order

    { TIPC_ENABLE_REQ,               (ke_msg_func_t) tipc_enable_req_handler             },
    {TIPC_RD_CHAR_CMD,               (ke_msg_func_t)tipc_rd_char_cmd_handler             },
    {TIPC_CT_NTF_CFG_CMD,            (ke_msg_func_t)tipc_ct_ntf_cfg_cmd_handler          },
    {TIPC_WR_TIME_UPD_CTNL_PT_CMD,   (ke_msg_func_t)tipc_wr_time_upd_ctnl_pt_cmd_handler },
};
/**
 ****************************************************************************************
 * @brief Completion of enable procedure
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 * @param[in] p_cts         Pointer to peer CTS database description bond data
 * @param[in] p_ndcs        Pointer to peer NDCS database description bond data
 * @param[in] p_rtus        Pointer to peer RTUS database description bond data
 ****************************************************************************************
 */
__STATIC void tipc_cb_enable_cmp(uint8_t conidx, uint16_t status, const tipc_cts_content_t* p_cts,
                                 const tipc_ndcs_content_t* p_ndcs, const tipc_rtus_content_t* p_rtus)

{
    // Send APP the details of the discovered attributes on TIPC
    struct tipc_enable_rsp *p_rsp = KE_MSG_ALLOC(TIPC_ENABLE_RSP, PRF_DST_TASK(TIPC), PRF_SRC_TASK(TIPC),
                                                 tipc_enable_rsp);
    if(p_rsp != NULL)
    {
        p_rsp->conidx = conidx;
        p_rsp->status = status;
        memcpy(&(p_rsp->cts),  p_cts,  sizeof(tipc_cts_content_t));
        memcpy(&(p_rsp->ndcs), p_ndcs, sizeof(tipc_ndcs_content_t));
        memcpy(&(p_rsp->rtus), p_rtus, sizeof(tipc_rtus_content_t));
        ke_msg_send(p_rsp);
    }
}

/**
 ****************************************************************************************
 * @brief Completion of read alert status procedure.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 * @param[in] val_id        Value identifier (@see enum tip_value_id)
 * @param[in] p_value       Pointer to time value information
 *
 ****************************************************************************************
 */
__STATIC void tipc_cb_read_cmp(uint8_t conidx, uint16_t status, uint8_t val_id, const union tip_value* p_value)
{
    ke_task_id_t src_id  = PRF_SRC_TASK(TIPC);
    ke_task_id_t dest_id = PRF_DST_TASK(TIPC);

    if(status == GAP_ERR_NO_ERROR)
    {
        struct tipc_rd_char_ind *p_ind = KE_MSG_ALLOC(TIPC_RD_CHAR_IND, dest_id, src_id, tipc_rd_char_ind);
        if(p_ind != NULL)
        {
            p_ind->conidx             = conidx;
            p_ind->val_id             = val_id;
            memcpy(&(p_ind->value), p_value, sizeof(union tip_value));
            ke_msg_send(p_ind);
        }
    }

    tipc_send_cmp_evt(conidx, TIPC_RD_CHAR_CMD_OP_CODE, status);
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
__STATIC void tipc_cb_write_cfg_cmp(uint8_t conidx, uint16_t status)
{
    tipc_send_cmp_evt(conidx, TIPC_CT_NTF_CFG_CMD_OP_CODE, status);
}

/**
 ****************************************************************************************
 * @brief Function called when current time update is received
 *
 * @param[in] conidx         Connection index
 * @param[in] p_current_time Pointer to current time update value
 ****************************************************************************************
 */
__STATIC void tipc_cb_curr_time(uint8_t conidx, const tip_curr_time_t* p_current_time)
{
    ke_task_id_t src_id  = PRF_SRC_TASK(TIPC);
    ke_task_id_t dest_id = PRF_DST_TASK(TIPC);

    struct tipc_ct_ind *p_ind = KE_MSG_ALLOC(TIPC_CT_IND, dest_id, src_id, tipc_ct_ind);
    if(p_ind != NULL)
    {
        p_ind->conidx             = conidx;
        memcpy(&(p_ind->ct_val), p_current_time, sizeof(tip_curr_time_t));
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
__STATIC void tipc_cb_ctnl_pt_req_cmp(uint8_t conidx, uint16_t status)
{
    tipc_send_cmp_evt(conidx, TIPC_WR_TIME_UPD_CTNL_PT_CMD_OP_CODE, status);
}

/// Default Message handle
__STATIC const tipc_cb_t tipc_msg_cb =
{
     .cb_enable_cmp              = tipc_cb_enable_cmp,
     .cb_read_cmp                = tipc_cb_read_cmp,
     .cb_write_cfg_cmp           = tipc_cb_write_cfg_cmp,
     .cb_curr_time               = tipc_cb_curr_time,
     .cb_ctnl_pt_req_cmp         = tipc_cb_ctnl_pt_req_cmp,
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
__STATIC uint16_t tipc_init(prf_data_t* p_env, uint16_t* p_start_hdl, uint8_t sec_lvl, uint8_t user_prio,
                          const void* p_params, const tipc_cb_t* p_cb)
{
    uint8_t conidx;
    // DB Creation Status
    uint16_t status = GAP_ERR_NO_ERROR;
    uint8_t user_lid = GATT_INVALID_USER_LID;

    do
    {
        tipc_env_t* p_tipc_env;

        #if (BLE_HL_MSG_API)
        if(p_cb == NULL)
        {
            p_cb = &(tipc_msg_cb);
        }
        #endif // (BLE_HL_MSG_API)

        if(   (p_params == NULL) || (p_cb == NULL) || (p_cb->cb_enable_cmp == NULL) || (p_cb->cb_read_cmp == NULL)
           || (p_cb->cb_write_cfg_cmp == NULL) || (p_cb->cb_curr_time == NULL) || (p_cb->cb_ctnl_pt_req_cmp == NULL))
        {
            status = GAP_ERR_INVALID_PARAM;
            break;
        }

        // register TIPC user
        status = gatt_user_cli_register(L2CAP_LE_MTU_MIN, user_prio, &tipc_cb, &user_lid);
        if(status != GAP_ERR_NO_ERROR) break;

        //-------------------- allocate memory required for the profile  ---------------------
        p_tipc_env = (tipc_env_t*) ke_malloc(sizeof(tipc_env_t), KE_MEM_ATT_DB);

        if(p_tipc_env != NULL)
        {
            // allocate TIPC required environment variable
            p_env->p_env = (prf_hdr_t *) p_tipc_env;

            // initialize environment variable
            p_tipc_env->prf_env.p_cb    = p_cb;
            #if (BLE_HL_MSG_API)
            p_env->desc.msg_handler_tab = tipc_msg_handler_tab;
            p_env->desc.msg_cnt         = ARRAY_LEN(tipc_msg_handler_tab);
            #endif // (BLE_HL_MSG_API)

            p_tipc_env->user_lid = user_lid;
            for (conidx = 0; conidx < BLE_CONNECTION_MAX; conidx++)
            {
                p_tipc_env->p_env[conidx] = NULL;
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
__STATIC uint16_t tipc_destroy(prf_data_t *p_env, uint8_t reason)
{
    uint16_t status = GAP_ERR_NO_ERROR;
    tipc_env_t* p_tipc_env = (tipc_env_t*) p_env->p_env;

    if(reason != PRF_DESTROY_RESET)
    {
        status = gatt_user_unregister(p_tipc_env->user_lid);
    }

    if(status == GAP_ERR_NO_ERROR)
    {
        uint8_t conidx;

        // cleanup environment variable for each task instances
        for (conidx = 0; conidx < BLE_CONNECTION_MAX; conidx++)
        {
            if (p_tipc_env->p_env[conidx] != NULL)
            {
                ke_free(p_tipc_env->p_env[conidx]);
            }
        }

        // free profile environment variables
        p_env->p_env = NULL;
        ke_free(p_tipc_env);
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
__STATIC void tipc_con_create(prf_data_t *p_env, uint8_t conidx, const gap_con_param_t* p_con_param)
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
__STATIC void tipc_con_cleanup(prf_data_t *p_env, uint8_t conidx, uint16_t reason)
{
    tipc_env_t* p_tipc_env = (tipc_env_t*) p_env->p_env;

    // clean-up environment variable allocated for task instance
    if (p_tipc_env->p_env[conidx] != NULL)
    {
        ke_free(p_tipc_env->p_env[conidx]);
        p_tipc_env->p_env[conidx] = NULL;
    }
}

/// TIPC Task interface required by profile manager
const prf_task_cbs_t tipc_itf =
{
    .cb_init          = (prf_init_cb) tipc_init,
    .cb_destroy       = tipc_destroy,
    .cb_con_create    = tipc_con_create,
    .cb_con_cleanup   = tipc_con_cleanup,
    .cb_con_upd       = NULL,
};

/**
 ****************************************************************************************
 * @brief Retrieve client profile interface
 *
 * @return Client profile interface
 ****************************************************************************************
 */
const prf_task_cbs_t* tipc_prf_itf_get(void)
{
    return &tipc_itf;
}

#endif /* (BLE_TIP_CLIENT) */

/// @} TIPC

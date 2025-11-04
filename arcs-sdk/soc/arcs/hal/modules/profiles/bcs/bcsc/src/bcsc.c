/**
 ****************************************************************************************
 *
 * @file bcsc.c
 *
 * @brief Body Composition Collector implementation.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 * $ Rev $
 *
 ****************************************************************************************
 */


/**
 ****************************************************************************************
 * @addtogroup BCSC
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "btip_config.h"

#if (BLE_BCS_CLIENT)
#include "bcsc.h"

#include "ble_prf.h"

#include "gatt.h"
#include "gap.h"

#include "ke_mem.h"

#include "co_utils.h"
#include "co_endian.h"

#include <string.h>

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
typedef struct bcsc_cnx_env
{
    /// Peer database discovered handle mapping
    bcsc_bcs_content_t bcs;
    /// Last measurement information
    bcs_meas_t         meas;
    /// counter used to check service uniqueness
    uint8_t            nb_svc;
    /// Client activity state
    bool               discover;
    /// True if second segment expected, False else
    bool               second_exp;
    /// Mass Resolution
    uint8_t            mass_resol;
    /// Height Resolution
    uint8_t            hght_resol;
    /// Measurement Units
    uint8_t            meas_u;
} bcsc_cnx_env_t;

/// Client environment variable
typedef struct bcsc_env
{
    /// profile environment
    prf_hdr_t            prf_env;
    /// Environment variable pointer for each connections
    bcsc_cnx_env_t*      p_env[BLE_CONNECTION_MAX];
    /// GATT User local identifier
    uint8_t              user_lid;
} bcsc_env_t;


/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */

/// State machine used to retrieve Body Composition Service characteristics information
const prf_char_def_t bcsc_bcs_char[BCSC_CHAR_BCS_MAX] =
{
    [BCSC_CHAR_BCS_FEATURE] = {GATT_CHAR_BODY_COMPOSITION_FEATURE,     ATT_REQ(PRES, MAND), PROP(RD) },
    [BCSC_CHAR_BCS_MEAS]    = {GATT_CHAR_BODY_COMPOSITION_MEASUREMENT, ATT_REQ(PRES, MAND), PROP(I)  },
};

/// State machine used to retrieve Body Composition Service characteristic description information
const prf_desc_def_t bcsc_bcs_char_desc[BCSC_DESC_BCS_MAX] =
{
    /// Client config
    [BCSC_DESC_BCS_MEAS_CCC] = {GATT_DESC_CLIENT_CHAR_CFG, ATT_REQ(PRES, MAND), BCSC_CHAR_BCS_MEAS},
};
/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Send discovery results to application.
 *
 * @param p_bcsc_env    Client Role task environment
 * @param conidx        Connection index
 * @param status        Response status code
 *****************************************************************************************
 */
__STATIC void bcsc_enable_cmp(bcsc_env_t* p_bcsc_env, uint8_t conidx, uint16_t status)
{
    const bcsc_cb_t* p_cb = (const bcsc_cb_t*) p_bcsc_env->prf_env.p_cb;

    if(p_bcsc_env != NULL)
    {
        bcsc_cnx_env_t* p_con_env = p_bcsc_env->p_env[conidx];
        p_cb->cb_enable_cmp(conidx, status, &(p_con_env->bcs));

        if (status != GAP_ERR_NO_ERROR)
        {
            // clean-up environment variable allocated for task instance
            ke_free(p_con_env);
            p_bcsc_env->p_env[conidx] = NULL;
        }
        else
        {
             p_con_env->discover   = false;
             p_con_env->second_exp = false;

             // Register profile handle to catch gatt indications
             gatt_cli_event_register(conidx, p_bcsc_env->user_lid, p_con_env->bcs.svc.shdl,
                                     p_con_env->bcs.svc.ehdl);
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
 * @param[in] p_data        Pointer of buffer that contains data value
 ****************************************************************************************
 */
__STATIC void bcsc_read_val_cmp(uint8_t conidx, uint16_t status, uint16_t val_id, co_buf_t* p_data)
{
    bcsc_env_t* p_bcsc_env = PRF_ENV_GET(BCSC, bcsc);

    if(p_bcsc_env != NULL)
    {
        const bcsc_cb_t* p_cb = (const bcsc_cb_t*) p_bcsc_env->prf_env.p_cb;

        switch(val_id)
        {
            case BCSC_READ_FEAT_OP_CODE:
            {
                uint32_t feature = 0;

                if(status == CO_BUF_ERR_NO_ERROR)
                {
                    feature = co_btohl(co_read32p(co_buf_data(p_data)));
                }

                p_cb->cb_read_feature_cmp(conidx, status, feature);
            }break;
            case BCSC_READ_CCC_OP_CODE:
            {
                uint16_t ind_cfg = 0;

                if(status == CO_BUF_ERR_NO_ERROR)
                {
                    ind_cfg = co_btohs(co_read16p(co_buf_data(p_data)));
                }

                p_cb->cb_read_ind_cfg_cmp(conidx, status, ind_cfg);
            }break;
            default: { /* Nothing to do */ } break;
        }
    }
}


/**
 ****************************************************************************************
 * @brief Perform Value read procedure.
 *
 * @param[in] conidx        Connection index
 * @param[in] val_id        Value Identifier (@see enum bcsc_info)
 ****************************************************************************************
 */
__STATIC uint16_t bcsc_read_val(uint8_t conidx, uint16_t val_id)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    bcsc_env_t* p_bcsc_env = PRF_ENV_GET(BCSC, bcsc);

    if(p_bcsc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_bcsc_env->p_env[conidx] != NULL) && (!p_bcsc_env->p_env[conidx]->discover))
        {
            bcsc_cnx_env_t* p_con_env = p_bcsc_env->p_env[conidx];
            uint16_t hdl;
            bcsc_bcs_content_t* p_bcs = &(p_con_env->bcs);

            switch(val_id)
            {
                case BCSC_READ_FEAT_OP_CODE:    { hdl = p_bcs->chars[BCSC_CHAR_BCS_FEATURE].val_hdl;              } break;
                case BCSC_READ_CCC_OP_CODE:     { hdl = p_bcs->descs[BCSC_DESC_BCS_MEAS_CCC].desc_hdl;         } break;
                default:                        { hdl = GATT_INVALID_HDL;                                       } break;
            }

            if(hdl == GATT_INVALID_HDL)
            {
                status = PRF_ERR_INEXISTENT_HDL;
            }
            else
            {
                // perform read request
                status = gatt_cli_read(conidx, p_bcsc_env->user_lid, val_id, hdl, 0, 0);
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
__STATIC void bcsc_svc_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t hdl, uint8_t disc_info,
                          uint8_t nb_att, const gatt_svc_att_t* p_atts)
{
    // Get the address of the environment
    bcsc_env_t* p_bcsc_env = PRF_ENV_GET(BCSC, bcsc);

    if(p_bcsc_env != NULL)
    {
        bcsc_cnx_env_t* p_con_env = p_bcsc_env->p_env[conidx];

        ASSERT_INFO(p_con_env != NULL, conidx, user_lid);

        if (p_con_env->nb_svc == 0)
        {
            //Even if we get multiple responses we only store 1 range
            if((disc_info == GATT_SVC_CMPLT) || (disc_info == GATT_SVC_START))
            {
                p_con_env->bcs.svc.shdl = hdl;
            }

            if((disc_info == GATT_SVC_CMPLT) || (disc_info == GATT_SVC_END))
            {
               p_con_env->bcs.svc.ehdl = hdl + nb_att -1;
            }

            // Retrieve characteristics
            prf_extract_svc_info(hdl, nb_att, p_atts,
                                 BCSC_CHAR_BCS_MAX, &bcsc_bcs_char[0],      &(p_con_env->bcs.chars[0]),
                                 BCSC_DESC_BCS_MAX, &bcsc_bcs_char_desc[0], &(p_con_env->bcs.descs[0]));
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
__STATIC void bcsc_discover_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    // Get the address of the environment
    bcsc_env_t* p_bcsc_env = PRF_ENV_GET(BCSC, bcsc);

    if(p_bcsc_env != NULL)
    {
        bcsc_cnx_env_t* p_con_env = p_bcsc_env->p_env[conidx];

        if (p_bcsc_env->p_env[conidx]->nb_svc >=  1)
        {
            status = prf_check_svc_validity(BCSC_DESC_BCS_MAX, p_con_env->bcs.chars, bcsc_bcs_char,
                                            BCSC_DESC_BCS_MAX, p_con_env->bcs.descs, bcsc_bcs_char_desc);
        }
        // no services found
        else
        {
            status = PRF_ERR_STOP_DISC_CHAR_MISSING;
        }

        bcsc_enable_cmp(p_bcsc_env, conidx, status);
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
__STATIC void bcsc_att_val_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t hdl, uint16_t offset,
                              co_buf_t* p_data)
{
    bcsc_read_val_cmp(conidx, GAP_ERR_NO_ERROR, (uint8_t) dummy, p_data);
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
__STATIC void bcsc_read_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    if(status != GAP_ERR_NO_ERROR)
    {
        bcsc_read_val_cmp(conidx, status, (uint8_t) dummy, NULL);
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
__STATIC void bcsc_write_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    bcsc_env_t* p_bcsc_env = PRF_ENV_GET(BCSC, bcsc);

    if(p_bcsc_env != NULL)
    {
        const bcsc_cb_t* p_cb = (const bcsc_cb_t*) p_bcsc_env->prf_env.p_cb;
        p_cb->cb_write_ind_cfg_cmp(conidx, status);
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
__STATIC void bcsc_att_val_evt_cb(uint8_t conidx, uint8_t user_lid, uint16_t token, uint8_t evt_type, bool complete,
                                  uint16_t hdl, co_buf_t* p_data)
{
    // Get the address of the environment
    bcsc_env_t* p_bcsc_env = PRF_ENV_GET(BCSC, bcsc);

    if(p_bcsc_env != NULL)
    {
        bcsc_cnx_env_t* p_con_env = p_bcsc_env->p_env[conidx];

        if(p_con_env != NULL)
        {
            const bcsc_cb_t* p_cb = (const bcsc_cb_t*) p_bcsc_env->prf_env.p_cb;

            if (hdl == p_con_env->bcs.chars[BCSC_CHAR_BCS_MEAS].val_hdl)
            {
                bool second_segment_of_multipacket;
                bcs_meas_t* p_meas = &(p_con_env->meas);
                uint16_t    flags  = co_btohs(co_read16p(co_buf_data(p_data)));
                co_buf_head_release(p_data, 2);

                if(GETB(flags, BCS_MEAS_FLAGS_MULTIPACKET_MEAS) && p_con_env->second_exp)
                {
                    p_con_env->second_exp         = false;
                    second_segment_of_multipacket = true;
                }
                else
                {
                    memset(p_meas, 0, sizeof(bcs_meas_t));
                    p_con_env->mass_resol = 0;
                    p_con_env->hght_resol = 0;
                    p_con_env->meas_u = 0;
                    second_segment_of_multipacket = false;
                    p_con_env->second_exp         = GETB(flags, BCS_MEAS_FLAGS_MULTIPACKET_MEAS);
                }

                // Body Fat Percentage is mandatory
                p_meas->body_fat_percent = co_btohs(co_read16p(co_buf_data(p_data)));
                co_buf_head_release(p_data, 2);

                // shall not be considered if it's in the second segment of multipacket
                if (!second_segment_of_multipacket)
                {
                    // Measurement Units
                    p_con_env->meas_u = co_buf_data(p_data)[0];
                    co_buf_head_release(p_data, 1);
                }

                // Time Stamp if present
                if (GETB(flags, BCS_MEAS_FLAGS_TIMESTAMP_PRESENT))
                {
                    prf_unpack_date_time(p_data, &(p_meas->time_stamp));
                }
                // User Id if present
                if (GETB(flags, BCS_MEAS_FLAGS_USER_ID_PRESENT))
                {
                    p_meas->user_id = co_buf_data(p_data)[0];
                    co_buf_head_release(p_data, 1);
                }

                // Basal Metabolism if present
                if (GETB(flags, BCS_MEAS_FLAGS_BASAL_METAB_PRESENT))
                {
                    p_meas->basal_metab = co_btohs(co_read16p(co_buf_data(p_data)));
                    co_buf_head_release(p_data, 2);
                }

                // Muscle Percentage if present
                if (GETB(flags, BCS_MEAS_FLAGS_MUSCLE_PERCENT_PRESENT))
                {
                    p_meas->muscle_percent = co_btohs(co_read16p(co_buf_data(p_data)));
                    co_buf_head_release(p_data, 2);
                }

                // Muscle Mass if present
                if (GETB(flags, BCS_MEAS_FLAGS_MUSCLE_MASS_PRESENT))
                {
                    p_meas->muscle_mass = co_btohs(co_read16p(co_buf_data(p_data)));
                    co_buf_head_release(p_data, 2);
                }

                // Fat Free Mass if present
                if (GETB(flags, BCS_MEAS_FLAGS_FAT_FREE_MASS_PRESENT))
                {
                    p_meas->fat_free_mass = co_btohs(co_read16p(co_buf_data(p_data)));
                    co_buf_head_release(p_data, 2);
                }

                // Soft Lean Mass if present
                if (GETB(flags, BCS_MEAS_FLAGS_SOFT_LEAN_MASS_PRESENT))
                {
                    p_meas->soft_lean_mass = co_btohs(co_read16p(co_buf_data(p_data)));
                    co_buf_head_release(p_data, 2);
                }

                // Body Water Mass if present
                if (GETB(flags, BCS_MEAS_FLAGS_BODY_WATER_MASS_PRESENT))
                {
                    p_meas->body_water_mass = co_btohs(co_read16p(co_buf_data(p_data)));
                    co_buf_head_release(p_data, 2);
                }

                // Impedance if present
                if (GETB(flags, BCS_MEAS_FLAGS_IMPEDANCE_PRESENT))
                {
                    p_meas->impedance = co_btohs(co_read16p(co_buf_data(p_data)));
                    co_buf_head_release(p_data, 2);
                }

                // Weight if present
                if (GETB(flags, BCS_MEAS_FLAGS_WEIGHT_PRESENT))
                {
                    p_meas->weight = co_btohs(co_read16p(co_buf_data(p_data)));
                    co_buf_head_release(p_data, 2);

                    // Mass resolution
                    p_con_env->mass_resol = co_buf_data(p_data)[0];
                    co_buf_head_release(p_data, 1);
                }

                // Height if present
                if (GETB(flags, BCS_MEAS_FLAGS_HEIGHT_PRESENT))
                {
                    p_meas->height = co_btohs(co_read16p(co_buf_data(p_data)));
                    co_buf_head_release(p_data, 2);

                    // Height resolution
                    p_con_env->hght_resol = co_buf_data(p_data)[0];
                    co_buf_head_release(p_data, 1);
                }

                // update flag status
                if (second_segment_of_multipacket)
                {
                    p_meas->flags |= flags;
                    SETB(p_meas->flags, BCS_MEAS_FLAGS_MULTIPACKET_MEAS, 0);
                }
                else
                {
                    p_meas->flags = flags;
                }

                // if no more fragment expected, consider job done
                if(!p_con_env->second_exp)
                {
                    p_cb->cb_meas(conidx, p_meas, p_con_env->meas_u, p_con_env->mass_resol, p_con_env->hght_resol);
                }
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
__STATIC void bcsc_svc_changed_cb(uint8_t conidx, uint8_t user_lid, bool out_of_sync, uint16_t start_hdl, uint16_t end_hdl)
{
    // Do Nothing
}

/// Client callback hander
__STATIC const gatt_cli_cb_t bcsc_cb =
{
    .cb_discover_cmp    = bcsc_discover_cmp_cb,
    .cb_read_cmp        = bcsc_read_cmp_cb,
    .cb_write_cmp       = bcsc_write_cmp_cb,
    .cb_att_val_get     = NULL,
    .cb_svc             = bcsc_svc_cb,
    .cb_svc_info        = NULL,
    .cb_inc_svc         = NULL,
    .cb_char            = NULL,
    .cb_desc            = NULL,
    .cb_att_val         = bcsc_att_val_cb,
    .cb_att_val_evt     = bcsc_att_val_evt_cb,
    .cb_svc_changed     = bcsc_svc_changed_cb,
};

/*
 * PROFILE NATIVE API
 ****************************************************************************************
 */

uint16_t bcsc_enable(uint8_t conidx, uint8_t con_type, uint8_t svc_type, const bcsc_bcs_content_t* p_bcs)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    bcsc_env_t* p_bcsc_env = PRF_ENV_GET(BCSC, bcsc);

    if(p_bcsc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_bcsc_env->p_env[conidx] == NULL))
        {
            // allocate environment variable for task instance
            p_bcsc_env->p_env[conidx] = (struct bcsc_cnx_env *) ke_malloc(sizeof(struct bcsc_cnx_env), KE_MEM_ATT_DB);

            if(p_bcsc_env->p_env[conidx] != NULL)
            {
                // Config connection, start discovering
                if (con_type == PRF_CON_DISCOVERY)
                {
                    uint16_t gatt_svc_uuid = GATT_SVC_BODY_COMPOSITION;
                    uint8_t disc_type = (svc_type == PRF_PRIMARY_SERVICE)
                                      ? GATT_DISCOVER_SVC_PRIMARY_BY_UUID
                                      : GATT_DISCOVER_SVC_SECONDARY_BY_UUID;

                    memset(p_bcsc_env->p_env[conidx], 0, sizeof(struct bcsc_cnx_env));

                    // start discovery
                    status = gatt_cli_discover_svc(conidx, p_bcsc_env->user_lid, 0, disc_type, true,
                                                   GATT_MIN_HDL, GATT_MAX_HDL, GATT_UUID_16, (uint8_t*) &gatt_svc_uuid);

                    // Go to DISCOVERING state
                    p_bcsc_env->p_env[conidx]->discover = true;
                }
                // normal connection, get saved att details
                else
                {
                    memcpy(&(p_bcsc_env->p_env[conidx]->bcs), p_bcs, sizeof(bcsc_bcs_content_t));
                    status = GAP_ERR_NO_ERROR;

                    // send APP confirmation that can start normal connection to TH
                    bcsc_enable_cmp(p_bcsc_env, conidx, GAP_ERR_NO_ERROR);
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

uint16_t bcsc_read_feature(uint8_t conidx)
{
    uint16_t status = bcsc_read_val(conidx,  BCSC_READ_FEAT_OP_CODE);
    return (status);
}

uint16_t bcsc_read_ind_cfg(uint8_t conidx)
{
    uint16_t status = bcsc_read_val(conidx, BCSC_READ_CCC_OP_CODE);
    return (status);
}

uint16_t bcsc_write_ind_cfg(uint8_t conidx, uint16_t ind_cfg)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    bcsc_env_t* p_bcsc_env = PRF_ENV_GET(BCSC, bcsc);

    if(p_bcsc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_bcsc_env->p_env[conidx] != NULL) && (!p_bcsc_env->p_env[conidx]->discover))
        {
            bcsc_cnx_env_t* p_con_env = p_bcsc_env->p_env[conidx];

            if (ind_cfg > PRF_CLI_START_IND)
            {
                status = PRF_ERR_INVALID_PARAM;
            }
            else
            {
                uint16_t hdl = p_con_env->bcs.descs[BCSC_DESC_BCS_MEAS_CCC].desc_hdl;;

                if(hdl == GATT_INVALID_HDL)
                {
                    status = PRF_ERR_INEXISTENT_HDL;
                }
                else
                {
                    // Force endianess
                    ind_cfg = co_htobs(ind_cfg);
                    status = prf_gatt_write(conidx, p_bcsc_env->user_lid, 0, GATT_WRITE,
                                            hdl, sizeof(uint16_t), (uint8_t *)&ind_cfg);
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
 * @brief Send a BCSC_CMP_EVT message to the task which enabled the profile
 *
 * @param[in] conidx       Connection Identifier
 * @param[in] operation    Indicates the operation for which the cmp_evt is being sent.
 * @param[in] status       Indicates the outcome of the operation
 ****************************************************************************************
 */
void bcsc_send_cmp_evt(uint8_t conidx, uint8_t operation, uint16_t status)
{
    struct bcsc_cmp_evt *p_evt = NULL;

    // Send the message
    p_evt = KE_MSG_ALLOC(BCSC_CMP_EVT,PRF_DST_TASK(BCSC), PRF_SRC_TASK(BCSC), bcsc_cmp_evt);

    if(p_evt != NULL)
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
__STATIC int bcsc_enable_req_handler(ke_msg_id_t const msgid, struct bcsc_enable_req const *p_param,
                                     ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = bcsc_enable(p_param->conidx, p_param->con_type, p_param->svc_type, &(p_param->bcs));

    // send an error if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct bcsc_enable_rsp *p_rsp = KE_MSG_ALLOC(BCSC_ENABLE_RSP, src_id, dest_id, bcsc_enable_rsp);
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
 * @brief Handles reception of the @ref BCSC_RD_FEAT_CMD message from the application.
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

__STATIC int bcsc_rd_feat_cmd_handler(ke_msg_id_t const msgid,
        struct bcsc_rd_feat_cmd *p_param,
        ke_task_id_t const dest_id,
        ke_task_id_t const src_id)
{
    uint16_t status = bcsc_read_feature(p_param->conidx);

    if (status != GAP_ERR_NO_ERROR)
    {
        bcsc_send_cmp_evt(p_param->conidx, BCSC_READ_FEAT_OP_CODE, status);
    }

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief Handles reception of the @ref BCSC_RD_MEAS_CCC_CMD  message from the application.
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

__STATIC int bcsc_rd_meas_ccc_cmd_handler(ke_msg_id_t const msgid,
        struct bcsc_rd_meas_ccc_cmd *p_param,
        ke_task_id_t const dest_id,
        ke_task_id_t const src_id)
{
    uint16_t status = bcsc_read_ind_cfg(p_param->conidx);

    if (status != GAP_ERR_NO_ERROR)
    {
        bcsc_send_cmp_evt(p_param->conidx, BCSC_READ_CCC_OP_CODE, status);
    }
    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief Handles reception of the @ref BCSC_WR_MEAS_CCC_CMD message.
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
__STATIC int bcsc_wr_meas_ccc_cmd_handler(ke_msg_id_t const msgid,
                                   struct bcsc_wr_meas_ccc_cmd *p_cmd,
                                   ke_task_id_t const dest_id,
                                   ke_task_id_t const src_id)
{
    uint16_t status = bcsc_write_ind_cfg(p_cmd->conidx, p_cmd->ccc);

    if (status != GAP_ERR_NO_ERROR)
    {
        bcsc_send_cmp_evt(p_cmd->conidx, BCSC_WRITE_CCC_OP_CODE, status);
    }
    return (KE_MSG_CONSUMED);
}

/// Default State handlers definition
KE_MSG_HANDLER_TAB(bcsc)
{
    // Note: all messages must be sorted in ID ascending order

    { BCSC_ENABLE_REQ,        (ke_msg_func_t)bcsc_enable_req_handler      },
    { BCSC_RD_FEAT_CMD,       (ke_msg_func_t)bcsc_rd_feat_cmd_handler     },
    { BCSC_RD_MEAS_CCC_CMD,   (ke_msg_func_t)bcsc_rd_meas_ccc_cmd_handler },
    { BCSC_WR_MEAS_CCC_CMD,   (ke_msg_func_t)bcsc_wr_meas_ccc_cmd_handler },
};

/**
 ****************************************************************************************
 * @brief Completion of enable procedure
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 * @param[in] p_bas         Pointer to peer database description bond data
 ****************************************************************************************
 */
void bcsc_cb_enable_cmp(uint8_t conidx, uint16_t status, const bcsc_bcs_content_t* p_bcs)
{
    // Send APP the details of the discovered attributes on BCSC
    struct bcsc_enable_rsp *p_rsp = KE_MSG_ALLOC(BCSC_ENABLE_RSP, PRF_DST_TASK(BCSC), PRF_SRC_TASK(BCSC),
                                                 bcsc_enable_rsp);
    if(p_rsp != NULL)
    {
        p_rsp->conidx = conidx;
        p_rsp->status = status;
        memcpy(&(p_rsp->bcs), p_bcs, sizeof(bcsc_bcs_content_t));
        ke_msg_send(p_rsp);
    }
}

/**
 ****************************************************************************************
 * @brief Inform that read the body composition feature procedure is over
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 * @param[in] feature       Body composition feature
 ****************************************************************************************
 */
void bcsc_cb_read_feature_cmp(uint8_t conidx, uint16_t status, uint32_t feature)
{
    if(status == GAP_ERR_NO_ERROR)
    {
        struct bcsc_feat_ind *p_ind = KE_MSG_ALLOC(BCSC_FEAT_IND, PRF_DST_TASK(BCSC),
                                                   PRF_SRC_TASK(BCSC), bcsc_feat_ind);
        if(p_ind != NULL)
        {
            p_ind->conidx   = conidx;
            p_ind->feature = feature;
            ke_msg_send(p_ind);
        }
    }
    bcsc_send_cmp_evt(conidx, BCSC_READ_FEAT_OP_CODE, status);
}

/**
 ****************************************************************************************
 * @brief Inform that read indication configuration procedure is over
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 * @param[in] ind_cfg       Indication configuration
 ****************************************************************************************
 */
void bcsc_cb_read_ind_cfg_cmp(uint8_t conidx, uint16_t status, uint16_t ind_cfg)
{
    if(status == GAP_ERR_NO_ERROR)
    {
        struct bcsc_meas_ccc_ind *p_ind = KE_MSG_ALLOC(BCSC_MEAS_CCC_IND, PRF_DST_TASK(BCSC),
                                                       PRF_SRC_TASK(BCSC), bcsc_meas_ccc_ind);
        if(p_ind != NULL)
        {
            p_ind->conidx   = conidx;
            p_ind->ccc      = ind_cfg;
            ke_msg_send(p_ind);
        }
    }
    bcsc_send_cmp_evt(conidx, BCSC_READ_CCC_OP_CODE, status);
}

/**
 ****************************************************************************************
 * @brief Inform that indication configuration write procedure is over
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 ****************************************************************************************
 */
void bcsc_cb_write_ind_cfg_cmp(uint8_t conidx, uint16_t status)
{
    bcsc_send_cmp_evt(conidx, BCSC_WRITE_CCC_OP_CODE, status);
}

/**
 ****************************************************************************************
 * @brief Function called when new body configuration measurement information is received
 *
 * @param[in] conidx        Connection index
 * @param[in] p_meas        Pointer to body configuration measurement information
 * @param[in] meas_u        Measurement Units
 * @param[in] mass_resol    Mass Resolution
 * @param[in] hght_resol    Height Resolution
 ****************************************************************************************
 */
void bcsc_cb_meas(uint8_t conidx, const bcs_meas_t* p_meas, uint8_t meas_u, uint8_t mass_resol, uint8_t hght_resol)
{
    struct bcsc_meas_ind *p_ind = KE_MSG_ALLOC(BCSC_MEAS_IND, PRF_DST_TASK(BCSC), PRF_SRC_TASK(BCSC), bcsc_meas_ind);
    if(p_ind != NULL)
    {
        p_ind->conidx              = conidx;
        p_ind->flags               = p_meas->flags;
        p_ind->body_fat_percent    = p_meas->body_fat_percent;
        p_ind->meas_u              = meas_u;
        memcpy(&(p_ind->time_stamp), &(p_meas->time_stamp), sizeof(prf_date_time_t));
        p_ind->user_id             = p_meas->user_id;
        p_ind->basal_metab         = p_meas->basal_metab;
        p_ind->muscle_percent      = p_meas->muscle_percent;
        p_ind->muscle_mass         = p_meas->muscle_mass;
        p_ind->fat_free_mass       = p_meas->fat_free_mass;
        p_ind->soft_lean_mass      = p_meas->soft_lean_mass;
        p_ind->body_water_mass     = p_meas->body_water_mass;
        p_ind->impedance           = p_meas->impedance;
        p_ind->weight              = p_meas->weight;
        p_ind->mass_resol          = mass_resol;
        p_ind->height              = p_meas->height;
        p_ind->hght_resol          = hght_resol;
        ke_msg_send(p_ind);
    }
}


/// Default Message handle
__STATIC const bcsc_cb_t bcsc_msg_cb =
{
    .cb_enable_cmp        = bcsc_cb_enable_cmp,
    .cb_read_feature_cmp  = bcsc_cb_read_feature_cmp,
    .cb_read_ind_cfg_cmp  = bcsc_cb_read_ind_cfg_cmp,
    .cb_write_ind_cfg_cmp = bcsc_cb_write_ind_cfg_cmp,
    .cb_meas              = bcsc_cb_meas,
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
__STATIC uint16_t bcsc_init(prf_data_t* p_env, uint16_t* p_start_hdl, uint8_t sec_lvl, uint8_t user_prio,
                          const void* p_params, const bcsc_cb_t* p_cb)
{
    uint8_t conidx;
    // DB Creation Status
    uint16_t status = GAP_ERR_NO_ERROR;
    uint8_t user_lid = GATT_INVALID_USER_LID;

    do
    {
        bcsc_env_t* p_bcsc_env;

        #if (BLE_HL_MSG_API)
        if(p_cb == NULL)
        {
            p_cb = &(bcsc_msg_cb);
        }
        #endif // (BLE_HL_MSG_API)

        if(   (p_params == NULL) || (p_cb == NULL) || (p_cb->cb_enable_cmp == NULL) || (p_cb->cb_read_feature_cmp == NULL)
           || (p_cb->cb_read_ind_cfg_cmp == NULL) || (p_cb->cb_write_ind_cfg_cmp == NULL) || (p_cb->cb_meas == NULL))
        {
            status = GAP_ERR_INVALID_PARAM;
            break;
        }

        // register BCSC user
        status = gatt_user_cli_register(BCS_MEAS_IND_SIZE, user_prio, &bcsc_cb, &user_lid);
        if(status != GAP_ERR_NO_ERROR) break;

        //-------------------- allocate memory required for the profile  ---------------------
        p_bcsc_env = (bcsc_env_t*) ke_malloc(sizeof(bcsc_env_t), KE_MEM_ATT_DB);

        if(p_bcsc_env != NULL)
        {
            // allocate BCSC required environment variable
            p_env->p_env = (prf_hdr_t *) p_bcsc_env;

            // initialize environment variable
            p_bcsc_env->prf_env.p_cb    = p_cb;
            #if (BLE_HL_MSG_API)
            p_env->desc.msg_handler_tab = bcsc_msg_handler_tab;
            p_env->desc.msg_cnt         = ARRAY_LEN(bcsc_msg_handler_tab);
            #endif // (BLE_HL_MSG_API)

            p_bcsc_env->user_lid = user_lid;
            for (conidx = 0; conidx < BLE_CONNECTION_MAX; conidx++)
            {
                p_bcsc_env->p_env[conidx] = NULL;
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
__STATIC uint16_t bcsc_destroy(prf_data_t *p_env, uint8_t reason)
{
    uint16_t status = GAP_ERR_NO_ERROR;
    bcsc_env_t* p_bcsc_env = (bcsc_env_t*) p_env->p_env;

    if(reason != PRF_DESTROY_RESET)
    {
        status = gatt_user_unregister(p_bcsc_env->user_lid);
    }

    if(status == GAP_ERR_NO_ERROR)
    {
        uint8_t idx;

        // cleanup environment variable for each task instances
        for (idx = 0; idx < BLE_CONNECTION_MAX; idx++)
        {
            if (p_bcsc_env->p_env[idx] != NULL)
            {
                ke_free(p_bcsc_env->p_env[idx]);
            }
        }

        // free profile environment variables
        p_env->p_env = NULL;
        ke_free(p_bcsc_env);
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
__STATIC void bcsc_con_create(prf_data_t *p_env, uint8_t conidx, const gap_con_param_t* p_con_param)
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
__STATIC void bcsc_con_cleanup(prf_data_t *p_env, uint8_t conidx, uint16_t reason)
{
    bcsc_env_t* p_bcsc_env = (bcsc_env_t*) p_env->p_env;

    // clean-up environment variable allocated for task instance
    if (p_bcsc_env->p_env[conidx] != NULL)
    {
        ke_free(p_bcsc_env->p_env[conidx]);
        p_bcsc_env->p_env[conidx] = NULL;
    }
}

/// BCSC Task interface required by profile manager
const prf_task_cbs_t bcsc_itf =
{
    .cb_init          = (prf_init_cb) bcsc_init,
    .cb_destroy       = bcsc_destroy,
    .cb_con_create    = bcsc_con_create,
    .cb_con_cleanup   = bcsc_con_cleanup,
    .cb_con_upd       = NULL,
};

/**
 ****************************************************************************************
 * @brief Retrieve client profile interface
 *
 * @return Client profile interface
 ****************************************************************************************
 */
const prf_task_cbs_t* bcsc_prf_itf_get(void)
{
    return &bcsc_itf;
}
#endif //(BLE_BCS_CLIENT)

/// @} BCSC

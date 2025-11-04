/**
 ****************************************************************************************
 *
 * @file basc.c
 *
 * @brief Battery Service Client implementation.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup BASC
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#define BLE_BATT_CLIENT         1

#if (BLE_BATT_CLIENT)

#include "basc.h"
#include "basc_msg.h"
#include "ble_cli_prf.h"
#include <string.h>

/*
 * DEFINES
 ****************************************************************************************
 */
/// Maximal length for Characteristic values - 23 bytes
#define BASC_VAL_MAX_LEN               (23)

/// Maximum number of Client task instances

/// Content of BASC dummy bit field
enum basc_dummy_bf
{
    /// BAS Instance
    BASC_DUMMY_BAS_INST_MASK = 0x00FF,
    BASC_DUMMY_BAS_INST_LSB  = 0,
    /// Value read
    BASC_DUMMY_VAL_ID_MASK   = 0xFF00,
    BASC_DUMMY_VAL_ID_LSB    = 8,
};



/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */


/// Environment variable for each Connections
typedef struct basc_cnx_env
{
    /// Peer database discovered handle mapping
    bas_content_t bas[BASC_NB_BAS_INSTANCES_MAX];
    /// counter used to check service uniqueness
    uint8_t       nb_svc;
    /// True if discovery procedure is on-going
    bool          discover;
} basc_cnx_env_t;

/// Client environment variable
typedef struct basc_env
{
    /// profile environment
    prf_hdr_t            prf_env;
    /// Environment variable pointer for each connections
    basc_cnx_env_t*      p_env[BLE_CONNECTION_MAX];
    /// GATT User local identifier
    uint8_t              user_lid;
} basc_env_t;


/*
 * LOCAL FUNCTIONS DECLARATION
 ****************************************************************************************
 */
#if 0//(BT_STACK_PRESENT)
__STATIC uint32_t basc_sdp_callback(struct sdp_service *service, enum sdp_cb_id id);
#endif

/*
 * LOCAL VARIABLES
 ****************************************************************************************
 */
#define BASC_NAME_MAX_LEN	10

#if 0//(BT_STACK_PRESENT)
/// sdp data
// service uuid
__STATIC uint8_t basc_uuid[16] = {0x00, 0x00, 0x18, 0x0F};


/// protocol l2cap
const struct sdp_data sdp_protocol_basc_l2cap[2] = {
        { .type = SDP_DE_UUID,                  .size = SDP_DE_SIZE_16, .value = BT_PROTOCOL_L2CAP      },
        // PSM
        { .type = SDP_DE_UINT,  				.size = SDP_DE_SIZE_8,  .value = L2CAP_PSM_ATT       },
};

const struct sdp_data sdp_protocol_basc_des[3] = {
        { .type = SDP_DE_UUID,                  .size = SDP_DE_SIZE_16, .value = BT_PROTOCOL_ATT      },
        // start handle
        { .type = SDP_DE_UINT,  				.size = SDP_DE_SIZE_16,  .value = 0x0001       },
        // end handle
		{ .type = SDP_DE_UINT,  				.size = SDP_DE_SIZE_16,  .value = 0x0009       },
};

/// protocol basc
const struct sdp_data sdp_protocol_basc[2] = {
        { .type = SDP_DE_DES,                   .size = 2,              .des = sdp_protocol_basc_l2cap       },
        { .type = SDP_DE_DES,                   .size = 3,              .des = sdp_protocol_basc_des       },
};

/// service class list
__STATIC const struct sdp_data basc_sdp_service[2] = {
        { .type = SDP_DE_UUID,                      .size = SDP_DE_SIZE_128,    .data = basc_uuid                },
};

/// basc profile descriptor list
__STATIC const struct sdp_data basc_sdp_basc_profile[2] = {
        { .type = SDP_DE_UUID,          .size = SDP_DE_SIZE_16,     .value = 0x180F       },
        { .type = SDP_DE_UINT,          .size = SDP_DE_SIZE_16,     .value = 0x0102       },
};

/// basc profile descriptor list
__STATIC const struct sdp_data basc_sdp_basc[1] = {
        { .type = SDP_DE_DES,           .size = 2,                  .des = basc_sdp_basc_profile                  },
};



__STATIC const struct sdp_attribute basc_sdp_basc_attrs[] = {
        {.id   = BT_ATTRIBUTE_SERVICE_CLASS_ID_LIST,
         .data = { .type = SDP_DE_DES,                  .size = 1,              .des = basc_sdp_service          }},
        {.id   = BT_ATTRIBUTE_PROTOCOL_DESCRIPTOR_LIST,
         .data = { .type = SDP_DE_DES,                  .size = 2,              .des = sdp_protocol_basc      	 }},
        {.id   = BT_ATTRIBUTE_BROWSE_GROUP_LIST,
         .data = { .type = SDP_DE_DES,                  .size = 1,              .des = sdp_browse_list           }},
	    {.id   = BT_ATTRIBUTE_BLUETOOTH_PROFILE_DESCRIPTOR_LIST,
	     .data = { .type = SDP_DE_DES,                  .size = 1,              .des = basc_sdp_basc             }},

};


__STATIC struct sdp_service basc_sdp_service_basc =
{
        .cb  = basc_sdp_callback,
        .attr_count = sizeof(basc_sdp_basc_attrs)/sizeof(basc_sdp_basc_attrs[0]),
        .attrs = basc_sdp_basc_attrs,
};
#endif

/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */
struct basc_env basc_env;
basc_env_t* p_basc_env = &basc_env;


/// State machine used to retrieve Battery Service characteristics information
const prf_char_def_t basc_bas_char[BAS_CHAR_MAX] =
{
    /// Battery Level
    [BAS_CHAR_BATT_LEVEL]  = { BLE_GATT_CHAR_BATTERY_LEVEL, ATT_REQ(PRES, MAND), BLE_PROP(RD) },
};

/// State machine used to retrieve Battery Service characteristic description information
const prf_desc_def_t basc_bas_char_desc[BAS_DESC_MAX] =
{
    /// Battery Level Characteristic Presentation Format
    [BAS_DESC_BATT_LEVEL_PRES_FORMAT]   = {BLE_GATT_DESC_CHAR_PRES_FORMAT,  ATT_REQ(PRES, OPT), BAS_CHAR_BATT_LEVEL},
    /// Battery Level Client Config
    [BAS_DESC_BATT_LEVEL_CFG]           = {BLE_GATT_DESC_CLIENT_CHAR_CFG,   ATT_REQ(PRES, OPT), BAS_CHAR_BATT_LEVEL},
};


/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */
#if 0//(BT_STACK_PRESENT)
__STATIC uint32_t basc_sdp_callback(struct sdp_service *service, enum sdp_cb_id id)
{
    uint32_t res = 0;

    switch(id)
    {
    case SDP_CB_HANDLE:
        res = service->hdl;
        break;
    }
    return res;
}
#endif


/**
 ****************************************************************************************
 * @brief Send discovery results to application.
 *
 * @param p_basc_env    Client Role task environment
 * @param conidx        Connection index
 * @param status        Response status code
 *****************************************************************************************
 */
__STATIC void basc_enable_cmp(basc_env_t* p_basc_env, uint8_t conidx, uint16_t status)
{
    const basc_cb_t* p_cb = (const basc_cb_t*) p_basc_env->prf_env.p_cb;

    if(p_basc_env != NULL)
    {
        basc_cnx_env_t* p_con_env = p_basc_env->p_env[conidx];

        if (status != BLE_GAP_ERR_NO_ERROR)
        {
            // clean-up environment variable allocated for task instance
            ke_free(p_con_env);
            p_basc_env->p_env[conidx] = NULL;
        }
        else
        {
            uint8_t cursor;
            p_con_env->discover = false;

            for(cursor = 0 ; cursor < p_con_env->nb_svc ; cursor++)
            {
                // Register profile handle to catch gatt indications
                gatt_cli_event_register(conidx, p_basc_env->user_lid, p_con_env->bas[cursor].svc.shdl,
                                        p_con_env->bas[cursor].svc.ehdl);
            }
        }
        p_cb->cb_enable_cmp(conidx, status, p_con_env->nb_svc, p_con_env->bas);
    }
}

/**
 ****************************************************************************************
 * @brief Send read result to application,.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the request
 * @param[in] dummy         Dummy token used to retrieve read on-going
 * @param[in] p_data        Pointer of buffer that contains data value
 ****************************************************************************************
 */
__STATIC void basc_read_val_cmp(uint8_t conidx, uint16_t status, uint16_t dummy, void* p_data)
{

    if(p_basc_env != NULL)
    {
        const basc_cb_t* p_cb = (const basc_cb_t*) p_basc_env->prf_env.p_cb;
        uint8_t bas_inst      = BLE_GETF(dummy, BASC_DUMMY_BAS_INST);
        uint8_t val_id        = BLE_GETF(dummy, BASC_DUMMY_VAL_ID);

        switch(val_id)
        {
            case BASC_BATT_LVL_VAL:
            {
                uint8_t batt_level = 0;

                if(status == BLE_GAP_ERR_NO_ERROR)
                {
                    uint8_t *level = (uint8_t *)p_data;
                    batt_level = level[0];
                }

                p_cb->cb_read_batt_level_cmp(conidx, status, bas_inst, batt_level);
            }break;
            case BASC_NTF_CFG:
            {
                uint16_t ntf_cfg = 0;

                if(status == BLE_GAP_ERR_NO_ERROR)
                {
                    ntf_cfg = ble_co_btohs(ble_co_read16p(p_data));
                }

                p_cb->cb_read_ntf_cfg_cmp(conidx, status, bas_inst, ntf_cfg);
            }break;
            case BASC_BATT_LVL_PRES_FORMAT:
            {
                prf_char_pres_fmt_t pres_format;

                if(status == BLE_GAP_ERR_NO_ERROR)
                {
//                    prf_unpack_char_pres_fmt(p_data, &(pres_format));
                }

                p_cb->cb_read_pres_format_cmp(conidx, status, bas_inst, &pres_format);
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
 * @param[in] bas_instance  Battery Service Instance - From 0 to BASC_NB_BAS_INSTANCES_MAX-1
 * @param[in] val_id        Value Identifier (@see enum basc_info)
 ****************************************************************************************
 */
__STATIC uint16_t basc_read_val(uint8_t conidx, uint8_t bas_instance, uint16_t val_id)
{
    uint16_t status = BLE_PRF_ERR_REQ_DISALLOWED;
    // Client environment
    //CLOGD("basc_read_val, env:0x%x,dis:%d", p_basc_env->p_env[conidx], p_basc_env->p_env[conidx]->discover);
    if(p_basc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_basc_env->p_env[conidx] != NULL) && (!p_basc_env->p_env[conidx]->discover))
        {
            basc_cnx_env_t* p_con_env = p_basc_env->p_env[conidx];

            if (bas_instance >= p_con_env->nb_svc)
            {
                status = BLE_PRF_ERR_INVALID_PARAM;
            }
            else
            {
                uint16_t hdl;
                bas_content_t* p_bas = &(p_con_env->bas[bas_instance]);

                switch(val_id)
                {
                    case BASC_BATT_LVL_VAL:         { hdl = p_bas->chars[BAS_CHAR_BATT_LEVEL].val_hdl;              } break;
                    case BASC_NTF_CFG:              { hdl = p_bas->descs[BAS_DESC_BATT_LEVEL_CFG].desc_hdl;         } break;
                    case BASC_BATT_LVL_PRES_FORMAT: { hdl = p_bas->descs[BAS_DESC_BATT_LEVEL_PRES_FORMAT].desc_hdl; } break;
                    default:                        { hdl = BLE_GATT_INVALID_HDL;                                       } break;
                }

                //CLOGD("basc_read_val, hdl:%d", hdl);

                if(hdl == BLE_GATT_INVALID_HDL)
                {
                    status = BLE_PRF_ERR_INEXISTENT_HDL;
                }
                else
                {
                    uint16_t dummy = 0;
                    BLE_SETF(dummy, BASC_DUMMY_BAS_INST, bas_instance);
                    BLE_SETF(dummy, BASC_DUMMY_VAL_ID,   val_id);
                    // perform read request
                    status = gatt_cli_read(conidx, p_basc_env->user_lid, dummy, hdl, 0, 0);
                }
                //CLOGD("basc_read_val, hdl:%d, status:0x%x", hdl, status);
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
__STATIC void basc_svc_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t hdl, uint8_t disc_info,
                          uint8_t nb_att, const ble_gatt_svc_att_t* p_atts)
{
    // Get the address of the environment

    //CLOGD("basc_svc_cb, p_atts:0x%x,hdl:%d,nb_att:%d",p_atts, hdl, nb_att);
    if(p_basc_env != NULL)
    {
        basc_cnx_env_t* p_con_env = p_basc_env->p_env[conidx];

        if(p_con_env != NULL)
        {
            if (p_con_env->nb_svc < BASC_NB_BAS_INSTANCES_MAX)
            {
                //Even if we get multiple responses we only store 1 range
                if((disc_info == BLE_GATT_SVC_CMPLT) || (disc_info == BLE_GATT_SVC_START))
                {
                    p_con_env->bas[p_con_env->nb_svc].svc.shdl = hdl;
                }

                if((disc_info == BLE_GATT_SVC_CMPLT) || (disc_info == BLE_GATT_SVC_END))
                {
                    p_con_env->bas[p_con_env->nb_svc].svc.ehdl = hdl + nb_att -1;
                }

                // Retrieve characteristics
                prf_extract_svc_info(hdl, nb_att, p_atts,
                                     BAS_CHAR_MAX, basc_bas_char, p_con_env->bas[p_con_env->nb_svc].chars,
                                     BAS_DESC_MAX, basc_bas_char_desc, p_con_env->bas[p_con_env->nb_svc].descs);

                if((disc_info == BLE_GATT_SVC_CMPLT) || (disc_info == BLE_GATT_SVC_END))
                {
                    p_con_env->nb_svc++;
                }
            }
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
__STATIC void basc_discover_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    if(p_basc_env != NULL)
    {
        basc_cnx_env_t* p_con_env = p_basc_env->p_env[conidx];

        if (p_con_env->nb_svc > 0)
        {
            uint8_t cursor;
            for (cursor = 0; (cursor < p_con_env->nb_svc) && (status == BLE_GAP_ERR_NO_ERROR); cursor++)
            {
                prf_desc_def_t bas_desc[BAS_DESC_MAX];
                status = prf_check_svc_char_validity(BAS_CHAR_MAX, p_con_env->bas[cursor].chars, basc_bas_char);
                if(status != BLE_GAP_ERR_NO_ERROR) break;

                memcpy(bas_desc, basc_bas_char_desc, sizeof(basc_bas_char_desc));

                if (p_con_env->nb_svc > 1)
                {
                    bas_desc[BAS_DESC_BATT_LEVEL_PRES_FORMAT].req_bf = ATT_REQ(PRES, MAND);
                }
                if ((p_con_env->bas[cursor].chars[BAS_CHAR_BATT_LEVEL].prop & BLE_PROP(N)) == BLE_PROP(N))
                {
                    bas_desc[BAS_DESC_BATT_LEVEL_CFG].req_bf = ATT_REQ(PRES, MAND);
                }

                status = prf_check_svc_desc_validity(BAS_DESC_MAX, p_con_env->bas[cursor].descs, bas_desc,
                                                          p_con_env->bas[cursor].chars);
            }
        }
        else
        {
            status = BLE_PRF_ERR_STOP_DISC_CHAR_MISSING;
        }

        basc_enable_cmp(p_basc_env, conidx, status);
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
__STATIC void basc_att_val_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t hdl, uint16_t offset,
                              void* p_data)
{
    basc_read_val_cmp(conidx, BLE_GAP_ERR_NO_ERROR, dummy, ble_co_buf_data(p_data));
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
__STATIC void basc_read_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    if(status != BLE_GAP_ERR_NO_ERROR)
    {
        basc_read_val_cmp(conidx, status, dummy, NULL);
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
__STATIC void basc_write_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    if(p_basc_env != NULL)
    {
        const basc_cb_t* p_cb = (const basc_cb_t*) p_basc_env->prf_env.p_cb;
        p_cb->cb_write_ntf_cfg_cmp(conidx, status, (uint8_t) dummy);
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
__STATIC void basc_att_val_evt_cb(uint8_t conidx, uint8_t user_lid, uint16_t token, uint8_t evt_type, bool complete,
                                  uint16_t hdl, void* p_data)
{
    if(p_basc_env != NULL)
    {
        basc_cnx_env_t* p_con_env = p_basc_env->p_env[conidx];

        if(p_con_env != NULL)
        {
            uint8_t bas_inst;
            const basc_cb_t* p_cb = (const basc_cb_t*) p_basc_env->prf_env.p_cb;

            // Search for Battery Level - BAS instance
            for (bas_inst = 0; (bas_inst < p_con_env->nb_svc); bas_inst++)
            {
                if (hdl == p_con_env->bas[bas_inst].chars[BAS_CHAR_BATT_LEVEL].val_hdl)
                {
                    uint8_t batt_level = *ble_co_buf_data(p_data);
                    p_cb->cb_batt_level_upd(conidx, bas_inst, batt_level);
                    break;
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
__STATIC void basc_svc_changed_cb(uint8_t conidx, uint8_t user_lid, bool out_of_sync, uint16_t start_hdl, uint16_t end_hdl)
{
    // Do Nothing
}

/// Client callback hander
__STATIC const ble_gatt_cli_cb_t basc_cb =
{
    .cb_discover_cmp    = basc_discover_cmp_cb,
    .cb_read_cmp        = basc_read_cmp_cb,
    .cb_write_cmp       = basc_write_cmp_cb,
    .cb_att_val_get     = NULL,
    .cb_svc             = basc_svc_cb,
    .cb_svc_info        = NULL,
    .cb_inc_svc         = NULL,
    .cb_char            = NULL,
    .cb_desc            = NULL,
    .cb_att_val         = basc_att_val_cb,
    .cb_att_val_evt     = basc_att_val_evt_cb,
    .cb_svc_changed     = basc_svc_changed_cb,
};

/*
 * PROFILE NATIVE API
 ****************************************************************************************
 */

uint16_t basc_enable(uint8_t conidx, uint8_t con_type, uint8_t nb_bas, const bas_content_t* p_bas)
{
    uint16_t status = BLE_PRF_ERR_REQ_DISALLOWED;

    if((nb_bas > BASC_NB_BAS_INSTANCES_MAX) || ((con_type == PRF_CON_NORMAL) && (p_bas == NULL)))
    {
        status = BLE_PRF_ERR_INVALID_PARAM;
    }
    else if(p_basc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_basc_env->p_env[conidx] == NULL))
        {
            // allocate environment variable for task instance
            p_basc_env->p_env[conidx] = (basc_cnx_env_t *) plf_malloc(sizeof(basc_cnx_env_t));

            if(p_basc_env->p_env[conidx] != NULL)
            {
                memset(p_basc_env->p_env[conidx], 0, sizeof(basc_cnx_env_t));
                // Config connection, start discovering
                if (con_type == PRF_CON_DISCOVERY)
                {
                    uint16_t gatt_svc_uuid = BLE_GATT_SVC_BATTERY_SERVICE;

                    // start discovery
                    status = ble_gatt_cli_discover_svc(conidx, p_basc_env->user_lid, 0, BLE_GATT_DISCOVER_SVC_PRIMARY_BY_UUID, true,
                                                   BLE_GATT_MIN_HDL, BLE_GATT_MAX_HDL, BLE_GATT_UUID_16, (uint8_t*) &gatt_svc_uuid);

                    // Go to DISCOVERING state
                    p_basc_env->p_env[conidx]->discover = true;
                }
                // normal connection, get saved att details
                else
                {
                    p_basc_env->p_env[conidx]->nb_svc = nb_bas;
                    memcpy(p_basc_env->p_env[conidx]->bas, p_bas, sizeof(bas_content_t) *nb_bas);
                    status = BLE_GAP_ERR_NO_ERROR;

                    // send APP confirmation that can start normal connection to TH
                    basc_enable_cmp(p_basc_env, conidx, BLE_GAP_ERR_NO_ERROR);
                }
            }
            else
            {
                status = BLE_GAP_ERR_INSUFF_RESOURCES;
            }
        }
    }

    return (status);
}

uint16_t basc_read_batt_level(uint8_t conidx, uint8_t bas_instance)
{
    uint16_t status = basc_read_val(conidx, bas_instance, BASC_BATT_LVL_VAL);
    return (status);
}

uint16_t basc_read_ntf_cfg(uint8_t conidx, uint8_t bas_instance)
{
    uint16_t status = basc_read_val(conidx, bas_instance, BASC_NTF_CFG);
    return (status);
}

uint16_t basc_read_pres_format(uint8_t conidx, uint8_t bas_instance)
{
    uint16_t status = basc_read_val(conidx, bas_instance, BASC_BATT_LVL_PRES_FORMAT);
    return (status);
}

uint16_t basc_write_ntf_cfg(uint8_t conidx, uint8_t bas_instance, uint16_t ntf_cfg)
{
    uint16_t status = BLE_PRF_ERR_REQ_DISALLOWED;

    if(p_basc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_basc_env->p_env[conidx] != NULL) && (!p_basc_env->p_env[conidx]->discover))
        {
            basc_cnx_env_t* p_con_env = p_basc_env->p_env[conidx];

            if ((bas_instance >= p_con_env->nb_svc) || (ntf_cfg > PRF_CLI_START_NTF))
            {
                status = BLE_PRF_ERR_INVALID_PARAM;
            }
            else
            {
                uint16_t hdl = p_con_env->bas[bas_instance].descs[BAS_DESC_BATT_LEVEL_CFG].desc_hdl;

                if(hdl == BLE_GATT_INVALID_HDL)
                {
                    status = BLE_PRF_ERR_INEXISTENT_HDL;
                }
                else
                {
                    // Force endianess
                    ntf_cfg = ble_co_btohs(ntf_cfg);
                    status = prf_gatt_write(conidx, p_basc_env->user_lid, bas_instance, BLE_GATT_WRITE,
                                            hdl, sizeof(uint16_t), (uint8_t *)&ntf_cfg);
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
 #if 0
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
__STATIC int basc_enable_req_handler(ke_msg_id_t const msgid, struct basc_enable_req const *p_param,
                                     ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = basc_enable(p_param->conidx, p_param->con_type, p_param->bas_nb, p_param->bas);

    // send an error if request fails
    if (status != BLE_GAP_ERR_NO_ERROR)
    {
        struct basc_enable_rsp *p_rsp = KE_MSG_ALLOC(BASC_ENABLE_RSP, src_id, dest_id, basc_enable_rsp);
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
 * @brief Handles reception of the @ref BASC_READ_INFO_REQ message.
 * @param[in] msgid Id of the message received (probably unused).
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int basc_read_info_req_handler(ke_msg_id_t const msgid, struct basc_read_info_req const *p_param,
                                        ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status;

    switch(p_param->info)
    {
        case BASC_BATT_LVL_VAL:         { status = basc_read_batt_level(p_param->conidx, p_param->bas_nb);  } break;
        case BASC_NTF_CFG:              { status = basc_read_ntf_cfg(p_param->conidx, p_param->bas_nb);     } break;
        case BASC_BATT_LVL_PRES_FORMAT: { status = basc_read_pres_format(p_param->conidx, p_param->bas_nb); } break;
        default:                        { status = BLE_PRF_ERR_INVALID_PARAM;                                   } break;
    }

    // request cannot be performed
    if (status != BLE_GAP_ERR_NO_ERROR)
    {
        struct basc_read_info_rsp *p_rsp = KE_MSG_ALLOC(BASC_READ_INFO_RSP, src_id, dest_id, basc_read_info_rsp);

        if(p_rsp != NULL)
        {
            p_rsp->conidx = p_param->conidx;
            p_rsp->status = status;
            p_rsp->info   = p_param->info;
            p_rsp->bas_nb = p_param->bas_nb;

            ke_msg_send(p_rsp);
        }
    }

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief Handles reception of the @ref BASC_BATT_LEVEL_NTF_CFG_REQ message.
 * It allows configuration of the peer ntf/stop characteristic for Battery Level Characteristic.
 * Will return an error code if that cfg char does not exist.
 * @param[in] msgid Id of the message received (probably unused).
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int basc_batt_level_ntf_cfg_req_handler(ke_msg_id_t const msgid, struct basc_batt_level_ntf_cfg_req const *p_param,
                                                 ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = basc_write_ntf_cfg(p_param->conidx, p_param->bas_nb, p_param->ntf_cfg);

    // request cannot be performed
    if (status != BLE_GAP_ERR_NO_ERROR)
    {
        struct basc_batt_level_ntf_cfg_rsp *p_rsp = KE_MSG_ALLOC(BASC_BATT_LEVEL_NTF_CFG_RSP, src_id, dest_id,
                                                                 basc_batt_level_ntf_cfg_rsp);
        if(p_rsp != NULL)
        {
            // set error status
            p_rsp->conidx = p_param->conidx;
            p_rsp->status = status;
            p_rsp->bas_nb = p_param->bas_nb;

            ke_msg_send(p_rsp);
        }
    }

    return (KE_MSG_CONSUMED);
}

/// Default State handlers definition
KE_MSG_HANDLER_TAB(basc)
{
    // Note: all messages must be sorted in ID ascending order

    {BASC_ENABLE_REQ,               (ke_msg_func_t) basc_enable_req_handler             },
    {BASC_READ_INFO_REQ,            (ke_msg_func_t) basc_read_info_req_handler          },
    {BASC_BATT_LEVEL_NTF_CFG_REQ,   (ke_msg_func_t) basc_batt_level_ntf_cfg_req_handler },
};
#endif

/**
 ****************************************************************************************
 * @brief Completion of Enable procedure
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 * @param[in] bas_nb        Number of BAS that have been found
 * @param[in] p_bas         Pointer to peer database description bond data
 ****************************************************************************************
 */
void basc_cb_enable_cmp(uint8_t conidx, uint16_t status, uint8_t bas_nb, const bas_content_t* p_bas)
{
    /// to do
    /// app save p_bas to nv.when reconnect use the saved bas content.
#if 0
    // Send APP the details of the discovered attributes on BASC
    struct basc_enable_rsp *p_rsp = KE_MSG_ALLOC(BASC_ENABLE_RSP, PRF_DST_TASK(BASC), PRF_SRC_TASK(BASC),
                                                 basc_enable_rsp);
    if(p_rsp != NULL)
    {
        p_rsp->conidx = conidx;
        p_rsp->status = status;
        p_rsp->bas_nb = bas_nb;
        memcpy(&(p_rsp->bas), p_bas, sizeof(bas_content_t) * BASC_NB_BAS_INSTANCES_MAX);
        ke_msg_send(p_rsp);
    }
#endif
}

/**
 ****************************************************************************************
 * @brief Inform that battery level read procedure is over
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 * @param[in] bas_instance  Battery Service Instance - From 0 to BASC_NB_BAS_INSTANCES_MAX-1
 * @param[in] batt_level    Battery Level
 ****************************************************************************************
 */
void basc_cb_read_batt_level_cmp(uint8_t conidx, uint16_t status, uint8_t hogprh_instance, uint8_t *p_data, uint16_t data_len)
{
#if 0
    struct basc_read_info_rsp *p_rsp = KE_MSG_ALLOC(BASC_READ_INFO_RSP, PRF_DST_TASK(BASC),
                                                    PRF_SRC_TASK(BASC), basc_read_info_rsp);

    if(p_rsp != NULL)
    {
        p_rsp->conidx          = conidx;
        p_rsp->status          = status;
        p_rsp->info            = BASC_BATT_LVL_VAL;
        p_rsp->bas_nb          = bas_instance;
        p_rsp->data.batt_level = batt_level;
        ke_msg_send(p_rsp);
    }
#endif
}

/**
 ****************************************************************************************
 * @brief Inform that Notification configuration read procedure is over
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 * @param[in] bas_instance  Battery Service Instance - From 0 to BASC_NB_BAS_INSTANCES_MAX-1
 * @param[in] ntf_cfg       Notification Configuration
 ****************************************************************************************
 */
void basc_cb_read_ntf_cfg_cmp(uint8_t conidx, uint16_t status, uint8_t bas_instance, uint16_t ntf_cfg)
{
#if 0
    struct basc_read_info_rsp *p_rsp = KE_MSG_ALLOC(BASC_READ_INFO_RSP, PRF_DST_TASK(BASC),
                                                    PRF_SRC_TASK(BASC), basc_read_info_rsp);

    if(p_rsp != NULL)
    {
        p_rsp->conidx          = conidx;
        p_rsp->status          = status;
        p_rsp->info            = BASC_NTF_CFG;
        p_rsp->bas_nb          = bas_instance;
        p_rsp->data.ntf_cfg    = ntf_cfg;
        ke_msg_send(p_rsp);
    }
#endif
}

/**
 ****************************************************************************************
 * @brief Inform that Presentation Format read procedure is over
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 * @param[in] bas_instance  Battery Service Instance - From 0 to BASC_NB_BAS_INSTANCES_MAX-1
 * @param[in] p_pres_format Pointer to Characteristic Presentation Format value
 ****************************************************************************************
 */
void basc_cb_read_pres_format_cmp(uint8_t conidx, uint16_t status, uint8_t bas_instance,
                                  const prf_char_pres_fmt_t* p_pres_format)
{
#if 0
    struct basc_read_info_rsp *p_rsp = KE_MSG_ALLOC(BASC_READ_INFO_RSP, PRF_DST_TASK(BASC),
                                                    PRF_SRC_TASK(BASC), basc_read_info_rsp);

    if(p_rsp != NULL)
    {
        p_rsp->conidx          = conidx;
        p_rsp->status          = status;
        p_rsp->info            = BASC_BATT_LVL_PRES_FORMAT;
        p_rsp->bas_nb          = bas_instance;
        memcpy(&(p_rsp->data.char_pres_format), p_pres_format, sizeof(prf_char_pres_fmt_t));

        ke_msg_send(p_rsp);
    }
#endif
}

/**
 ****************************************************************************************
 * @brief Inform that Notification configuration write procedure is over
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 * @param[in] bas_instance  Battery Service Instance - From 0 to BASC_NB_BAS_INSTANCES_MAX-1
 ****************************************************************************************
 */
void basc_cb_write_ntf_cfg_cmp(uint8_t conidx, uint16_t status, uint8_t bas_instance)
{
#if 0
    struct basc_batt_level_ntf_cfg_rsp *p_rsp = KE_MSG_ALLOC(BASC_BATT_LEVEL_NTF_CFG_RSP, PRF_DST_TASK(BASC),
                                                             PRF_SRC_TASK(BASC), basc_batt_level_ntf_cfg_rsp);

    if(p_rsp != NULL)
    {
        p_rsp->conidx          = conidx;
        p_rsp->status          = status;
        p_rsp->bas_nb          = bas_instance;

        ke_msg_send(p_rsp);
    }
#endif
}

/**
 ****************************************************************************************
 * @brief Inform that battery level update has been received from peer device
 *
 * @param[in] conidx        Connection index
 * @param[in] bas_instance  Battery Service Instance - From 0 to BASC_NB_BAS_INSTANCES_MAX-1
 * @param[in] batt_level    Battery Level
 ****************************************************************************************
 */
void basc_cb_batt_level_upd(uint8_t conidx, uint8_t bas_instance, uint8_t batt_level)
{
#if 0
    struct basc_batt_level_ind *p_ind =
            KE_MSG_ALLOC(BASC_BATT_LEVEL_IND, PRF_DST_TASK(BASC), PRF_SRC_TASK(BASC), basc_batt_level_ind);

    if(p_ind != NULL)
    {
        p_ind->conidx          = conidx;
        p_ind->batt_level      = batt_level;
        p_ind->bas_nb          = bas_instance;

        ke_msg_send(p_ind);
    }
#endif
}

/// Default Message handle
__STATIC const basc_cb_t basc_msg_cb =
{
        .cb_enable_cmp           = basc_cb_enable_cmp,
        .cb_read_batt_level_cmp  = basc_cb_read_batt_level_cmp,
        .cb_read_ntf_cfg_cmp     = basc_cb_read_ntf_cfg_cmp,
        .cb_read_pres_format_cmp = basc_cb_read_pres_format_cmp,
        .cb_write_ntf_cfg_cmp    = basc_cb_write_ntf_cfg_cmp,
        .cb_batt_level_upd       = basc_cb_batt_level_upd,
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
__STATIC uint16_t basc_init(uint16_t* p_start_hdl, uint8_t sec_lvl, uint8_t user_prio,
                          const void* p_params, const basc_cb_t* p_cb)
{
    uint8_t conidx;
    // DB Creation Status
    uint16_t status = BLE_GAP_ERR_NO_ERROR;
    uint8_t user_lid = BLE_GATT_INVALID_USER_LID;

    do
    {
        #if (BLE_HL_MSG_API)
        if(p_cb == NULL)
        {
            p_cb = &(basc_msg_cb);
        }
        #endif // (BLE_HL_MSG_API)

        if(   (p_params == NULL) || (p_cb == NULL) || (p_cb->cb_enable_cmp == NULL)
           || (p_cb->cb_read_batt_level_cmp == NULL) || (p_cb->cb_read_ntf_cfg_cmp == NULL)
           || (p_cb->cb_read_pres_format_cmp == NULL) || (p_cb->cb_write_ntf_cfg_cmp == NULL)
           || (p_cb->cb_batt_level_upd == NULL))
        {
            status = BLE_GAP_ERR_INVALID_PARAM;
            break;
        }

        // register BASC user
        status = gatt_user_cli_register(BASC_VAL_MAX_LEN, user_prio, &basc_cb, &user_lid);
        if(status != BLE_GAP_ERR_NO_ERROR) break;

        if(p_basc_env != NULL)
        {
            // allocate BASC required environment variable
            //p_env->p_env = (prf_hdr_t *) p_basc_env;

            // initialize environment variable
            p_basc_env->prf_env.p_cb    = p_cb;
            #if 0//(BLE_HL_MSG_API)
            p_env->desc.msg_handler_tab = basc_msg_handler_tab;
            p_env->desc.msg_cnt         = ARRAY_LEN(basc_msg_handler_tab);
            #endif // (BLE_HL_MSG_API)

            p_basc_env->user_lid = user_lid;
            for (conidx = 0; conidx < BLE_CONNECTION_MAX; conidx++)
            {
                p_basc_env->p_env[conidx] = NULL;
            }
#if 0//(BT_STACK_PRESENT)
            // register sdp
            sdp_register_service(&basc_sdp_service_basc);
#endif
        }
    } while(0);


    if((status != BLE_GAP_ERR_NO_ERROR) && (user_lid != BLE_GATT_INVALID_USER_LID))
    {
        ble_gatt_user_unregister(user_lid);
    }

    return (status);
}


/**
 ****************************************************************************************
 * @brief Destruction of the profile module - due to a reset or profile remove.
 *
 * This function clean-up allocated memory.
 *
 * @param[in]        reason       Destroy reason (@see enum prf_destroy_reason)
 *
 * @return status of the destruction, if fails, profile considered not removed.
 ****************************************************************************************
 */
__STATIC uint16_t basc_destroy(uint8_t reason)
{
    uint16_t status = BLE_GAP_ERR_NO_ERROR;
    //if(reason != BLE_PRF_DESTROY_RESET)
    {
        status = ble_gatt_user_unregister(p_basc_env->user_lid);
    }

    if(status == BLE_GAP_ERR_NO_ERROR)
    {
        uint8_t idx;

        // cleanup environment variable for each task instances
        for (idx = 0; idx < BLE_CONNECTION_MAX; idx++)
        {
            if (p_basc_env->p_env[idx] != NULL)
            {
                ke_free(p_basc_env->p_env[idx]);
            }
        }
    }

    return (status);
}
#if 0
/**
 ****************************************************************************************
 * @brief Handles Connection creation
 *
 * @param[in|out]    env          Collector or Service allocated environment data.
 * @param[in]        conidx       Connection index
 * @param[in]        p_con_param  Pointer to connection parameters information
 ****************************************************************************************
 */
__STATIC void basc_con_create(uint8_t conidx, const gap_con_param_t* p_con_param)
{
    // Nothing to do
}
#endif
/**
 ****************************************************************************************
 * @brief Handles Disconnection
 *
 * @param[in]        conidx     Connection index
 * @param[in]        reason     Detach reason
 ****************************************************************************************
 */
__STATIC void basc_con_cleanup(uint8_t conidx, uint16_t reason)
{
    // clean-up environment variable allocated for task instance
    if (p_basc_env->p_env[conidx] != NULL)
    {
        ke_free(p_basc_env->p_env[conidx]);
        p_basc_env->p_env[conidx] = NULL;
    }
}


/**
 ****************************************************************************************
 * @brief Initialization of the BASC module.
 *
 * @return status code to know if profile initialization succeed or not.
 ****************************************************************************************
 */
uint16_t ble_basc_init(const basc_cb_t* p_cb)
{
    uint8_t db_cfg_params = 0;
    uint16_t start_hdl = 0;
    return basc_init(&start_hdl, 0, 0, &db_cfg_params, p_cb);
}

/**
 ****************************************************************************************
 * @brief The function enables the BASC.
 *
 * @return status code to know if profile enable succeed or not.
 ****************************************************************************************
 */
uint16_t ble_basc_enable(uint8_t conidx, uint8_t con_type, const bas_content_t* p_bas)
{
    basc_enable(conidx, con_type, 1, p_bas);
}

/**
 ****************************************************************************************
 * @brief Initialization of the BASC module.
 *
 * @return None.
 ****************************************************************************************
 */
void ble_basc_cleanup(uint8_t conidx)
{
    basc_con_cleanup(conidx, 0);
}

/**
 ****************************************************************************************
 * @brief Initialization of the BASC module.
 *
 * @return status code to know if profile destory succeed or not.
 ****************************************************************************************
 */
uint16_t ble_basc_destory(void)
{
    return basc_destroy(0);
}

#endif /* (BLE_BATT_CLIENT) */

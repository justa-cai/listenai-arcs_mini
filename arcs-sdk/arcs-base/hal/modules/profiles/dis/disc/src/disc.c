/**
 ****************************************************************************************
 *
 * @file disc.c
 *
 * @brief Device Information Service Client Implementation.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup DISC
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#define BLE_DIS_CLIENT         1

#if (BLE_DIS_CLIENT)
#include "disc.h"
#include "disc_msg.h"
#include "ble_cli_prf.h"
#include <string.h>



/*
 * DEFINES
 ****************************************************************************************
 */

/// Maximum number of Device Information Service Client task instances
#define DIS_VAL_MAX_LEN (128)

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */


/// Environment variable for each Connections
typedef struct disc_cnx_env
{
    /// Peer database discovered handle mapping
    disc_dis_content_t dis;
    /// counter used to check service uniqueness
    uint8_t            nb_svc;
    /// True if discovery procedure is on-going
    bool          discover;
} disc_cnx_env_t;

/// Device Information Service Client environment variable
typedef struct disc_env
{
    /// profile environment
    prf_hdr_t            prf_env;
    /// Environment variable pointer for each connections
    disc_cnx_env_t*      p_env[BLE_CONNECTION_MAX];
    /// GATT User local identifier
    uint8_t              user_lid;
} disc_env_t;



/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */
disc_env_t disc_env;
disc_env_t* p_disc_env = &disc_env;

/// State machine used to retrieve Device Information Service characteristics information
const prf_char_def_t disc_dis_char[DISC_VAL_MAX] =
{
    // Manufacturer Name
    [DISC_VAL_MANUFACTURER_NAME] = {BLE_GATT_CHAR_MANUF_NAME,  ATT_REQ(PRES, OPT), BLE_PROP(RD)},
    // Model Number String
    [DISC_VAL_MODEL_NB_STR]      = {BLE_GATT_CHAR_MODEL_NB,    ATT_REQ(PRES, OPT), BLE_PROP(RD)},
    // Serial Number String
    [DISC_VAL_SERIAL_NB_STR]     = {BLE_GATT_CHAR_SERIAL_NB,   ATT_REQ(PRES, OPT), BLE_PROP(RD)},
    // Hardware Revision String
    [DISC_VAL_HARD_REV_STR]      = {BLE_GATT_CHAR_HW_REV,      ATT_REQ(PRES, OPT), BLE_PROP(RD)},
    // Firmware Revision String
    [DISC_VAL_FIRM_REV_STR]      = {BLE_GATT_CHAR_FW_REV,      ATT_REQ(PRES, OPT), BLE_PROP(RD)},
    // TSoftware Revision String
    [DISC_VAL_SW_REV_STR]        = {BLE_GATT_CHAR_SW_REV,      ATT_REQ(PRES, OPT), BLE_PROP(RD)},
    // System ID
    [DISC_VAL_SYSTEM_ID]         = {BLE_GATT_CHAR_SYS_ID,      ATT_REQ(PRES, OPT), BLE_PROP(RD)},
    // IEEE
    [DISC_VAL_IEEE]              = {BLE_GATT_CHAR_IEEE_CERTIF, ATT_REQ(PRES, OPT), BLE_PROP(RD)},
    // PnP ID
    [DISC_VAL_PNP_ID]            = {BLE_GATT_CHAR_PNP_ID,      ATT_REQ(PRES, OPT), BLE_PROP(RD)},
};

/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Send discovery results to application.
 *
 * @param p_disc_env    Client Role task environment
 * @param conidx        Connection index
 * @param status        Response status code
 *****************************************************************************************
 */
__STATIC void disc_enable_cmp(disc_env_t* p_disc_env, uint8_t conidx, uint16_t status)
{
    const disc_cb_t* p_cb = (const disc_cb_t*) p_disc_env->prf_env.p_cb;

    if(p_disc_env != NULL)
    {
        disc_cnx_env_t* p_con_env = p_disc_env->p_env[conidx];
        
        if (status != BLE_GAP_ERR_NO_ERROR)
        {
            // clean-up environment variable allocated for task instance
            ke_free(p_con_env);
            p_disc_env->p_env[conidx] = NULL;
        }
        else
        {
             p_con_env->discover = false;
        }
        p_cb->cb_enable_cmp(conidx, status, &p_con_env->dis);
    }
}

/**
 ****************************************************************************************
 * @brief Send read result to application,.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the request
 * @param[in] val_id        Value Identifier (@see enum disc_val_id)
 * @param[in] length        Length of data value)
 * @param[in] p_data        Pointer of data value
 ****************************************************************************************
 */
__STATIC void disc_read_val_cmp(uint8_t conidx, uint16_t status, uint8_t val_id, uint16_t length, const uint8_t* p_data)
{
    if(p_disc_env != NULL)
    {
        const disc_cb_t* p_cb = (const disc_cb_t*) p_disc_env->prf_env.p_cb;

        p_cb->cb_read_val_cmp(conidx, status, val_id, p_data, length);
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
__STATIC void disc_discover_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    if(p_disc_env != NULL)
    {
        if (p_disc_env->p_env[conidx]->nb_svc ==  1)
        {
            status = prf_check_svc_char_validity(DISC_VAL_MAX, p_disc_env->p_env[conidx]->dis.vals, disc_dis_char);
        }
        // too much services
        else if (p_disc_env->p_env[conidx]->nb_svc > 1)
        {
            status = BLE_PRF_ERR_MULTIPLE_SVC;
        }
        // no services found
        else
        {
            status = BLE_PRF_ERR_STOP_DISC_CHAR_MISSING;
        }

        disc_enable_cmp(p_disc_env, conidx, status);
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
__STATIC void disc_read_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    if(status != BLE_GAP_ERR_NO_ERROR)
    {
        disc_read_val_cmp(conidx, status, (uint8_t) dummy, 0, NULL);
    }
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
__STATIC void disc_svc_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t hdl, uint8_t disc_info,
                          uint8_t nb_att, const ble_gatt_svc_att_t* p_atts)
{
    if(p_disc_env != NULL)
    {
        disc_cnx_env_t* p_con_env = p_disc_env->p_env[conidx];
        if(p_con_env != NULL)
        {
            if (p_con_env->nb_svc == 0)
            {
                //Even if we get multiple responses we only store 1 range
                if((disc_info == BLE_GATT_SVC_CMPLT) || (disc_info == BLE_GATT_SVC_START))
                {
                    p_con_env->dis.svc.shdl = hdl;
                }

                if((disc_info == BLE_GATT_SVC_CMPLT) || (disc_info == BLE_GATT_SVC_END))
                {
                    p_con_env->dis.svc.ehdl = hdl + nb_att -1;
                }

                // Retrieve characteristics
                prf_extract_svc_info(hdl, nb_att, p_atts, DISC_VAL_MAX,
                                     &disc_dis_char[0], &(p_con_env->dis.vals[0]), 0, NULL, NULL);

            }
            if((disc_info == BLE_GATT_SVC_CMPLT) || (disc_info == BLE_GATT_SVC_END))
            {
                p_con_env->nb_svc++;
            }
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
__STATIC void disc_att_val_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t hdl, uint16_t offset,
                              void* p_data)
{
    disc_read_val_cmp(conidx, BLE_GAP_ERR_NO_ERROR, (uint8_t) dummy, ble_co_buf_data_len(p_data), ble_co_buf_data(p_data));
}


/// Client callback hander
__STATIC const ble_gatt_cli_cb_t disc_cb =
{
    .cb_discover_cmp    = disc_discover_cmp_cb,
    .cb_read_cmp        = disc_read_cmp_cb,
    .cb_write_cmp       = NULL,
    .cb_att_val_get     = NULL,
    .cb_svc             = disc_svc_cb,
    .cb_svc_info        = NULL,
    .cb_inc_svc         = NULL,
    .cb_char            = NULL,
    .cb_desc            = NULL,
    .cb_att_val         = disc_att_val_cb,
    .cb_att_val_evt     = NULL,
    .cb_svc_changed     = NULL,
};

/*
 * PROFILE NATIVE API
 ****************************************************************************************
 */

uint16_t disc_enable(uint8_t conidx, uint8_t con_type, const disc_dis_content_t* p_dis)
{
    // Status
    uint16_t status = BLE_PRF_ERR_REQ_DISALLOWED;

    if((con_type == PRF_CON_NORMAL) && (p_dis == NULL))
    {
        status = BLE_PRF_ERR_INVALID_PARAM;
    }
    else if(p_disc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_disc_env->p_env[conidx] == NULL))
        {
            // allocate environment variable for task instance
            p_disc_env->p_env[conidx] = (disc_cnx_env_t *) plf_malloc(sizeof(disc_cnx_env_t));

            if(p_disc_env->p_env[conidx] != NULL)
            {
                memset(p_disc_env->p_env[conidx], 0, sizeof(disc_cnx_env_t));
                // Config connection, start discovering
                if (con_type == PRF_CON_DISCOVERY)
                {
                    uint16_t gatt_svc_uuid = BLE_GATT_SVC_DEVICE_INFO;

                    // start discovery
                    status = gatt_cli_discover_svc(conidx, p_disc_env->user_lid, 0, BLE_GATT_DISCOVER_SVC_PRIMARY_BY_UUID, true,
                                                   BLE_GATT_MIN_HDL, BLE_GATT_MAX_HDL, BLE_GATT_UUID_16, (uint8_t*) &gatt_svc_uuid);

                    // Go to DISCOVERING state
                    p_disc_env->p_env[conidx]->discover = true;
                }
                // normal connection, get saved att details
                else
                {
                    memcpy(&(p_disc_env->p_env[conidx]->dis), p_dis, sizeof(disc_dis_content_t));
                    status = BLE_GAP_ERR_NO_ERROR;

                    // send APP confirmation that can start normal connection to TH
                    disc_enable_cmp(p_disc_env, conidx, BLE_GAP_ERR_NO_ERROR);
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

uint16_t disc_read_val(uint8_t conidx, uint8_t val_id)
{
    // Status
    uint16_t status = BLE_PRF_ERR_REQ_DISALLOWED;

    if(p_disc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_disc_env->p_env[conidx] != NULL) && (!p_disc_env->p_env[conidx]->discover))
        {
            uint16_t search_hdl = BLE_GATT_INVALID_HDL;

            // retrieve search handle
            if (val_id < DISC_VAL_MAX)
            {
                search_hdl = p_disc_env->p_env[conidx]->dis.vals[val_id].val_hdl;
            }

            //Check if handle is viable
            if (search_hdl != BLE_GATT_INVALID_HDL)
            {
                // perform read request
                status = gatt_cli_read(conidx, p_disc_env->user_lid, val_id, search_hdl, 0, 0);
            }
            else
            {
                // invalid handle requested
                status = BLE_PRF_ERR_INEXISTENT_HDL;
            }
        }
    }
    //CLOGD("disc read:idx:%d, val_id:%d, sta:0x%x", conidx, val_id, status);
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
 * @brief Handles reception of the @ref DISC_ENABLE_REQ message.
 * The handler enables the Device Information Service Client Role.
 * @param[in] msgid Id of the message received (probably unused).
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int disc_enable_req_handler(ke_msg_id_t const msgid, struct disc_enable_req const *p_param,
                                     ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = disc_enable(p_param->conidx, p_param->con_type, &(p_param->dis));

    // send an error if request fails
    if (status != BLE_GAP_ERR_NO_ERROR)
    {
        struct disc_enable_rsp *p_rsp = KE_MSG_ALLOC(DISC_ENABLE_RSP, src_id, dest_id, disc_enable_rsp);
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
 * @brief Handles reception of the @ref DISC_RD_CHAR_CMD message.
 * Check if the handle exists in profile(already discovered) and send request, otherwise
 * error to APP.
 * @param[in] msgid Id of the message received (probably unused).
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int disc_rd_val_cmd_handler(ke_msg_id_t const msgid, struct disc_rd_val_cmd const *p_param,
                                      ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = disc_read_val(p_param->conidx, p_param->val_id);

    // send error response if request fails
    if (status != BLE_GAP_ERR_NO_ERROR)
    {
        struct disc_cmp_evt *p_evt = KE_MSG_ALLOC(DISC_CMP_EVT, src_id, dest_id, disc_cmp_evt);

        if(p_evt != NULL)
        {
            p_evt->conidx      = p_param->conidx;
            p_evt->operation   = DISC_RD_VAL_CMD_OP_CODE;
            p_evt->status      = status;

            ke_msg_send(p_evt);
        }
    }

    return (KE_MSG_CONSUMED);
}

/// Default State handlers definition
KE_MSG_HANDLER_TAB(disc)
{
    // Note: all messages must be sorted in ID ascending order

    {DISC_ENABLE_REQ,        (ke_msg_func_t)disc_enable_req_handler},
    {DISC_RD_VAL_CMD,        (ke_msg_func_t)disc_rd_val_cmd_handler},
};
#endif

/**
 ****************************************************************************************
 * @brief This function is called when GATT server user has initiated event send to peer
 *        device or if an error occurs.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Client Enable status (@see enum hl_err)
 * @param[in] p_dis         Pointer to bond data information that describe peer database
 ****************************************************************************************
 */
void disc_enable_cmp_handler(uint8_t conidx, uint16_t status, const disc_dis_content_t* p_dis)
{
#if 0
    // Send APP the details of the discovered attributes on DISC
    struct disc_enable_rsp *p_rsp = KE_MSG_ALLOC(DISC_ENABLE_RSP, PRF_DST_TASK(DISC), PRF_SRC_TASK(DISC),
                                                 disc_enable_rsp);
    if(p_rsp != NULL)
    {
        p_rsp->conidx = conidx;
        p_rsp->status = status;
        memcpy(&(p_rsp->dis), p_dis, sizeof(disc_dis_content_t));
        ke_msg_send(p_rsp);
    }
#endif
}

/**
 ****************************************************************************************
 * @brief This function is called when GATT server user has initiated event send to peer
 *        device or if an error occurs.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Read status (@see enum hl_err)
 * @param[in] val_id        Value identifer read (@see enum disc_val_id)
 * @param[in] length        Value data length
 * @param[in] p_data        Pointer to value data
 ****************************************************************************************
 */
void disc_read_val_cmp_handler(uint8_t conidx, uint16_t status, uint8_t val_id, uint16_t length, const uint8_t* p_data)
{
#if 0
    ke_task_id_t src_id  = PRF_SRC_TASK(DISC);
    ke_task_id_t dest_id = PRF_DST_TASK(DISC);

    if(status == BLE_GAP_ERR_NO_ERROR)
    {
        struct disc_rd_val_ind *p_ind = KE_MSG_ALLOC_DYN(DISC_RD_VAL_IND, dest_id, src_id, disc_rd_val_ind, length);
        if(p_ind != NULL)
        {
            p_ind->conidx = conidx;
            p_ind->val_id = val_id;
            p_ind->length = length;
            memcpy(&(p_ind->value), p_data, length);
            ke_msg_send(p_ind);
        }
    }

    struct disc_cmp_evt *p_evt = KE_MSG_ALLOC(DISC_CMP_EVT, dest_id, src_id, disc_cmp_evt);
    if(p_evt != NULL)
    {
        p_evt->conidx      = conidx;
        p_evt->operation   = DISC_RD_VAL_CMD_OP_CODE;
        p_evt->status      = status;

        ke_msg_send(p_evt);
    }
#endif
}

/// Default Message handle
__STATIC const disc_cb_t disc_msg_cb =
{
    .cb_enable_cmp   = disc_enable_cmp_handler,
    .cb_read_val_cmp = disc_read_val_cmp_handler,
};

#endif // (BLE_HL_MSG_API)


/*
 * PROFILE DEFAULT HANDLERS
 ****************************************************************************************
 */
/**
 ****************************************************************************************
 * @brief Initialization of the DISC module.
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
__STATIC uint16_t disc_init(uint16_t* p_start_hdl, uint8_t sec_lvl, uint8_t user_prio,
                          const void* p_params, const disc_cb_t* p_cb)
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
            p_cb = &(disc_msg_cb);
        }
        #endif // (BLE_HL_MSG_API)

        if((p_params == NULL) || (p_cb == NULL) || (p_cb->cb_enable_cmp == NULL) || (p_cb->cb_read_val_cmp == NULL))
        {
            status = BLE_GAP_ERR_INVALID_PARAM;
            break;
        }

        // register DISC user
        status = gatt_user_cli_register(DIS_VAL_MAX_LEN, user_prio, &disc_cb, &user_lid);
        if(status != BLE_GAP_ERR_NO_ERROR) break;

        if(p_disc_env != NULL)
        {
            // initialize environment variable
            p_disc_env->prf_env.p_cb    = p_cb;
            #if 0//(BLE_HL_MSG_API)
            p_env->desc.msg_handler_tab = disc_msg_handler_tab;
            p_env->desc.msg_cnt         = ARRAY_LEN(disc_msg_handler_tab);
            #endif // (BLE_HL_MSG_API)

            p_disc_env->user_lid = user_lid;
            for (conidx = 0; conidx < BLE_CONNECTION_MAX; conidx++)
            {
                p_disc_env->p_env[conidx] = NULL;
            }
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
 * @param[in|out]    p_env        Collector or Service allocated environment data.
 * @param[in]        reason       Destroy reason (@see enum prf_destroy_reason)
 *
 * @return status of the destruction, if fails, profile considered not removed.
 ****************************************************************************************
 */
__STATIC uint16_t disc_destroy(uint8_t reason)
{
    uint16_t status = BLE_GAP_ERR_NO_ERROR;
    //if(reason != PRF_DESTROY_RESET)
    {
        status = gatt_user_unregister(p_disc_env->user_lid);
    }

    if(status == BLE_GAP_ERR_NO_ERROR)
    {
        uint8_t idx;
        // cleanup environment variable for each task instances
        for (idx = 0; idx < BLE_CONNECTION_MAX; idx++)
        {
            if (p_disc_env->p_env[idx] != NULL)
            {
                ke_free(p_disc_env->p_env[idx]);
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
__STATIC void disc_con_create(uint8_t conidx, const gap_con_param_t* p_con_param)
{
    // Nothing to do
}
#endif
/**
 ****************************************************************************************
 * @brief Handles Disconnection
 *
 * @param[in|out]    p_env      Collector or Service allocated environment data.
 * @param[in]        conidx     Connection index
 * @param[in]        reason     Detach reason
 ****************************************************************************************
 */
__STATIC void disc_con_cleanup(uint8_t conidx, uint16_t reason)
{
    // clean-up environment variable allocated for task instance
    if (p_disc_env->p_env[conidx] != NULL)
    {
        ke_free(p_disc_env->p_env[conidx]);
        p_disc_env->p_env[conidx] = NULL;
    }
}

/**
 ****************************************************************************************
 * @brief Initialization of the DISC module.
 *
 * @return status code to know if profile initialization succeed or not.
 ****************************************************************************************
 */
uint16_t ble_disc_init(const disc_cb_t* p_cb)
{
    uint8_t db_cfg_params = 0;
    uint16_t start_hdl = 0;
    return disc_init(&start_hdl, 0, 0, &db_cfg_params, p_cb);
}

/**
 ****************************************************************************************
 * @brief The function enables the DISC.
 *
 * @return status code to know if profile enable succeed or not.
 ****************************************************************************************
 */
uint16_t ble_disc_enable(uint8_t conidx, uint8_t con_type, const disc_dis_content_t* p_disc)
{
    disc_enable(conidx, con_type, p_disc);
}

/**
 ****************************************************************************************
 * @brief Initialization of the DISC module.
 *
 * @return None.
 ****************************************************************************************
 */
void ble_disc_cleanup(uint8_t conidx)
{
    disc_con_cleanup(conidx, 0);
}

/**
 ****************************************************************************************
 * @brief Initialization of the DISC module.
 *
 * @return status code to know if profile destory succeed or not.
 ****************************************************************************************
 */
uint16_t ble_disc_destory(void)
{
    return disc_destroy(0);
}

/**
 ****************************************************************************************
 * @brief Read the DISC module.
 *
 * @return None.
 ****************************************************************************************
 */
void ble_disc_read_val(uint8_t conidx, uint16_t val_id)
{
    disc_read_val(conidx, val_id);
}

#endif //BLE_DIS_CLIENT

/// @} DISC

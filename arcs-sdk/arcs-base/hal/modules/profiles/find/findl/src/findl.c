/**
 ****************************************************************************************
 *
 * @file findl.c
 *
 * @brief Find Me Locator implementation.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup FINDL
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "btip_config.h"

#if (BLE_FINDME_LOCATOR)

#include "findl.h"
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

enum
{
    FINDL_IAS_ALERT_LVL_CHAR,

    FINDL_IAS_CHAR_MAX,
};

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */


/// Environment variable for each Connections
typedef struct findl_cnx_env
{
    /// Peer database discovered handle mapping
    ias_content_t ias;
    /// counter used to check service uniqueness
    uint8_t       nb_svc;
    /// Client is in discovering state
    bool          discover;
} findl_cnx_env_t;

/// Client environment variable
typedef struct findl_env
{
    /// profile environment
    prf_hdr_t            prf_env;
    /// Environment variable pointer for each connections
    findl_cnx_env_t*     p_env[BLE_CONNECTION_MAX];
    /// GATT User local identifier
    uint8_t              user_lid;
} findl_env_t;


/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */

/// State machine used to retrieve Service characteristics information
const prf_char_def_t findl_ias_char[FINDL_IAS_CHAR_MAX] =
{
    // Alert Level
    [FINDL_IAS_ALERT_LVL_CHAR] = { GATT_CHAR_ALERT_LEVEL, ATT_REQ(PRES, MAND), PROP(WC) },
};

/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Send discovery results to application.
 *
 * @param p_findl_env    Client Role task environment
 * @param conidx        Connection index
 * @param status        Response status code
 *****************************************************************************************
 */
__STATIC void findl_enable_cmp(findl_env_t* p_findl_env, uint8_t conidx, uint16_t status)
{
    if(p_findl_env != NULL)
    {
        const findl_cb_t* p_cb = (const findl_cb_t*) p_findl_env->prf_env.p_cb;
        findl_cnx_env_t* p_con_env = p_findl_env->p_env[conidx];
        p_cb->cb_enable_cmp(conidx, status, &(p_con_env->ias));

        if (status != GAP_ERR_NO_ERROR)
        {
            // clean-up environment variable allocated for task instance
            ke_free(p_con_env);
            p_findl_env->p_env[conidx] = NULL;
        }
        else
        {
             p_con_env->discover = false;
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
__STATIC void findl_svc_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t hdl, uint8_t disc_info,
                          uint8_t nb_att, const gatt_svc_att_t* p_atts)
{
    // Get the address of the environment
    findl_env_t* p_findl_env = PRF_ENV_GET(FINDL, findl);

    if(p_findl_env != NULL)
    {
        findl_cnx_env_t* p_con_env = p_findl_env->p_env[conidx];

        ASSERT_INFO(p_con_env != NULL, conidx, user_lid);

        if (p_con_env->nb_svc == 0)
        {
            //Even if we get multiple responses we only store 1 range
            if((disc_info == GATT_SVC_CMPLT) || (disc_info == GATT_SVC_START))
            {
                p_con_env->ias.svc.shdl = hdl;
            }

            if((disc_info == GATT_SVC_CMPLT) || (disc_info == GATT_SVC_END))
            {
                p_con_env->ias.svc.ehdl = hdl + nb_att -1;
            }

            // Retrieve characteristics
            prf_extract_svc_info(hdl, nb_att, p_atts,
                                 FINDL_IAS_CHAR_MAX, &findl_ias_char[0], &(p_con_env->ias.alert_lvl_char),
                                 0, NULL, NULL);
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
__STATIC void findl_discover_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    // Get the address of the environment
    findl_env_t* p_findl_env = PRF_ENV_GET(FINDL, findl);

    if(p_findl_env != NULL)
    {
        findl_cnx_env_t* p_con_env = p_findl_env->p_env[conidx];

        if (p_con_env->nb_svc ==  1)
        {
            status = prf_check_svc_char_validity(FINDL_IAS_CHAR_MAX, &(p_con_env->ias.alert_lvl_char), findl_ias_char);
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

        findl_enable_cmp(p_findl_env, conidx, status);
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
__STATIC void findl_write_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    // Get the address of the environment
    findl_env_t* p_findl_env = PRF_ENV_GET(FINDL, findl);
    if(p_findl_env != NULL)
    {
        const findl_cb_t* p_cb = (const findl_cb_t*) p_findl_env->prf_env.p_cb;
        p_cb->cb_alert_upd_cmp(conidx, status);
    }

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
__STATIC void findl_svc_changed_cb(uint8_t conidx, uint8_t user_lid, bool out_of_sync, uint16_t start_hdl, uint16_t end_hdl)
{
    // Do Nothing
}

/// Client callback hander
__STATIC const gatt_cli_cb_t findl_cb =
{
    .cb_discover_cmp    = findl_discover_cmp_cb,
    .cb_read_cmp        = NULL,
    .cb_write_cmp       = findl_write_cmp_cb,
    .cb_att_val_get     = NULL,
    .cb_svc             = findl_svc_cb,
    .cb_svc_info        = NULL,
    .cb_inc_svc         = NULL,
    .cb_char            = NULL,
    .cb_desc            = NULL,
    .cb_att_val         = NULL,
    .cb_att_val_evt     = NULL,
    .cb_svc_changed     = findl_svc_changed_cb,
};

/*
 * PROFILE NATIVE API
 ****************************************************************************************
 */

uint16_t findl_enable(uint8_t conidx, uint8_t con_type, const ias_content_t* p_ias)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    findl_env_t* p_findl_env = PRF_ENV_GET(FINDL, findl);

    if(p_findl_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_findl_env->p_env[conidx] == NULL))
        {
            // allocate environment variable for task instance
            p_findl_env->p_env[conidx] = (struct findl_cnx_env *) ke_malloc(sizeof(struct findl_cnx_env), KE_MEM_ATT_DB);

            if(p_findl_env->p_env[conidx] != NULL)
            {
                // Config connection, start discovering
                if (con_type == PRF_CON_DISCOVERY)
                {
                    uint16_t gatt_svc_uuid = GATT_SVC_IMMEDIATE_ALERT;
                    memset(p_findl_env->p_env[conidx], 0, sizeof(struct findl_cnx_env));

                    // start discovery
                    status = gatt_cli_discover_svc(conidx, p_findl_env->user_lid, 0, GATT_DISCOVER_SVC_PRIMARY_BY_UUID, true,
                                                   GATT_MIN_HDL, GATT_MAX_HDL, GATT_UUID_16, (uint8_t*) &gatt_svc_uuid);

                    // Go to DISCOVERING state
                    p_findl_env->p_env[conidx]->discover = true;
                }
                // normal connection, get saved att details
                else
                {
                    memcpy(&(p_findl_env->p_env[conidx]->ias), p_ias, sizeof(ias_content_t));
                    status = GAP_ERR_NO_ERROR;

                    // send APP confirmation that can start normal connection to TH
                    findl_enable_cmp(p_findl_env, conidx, GAP_ERR_NO_ERROR);
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

uint16_t findl_alert_upd(uint8_t conidx, uint8_t alert_lvl)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    findl_env_t* p_findl_env = PRF_ENV_GET(FINDL, findl);

    if(alert_lvl > FINDL_ALERT_HIGH)
    {
        status = PRF_ERR_INVALID_PARAM;
    }
    else if(p_findl_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_findl_env->p_env[conidx] != NULL) && (!p_findl_env->p_env[conidx]->discover))
        {
            findl_cnx_env_t* p_con_env = p_findl_env->p_env[conidx];
            uint16_t hdl = p_con_env->ias.alert_lvl_char.val_hdl;

            if(hdl == GATT_INVALID_HDL)
            {
                status = PRF_ERR_INEXISTENT_HDL;
            }
            else
            {
                status = prf_gatt_write(conidx, p_findl_env->user_lid, 0, GATT_WRITE_NO_RESP, hdl, sizeof(uint8_t), &alert_lvl);
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
 * @brief  Message handler example
 *
 * @param[in] msgid Id of the message received (probably unused).
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int findl_enable_req_handler(ke_msg_id_t const msgid, struct findl_enable_req const *p_param,
                                     ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = findl_enable(p_param->conidx, p_param->con_type, &(p_param->ias));

    // send an error if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct findl_enable_rsp *p_rsp = KE_MSG_ALLOC(FINDL_ENABLE_RSP, src_id, dest_id, findl_enable_rsp);
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
 * @brief Handles reception of the @ref FINDL_SET_ALERT_CMD message.
 * The handler disables the Find Me profile - Target Role.
 * @param[in] msgid Id of the message received (probably unused).
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int findl_set_alert_cmd_handler(ke_msg_id_t const msgid, struct findl_set_alert_cmd const *p_param,
                                         ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = findl_alert_upd(p_param->conidx, p_param->alert_lvl);

    // send an error if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct findl_cmp_evt *p_evt = KE_MSG_ALLOC(FINDL_CMP_EVT, src_id, dest_id, findl_cmp_evt);
        if(p_evt != NULL)
        {
            p_evt->conidx    = p_param->conidx;
            p_evt->operation = FINDL_SET_ALERT_CMD_OP_CODE;
            p_evt->status    = status;
            ke_msg_send(p_evt);
        }
    }

    return (KE_MSG_CONSUMED);
}

/// Default State handlers definition
KE_MSG_HANDLER_TAB(findl)
{
    // Note: all messages must be sorted in ID ascending order

    { FINDL_ENABLE_REQ,        (ke_msg_func_t)findl_enable_req_handler    },
    { FINDL_SET_ALERT_CMD,     (ke_msg_func_t)findl_set_alert_cmd_handler },
};

/**
 ****************************************************************************************
 * @brief This function is called when GATT server user has initiated event send to peer
 *        device or if an error occurs.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Client Enable status (@see enum hl_err)
 * @param[in] p_ias         Pointer to bond data information that describe peer database
 ****************************************************************************************
 */
void findl_cb_enable_cmp(uint8_t conidx, uint16_t status, const ias_content_t* p_ias)
{
    // Send APP the details of the discovered attributes on FINDL
    struct findl_enable_rsp *p_rsp = KE_MSG_ALLOC(FINDL_ENABLE_RSP, PRF_DST_TASK(FINDL), PRF_SRC_TASK(FINDL),
                                                  findl_enable_rsp);
    if(p_rsp != NULL)
    {
        p_rsp->conidx = conidx;
        p_rsp->status = status;
        memcpy(&(p_rsp->ias), p_ias, sizeof(ias_content_t));
        ke_msg_send(p_rsp);
    }
}

/**
 ****************************************************************************************
 * @brief This function is called when Alert update procedure is over.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Read status (@see enum hl_err)
 ****************************************************************************************
 */
void findl_cb_alert_upd_cmp(uint8_t conidx, uint16_t status)
{
    struct findl_cmp_evt *p_evt = KE_MSG_ALLOC(FINDL_CMP_EVT, PRF_DST_TASK(FINDL), PRF_SRC_TASK(FINDL),
                                               findl_cmp_evt);
    if(p_evt != NULL)
    {
        p_evt->conidx    = conidx;
        p_evt->operation = FINDL_SET_ALERT_CMD_OP_CODE;
        p_evt->status    = status;
        ke_msg_send(p_evt);
    }
}

/// Default Message handle
__STATIC const findl_cb_t findl_msg_cb =
{
    .cb_enable_cmp    = findl_cb_enable_cmp,
    .cb_alert_upd_cmp = findl_cb_alert_upd_cmp,
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
__STATIC uint16_t findl_init(prf_data_t* p_env, uint16_t* p_start_hdl, uint8_t sec_lvl, uint8_t user_prio,
                          const void* p_params, const findl_cb_t* p_cb)
{
    uint8_t conidx;
    // DB Creation Status
    uint16_t status = GAP_ERR_NO_ERROR;
    uint8_t user_lid = GATT_INVALID_USER_LID;

    do
    {
        findl_env_t* p_findl_env;

        #if (BLE_HL_MSG_API)
        if(p_cb == NULL)
        {
            p_cb = &(findl_msg_cb);
        }
        #endif // (BLE_HL_MSG_API)

        if((p_params == NULL) || (p_cb == NULL) || (p_cb->cb_enable_cmp == NULL) || (p_cb->cb_alert_upd_cmp == NULL))
        {
            status = GAP_ERR_INVALID_PARAM;
            break;
        }

        // register FINDL user
        status = gatt_user_cli_register(L2CAP_LE_MTU_MIN, user_prio, &findl_cb, &user_lid);
        if(status != GAP_ERR_NO_ERROR) break;

        //-------------------- allocate memory required for the profile  ---------------------
        p_findl_env = (findl_env_t*) ke_malloc(sizeof(findl_env_t), KE_MEM_ATT_DB);

        if(p_findl_env != NULL)
        {
            // allocate FINDL required environment variable
            p_env->p_env = (prf_hdr_t *) p_findl_env;

            // initialize environment variable
            p_findl_env->prf_env.p_cb    = p_cb;
            #if (BLE_HL_MSG_API)
            p_env->desc.msg_handler_tab = findl_msg_handler_tab;
            p_env->desc.msg_cnt         = ARRAY_LEN(findl_msg_handler_tab);
            #endif // (BLE_HL_MSG_API)

            p_findl_env->user_lid = user_lid;
            for (conidx = 0; conidx < BLE_CONNECTION_MAX; conidx++)
            {
                p_findl_env->p_env[conidx] = NULL;
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
__STATIC uint16_t findl_destroy(prf_data_t *p_env, uint8_t reason)
{
    uint16_t status = GAP_ERR_NO_ERROR;
    findl_env_t* p_findl_env = (findl_env_t*) p_env->p_env;

    if(reason != PRF_DESTROY_RESET)
    {
        status = gatt_user_unregister(p_findl_env->user_lid);
    }

    if(status == GAP_ERR_NO_ERROR)
    {
        uint8_t idx;

        // cleanup environment variable for each task instances
        for (idx = 0; idx < BLE_CONNECTION_MAX; idx++)
        {
            if (p_findl_env->p_env[idx] != NULL)
            {
                ke_free(p_findl_env->p_env[idx]);
            }
        }

        // free profile environment variables
        p_env->p_env = NULL;
        ke_free(p_findl_env);
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
__STATIC void findl_con_create(prf_data_t *p_env, uint8_t conidx, const gap_con_param_t* p_con_param)
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
__STATIC void findl_con_cleanup(prf_data_t *p_env, uint8_t conidx, uint16_t reason)
{
    findl_env_t* p_findl_env = (findl_env_t*) p_env->p_env;

    // clean-up environment variable allocated for task instance
    if (p_findl_env->p_env[conidx] != NULL)
    {
        ke_free(p_findl_env->p_env[conidx]);
        p_findl_env->p_env[conidx] = NULL;
    }
}

/// FINDL Task interface required by profile manager
const prf_task_cbs_t findl_itf =
{
    .cb_init          = (prf_init_cb) findl_init,
    .cb_destroy       = findl_destroy,
    .cb_con_create    = findl_con_create,
    .cb_con_cleanup   = findl_con_cleanup,
    .cb_con_upd       = NULL,
};

/**
 ****************************************************************************************
 * @brief Retrieve client profile interface
 *
 * @return Client profile interface
 ****************************************************************************************
 */
const prf_task_cbs_t* findl_prf_itf_get(void)
{
    return &findl_itf;
}

#endif //BLE_FINDME_LOCATOR

/// @} FINDL

/**
 ****************************************************************************************
 *
 * @file findt.c
 *
 * @brief Find Me Target implementation.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup FINDT
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "btip_config.h"

#if (BLE_FINDME_TARGET)
#include "findt.h"
#include "gap.h"
#include "gatt.h"
#include "ble_prf.h"

#include "co_utils.h"

#include <string.h>
#include "ke_mem.h"

/*
 * DEFINES
 ****************************************************************************************
 */

///FINDT Attributes database handle list
enum findt_att_db_handles
{
    FINDT_IAS_IDX_SVC,

    FINDT_IAS_IDX_ALERT_LVL_CHAR,
    FINDT_IAS_IDX_ALERT_LVL_VAL,

    FINDT_IAS_IDX_NB,
};

/*
 * MACROS
 ****************************************************************************************
 */

/// Get database attribute handle
#define FINDT_HANDLE(idx) \
    (p_findt_env->start_hdl + (idx) )

/// Get database attribute index
#define FINDT_IDX(hdl) \
    ( (hdl) - p_findt_env->start_hdl )

/*
 * TYPES DEFINITION
 ****************************************************************************************
 */


/// Find me target server environment variable
typedef struct findt_env
{
    /// profile environment
    prf_hdr_t prf_env;
    /// Service Attribute Start Handle
    uint16_t  start_hdl;
    /// GATT user local identifier
    uint8_t   user_lid;

} findt_env_t;


/*
 * ATTRIBUTES DATABASE
 ****************************************************************************************
 */

/// Full IAS Database Description - Used to add attributes into the database
const gatt_att16_desc_t findt_att_db[FINDT_IAS_IDX_NB] =
{
    // Immediate Alert Service Declaration
    [FINDT_IAS_IDX_SVC]            = { GATT_DECL_PRIMARY_SERVICE, PROP(RD), 0                                },
    // Alert Level Characteristic Declaration
    [FINDT_IAS_IDX_ALERT_LVL_CHAR] = { GATT_DECL_CHARACTERISTIC,  PROP(RD), 0                                },
    // Alert Level Characteristic Value
    [FINDT_IAS_IDX_ALERT_LVL_VAL]  = { GATT_CHAR_ALERT_LEVEL,     PROP(WC), OPT(NO_OFFSET) | sizeof(uint8_t) },
};

/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */

/*
 * GATT USER SERVICE HANDLERS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief This function is called during a write procedure to modify attribute handle.
 *
 *        @see gatt_srv_att_val_set_cfm shall be called to accept or reject attribute
 *        update.
 *
 * @param[in] conidx        Connection index
 * @param[in] user_lid      GATT user local identifier
 * @param[in] token         Procedure token that must be returned in confirmation function
 * @param[in] hdl           Attribute handle
 * @param[in] offset        Value offset
 * @param[in] p_data        Pointer to buffer that contains data to write starting from offset
 ****************************************************************************************
 */
__STATIC void findt_cb_att_val_set(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset,
                                   co_buf_t* p_data)
{
    uint16_t status = ATT_ERR_APP_ERROR;
    findt_env_t *p_findt_env = PRF_ENV_GET(FINDT, findt);

    if(p_findt_env != NULL)
    {
        uint8_t  alert_lvl = co_buf_data(p_data)[0];

        // Check if Alert Level is valid
        if ((FINDT_IDX(hdl) == FINDT_IAS_IDX_ALERT_LVL_VAL) && (co_buf_data_len(p_data) > 0) && (alert_lvl <= FINDT_ALERT_HIGH))
        {
            // inform application about the new alert level
            const findt_cb_t* p_cb = (const findt_cb_t*) p_findt_env->prf_env.p_cb;
            p_cb->cb_alert_upd(conidx, alert_lvl);
            status = GAP_ERR_NO_ERROR;
        }
    }

    gatt_srv_att_val_set_cfm(conidx, user_lid, token, status);
}

/**
 ****************************************************************************************
 * @brief This function is called when peer want to read local attribute database value.
 *
 *        @see gatt_srv_att_read_get_cfm shall be called to provide attribute value
 *
 * @param[in] conidx        Connection index
 * @param[in] user_lid      GATT user local identifier
 * @param[in] token         Procedure token that must be returned in confirmation function
 * @param[in] hdl           Attribute handle
 * @param[in] offset        Value offset
 * @param[in] max_length    Maximum value length to return
 ****************************************************************************************
 */
__STATIC void findt_cb_att_read_get(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset,
                                    uint16_t max_length)
{
    // unused
    gatt_srv_att_read_get_cfm(conidx, user_lid, token, PRF_APP_ERROR, 0, NULL);
}

/// Service callback hander
__STATIC const gatt_srv_cb_t findt_cb =
{
        .cb_event_sent    = NULL,
        .cb_att_read_get  = findt_cb_att_read_get,
        .cb_att_event_get = NULL,
        .cb_att_info_get  = NULL,
        .cb_att_val_set   = findt_cb_att_val_set,
};

/*
 * PROFILE NATIVE HANDLERS
 ****************************************************************************************
 */

#if (BLE_HL_MSG_API)
/*
 * PROFILE MSG HANDLERS
 ****************************************************************************************
 */


/// Default State handlers definition
KE_MSG_HANDLER_TAB(findt)
{
    // Note: all messages must be sorted in ID ascending order
};

/**
 ****************************************************************************************
 * @brief This function is called when a Find me locator update the Alert level
 *
 * @param[in] conidx        Connection Index
 * @param[in] alert_lvl     Alert Level (@see enum findt_alert_lvl)
 ****************************************************************************************
 */
void findt_cb_alert_upd(uint8_t conidx, uint8_t alert_lvl)
{
    struct findt_alert_ind *p_ind;

    // request value to application

    p_ind = KE_MSG_ALLOC(FINDT_ALERT_IND, PRF_DST_TASK(FINDT), PRF_SRC_TASK(FINDT), findt_alert_ind);

    if(p_ind)
    {
        p_ind->conidx    = conidx;
        p_ind->alert_lvl = alert_lvl;
        ke_msg_send(p_ind);
    }
}

/// Default Message handle
__STATIC const findt_cb_t findt_msg_cb =
{
    .cb_alert_upd = findt_cb_alert_upd,
};
#endif // (BLE_HL_MSG_API)



/*
 * PROFILE DEFAULT HANDLERS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Initialization of the FINDT module.
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
__STATIC uint16_t findt_init(prf_data_t *p_env, uint16_t *p_start_hdl, uint8_t sec_lvl, uint8_t user_prio,
                          struct findt_db_cfg *p_params, const findt_cb_t* p_cb)
{
    //------------------ create the attribute database for the profile -------------------

    // DB Creation Status
    uint16_t status = GAP_ERR_NO_ERROR;
    uint8_t user_lid = GATT_INVALID_USER_LID;

    do
    {
        findt_env_t* p_findt_env;

        #if (BLE_HL_MSG_API)
        if(p_cb == NULL)
        {
            p_cb = &(findt_msg_cb);
        }
        #endif // (BLE_HL_MSG_API)

        if((p_params == NULL) || (p_start_hdl == NULL) || (p_cb == NULL) || (p_cb->cb_alert_upd == NULL))
        {
            status = GAP_ERR_INVALID_PARAM;
            break;
        }

        // register FINDT user
        status = gatt_user_srv_register(L2CAP_LE_MTU_MIN, user_prio, &findt_cb, &user_lid);
        if(status != GAP_ERR_NO_ERROR) break;

        // Add GAP service
        status = gatt_db_svc16_add(user_lid, sec_lvl, GATT_SVC_IMMEDIATE_ALERT, FINDT_IAS_IDX_NB,
                                   NULL, &(findt_att_db[0]), FINDT_IAS_IDX_NB, p_start_hdl);
        if(status != GAP_ERR_NO_ERROR) break;

        //-------------------- allocate memory required for the profile  ---------------------
        p_findt_env = (findt_env_t *) ke_malloc(sizeof(findt_env_t), KE_MEM_ATT_DB);

        if(p_findt_env != NULL)
        {
            // allocate FINDT required environment variable
            p_env->p_env = (prf_hdr_t *) p_findt_env;
            p_findt_env->start_hdl = *p_start_hdl;
            p_findt_env->user_lid  = user_lid;

            // initialize profile environment variable
            p_findt_env->prf_env.p_cb     = p_cb;
            #if (BLE_HL_MSG_API)
            p_env->desc.msg_handler_tab  = findt_msg_handler_tab;
            p_env->desc.msg_cnt          = ARRAY_LEN(findt_msg_handler_tab);
            #endif // (BLE_HL_MSG_API)
        }
        else
        {
            status = GAP_ERR_INSUFF_RESOURCES;
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
__STATIC uint16_t findt_destroy(prf_data_t *p_env, uint8_t reason)
{
    uint16_t status = GAP_ERR_NO_ERROR;
    findt_env_t *p_findt_env = (findt_env_t *) p_env->p_env;

    if(reason != PRF_DESTROY_RESET)
    {
        status = gatt_user_unregister(p_findt_env->user_lid);
    }

    if(status == GAP_ERR_NO_ERROR)
    {
        // free profile environment variables
        p_env->p_env = NULL;
        ke_free(p_findt_env);
    }

    return (status);
}

/**
 ****************************************************************************************
 * @brief @brief Handles Connection creation
 *
 * @param[in|out]    env          Collector or Service allocated environment data.
 * @param[in]        conidx       Connection index
 * @param[in]        p_con_param  Pointer to connection parameters information
 ****************************************************************************************
 */
__STATIC void findt_con_create(prf_data_t *p_env, uint8_t conidx, const gap_con_param_t* p_con_param)
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
__STATIC void findt_con_cleanup(prf_data_t *p_env, uint8_t conidx, uint16_t reason)
{
    // Nothing to do
}

/// FINDT Task interface required by profile manager
const prf_task_cbs_t findt_itf =
{
    .cb_init          = (prf_init_cb) findt_init,
    .cb_destroy       = findt_destroy,
    .cb_con_create    = findt_con_create,
    .cb_con_cleanup   = findt_con_cleanup,
    .cb_con_upd       = NULL,
};

/**
 ****************************************************************************************
 * @brief Retrieve service profile interface
 *
 * @return service profile interface
 ****************************************************************************************
 */
const prf_task_cbs_t *findt_prf_itf_get(void)
{
    return &findt_itf;
}

#endif // BLE_FINDME_TARGET

/// @} FINDT

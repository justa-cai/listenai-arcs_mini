/**
 ****************************************************************************************
 *
 * @file proxr.c
 *
 * @brief Proximity Reporter Implementation.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup PROXR
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "btip_config.h"

#if (BLE_PROX_REPORTER)

#include "proxr.h"
#include "gap.h"
#include "gapm.h"
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


/// LLS Handles offsets
enum
{
    LLS_IDX_SVC,

    LLS_IDX_ALERT_LVL_CHAR,
    LLS_IDX_ALERT_LVL_VAL,

    LLS_IDX_NB,
};

/// IAS Handles offsets
enum
{
    IAS_IDX_SVC,

    IAS_IDX_ALERT_LVL_CHAR,
    IAS_IDX_ALERT_LVL_VAL,

    IAS_IDX_NB,
};

/// TXPS Handles offsets
enum
{
    TXPS_IDX_SVC,

    TXPS_IDX_TX_POWER_LVL_CHAR,
    TXPS_IDX_TX_POWER_LVL_VAL,

    TXPS_IDX_NB,
};

/// Content of PROXR token
enum proxr_token_bf
{
    /// GATT procedure token
    PROXR_TOKEN_GATT_TOKEN_MASK = 0x0000FFFF,
    PROXR_TOKEN_GATT_TOKEN_LSB  = 0,
    /// Connection index
    PROXR_TOKEN_CONIDX_MASK     = 0x00FF0000,
    PROXR_TOKEN_CONIDX_LSB      = 16,
};

/*
 * TYPES DEFINITION
 ****************************************************************************************
 */

/// Proximity reporter server environment variable
typedef struct proxr_env
{
    /// profile environment
    prf_hdr_t prf_env;
    /// IAS Start Handle
    uint16_t  ias_start_hdl;
    /// LLS Start Handle
    uint16_t  lls_start_hdl;
    /// TXP Start Handle
    uint16_t  txp_start_hdl;
    /// Services features
    uint16_t  features;
    /// GATT user local identifier
    uint8_t   user_lid;
    /// Store link loss alert level
    uint8_t   lls_alert_lvl[BLE_CONNECTION_MAX];

} proxr_env_t;


/*
 * ATTRIBUTES DATABASE
 ****************************************************************************************
 */

/// Full LLS Database Description - Used to add attributes into the database
const gatt_att16_desc_t proxr_lls_att_db[LLS_IDX_NB] =
{
    // Link Loss Service Declaration
    [LLS_IDX_SVC]                = { GATT_DECL_PRIMARY_SERVICE, PROP(RD),             0                                },
    // Alert Level Characteristic Declaration
    [LLS_IDX_ALERT_LVL_CHAR]     = { GATT_DECL_CHARACTERISTIC,  PROP(RD),             0                                },
    // Alert Level Characteristic Value
    [LLS_IDX_ALERT_LVL_VAL]      = { GATT_CHAR_ALERT_LEVEL,     PROP(RD) | PROP(WR),  OPT(NO_OFFSET) | sizeof(uint8_t) },
};

/// Full IAS Database Description - Used to add attributes into the database
const gatt_att16_desc_t proxr_ias_att_db[IAS_IDX_NB] =
{
    // Immediate Alert Service Declaration
    [IAS_IDX_SVC]                = { GATT_DECL_PRIMARY_SERVICE, PROP(RD),             0                                },
    // Alert Level Characteristic Declaration
    [IAS_IDX_ALERT_LVL_CHAR]     = { GATT_DECL_CHARACTERISTIC,  PROP(RD),             0                                },
    // Alert Level Characteristic Value
    [IAS_IDX_ALERT_LVL_VAL]      = { GATT_CHAR_ALERT_LEVEL,     PROP(WC),             OPT(NO_OFFSET) | sizeof(uint8_t) },
};

/// Full TXPS Database Description - Used to add attributes into the database
const gatt_att16_desc_t proxr_txps_att_db[TXPS_IDX_NB] =
{
    // TX Power Service Declaration
    [TXPS_IDX_SVC]               = { GATT_DECL_PRIMARY_SERVICE, PROP(RD),             0                                },
    // TX Power Level Characteristic Declaration
    [TXPS_IDX_TX_POWER_LVL_CHAR] = { GATT_DECL_CHARACTERISTIC,  PROP(RD),             0                                },
    // TX Power Level Characteristic Value
    [TXPS_IDX_TX_POWER_LVL_VAL]  = { GATT_CHAR_TX_POWER_LEVEL,  PROP(RD),             OPT(NO_OFFSET) | sizeof(uint8_t) },
};

/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */


/**
 ***************************************************************************************
 * @brief Function executed when procedure execution is over.
 *
 * @param[in] token         Procedure token provided by upper layer application
 * @param[in] status        Procedure execution status
 * @param[in] power_lvl     Advertising channel TX power level
 ***************************************************************************************
 */
void proxr_cb_adv_tx_power(uint32_t proxr_token, uint16_t status, int8_t power_lvl)
{
    proxr_env_t *p_proxr_env = PRF_ENV_GET(PROXR, proxr);

    if(p_proxr_env != NULL)
    {
        co_buf_t* p_buf = NULL;
        uint16_t token  = GETF(proxr_token, PROXR_TOKEN_GATT_TOKEN);
        uint8_t conidx  = GETF(proxr_token, PROXR_TOKEN_CONIDX);
        uint8_t att_val_len = 0;

        if(status != GAP_ERR_NO_ERROR)
        {
            status = PRF_APP_ERROR;
        }
        // prepare a buffer to send back value
        else if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, sizeof(uint8_t), GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
        {
            co_buf_data(p_buf)[0] = power_lvl;
            att_val_len = 1;
            status = GAP_ERR_NO_ERROR;
        }
        else
        {
            status = ATT_ERR_INSUFF_RESOURCE;
        }

        // send response
        gatt_srv_att_read_get_cfm(conidx, p_proxr_env->user_lid, token, status, att_val_len, p_buf);

        if(p_buf != NULL)
        {
            co_buf_release(p_buf);
        }
    }
}


/*
 * GATT USER SERVICE HANDLERS
 ****************************************************************************************
 */

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
 * @param[in] offset        Data offset
 * @param[in] max_length    Maximum data length to return
 ****************************************************************************************
 */
__STATIC void proxr_cb_att_read_get(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset,
                                    uint16_t max_length)
{
    // Get the address of the environment
    proxr_env_t *p_proxr_env = PRF_ENV_GET(PROXR, proxr);
    co_buf_t* p_buf = NULL;
    uint16_t  att_val_len = 0;
    uint16_t  status = ATT_ERR_REQUEST_NOT_SUPPORTED;
    bool      send_rsp = true;

    if(p_proxr_env != NULL)
    {
        if (hdl == (p_proxr_env->lls_start_hdl + LLS_IDX_ALERT_LVL_VAL))
        {
            if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, sizeof(uint8_t), GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
            {
                co_buf_data(p_buf)[0] = p_proxr_env->lls_alert_lvl[conidx];
                att_val_len = 1;
                status = GAP_ERR_NO_ERROR;
            }
            else
            {
                status = ATT_ERR_INSUFF_RESOURCE;
            }
        }
        else if(hdl == (p_proxr_env->txp_start_hdl + TXPS_IDX_TX_POWER_LVL_VAL))
        {
            uint32_t proxr_token = 0;

            SETF(proxr_token, PROXR_TOKEN_GATT_TOKEN, token);
            SETF(proxr_token, PROXR_TOKEN_CONIDX,     conidx);

            // retrieve TX power
            status = gapm_adv_tx_power_get(proxr_token, proxr_cb_adv_tx_power);
        }
    }

    if(send_rsp)
    {
        // send response
        gatt_srv_att_read_get_cfm(conidx, user_lid, token, status, att_val_len, p_buf);

        if(p_buf != NULL)
        {
            co_buf_release(p_buf);
        }
    }
}

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
__STATIC void proxr_cb_att_val_set(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset,
                                   co_buf_t* p_data)
{
    uint16_t status = ATT_ERR_APP_ERROR;
    proxr_env_t *p_proxr_env = PRF_ENV_GET(PROXR, proxr);

    if(p_proxr_env != NULL)
    {
        uint8_t  alert_lvl = co_buf_data(p_data)[0];
        if((co_buf_data_len(p_data) > 0) && (alert_lvl <= PROXR_ALERT_HIGH))
        {

            // Check if Alert Level is valid
            if (hdl == (p_proxr_env->ias_start_hdl + IAS_IDX_ALERT_LVL_VAL))
            {
                // inform application about the new alert level
                const proxr_cb_t* p_cb = (const proxr_cb_t*) p_proxr_env->prf_env.p_cb;
                p_cb->cb_alert_upd(conidx, PROXR_IAS_CHAR, alert_lvl);
                status = GAP_ERR_NO_ERROR;
            }
            else if (hdl == (p_proxr_env->lls_start_hdl + LLS_IDX_ALERT_LVL_VAL))
            {
                p_proxr_env->lls_alert_lvl[conidx] = alert_lvl;
                status = GAP_ERR_NO_ERROR;
            }
        }
    }

    gatt_srv_att_val_set_cfm(conidx, user_lid, token, status);
}

/// Service callback hander
__STATIC const gatt_srv_cb_t proxr_cb =
{
        .cb_event_sent    = NULL,
        .cb_att_read_get  = proxr_cb_att_read_get,
        .cb_att_event_get = NULL,
        .cb_att_info_get  = NULL,
        .cb_att_val_set   = proxr_cb_att_val_set,
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
KE_MSG_HANDLER_TAB(proxr)
{
    // Note: all messages must be sorted in ID ascending order
};

/**
 ****************************************************************************************
 * @brief This function is called when a Find me locator update the Alert level
 *
 * @param[in] conidx        Connection Index
 * @param[in] char_code     Char Code - Indicate if LLS or IAS (@see enum proxr_alert_lvl)
 * @param[in] alert_lvl     Alert Level (@see enum proxr_alert_lvl)
 ****************************************************************************************
 */
void proxr_cb_alert_upd(uint8_t conidx, uint8_t char_code, uint8_t alert_lvl)
{
    struct proxr_alert_ind *p_ind;

    // request value to application

    p_ind = KE_MSG_ALLOC(PROXR_ALERT_IND, PRF_DST_TASK(PROXR), PRF_SRC_TASK(PROXR), proxr_alert_ind);

    if(p_ind)
    {
        p_ind->conidx    = conidx;
        p_ind->char_code = char_code;
        p_ind->alert_lvl = alert_lvl;
        ke_msg_send(p_ind);
    }
}

/// Default Message handle
__STATIC const proxr_cb_t proxr_msg_cb =
{
    .cb_alert_upd = proxr_cb_alert_upd,
};
#endif // (BLE_HL_MSG_API)

/*
 * PROFILE DEFAULT HANDLERS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Initialization of the PROXR module.
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
__STATIC uint16_t proxr_init(prf_data_t *p_env, uint16_t *p_start_hdl, uint8_t sec_lvl, uint8_t user_prio,
                           struct proxr_db_cfg *p_params, const proxr_cb_t* p_cb)
{
    //------------------ create the attribute database for the profile -------------------

    // DB Creation Status
    uint16_t status = GAP_ERR_NO_ERROR;
    uint8_t user_lid = GATT_INVALID_USER_LID;

    do
    {
        proxr_env_t* p_proxr_env;
        // Service content flag
        uint16_t lls_start_hdl = GATT_INVALID_HDL;
        uint16_t ias_start_hdl = GATT_INVALID_HDL;
        uint16_t txp_start_hdl = GATT_INVALID_HDL;

        #if (BLE_HL_MSG_API)
        if(p_cb == NULL)
        {
            p_cb = &(proxr_msg_cb);
        }
        #endif // (BLE_HL_MSG_API)

        if((p_params == NULL) || (p_start_hdl == NULL) || (p_cb == NULL) || (p_cb->cb_alert_upd == NULL))
        {
            status = GAP_ERR_INVALID_PARAM;
            break;
        }

        // register PROXR user
        status = gatt_user_srv_register(L2CAP_LE_MTU_MIN, user_prio, &proxr_cb, &user_lid);
        if(status != GAP_ERR_NO_ERROR) break;

        // Add Link loss service
        status = gatt_db_svc16_add(user_lid, sec_lvl, GATT_SVC_LINK_LOSS, LLS_IDX_NB,
                                   NULL, &(proxr_lls_att_db[0]), LLS_IDX_NB, p_start_hdl);
        if(status != GAP_ERR_NO_ERROR) break;

        lls_start_hdl = *p_start_hdl;
        *p_start_hdl += LLS_IDX_NB;

        if(p_params->features == PROXR_IAS_TXPS_SUP)
        {
            // Add Immediate Alert service
            status = gatt_db_svc16_add(user_lid, sec_lvl, GATT_SVC_IMMEDIATE_ALERT, IAS_IDX_NB,
                                       NULL, &(proxr_ias_att_db[0]), IAS_IDX_NB, p_start_hdl);
            if(status != GAP_ERR_NO_ERROR) break;

           ias_start_hdl = *p_start_hdl;
           *p_start_hdl += IAS_IDX_NB;
        }

        // Add Tx Power service
        status = gatt_db_svc16_add(user_lid, sec_lvl, GATT_SVC_TX_POWER, TXPS_IDX_NB,
                                   NULL, &(proxr_txps_att_db[0]), TXPS_IDX_NB, p_start_hdl);
        if(status != GAP_ERR_NO_ERROR) break;

        txp_start_hdl = *p_start_hdl;

        //-------------------- allocate memory required for the profile  ---------------------
        p_proxr_env = (proxr_env_t *) ke_malloc(sizeof(proxr_env_t), KE_MEM_ATT_DB);

        if(p_proxr_env != NULL)
        {
            // allocate PROXR required environment variable
            p_env->p_env = (prf_hdr_t *) p_proxr_env;
            p_proxr_env->lls_start_hdl = lls_start_hdl;
            p_proxr_env->ias_start_hdl = ias_start_hdl;
            p_proxr_env->txp_start_hdl = txp_start_hdl;

            p_proxr_env->features  = p_params->features;
            p_proxr_env->user_lid  = user_lid;
            memset(p_proxr_env->lls_alert_lvl, PROXR_ALERT_NONE, BLE_CONNECTION_MAX);

            // initialize profile environment variable
            p_proxr_env->prf_env.p_cb     = p_cb;
            #if (BLE_HL_MSG_API)
            p_env->desc.msg_handler_tab  = proxr_msg_handler_tab;
            p_env->desc.msg_cnt          = ARRAY_LEN(proxr_msg_handler_tab);
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
__STATIC uint16_t proxr_destroy(prf_data_t *p_env, uint8_t reason)
{
    uint16_t status = GAP_ERR_NO_ERROR;
    proxr_env_t *p_proxr_env = (proxr_env_t *) p_env->p_env;

    if(reason != PRF_DESTROY_RESET)
    {
        status = gatt_user_unregister(p_proxr_env->user_lid);
    }

    if(status == GAP_ERR_NO_ERROR)
    {
        // free profile environment variables
        p_env->p_env = NULL;
        ke_free(p_proxr_env);
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
__STATIC void proxr_con_create(prf_data_t *p_env, uint8_t conidx, const gap_con_param_t* p_con_param)
{
    proxr_env_t *p_proxr_env = (proxr_env_t *) p_env->p_env;
    p_proxr_env->lls_alert_lvl[conidx] = PROXR_ALERT_NONE;
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
__STATIC void proxr_con_cleanup(prf_data_t *p_env, uint8_t conidx, uint16_t reason)
{
    proxr_env_t *p_proxr_env = (proxr_env_t *) p_env->p_env;

    if ((p_proxr_env->features == PROXR_IAS_TXPS_SUP) && (reason != LL_ERR_REMOTE_USER_TERM_CON))
    {
        if (p_proxr_env->lls_alert_lvl[conidx] > PROXR_ALERT_NONE)
        {
            // inform application about the new alert level
            const proxr_cb_t* p_cb = (const proxr_cb_t*) p_proxr_env->prf_env.p_cb;
            p_cb->cb_alert_upd(conidx, PROXR_LLS_CHAR, p_proxr_env->lls_alert_lvl[conidx]);
        }
    }
    p_proxr_env->lls_alert_lvl[conidx] = PROXR_ALERT_NONE;
}

/// PROXR Task interface required by profile manager
const prf_task_cbs_t proxr_itf =
{
    .cb_init          = (prf_init_cb) proxr_init,
    .cb_destroy       = proxr_destroy,
    .cb_con_create    = proxr_con_create,
    .cb_con_cleanup   = proxr_con_cleanup,
    .cb_con_upd       = NULL,
};

/**
 ****************************************************************************************
 * @brief Retrieve service profile interface
 *
 * @return service profile interface
 ****************************************************************************************
 */
const prf_task_cbs_t *proxr_prf_itf_get(void)
{
    return &proxr_itf;
}

#endif //BLE_PROX_REPORTER

/// @} PROXR

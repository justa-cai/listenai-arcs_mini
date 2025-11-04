/**
 ****************************************************************************************
 *
 * @file scpps.c
 *
 * @brief Scan Parameters Profile Server implementation.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup SCPPS
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "btip_config.h"

#if (BLE_SP_SERVER)

#include "scpps.h"
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


#define SCPPS_CFG_FLAG_MANDATORY_MASK            (0x07)
#define SCPPS_CFG_FLAG_SCAN_REFRESH_MASK         (0x38)

/// Scanning set parameters range min (0x4)
#define SCPPS_SCAN_INTERVAL_MIN                  (4)
/// Scanning set parameters range max (0x4000)
#define SCPPS_SCAN_INTERVAL_MAX                  (16384)

/// Scanning set parameters range min (0x4)
#define SCPPS_SCAN_WINDOW_MIN                    (4)
/// Scanning set parameters range max (0x4000)
#define SCPPS_SCAN_WINDOW_MAX                    (16384)

/// Scan Parameters Service Attributes Indexes
enum scpps_att_db_handles
{
    SCPS_IDX_SVC,

    SCPS_IDX_SCAN_INTV_WD_CHAR,
    SCPS_IDX_SCAN_INTV_WD_VAL,

    SCPS_IDX_SCAN_REFRESH_CHAR,
    SCPS_IDX_SCAN_REFRESH_VAL,
    SCPS_IDX_SCAN_REFRESH_NTF_CFG,

    SCPS_IDX_NB,
};

enum
{
    SCPP_SERVER_REQUIRES_REFRESH    = 0x00,
};

/*
 * TYPES DEFINITION
 ****************************************************************************************
 */


/// Scan parameter service server environment variable
typedef struct scpps_env
{
    /// profile environment
    prf_hdr_t prf_env;
    /// Service Attribute Start Handle
    uint16_t  start_hdl;
    /// Database configuration
    uint8_t   features;
    /// GATT user local identifier
    uint8_t   user_lid;
    /// Notification configuration of peer devices.
    uint8_t   ntf_cfg[BLE_CONNECTION_MAX];

} scpps_env_t;


/*
 * ATTRIBUTES DATABASE
 ****************************************************************************************
 */

/// Full Database Description - Used to add attributes into the database
const gatt_att16_desc_t scpps_att_db[SCPS_IDX_NB] =
{
    // Scan Parameters Service Declaration
    [SCPS_IDX_SVC]                  = { GATT_DECL_PRIMARY_SERVICE, PROP(RD),            0                                            },

    // Scan Interval Window Characteristic Declaration
    [SCPS_IDX_SCAN_INTV_WD_CHAR]    = { GATT_DECL_CHARACTERISTIC,  PROP(RD),            0                                            },
    // Scan Interval Window Characteristic Value
    [SCPS_IDX_SCAN_INTV_WD_VAL]     = { GATT_CHAR_SCAN_INTV_WD,    PROP(WC),            OPT(NO_OFFSET) | sizeof(scpp_scan_intv_wd_t) },

    // Scan Refresh Characteristic Declaration
    [SCPS_IDX_SCAN_REFRESH_CHAR]    = { GATT_DECL_CHARACTERISTIC,  PROP(RD),            0                                            },
    // Scan Refresh Characteristic Value
    [SCPS_IDX_SCAN_REFRESH_VAL]     = { GATT_CHAR_SCAN_REFRESH,    PROP(N),             0                                            },
    // Scan Refresh Characteristic - Client Characteristic Configuration Descriptor
    [SCPS_IDX_SCAN_REFRESH_NTF_CFG] = { GATT_DESC_CLIENT_CHAR_CFG, PROP(RD) | PROP(WR), 0                                            },
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
__STATIC void scpps_cb_att_read_get(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset,
                                   uint16_t max_length)
{
    scpps_env_t *p_scpps_env = PRF_ENV_GET(SCPPS, scpps);
    // retrieve value attribute
    co_buf_t* p_buf       = NULL;
    uint16_t  status      = GAP_ERR_NO_ERROR;
    uint16_t  att_val_len = 0;

    if((p_scpps_env == NULL) || (hdl != (p_scpps_env->start_hdl + SCPS_IDX_SCAN_REFRESH_NTF_CFG)))
    {
        status = PRF_APP_ERROR;
    }
    else if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, sizeof(uint16_t), GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
    {
        co_write16p(co_buf_data(p_buf), co_htobs((uint16_t) p_scpps_env->ntf_cfg[conidx]));
        att_val_len = co_buf_data_len(p_buf);
    }
    else
    {
        status = ATT_ERR_INSUFF_RESOURCE;
    }

    gatt_srv_att_read_get_cfm(conidx, user_lid, token, status, att_val_len, p_buf);
    if(p_buf != NULL)
    {
        co_buf_release(p_buf);
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
 ****************************************************************************************SCPPC_DESC_SCAN_REFRESH_CFG
 */
__STATIC void scpps_cb_att_val_set(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset,
                                   co_buf_t* p_data)
{
    scpps_env_t *p_scpps_env = PRF_ENV_GET(SCPPS, scpps);
    uint16_t  status      = PRF_APP_ERROR;

    if(p_scpps_env != NULL)
    {
        const scpps_cb_t* p_cb  = (const scpps_cb_t*) p_scpps_env->prf_env.p_cb;

        switch (hdl - p_scpps_env->start_hdl)
        {
            case SCPS_IDX_SCAN_REFRESH_NTF_CFG:
            {
                uint16_t cfg = co_btohs(co_read16p(co_buf_data(p_data)));

                // parameter check
                if(   (co_buf_data_len(p_data) == sizeof(uint16_t))
                   && ((cfg == PRF_CLI_STOP_NTFIND) || (cfg == PRF_CLI_START_NTF)))
                {
                    p_scpps_env->ntf_cfg[conidx] = (uint8_t) cfg;

                    // inform application about update
                    p_cb->cb_bond_data_upd(conidx, cfg);
                    status = GAP_ERR_NO_ERROR;
                }
                else
                {
                    status = PRF_CCCD_IMPR_CONFIGURED;
                }
            } break;

            case SCPS_IDX_SCAN_INTV_WD_VAL:
            {
                if(co_buf_data_len(p_data) >= sizeof(scpp_scan_intv_wd_t))
                {
                    scpp_scan_intv_wd_t scan_intv_wd;
                    scan_intv_wd.le_scan_intv   = co_btohs(co_read16p(co_buf_data(p_data)));
                    co_buf_head_release(p_data, 2);
                    scan_intv_wd.le_scan_window = co_btohs(co_read16p(co_buf_data(p_data)));

                    // Check interval and window validity
                    if (   (scan_intv_wd.le_scan_window <= scan_intv_wd.le_scan_intv)
                        && (scan_intv_wd.le_scan_window <= SCPPS_SCAN_WINDOW_MAX)
                        && (scan_intv_wd.le_scan_window >= SCPPS_SCAN_WINDOW_MIN)
                        && (scan_intv_wd.le_scan_intv   <= SCPPS_SCAN_INTERVAL_MAX)
                        && (scan_intv_wd.le_scan_intv   >= SCPPS_SCAN_INTERVAL_MIN))
                    {
                        // Inform APP of Scan Interval Window change
                        p_cb->cb_scan_intv_wd_upd(conidx, &scan_intv_wd);
                        status = GAP_ERR_NO_ERROR;
                    }
                }
            } break;

            default: { status = ATT_ERR_REQUEST_NOT_SUPPORTED; } break;
        }
    }

    gatt_srv_att_val_set_cfm(conidx, user_lid, token, status);
}

/**
 ****************************************************************************************
 * @brief This function is called when GATT server user has initiated event send to peer
 *        device or if an error occurs.
 *
 * @param[in] conidx        Connection index
 * @param[in] user_lid      GATT user local identifier
 * @param[in] dummy         Dummy parameter provided by upper layer for command execution
 * @param[in] status        Status of the procedure (@see enum hl_err)
 ****************************************************************************************
 */
__STATIC void scpps_cb_event_sent(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    // refresh scan done
    scpps_env_t *p_scpps_env = PRF_ENV_GET(SCPPS, scpps);
    if(p_scpps_env != NULL)
    {
        const scpps_cb_t* p_cb  = (const scpps_cb_t*) p_scpps_env->prf_env.p_cb;
        p_cb->cb_scan_refresh_send_cmp(conidx, status);
    }
}


/// Service callback hander
__STATIC const gatt_srv_cb_t scpps_cb =
{
        .cb_event_sent    = scpps_cb_event_sent,
        .cb_att_read_get  = scpps_cb_att_read_get,
        .cb_att_event_get = NULL,
        .cb_att_info_get  = NULL,
        .cb_att_val_set   = scpps_cb_att_val_set,
};

/*
 * PROFILE NATIVE HANDLERS
 ****************************************************************************************
 */

uint16_t scpps_enable(uint8_t conidx, uint8_t ntf_cfg)
{
    scpps_env_t* p_scpps_env = PRF_ENV_GET(SCPPS, scpps);
    uint16_t status = PRF_ERR_REQ_DISALLOWED;

    if(p_scpps_env != NULL)
    {
        // check state of the task
        if (gapc_get_conhdl(conidx) != GAP_INVALID_CONHDL)
        {
            // restore Bond Data
            p_scpps_env->ntf_cfg[conidx] = ntf_cfg;
            status = GAP_ERR_NO_ERROR;
        }
    }

    return (status);
}

uint16_t scpps_scan_refresh_send(uint8_t conidx)
{
    scpps_env_t* p_scpps_env = PRF_ENV_GET(SCPPS, scpps);
    uint16_t status = PRF_ERR_REQ_DISALLOWED;

    if(p_scpps_env != NULL)
    {
        co_buf_t* p_buf;

        // check if Notification supported
        if((p_scpps_env->features & SCPPS_SCAN_REFRESH_SUP) == 0)
        {
            status = PRF_ERR_FEATURE_NOT_SUPPORTED;
        }
        else if ((p_scpps_env->ntf_cfg[conidx] & PRF_CLI_START_NTF) == 0)
        {
            status = PRF_ERR_NTF_DISABLED;
        }
        else if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, sizeof(uint8_t), GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
        {
            co_buf_data(p_buf)[0] = SCPP_SERVER_REQUIRES_REFRESH;
            status = gatt_srv_event_send(conidx, p_scpps_env->user_lid, 0, GATT_NOTIFY,
                                         p_scpps_env->start_hdl + SCPS_IDX_SCAN_REFRESH_VAL, p_buf);
        }
        else
        {
            status = GAP_ERR_INSUFF_RESOURCES;
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
 * @brief Handles reception of the @ref SCPPS_ENABLE_REQ message.
 * The handler enables the Scan Parameters Profile Server Role.
 * @param[in] msgid Id of the message received (probably unused).
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int scpps_enable_req_handler(ke_msg_id_t const msgid, struct scpps_enable_req const *p_param,
                                      ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = scpps_enable(p_param->conidx, p_param->ntf_cfg);

    struct scpps_enable_rsp *p_rsp;
    p_rsp = KE_MSG_ALLOC(SCPPS_ENABLE_RSP, src_id, dest_id, scpps_enable_rsp);
    // Send the message to the application
    if(p_rsp != NULL)
    {
        p_rsp->conidx      = p_param->conidx;
        p_rsp->status      = status;

        ke_msg_send(p_rsp);
    }

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief Handles reception of the @ref SCPPS_SCAN_REFRESH_SEND_CMD message.
 * @param[in] msgid Id of the message received (probably unused).
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int scpps_scan_refresh_send_cmd_handler(ke_msg_id_t const msgid, struct scpps_scan_refresh_send_cmd const *p_param,
                                                 ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = scpps_scan_refresh_send(p_param->conidx);

    if (status != GAP_ERR_NO_ERROR)
    {
        struct scpps_cmp_evt *p_evt;
        p_evt = KE_MSG_ALLOC(SCPPS_CMP_EVT, src_id, dest_id, scpps_cmp_evt);
        // Send the message to the application
        if(p_evt != NULL)
        {
            p_evt->conidx      = p_param->conidx;
            p_evt->operation   = SCPPS_SCAN_REFRESH_SEND_CMD_OP_CODE;
            p_evt->status      = status;

            ke_msg_send(p_evt);
        }
    }

    return (KE_MSG_CONSUMED);
}

/// Default State handlers definition
KE_MSG_HANDLER_TAB(scpps)
{
    // Note: all messages must be sorted in ID ascending order

    { SCPPS_ENABLE_REQ,              (ke_msg_func_t) scpps_enable_req_handler            },
    { SCPPS_SCAN_REFRESH_SEND_CMD,   (ke_msg_func_t) scpps_scan_refresh_send_cmd_handler },
};

/**
 ****************************************************************************************
 * @brief Completion of scan refresh notification procedure
 *
 * @param[in] conidx        Connection index
 * @param[in] status Status of the procedure execution (@see enum hl_err)
 ****************************************************************************************
 */
__STATIC void scpps_cb_scan_refresh_send_cmp(uint8_t conidx, uint16_t status)
{
    ke_task_id_t src_id = PRF_SRC_TASK(SCPPS);
    ke_task_id_t dest_id = PRF_DST_TASK(SCPPS);
    struct scpps_cmp_evt *p_evt;
    p_evt = KE_MSG_ALLOC(SCPPS_CMP_EVT, dest_id, src_id, scpps_cmp_evt);
    // Send the message to the application
    if(p_evt != NULL)
    {
        p_evt->conidx      = conidx;
        p_evt->operation   = SCPPS_SCAN_REFRESH_SEND_CMD_OP_CODE;
        p_evt->status      = status;

        ke_msg_send(p_evt);
    }
}

/**
 ****************************************************************************************
 * @brief Inform that bond data updated for the connection.
 *
 * @param[in] conidx        Connection index
 * @param[in] ntf_cfg             Scan Refresh Notification Configurations
 ****************************************************************************************
 */
__STATIC void scpps_cb_bond_data_upd(uint8_t conidx, uint8_t ntf_cfg)
{
    ke_task_id_t src_id = PRF_SRC_TASK(SCPPS);
    ke_task_id_t dest_id = PRF_DST_TASK(SCPPS);
    struct scpps_scan_refresh_ntf_cfg_ind *p_ind;

    p_ind = KE_MSG_ALLOC(SCPPS_SCAN_REFRESH_NTF_CFG_IND, dest_id, src_id, scpps_scan_refresh_ntf_cfg_ind);
    if(p_ind != NULL)
    {
        p_ind->conidx      = conidx;
        p_ind->ntf_cfg     = ntf_cfg;

        ke_msg_send(p_ind);
    }
}

/**
 ****************************************************************************************
 * @brief Inform that peer device has updated scan interval and scan window parameters
 *
 * @param[in] conidx         Connection index
 * @param[in] p_scan_intv_wd Pointer to Scan Interval Window parameters
 ****************************************************************************************
 */
__STATIC void scpps_cb_scan_intv_wd_upd(uint8_t conidx, const scpp_scan_intv_wd_t* p_scan_intv_wd)
{
    ke_task_id_t src_id = PRF_SRC_TASK(SCPPS);
    ke_task_id_t dest_id = PRF_DST_TASK(SCPPS);
    struct scpps_scan_intv_wd_ind *p_ind;

    p_ind = KE_MSG_ALLOC(SCPPS_SCAN_INTV_WD_IND, dest_id, src_id, scpps_scan_intv_wd_ind);
    if(p_ind != NULL)
    {
        p_ind->conidx      = conidx;
        memcpy(&(p_ind->scan_intv_wd), p_scan_intv_wd, sizeof(scpp_scan_intv_wd_t));

        ke_msg_send(p_ind);
    }
}

/// Default Message handle
__STATIC const scpps_cb_t scpps_msg_cb =
{
    .cb_scan_refresh_send_cmp = scpps_cb_scan_refresh_send_cmp,
    .cb_bond_data_upd         = scpps_cb_bond_data_upd,
    .cb_scan_intv_wd_upd      = scpps_cb_scan_intv_wd_upd,

};
#endif // (BLE_HL_MSG_API)



/*
 * PROFILE DEFAULT HANDLERS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Initialization of the SCPPS module.
 * This function performs all the initializations of the Profile module.
 *  - Creation of database (if it's a service)
 *  - Allocation of profile required memory
 *  - Initialization of task descriptor to register application
 *      - Task State array
 *      - Number of tasks
 *      - Default task handler
 *
 * @param[out]    p_env      Collector or Service allocated environment data.
 * @param[in|out] p_start_hdl  Service start handle (0 - dynamically allocated), only applies for services.
 * @param[in]     sec_lvl    Security level (@see enum gatt_svc_info_bf)
 * @param[in]     p_param    Configuration parameters of profile collector or service (32 bits aligned)
 * @param[in]     p_cb       Callback structure that handles event from profile
 *
 * @return status code to know if profile initialization succeed or not.
 ****************************************************************************************
 */
__STATIC uint16_t scpps_init(prf_data_t *p_env, uint16_t *p_start_hdl, uint8_t sec_lvl, uint8_t user_prio,
                          struct scpps_db_cfg *p_params, const scpps_cb_t* p_cb)
{
    //------------------ create the attribute database for the profile -------------------

    // DB Creation Status
    uint16_t status = GAP_ERR_NO_ERROR;
    uint8_t user_lid = GATT_INVALID_USER_LID;

    do
    {
        scpps_env_t* p_scpps_env;
        // Service content flag
        uint32_t cfg_flag = SCPPS_CFG_FLAG_MANDATORY_MASK;

        #if (BLE_HL_MSG_API)
        if(p_cb == NULL)
        {
            p_cb = &(scpps_msg_cb);
        }
        #endif // (BLE_HL_MSG_API)

        if(   (p_params == NULL) || (p_start_hdl == NULL) || (p_cb == NULL) || (p_cb->cb_scan_refresh_send_cmp == NULL)
           || (p_cb->cb_bond_data_upd == NULL) || (p_cb->cb_scan_intv_wd_upd == NULL))
        {
            status = GAP_ERR_INVALID_PARAM;
            break;
        }

        // register SCPPS user
        status = gatt_user_srv_register(L2CAP_LE_MTU_MIN, user_prio, &scpps_cb, &user_lid);
        if(status != GAP_ERR_NO_ERROR) break;

        // Check if multiple instances
        if ((p_params->features & SCPPS_SCAN_REFRESH_SUP) != 0)
        {
            cfg_flag |= SCPPS_CFG_FLAG_SCAN_REFRESH_MASK;
        }

        // Add GAP service
        status = gatt_db_svc16_add(user_lid, sec_lvl, GATT_SVC_SCAN_PARAMETERS, SCPS_IDX_NB,
                                   (uint8_t *)&cfg_flag, &(scpps_att_db[0]), SCPS_IDX_NB, p_start_hdl);
        if(status != GAP_ERR_NO_ERROR) break;

        //-------------------- allocate memory required for the profile  ---------------------
        p_scpps_env = (scpps_env_t *) ke_malloc(sizeof(scpps_env_t), KE_MEM_ATT_DB);

        if(p_scpps_env != NULL)
        {
            // allocate SCPPS required environment variable
            p_env->p_env = (prf_hdr_t *) p_scpps_env;
            p_scpps_env->start_hdl = *p_start_hdl;
            p_scpps_env->features  = p_params->features;
            p_scpps_env->user_lid  = user_lid;
            memset(p_scpps_env->ntf_cfg, 0, BLE_CONNECTION_MAX);

            // initialize profile environment variable
            p_scpps_env->prf_env.p_cb     = p_cb;
            #if (BLE_HL_MSG_API)
            p_env->desc.msg_handler_tab  = scpps_msg_handler_tab;
            p_env->desc.msg_cnt          = ARRAY_LEN(scpps_msg_handler_tab);
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
__STATIC uint16_t scpps_destroy(prf_data_t *p_env, uint8_t reason)
{
    uint16_t status = GAP_ERR_NO_ERROR;
    scpps_env_t *p_scpps_env = (scpps_env_t *) p_env->p_env;

    if(reason != PRF_DESTROY_RESET)
    {
        status = gatt_user_unregister(p_scpps_env->user_lid);
    }

    if(status == GAP_ERR_NO_ERROR)
    {
        // free profile environment variables
        p_env->p_env = NULL;
        ke_free(p_scpps_env);
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
__STATIC void scpps_con_create(prf_data_t *p_env, uint8_t conidx, const gap_con_param_t* p_con_param)
{
    scpps_env_t *p_scpps_env = (scpps_env_t *) p_env->p_env;
    // force notification config to zero when peer device is disconnected
    p_scpps_env->ntf_cfg[conidx] = 0;
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
__STATIC void scpps_con_cleanup(prf_data_t *p_env, uint8_t conidx, uint16_t reason)
{
    scpps_env_t *p_scpps_env = (scpps_env_t *) p_env->p_env;
    // force notification config to zero when peer device is disconnected
    p_scpps_env->ntf_cfg[conidx] = 0;
}

/// SCPPS Task interface required by profile manager
const prf_task_cbs_t scpps_itf =
{
    .cb_init          = (prf_init_cb) scpps_init,
    .cb_destroy       = scpps_destroy,
    .cb_con_create    = scpps_con_create,
    .cb_con_cleanup   = scpps_con_cleanup,
    .cb_con_upd       = NULL,
};

/**
 ****************************************************************************************
 * @brief Retrieve service profile interface
 *
 * @return service profile interface
 ****************************************************************************************
 */
const prf_task_cbs_t *scpps_prf_itf_get(void)
{
    return &scpps_itf;
}

#endif // (BLE_SP_SERVER)

/// @} SCPPS

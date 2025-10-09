/**
 ****************************************************************************************
 *
 * @file proxm.c
 *
 * @brief Proximity monitor implementation.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup PROXM
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "btip_config.h"

#if (BLE_PROX_MONITOR)

#include "proxm.h"
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

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */


/// Environment variable for each Connections
typedef struct proxm_cnx_env
{
    /// Services content information
    svc_content_t       prox[PROXM_SVC_NB];
    /// counter used to check service uniqueness
    uint8_t             nb_svc;
    /// Client is in discovering state
    bool                discover;
} proxm_cnx_env_t;

/// Client environment variable
typedef struct proxm_env
{
    /// profile environment
    prf_hdr_t            prf_env;
    /// Environment variable pointer for each connections
    proxm_cnx_env_t*     p_env[BLE_CONNECTION_MAX];
    /// GATT User local identifier
    uint8_t              user_lid;
} proxm_env_t;


/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */

/// State machine used to retrieve services characteristics information
const prf_char_def_t proxm_svc_char[PROXM_SVC_NB][PROXM_CHAR_NB_MAX] =
{
    [PROXM_LK_LOSS_SVC]  = { { GATT_CHAR_ALERT_LEVEL,    ATT_REQ(PRES, MAND), PROP(RD) | PROP(WR) } },
    [PROXM_IAS_SVC]      = { { GATT_CHAR_ALERT_LEVEL,    ATT_REQ(PRES, MAND), PROP(WC)            } },
    [PROXM_TX_POWER_SVC] = { { GATT_CHAR_TX_POWER_LEVEL, ATT_REQ(PRES, MAND), PROP(RD)            } }
};

/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Send discovery results to application.
 *
 * @param p_proxm_env    Client Role task environment
 * @param conidx        Connection index
 * @param status        Response status code
 *****************************************************************************************
 */
__STATIC void proxm_enable_cmp(proxm_env_t* p_proxm_env, uint8_t conidx, uint16_t status)
{
    const proxm_cb_t* p_cb = (const proxm_cb_t*) p_proxm_env->prf_env.p_cb;

    if(p_proxm_env != NULL)
    {
        proxm_cnx_env_t* p_con_env = p_proxm_env->p_env[conidx];

        p_cb->cb_enable_cmp(conidx, status, &(p_con_env->prox[PROXM_LK_LOSS_SVC]), &(p_con_env->prox[PROXM_IAS_SVC]),
                            &(p_con_env->prox[PROXM_TX_POWER_SVC]));

        if (status != GAP_ERR_NO_ERROR)
        {
            // clean-up environment variable allocated for task instance
            ke_free(p_con_env);
            p_proxm_env->p_env[conidx] = NULL;
        }
        else
        {
             p_con_env->discover = false;
        }
    }
}

/**
 ****************************************************************************************
 * @brief Send read result to application,.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the request
 * @param[in] val_id        Value Identifier (@see enum proxm_rd_char_code)
 * @param[in] length        Length of data value)
 * @param[in] p_data        Pointer of data value
 ****************************************************************************************
 */
__STATIC void proxm_read_val_cmp(uint8_t conidx, uint16_t status, uint8_t val_id, co_buf_t* p_data)
{
    proxm_env_t* p_proxm_env = PRF_ENV_GET(PROXM, proxm);

    if(p_proxm_env != NULL)
    {
        const proxm_cb_t* p_cb = (const proxm_cb_t*) p_proxm_env->prf_env.p_cb;

        switch(val_id)
        {
            case PROXM_RD_LL_ALERT_LVL:
            {
                uint8_t alert_lvl = 0;
                if(p_data != NULL)
                {
                    alert_lvl = co_buf_data(p_data)[0];
                }
                p_cb->cb_read_lls_alert_lvl_cmp(conidx, status, alert_lvl);
            } break;
            case PROXM_RD_TX_POWER_LVL:
            {
                int8_t pwr_lvl = 0;
                if(p_data != NULL)
                {
                    pwr_lvl = (int8_t) co_buf_data(p_data)[0];
                }
                p_cb->cb_read_tx_pwr_lvl_cmp(conidx, status, pwr_lvl);
            } break;
            default: { ASSERT_ERR(0); } break;
        }
    }
}


/**
 ****************************************************************************************
 * @brief Perform Value read procedure.
 *
 * @param[in] conidx        Connection index
 * @param[in] val_id        Value Identifier
 ****************************************************************************************
 */
__STATIC uint16_t proxm_read_val(uint8_t conidx, uint16_t val_id)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    proxm_env_t* p_proxm_env = PRF_ENV_GET(PROXM, proxm);

    if(p_proxm_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_proxm_env->p_env[conidx] != NULL) && (!p_proxm_env->p_env[conidx]->discover))
        {
            proxm_cnx_env_t* p_con_env = p_proxm_env->p_env[conidx];
            uint16_t hdl;

            switch(val_id)
            {
                case PROXM_RD_LL_ALERT_LVL: { hdl = p_con_env->prox[PROXM_LK_LOSS_SVC].chars[0].val_hdl;  } break;
                case PROXM_RD_TX_POWER_LVL: { hdl = p_con_env->prox[PROXM_TX_POWER_SVC].chars[0].val_hdl; } break;
                default:                    { hdl = GATT_INVALID_HDL;                                     } break;
            }

            if(hdl == GATT_INVALID_HDL)
            {
                status = PRF_ERR_INEXISTENT_HDL;
            }
            else
            {
                // perform read request
                status = gatt_cli_read(conidx, p_proxm_env->user_lid, val_id, hdl, 0, 0);
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
__STATIC void proxm_svc_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t hdl, uint8_t disc_info,
                           uint8_t nb_att, const gatt_svc_att_t* p_atts)
{
    // Get the address of the environment
    proxm_env_t* p_proxm_env = PRF_ENV_GET(PROXM, proxm);

    if(p_proxm_env != NULL)
    {
        proxm_cnx_env_t* p_con_env = p_proxm_env->p_env[conidx];

        ASSERT_INFO(p_con_env != NULL, conidx, user_lid);

        if (p_con_env->nb_svc == 0)
        {
            //Even if we get multiple responses we only store 1 range
            if((disc_info == GATT_SVC_CMPLT) || (disc_info == GATT_SVC_START))
            {
                p_con_env->prox[dummy].svc.shdl = hdl;
            }

            if((disc_info == GATT_SVC_CMPLT) || (disc_info == GATT_SVC_END))
            {
                p_con_env->prox[dummy].svc.ehdl = hdl + nb_att -1;
            }

            // Retrieve characteristics
            prf_extract_svc_info(hdl, nb_att, p_atts,
                                 PROXM_CHAR_NB_MAX, proxm_svc_char[dummy], &(p_con_env->prox[dummy].chars[0]),
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
__STATIC void proxm_discover_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    // Get the address of the environment
    proxm_env_t* p_proxm_env = PRF_ENV_GET(PROXM, proxm);

    if(p_proxm_env != NULL)
    {
        proxm_cnx_env_t* p_con_env = p_proxm_env->p_env[conidx];

        if (p_con_env->nb_svc ==  1)
        {
            status = prf_check_svc_char_validity(PROXM_CHAR_NB_MAX, p_con_env->prox[dummy].chars, proxm_svc_char[dummy]);
        }
        // too much services
        else if (p_con_env->nb_svc > 1)
        {
            status = PRF_ERR_MULTIPLE_SVC;
        }
        // if an optional service is not present does not raise an error
        else if ((status == ATT_ERR_ATTRIBUTE_NOT_FOUND) && (dummy == PROXM_IAS_SVC))
        {
            status = GAP_ERR_NO_ERROR;
        }

        // check next service
        dummy += 1;
        if((status == GAP_ERR_NO_ERROR) && (dummy < PROXM_SVC_NB))
        {
            uint16_t gatt_svc_uuid = GATT_SVC_LINK_LOSS;
            switch (dummy)
            {
                case PROXM_IAS_SVC:      { gatt_svc_uuid = GATT_SVC_IMMEDIATE_ALERT; } break;
                case PROXM_TX_POWER_SVC: { gatt_svc_uuid = GATT_SVC_TX_POWER;        } break;
                default:                 { ASSERT_ERR(0);                            } break;
            }

            // start discovery
            p_con_env->nb_svc = 0;
            status = gatt_cli_discover_svc(conidx, p_proxm_env->user_lid, dummy,
                                           GATT_DISCOVER_SVC_PRIMARY_BY_UUID, true,
                                           GATT_MIN_HDL, GATT_MAX_HDL, GATT_UUID_16, (uint8_t*) &gatt_svc_uuid);
        }

        if((status != GAP_ERR_NO_ERROR) || (dummy == PROXM_SVC_NB))
        {
            proxm_enable_cmp(p_proxm_env, conidx, status);
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
__STATIC void proxm_att_val_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t hdl, uint16_t offset,
                              co_buf_t* p_data)
{
    proxm_read_val_cmp(conidx, GAP_ERR_NO_ERROR, (uint8_t) dummy, p_data);
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
__STATIC void proxm_read_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    if(status != GAP_ERR_NO_ERROR)
    {
        proxm_read_val_cmp(conidx, status, (uint8_t) dummy, NULL);
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
__STATIC void proxm_write_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    proxm_env_t* p_proxm_env = PRF_ENV_GET(PROXM, proxm);

    if(p_proxm_env != NULL)
    {
        const proxm_cb_t* p_cb = (const proxm_cb_t*) p_proxm_env->prf_env.p_cb;
        p_cb->cb_alert_upd_cmp(conidx, dummy, status);
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
__STATIC void proxm_svc_changed_cb(uint8_t conidx, uint8_t user_lid, bool out_of_sync, uint16_t start_hdl, uint16_t end_hdl)
{
    // Do Nothing
}

/// Client callback hander
__STATIC const gatt_cli_cb_t proxm_cb =
{
    .cb_discover_cmp    = proxm_discover_cmp_cb,
    .cb_read_cmp        = proxm_read_cmp_cb,
    .cb_write_cmp       = proxm_write_cmp_cb,
    .cb_att_val_get     = NULL,
    .cb_svc             = proxm_svc_cb,
    .cb_svc_info        = NULL,
    .cb_inc_svc         = NULL,
    .cb_char            = NULL,
    .cb_desc            = NULL,
    .cb_att_val         = proxm_att_val_cb,
    .cb_att_val_evt     = NULL,
    .cb_svc_changed     = proxm_svc_changed_cb,
};

/*
 * PROFILE NATIVE API
 ****************************************************************************************
 */

uint16_t proxm_enable(uint8_t conidx, uint8_t con_type, const svc_content_t* p_lls, const svc_content_t* p_ias,
                      const svc_content_t* p_txps)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    proxm_env_t* p_proxm_env = PRF_ENV_GET(PROXM, proxm);

    if(p_proxm_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_proxm_env->p_env[conidx] == NULL))
        {
            // allocate environment variable for task instance
            p_proxm_env->p_env[conidx] = (struct proxm_cnx_env *) ke_malloc(sizeof(struct proxm_cnx_env), KE_MEM_ATT_DB);

            if(p_proxm_env->p_env[conidx] != NULL)
            {
                // Config connection, start discovering
                if (con_type == PRF_CON_DISCOVERY)
                {
                    uint16_t gatt_svc_uuid = GATT_SVC_LINK_LOSS;
                    memset(p_proxm_env->p_env[conidx], 0, sizeof(struct proxm_cnx_env));

                    // start discovery
                    status = gatt_cli_discover_svc(conidx, p_proxm_env->user_lid, PROXM_LK_LOSS_SVC,
                                                   GATT_DISCOVER_SVC_PRIMARY_BY_UUID, true,
                                                   GATT_MIN_HDL, GATT_MAX_HDL, GATT_UUID_16, (uint8_t*) &gatt_svc_uuid);

                    // Go to DISCOVERING state
                    p_proxm_env->p_env[conidx]->discover = true;
                }
                // normal connection, get saved att details
                else
                {
                    memcpy(&(p_proxm_env->p_env[conidx]->prox[PROXM_LK_LOSS_SVC]),  p_lls,  sizeof(svc_content_t));
                    memcpy(&(p_proxm_env->p_env[conidx]->prox[PROXM_IAS_SVC]),      p_ias,  sizeof(svc_content_t));
                    memcpy(&(p_proxm_env->p_env[conidx]->prox[PROXM_TX_POWER_SVC]), p_txps, sizeof(svc_content_t));
                    status = GAP_ERR_NO_ERROR;
                    // send APP confirmation that can start normal connection to TH
                    proxm_enable_cmp(p_proxm_env, conidx, GAP_ERR_NO_ERROR);
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

uint16_t proxm_alert_upd(uint8_t conidx, uint8_t svc_code, uint8_t alert_lvl)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    proxm_env_t* p_proxm_env = PRF_ENV_GET(PROXM, proxm);

    if(p_proxm_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_proxm_env->p_env[conidx] != NULL) && (!p_proxm_env->p_env[conidx]->discover))
        {
            proxm_cnx_env_t* p_con_env = p_proxm_env->p_env[conidx];
            uint16_t hdl;

            switch(svc_code)
            {
                case PROXM_SET_LK_LOSS_ALERT: { hdl = p_con_env->prox[PROXM_LK_LOSS_SVC].chars[0].val_hdl; } break;
                case PROXM_SET_IMMDT_ALERT:   { hdl = p_con_env->prox[PROXM_IAS_SVC].chars[0].val_hdl;     } break;
                default:                      { hdl = GATT_INVALID_HDL;                                    } break;
            }

            if(hdl == GATT_INVALID_HDL)
            {
                status = PRF_ERR_INEXISTENT_HDL;
            }
            else if(alert_lvl > PROXM_ALERT_HIGH)
            {
                status = PRF_ERR_INVALID_PARAM;
            }
            else
            {
                status = prf_gatt_write(conidx, p_proxm_env->user_lid, svc_code,
                                        (svc_code == PROXM_SET_IMMDT_ALERT) ? GATT_WRITE_NO_RESP : GATT_WRITE,
                                        hdl, sizeof(uint8_t), &alert_lvl);
            }
        }
    }

    return (status);
}

uint16_t proxm_read_lls_alert_lvl(uint8_t conidx)
{
    return (proxm_read_val(conidx, PROXM_RD_LL_ALERT_LVL));
}

uint16_t proxm_read_tx_pwr_lvl(uint8_t conidx)
{
    return (proxm_read_val(conidx, PROXM_RD_TX_POWER_LVL));
}

#if (BLE_HL_MSG_API)
/*
 * PROFILE MSG HANDLERS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Send a PROXM_CMP_EVT message to the task which enabled the profile
 * @param[in] conidx Connection index
 * @param[in] operation Operation
 * @param[in] status Satus
 ****************************************************************************************
 */
__STATIC void proxm_send_cmp_evt(uint8_t conidx, uint8_t operation, uint16_t status)
{
    struct proxm_cmp_evt *p_evt;

    // Send the message
    p_evt = KE_MSG_ALLOC(PROXM_CMP_EVT, PRF_DST_TASK(PROXM), PRF_SRC_TASK(PROXM), proxm_cmp_evt);
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
__STATIC int proxm_enable_req_handler(ke_msg_id_t const msgid, struct proxm_enable_req const *p_param,
                                     ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = proxm_enable(p_param->conidx, p_param->con_type, &(p_param->lls), &(p_param->ias), &(p_param->txps));

    // send an error if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct proxm_enable_rsp *p_rsp = KE_MSG_ALLOC(PROXM_ENABLE_RSP, src_id, dest_id, proxm_enable_rsp);
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
 * @brief Handles reception of the @ref PROXM_RD_CMD message.
 * Request to read the LLS alert level.
 * @param[in] msgid Id of the message received (probably unused).
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int proxm_rd_cmd_handler(ke_msg_id_t const msgid, struct proxm_rd_cmd const *p_param,
                                  ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status;

    switch (p_param->svc_code)
    {
        case PROXM_RD_LL_ALERT_LVL: { status = proxm_read_lls_alert_lvl(p_param->conidx); } break;
        case PROXM_RD_TX_POWER_LVL: { status = proxm_read_tx_pwr_lvl(p_param->conidx);    } break;
        default:                    { status = PRF_ERR_INVALID_PARAM;                     } break;
    }

    // send error response if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct proxm_cmp_evt *p_evt = KE_MSG_ALLOC(PROXM_CMP_EVT, src_id, dest_id, proxm_cmp_evt);

        if(p_evt != NULL)
        {
            p_evt->conidx      = p_param->conidx;
            p_evt->operation   = PROXM_RD_OP_CODE;
            p_evt->status      = status;

            ke_msg_send(p_evt);
        }
    }

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief Handles reception of the @ref PROXM_WR_ALERT_LVL_CMD message.
 * Request to write either the LLS or IAS alert levels.
 * @param[in] msgid Id of the message received (probably unused).
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int proxm_wr_alert_lvl_cmd_handler(ke_msg_id_t const msgid, struct proxm_wr_alert_lvl_cmd const *p_param,
                                            ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status;

    switch (p_param->svc_code)
    {
        case PROXM_SET_LK_LOSS_ALERT:
        case PROXM_SET_IMMDT_ALERT: { status = proxm_alert_upd(p_param->conidx, p_param->svc_code, p_param->lvl); } break;
        default:                    { status = PRF_ERR_INVALID_PARAM;                                             } break;
    }

    // send error response if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct proxm_cmp_evt *p_evt = KE_MSG_ALLOC(PROXM_CMP_EVT, src_id, dest_id, proxm_cmp_evt);

        if(p_evt != NULL)
        {
            p_evt->conidx      = p_param->conidx;
            p_evt->operation   = PROXM_WR_ALERT_LVL_OP_CODE;
            p_evt->status      = status;

            ke_msg_send(p_evt);
        }
    }

    return (KE_MSG_CONSUMED);
}

/// Default State handlers definition
KE_MSG_HANDLER_TAB(proxm)
{
    // Note: all messages must be sorted in ID ascending order

    {PROXM_ENABLE_REQ,        (ke_msg_func_t)proxm_enable_req_handler       },
    {PROXM_RD_CMD,            (ke_msg_func_t)proxm_rd_cmd_handler           },
    {PROXM_WR_ALERT_LVL_CMD,  (ke_msg_func_t)proxm_wr_alert_lvl_cmd_handler },
};

/**
 ****************************************************************************************
 * @brief This function is called when GATT server user has initiated event send to peer
 *        device or if an error occurs.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Client Enable status (@see enum hl_err)
 * @param[in] p_lls         Pointer to bond data information that describe link loss peer service
 * @param[in] p_ias         Pointer to bond data information that describe immediate alert peer service
 * @param[in] p_txps        Pointer to bond data information that describe TX power peer service
 ****************************************************************************************
 */
__STATIC void proxm_cb_enable_cmp(uint8_t conidx, uint16_t status, const svc_content_t* p_lls, const svc_content_t* p_ias,
                                  const svc_content_t* p_txps)
{
    // Send APP the details of the discovered attributes on PROXM
    struct proxm_enable_rsp *p_rsp = KE_MSG_ALLOC(PROXM_ENABLE_RSP, PRF_DST_TASK(PROXM), PRF_SRC_TASK(PROXM),
                                                 proxm_enable_rsp);
    if(p_rsp != NULL)
    {
        p_rsp->conidx = conidx;
        p_rsp->status = status;
        memcpy(&(p_rsp->lls),  p_lls,  sizeof(svc_content_t));
        memcpy(&(p_rsp->ias),  p_ias,  sizeof(svc_content_t));
        memcpy(&(p_rsp->txps), p_txps, sizeof(svc_content_t));
        ke_msg_send(p_rsp);
    }
}

/**
 ****************************************************************************************
 * @brief This function is called when Alert update procedure is over.
 *
 * @param[in] conidx        Connection index
 * @param[in] svc_code      0=LLS or 1=IAS, code for the service in which the alert level should be written
 * @param[in] status        Read status (@see enum hl_err)
 ****************************************************************************************
 */
__STATIC void proxm_cb_alert_upd_cmp(uint8_t conidx, uint8_t svc_code, uint16_t status)
{
    proxm_send_cmp_evt(conidx, PROXM_WR_ALERT_LVL_OP_CODE, status);
}

/**
 ****************************************************************************************
 * @brief This function is called when read link loss alert level procedure is over.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Client Enable status (@see enum hl_err)
 * @param[in] alert_lvl     Alert level (see enum proxm_alert_lvl)
 ****************************************************************************************
 */
__STATIC void proxm_cb_read_lls_alert_lvl_cmp(uint8_t conidx, uint16_t status, uint8_t alert_lvl)
{
    if(status == GAP_ERR_NO_ERROR)
    {
        ke_task_id_t src_id  = PRF_SRC_TASK(PROXM);
        ke_task_id_t dest_id = PRF_DST_TASK(PROXM);

        struct proxm_rd_ind *p_ind = KE_MSG_ALLOC(PROXM_RD_IND, dest_id, src_id, proxm_rd_ind);
        if(p_ind != NULL)
        {
            p_ind->conidx            = conidx;
            p_ind->svc_code          = PROXM_RD_LL_ALERT_LVL;
            p_ind->value             = alert_lvl;
            ke_msg_send(p_ind);
        }
    }

    proxm_send_cmp_evt(conidx, PROXM_RD_OP_CODE, status);

}

/**
 ****************************************************************************************
 * @brief This function is called when read peer ADV TX power level procedure is over.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Client Enable status (@see enum hl_err)
 * @param[in] power_lvl     Advertising channel TX power level
 ****************************************************************************************
 */
__STATIC void proxm_cb_read_tx_pwr_lvl_cmp(uint8_t conidx, uint16_t status, int8_t power_lvl)
{
    if(status == GAP_ERR_NO_ERROR)
    {
        ke_task_id_t src_id  = PRF_SRC_TASK(PROXM);
        ke_task_id_t dest_id = PRF_DST_TASK(PROXM);

        struct proxm_rd_ind *p_ind = KE_MSG_ALLOC(PROXM_RD_IND, dest_id, src_id, proxm_rd_ind);
        if(p_ind != NULL)
        {
            p_ind->conidx            = conidx;
            p_ind->svc_code          = PROXM_RD_TX_POWER_LVL;
            p_ind->value             = power_lvl;
            ke_msg_send(p_ind);
        }
    }

    proxm_send_cmp_evt(conidx, PROXM_RD_OP_CODE, status);
}

/// Default Message handle
__STATIC const proxm_cb_t proxm_msg_cb =
{
    .cb_enable_cmp             = proxm_cb_enable_cmp,
    .cb_alert_upd_cmp          = proxm_cb_alert_upd_cmp,
    .cb_read_lls_alert_lvl_cmp = proxm_cb_read_lls_alert_lvl_cmp,
    .cb_read_tx_pwr_lvl_cmp    = proxm_cb_read_tx_pwr_lvl_cmp,
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
__STATIC uint16_t proxm_init(prf_data_t* p_env, uint16_t* p_start_hdl, uint8_t sec_lvl, uint8_t user_prio,
                          const void* p_params, const proxm_cb_t* p_cb)
{
    uint8_t conidx;
    // DB Creation Status
    uint16_t status = GAP_ERR_NO_ERROR;
    uint8_t user_lid = GATT_INVALID_USER_LID;

    do
    {
        proxm_env_t* p_proxm_env;

        #if (BLE_HL_MSG_API)
        if(p_cb == NULL)
        {
            p_cb = &(proxm_msg_cb);
        }
        #endif // (BLE_HL_MSG_API)

        if(   (p_params == NULL) || (p_cb == NULL) || (p_cb->cb_enable_cmp == NULL) || (p_cb->cb_alert_upd_cmp == NULL)
           || (p_cb->cb_read_lls_alert_lvl_cmp == NULL) || (p_cb->cb_read_tx_pwr_lvl_cmp == NULL))
        {
            status = GAP_ERR_INVALID_PARAM;
            break;
        }

        // register PROXM user
        status = gatt_user_cli_register(L2CAP_LE_MTU_MIN, user_prio, &proxm_cb, &user_lid);
        if(status != GAP_ERR_NO_ERROR) break;

        //-------------------- allocate memory required for the profile  ---------------------
        p_proxm_env = (proxm_env_t*) ke_malloc(sizeof(proxm_env_t), KE_MEM_ATT_DB);

        if(p_proxm_env != NULL)
        {
            // allocate PROXM required environment variable
            p_env->p_env = (prf_hdr_t *) p_proxm_env;

            // initialize environment variable
            p_proxm_env->prf_env.p_cb    = p_cb;
            #if (BLE_HL_MSG_API)
            p_env->desc.msg_handler_tab = proxm_msg_handler_tab;
            p_env->desc.msg_cnt         = ARRAY_LEN(proxm_msg_handler_tab);
            #endif // (BLE_HL_MSG_API)

            p_proxm_env->user_lid = user_lid;
            for (conidx = 0; conidx < BLE_CONNECTION_MAX; conidx++)
            {
                p_proxm_env->p_env[conidx] = NULL;
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
__STATIC uint16_t proxm_destroy(prf_data_t *p_env, uint8_t reason)
{
    uint16_t status = GAP_ERR_NO_ERROR;
    proxm_env_t* p_proxm_env = (proxm_env_t*) p_env->p_env;

    if(reason != PRF_DESTROY_RESET)
    {
        status = gatt_user_unregister(p_proxm_env->user_lid);
    }

    if(status == GAP_ERR_NO_ERROR)
    {
        uint8_t idx;

        // cleanup environment variable for each task instances
        for (idx = 0; idx < BLE_CONNECTION_MAX; idx++)
        {
            if (p_proxm_env->p_env[idx] != NULL)
            {
                ke_free(p_proxm_env->p_env[idx]);
            }
        }

        // free profile environment variables
        p_env->p_env = NULL;
        ke_free(p_proxm_env);
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
__STATIC void proxm_con_create(prf_data_t *p_env, uint8_t conidx, const gap_con_param_t* p_con_param)
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
__STATIC void proxm_con_cleanup(prf_data_t *p_env, uint8_t conidx, uint16_t reason)
{
    proxm_env_t* p_proxm_env = (proxm_env_t*) p_env->p_env;

    // clean-up environment variable allocated for task instance
    if (p_proxm_env->p_env[conidx] != NULL)
    {
        ke_free(p_proxm_env->p_env[conidx]);
        p_proxm_env->p_env[conidx] = NULL;
    }
}

/// PROXM Task interface required by profile manager
const prf_task_cbs_t proxm_itf =
{
    .cb_init          = (prf_init_cb) proxm_init,
    .cb_destroy       = proxm_destroy,
    .cb_con_create    = proxm_con_create,
    .cb_con_cleanup   = proxm_con_cleanup,
    .cb_con_upd       = NULL,
};

/**
 ****************************************************************************************
 * @brief Retrieve client profile interface
 *
 * @return Client profile interface
 ****************************************************************************************
 */
const prf_task_cbs_t* proxm_prf_itf_get(void)
{
    return &proxm_itf;
}

#endif //BLE_PROX_MONITOR

/// @} PROXM

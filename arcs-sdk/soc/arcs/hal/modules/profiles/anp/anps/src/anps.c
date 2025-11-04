/**
 ****************************************************************************************
 *
 * @file anps.c
 *
 * @brief Alert Notification Profile Server implementation.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup ANPS
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "btip_config.h"

#if (BLE_AN_SERVER)

#include "gatt.h"

#include "anps.h"
#include "anp_common.h"

#include "ble_prf.h"

#include "ke_mem.h"

#include "co_utils.h"
#include "co_endian.h"
#include <string.h>


/*
 * MACROS
 * **************************************************************************************
 */

#define ANPS_IS_NEW_ALERT_CATEGORY_SUPPORTED(category_id) \
        (((p_anps_env->supp_new_alert_cat >> category_id) & 1) == 1)

#define ANPS_IS_UNREAD_ALERT_CATEGORY_SUPPORTED(category_id) \
        (((p_anps_env->supp_unread_alert_cat >> category_id) & 1) == 1)

#define ANPS_IS_ALERT_ENABLED(conidx, idx_env, alert_type) \
        (((idx_env->p_env[conidx]->ntf_cfg >> alert_type) & 1) == 1)

#define ANPS_IS_NEW_ALERT_CATEGORY_ENABLED(conidx, category_id, idx_env) \
        (((idx_env->p_env[conidx]->ntf_new_alert_cfg >> category_id) & 1) == 1)

#define ANPS_IS_UNREAD_ALERT_CATEGORY_ENABLED(conidx, category_id, idx_env) \
        (((idx_env->p_env[conidx]->ntf_unread_alert_cfg >> category_id) & 1) == 1)

#define ANPS_ENABLE_ALERT(conidx, idx_env, alert_type) \
        (idx_env->p_env[conidx]->ntf_cfg |= (1 << alert_type))

#define ANPS_DISABLE_ALERT(conidx, idx_env, alert_type) \
        (idx_env->p_env[conidx]->ntf_cfg &= ~(1 << alert_type))

#define ANPS_ENABLE_NEW_ALERT_CATEGORY(conidx, category_id, idx_env) \
        (idx_env->p_env[conidx]->ntf_new_alert_cfg |= (1 << category_id))

#define ANPS_ENABLE_UNREAD_ALERT_CATEGORY(conidx, category_id, idx_env) \
        (idx_env->p_env[conidx]->ntf_unread_alert_cfg |= (1 << category_id))

#define ANPS_DISABLE_NEW_ALERT_CATEGORY(conidx, category_id, idx_env) \
        (idx_env->p_env[conidx]->ntf_new_alert_cfg &= ~(1 << category_id))

#define ANPS_DISABLE_UNREAD_ALERT_CATEGORY(conidx, category_id, idx_env) \
        (idx_env->p_env[conidx]->ntf_unread_alert_cfg &= ~(1 << category_id))


/*
 * DEFINES
 ****************************************************************************************
 */

/// Maximal number of Alert Notification Server task instances
/// Database Configuration Flag
#define ANPS_DB_CONFIG_MASK         (0x1FFF)

/// Alert Notification Service Attributes
enum anps_ans_att_list
{
    ANS_IDX_SVC,

    ANS_IDX_SUPP_NEW_ALERT_CAT_CHAR,
    ANS_IDX_SUPP_NEW_ALERT_CAT_VAL,

    ANS_IDX_NEW_ALERT_CHAR,
    ANS_IDX_NEW_ALERT_VAL,
    ANS_IDX_NEW_ALERT_CFG,

    ANS_IDX_SUPP_UNREAD_ALERT_CAT_CHAR,
    ANS_IDX_SUPP_UNREAD_ALERT_CAT_VAL,

    ANS_IDX_UNREAD_ALERT_STATUS_CHAR,
    ANS_IDX_UNREAD_ALERT_STATUS_VAL,
    ANS_IDX_UNREAD_ALERT_STATUS_CFG,

    ANS_IDX_ALERT_NTF_CTNL_PT_CHAR,
    ANS_IDX_ALERT_NTF_CTNL_PT_VAL,

    ANS_IDX_NB,
};

/*
 * TYPES DEFINITION
 ****************************************************************************************
 */

/// Alert Notification Profile Server Connection Dependent Environment Variable
typedef struct anps_cnx_env
{
    /// transmission wait queue
    co_list_t wait_queue;
    /**
     * Category Notification Configuration
     *   Bit 0 : Simple Alert
     *   Bit 1 : Email
     *   Bit 2 : News
     *   Bit 3 : Call
     *   Bit 4 : Missed Call
     *   Bit 5 : SMS/MMS
     *   Bit 6 : Voice Mail
     *   Bit 7 : Schedule
     *   Bit 8 : High Prioritized Alert
     *   Bit 9 : Instance Message
     */
    uint16_t ntf_new_alert_cfg;
    uint16_t ntf_unread_alert_cfg;
    /**
     * Client Characteristic Configuration Status
     *   Bit 0 : New Alert Characteristic
     *   Bit 1 : Unread Alert Status Characteristic
     */
    uint8_t  ntf_cfg;
    /// True if a notification is sent to a peer device, false else.
    bool     busy;
} anps_cnx_env_t;

/// Alert Notification Server Environment Variable
typedef struct anps_env
{
    /// profile environment
    prf_hdr_t       prf_env;
    /// Environment variable pointer for each connection
    anps_cnx_env_t* p_env[BLE_CONNECTION_MAX];
    /// ANS Start Handle
    uint16_t        s_hdl;
    /// Supported New Alert Category Characteristic Value
    uint16_t        supp_new_alert_cat;
    /// Supported Unread Alert Category Characteristic Value
    uint16_t        supp_unread_alert_cat;
    /// GATT user local identifier
    uint8_t         user_lid;
} anps_env_t;


/*
 * ATTRIBUTES DATABASE
 ****************************************************************************************
 */

/// Full ANS Database Description - Used to add attributes into the database
const gatt_att16_desc_t anps_att_db[ANS_IDX_NB] =
{
    // Alert Notification Service Declaration
    [ANS_IDX_SVC]                        = {GATT_DECL_PRIMARY_SERVICE,      PROP(RD),            0                                              },
    // Supported New Alert Category Characteristic Declaration
    [ANS_IDX_SUPP_NEW_ALERT_CAT_CHAR]    = {GATT_DECL_CHARACTERISTIC,       PROP(RD),            0                                              },
    // Supported New Alert Category Characteristic Value
    [ANS_IDX_SUPP_NEW_ALERT_CAT_VAL]     = {GATT_CHAR_SUP_NEW_ALERT_CAT,    PROP(RD),            OPT(NO_OFFSET) | sizeof(anp_cat_id_bit_mask_t) },
    // New Alert Characteristic Declaration
    [ANS_IDX_NEW_ALERT_CHAR]             = {GATT_DECL_CHARACTERISTIC,       PROP(RD),            0                                              },
    // New Alert Characteristic Value
    [ANS_IDX_NEW_ALERT_VAL]              = {GATT_CHAR_NEW_ALERT,            PROP(N),             OPT(NO_OFFSET) | ANS_NEW_ALERT_MAX_LEN         },
    // New Alert Characteristic - Client Char. Configuration Descriptor
    [ANS_IDX_NEW_ALERT_CFG]              = {GATT_DESC_CLIENT_CHAR_CFG,      PROP(RD) | PROP(WR), OPT(NO_OFFSET)                                 },
    // Supported Unread Alert Category Characteristic Declaration
    [ANS_IDX_SUPP_UNREAD_ALERT_CAT_CHAR] = {GATT_DECL_CHARACTERISTIC,       PROP(RD),            0                                              },
    // Supported New Alert Category Characteristic Value
    [ANS_IDX_SUPP_UNREAD_ALERT_CAT_VAL]  = {GATT_CHAR_SUP_UNREAD_ALERT_CAT, PROP(RD),            OPT(NO_OFFSET) | sizeof(anp_cat_id_bit_mask_t) },
    // Unread Alert Status Characteristic Declaration
    [ANS_IDX_UNREAD_ALERT_STATUS_CHAR]   = {GATT_DECL_CHARACTERISTIC,       PROP(RD),            0                                              },
    // Unread Alert Status Characteristic Value
    [ANS_IDX_UNREAD_ALERT_STATUS_VAL]    = {GATT_CHAR_UNREAD_ALERT_STATUS,  PROP(N),             OPT(NO_OFFSET) | sizeof(anp_unread_alert_t)    },
    // Unread Alert Status Characteristic - Client Char. Configuration Descriptor
    [ANS_IDX_UNREAD_ALERT_STATUS_CFG]    = {GATT_DESC_CLIENT_CHAR_CFG,      PROP(RD) | PROP(WR), OPT(NO_OFFSET)                                 },
    // Alert Notification Control Point Characteristic Declaration
    [ANS_IDX_ALERT_NTF_CTNL_PT_CHAR]     = {GATT_DECL_CHARACTERISTIC,       PROP(RD),            0                                              },
    // Alert Notification Control Point Characteristic Value
    [ANS_IDX_ALERT_NTF_CTNL_PT_VAL]      = {GATT_CHAR_ALERT_NTF_CTNL_PT,    PROP(WR),            OPT(NO_OFFSET) | 2*sizeof(uint8_t)             },
};

/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Inform application that notification has been sent
 *
 * @param[in] conidx     Connection index
 * @param[in] p_anps_env Environment variable
 * @param[in] handle     Handle value where notification is sent
 * @param[in] status     Status of notification transmission
 ****************************************************************************************
 */
__STATIC void anps_ntf_cmp_evt(uint8_t conidx, anps_env_t *p_anps_env, uint16_t handle, uint16_t status)
{
    const anps_cb_t* p_cb = (const anps_cb_t*) p_anps_env->prf_env.p_cb;

    switch(handle - p_anps_env->s_hdl)
    {
        case ANS_IDX_NEW_ALERT_VAL:           { p_cb->cb_new_alert_upd_cmp(conidx, status);           } break;
        case ANS_IDX_UNREAD_ALERT_STATUS_VAL: { p_cb->cb_unread_alert_status_upd_cmp(conidx, status); } break;
        default:                              { /* Nothing to do */                                   } break;
    }
}


/**
 ****************************************************************************************
 * @brief Send notification to peer device.
 *
 * @param[in] conidx     Connection index
 * @param[in] p_anps_env Environment variable
 ****************************************************************************************
 */
__STATIC void anps_ntf_send(uint8_t conidx, anps_env_t *p_anps_env)
{
    while(!co_list_is_empty(&(p_anps_env->p_env[conidx]->wait_queue)))
    {
        co_buf_t* p_buf;
        uint16_t* p_buf_hdl;

        // get buffer to send
        p_buf = (co_buf_t*) co_list_pop_front(&(p_anps_env->p_env[conidx]->wait_queue));

        if(p_buf != NULL)
        {
            uint16_t status;
            p_buf_hdl = (uint16_t*) co_buf_metadata(p_buf);

            status = gatt_srv_event_send(conidx, p_anps_env->user_lid, *p_buf_hdl, GATT_NOTIFY, *p_buf_hdl, p_buf);
            co_buf_release(p_buf);

            if(status != GAP_ERR_NO_ERROR)
            {
                // inform that notification transmission failed
                anps_ntf_cmp_evt(conidx, p_anps_env, *p_buf_hdl, status);
            }
            else
            {
                p_anps_env->p_env[conidx]->busy = true;
                break;
            }
        }
    }
}

/**
 ****************************************************************************************
 * @brief Inform application about notification status update.
 *
 * @param[in] conidx     Connection index
 * @param[in] p_anps_env Environment variable
 * @param[in] alert_type Alert type @see enum anp_alert_codes
 ****************************************************************************************
 */
__STATIC void anps_ntf_status_upd(uint8_t conidx, anps_env_t *p_anps_env, uint8_t alert_type)
{
    const anps_cb_t* p_cb = (const anps_cb_t*) p_anps_env->prf_env.p_cb;
    uint16_t ntf_ccc_cfg = ANPS_IS_ALERT_ENABLED(conidx, p_anps_env, alert_type) ? PRF_CLI_START_NTF : PRF_CLI_STOP_NTFIND;
    uint8_t cat_id_mask_0, cat_id_mask_1;

    if (alert_type == ANP_NEW_ALERT)
    {
        cat_id_mask_0 = (uint8_t)(p_anps_env->p_env[conidx]->ntf_new_alert_cfg & 0x00FF);
        cat_id_mask_1 = (uint8_t)((p_anps_env->p_env[conidx]->ntf_new_alert_cfg & 0xFF00) >> 8);
    }
    else
    {
        cat_id_mask_0 = (uint8_t)(p_anps_env->p_env[conidx]->ntf_unread_alert_cfg & 0x00FF);
        cat_id_mask_1 = (uint8_t)(p_anps_env->p_env[conidx]->ntf_unread_alert_cfg >> 8);
    }

    p_cb->cb_ntf_status_upd(conidx, alert_type, ntf_ccc_cfg, cat_id_mask_0, cat_id_mask_1);
}

/**
 ****************************************************************************************
 * @brief Inform application about notification immediate update.
 *
 * @param[in] conidx      Connection index
 * @param[in] p_anps_env  Environment variable
 * @param[in] alert_type  Alert type @see enum anp_alert_codes
 * @param[in] category_id Category Index @see enum anp_category_id
 ****************************************************************************************
 */
__STATIC void anps_ntf_immediate_upd(uint8_t conidx, anps_env_t *p_anps_env, uint8_t alert_type, uint8_t category_id)
{
    const anps_cb_t* p_cb = (const anps_cb_t*) p_anps_env->prf_env.p_cb;
    uint16_t req_cat;

    if (alert_type == ANP_NEW_ALERT)
    {
        if (category_id == CAT_ID_ALL_SUPPORTED_CAT)
        {
            // All category that are supported and enabled shall be notified
            req_cat = p_anps_env->p_env[conidx]->ntf_new_alert_cfg;
        }
        else
        {
            req_cat = (1 << category_id);
        }
    }
    // Unread alert
    else
    {
        if (category_id == CAT_ID_ALL_SUPPORTED_CAT)
        {
            // All category that are supported and enabled shall be notified
            req_cat = p_anps_env->p_env[conidx]->ntf_unread_alert_cfg;
        }
        else
        {
            req_cat = (1 << category_id);
        }
    }

    p_cb->cb_ntf_immediate_req(conidx, alert_type, (uint8_t)(req_cat & 0x00FF), (uint8_t)((req_cat & 0xFF00) >> 8));
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
__STATIC void anps_cb_att_read_get(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset,
                                   uint16_t max_length)
{
    anps_env_t *p_anps_env = PRF_ENV_GET(ANPS, anps);
    uint16_t  status       = GAP_ERR_NO_ERROR;
    uint16_t  value        = 0;
    co_buf_t* p_buf        = NULL;

    if(p_anps_env != NULL)
    {
        switch(hdl - p_anps_env->s_hdl)
        {
            case ANS_IDX_SUPP_NEW_ALERT_CAT_VAL:    { value = p_anps_env->supp_new_alert_cat;                                        } break;
            case ANS_IDX_NEW_ALERT_CFG:             { value = co_htobs(ANPS_IS_ALERT_ENABLED(conidx, p_anps_env, ANP_NEW_ALERT));    } break;
            case ANS_IDX_SUPP_UNREAD_ALERT_CAT_VAL: { value = p_anps_env->supp_unread_alert_cat;                                     } break;
            case ANS_IDX_UNREAD_ALERT_STATUS_CFG:   { value = co_htobs(ANPS_IS_ALERT_ENABLED(conidx, p_anps_env, ANP_UNREAD_ALERT)); } break;
            default:                                { status = ATT_ERR_REQUEST_NOT_SUPPORTED;                                        } break;
        }

        if(status == GAP_ERR_NO_ERROR)
        {
            if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, sizeof(uint16_t), GATT_BUFFER_TAIL_LEN) == GAP_ERR_NO_ERROR)
            {
                co_write16p(co_buf_data(p_buf), value);
            }
            else
            {
                status = ATT_ERR_INSUFF_RESOURCE;
            }
        }
    }
    else
    {
        status = PRF_APP_ERROR;
    }

    // Immediately send confirmation message
    gatt_srv_att_read_get_cfm(conidx, user_lid, token, status, sizeof(uint16_t), p_buf);
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
 ****************************************************************************************
 */
__STATIC void anps_cb_att_val_set(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset,
                                  co_buf_t* p_data)
{
    anps_env_t *p_anps_env = PRF_ENV_GET(ANPS, anps);
    uint16_t  status       = PRF_APP_ERROR;

    if(p_anps_env != NULL)
    {
        uint16_t att_idx = hdl - p_anps_env->s_hdl;

        /*
         * ---------------------------------------------------------------------------------------------
         * New Alert Client Characteristic Configuration Descriptor Value - Write
         * ---------------------------------------------------------------------------------------------
         * Unread Status Alert Client Characteristic Configuration Descriptor Value - Write
         * ---------------------------------------------------------------------------------------------
         */
        switch(att_idx)
        {
            case ANS_IDX_NEW_ALERT_CFG:
            case ANS_IDX_UNREAD_ALERT_STATUS_CFG:
            {
                uint16_t ntf_cfg = co_btohs(co_read16p(co_buf_data(p_data)));

                // check value and length
                if((co_buf_data_len(p_data) != sizeof(uint16_t)) || (ntf_cfg > PRF_CLI_START_NTF))  break;

                // Alert type
                uint8_t alert_type = ((att_idx == ANS_IDX_NEW_ALERT_CFG) ? ANP_NEW_ALERT : ANP_UNREAD_ALERT);

                // Update the status in the environment
                if (ntf_cfg == PRF_CLI_START_NTF)
                {
                    ANPS_ENABLE_ALERT(conidx, p_anps_env, alert_type);
                }
                else
                {
                    ANPS_DISABLE_ALERT(conidx, p_anps_env, alert_type);
                }

                // Inform the HL that the notification configuration status has been written
                anps_ntf_status_upd(conidx, p_anps_env, alert_type);

                // Enable bond Data
                ANPS_ENABLE_ALERT(conidx, p_anps_env, ANPS_FLAG_CFG_PERFORMED_OK);

                status = GAP_ERR_NO_ERROR;
            } break;
            case ANS_IDX_ALERT_NTF_CTNL_PT_VAL:
            {
                uint8_t cmd_id;
                uint8_t cat_id;

                // check value
                if((co_buf_data_len(p_data) < 2))  break;

                cmd_id = co_buf_data(p_data)[0];
                co_buf_head_release(p_data, 1);
                cat_id = co_buf_data(p_data)[0];

                // Check the command ID value
                if (cmd_id >= CMD_ID_NB)
                {
                    status = ANP_CMD_NOT_SUPPORTED;
                    break;
                }

                // Check the category ID value
                if ((cat_id >= CAT_ID_NB) && (cat_id != CAT_ID_ALL_SUPPORTED_CAT))
                {
                    status = ANP_CAT_NOT_SUPPORTED;
                    break;
                }

                if (cat_id < CAT_ID_NB)
                {
                    // New Alert
                    if ((cmd_id & 0x1) == 0)
                    {
                        // Check if the category is supported
                        if (!ANPS_IS_NEW_ALERT_CATEGORY_SUPPORTED(cat_id))
                        {
                            status = ANP_CAT_NOT_SUPPORTED;
                            break;
                        }
                    }
                    // Unread Alert Status
                    else
                    {
                        // Check if the category is supported
                        if (!ANPS_IS_UNREAD_ALERT_CATEGORY_SUPPORTED(cat_id))
                        {
                            status = ANP_CAT_NOT_SUPPORTED;
                            break;
                        }
                    }
                }

                // React according to the received command id value
                switch (cmd_id)
                {
                    // Enable New Alert Notification
                    case (CMD_ID_EN_NEW_IN_ALERT_NTF):
                    {
                        if (cat_id != CAT_ID_ALL_SUPPORTED_CAT)
                        {
                            // Enable sending of new alert notification for the specified category
                            ANPS_ENABLE_NEW_ALERT_CATEGORY(conidx, cat_id, p_anps_env);
                        }
                        else
                        {
                            // Enable sending of new alert notification for all supported category
                            p_anps_env->p_env[conidx]->ntf_new_alert_cfg |= p_anps_env->supp_new_alert_cat;
                        }

                        anps_ntf_status_upd(conidx, p_anps_env, ANP_NEW_ALERT);
                    } break;

                    // Enable Unread Alert Status Notification
                    case (CMD_ID_EN_UNREAD_CAT_STATUS_NTF):
                    {
                        if (cat_id != CAT_ID_ALL_SUPPORTED_CAT)
                        {
                            // Enable sending of unread alert notification for the specified category
                            ANPS_ENABLE_UNREAD_ALERT_CATEGORY(conidx, cat_id, p_anps_env);
                        }
                        else
                        {
                            // Enable sending of unread alert notification for all supported category
                            p_anps_env->p_env[conidx]->ntf_unread_alert_cfg |= p_anps_env->supp_unread_alert_cat;
                        }

                        anps_ntf_status_upd(conidx, p_anps_env, ANP_UNREAD_ALERT);
                    } break;

                    // Disable New Alert Notification
                    case (CMD_ID_DIS_NEW_IN_ALERT_NTF):
                    {
                        if (cat_id != CAT_ID_ALL_SUPPORTED_CAT)
                        {
                            // Disable sending of new alert notification for the specified category
                            ANPS_DISABLE_NEW_ALERT_CATEGORY(conidx, cat_id, p_anps_env);
                        }
                        else
                        {
                            // Disable sending of new alert notification for all supported category
                            p_anps_env->p_env[conidx]->ntf_new_alert_cfg &= ~p_anps_env->supp_new_alert_cat;
                        }

                        anps_ntf_status_upd(conidx, p_anps_env, ANP_NEW_ALERT);
                    } break;

                    // Disable Unread Alert Status Notification
                    case (CMD_ID_DIS_UNREAD_CAT_STATUS_NTF):
                    {
                        if (cat_id != CAT_ID_ALL_SUPPORTED_CAT)
                        {
                            // Disable sending of unread alert notification for the specified category
                            ANPS_DISABLE_UNREAD_ALERT_CATEGORY(conidx, cat_id, p_anps_env);
                        }
                        else
                        {
                            // Enable sending of unread alert notification for all supported category
                            p_anps_env->p_env[conidx]->ntf_unread_alert_cfg &= ~p_anps_env->supp_unread_alert_cat;
                        }

                        anps_ntf_status_upd(conidx, p_anps_env, ANP_UNREAD_ALERT);
                    } break;

                    // Notify New Alert immediately
                    case (CMD_ID_NTF_NEW_IN_ALERT_IMM):
                    {
                        // Check if sending of notification is enabled
                        if (ANPS_IS_ALERT_ENABLED(conidx, p_anps_env, ANP_NEW_ALERT))
                        {
                            if (cat_id == CAT_ID_ALL_SUPPORTED_CAT)
                            {
                                // Check if at least one category can be notified
                                if (p_anps_env->p_env[conidx]->ntf_new_alert_cfg != 0)
                                {
                                    anps_ntf_immediate_upd(conidx, p_anps_env, ANP_NEW_ALERT, CAT_ID_ALL_SUPPORTED_CAT);
                                }
                            }
                            else
                            {
                                // Check if sending of notifications has been enabled for the specified category.
                                if (ANPS_IS_NEW_ALERT_CATEGORY_ENABLED(conidx, cat_id, p_anps_env))
                                {
                                    anps_ntf_immediate_upd(conidx, p_anps_env, ANP_NEW_ALERT, cat_id);
                                }
                            }
                        }
                    } break;

                    // Notify Unread Alert Status immediately
                    case (CMD_ID_NTF_UNREAD_CAT_STATUS_IMM):
                    {
                        // Check if sending of notification is enabled
                        if (ANPS_IS_ALERT_ENABLED(conidx, p_anps_env, ANP_UNREAD_ALERT))
                        {
                            if (cat_id == CAT_ID_ALL_SUPPORTED_CAT)
                            {
                                // Check if at least one category can be notified
                                if (p_anps_env->p_env[conidx]->ntf_unread_alert_cfg != 0)
                                {
                                    anps_ntf_immediate_upd(conidx, p_anps_env, ANP_UNREAD_ALERT, CAT_ID_ALL_SUPPORTED_CAT);
                                }
                            }
                            else
                            {
                                // Check if sending of notifications has been enabled for the specified category.
                                if (ANPS_IS_UNREAD_ALERT_CATEGORY_ENABLED(conidx, cat_id, p_anps_env))
                                {
                                    anps_ntf_immediate_upd(conidx, p_anps_env, ANP_UNREAD_ALERT, cat_id);
                                }
                            }
                        }
                    } break;
                    default: { /* Nothing to do */ } break;
                }

                status = GAP_ERR_NO_ERROR;
            } break;
            default: { /* Nothing to do */ } break;
        }
    }

    // Immediately send confirmation message
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
__STATIC void anps_cb_event_sent(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    anps_env_t *p_anps_env = PRF_ENV_GET(ANPS, anps);
    if(p_anps_env != NULL)
    {
        anps_ntf_cmp_evt(conidx, p_anps_env, dummy, status);
        p_anps_env->p_env[conidx]->busy = false;
        anps_ntf_send(conidx, p_anps_env);
    }
}


/// Service callback hander
__STATIC const gatt_srv_cb_t anps_cb =
{
        .cb_event_sent    = anps_cb_event_sent,
        .cb_att_read_get  = anps_cb_att_read_get,
        .cb_att_val_set   = anps_cb_att_val_set,
        .cb_att_event_get = NULL,
        .cb_att_info_get  = NULL,
};

/*
 * PROFILE NATIVE HANDLERS
 ****************************************************************************************
 */

uint16_t anps_enable(uint8_t conidx, uint16_t new_alert_ntf_cfg, uint16_t unread_alert_status_ntf_cfg)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    anps_env_t *p_anps_env = PRF_ENV_GET(ANPS, anps);

    if(p_anps_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_anps_env->p_env[conidx] != NULL))
        {
            // Bonded data was not used before
            if (!ANPS_IS_ALERT_ENABLED(conidx, p_anps_env, ANPS_FLAG_CFG_PERFORMED_OK))
            {
                status = GAP_ERR_NO_ERROR;
                // Update the state in the environment
                if (new_alert_ntf_cfg != PRF_CLI_STOP_NTFIND)
                {
                    // Force to PRF_CLI_START_NTF
                    new_alert_ntf_cfg = PRF_CLI_START_NTF;

                    ANPS_ENABLE_ALERT(conidx, p_anps_env, ANP_NEW_ALERT);
                }

                if (unread_alert_status_ntf_cfg != PRF_CLI_STOP_NTFIND)
                {
                    // Force to PRF_CLI_START_NTF
                    unread_alert_status_ntf_cfg = PRF_CLI_START_NTF;

                    ANPS_ENABLE_ALERT(conidx, p_anps_env, ANP_UNREAD_ALERT);
                }
            }
            // Enable Bonded Data
            ANPS_ENABLE_ALERT(conidx, p_anps_env, ANPS_FLAG_CFG_PERFORMED_OK);
        }
    }

    return (status);
}

uint16_t anps_new_alert_upd(uint8_t conidx, const anp_new_alert_t* p_new_alert)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    anps_env_t *p_anps_env = PRF_ENV_GET(ANPS, anps);

    if(p_anps_env != NULL)
    {
        uint8_t cat_id        = p_new_alert->cat_id;
        bool    cat_supported = ANPS_IS_NEW_ALERT_CATEGORY_SUPPORTED(cat_id);

        // Check the length of the string info value and the category ID
        if ((p_new_alert->info_str_len > ANS_NEW_ALERT_STRING_INFO_MAX_LEN) || (cat_id >= CAT_ID_NB) || (!cat_supported))
        {
            status = PRF_ERR_INVALID_PARAM;
        }
        else if ((conidx < BLE_CONNECTION_MAX) && (p_anps_env->p_env[conidx] != NULL))
        {
            // Check if sending of notification is enabled for the provided category
            if (   ANPS_IS_NEW_ALERT_CATEGORY_ENABLED(conidx, cat_id, p_anps_env)
                && ANPS_IS_ALERT_ENABLED(conidx, p_anps_env, ANP_NEW_ALERT))
            {
                co_buf_t* p_buf = NULL;
                status = co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, 0, ANS_NEW_ALERT_STRING_INFO_MAX_LEN + GATT_BUFFER_TAIL_LEN);

                if(status == CO_BUF_ERR_NO_ERROR)
                {
                    uint16_t* p_buf_hdl = (uint16_t*) co_buf_metadata(p_buf);
                    *p_buf_hdl          = p_anps_env->s_hdl + ANS_IDX_NEW_ALERT_VAL;

                    co_buf_tail(p_buf)[0] = p_new_alert->cat_id;
                    co_buf_tail_reserve(p_buf, 1);
                    co_buf_tail(p_buf)[0] = p_new_alert->nb_new_alert;
                    co_buf_tail_reserve(p_buf, 1);
                    memcpy(co_buf_tail(p_buf), p_new_alert->str_info, p_new_alert->info_str_len);
                    co_buf_tail_reserve(p_buf, p_new_alert->info_str_len);

                    // put event on wait queue
                    co_list_push_back(&(p_anps_env->p_env[conidx]->wait_queue), &(p_buf->hdr));

                    // try to send notification
                    if(!p_anps_env->p_env[conidx]->busy)
                    {
                        anps_ntf_send(conidx, p_anps_env);
                    }
                }
                else
                {
                    status = GAP_ERR_INSUFF_RESOURCES;
                }

            }
            else
            {
                status = PRF_ERR_NTF_DISABLED;
            }
        }
    }
    return (status);
}

uint16_t anps_unread_alert_status_upd(uint8_t conidx, const anp_unread_alert_t* p_unread_alert_status)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    anps_env_t *p_anps_env = PRF_ENV_GET(ANPS, anps);

    if(p_anps_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_anps_env->p_env[conidx] != NULL))
        {
            uint8_t cat_id        = p_unread_alert_status->cat_id;
            bool    cat_supported = ANPS_IS_NEW_ALERT_CATEGORY_SUPPORTED(cat_id);

            // The category ID
            if ((cat_id >= CAT_ID_NB) || (!cat_supported))
            {
                status = PRF_ERR_INVALID_PARAM;
            }
            else if ((conidx < BLE_CONNECTION_MAX) && (p_anps_env->p_env[conidx] != NULL))
            {
                // Check if sending of notification is enabled for the provided category
                if (   ANPS_IS_UNREAD_ALERT_CATEGORY_ENABLED(conidx, cat_id, p_anps_env)
                    && ANPS_IS_ALERT_ENABLED(conidx, p_anps_env, ANP_UNREAD_ALERT))
                {
                    co_buf_t* p_buf = NULL;
                    status = co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, 0, sizeof(anp_unread_alert_t) + GATT_BUFFER_TAIL_LEN);

                    if(status == CO_BUF_ERR_NO_ERROR)
                    {
                        uint16_t* p_buf_hdl = (uint16_t*) co_buf_metadata(p_buf);
                        *p_buf_hdl          = p_anps_env->s_hdl + ANS_IDX_UNREAD_ALERT_STATUS_VAL;

                        co_buf_tail(p_buf)[0] = p_unread_alert_status->cat_id;
                        co_buf_tail_reserve(p_buf, 1);
                        co_buf_tail(p_buf)[0] = p_unread_alert_status->nb_unread_alert;
                        co_buf_tail_reserve(p_buf, 1);

                        // put event on wait queue
                        co_list_push_back(&(p_anps_env->p_env[conidx]->wait_queue), &(p_buf->hdr));

                        // try to send notification
                        if(!p_anps_env->p_env[conidx]->busy)
                        {
                            anps_ntf_send(conidx, p_anps_env);
                        }
                    }
                    else
                    {
                        status = GAP_ERR_INSUFF_RESOURCES;
                    }
                }
                else
                {
                    status = PRF_ERR_NTF_DISABLED;
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
 * @brief Handles reception of the @ref ANPS_ENABLE_REQ message.
 * @param[in] msgid Id of the message received.
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int anps_enable_req_handler(ke_msg_id_t const msgid, struct anps_enable_req *p_param,
                                     ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    struct anps_enable_rsp *p_rsp;
    // Request status
    uint16_t status = anps_enable(p_param->conidx, p_param->new_alert_ntf_cfg, p_param->unread_alert_status_ntf_cfg);

    p_rsp         = KE_MSG_ALLOC(ANPS_ENABLE_RSP, src_id, dest_id, anps_enable_rsp);
    if(p_rsp)
    {
        p_rsp->conidx  = p_param->conidx;
        p_rsp->status  = status;
        ke_msg_send(p_rsp);
    }

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief Handles reception of the @ref ANPS_NTF_ALERT_CMD message.
 * @param[in] msgid Id of the message received.
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int anps_ntf_alert_cmd_handler(ke_msg_id_t const msgid, struct anps_ntf_alert_cmd const *p_param,
                                        ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status;
    switch(p_param->operation)
    {
        case ANPS_UPD_NEW_ALERT_OP_CODE:           { status = anps_new_alert_upd(p_param->conidx, &(p_param->value.new_alert));                     } break;
        case ANPS_UPD_UNREAD_ALERT_STATUS_OP_CODE: { status = anps_unread_alert_status_upd(p_param->conidx, &(p_param->value.unread_alert_status)); } break;
        default:                                   { status = PRF_ERR_INVALID_PARAM;                                                                 } break;
    }

    if(status != GAP_ERR_NO_ERROR)
    {
        struct anps_cmp_evt* p_cmp_evt = KE_MSG_ALLOC(ANPS_CMP_EVT, src_id, dest_id ,anps_cmp_evt);
        if(p_cmp_evt != NULL)
        {
            p_cmp_evt->conidx    = p_param->conidx;
            p_cmp_evt->operation = p_param->operation;
            p_cmp_evt->status    = status;
            ke_msg_send(p_cmp_evt);
        }
    }

    return (KE_MSG_CONSUMED);
}


/// Specifies the default message handlers
KE_MSG_HANDLER_TAB(anps)
{
    // Note: all messages must be sorted in ID ascending order

    {ANPS_ENABLE_REQ,               (ke_msg_func_t)anps_enable_req_handler},
    {ANPS_NTF_ALERT_CMD,            (ke_msg_func_t)anps_ntf_alert_cmd_handler},
};

/**
 ****************************************************************************************
 * @brief Completion of New Alert information Update procedure
 *
 * @param[in] conidx Connection index
 * @param[in] status Status of the procedure execution (@see enum hl_err)
 ****************************************************************************************
 */
__STATIC void anps_cb_new_alert_upd_cmp(uint8_t conidx, uint16_t status)

{
    struct anps_cmp_evt* p_evt = KE_MSG_ALLOC(ANPS_CMP_EVT, PRF_DST_TASK(ANPS), PRF_SRC_TASK(ANPS), anps_cmp_evt);

    if(p_evt != NULL)
    {
        p_evt->conidx    = conidx;
        p_evt->operation = ANPS_UPD_NEW_ALERT_OP_CODE;
        p_evt->status    = status;
        ke_msg_send(p_evt);
    }
}

/**
 ****************************************************************************************
 * @brief Completion of Unread Alert status Update procedure
 *
 * @param[in] conidx Connection index
 * @param[in] status Status of the procedure execution (@see enum hl_err)
 ****************************************************************************************
 */
__STATIC void anps_cb_unread_alert_status_upd_cmp(uint8_t conidx, uint16_t status)
{
    struct anps_cmp_evt*p_evt = KE_MSG_ALLOC(ANPS_CMP_EVT, PRF_DST_TASK(ANPS), PRF_SRC_TASK(ANPS), anps_cmp_evt);

    if(p_evt != NULL)
    {
        p_evt->conidx    = conidx;
        p_evt->operation = ANPS_UPD_UNREAD_ALERT_STATUS_OP_CODE;
        p_evt->status    = status;
        ke_msg_send(p_evt);
    }
}

/**
 ****************************************************************************************
 * @brief The peer device requests to be notified about new alert values
 *
 * @param[in] conidx        Connection index
 * @param[in] alert_type    Alert type (New Alert or Unread Alert Status)
 * @param[in] cat_id_mask_0 Category ID Bit Mask 0
 * @param[in] cat_id_mask_1 Category ID Bit Mask 1
 ****************************************************************************************
 */
__STATIC void anps_cb_ntf_immediate_req(uint8_t conidx, uint8_t alert_type, uint8_t cat_id_mask_0, uint8_t cat_id_mask_1)
{
    struct anps_ntf_immediate_req_ind *p_ind =
            KE_MSG_ALLOC(ANPS_NTF_IMMEDIATE_REQ_IND, PRF_DST_TASK(ANPS), PRF_SRC_TASK(ANPS), anps_ntf_immediate_req_ind);

    if(p_ind != NULL)
    {
        p_ind->conidx                    = conidx;
        p_ind->alert_type                = alert_type;
        p_ind->cat_ntf_cfg.cat_id_mask_0 = cat_id_mask_0;
        p_ind->cat_ntf_cfg.cat_id_mask_1 = cat_id_mask_1;
        ke_msg_send(p_ind);
    }

}

/**
 ****************************************************************************************
 * @brief  Indicate that the notification configuration has been modified by the peer device
 *         (must be considered as bond data)
 *
 * @param[in] conidx        Connection index
 * @param[in] alert_type    Alert type (New Alert or Unread Alert Status)
 * @param[in] ntf_ind_cfg   Client Characteristic Configuration Descriptor Status
 * @param[in] cat_id_mask_0 Category ID Bit Mask 0
 * @param[in] cat_id_mask_1 Category ID Bit Mask 1
 ****************************************************************************************
 */
__STATIC void anps_cb_ntf_status_upd(uint8_t conidx, uint8_t alert_type, uint16_t ntf_ccc_cfg,
                                     uint8_t cat_id_mask_0, uint8_t cat_id_mask_1)
{
    struct anps_ntf_status_update_ind *p_ind =
            KE_MSG_ALLOC(ANPS_NTF_STATUS_UPDATE_IND, PRF_DST_TASK(ANPS), PRF_SRC_TASK(ANPS), anps_ntf_status_update_ind);

    if(p_ind != NULL)
    {
        p_ind->conidx                    = conidx;
        p_ind->alert_type                = alert_type;
        p_ind->ntf_ccc_cfg               = ntf_ccc_cfg;
        p_ind->cat_ntf_cfg.cat_id_mask_0 = cat_id_mask_0;
        p_ind->cat_ntf_cfg.cat_id_mask_1 = cat_id_mask_1;
        ke_msg_send(p_ind);
    }
}

/// Default Message handle
__STATIC const anps_cb_t anps_msg_cb =
{
        .cb_new_alert_upd_cmp            = anps_cb_new_alert_upd_cmp,
        .cb_unread_alert_status_upd_cmp  = anps_cb_unread_alert_status_upd_cmp,
        .cb_ntf_immediate_req            = anps_cb_ntf_immediate_req,
        .cb_ntf_status_upd               = anps_cb_ntf_status_upd,
};
#endif // (BLE_HL_MSG_API)

/*
 * PROFILE DEFAULT HANDLERS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Initialization of the ANSP module.
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
__STATIC uint16_t anps_init(prf_data_t *p_env, uint16_t *p_start_hdl,  uint8_t sec_lvl, uint8_t user_prio,
                          struct anps_db_cfg *p_params, const anps_cb_t* p_cb)
{
    //------------------ create the attribute database for the profile -------------------

    // DB Creation Status
    uint16_t status = GAP_ERR_NO_ERROR;
    uint8_t user_lid = GATT_INVALID_USER_LID;

    do
    {
        anps_env_t* p_anps_env;
        // Service content flag
        uint32_t cfg_flag = ANPS_DB_CONFIG_MASK;

        #if (BLE_HL_MSG_API)
        if(p_cb == NULL)
        {
            p_cb = &(anps_msg_cb);
        }
        #endif // (BLE_HL_MSG_API)

        if(   (p_params == NULL) || (p_start_hdl == NULL) || (p_cb == NULL) || (p_cb->cb_new_alert_upd_cmp == NULL)
           || (p_cb->cb_unread_alert_status_upd_cmp == NULL) || (p_cb->cb_ntf_immediate_req == NULL)
           || (p_cb->cb_ntf_status_upd == NULL))
        {
            status = GAP_ERR_INVALID_PARAM;
            break;
        }

        // register user
        status = gatt_user_srv_register(L2CAP_LE_MTU_MIN, user_prio, &anps_cb, &user_lid);
        if(status != GAP_ERR_NO_ERROR) break;

        // Add Service
        status = gatt_db_svc16_add(user_lid, sec_lvl, GATT_SVC_ALERT_NTF, ANS_IDX_NB,
                                   (uint8_t *)&cfg_flag, &(anps_att_db[0]), ANS_IDX_NB, p_start_hdl);
        if(status != GAP_ERR_NO_ERROR) break;

        //-------------------- allocate memory required for the profile  ---------------------
        p_anps_env = (anps_env_t *) ke_malloc(sizeof(anps_env_t), KE_MEM_ATT_DB);

        if(p_anps_env != NULL)
        {
            // allocate ANPS required environment variable
            p_env->p_env = (prf_hdr_t *) p_anps_env;
            p_anps_env->s_hdl            = *p_start_hdl;
            p_anps_env->user_lid         = user_lid;
            memset(p_anps_env->p_env, 0, sizeof(p_anps_env->p_env));
            memcpy(&p_anps_env->supp_new_alert_cat,    &p_params->supp_new_alert_cat,    sizeof(anp_cat_id_bit_mask_t));
            memcpy(&p_anps_env->supp_unread_alert_cat, &p_params->supp_unread_alert_cat, sizeof(anp_cat_id_bit_mask_t));

            // initialize profile environment variable
            p_anps_env->prf_env.p_cb     = p_cb;
            #if (BLE_HL_MSG_API)
            p_env->desc.msg_handler_tab  = anps_msg_handler_tab;
            p_env->desc.msg_cnt          = ARRAY_LEN(anps_msg_handler_tab);
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
__STATIC uint16_t anps_destroy(prf_data_t *p_env, uint8_t reason)
{
    uint16_t status = GAP_ERR_NO_ERROR;
    anps_env_t *p_anps_env = (anps_env_t *) p_env->p_env;

    if(reason != PRF_DESTROY_RESET)
    {
        status = gatt_user_unregister(p_anps_env->user_lid);
    }

    if(status == GAP_ERR_NO_ERROR)
    {
        uint8_t idx;
        // cleanup environment variable for each task instances
        for (idx = 0; idx < BLE_CONNECTION_MAX ; idx++)
        {
            if (p_anps_env->p_env[idx] != NULL)
            {
                if(reason != PRF_DESTROY_RESET)
                {
                    while(co_list_is_empty(&(p_anps_env->p_env[idx]->wait_queue)))
                    {
                        co_buf_t* p_buf = (co_buf_t*) co_list_pop_front(&(p_anps_env->p_env[idx]->wait_queue));
                        co_buf_release(p_buf);
                    }
                }

                ke_free(p_anps_env->p_env[idx]);
            }
        }

        // free profile environment variables
        p_env->p_env = NULL;
        ke_free(p_anps_env);
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
__STATIC void anps_con_create(prf_data_t *p_env, uint8_t conidx, const gap_con_param_t* p_con_param)
{
    anps_env_t *p_anps_env = (anps_env_t *) p_env->p_env;

    p_anps_env->p_env[conidx] = (anps_cnx_env_t *) ke_malloc(sizeof(anps_cnx_env_t), KE_MEM_ATT_DB);

    if(p_anps_env->p_env[conidx] != NULL)
    {
        memset(p_anps_env->p_env[conidx], 0, sizeof(anps_cnx_env_t));
    }
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
__STATIC void anps_con_cleanup(prf_data_t *p_env, uint8_t conidx, uint16_t reason)
{
    anps_env_t* p_anps_env = (anps_env_t*) p_env->p_env;

    // clean-up environment variable allocated for task instance
    if (p_anps_env->p_env[conidx] != NULL)
    {
        ke_free(p_anps_env->p_env[conidx]);
        p_anps_env->p_env[conidx] = NULL;
    }
}



/// ANPS Task interface required by profile manager
const prf_task_cbs_t anps_itf =
{
    .cb_init          = (prf_init_cb) anps_init,
    .cb_destroy       = anps_destroy,
    .cb_con_create    = anps_con_create,
    .cb_con_cleanup   = anps_con_cleanup,
    .cb_con_upd       = NULL,
};

/**
 ****************************************************************************************
 * @brief Retrieve service profile interface
 *
 * @return service profile interface
 ****************************************************************************************
 */
const prf_task_cbs_t *anps_prf_itf_get(void)
{
    return &anps_itf;
}

#endif //(BLE_AN_SERVER)
/// @} ANPS

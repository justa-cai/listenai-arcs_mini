/**
 ****************************************************************************************
 *
 * @file tips.c
 *
 * @brief Time Profile Server implementation.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup TIPS
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "btip_config.h"

#if (BLE_TIP_SERVER)

#include "tips.h"
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

/// CTS Configuration Flag Masks
#define TIPS_CTS_CURRENT_TIME_MASK         (0x0F)
#define TIPS_CTS_LOC_TIME_INFO_MASK        (0x30)
#define TIPS_CTS_REF_TIME_INFO_MASK        (0xC0)

/// Packed Values Length
#define CTS_CURRENT_TIME_VAL_LEN           (10)
#define NDCS_TIME_DST_VAL_LEN              (8)


/// Database Configuration - Bit Field Flags
enum tips_db_config_bf
{
    /// Current Time Service Local Time Information char. support bit
    TIPS_CTS_LOC_TIME_INFO_SUP_POS = 0,
    TIPS_CTS_LOC_TIME_INFO_SUP_BIT = CO_BIT(TIPS_CTS_LOC_TIME_INFO_SUP_POS),

    /// Current Time Service Reference Time Information char. support bit
    TIPS_CTS_REF_TIME_INFO_SUP_POS = 1,
    TIPS_CTS_REF_TIME_INFO_SUP_BIT = CO_BIT(TIPS_CTS_REF_TIME_INFO_SUP_POS),

    /// Next DST Change Service support bit
    TIPS_NDCS_SUP_POS = 2,
    TIPS_NDCS_SUP_BIT = CO_BIT(TIPS_NDCS_SUP_POS),

    /// Reference Time Update Service support bit
    TIPS_RTUS_SUP_POS = 3,
    TIPS_RTUS_SUP_BIT = CO_BIT(TIPS_RTUS_SUP_POS),

    /// Current Time Service Current Time Configuration support bit
    TIPS_CTS_CURRENT_TIME_CFG_POS = 4,
    TIPS_CTS_CURRENT_TIME_CFG_BIT = CO_BIT(TIPS_CTS_CURRENT_TIME_CFG_POS),
};

/// Database Attributes code
enum
{
    CTS_IDX_SVC,
    CTS_IDX_CURRENT_TIME_CHAR,
    CTS_IDX_CURRENT_TIME_VAL,
    CTS_IDX_CURRENT_TIME_CFG,
    CTS_IDX_LOCAL_TIME_INFO_CHAR,
    CTS_IDX_LOCAL_TIME_INFO_VAL,
    CTS_IDX_REF_TIME_INFO_CHAR,
    CTS_IDX_REF_TIME_INFO_VAL,


    NDCS_IDX_SVC,
    NDCS_IDX_TIME_DST_CHAR,
    NDCS_IDX_TIME_DST_VAL,

    RTUS_IDX_SVC,
    RTUS_IDX_TIME_UPD_CTNL_PT_CHAR,
    RTUS_IDX_TIME_UPD_CTNL_PT_VAL,
    RTUS_IDX_TIME_UPD_STATE_CHAR,
    RTUS_IDX_TIME_UPD_STATE_VAL,

    TIPS_IDX_DB_MAX,

    CTS_IDX_NB  = NDCS_IDX_SVC,
    NDCS_IDX_NB = RTUS_IDX_SVC - NDCS_IDX_SVC,
    RTUS_IDX_NB = TIPS_IDX_DB_MAX - RTUS_IDX_SVC,
};


/// Content of TIPS token
enum tips_token_bf
{
    /// GATT procedure token
    TIPS_TOKEN_GATT_TOKEN_MASK = 0x0000FFFF,
    TIPS_TOKEN_GATT_TOKEN_LSB  = 0,
    /// Connection index
    TIPS_TOKEN_CONIDX_MASK     = 0x00FF0000,
    TIPS_TOKEN_CONIDX_LSB      = 16,
    /// Value identifier
    TIPS_TOKEN_VAL_ID_MASK     = 0xFF000000,
    TIPS_TOKEN_VAL_ID_LSB      = 24,
};


/*
 * TYPES DEFINITION
 ****************************************************************************************
 */

/// ongoing operation information
typedef struct tips_buf_meta
{
    /// Connection index targeted
    uint8_t   conidx;
    /// notification targeted handle
    uint16_t  hdl;
} tips_buf_meta_t;


/// TIPS Server Environment Variable
typedef struct tips_env
{
    /// profile environment
    prf_hdr_t       prf_env;
    /// Operation Event TX wait queue
    co_list_t       wait_queue;
    /// Services Start Handle
    uint16_t        start_hdl;
    /// Database configuration
    uint8_t         features;
    /// Time update state for the Ctl Pt
    uint8_t         time_upd_state;
    /// GATT user local identifier
    uint8_t         user_lid;
    /// Operation On-going
    bool            op_ongoing;
    /// Prevent recursion in execute_operation function
    bool            in_exe_op;
    /// Notification configuration
    uint8_t         ntf_cfg[BLE_CONNECTION_MAX];

} tips_env_t;


/*
 * ATTRIBUTES DATABASE
 ****************************************************************************************
 */

/// Full Databases Description - Used to add attributes into the database
const gatt_att16_desc_t tips_att_db[TIPS_IDX_DB_MAX] =
{
    /*---------------------------------------------------*
     * Current Time Service
     *---------------------------------------------------*/

    // Current Time Service Declaration
    [CTS_IDX_SVC]                    = { GATT_DECL_PRIMARY_SERVICE,       PROP(RD),            0                               },

    // Current Time Characteristic Declaration
    [CTS_IDX_CURRENT_TIME_CHAR]      = { GATT_DECL_CHARACTERISTIC,        PROP(RD),            0                               },
    // Current Time Characteristic Value
    [CTS_IDX_CURRENT_TIME_VAL]       = { GATT_CHAR_CT_TIME,               PROP(RD) | PROP(N),  OPT(NO_OFFSET)                  },
    // Current Time Characteristic - Client Char. Configuration Descriptor
    [CTS_IDX_CURRENT_TIME_CFG]       = { GATT_DESC_CLIENT_CHAR_CFG,       PROP(RD) | PROP(WR), OPT(NO_OFFSET)                  },

    // Local Time Information Characteristic Declaration
    [CTS_IDX_LOCAL_TIME_INFO_CHAR]   = { GATT_DECL_CHARACTERISTIC,        PROP(RD),            0                               },
    // Local Time Information Characteristic Value
    [CTS_IDX_LOCAL_TIME_INFO_VAL]    = { GATT_CHAR_LOCAL_TIME_INFO,       PROP(RD),            OPT(NO_OFFSET)                  },

    // Reference Time Information Characteristic Declaration
    [CTS_IDX_REF_TIME_INFO_CHAR]     = { GATT_DECL_CHARACTERISTIC,        PROP(RD),            0                               },
    // Reference Time Info Characteristic Value
    [CTS_IDX_REF_TIME_INFO_VAL]      = { GATT_CHAR_REFERENCE_TIME_INFO,   PROP(RD),            OPT(NO_OFFSET)                  },


    /*---------------------------------------------------*
     * Next DST Change Service
     *---------------------------------------------------*/

    // Next DST Change Service Declaration
    [NDCS_IDX_SVC]                   = {GATT_DECL_PRIMARY_SERVICE,        PROP(RD),            0                               },

    // Time with DST Characteristic Declaration
    [NDCS_IDX_TIME_DST_CHAR]         = {GATT_DECL_CHARACTERISTIC,         PROP(RD),            0                               },
    // Time With DST Characteristic Value
    [NDCS_IDX_TIME_DST_VAL]          = {GATT_CHAR_TIME_WITH_DST,          PROP(RD),            OPT(NO_OFFSET)                  },


    /*---------------------------------------------------*
     * Reference Time Update Service Creation
     *---------------------------------------------------*/

    // Reference Time Information Service Declaration
    [RTUS_IDX_SVC]                   = {GATT_DECL_PRIMARY_SERVICE,        PROP(RD),                                            },

    // Time Update Control Point Characteristic Declaration
    [RTUS_IDX_TIME_UPD_CTNL_PT_CHAR] = {GATT_DECL_CHARACTERISTIC,         PROP(RD),                                            },
    // Time Update Control Point Characteristic Value
    [RTUS_IDX_TIME_UPD_CTNL_PT_VAL]  = {GATT_CHAR_TIME_UPDATE_CNTL_POINT, PROP(WC),            sizeof(tip_time_upd_contr_pt_t) },

    // Time Update State Characteristic Declaration
    [RTUS_IDX_TIME_UPD_STATE_CHAR]   = {GATT_DECL_CHARACTERISTIC,         PROP(RD),                                            },
    // Time Update State Characteristic Value
    [RTUS_IDX_TIME_UPD_STATE_VAL]    = {GATT_CHAR_TIME_UPDATE_STATE,      PROP(RD),            OPT(NO_OFFSET)                  },

};

/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */


/**
 ****************************************************************************************
 * @brief Pack Current Time
 *
 * @param[in] p_buf     Pointer to output buffer
 * @param[in] p_value   Pointer to Current Time value
 ****************************************************************************************
 */
__STATIC void tips_pack_cts_current_time(co_buf_t* p_buf, const tip_curr_time_t* p_value)
{
    // Date-Time
    prf_pack_date_time(p_buf, &(p_value->date_time));

    //Day of Week
    co_buf_tail(p_buf)[0] = p_value->day_of_week;
    co_buf_tail_reserve(p_buf, 1);

    //Fraction 256
    co_buf_tail(p_buf)[0] = p_value->fraction_256;
    co_buf_tail_reserve(p_buf, 1);

    //Adjust Reason
    co_buf_tail(p_buf)[0] = p_value->adjust_reason;
    co_buf_tail_reserve(p_buf, 1);
}

/**
 ****************************************************************************************
 * @brief Pack Local Time Information
 *
 * @param[in] p_buf     Pointer to output buffer
 * @param[in] p_value   Pointer to Local Time Information
 ****************************************************************************************
 */
__STATIC void tips_pack_cts_local_time_info(co_buf_t* p_buf, const tip_loc_time_info_t* p_value)
{
    // Time Zone
    co_buf_tail(p_buf)[0] = p_value->time_zone;
    co_buf_tail_reserve(p_buf, 1);

    //DST offset
    co_buf_tail(p_buf)[0] = p_value->dst_offset;
    co_buf_tail_reserve(p_buf, 1);
}

/**
 ****************************************************************************************
 * @brief Pack Reference Time Information
 *
 * @param[in] p_buf     Pointer to output buffer
 * @param[in] p_value   Pointer to Reference Time Information
 ****************************************************************************************
 */
__STATIC void tips_pack_cts_ref_time_info(co_buf_t* p_buf, const tip_ref_time_info_t* p_value)
{
    // Time source
    co_buf_tail(p_buf)[0] = p_value->time_source;
    co_buf_tail_reserve(p_buf, 1);

    // Time Accuracy
    co_buf_tail(p_buf)[0] = p_value->time_accuracy;
    co_buf_tail_reserve(p_buf, 1);

    // Days since last update about Reference Source
    co_buf_tail(p_buf)[0] = p_value->days_update;
    co_buf_tail_reserve(p_buf, 1);

    // Hours since update about Reference Source
    co_buf_tail(p_buf)[0] = p_value->hours_update;
    co_buf_tail_reserve(p_buf, 1);

}

/**
 ****************************************************************************************
 * @brief Pack Time With DST value
 *
 * @param[in] p_buf     Pointer to output buffer
 * @param[in] p_value   Pointer to Time With DST value
 ****************************************************************************************
 */
__STATIC void tips_pack_ndcs_time_dst(co_buf_t* p_buf, const tip_time_with_dst_t* p_value)
{
    // Date-Time
    prf_pack_date_time(p_buf, &(p_value->date_time));

    // DST Offset
    co_buf_tail(p_buf)[0] = p_value->dst_offset;
    co_buf_tail_reserve(p_buf, 1);
}


/**
 ****************************************************************************************
 * @brief Pack Time Update State value
 *
 * @param[in] p_buf     Pointer to output buffer
 * @param[in] p_value   Pointer to Time Update State
 ****************************************************************************************
 */
__STATIC void tips_pack_rtus_time_upd_state_val(co_buf_t* p_buf, const tip_time_upd_state_t* p_value)
{
    // Current State
    co_buf_tail(p_buf)[0] = p_value->current_state;
    co_buf_tail_reserve(p_buf, 1);

    // Result
    co_buf_tail(p_buf)[0] = p_value->result;
    co_buf_tail_reserve(p_buf, 1);
}

/**
 ****************************************************************************************
 * @brief  This function manages report notification
 ****************************************************************************************
 */
__STATIC void tips_exe_operation(tips_env_t* p_tips_env)
{
    if(!p_tips_env->in_exe_op)
    {
        p_tips_env->in_exe_op = true;

        while(!co_list_is_empty(&(p_tips_env->wait_queue)) && !(p_tips_env->op_ongoing))
        {
            uint16_t status = GAP_ERR_NO_ERROR;
            co_buf_t* p_buf = (co_buf_t*) co_list_pop_front(&(p_tips_env->wait_queue));
            tips_buf_meta_t* p_meta = (tips_buf_meta_t*) co_buf_metadata(p_buf);
            uint8_t conidx = p_meta->conidx;

            status = gatt_srv_event_send(conidx, p_tips_env->user_lid, 0, GATT_NOTIFY, p_meta->hdl, p_buf);

            if(status == GAP_ERR_NO_ERROR)
            {
                p_tips_env->op_ongoing = true;
            }

            co_buf_release(p_buf);

            if(!p_tips_env->op_ongoing)
            {
                const tips_cb_t* p_cb = (const tips_cb_t*) p_tips_env->prf_env.p_cb;
                // Inform application that event has been sent
                p_cb->cb_curr_time_upd_cmp(conidx, status);
            }
        }

        p_tips_env->in_exe_op = false;
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
__STATIC void tips_cb_att_read_get(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset,
                                    uint16_t max_length)
{
    uint8_t      val_id       = TIP_VAL_INVALID;
    tips_env_t*  p_tips_env   = PRF_ENV_GET(TIPS, tips);
    co_buf_t*    p_buf        = NULL;
    uint16_t     att_val_len  = 0;
    uint16_t     status       = PRF_APP_ERROR;

   do
    {
        uint8_t att_idx;

        if(p_tips_env == NULL) break;

        att_idx = (hdl - p_tips_env->start_hdl);

        switch (att_idx)
        {
            //  ------------ READ Notification configuration
            case CTS_IDX_CURRENT_TIME_CFG:
            {
                if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, sizeof(uint16_t), GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
                {
                    uint16_t ntf_cfg = (GETB(p_tips_env->ntf_cfg[conidx], TIPS_CTS_CURRENT_TIME_CFG))
                                     ? PRF_CLI_START_NTF : PRF_CLI_STOP_NTFIND;
                    co_write16p(co_buf_data(p_buf), co_htobs(ntf_cfg));
                    att_val_len = 2;
                    status = GAP_ERR_NO_ERROR;
                }
                else
                {
                    status = ATT_ERR_INSUFF_RESOURCE;
                }
            } break;

            // read time information
            case CTS_IDX_CURRENT_TIME_VAL:    { val_id = TIP_VAL_CTS_CURRENT_TIME;        } break;
            case CTS_IDX_LOCAL_TIME_INFO_VAL: { val_id = TIP_VAL_CTS_LOCAL_TIME_INFO;     } break;
            case CTS_IDX_REF_TIME_INFO_VAL:   { val_id = TIP_VAL_CTS_REF_TIME_INFO;       } break;
            case NDCS_IDX_TIME_DST_VAL:       { val_id = TIP_VAL_NDCS_TIME_DST;           } break;
            case RTUS_IDX_TIME_UPD_STATE_VAL: { val_id = TIP_VAL_RTUS_TIME_UPD_STATE_VAL; } break;

            default: { /* Nothing to do */ } break;
        }
    } while(0);

    // Immediately send confirmation message
    if(val_id == TIP_VAL_INVALID)
    {
        gatt_srv_att_read_get_cfm(conidx, user_lid, token, status, att_val_len, p_buf);

        // release allocated buffer
        if(p_buf != NULL)
        {
            co_buf_release(p_buf);
        }
    }
    else
    {
        // Ask application to provide time information
        const tips_cb_t* p_cb  = (const tips_cb_t*) p_tips_env->prf_env.p_cb;
        uint32_t tips_token = 0;
        SETF(tips_token, TIPS_TOKEN_CONIDX,     conidx);
        SETF(tips_token, TIPS_TOKEN_GATT_TOKEN, token);
        SETF(tips_token, TIPS_TOKEN_VAL_ID,     val_id);
        p_cb->cb_rd_info_req(conidx, tips_token, val_id);
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
__STATIC void tips_cb_att_val_set(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset,
                                   co_buf_t* p_data)
{
    tips_env_t *p_tips_env = PRF_ENV_GET(TIPS, tips);
    bool confirm_send = true;
    uint16_t status = PRF_APP_ERROR;

    do
    {
        const tips_cb_t* p_cb  = (const tips_cb_t*) p_tips_env->prf_env.p_cb;
        uint8_t att_idx;

        if((p_tips_env == NULL) || (offset != 0)) break; // reject write if offset != 0

        att_idx = (hdl - p_tips_env->start_hdl);

        // check which attribute is requested by peer device
        switch (att_idx)
        {
            // Control point value updated
            case RTUS_IDX_TIME_UPD_CTNL_PT_VAL:
            {
                uint8_t ctnl_pt_val = co_buf_data(p_data)[0];
                // Check if value to write is in allowed range
                if((ctnl_pt_val == TIPS_TIME_UPD_CTNL_PT_GET) || (ctnl_pt_val == TIPS_TIME_UPD_CTNL_PT_CANCEL))
                {
                    p_cb->cb_ctnl_pt(conidx, ctnl_pt_val);
                    status = GAP_ERR_NO_ERROR;
                }
            } break;

            // Notification configuration update
            case CTS_IDX_CURRENT_TIME_CFG:
            {
                uint16_t ntf_cfg = co_btohs(co_read16p(co_buf_data(p_data)));

                // Start notification
                if((ntf_cfg == PRF_CLI_START_NTF) || (ntf_cfg == PRF_CLI_STOP_NTFIND))
                {
                    SETB(p_tips_env->ntf_cfg[conidx], TIPS_CTS_CURRENT_TIME_CFG, (ntf_cfg == PRF_CLI_START_NTF));
                    p_cb->cb_bond_data_upd(conidx, ntf_cfg);
                    status = GAP_ERR_NO_ERROR;
                }
            } break;

            default: { /* Nothing to do */ } break;
        }


    } while(0);


    if(confirm_send)
    {
        gatt_srv_att_val_set_cfm(conidx, user_lid, token, status);
    }
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
__STATIC void tips_cb_event_sent(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    // Consider notification done
    tips_env_t* p_tips_env = PRF_ENV_GET(TIPS, tips);
    if(p_tips_env != NULL)
    {
        const tips_cb_t* p_cb = (const tips_cb_t*) p_tips_env->prf_env.p_cb;
        p_tips_env->op_ongoing = false;
        // Inform application that event has been sent
        p_cb->cb_curr_time_upd_cmp(conidx, status);

        // continue operation execution
        tips_exe_operation(p_tips_env);
    }
}

/// Service callback hander
__STATIC const gatt_srv_cb_t tips_cb =
{
        .cb_event_sent    = tips_cb_event_sent,
        .cb_att_read_get  = tips_cb_att_read_get,
        .cb_att_event_get = NULL,
        .cb_att_info_get  = NULL, // Does not support a write starting from != 0 offset
        .cb_att_val_set   = tips_cb_att_val_set,
};

/*
 * PROFILE NATIVE HANDLERS
 ****************************************************************************************
 */

uint16_t tips_enable(uint8_t conidx, uint16_t ntf_cfg)
{
    tips_env_t* p_tips_env = PRF_ENV_GET(TIPS, tips);
    uint16_t status = PRF_ERR_REQ_DISALLOWED;

    if(p_tips_env != NULL)
    {
        // check state of the task
        if (gapc_get_conhdl(conidx) != GAP_INVALID_CONHDL)
        {
            if (ntf_cfg == PRF_CLI_START_NTF)
            {
                // Enable Bonded Data
                SETB(p_tips_env->ntf_cfg[conidx], TIPS_CTS_CURRENT_TIME_CFG, 1);
            }

            status = GAP_ERR_NO_ERROR;
        }
    }

    return (status);
}


uint16_t tips_rd_info_cfm(uint8_t conidx, uint32_t token, uint8_t val_id, const union tip_value* p_value)
{
    tips_env_t* p_tips_env = PRF_ENV_GET(TIPS, tips);
    uint16_t status = GAP_ERR_NO_ERROR;

    if((conidx != GETF(token, TIPS_TOKEN_CONIDX)) || (val_id != GETF(token, TIPS_TOKEN_VAL_ID)))
    {
       status = GAP_ERR_INVALID_PARAM;
    }
    else if(p_tips_env != NULL)
    {
        co_buf_t* p_buf = NULL;
        uint16_t  data_len = 0;

        if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, 0, L2CAP_LE_MTU_MIN + GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
        {
            switch (val_id)
            {
                // pack time information
                case TIP_VAL_CTS_CURRENT_TIME:        { tips_pack_cts_current_time(p_buf, &(p_value->curr_time));             } break;
                case TIP_VAL_CTS_LOCAL_TIME_INFO:     { tips_pack_cts_local_time_info(p_buf, &(p_value->loc_time_info));      } break;
                case TIP_VAL_CTS_REF_TIME_INFO:       { tips_pack_cts_ref_time_info(p_buf, &(p_value->ref_time_info));        } break;
                case TIP_VAL_NDCS_TIME_DST:           { tips_pack_ndcs_time_dst(p_buf, &(p_value->time_with_dst));            } break;
                case TIP_VAL_RTUS_TIME_UPD_STATE_VAL: { tips_pack_rtus_time_upd_state_val(p_buf, &(p_value->time_upd_state)); } break;

                default:                              { /* Nothing to do */                                                   } break;
            }

            data_len = co_buf_data_len(p_buf);
        }
        else
        {
            status = ATT_ERR_INSUFF_RESOURCE;
        }


        // Return buffer that contains report data requested by peer device
        status = gatt_srv_att_read_get_cfm(conidx, p_tips_env->user_lid, GETF(token, TIPS_TOKEN_GATT_TOKEN),
                                           status, data_len, p_buf);
    }
    else
    {
        status = PRF_ERR_REQ_DISALLOWED;
    }

    return (status);
}

uint16_t tips_curr_time_upd(uint8_t conidx, bool enable_ntf_send, const tip_curr_time_t* p_current_time)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;

    tips_env_t* p_tips_env = PRF_ENV_GET(TIPS, tips);

    if(p_current_time == NULL)
    {
        status = PRF_ERR_INVALID_PARAM;
    }
    else if((p_tips_env != NULL) && (gapc_get_conhdl(conidx) != GAP_INVALID_CONHDL))
    {
        co_buf_t* p_buf;
        status = GAP_ERR_NO_ERROR;


        // Check if Notifications are enabled
        if (GETB(p_tips_env->ntf_cfg[conidx],TIPS_CTS_CURRENT_TIME_CFG))
        {
            // Check if notification can be sent
            if (GETB(p_current_time->adjust_reason, TIPS_FLAG_EXT_TIME_UPDATE))
            {
                if (!enable_ntf_send)
                {
                    status = PRF_ERR_REQ_DISALLOWED;
                }
            }

            if (status == GAP_ERR_NO_ERROR)
            {
                if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, 0, L2CAP_LE_MTU_MIN + GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
                {
                    // store buffer context
                    tips_buf_meta_t* p_buf_meta = (tips_buf_meta_t*) co_buf_metadata(p_buf);
                    p_buf_meta->conidx = conidx;
                    p_buf_meta->hdl    = p_tips_env->start_hdl + CTS_IDX_CURRENT_TIME_VAL;

                    // pack current time
                    tips_pack_cts_current_time(p_buf, p_current_time);

                    // put event on wait queue
                    co_list_push_back(&(p_tips_env->wait_queue), &(p_buf->hdr));
                    // execute operation
                    tips_exe_operation(p_tips_env);
                }
                else
                {
                    status = GAP_ERR_INSUFF_RESOURCES;
                }
            }
        }
        else
        {
            status = PRF_ERR_NTF_DISABLED;
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
 * @brief Handles reception of the @ref TIPS_ENABLE_REQ message.
 * The handler enables the HID Over GATT Profile Device Role.
 * @param[in] msgid Id of the message received (probably unused).
 * @param[in] p_param Pointer to the p_parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int tips_enable_req_handler(ke_msg_id_t const msgid, struct tips_enable_req const *p_param,
                                      ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    // Counter for HIDS instance
    struct tips_enable_rsp *p_rsp;
    uint16_t status = tips_enable(p_param->conidx, p_param->current_time_ntf_en);

    // Send back response
    p_rsp = KE_MSG_ALLOC(TIPS_ENABLE_RSP, src_id, dest_id, tips_enable_rsp);
    if(p_rsp != NULL)
    {
        p_rsp->conidx = p_param->conidx;
        p_rsp->status = status;
        ke_msg_send(p_rsp);
    }

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief Handles reception of the @ref TIPS_UPD_CURR_TIME_CMD message.
 * @param[in] msgid Id of the message received (probably unused).
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int tips_upd_curr_time_cmd_handler(ke_msg_id_t const msgid,
                                          struct tips_upd_curr_time_cmd const *p_param,
                                          ke_task_id_t const dest_id,
                                          ke_task_id_t const src_id)
{
    uint16_t status = tips_curr_time_upd(p_param->conidx, p_param->enable_ntf_send, &(p_param->current_time));

    if(status != GAP_ERR_NO_ERROR)
    {
        struct tips_cmp_evt *p_evt;
        // Send the message to the application
        p_evt = KE_MSG_ALLOC(TIPS_CMP_EVT, src_id, dest_id, tips_cmp_evt);
        if(p_evt != NULL)
        {
            p_evt->conidx    = p_param->conidx;
            p_evt->operation = TIPS_UPD_CURR_TIME_CMD_OP_CODE;
            p_evt->status    = status;
            ke_msg_send(p_evt);
        }
    }

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief Handles reception of the @ref TIPS_RD_CFM message.
 * @param[in] msgid Id of the message received (probably unused).
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int tips_rd_cfm_handler(ke_msg_id_t const msgid, struct tips_rd_cfm const *p_param,
                                 ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    tips_rd_info_cfm(p_param->conidx, p_param->token, p_param->val_id, &(p_param->value));

    return (KE_MSG_CONSUMED);
}


/// Default State handlers definition
KE_MSG_HANDLER_TAB(tips)
{
    // Note: all messages must be sorted in ID ascending order

    { TIPS_ENABLE_REQ,               (ke_msg_func_t) tips_enable_req_handler        },
    { TIPS_UPD_CURR_TIME_CMD,        (ke_msg_func_t) tips_upd_curr_time_cmd_handler },
    { TIPS_RD_CFM,                   (ke_msg_func_t) tips_rd_cfm_handler            },
};


    /**
     ****************************************************************************************
     * @brief Inform that bond data updated for the connection.
     *
     * @param[in] conidx        Connection index
     * @param[in] cfg_val       Char. Client Characteristic Configuration
     ****************************************************************************************
     */
__STATIC void tips_cb_bond_data_upd(uint8_t conidx, uint16_t cfg_val)
{
    struct tips_current_time_ccc_ind* p_ind;
    p_ind =  KE_MSG_ALLOC(TIPS_CURRENT_TIME_CCC_IND, PRF_DST_TASK(TIPS), PRF_SRC_TASK(TIPS), tips_current_time_ccc_ind);

    if(p_ind != NULL)
    {
        p_ind->conidx  = conidx;
        p_ind->cfg_val = cfg_val;
        ke_msg_send(p_ind);
    }
}

/**
 ****************************************************************************************
 * @brief Completion of current time update procedure
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 ****************************************************************************************
 */
__STATIC void tips_cb_curr_time_upd_cmp(uint8_t conidx, uint16_t status)
{
    struct tips_cmp_evt *p_evt;
    // Send the message to the application
    p_evt = KE_MSG_ALLOC(TIPS_CMP_EVT, PRF_DST_TASK(TIPS), PRF_SRC_TASK(TIPS), tips_cmp_evt);
    if(p_evt != NULL)
    {
        p_evt->conidx    = conidx;
        p_evt->operation = TIPS_UPD_CURR_TIME_CMD_OP_CODE;
        p_evt->status    = status;
        ke_msg_send(p_evt);
    }
}

/**
 ****************************************************************************************
 * @brief Read a time information
 *
 * Time information must be returned by application using @see tips_rd_info_cfm
 *
 * @param[in] conidx        Connection index
 * @param[in] token         Token information that must be returned in confirmation
 * @param[in] val_id        Value identifier (@see enum tip_value_id)
 ****************************************************************************************
 */
__STATIC void tips_cb_cb_rd_info_req(uint8_t conidx, uint32_t token, uint8_t val_id)
{
    struct tips_rd_req_ind* p_req_ind =  KE_MSG_ALLOC(TIPS_RD_REQ_IND, PRF_DST_TASK(TIPS),
                                                      PRF_SRC_TASK(TIPS), tips_rd_req_ind);

    if(p_req_ind != NULL)
    {
        p_req_ind->conidx      = conidx;
        p_req_ind->token       = token;
        p_req_ind->val_id      = val_id;
        ke_msg_send(p_req_ind);
    }
}

/**
 ****************************************************************************************
 * @brief Command received on  control point
 *
 * @param[in] conidx          Connection index
 * @param[in] value           Time Update Control Point Value
 ****************************************************************************************
 */
__STATIC void tips_cb_ctnl_pt(uint8_t conidx, tip_time_upd_contr_pt_t value)
{
    struct tips_time_upd_ctnl_pt_ind* p_ind;
    p_ind = KE_MSG_ALLOC(TIPS_TIME_UPD_CTNL_PT_IND, PRF_DST_TASK(TIPS), PRF_SRC_TASK(TIPS), tips_time_upd_ctnl_pt_ind);

    if(p_ind != NULL)
    {
        p_ind->conidx      = conidx;
        p_ind->value     = value;

        ke_msg_send(p_ind);
    }
}


/// Default Message handle
__STATIC const tips_cb_t tips_msg_cb =
{
    .cb_bond_data_upd     = tips_cb_bond_data_upd,
    .cb_curr_time_upd_cmp = tips_cb_curr_time_upd_cmp,
    .cb_rd_info_req       = tips_cb_cb_rd_info_req,
    .cb_ctnl_pt           = tips_cb_ctnl_pt,
};
#endif // (BLE_HL_MSG_API)

/*
 * PROFILE DEFAULT HANDLERS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Initialization of the TIPS module.
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
__STATIC uint16_t tips_init(prf_data_t *p_env, uint16_t *p_start_hdl, uint8_t sec_lvl, uint8_t user_prio,
                          struct tips_db_cfg *p_params, const tips_cb_t* p_cb)
{
    //------------------ create the attribute database for the profile -------------------

    // DB Creation Status
    uint16_t status = GAP_ERR_NO_ERROR;
    uint8_t user_lid = GATT_INVALID_USER_LID;

    do
    {
        // Service content flag
        uint32_t cfg_flag= TIPS_CTS_CURRENT_TIME_MASK;

        tips_env_t* p_tips_env;
        // Service content flag
        uint16_t cts_start_hdl  = GATT_INVALID_HDL;

        #if (BLE_HL_MSG_API)
        if(p_cb == NULL)
        {
            p_cb = &(tips_msg_cb);
        }
        #endif // (BLE_HL_MSG_API)

        if(   (p_params == NULL) || (p_start_hdl == NULL) || (p_cb == NULL) || (p_cb->cb_bond_data_upd == NULL)
           || (p_cb->cb_curr_time_upd_cmp == NULL) || (p_cb->cb_rd_info_req == NULL) || (p_cb->cb_ctnl_pt == NULL))
        {
            status = GAP_ERR_INVALID_PARAM;
            break;
        }

        // register TIPS user
        status = gatt_user_srv_register(L2CAP_LE_MTU_MIN, user_prio, &tips_cb, &user_lid);
        if(status != GAP_ERR_NO_ERROR) break;

         //------------------ create the attribute database for the profile -------------------

        // Check that attribute list can be allocated.
        status = gatt_db_handle_range_reserve(user_lid, TIPS_IDX_DB_MAX, p_start_hdl);
        if(status != GAP_ERR_NO_ERROR) break;

        // Set Configuration Flag Value
        if (GETB(p_params->features, TIPS_CTS_LOC_TIME_INFO_SUP))
        {
            cfg_flag |= TIPS_CTS_LOC_TIME_INFO_MASK;
        }

        if (GETB(p_params->features, TIPS_CTS_REF_TIME_INFO_SUP))
        {
            cfg_flag |= TIPS_CTS_REF_TIME_INFO_MASK;
        }

        /*---------------------------------------------------*
         * Current Time Service Creation
         *---------------------------------------------------*/
        status = gatt_db_svc16_add(user_lid, sec_lvl, GATT_SVC_CURRENT_TIME, CTS_IDX_NB,
                                   (uint8_t *)&cfg_flag, &(tips_att_db[CTS_IDX_SVC]), CTS_IDX_NB, p_start_hdl);
        if(status != GAP_ERR_NO_ERROR) break;

        cts_start_hdl = *p_start_hdl;
        *p_start_hdl += CTS_IDX_NB;


        /*---------------------------------------------------*
         * Next DST Change Service Creation
         *---------------------------------------------------*/
        if(GETB(p_params->features, TIPS_NDCS_SUP))
        {
            status = gatt_db_svc16_add(user_lid, sec_lvl, GATT_SVC_NEXT_DST_CHANGE, NDCS_IDX_NB,
                                       NULL, &(tips_att_db[NDCS_IDX_SVC]), NDCS_IDX_NB, p_start_hdl);
            if(status != GAP_ERR_NO_ERROR) break;
        }
        *p_start_hdl += NDCS_IDX_NB;

        /*---------------------------------------------------*
         * Reference Time Update Service Creation
         *---------------------------------------------------*/
        if(GETB(p_params->features, TIPS_RTUS_SUP))
        {
            status = gatt_db_svc16_add(user_lid, sec_lvl, GATT_SVC_REF_TIME_UPDATE, RTUS_IDX_NB,
                                       NULL, &(tips_att_db[RTUS_IDX_SVC]), RTUS_IDX_NB, p_start_hdl);
            if(status != GAP_ERR_NO_ERROR) break;
        }

        //-------------------- allocate memory required for the profile  ---------------------
        p_tips_env = (tips_env_t *) ke_malloc(sizeof(tips_env_t), KE_MEM_ATT_DB);

        if(p_tips_env != NULL)
        {
            // allocate TIPS required environment variable
            p_env->p_env = (prf_hdr_t *) p_tips_env;
            p_tips_env->start_hdl        = cts_start_hdl;
            p_tips_env->in_exe_op        = false;
            p_tips_env->features         = p_params->features;
            p_tips_env->user_lid         = user_lid;
            p_tips_env->op_ongoing       = false;
            memset(p_tips_env->ntf_cfg, 0, BLE_CONNECTION_MAX);
            co_list_init(&(p_tips_env->wait_queue));

            // initialize profile environment variable
            p_tips_env->prf_env.p_cb     = p_cb;
            #if (BLE_HL_MSG_API)
            p_env->desc.msg_handler_tab  = tips_msg_handler_tab;
            p_env->desc.msg_cnt          = ARRAY_LEN(tips_msg_handler_tab);
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
__STATIC uint16_t tips_destroy(prf_data_t *p_env, uint8_t reason)
{
    uint16_t status = GAP_ERR_NO_ERROR;
    tips_env_t *p_tips_env = (tips_env_t *) p_env->p_env;

    if(reason != PRF_DESTROY_RESET)
    {
        status = gatt_user_unregister(p_tips_env->user_lid);
    }

    if(status == GAP_ERR_NO_ERROR)
    {
        if(reason != PRF_DESTROY_RESET)
        {
            // remove buffer in wait queue
            while(!co_list_is_empty(&p_tips_env->wait_queue))
            {
                co_buf_t* p_buf = (co_buf_t*) co_list_pop_front(&p_tips_env->wait_queue);
                co_buf_release(p_buf);
            }
        }

        // free profile environment variables
        p_env->p_env = NULL;
        ke_free(p_tips_env);
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
__STATIC void tips_con_create(prf_data_t *p_env, uint8_t conidx, const gap_con_param_t* p_con_param)
{
    tips_env_t *p_tips_env = (tips_env_t *) p_env->p_env;
    p_tips_env->ntf_cfg[conidx] = 0;
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
__STATIC void tips_con_cleanup(prf_data_t *p_env, uint8_t conidx, uint16_t reason)
{
    tips_env_t *p_tips_env = (tips_env_t *) p_env->p_env;
    p_tips_env->ntf_cfg[conidx] = 0;
}

/// TIPS Task interface required by profile manager
const prf_task_cbs_t tips_itf =
{
    .cb_init          = (prf_init_cb) tips_init,
    .cb_destroy       = tips_destroy,
    .cb_con_create    = tips_con_create,
    .cb_con_cleanup   = tips_con_cleanup,
    .cb_con_upd       = NULL,
};

/**
 ****************************************************************************************
 * @brief Retrieve service profile interface
 *
 * @return service profile interface
 ****************************************************************************************
 */
const prf_task_cbs_t *tips_prf_itf_get(void)
{
    return &tips_itf;
}

#endif //BLE_TIP_SERVER

/// @} TIPS

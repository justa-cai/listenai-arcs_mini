/**
 ****************************************************************************************
 *
 * @file htpt.c
 *
 * @brief Health Thermometer Profile Thermometer implementation.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup HTPT
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "btip_config.h"
//include "prf_config.h"

//#if (BLE_HT_THERMOM)
#include "htpt.h"
#include "gatt.h"
#include "ke_mem.h"
#include "co_utils.h"
#include "co_utils.h"
#include "co_endian.h"
#include "co_list.h"
#include "co_math.h"
#include <string.h>

#include "prf_common.h"

/*
 * DEFINES
 ****************************************************************************************
 */
#define HTPT_INVALID_IDX                  (0xFF)

///Valid range for measurement interval values (s)
#define HTPT_MEAS_INTV_DFLT_MIN           (0x0001)
#define HTPT_MEAS_INTV_DFLT_MAX           (0x000A)

#define HTPT_TEMP_MEAS_MAX_LEN            (13)
#define HTPT_TEMP_TYPE_MAX_LEN            (1)
#define HTPT_MEAS_INTV_LEN                (2)
#define HTPT_MEAS_INTV_RANGE_LEN          (4)
#define HTPT_IND_NTF_CFG_LEN              (2)

#define HTPT_TEMP_MEAS_MASK               (0x000F)
#define HTPT_TEMP_TYPE_MASK               (0x0030)
#define HTPT_INTM_TEMP_MASK               (0x01C0)
#define HTPT_MEAS_INTV_MASK               (0x0600)
#define HTPT_MEAS_INTV_CCC_MASK           (0x0800)
#define HTPT_MEAS_INTV_VALID_RGE_MASK     (0x1000)

#define HTPT_TEMP_MEAS_ATT_NB             (4)
#define HTPT_TEMP_TYPE_ATT_NB             (2)
#define HTPT_INTERM_MEAS_ATT_NB           (3)
#define HTPT_MEAS_INTV_ATT_NB             (2)
#define HTPT_MEAS_INTV_CCC_ATT_NB         (1)
#define HTPT_MEAS_INTV_RNG_ATT_NB         (1)


#define HTPT_HANDLE(idx) (htpt_att_hdl_get(p_htpt_env, (idx)))
#define HTPT_IDX(hdl)    (htpt_att_idx_get(p_htpt_env, (hdl)))


///Attributes database elements
enum hts_att_db_list
{
    HTS_IDX_SVC,

    HTS_IDX_TEMP_MEAS_CHAR,
    HTS_IDX_TEMP_MEAS_VAL,
    HTS_IDX_TEMP_MEAS_IND_CFG,

    HTS_IDX_TEMP_TYPE_CHAR,
    HTS_IDX_TEMP_TYPE_VAL,

    HTS_IDX_INTERM_TEMP_CHAR,
    HTS_IDX_INTERM_TEMP_VAL,
    HTS_IDX_INTERM_TEMP_CFG,

    HTS_IDX_MEAS_INTV_CHAR,
    HTS_IDX_MEAS_INTV_VAL,
    HTS_IDX_MEAS_INTV_CFG,
    HTS_IDX_MEAS_INTV_VAL_RANGE,

    HTS_IDX_NB,
};

/*
 * TYPES DEFINITION
 ****************************************************************************************
 */

/// ongoing operation information
typedef struct htpt_buf_meta
{
     /// Handle of the attribute to indicate/notify
     uint16_t handle;
     /// Operation
     uint8_t  operation;
     /// used to know on which device interval update has been requested, and to prevent
     /// indication to be triggered on this connection index
     uint8_t  conidx;
} htpt_buf_meta_t;


///Health Thermometer Profile Thermometer Environment Variable
typedef struct htpt_env
{
    /// profile environment
    prf_hdr_t   prf_env;
    /// Operation Event TX wait queue
    co_list_t   wait_queue;
    /// Service Start Handle
    uint16_t    start_hdl;
    /// Database configuration
    uint16_t    features;
    /// Token used for
    uint16_t    new_meas_intv_token;
    /// New Measure interval - waiting for application confirmation
    uint16_t    new_meas_intv;
    /// Current Measure interval
    uint16_t    meas_intv;
    /// measurement interval range min
    uint16_t    meas_intv_min;
    /// measurement interval range max
    uint16_t    meas_intv_max;
    /// Temperature Type Value
    uint8_t     temp_type;
    /// GATT User Local Identifier
    uint8_t     user_lid;
    /// Operation On-going
    bool        op_ongoing;
    /// Prevent recursion in execute_operation function
    bool        in_exe_op;

    /// Notification and indication configuration of peer devices.
    uint8_t     ntf_ind_cfg[BLE_CONNECTION_MAX];
} htpt_env_t;


/*
 * DIS ATTRIBUTES DATABASE
 ****************************************************************************************
 */

/// Full HTS Database Description - Used to add attributes into the database
const gatt_att16_desc_t htpt_att_db[HTS_IDX_NB] =
{
    // Health Thermometer Service Declaration
    [HTS_IDX_SVC]                 = {GATT_DECL_PRIMARY_SERVICE,      PROP(RD),          0                       },

    // Temperature Measurement Characteristic Declaration
    [HTS_IDX_TEMP_MEAS_CHAR]      = {GATT_DECL_CHARACTERISTIC,       PROP(RD),          0                       },
    // Temperature Measurement Characteristic Value
    [HTS_IDX_TEMP_MEAS_VAL]       = {GATT_CHAR_TEMPERATURE_MEAS,     PROP(I),           OPT(NO_OFFSET)          },
    // Temperature Measurement Characteristic - Client Characteristic Configuration Descriptor
    [HTS_IDX_TEMP_MEAS_IND_CFG]   = {GATT_DESC_CLIENT_CHAR_CFG,      PROP(RD)|PROP(WR), OPT(NO_OFFSET)          },

    // Temperature Type Characteristic Declaration
    [HTS_IDX_TEMP_TYPE_CHAR]      = {GATT_DECL_CHARACTERISTIC,       PROP(RD),          0                       },
    // Temperature Type Characteristic Value
    [HTS_IDX_TEMP_TYPE_VAL]       = {GATT_CHAR_TEMPERATURE_TYPE,     PROP(RD),          OPT(NO_OFFSET)          },

    // Intermediate Measurement Characteristic Declaration
    [HTS_IDX_INTERM_TEMP_CHAR]    = {GATT_DECL_CHARACTERISTIC,       PROP(RD),          0                       },
    // Intermediate Measurement Characteristic Value
    [HTS_IDX_INTERM_TEMP_VAL]     = {GATT_CHAR_INTERMED_TEMPERATURE, PROP(N),           OPT(NO_OFFSET)          },
    // Intermediate Measurement Characteristic - Client Characteristic Configuration Descriptor
    [HTS_IDX_INTERM_TEMP_CFG]     = {GATT_DESC_CLIENT_CHAR_CFG,      PROP(RD)|PROP(WR), OPT(NO_OFFSET)          },

    // Measurement Interval Characteristic Declaration
    [HTS_IDX_MEAS_INTV_CHAR]      = {GATT_DECL_CHARACTERISTIC,       PROP(RD),          0                       },
    // Measurement Interval Characteristic Value
    [HTS_IDX_MEAS_INTV_VAL]       = {GATT_CHAR_MEAS_INTERVAL,        PROP(RD),          OPT(NO_OFFSET) | HTPT_MEAS_INTV_LEN  },
    // Measurement Interval Characteristic - Client Characteristic Configuration Descriptor
    [HTS_IDX_MEAS_INTV_CFG]       = {GATT_DESC_CLIENT_CHAR_CFG,      PROP(RD)|PROP(WR), OPT(NO_OFFSET)          },
    // Measurement Interval Characteristic - Valid Range Descriptor
    [HTS_IDX_MEAS_INTV_VAL_RANGE] = {GATT_DESC_VALID_RANGE,          PROP(RD),          OPT(NO_OFFSET)          },
};

/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Compute a flag allowing to know attributes to add into the database
 *
 * @param[in] features     Health thermometer Feature (@see enum htpt_features)
 *
 * @return A 16-bit flag whose each bit matches an attribute.
 *         If the bit is set to 1, the attribute will be added into the database.
 ****************************************************************************************
 */
__STATIC uint16_t htpt_compute_cfg_flag(uint16_t features)
{
    //Temperature Measurement Characteristic is mandatory
    uint16_t cfg_bf = HTPT_TEMP_MEAS_MASK;

    //Check if Temperature Type Char. is supported
    if (GETB(features, HTPT_TEMP_TYPE_CHAR_SUP))
    {
        cfg_bf |= HTPT_TEMP_TYPE_MASK;
    }

    //Check if Intermediate Temperature Char. is supported
    if (GETB(features, HTPT_INTERM_TEMP_CHAR_SUP))
    {
        cfg_bf |= HTPT_INTM_TEMP_MASK;
    }

    //Check if Measurement Interval Char. is supported
    if (GETB(features, HTPT_MEAS_INTV_CHAR_SUP))
    {
        cfg_bf |= HTPT_MEAS_INTV_MASK;

        //Check if Measurement Interval Char. supports indications
        if (GETB(features, HTPT_MEAS_INTV_IND_SUP))
        {
            cfg_bf |= HTPT_MEAS_INTV_CCC_MASK;
        }

        //Check if Measurement Interval Char. is writable
        if (GETB(features, HTPT_MEAS_INTV_WR_SUP))
        {
            cfg_bf |= HTPT_MEAS_INTV_VALID_RGE_MASK;
        }
    }

    return cfg_bf;
}

/**
 ****************************************************************************************
 * @brief Retrieve attribute handle from attribute index
 *
 * @param[in] p_htpt_env   Environment variable
 * @param[in] att_idx      Attribute Index
 *
 * @return attribute Handle
 ****************************************************************************************
 */
__STATIC uint16_t htpt_att_hdl_get(htpt_env_t* p_htpt_env, uint8_t att_idx)
{
    uint16_t handle = p_htpt_env->start_hdl;

    do
    {
        // Mandatory attribute handle
        if (att_idx > HTS_IDX_TEMP_MEAS_IND_CFG)
        {
            handle += HTPT_TEMP_MEAS_ATT_NB;
        }
        else
        {
            handle += att_idx;
            break;
        }

        // Temperature Type
        if ((GETB(p_htpt_env->features, HTPT_TEMP_TYPE_CHAR_SUP)) && (att_idx > HTS_IDX_TEMP_TYPE_VAL))
        {
            handle += HTPT_TEMP_TYPE_ATT_NB;
        }
        else if (!GETB(p_htpt_env->features, HTPT_TEMP_TYPE_CHAR_SUP))
        {
            handle = GATT_INVALID_HDL;
            break;
        }
        else
        {
            handle += att_idx - HTS_IDX_TEMP_TYPE_CHAR;
            break;
        }

        // Intermediate Temperature Measurement
        if ((GETB(p_htpt_env->features, HTPT_INTERM_TEMP_CHAR_SUP)) && (att_idx > HTS_IDX_INTERM_TEMP_CFG))
        {
            handle += HTPT_INTERM_MEAS_ATT_NB;
        }
        else if (!GETB(p_htpt_env->features, HTPT_INTERM_TEMP_CHAR_SUP))
        {
            handle = GATT_INVALID_HDL;
            break;
        }
        else
        {
            handle += att_idx - HTS_IDX_INTERM_TEMP_CHAR;
            break;
        }

        // Measurement Interval
        if (!GETB(p_htpt_env->features, HTPT_MEAS_INTV_CHAR_SUP) || (att_idx >= HTS_IDX_NB))
        {
            handle = GATT_INVALID_HDL;
            break;
        }

        if (att_idx <= HTS_IDX_MEAS_INTV_VAL)
        {
            handle += att_idx - HTS_IDX_MEAS_INTV_CHAR;
            break;
        }
        else
        {
            handle += HTPT_MEAS_INTV_ATT_NB;
        }

        // Measurement Interval Indication
        if (att_idx == HTS_IDX_MEAS_INTV_CFG)
        {
            if (!GETB(p_htpt_env->features, HTPT_MEAS_INTV_IND_SUP))
            {
                handle = GATT_INVALID_HDL;
                break;
            }
        }
        // Measurement Interval Write permission
        else if (GETB(p_htpt_env->features, HTPT_MEAS_INTV_WR_SUP))
        {
            handle += HTPT_MEAS_INTV_CCC_ATT_NB;

            if (!GETB(p_htpt_env->features, HTPT_MEAS_INTV_WR_SUP))
            {
                handle = GATT_INVALID_HDL;
                break;
            }
        }
    } while (0);

    return handle;
}

/**
 ****************************************************************************************
 * @brief Retrieve attribute index from attribute handle
 *
 * @param[in] p_htpt_env   Environment variable
 * @param[in] handle       Attribute Handle
 *
 * @return attribute Index
 ****************************************************************************************
 */
__STATIC uint8_t htpt_att_idx_get(htpt_env_t* p_htpt_env, uint16_t handle)
{
    uint16_t handle_ref = p_htpt_env->start_hdl;
    uint8_t att_idx = HTPT_INVALID_IDX;

    do
    {
        // not valid handle
        if (handle < handle_ref)
        {
            break;
        }

        // Mandatory attribute handle
        handle_ref += HTPT_TEMP_MEAS_ATT_NB;

        if (handle < handle_ref)
        {
            att_idx = HTS_IDX_TEMP_TYPE_CHAR - (handle_ref - handle);
            break;
        }

        // Temperature Type
        if (GETB(p_htpt_env->features, HTPT_TEMP_TYPE_CHAR_SUP))
        {
            handle_ref += HTPT_TEMP_TYPE_ATT_NB;

            if (handle < handle_ref)
            {
                att_idx = HTS_IDX_INTERM_TEMP_CHAR - (handle_ref - handle);
                break;
            }
        }

        // Intermediate Temperature Measurement
        if (GETB(p_htpt_env->features, HTPT_INTERM_TEMP_CHAR_SUP))
        {
            handle_ref += HTPT_INTERM_MEAS_ATT_NB;

            if (handle < handle_ref)
            {
                att_idx = HTS_IDX_MEAS_INTV_CHAR - (handle_ref - handle);
                break;
            }
        }

        // Measurement Interval
        if (GETB(p_htpt_env->features, HTPT_MEAS_INTV_CHAR_SUP))
        {
            handle_ref += HTPT_MEAS_INTV_ATT_NB;

            if (handle < handle_ref)
            {
                att_idx = HTS_IDX_MEAS_INTV_CFG - (handle_ref - handle);
                break;
            }

            if (GETB(p_htpt_env->features, HTPT_MEAS_INTV_IND_SUP))
            {
                if (handle == handle_ref)
                {
                    att_idx = HTS_IDX_MEAS_INTV_CFG;
                    break;
                }

                handle_ref += HTPT_MEAS_INTV_CCC_ATT_NB;
            }

            if (GETB(p_htpt_env->features, HTPT_MEAS_INTV_WR_SUP))
            {
                if (handle == handle_ref)
                {
                    att_idx = HTS_IDX_MEAS_INTV_VAL_RANGE;
                    break;
                }
            }
        }
    } while (0);

    return att_idx;
}

/**
 ****************************************************************************************
 * @brief Pack temperature value from several components
 * @param p_buf                 Pointer to the output buffer
 * @param p_temp_meas           Pointer to Temperature measurement value to pack
 ****************************************************************************************
 */
__STATIC void htpt_pack_temp_value(co_buf_t* p_buf, const htp_temp_meas_t* p_temp_meas)
{

    co_buf_tail(p_buf)[0] = p_temp_meas->flags;
    co_buf_tail_reserve(p_buf, 1);

    co_write32p(co_buf_tail(p_buf), co_htobl(p_temp_meas->temp));
    co_buf_tail_reserve(p_buf, 4);

    // Time Flag Set
    if (GETB(p_temp_meas->flags, HTP_FLAG_TIME))
    {
        prf_pack_date_time(p_buf, &(p_temp_meas->time_stamp));
    }

    // Type flag set
    if (GETB(p_temp_meas->flags, HTP_FLAG_TYPE))
    {
        co_buf_tail(p_buf)[0] = p_temp_meas->type;
        co_buf_tail_reserve(p_buf, 1);
    }
}

/**
 ****************************************************************************************
 * @brief Update Notification, Indication configuration
 *
 * @param[in] conidx    Connection index
 * @param[in] cfg       Indication configuration flag
 * @param[in] valid_val Valid value if NTF/IND enable
 * @param[in] value     Value set by peer device.
 *
 * @return status of configuration update
 ****************************************************************************************
 */
__STATIC uint8_t htpt_update_ntf_ind_cfg(uint8_t conidx, uint8_t cfg, uint16_t valid_val, uint16_t value)
{
    htpt_env_t* p_htpt_env = PRF_ENV_GET(HTPT, htpt);
    uint16_t status = GAP_ERR_NO_ERROR;

    if ((value != valid_val) && (value != PRF_CLI_STOP_NTFIND))
    {
        status = PRF_APP_ERROR;

    }
    else if (value == valid_val)
    {
        p_htpt_env->ntf_ind_cfg[conidx] |= cfg;
    }
    else
    {
        p_htpt_env->ntf_ind_cfg[conidx] &= ~cfg;
    }

    if (status == GAP_ERR_NO_ERROR)
    {
        const htpt_cb_t* p_cb = (const htpt_cb_t*) p_htpt_env->prf_env.p_cb;
        p_cb->cb_bond_data_upd(conidx, p_htpt_env->ntf_ind_cfg[conidx]);
    }

    return (status);
}


/**
 ****************************************************************************************
 * @brief  This function fully manage notification and indication of health thermometer
 *         to peer(s) device(s) according to on-going operation requested by application:
 *            - Modification of Intermediate Temperature
 *            - Indicate to a known device that Temperature Measured has change
 *            - Indicate to a known device that Measure Interval has change
 ****************************************************************************************
 */
__STATIC void htpt_exe_operation(void)
{
    htpt_env_t* p_htpt_env = PRF_ENV_GET(HTPT, htpt);

    if((p_htpt_env != NULL) && (!p_htpt_env->in_exe_op))
    {
        p_htpt_env->in_exe_op = true;

        while(!co_list_is_empty(&(p_htpt_env->wait_queue)) && !(p_htpt_env->op_ongoing))
        {
            uint16_t status = GAP_ERR_NO_ERROR;
            uint8_t  conidx;
            co_buf_t* p_buf = (co_buf_t*) co_list_pop_front(&(p_htpt_env->wait_queue));
            htpt_buf_meta_t* p_buf_meta = (htpt_buf_meta_t*) co_buf_metadata(p_buf);
            uint32_t conidx_bf = 0;

            // prepare bit field of connection where event must be triggered
            for(conidx = 0 ; conidx < BLE_CONNECTION_MAX ; conidx++)
            {
                if(p_htpt_env->ntf_ind_cfg[conidx] & p_buf_meta->operation)
                {
                    conidx_bf |= CO_BIT(conidx);
                }
            }

            // check if a connection index must be ignored
            if(p_buf_meta->conidx != GAP_INVALID_CONIDX)
            {
                conidx_bf &= ~CO_BIT(p_buf_meta->conidx);
            }

            // Send Notification / Indication
            if(conidx_bf != 0)
            {
                // Send Multi point event
                status = gatt_srv_event_mtp_send(conidx_bf, p_htpt_env->user_lid, p_buf_meta->operation,
                                                (p_buf_meta->operation != HTPT_CFG_INTERM_MEAS_NTF) ? GATT_INDICATE : GATT_NOTIFY,
                                                 p_buf_meta->handle, p_buf, true);
                if(status == GAP_ERR_NO_ERROR)
                {
                    p_htpt_env->op_ongoing = true;
                }
            }

            // Consider job done
            if ((!p_htpt_env->op_ongoing) && (p_buf_meta->operation != HTPT_CFG_MEAS_INTV_IND))
            {
                const htpt_cb_t* p_cb = (const htpt_cb_t*) p_htpt_env->prf_env.p_cb;
                // Inform application that event has been sent
                p_cb->cb_temp_send_cmp(status);
            }

            // release buffer
            co_buf_release(p_buf);
        }

        p_htpt_env->in_exe_op = false;
    }
}


/**
 ****************************************************************************************
 * @brief  Try to send measurement interval change indication
 *
 * @param[in] p_htpt_env Pointer to module environment
 * @param[in] meas_intv  New Measurement interval
 * @param[in] conidx     Connection index to ignore
 ****************************************************************************************
 */
__STATIC void htpt_meas_intv_ind_send(htpt_env_t* p_htpt_env, uint16_t meas_intv, uint8_t conidx)
{
    // trigger measurement update indication
    if(GETB(p_htpt_env->features, HTPT_MEAS_INTV_IND_SUP))
    {
        co_buf_t* p_buf = NULL;
        uint16_t status = co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, 0, HTPT_TEMP_MEAS_MAX_LEN + GATT_BUFFER_TAIL_LEN);

        if(status == CO_BUF_ERR_NO_ERROR)
        {
            htpt_buf_meta_t* p_buf_meta = (htpt_buf_meta_t*) co_buf_metadata(p_buf);
            p_buf_meta->conidx    = conidx;
            p_buf_meta->operation = HTPT_CFG_MEAS_INTV_IND;
            p_buf_meta->handle    = HTPT_HANDLE(HTS_IDX_MEAS_INTV_VAL);
            co_write16p(co_buf_tail(p_buf), co_htobs(meas_intv));
            co_buf_tail_reserve(p_buf, HTPT_MEAS_INTV_LEN);

            // put event on wait queue
            co_list_push_back(&(p_htpt_env->wait_queue), &(p_buf->hdr));

            // execute operation
            htpt_exe_operation();
        }
        else
        {
            status = GAP_ERR_INSUFF_RESOURCES;
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
__STATIC void htpt_cb_att_read_get(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset,
                                   uint16_t max_length)
{
    htpt_env_t* p_htpt_env = PRF_ENV_GET(HTPT, htpt);
    uint8_t     value_size = 0;
    uint16_t    status     = PRF_APP_ERROR;
    co_buf_t*   p_buf      = NULL;

    if(p_htpt_env != NULL)
    {
        status = co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, 0, HTPT_TEMP_MEAS_MAX_LEN + GATT_BUFFER_TAIL_LEN);

        if(status == GAP_ERR_NO_ERROR)
        {
            // retrieve handle information
            switch(HTPT_IDX(hdl))
            {
                case HTS_IDX_MEAS_INTV_VAL:
                {
                    value_size = HTPT_MEAS_INTV_LEN;
                    co_write16p(co_buf_tail(p_buf), co_htobs(p_htpt_env->meas_intv));
                    co_buf_tail_reserve(p_buf, HTPT_MEAS_INTV_LEN);
                } break;

                case HTS_IDX_MEAS_INTV_VAL_RANGE:
                {
                    value_size = HTPT_MEAS_INTV_RANGE_LEN;
                    co_write16p(co_buf_tail(p_buf), co_htobs(p_htpt_env->meas_intv_min));
                    co_buf_tail_reserve(p_buf, HTPT_MEAS_INTV_LEN);
                    co_write16p(co_buf_tail(p_buf), co_htobs(p_htpt_env->meas_intv_max));
                    co_buf_tail_reserve(p_buf, HTPT_MEAS_INTV_LEN);
                } break;

                case HTS_IDX_TEMP_MEAS_IND_CFG:
                {
                    value_size = HTPT_IND_NTF_CFG_LEN;
                    co_write16p(co_buf_tail(p_buf),
                                co_htobs(((p_htpt_env->ntf_ind_cfg[conidx] & HTPT_CFG_STABLE_MEAS_IND) != 0)
                                         ? PRF_CLI_START_IND : PRF_CLI_STOP_NTFIND));
                    co_buf_tail_reserve(p_buf, HTPT_IND_NTF_CFG_LEN);
                } break;

                case HTS_IDX_INTERM_TEMP_CFG:
                {
                    value_size = HTPT_IND_NTF_CFG_LEN;
                    co_write16p(co_buf_tail(p_buf),
                                co_htobs(((p_htpt_env->ntf_ind_cfg[conidx] & HTPT_CFG_INTERM_MEAS_NTF) != 0)
                                ? PRF_CLI_START_NTF : PRF_CLI_STOP_NTFIND));
                    co_buf_tail_reserve(p_buf, HTPT_IND_NTF_CFG_LEN);
                } break;

                case HTS_IDX_MEAS_INTV_CFG:
                {
                    value_size = HTPT_IND_NTF_CFG_LEN;
                    co_write16p(co_buf_tail(p_buf),
                                co_htobs(((p_htpt_env->ntf_ind_cfg[conidx] & HTPT_CFG_MEAS_INTV_IND) != 0)
                                ? PRF_CLI_START_IND : PRF_CLI_STOP_NTFIND));
                    co_buf_tail_reserve(p_buf, HTPT_IND_NTF_CFG_LEN);
                } break;

                case HTS_IDX_TEMP_TYPE_VAL:
                {
                    value_size = HTPT_TEMP_TYPE_MAX_LEN;
                    co_buf_tail(p_buf)[0] = p_htpt_env->temp_type;
                    co_buf_tail_reserve(p_buf, HTPT_TEMP_TYPE_MAX_LEN);
                } break;

                default:
                {
                    status = ATT_ERR_REQUEST_NOT_SUPPORTED;
                } break;
            }
        }
        else
        {
            status = ATT_ERR_INSUFF_RESOURCE;
        }
    }

    // Send result to peer device
    gatt_srv_att_read_get_cfm(conidx, user_lid, token, status, value_size, p_buf);
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
__STATIC void htpt_cb_att_val_set(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset,
                                  co_buf_t* p_data)
{
    htpt_env_t* p_htpt_env = PRF_ENV_GET(HTPT, htpt);
    uint16_t status = PRF_APP_ERROR;
    bool send_cfm = true;

    if(p_htpt_env != NULL)
    {
        if (co_buf_data_len(p_data) != HTPT_MEAS_INTV_LEN)
        {
            status = PRF_ERR_UNEXPECTED_LEN;
        }
        else
        {
            // retrieve handle information
            switch(HTPT_IDX(hdl))
            {
                case HTS_IDX_MEAS_INTV_VAL:
                {
                    uint16_t meas_intv = co_btohs(co_read16p(co_buf_data(p_data)));

                    // check measurement length validity
                    if (   ((meas_intv >= p_htpt_env->meas_intv_min) && (meas_intv <= p_htpt_env->meas_intv_max) )
                        // notification can be disabled anyway
                        || (meas_intv == 0))
                    {
                        // check if there is no update on-going
                        if (p_htpt_env->new_meas_intv_token == GAP_INVALID_TOKEN)
                        {
                            // check if there is nothing to do
                            if(meas_intv == p_htpt_env->meas_intv)
                            {
                                status = GAP_ERR_NO_ERROR;
                            }
                            else
                            {
                                const htpt_cb_t* p_cb           = (const htpt_cb_t*) p_htpt_env->prf_env.p_cb;
                                p_htpt_env->new_meas_intv_token = token;
                                p_htpt_env->new_meas_intv       = meas_intv;
                                send_cfm                        = false;

                                // Inform application of measurement interval change request
                                p_cb->cb_meas_intv_chg_req(conidx, meas_intv);
                            }
                        }
                        else
                        {
                            status = PRF_PROC_IN_PROGRESS;
                        }
                    }
                    else
                    {
                        // value not in expected range
                        status = HTP_OUT_OF_RANGE_ERR_CODE;
                    }
                } break;

                case HTS_IDX_TEMP_MEAS_IND_CFG:
                {
                    status = htpt_update_ntf_ind_cfg(conidx, HTPT_CFG_STABLE_MEAS_IND, PRF_CLI_START_IND,
                                                     co_btohs(co_read16p(co_buf_data(p_data))));
                } break;

                case HTS_IDX_INTERM_TEMP_CFG:
                {
                    status = htpt_update_ntf_ind_cfg(conidx, HTPT_CFG_INTERM_MEAS_NTF, PRF_CLI_START_NTF,
                                                     co_btohs(co_read16p(co_buf_data(p_data))));
                } break;

                case HTS_IDX_MEAS_INTV_CFG:
                {
                    status = htpt_update_ntf_ind_cfg(conidx, HTPT_CFG_MEAS_INTV_IND, PRF_CLI_START_IND,
                                                     co_btohs(co_read16p(co_buf_data(p_data))));
                } break;

                default:
                {
                    status = ATT_ERR_REQUEST_NOT_SUPPORTED;
                } break;
            }
        }
    }

    if (send_cfm)
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
__STATIC void htpt_cb_event_sent(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    if(conidx == GAP_INVALID_CONIDX)
    {
        // Consider job done
        htpt_env_t* p_htpt_env = PRF_ENV_GET(HTPT, htpt);
        if(p_htpt_env != NULL)
        {
            p_htpt_env->op_ongoing = false;

            if (dummy != HTPT_CFG_MEAS_INTV_IND)
            {
                const htpt_cb_t* p_cb = (const htpt_cb_t*) p_htpt_env->prf_env.p_cb;
                // Inform application that event has been sent
                p_cb->cb_temp_send_cmp(status);
            }

            // continue operation execution
            htpt_exe_operation();
        }
    }
}

/// Service callback hander
__STATIC const gatt_srv_cb_t htpt_cb =
{
        .cb_event_sent    = htpt_cb_event_sent,
        .cb_att_read_get  = htpt_cb_att_read_get,
        .cb_att_event_get = NULL,
        .cb_att_info_get  = NULL,
        .cb_att_val_set   = htpt_cb_att_val_set,
};

/*
 * PROFILE NATIVE HANDLERS
 ****************************************************************************************
 */

uint16_t htpt_enable(uint8_t conidx, uint8_t ntf_ind_cfg)
{
    htpt_env_t* p_htpt_env = PRF_ENV_GET(HTPT, htpt);
    uint16_t status = PRF_ERR_REQ_DISALLOWED;

    if(p_htpt_env != NULL)
    {
        // check state of the task
        if (gapc_get_conhdl(conidx) != GAP_INVALID_CONHDL)
        {
            p_htpt_env->ntf_ind_cfg[conidx] = ntf_ind_cfg;
            status = GAP_ERR_NO_ERROR;
        }
    }

    return (status);
}


uint16_t htpt_temp_send(const htp_temp_meas_t* p_temp_meas, bool stable_meas)
{
    htpt_env_t* p_htpt_env = PRF_ENV_GET(HTPT, htpt);
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    if(p_temp_meas == NULL)
    {
        status = GAP_ERR_INVALID_PARAM;
    }
    else if(p_htpt_env != NULL)
    {
        // for intermediate measurement, feature must be enabled
        if (!stable_meas && (!GETB(p_htpt_env->features, HTPT_INTERM_TEMP_CHAR_SUP)))
        {
            status = PRF_ERR_FEATURE_NOT_SUPPORTED;
        }
        else
        {
            co_buf_t* p_buf = NULL;
            status = co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, 0, HTPT_TEMP_MEAS_MAX_LEN + GATT_BUFFER_TAIL_LEN);

            if(status == CO_BUF_ERR_NO_ERROR)
            {
                htpt_buf_meta_t* p_buf_meta = (htpt_buf_meta_t*) co_buf_metadata(p_buf);
                p_buf_meta->conidx = GAP_INVALID_CONIDX;

                // Stable measurement indication or intermediate measurement notification
                if (stable_meas)
                {
                    p_buf_meta->operation = HTPT_CFG_STABLE_MEAS_IND;
                    p_buf_meta->handle    = HTPT_HANDLE(HTS_IDX_TEMP_MEAS_VAL);
                }
                else
                {
                    p_buf_meta->operation = HTPT_CFG_INTERM_MEAS_NTF;
                    p_buf_meta->handle    = HTPT_HANDLE(HTS_IDX_INTERM_TEMP_VAL);
                }

                // Pack the temperature measurement value
                htpt_pack_temp_value(p_buf, p_temp_meas);

                // put event on wait queue
                co_list_push_back(&(p_htpt_env->wait_queue), &(p_buf->hdr));

                // execute operation
                htpt_exe_operation();
            }
            else
            {
                status = GAP_ERR_INSUFF_RESOURCES;
            }
        }
    }

    return (status);
}


uint16_t htpt_meas_intv_upd(uint16_t meas_intv)
{
    htpt_env_t* p_htpt_env = PRF_ENV_GET(HTPT, htpt);
    uint16_t status = PRF_ERR_REQ_DISALLOWED;

    if(p_htpt_env != NULL)
    {
        // Check if Measurement Interval indication is supported
        if (!GETB(p_htpt_env->features, HTPT_MEAS_INTV_CHAR_SUP))
        {
            status = PRF_ERR_FEATURE_NOT_SUPPORTED;
        }
        else if(p_htpt_env->new_meas_intv_token != GAP_INVALID_TOKEN)
        {
            status = PRF_PROC_IN_PROGRESS;
        }
        else
        {
            // update measurement interval
            p_htpt_env->meas_intv = meas_intv;

            htpt_meas_intv_ind_send(p_htpt_env, meas_intv, GAP_INVALID_CONIDX);

            status = GAP_ERR_NO_ERROR;
        }
    }

    return (status);
}

uint16_t htpt_meas_intv_chg_cfm(uint8_t conidx, uint16_t status)
{
    htpt_env_t* p_htpt_env = PRF_ENV_GET(HTPT, htpt);

    if((p_htpt_env != NULL) && (p_htpt_env->new_meas_intv_token != GAP_INVALID_TOKEN))
    {
        // send back result to peer device
        gatt_srv_att_val_set_cfm(conidx, p_htpt_env->user_lid, p_htpt_env->new_meas_intv_token, status);
        p_htpt_env->new_meas_intv_token = GAP_INVALID_TOKEN;

        if(status == GAP_ERR_NO_ERROR)
        {
            // update the current measurement interval
            p_htpt_env->meas_intv = p_htpt_env->new_meas_intv;

            htpt_meas_intv_ind_send(p_htpt_env, p_htpt_env->meas_intv, conidx);
        }
    }
    else
    {
        status = PRF_ERR_REQ_DISALLOWED;
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
 * @brief Handles reception of the @ref HTPT_ENABLE_REQ message.
 * The handler enables the Health Thermometer Profile Thermometer Role.
 * @param[in] msgid Id of the message received (probably unused).
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int htpt_enable_req_handler(ke_msg_id_t const msgid, struct htpt_enable_req const *p_param,
                                     ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = htpt_enable(p_param->conidx, p_param->ntf_ind_cfg);
    struct htpt_enable_rsp *p_rsp;

    // send response
    p_rsp = KE_MSG_ALLOC(HTPT_ENABLE_RSP, src_id, dest_id, htpt_enable_rsp);
    if(p_param != NULL)
    {
        p_rsp->conidx = p_param->conidx;
        p_rsp->status = status;
        ke_msg_send(p_rsp);
    }

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief Handles reception of the @ref HTPT_TEMP_SEND_REQ message.
 * @param[in] msgid Id of the message received (probably unused).
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int htpt_temp_send_req_handler(ke_msg_id_t const msgid,
                                      struct htpt_temp_send_req const *p_param,
                                      ke_task_id_t const dest_id,
                                      ke_task_id_t const src_id)
{
    uint16_t status = htpt_temp_send(&(p_param->temp_meas), p_param->stable_meas);

    if(status != GAP_ERR_NO_ERROR)
    {
        struct htpt_temp_send_rsp *p_rsp = KE_MSG_ALLOC(HTPT_TEMP_SEND_RSP, src_id, dest_id, htpt_temp_send_rsp);
        if(p_rsp != NULL)
        {
            p_rsp->status = status;
            ke_msg_send(p_rsp);
        }
    }

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief Request to update Measurement Interval Value
 * @param[in] msgid     Id of the message received.
 * @param[in] p_param     Pointer to the parameters of the message.
 * @param[in] dest_id   ID of the receiving task instance
 * @param[in] src_id    ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int htpt_meas_intv_upd_req_handler(ke_msg_id_t const msgid, struct htpt_meas_intv_upd_req const *p_param,
                                            ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    struct htpt_meas_intv_upd_rsp* p_rsp;
    uint16_t status = htpt_meas_intv_upd(p_param->meas_intv);

    p_rsp = KE_MSG_ALLOC(HTPT_MEAS_INTV_UPD_RSP, src_id, dest_id, htpt_meas_intv_upd_rsp);
    if(p_rsp != NULL)
    {
        p_rsp->status = status;
        ke_msg_send(p_rsp);
    }

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief Handles reception of the @ref HTPT_MEAS_INTV_UPD_CFM message.
 * @param[in] msgid Id of the message received (probably unused).
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int htpt_meas_intv_chg_cfm_handler(ke_msg_id_t const msgid, struct htpt_meas_intv_chg_cfm const *p_param,
                                            ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    htpt_meas_intv_chg_cfm(p_param->conidx, p_param->status);

    return (KE_MSG_CONSUMED);
}

/// Default State handlers definition
KE_MSG_HANDLER_TAB(htpt)
{
    // Note: all messages must be sorted in ID ascending order

    {HTPT_ENABLE_REQ,            (ke_msg_func_t) htpt_enable_req_handler},

    {HTPT_TEMP_SEND_REQ,         (ke_msg_func_t) htpt_temp_send_req_handler},
    {HTPT_MEAS_INTV_UPD_REQ,     (ke_msg_func_t) htpt_meas_intv_upd_req_handler},
    {HTPT_MEAS_INTV_CHG_CFM,     (ke_msg_func_t) htpt_meas_intv_chg_cfm_handler},
};

/**
 ****************************************************************************************
 * @brief Completion of Send temperature procedure
 *
 * @param[in] status Status of the procedure execution (@see enum hl_err)
 ****************************************************************************************
 */
__STATIC void htpt_cb_temp_send_cmp(uint16_t status)
{
//    struct htpt_temp_send_rsp *p_rsp = KE_MSG_ALLOC(HTPT_TEMP_SEND_RSP, PRF_DST_TASK(HTPT), PRF_SRC_TASK(HTPT),
//                                                    htpt_temp_send_rsp);
    struct htpt_temp_send_rsp *p_rsp = KE_MSG_ALLOC(HTPT_TEMP_SEND_RSP, TASK_APP,
    															(PRF_IDX_BUILD(PRF_ID_HTPT) | TASK_APP), htpt_temp_send_rsp);

    if(p_rsp != NULL)
    {
        p_rsp->status = status;
        ke_msg_send(p_rsp);
    }
}

/**
 ****************************************************************************************
 * @brief Inform that peer device want to update of measurement interval value
 *
 * @param[in] conidx    Connection index
 * @param[in] token     Token that must be return in confirmation
 * @param[in] meas_intv Measurement Interval value in seconds
 ****************************************************************************************
 */
__STATIC void htpt_cb_meas_intv_chg_req(uint8_t conidx, uint16_t meas_intv)
{
    // inform application that update of measurement interval is requested by peer device.
//    struct htpt_meas_intv_chg_req_ind *p_req_ind = KE_MSG_ALLOC(HTPT_MEAS_INTV_CHG_REQ_IND, PRF_DST_TASK(HTPT),
//                                                                PRF_SRC_TASK(HTPT), htpt_meas_intv_chg_req_ind);

        struct htpt_meas_intv_chg_req_ind *p_req_ind = KE_MSG_ALLOC(HTPT_MEAS_INTV_CHG_REQ_IND, TASK_APP,
        															(PRF_IDX_BUILD(PRF_ID_HTPT) | TASK_APP), htpt_meas_intv_chg_req_ind);


    if(p_req_ind != NULL)
    {
        p_req_ind->conidx = conidx;
        p_req_ind->intv   = meas_intv;
        ke_msg_send(p_req_ind);
    }
}

/**
 ****************************************************************************************
 * @brief Inform that Bond data updated for the connection.
 *
 * @param[in] conidx       Connection index
 * @param[in] ntf_ind_cfg  Notification Configuration (@see enum htpt_ntf_ind_cfg)
 ****************************************************************************************
 */
__STATIC void htpt_cb_bond_data_upd(uint8_t conidx, uint8_t ntf_ind_cfg)
{
    // inform application that notification/indication configuration has changed
//    struct htpt_cfg_indntf_ind *p_ind = KE_MSG_ALLOC(HTPT_CFG_INDNTF_IND, PRF_DST_TASK(HTPT), PRF_SRC_TASK(HTPT),
//                                                     htpt_cfg_indntf_ind);

    struct htpt_cfg_indntf_ind *p_ind = KE_MSG_ALLOC(HTPT_CFG_INDNTF_IND, TASK_APP,
    															(PRF_IDX_BUILD(PRF_ID_HTPT) | TASK_APP), htpt_cfg_indntf_ind);

    if(p_ind != NULL)
    {
        p_ind->conidx      = conidx;
        p_ind->ntf_ind_cfg = ntf_ind_cfg;
        ke_msg_send(p_ind);
    }
}

/// Default Message handle
__STATIC const htpt_cb_t htpt_msg_cb =
{
    .cb_temp_send_cmp     = htpt_cb_temp_send_cmp,
    .cb_meas_intv_chg_req = htpt_cb_meas_intv_chg_req,
    .cb_bond_data_upd     = htpt_cb_bond_data_upd,
};
#endif // (BLE_HL_MSG_API)

/*
 * PROFILE DEFAULT HANDLERS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Initialization of the HTPT module.
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
__STATIC uint16_t htpt_init(prf_data_t *p_env, uint16_t *p_start_hdl, uint8_t sec_lvl, uint8_t user_prio,
                          struct htpt_db_cfg *p_params, const htpt_cb_t* p_cb)
{
    //------------------ create the attribute database for the profile -------------------

    // DB Creation Status
    uint16_t status = GAP_ERR_NO_ERROR;
    uint8_t user_lid = GATT_INVALID_USER_LID;

    do
    {
        htpt_env_t* p_htpt_env;
        // Service content flag
        uint32_t cfg_bf;

        #if (BLE_HL_MSG_API)
        if(p_cb == NULL)
        {
            p_cb = &(htpt_msg_cb);
        }
        #endif // (BLE_HL_MSG_API)

        if(   (p_params == NULL) || (p_start_hdl == NULL) || (p_cb == NULL) || (p_cb->cb_temp_send_cmp == NULL)
           || (p_cb->cb_meas_intv_chg_req == NULL) || (p_cb->cb_bond_data_upd == NULL))
        {
            status = GAP_ERR_INVALID_PARAM;
            break;
        }

        // register HTPT user
        status = gatt_user_srv_register(L2CAP_LE_MTU_MIN, user_prio, &htpt_cb, &user_lid);
        if(status != GAP_ERR_NO_ERROR) break;

        // Compute Attribute Table and save it in environment
        cfg_bf = htpt_compute_cfg_flag(p_params->features);

        // Add GAP service
        status = gatt_db_svc16_add(user_lid, sec_lvl, GATT_SVC_HEALTH_THERMOM, HTS_IDX_NB,
                                   (uint8_t *)&cfg_bf, &(htpt_att_db[0]), HTS_IDX_NB, p_start_hdl);
        if(status != GAP_ERR_NO_ERROR) break;

        //-------------------- allocate memory required for the profile  ---------------------
        p_htpt_env = (htpt_env_t *) ke_malloc(sizeof(htpt_env_t), KE_MEM_ATT_DB);

        if(p_htpt_env != NULL)
        {
            // allocate HTPT required environment variable
            p_env->p_env = (prf_hdr_t *) p_htpt_env;

            p_htpt_env->start_hdl           = *p_start_hdl;
            p_htpt_env->features            = p_params->features;
            p_htpt_env->user_lid            = user_lid;
            p_htpt_env->features            = p_params->features;
            p_htpt_env->meas_intv           = p_params->meas_intv;
            p_htpt_env->meas_intv_min       = p_params->valid_range_min;
            p_htpt_env->meas_intv_max       = p_params->valid_range_max;
            p_htpt_env->temp_type           = p_params->temp_type;
            p_htpt_env->new_meas_intv_token = GAP_INVALID_TOKEN;
            p_htpt_env->op_ongoing          = false;
            p_htpt_env->in_exe_op           = false;
            memset(p_htpt_env->ntf_ind_cfg, 0 , sizeof(p_htpt_env->ntf_ind_cfg));
            co_list_init(&(p_htpt_env->wait_queue));

            // Update measurement interval permissions
            if (GETB(p_params->features, HTPT_MEAS_INTV_CHAR_SUP))
            {
                uint16_t perm = PROP(RD);

                // Check if Measurement Interval Char. supports indications
                if (GETB(p_params->features, HTPT_MEAS_INTV_IND_SUP))
                {
                    perm |= PROP(I);
                }

                // Check if Measurement Interval Char. is writable
                if (GETB(p_params->features, HTPT_MEAS_INTV_WR_SUP))
                {
                    perm |= SEC_LVL(WP, NO_AUTH)|PROP(WR);
                }

                gatt_db_att_info_set(user_lid, HTPT_HANDLE(HTS_IDX_MEAS_INTV_VAL), perm);
            }

            // initialize profile environment variable
            p_htpt_env->prf_env.p_cb     = p_cb;
            #if (BLE_HL_MSG_API)
            p_env->desc.msg_handler_tab  = htpt_msg_handler_tab;
            p_env->desc.msg_cnt          = ARRAY_LEN(htpt_msg_handler_tab);
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
__STATIC uint16_t htpt_destroy(prf_data_t *p_env, uint8_t reason)
{
    uint16_t status = GAP_ERR_NO_ERROR;
    htpt_env_t* p_htpt_env = (htpt_env_t* ) p_env->p_env;

    if(reason != PRF_DESTROY_RESET)
    {
        status = gatt_user_unregister(p_htpt_env->user_lid);
    }

    if(status == GAP_ERR_NO_ERROR)
    {
        if(reason != PRF_DESTROY_RESET)
        {
            // remove buffer in wait queue
            while(!co_list_is_empty(&p_htpt_env->wait_queue))
            {
                co_buf_t* p_buf = (co_buf_t*) co_list_pop_front(&p_htpt_env->wait_queue);
                co_buf_release(p_buf);
            }
        }

        p_env->p_env = NULL;
        ke_free(p_htpt_env);
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
__STATIC void htpt_con_create(prf_data_t *p_env, uint8_t conidx, const gap_con_param_t* p_con_param)
{
    // Clear configuration for this connection
    htpt_env_t* p_htpt_env = (htpt_env_t* ) p_env->p_env;

    p_htpt_env->ntf_ind_cfg[conidx] = 0;
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
__STATIC void htpt_con_cleanup(prf_data_t *p_env, uint8_t conidx, uint16_t reason)
{
    // Clear configuration for this connection
    htpt_env_t* p_htpt_env = (htpt_env_t* ) p_env->p_env;

    p_htpt_env->ntf_ind_cfg[conidx] = 0;
}



/// HTPT Task interface required by profile manager
const prf_task_cbs_t htpt_itf =
{
    .cb_init          = (prf_init_cb) htpt_init,
    .cb_destroy       = htpt_destroy,
    .cb_con_create    = htpt_con_create,
    .cb_con_cleanup   = htpt_con_cleanup,
    .cb_con_upd       = NULL,
};

/**
 ****************************************************************************************
 * @brief Retrieve Service profile interface
 *
 * @return Service profile interface
 ****************************************************************************************
 */
const prf_task_cbs_t *htpt_prf_itf_get(void)
{
    return &htpt_itf;
}

//#endif //BLE_HT_THERMOM

/// @} HTPT

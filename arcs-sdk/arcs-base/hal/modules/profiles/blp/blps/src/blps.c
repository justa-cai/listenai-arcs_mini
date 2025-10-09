/**
 ****************************************************************************************
 *
 * @file blps.c
 *
 * @brief Blood Pressure Profile Sensor implementation.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup BLPS Blood Pressure Profile Sensor
 * @ingroup BLP
 * @brief Blood Pressure Profile Sensor
 *
 * Blood pressure sensor (BPS) profile provides functionalities to upper layer module
 * application. The device using this profile takes the role of Blood pressure sensor.
 *
 * The interface of this role to the Application is:
 *  - Enable the profile role (from APP)
 *  - Disable the profile role (from APP)
 *  - Notify peer device during Blood Pressure measurement (from APP)
 *  - Indicate measurements performed to peer device (from APP)
 *
 * Profile didn't manages multiple users configuration and storage of offline measurements.
 * This must be handled by application.
 *
 * Blood Pressure Profile Sensor. (BLPS): A BLPS (e.g. PC, phone, etc)
 * is the term used by this profile to describe a device that can perform blood pressure
 * measurement and notify about on-going measurement and indicate final result to a peer
 * BLE device.
 *
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "btip_config.h"

#if (BLE_BP_SENSOR)
#include "blps.h"
#include "gap.h"
#include "gatt.h"

#include "co_utils.h"
#include "co_endian.h"

#include <string.h>
#include "ke_mem.h"

/*
 * DEFINES
 ****************************************************************************************
 */

///BPS Configuration Flag Masks
#define BLPS_MANDATORY_MASK              (0x003F)
#define BLPS_INTM_CUFF_PRESS_MASK        (0x01C0)

/// indication/notification config mask
#define BLPS_NTFIND_MASK                 (0x06)

/// BPS Attributes database handle list
enum blps_att_db_handles
{
    BPS_IDX_SVC,

    BPS_IDX_BP_MEAS_CHAR,
    BPS_IDX_BP_MEAS_VAL,
    BPS_IDX_BP_MEAS_IND_CFG,

    BPS_IDX_BP_FEATURE_CHAR,
    BPS_IDX_BP_FEATURE_VAL,

    BPS_IDX_INTM_CUFF_PRESS_CHAR,
    BPS_IDX_INTM_CUFF_PRESS_VAL,
    BPS_IDX_INTM_CUFF_PRESS_NTF_CFG,

    BPS_IDX_NB,
};

///Characteristic Codes
enum
{
    BPS_BP_MEAS_CHAR,
    BPS_INTM_CUFF_MEAS_CHAR,
    BPS_BP_FEATURE_CHAR,
};

/// Database Configuration Bit Field Flags
enum blps_db_config_bf
{
    /// support of Intermediate Cuff Pressure
    BLPS_INTM_CUFF_PRESS_SUP_POS = 0,
    BLPS_INTM_CUFF_PRESS_SUP_BIT = 0x01,
};

/// Indication/notification configuration (put in feature flag to optimize memory usage)
enum blps_indntf_config_bf
{
    /// Bit used to know if blood pressure measurement indication is enabled
    BLPS_BP_MEAS_IND_CFG_POS         = 1,
    BLPS_BP_MEAS_IND_CFG_BIT         = 0x02,
    /// Bit used to know if cuff pressure measurement notification is enabled
    BLPS_INTM_CUFF_PRESS_NTF_CFG_POS = 2,
    BLPS_INTM_CUFF_PRESS_NTF_CFG_BIT = 0x04,
};

/*
 * TYPES DEFINITION
 ****************************************************************************************
 */


/// Blood pressure server environment variable
typedef struct blps_env
{
    /// profile environment
    prf_hdr_t   prf_env;
    /// Service Attribute Start Handle
    uint16_t    start_hdl;
    /// Services features
    uint16_t    features;
    /// Profile configuration flags
    uint8_t     prfl_cfg;
    /// GATT user local identifier
    uint8_t     user_lid;
    /// Environment variable pointer for each connections
    uint8_t     ntf_ind_cfg[BLE_CONNECTION_MAX];

} blps_env_t;


/*
 * ATTRIBUTES DATABASE
 ****************************************************************************************
 */

/// Full BLPS Database Description - Used to add attributes into the database
const gatt_att16_desc_t blps_att_db[BPS_IDX_NB] =
{
    // Blood Pressure Service Declaration
    [BPS_IDX_SVC] =                     {GATT_DECL_PRIMARY_SERVICE,            PROP(RD),            0                    },

    // Blood Pressure Measurement Characteristic Declaration
    [BPS_IDX_BP_MEAS_CHAR] =            {GATT_DECL_CHARACTERISTIC,             PROP(RD),            0                    },
    // Blood Pressure Measurement Characteristic Value
    [BPS_IDX_BP_MEAS_VAL] =             {GATT_CHAR_BLOOD_PRESSURE_MEAS,        PROP(I),             BLS_BP_MEAS_MAX_LEN  },
    // Blood Pressure Measurement Characteristic - Client Characteristic Configuration Descriptor
    [BPS_IDX_BP_MEAS_IND_CFG] =         {GATT_DESC_CLIENT_CHAR_CFG,            PROP(RD) | PROP(WR), OPT(NO_OFFSET)       },

    // Blood Pressure Feature Characteristic Declaration
    [BPS_IDX_BP_FEATURE_CHAR] =         {GATT_DECL_CHARACTERISTIC,             PROP(RD),            0                    },
    // Blood Pressure Feature Characteristic Value
    [BPS_IDX_BP_FEATURE_VAL] =          {GATT_CHAR_BLOOD_PRESSURE_FEATURE,     PROP(RD),            OPT(NO_OFFSET)       },

    // Intermediate Cuff Pressure Characteristic Declaration
    [BPS_IDX_INTM_CUFF_PRESS_CHAR] =    {GATT_DECL_CHARACTERISTIC,             PROP(RD),            0                    },
    // Intermediate Cuff Pressure Characteristic Value
    [BPS_IDX_INTM_CUFF_PRESS_VAL] =     {GATT_CHAR_INTERMEDIATE_CUFF_PRESSURE, PROP(N),             BLS_BP_MEAS_MAX_LEN  },
    // Intermediate Cuff Pressure Characteristic - Client Characteristic Configuration Descriptor
    [BPS_IDX_INTM_CUFF_PRESS_NTF_CFG] = {GATT_DESC_CLIENT_CHAR_CFG,            PROP(RD) | PROP(WR), OPT(NO_OFFSET)       },
};

/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Pack Blood Pressure measurement value
 *
 * @param[in] p_blps_env  Pointer to environment
 * @param[in] p_buf       Pointer to output buffer
 * @param[in] p_meas      Pointer to  Blood Pressure measurement value
 ****************************************************************************************
 */
__STATIC void blps_pack_meas_value(blps_env_t *p_blps_env, co_buf_t* p_buf, const bps_bp_meas_t *p_meas)
{

    co_buf_tail(p_buf)[0] = p_meas->flags;
    co_buf_tail_reserve(p_buf, 1);

    // Blood Pressure Measurement Compound Value - Systolic
    co_write16p(co_buf_tail(p_buf), co_htobs(p_meas->systolic));
    co_buf_tail_reserve(p_buf, 2);

    // Blood Pressure Measurement Compound Value - Diastolic (mmHg)
    co_write16p(co_buf_tail(p_buf), co_htobs(p_meas->diastolic));
    co_buf_tail_reserve(p_buf, 2);

    //  Blood Pressure Measurement Compound Value - Mean Arterial Pressure (mmHg)
    co_write16p(co_buf_tail(p_buf), co_htobs(p_meas->mean_arterial_pressure));
    co_buf_tail_reserve(p_buf, 2);


    // time flag set
    if (GETB(p_meas->flags, BPS_MEAS_FLAG_TIME_STAMP))
    {
        prf_pack_date_time(p_buf, &(p_meas->time_stamp));
    }

    // Pulse rate flag set
    if (GETB(p_meas->flags, BPS_MEAS_PULSE_RATE))
    {
        co_write16p(co_buf_tail(p_buf), co_htobs(p_meas->pulse_rate));
        co_buf_tail_reserve(p_buf, 2);
    }

    // User ID flag set
    if (GETB(p_meas->flags, BPS_MEAS_USER_ID))
    {
        co_buf_tail(p_buf)[0] = p_meas->user_id;
        co_buf_tail_reserve(p_buf, 1);
    }

    // Measurement status flag set
    if (GETB(p_meas->flags, BPS_MEAS_MEAS_STATUS))
    {
        // To avoid modify the value pointed by p_meas
        uint16_t meas_status_temp = p_meas->meas_status;

        // If feature is not supported, the corresponding Measurement status bit(s) shall be set to 0
        if (!GETB(p_blps_env->features, BPS_F_BODY_MVMT_DETECT_SUP))
        {
            SETB(meas_status_temp, BPS_STATUS_MVMT_DETECT, 0);
        }

        if (!GETB(p_blps_env->features, BPS_F_CUFF_FIT_DETECT_SUP))
        {
            SETB(meas_status_temp, BPS_STATUS_CUFF_FIT_DETECT, 0);
        }

        if (!GETB(p_blps_env->features, BPS_F_IRREGULAR_PULSE_DETECT_SUP))
        {
            SETB(meas_status_temp, BPS_STATUS_IRREGULAR_PULSE_DETECT, 0);
        }

        if (!GETB(p_blps_env->features, BPS_F_PULSE_RATE_RANGE_DETECT_SUP))
        {
            SETB(meas_status_temp, BPS_STATUS_PR_RANGE_DETECT_LSB, 0);
            SETB(meas_status_temp, BPS_STATUS_PR_RANGE_DETECT_MSB, 0);
        }

        if (!GETB(p_blps_env->features, BPS_F_MEAS_POS_DETECT_SUP))
        {
            SETB(meas_status_temp, BPS_STATUS_MEAS_POS_DETECT, 0);
        }

        co_write16p(co_buf_tail(p_buf), co_htobs(meas_status_temp));
        co_buf_tail_reserve(p_buf, 2);
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
__STATIC void blps_cb_att_read_get(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset,
                                   uint16_t max_length)
{
    blps_env_t *p_blps_env = PRF_ENV_GET(BLPS, blps);
    // retrieve value attribute
    co_buf_t* p_buf       = NULL;
    uint16_t  status      = GAP_ERR_NO_ERROR;

    if(p_blps_env != NULL)
    {
        status = co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, sizeof(uint16_t), GATT_BUFFER_TAIL_LEN);

        if(status == GAP_ERR_NO_ERROR)
        {
            switch (hdl - p_blps_env->start_hdl)
            {
                case BPS_IDX_BP_FEATURE_VAL:
                {
                    co_write16p(co_buf_data(p_buf), co_htobs(p_blps_env->features));
                } break;

                case BPS_IDX_BP_MEAS_IND_CFG:
                {
                    uint16_t ind_cfg = GETB(p_blps_env->ntf_ind_cfg[conidx], BLPS_BP_MEAS_IND_CFG)
                                     ? PRF_CLI_START_IND : PRF_CLI_STOP_NTFIND;
                    co_write16p(co_buf_data(p_buf), co_htobs(ind_cfg));
                } break;

                case BPS_IDX_INTM_CUFF_PRESS_NTF_CFG:
                {
                    uint16_t ntf_cfg = GETB(p_blps_env->ntf_ind_cfg[conidx], BLPS_INTM_CUFF_PRESS_NTF_CFG)
                                     ? PRF_CLI_START_NTF : PRF_CLI_STOP_NTFIND;
                    co_write16p(co_buf_data(p_buf), co_htobs(ntf_cfg));
                } break;

                default: { status = ATT_ERR_REQUEST_NOT_SUPPORTED; } break;
            }
        }
        else
        {
            status = ATT_ERR_INSUFF_RESOURCE;
        }
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
__STATIC void blps_cb_att_val_set(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset,
                        co_buf_t* p_data)
{
    blps_env_t *p_blps_env = PRF_ENV_GET(BLPS, blps);
    uint16_t  status      = PRF_APP_ERROR;

    if((p_blps_env != NULL) || (co_buf_data_len(p_data) != sizeof(uint16_t)))
    {
        const blps_cb_t* p_cb  = (const blps_cb_t*) p_blps_env->prf_env.p_cb;
        uint16_t value = co_btohs(co_read16p(co_buf_data(p_data)));

        switch (hdl - p_blps_env->start_hdl)
        {
            case BPS_IDX_BP_MEAS_IND_CFG:
            {
                if ((value != PRF_CLI_STOP_NTFIND) && (value != PRF_CLI_START_IND)) break;

                SETB(p_blps_env->ntf_ind_cfg[conidx], BLPS_BP_MEAS_IND_CFG, (value == PRF_CLI_START_IND));

                // Inform application about bond data update
                p_cb->cb_bond_data_upd(conidx, BPS_BP_MEAS_CHAR, value);
                status = GAP_ERR_NO_ERROR;
            } break;

            case BPS_IDX_INTM_CUFF_PRESS_NTF_CFG:
            {
                if ((value != PRF_CLI_STOP_NTFIND) && (value != PRF_CLI_START_NTF)) break;

                SETB(p_blps_env->ntf_ind_cfg[conidx], BLPS_INTM_CUFF_PRESS_NTF_CFG, (value == PRF_CLI_START_NTF));

                // Inform application about bond data update
                p_cb->cb_bond_data_upd(conidx, BPS_INTM_CUFF_MEAS_CHAR, value);
                status = GAP_ERR_NO_ERROR;
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
__STATIC void blps_cb_event_sent(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    blps_env_t *p_blps_env = PRF_ENV_GET(BLPS, blps);

    if((p_blps_env != NULL))
    {
        const blps_cb_t* p_cb  = (const blps_cb_t*) p_blps_env->prf_env.p_cb;
        p_cb->cb_meas_send_cmp(conidx, status);
    }
}

/// Service callback hander
__STATIC const gatt_srv_cb_t blps_cb =
{
        .cb_event_sent    = blps_cb_event_sent,
        .cb_att_read_get  = blps_cb_att_read_get,
        .cb_att_event_get = NULL,
        .cb_att_info_get  = NULL,
        .cb_att_val_set   = blps_cb_att_val_set,
};

/*
 * PROFILE NATIVE HANDLERS
 ****************************************************************************************
 */

uint16_t blps_enable(uint8_t conidx, uint16_t bp_meas_ind_en, uint16_t interm_cp_ntf_en)
{
    blps_env_t *p_blps_env = PRF_ENV_GET(BLPS, blps);
    uint16_t status = PRF_ERR_REQ_DISALLOWED;

    if(p_blps_env != NULL)
    {
        // check state of the task
        if (gapc_get_conhdl(conidx) != GAP_INVALID_CONHDL)
        {
            if (interm_cp_ntf_en == PRF_CLI_START_NTF)
            {
                // Enable Bonded Data
                SETB(p_blps_env->ntf_ind_cfg[conidx], BLPS_INTM_CUFF_PRESS_NTF_CFG, 1);
            }

            if (bp_meas_ind_en == PRF_CLI_START_IND)
            {
                // Enable Bonded Data
                SETB(p_blps_env->ntf_ind_cfg[conidx], BLPS_BP_MEAS_IND_CFG, 1);
            }
            status = GAP_ERR_NO_ERROR;
        }
    }

    return (status);
}

uint16_t blps_meas_send(uint8_t conidx, bool stable, const bps_bp_meas_t* p_meas)
{
    blps_env_t *p_blps_env = PRF_ENV_GET(BLPS, blps);
    uint16_t status = PRF_ERR_REQ_DISALLOWED;

    do
    {
        co_buf_t* p_buf = NULL;
        uint16_t  hdl;

        // Parameter sanity check
        if(p_blps_env == NULL) break;

        if(p_meas == NULL)
        {
            status = PRF_ERR_INVALID_PARAM;
            break;
        }

        if (stable)
        {
             if (!GETB(p_blps_env->ntf_ind_cfg[conidx], BLPS_BP_MEAS_IND_CFG))
            {
                status = PRF_ERR_IND_DISABLED;
                break;
            }
        }
        else
        {
            if(!GETB(p_blps_env->prfl_cfg, BLPS_INTM_CUFF_PRESS_SUP))
            {
                status = PRF_ERR_FEATURE_NOT_SUPPORTED;
                break;
            }

            if (!GETB(p_blps_env->ntf_ind_cfg[conidx], BLPS_INTM_CUFF_PRESS_NTF_CFG))
            {
                status = PRF_ERR_NTF_DISABLED;
                break;
            }
        }

        // allocate notification / indication buffer
        status = co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, 0, BLS_BP_MEAS_MAX_LEN + GATT_BUFFER_TAIL_LEN);
        if(status != GAP_ERR_NO_ERROR)
        {
            status = GAP_ERR_INSUFF_RESOURCES;
            break;
        }

        // compute targeted handle
        hdl = p_blps_env->start_hdl + (stable ? BPS_IDX_BP_MEAS_VAL : BPS_IDX_INTM_CUFF_PRESS_VAL);

        // Pack the BP Measurement value
        blps_pack_meas_value(p_blps_env, p_buf, p_meas);

        status = gatt_srv_event_send(conidx, p_blps_env->user_lid, 0, (stable ? GATT_INDICATE : GATT_NOTIFY),
                                     hdl, p_buf);
        co_buf_release(p_buf);
    } while(0);

    return (status);
}



#if (BLE_HL_MSG_API)
/*
 * PROFILE MSG HANDLERS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Send a BLPS_CMP_EVT message to the task which enabled the profile
 *
 * @param[in] conidx       Connection Identifier
 * @param[in] operation    Indicates the operation for which the cmp_evt is being sent.
 * @param[in] status       Indicates the outcome of the operation
 ****************************************************************************************
 */
void blps_send_cmp_evt(uint8_t conidx, uint8_t operation, uint16_t status)
{
    struct blps_cmp_evt *p_evt = NULL;

    // Send the message
    p_evt = KE_MSG_ALLOC(BLPS_CMP_EVT,PRF_DST_TASK(BLPS), PRF_SRC_TASK(BLPS), blps_cmp_evt);

    if(p_evt != NULL)
    {
        p_evt->conidx     = conidx;
        p_evt->operation  = operation;
        p_evt->status     = status;

        ke_msg_send(p_evt);
    }
}

/**
 ****************************************************************************************
 * @brief Handles reception of the @ref BLPS_ENABLE_REQ message.
 * The handler enables the Blood Pressure Sensor Profile and initialize readable values.
 * @param[in] msgid Id of the message received (probably unused).off
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int blps_enable_req_handler(ke_msg_id_t const msgid, struct blps_enable_req const *p_param,
                                     ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    struct blps_enable_rsp *p_rsp;
    // Request status
    uint16_t status = blps_enable(p_param->conidx, p_param->bp_meas_ind_en, p_param->interm_cp_ntf_en);

    // send response to application
    p_rsp         = KE_MSG_ALLOC(BLPS_ENABLE_RSP, src_id, dest_id, blps_enable_rsp);
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
 * @brief Handles reception of the @ref BLPS_MEAS_SEND_CMD message.
 * @param[in] msgid Id of the message received (probably unused).
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int blps_meas_send_cmd_handler(ke_msg_id_t const msgid, struct blps_meas_send_cmd const *p_param,
                                        ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    // Request status
    uint16_t status = blps_meas_send(p_param->conidx, (p_param->flag_interm_cp == BPS_STABLE_MEAS), &(p_param->meas_val));

    if(status != GAP_ERR_NO_ERROR)
    {
        blps_send_cmp_evt(p_param->conidx, BLPS_MEAS_SEND_CMD_OP_CODE, status);
    }

    return (KE_MSG_CONSUMED);
}

/// Default State handlers definition
KE_MSG_HANDLER_TAB(blps)
{
    // Note: all messages must be sorted in ID ascending order

    { BLPS_ENABLE_REQ,       (ke_msg_func_t) blps_enable_req_handler    },
    { BLPS_MEAS_SEND_CMD,    (ke_msg_func_t) blps_meas_send_cmd_handler },
};

/**
 ****************************************************************************************
 * @brief Completion of BCS sensor measurement transmission
 *
 * @param[in] status Status of the procedure execution (@see enum hl_err)
 ****************************************************************************************
 */
void blps_cb_meas_send_cmp(uint8_t conidx, uint16_t status)
{
    blps_send_cmp_evt(conidx, BLPS_MEAS_SEND_CMD_OP_CODE, status);
}

/**
 ****************************************************************************************
 * @brief Inform that bond data updated for the connection.
 *
 * @param[in] conidx        Connection index
 * @param[in] char_code     Own code for differentiating between Blood Pressure Measurement,
 *                          and Intermediate Cuff Pressure Measurement characteristics
 * @param[in] cfg_val       Stop/notify/indicate value to configure into the peer characteristic
 ****************************************************************************************
 */
void blps_cb_bond_data_upd(uint8_t conidx, uint8_t char_code, uint16_t cfg_val)
{
    struct blps_cfg_indntf_ind *p_ind;

    // Send the message
    p_ind = KE_MSG_ALLOC(BLPS_CFG_INDNTF_IND,PRF_DST_TASK(BLPS), PRF_SRC_TASK(BLPS), blps_cfg_indntf_ind);

    if(p_ind != NULL)
    {
        p_ind->conidx     = conidx;
        p_ind->char_code  = char_code;
        p_ind->cfg_val    = cfg_val;

        ke_msg_send(p_ind);
    }
}

/// Default Message handle
__STATIC const blps_cb_t blps_msg_cb =
{
    .cb_meas_send_cmp = blps_cb_meas_send_cmp,
    .cb_bond_data_upd = blps_cb_bond_data_upd,
};
#endif // (BLE_HL_MSG_API)



/*
 * PROFILE DEFAULT HANDLERS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Initialization of the BLPS module.
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
__STATIC uint16_t blps_init(prf_data_t *p_env, uint16_t *p_start_hdl, uint8_t sec_lvl, uint8_t user_prio,
                          struct blps_db_cfg *p_params, const blps_cb_t* p_cb)
{
    //------------------ create the attribute database for the profile -------------------

    // DB Creation Status
    uint16_t status = GAP_ERR_NO_ERROR;
    uint8_t user_lid = GATT_INVALID_USER_LID;

    do
    {
        blps_env_t* p_blps_env;
        // Service content flag
        uint32_t cfg_flag = BLPS_MANDATORY_MASK;

        #if (BLE_HL_MSG_API)
        if(p_cb == NULL)
        {
            p_cb = &(blps_msg_cb);
        }
        #endif // (BLE_HL_MSG_API)

        if(   (p_params == NULL) || (p_start_hdl == NULL) || (p_cb == NULL) || (p_cb->cb_meas_send_cmp == NULL)
           || (p_cb->cb_bond_data_upd == NULL))
        {
            status = GAP_ERR_INVALID_PARAM;
            break;
        }

        // register BLPS user
        status = gatt_user_srv_register(L2CAP_LE_MTU_MIN, user_prio, &blps_cb, &user_lid);
        if(status != GAP_ERR_NO_ERROR) break;


        //Set Configuration Flag Value
        if (GETB(p_params->prfl_cfg, BLPS_INTM_CUFF_PRESS_SUP))
        {
            cfg_flag |= BLPS_INTM_CUFF_PRESS_MASK;
        }

        // Add GAP service
        status = gatt_db_svc16_add(user_lid, sec_lvl, GATT_SVC_BLOOD_PRESSURE, BPS_IDX_NB,
                                   (uint8_t *)&cfg_flag, &(blps_att_db[0]), BPS_IDX_NB, p_start_hdl);
        if(status != GAP_ERR_NO_ERROR) break;

        //-------------------- allocate memory required for the profile  ---------------------
        p_blps_env = (blps_env_t *) ke_malloc(sizeof(blps_env_t), KE_MEM_ATT_DB);

        if(p_blps_env != NULL)
        {
            // allocate BLPS required environment variable
            p_env->p_env = (prf_hdr_t *) p_blps_env;
            p_blps_env->start_hdl = *p_start_hdl;
            p_blps_env->features  = p_params->features;
            p_blps_env->prfl_cfg  = p_params->prfl_cfg;
            p_blps_env->user_lid  = user_lid;
            memset(p_blps_env->ntf_ind_cfg, 0, BLE_CONNECTION_MAX);

            // initialize profile environment variable
            p_blps_env->prf_env.p_cb     = p_cb;
            #if (BLE_HL_MSG_API)
            p_env->desc.msg_handler_tab  = blps_msg_handler_tab;
            p_env->desc.msg_cnt          = ARRAY_LEN(blps_msg_handler_tab);
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
__STATIC uint16_t blps_destroy(prf_data_t *p_env, uint8_t reason)
{
    uint16_t status = GAP_ERR_NO_ERROR;
    blps_env_t *p_blps_env = (blps_env_t *) p_env->p_env;

    if(reason != PRF_DESTROY_RESET)
    {
        status = gatt_user_unregister(p_blps_env->user_lid);
    }

    if(status == GAP_ERR_NO_ERROR)
    {
        // free profile environment variables
        p_env->p_env = NULL;
        ke_free(p_blps_env);
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
__STATIC void blps_con_create(prf_data_t *p_env, uint8_t conidx, const gap_con_param_t* p_con_param)
{
    blps_env_t *p_blps_env = (blps_env_t *) p_env->p_env;
    p_blps_env->ntf_ind_cfg[conidx] = 0;
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
__STATIC void blps_con_cleanup(prf_data_t *p_env, uint8_t conidx, uint16_t reason)
{
    blps_env_t *p_blps_env = (blps_env_t *) p_env->p_env;
    // clean-up environment variable allocated for task instance
    p_blps_env->ntf_ind_cfg[conidx] = 0;
}

/// BLPS Task interface required by profile manager
const prf_task_cbs_t blps_itf =
{
    .cb_init          = (prf_init_cb) blps_init,
    .cb_destroy       = blps_destroy,
    .cb_con_create    = blps_con_create,
    .cb_con_cleanup   = blps_con_cleanup,
    .cb_con_upd       = NULL,
};

/**
 ****************************************************************************************
 * @brief Retrieve service profile interface
 *
 * @return service profile interface
 ****************************************************************************************
 */
const prf_task_cbs_t *blps_prf_itf_get(void)
{
    return &blps_itf;
}

#endif /* BLE_BP_SENSOR */

/// @} BLPS

/**
 ****************************************************************************************
 *
 * @file bass.c
 *
 * @brief Battery Server Implementation.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup BASS
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#define BLE_BATT_SERVER 1
#if (BLE_BATT_SERVER)
#include "bass.h"

#include <string.h>

/*
 * DEFINES
 ****************************************************************************************
 */
///Maximum number of Battery Server task instances

#define BAS_CFG_FLAG_MANDATORY_MASK       (0x07)
#define BAS_CFG_FLAG_NTF_SUP_MASK         (0x08)
#define BAS_CFG_FLAG_MTP_BAS_MASK         (0x10)

#define BASS_FLAG_NTF_CFG_BIT             (0x02)

/// Maximal length for Characteristic values - 128 bytes
#define BASS_VAL_MAX_LEN               (128)

/// Battery Service Attributes Indexes
enum
{
    BAS_IDX_SVC,

    BAS_IDX_BATT_LVL_CHAR,
    BAS_IDX_BATT_LVL_VAL,
    BAS_IDX_BATT_LVL_NTF_CFG,
    BAS_IDX_BATT_LVL_PRES_FMT,

    BAS_IDX_NB,
};

/*
 * TYPES DEFINITION
 ****************************************************************************************
 */


/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */
struct bass_env bass_env;
bass_env_t* p_bass_env = &bass_env;


/*
 * ATTRIBUTES DATABASE
 ****************************************************************************************
 */

/// Full Database Description - Used to add attributes into the database
const ble_gatt_att16_desc_t bass_att_db[BAS_IDX_NB] =
{
    // Battery Service Declaration
    [BAS_IDX_SVC]               = { BLE_GATT_DECL_PRIMARY_SERVICE,  BLE_PROP(RD),          0              },
    // Battery Level Characteristic Declaration
    [BAS_IDX_BATT_LVL_CHAR]     = { BLE_GATT_DECL_CHARACTERISTIC,   BLE_PROP(RD),          0              },
    // Battery Level Characteristic Value
    [BAS_IDX_BATT_LVL_VAL]      = { BLE_GATT_CHAR_BATTERY_LEVEL,    BLE_PROP(RD),          0 },
    // Battery Level Characteristic - Client Characteristic Configuration Descriptor
    [BAS_IDX_BATT_LVL_NTF_CFG]  = { BLE_GATT_DESC_CLIENT_CHAR_CFG,  BLE_PROP(RD)|BLE_PROP(WR), 0 },
    // Battery Level Characteristic - Characteristic Presentation Format Descriptor
    [BAS_IDX_BATT_LVL_PRES_FMT] = { BLE_GATT_DESC_CHAR_PRES_FORMAT, BLE_PROP(RD),          0 },
};


/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Retrieve Attribute handle from service and attribute index
 *
 * @param[in] svc_idx BAS Service index
 * @param[in] att_idx Attribute index
 *
 * @return BAS attribute handle or INVALID HANDLE if nothing found
 ****************************************************************************************
 */
static uint16_t bass_get_att_handle(uint8_t svc_idx, uint8_t att_idx)
{
    uint16_t    handle = BLE_GATT_INVALID_HDL;

    if (svc_idx < p_bass_env ->nb_svc)
    {
        // full service size is reserved indatabase
        handle = p_bass_env->start_hdl + (BAS_IDX_NB * svc_idx) + att_idx;

        // sanity check
        if(   ((att_idx == BAS_IDX_BATT_LVL_NTF_CFG) && (((p_bass_env->features >> svc_idx) & 0x01) != BAS_BATT_LVL_NTF_SUP))
           || ((att_idx == BAS_IDX_BATT_LVL_PRES_FMT) && (p_bass_env->nb_svc < 2)))
        {
            handle = BLE_GATT_INVALID_HDL;
        }
        // update handle if battery level notification not present
        else if ((att_idx > BAS_IDX_BATT_LVL_NTF_CFG) && (((p_bass_env->features >> svc_idx) & 0x01) != BAS_BATT_LVL_NTF_SUP))
        {
            handle -= 1;
        }
    }

    return handle;
}

/**
 ****************************************************************************************
 * @brief Retrieve Service and attribute index form attribute handle
 *
 * @param[out] handle     Attribute handle
 * @param[out] p_svc_idx  BAS Service index
 * @param[out] p_att_idx  Attribute index
 *
 * @return Success if attribute and service index found, else Application error
 ****************************************************************************************
 */
static uint16_t bass_get_att_idx(uint16_t handle, uint8_t *p_svc_idx, uint8_t *p_att_idx)
{
    uint16_t hdl_cursor = p_bass_env->start_hdl;
    uint16_t status = BLE_PRF_APP_ERROR;

    // Browse list of services
    // handle must be greater than current index
    for (*p_svc_idx = 0; (*p_svc_idx < p_bass_env->nb_svc) && (handle >= hdl_cursor); (*p_svc_idx)++)
    {
        // check if handle is within service range
        if (handle <= (hdl_cursor + BAS_IDX_NB))
        {
            *p_att_idx = handle - hdl_cursor;

            // check if notification are present
            if ((*p_att_idx >= BAS_IDX_BATT_LVL_NTF_CFG) && ((p_bass_env->features >> *p_svc_idx) & 0x01) != BAS_BATT_LVL_NTF_SUP)
            {
                *p_att_idx += 1;
            }

            // If Battery level presentation format but should be not present, there is an error
            if((*p_att_idx == BAS_IDX_BATT_LVL_PRES_FMT) && (p_bass_env->nb_svc == 1))
            {
                break;
            }

            // search succeed
            status = BLE_GAP_ERR_NO_ERROR;
            break;
        }

        hdl_cursor += BAS_IDX_NB;
    }

    return (status);
}


/**
 ****************************************************************************************
 * @brief  Trigger battery level notification
 *
 * @param p_bass_env profile environment
 * @param conidx     peer destination connection index
 * @param svc_idx    Service index
 ****************************************************************************************
 */
static uint16_t bass_notify_batt_lvl(bass_env_t *p_bass_env, uint8_t conidx, uint8_t svc_idx)
{
    uint16_t handle = BLE_GATT_INVALID_HDL;
    uint16_t status = BLE_PRF_APP_ERROR;
    handle = bass_get_att_handle(svc_idx, BAS_IDX_BATT_LVL_VAL);

    if((p_bass_env != NULL) && (!p_bass_env->in_exe_op))
    {
        p_bass_env->in_exe_op = true;

        while(!(p_bass_env->op_ongoing))
        {
            // send notify
            status = ble_gatt_srv_event_send(conidx, p_bass_env->user_lid, 0,
                                    BLE_GATT_NOTIFY, handle, &p_bass_env->batt_lvl[svc_idx], sizeof(p_bass_env->batt_lvl[svc_idx]));
            if(status == BLE_GAP_ERR_NO_ERROR)
            {
                p_bass_env->op_ongoing = true;
            }
        }
        p_bass_env->in_exe_op = false;
    }
    return (status);
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
static void bass_cb_att_read_get(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset,
                                   uint16_t max_length)
{
    uint16_t status = BLE_GAP_ERR_NO_ERROR;
    uint32_t value = 0x00;
    uint16_t length = 0;

    if(p_bass_env != NULL)
    {
        uint8_t svc_idx = 0, att_idx = 0;
        status = bass_get_att_idx(hdl, &svc_idx, &att_idx);
        if(status == BLE_GAP_ERR_NO_ERROR)
        {
            switch(att_idx)
            {
                case BAS_IDX_BATT_LVL_VAL:
                {
                     value =  p_bass_env->batt_lvl[svc_idx];
                     length = sizeof(uint8_t);
                } break;
                case BAS_IDX_BATT_LVL_NTF_CFG:
                {
                    value = (p_bass_env->ntf_cfg[conidx] >> svc_idx & BAS_BATT_LVL_NTF_SUP)
                                     ? BLE_PRF_CLI_START_NTF : BLE_PRF_CLI_STOP_NTFIND;
                    length = sizeof(uint16_t);
                } break;
                case BAS_IDX_BATT_LVL_PRES_FMT:
                {
                    //Todu:Add format information
                } break;
                default: { status = BLE_PRF_APP_ERROR; } break;
            }
        }
    }
    // Send result to peer device
    status = ble_gatt_srv_att_read_get_cfm(conidx, user_lid, token, status, length, length, (uint8_t*)&value);
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
static void bass_cb_att_val_set(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset,
                                  void* p_data)
{
    uint16_t status = BLE_PRF_APP_ERROR;

    if(p_bass_env != NULL)
    {
            uint8_t svc_idx = 0, att_idx = 0;
            status = bass_get_att_idx(hdl, &svc_idx, &att_idx);

            if(status == BLE_GAP_ERR_NO_ERROR)
            {
                // Extract value
                uint16_t ntf_cfg = ble_co_btohs(ble_co_read16p(ble_co_buf_data(p_data)));

                 // Only update configuration if value for stop or notification enable
                if (   (att_idx == BAS_IDX_BATT_LVL_NTF_CFG)
                    && ((ntf_cfg == BLE_PRF_CLI_STOP_NTFIND) || (ntf_cfg == BLE_PRF_CLI_START_NTF)))
                {
                    const bass_cb_t* p_cb  = (const bass_cb_t*) p_bass_env->p_cb;

                    // Conserve information in environment
                    if (ntf_cfg == BLE_PRF_CLI_START_NTF)
                    {
                        // Ntf cfg bit set to 1
                        p_bass_env->ntf_cfg[conidx] |= (BAS_BATT_LVL_NTF_SUP << svc_idx);
                    }
                    else
                    {
                        // Ntf cfg bit set to 0
                        p_bass_env->ntf_cfg[conidx] &= ~(BAS_BATT_LVL_NTF_SUP << svc_idx);
                    }

                    // Inform application about bond data update
                    p_cb->cb_bond_data_upd(conidx, p_bass_env->ntf_cfg[conidx]);
                }
                else
                {
                    status = BLE_PRF_APP_ERROR;
                }
            }
    }

    ble_gatt_srv_att_val_set_cfm(conidx, user_lid, token, status);
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
static void bass_cb_event_sent(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
        // Consider job done
        if(p_bass_env != NULL)
        {
            const bass_cb_t* p_cb  = (const bass_cb_t*) p_bass_env->p_cb;
            p_bass_env->op_ongoing = false;

            // Inform application that event has been sent
            p_cb->cb_batt_level_upd_cmp(status);
        }
}


void bass_cb_batt_level_upd_cmp(uint16_t status)
{
    //Todu: user define actions

}

void bass_cb_bond_data_upd(uint8_t conidx, uint8_t ntf_ind_cfg)
{
    //Todu: user define actions

}


/// Message callback handle from APP
const bass_cb_t bass_msg_cb =
{
    .cb_batt_level_upd_cmp = bass_cb_batt_level_upd_cmp,
    .cb_bond_data_upd = bass_cb_bond_data_upd,
};


/// Service callback hander
static const ble_gatt_srv_cb_t bass_cb =
{
        .cb_event_sent    = bass_cb_event_sent,
        .cb_att_read_get  = bass_cb_att_read_get,
        .cb_att_event_get = NULL,
        .cb_att_info_get  = NULL,
        .cb_att_val_set   = bass_cb_att_val_set,
};

/*
 * PROFILE NATIVE HANDLERS
 ****************************************************************************************
 */


/**
 ****************************************************************************************
 * @brief Restore bond data of a known peer device (at connection establishment)
 *
 * @param[in] conidx          Connection index
 * @param[in] ntf_cfg         Notification Configuration
 * @param[in] p_old_batt_lvl  Old Battery Level used to decide if notification should be triggered
 *                            Array of BASS_NB_BAS_INSTANCES_MAX size.
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t bass_enable(uint8_t conidx, uint8_t ntf_cfg, const uint8_t* p_old_batt_lvl)
{
    uint16_t status = BLE_PRF_ERR_REQ_DISALLOWED;

    if(p_old_batt_lvl == NULL)
    {
        status = BLE_PRF_ERR_INVALID_PARAM;
    }
    else if(p_bass_env != NULL)
    {
        uint8_t svc_cursor;
        p_bass_env->ntf_cfg[conidx] = ntf_cfg;

        // loop on all services to check if notification should be triggered
        for(svc_cursor = 0 ; svc_cursor < p_bass_env->nb_svc; svc_cursor++)
        {
            if (   ((p_bass_env->ntf_cfg[conidx] & (1 << svc_cursor)) != 0)
                && (p_old_batt_lvl[svc_cursor] != p_bass_env->batt_lvl[svc_cursor]))
            {
                // trigger notification
                bass_notify_batt_lvl(p_bass_env, conidx, svc_cursor);
            }
        }
        status = BLE_GAP_ERR_NO_ERROR;
    }

    return (status);
}

/**
 ****************************************************************************************
 * @brief Update a battery level
 *
 * Wait for @see cb_batt_level_upd_cmp execution before starting a new procedure
 *
 * @param[in] p_temp_meas   Pointer to Temperature Measurement information
 * @param[in] batt_level   Stable or intermediary type of temperature (True stable meas, else false)
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t bass_batt_level_upd(uint8_t conidx,uint8_t bas_instance, uint8_t batt_level)
{
    uint16_t status = BLE_PRF_ERR_REQ_DISALLOWED;

    if(p_bass_env != NULL)
    {
        // Parameter sanity check
        if ((bas_instance < p_bass_env->nb_svc) && (batt_level <= BAS_BATTERY_LVL_MAX))
        {
            // update the battery level value
            p_bass_env->batt_lvl[bas_instance] = batt_level;
            if (   ((p_bass_env->ntf_cfg[conidx] & (1 << bas_instance)) != 0))
            {
                status = bass_notify_batt_lvl(p_bass_env, conidx, bas_instance);
            }
        }
        else
        {
            status = BLE_PRF_ERR_INVALID_PARAM;
        }
    }

    return (status);
}




/*
 * PROFILE DEFAULT HANDLERS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Initialization of the BASS module.
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
static uint16_t bass_init(uint16_t *p_start_hdl, uint8_t sec_lvl, uint8_t user_prio,
                          struct bass_db_cfg *p_params, const bass_cb_t* p_cb)
{
    //------------------ create the attribute database for the profile -------------------

    // DB Creation Status
    uint16_t status = BLE_GAP_ERR_NO_ERROR;
    uint8_t user_lid = BLE_GATT_INVALID_USER_LID;

    do
    {
        uint16_t shdl[BASS_NB_BAS_INSTANCES_MAX];
        uint8_t  cursor;
        uint16_t features = 0;

        if(p_cb == NULL)
        {
            p_cb = &(bass_msg_cb);
        }

        if(   (p_params->bas_nb == 0) || (p_params->bas_nb > BASS_NB_BAS_INSTANCES_MAX)
           || (p_params == NULL) || (p_start_hdl == NULL) || (p_cb == NULL) || (p_cb->cb_batt_level_upd_cmp == NULL)
           || (p_cb->cb_bond_data_upd == NULL))
        {
            status = BLE_GAP_ERR_INVALID_PARAM;
            break;
        }

        // register BASS user
        status = ble_gatt_user_register(BASS_VAL_MAX_LEN, user_prio, &bass_cb, &user_lid);
        if(status != BLE_GAP_ERR_NO_ERROR) break;


        for (cursor = 0; (cursor < p_params->bas_nb); cursor++)
        {
            // Service content flag
            uint32_t cfg_flag = BAS_CFG_FLAG_MANDATORY_MASK;
            features |= (p_params->features[cursor]) << cursor;

            // Check if notifications are supported
            if (p_params->features[cursor] == BAS_BATT_LVL_NTF_SUP)
            {
                cfg_flag |= BAS_CFG_FLAG_NTF_SUP_MASK;
            }

            // Check if multiple instances
            if (p_params->bas_nb > 1)
            {
                cfg_flag |= BAS_CFG_FLAG_MTP_BAS_MASK;
            }

            shdl[cursor] = *p_start_hdl;

            // Add BASS service
            status = ble_gatt_db_svc16_add(user_lid, sec_lvl, BLE_GATT_SVC_BATTERY_SERVICE, BAS_IDX_NB,
                                       (uint8_t *)&cfg_flag, &(bass_att_db[0]), BAS_IDX_NB, &(shdl[cursor]));

            if(status != BLE_GAP_ERR_NO_ERROR) break;

            // update start handle for next service - only useful if multiple service, else not used.
            // 4 characteristics + optional notification characteristic.
            *p_start_hdl = shdl[cursor] + BAS_IDX_NB;

            //Set optional permissions
            if (p_params->features[cursor] == BAS_BATT_LVL_NTF_SUP)
            {
                // Battery Level characteristic value permissions
                uint16_t perm = BLE_PROP(RD) | BLE_PROP(N);
                ble_gatt_db_att_info_set(user_lid, shdl[cursor] + BAS_IDX_BATT_LVL_VAL, perm);
            }
        }

        if(status != BLE_GAP_ERR_NO_ERROR) break;

        if(p_bass_env != NULL)
        {
            // allocate BASS required environment variable
            p_bass_env->p_cb = p_cb;
            p_bass_env->start_hdl  = shdl[0];
            p_bass_env->batt_lvl[0] = 0x50;
            p_bass_env->features   = features;
            p_bass_env->user_lid   = user_lid;
            p_bass_env->nb_svc     = p_params->bas_nb;
            p_bass_env->op_ongoing = false;
            p_bass_env->in_exe_op  = false;

            memset(p_bass_env->ntf_cfg, 0, BLE_CONNECTION_MAX);
//          memcpy(p_bass_env->batt_level_pres_format, p_params->batt_level_pres_format,
//                   sizeof(prf_char_pres_fmt_t) * BASS_NB_BAS_INSTANCES_MAX);

            *p_start_hdl = shdl[0];
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
 * @brief Initialization of the BASS module.
 *
 * @return status code to know if profile initialization succeed or not.
 ****************************************************************************************
 */
uint16_t ble_bass_init(void)
{
    struct bass_db_cfg db_cfg_params;
    uint16_t start_hdl=0;
    db_cfg_params.bas_nb = 1;
    db_cfg_params.features[0] = BAS_BATT_LVL_NTF_SUP;
    return bass_init(&start_hdl, 0, 0, &db_cfg_params, NULL);
}

/**
 ****************************************************************************************
 * @brief The function enables the BASS.
 * @param[in] None.
 ****************************************************************************************
 */
void ble_bass_enable(uint8_t conidx, uint8_t *batt_val)
{
    uint16_t ntf_cfg = 0x0;
    bass_enable(conidx, ntf_cfg, batt_val);
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
static uint16_t bass_destroy(uint8_t reason)
{
    uint16_t status = BLE_GAP_ERR_NO_ERROR;

    status = ble_gatt_user_unregister(p_bass_env->user_lid);

    return (status);
}


/**
 ****************************************************************************************
 * @brief Handles Disconnection
 *
 * @param[in|out]    p_env      Collector or Service allocated environment data.
 * @param[in]        conidx     Connection index
 ****************************************************************************************
 */
static void bass_cleanup(uint8_t conidx)
{
    uint8_t svc_idx;
    //ASSERT_ERR(conidx < BLE_CONNECTION_MAX);
    // force notification config to zero when peer device is disconnected
    p_bass_env->ntf_cfg[conidx] = 0;
    for (svc_idx = 0; svc_idx < p_bass_env->nb_svc; svc_idx++)
    {
        p_bass_env->ntf_cfg[conidx] = 0;
    }
}


#endif // (BLE_BATT_SERVER)

/// @} BASS

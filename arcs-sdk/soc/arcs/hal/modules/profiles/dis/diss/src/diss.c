/**
 ****************************************************************************************
 *
 * @file diss.c
 *
 * @brief Device Information Service Server Implementation.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup DISS
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#define BLE_DIS_SERVER 1

#if (BLE_DIS_SERVER)
#include "diss_msg.h"
#include "diss.h"

#include <string.h>
#include "dis_param_ctrl.h"

/*
 * DEFINES
 ****************************************************************************************
 */


/// Content of DISS token
enum diss_token_bf
{
    /// GATT procedure token
    DISS_TOKEN_GATT_TOKEN_MASK = 0x0000FFFF,
    DISS_TOKEN_GATT_TOKEN_LSB  = 0,
    /// Connection index
    DISS_TOKEN_CONIDX_MASK     = 0x00FF0000,
    DISS_TOKEN_CONIDX_LSB      = 16,
    /// Data offset requested
    DISS_TOKEN_OFFSET_MASK     = 0xFF000000,
    DISS_TOKEN_OFFSET_LSB      = 24,
};

/*
 * TYPES DEFINITION
 ****************************************************************************************
 */
///Device Information Service Server Environment Variable
typedef struct diss_env
{
    diss_cb_t* p_cb;

    /// Service Attribute Start Handle
    uint16_t start_hdl;
    /// Services features
    uint16_t features;
    /// GATT user local identifier
    uint8_t  user_lid;
    /// Number of DIS
    uint8_t  nb_svc;
} diss_env_t;


/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */
struct diss_env diss_env;
diss_env_t* p_diss_env = &diss_env;

/*
 * DIS ATTRIBUTES DATABASE
 ****************************************************************************************
 */

/// Full DIS Database Description - Used to add attributes into the database
const ble_gatt_att16_desc_t diss_att_db[DIS_IDX_NB] =
{
    // Device Information Service Declaration
    [DIS_IDX_SVC]                       =   {BLE_GATT_DECL_PRIMARY_SERVICE,     BLE_PROP(RD), 0                 },

    // Manufacturer Name Characteristic Declaration
    [DIS_IDX_MANUFACTURER_NAME_CHAR]    =   {BLE_GATT_DECL_CHARACTERISTIC,      BLE_PROP(RD), 0                 },
    // Manufacturer Name Characteristic Value
    [DIS_IDX_MANUFACTURER_NAME_VAL]     =   {BLE_GATT_CHAR_MANUF_NAME,          BLE_PROP(RD), DIS_VAL_MAX_LEN   },

    // Model Number String Characteristic Declaration
    [DIS_IDX_MODEL_NB_STR_CHAR]         =   {BLE_GATT_DECL_CHARACTERISTIC,      BLE_PROP(RD), 0                 },
    // Model Number String Characteristic Value
    [DIS_IDX_MODEL_NB_STR_VAL]          =   {BLE_GATT_CHAR_MODEL_NB,            BLE_PROP(RD), DIS_VAL_MAX_LEN   },

    // Serial Number String Characteristic Declaration
    [DIS_IDX_SERIAL_NB_STR_CHAR]        =   {BLE_GATT_DECL_CHARACTERISTIC,      BLE_PROP(RD), 0                 },
    // Serial Number String Characteristic Value
    [DIS_IDX_SERIAL_NB_STR_VAL]         =   {BLE_GATT_CHAR_SERIAL_NB,           BLE_PROP(RD), DIS_VAL_MAX_LEN   },

    // Hardware Revision String Characteristic Declaration
    [DIS_IDX_HARD_REV_STR_CHAR]         =   {BLE_GATT_DECL_CHARACTERISTIC,      BLE_PROP(RD), 0                 },
    // Hardware Revision String Characteristic Value
    [DIS_IDX_HARD_REV_STR_VAL]          =   {BLE_GATT_CHAR_HW_REV,              BLE_PROP(RD), DIS_VAL_MAX_LEN   },

    // Firmware Revision String Characteristic Declaration
    [DIS_IDX_FIRM_REV_STR_CHAR]         =   {BLE_GATT_DECL_CHARACTERISTIC,      BLE_PROP(RD), 0                 },
    // Firmware Revision String Characteristic Value
    [DIS_IDX_FIRM_REV_STR_VAL]          =   {BLE_GATT_CHAR_FW_REV,              BLE_PROP(RD), DIS_VAL_MAX_LEN   },

    // Software Revision String Characteristic Declaration
    [DIS_IDX_SW_REV_STR_CHAR]           =   {BLE_GATT_DECL_CHARACTERISTIC,      BLE_PROP(RD), 0                 },
    // Software Revision String Characteristic Value
    [DIS_IDX_SW_REV_STR_VAL]            =   {BLE_GATT_CHAR_SW_REV,              BLE_PROP(RD), DIS_VAL_MAX_LEN   },

    // System ID Characteristic Declaration
    [DIS_IDX_SYSTEM_ID_CHAR]            =   {BLE_GATT_DECL_CHARACTERISTIC,      BLE_PROP(RD), 0                 },
    // System ID Characteristic Value
    [DIS_IDX_SYSTEM_ID_VAL]             =   {BLE_GATT_CHAR_SYS_ID,              BLE_PROP(RD), DIS_VAL_MAX_LEN    },

//    // IEEE 11073-20601 Regulatory Certification Data List Characteristic Declaration
//    [DIS_IDX_IEEE_CHAR]                 =   {BLE_GATT_DECL_CHARACTERISTIC,      BLE_PROP(RD), 0                 },
//    // IEEE 11073-20601 Regulatory Certification Data List Characteristic Value
//    [DIS_IDX_IEEE_VAL]                  =   {BLE_GATT_CHAR_IEEE_CERTIF,         BLE_PROP(RD), DIS_SYS_ID_LEN    },

    // PnP ID Characteristic Declaration
    [DIS_IDX_PNP_ID_CHAR]               =   {BLE_GATT_DECL_CHARACTERISTIC,      BLE_PROP(RD), 0                 },
    // PnP ID Characteristic Value
    [DIS_IDX_PNP_ID_VAL]                =   {BLE_GATT_CHAR_PNP_ID,              BLE_PROP(RD), DIS_PNP_ID_LEN    },
};

/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Check if an attribute shall be added or not in the database
 *
 * @param features DIS features
 *
 * @return Feature config flag
 ****************************************************************************************
 */
static uint32_t diss_compute_cfg_flag(uint16_t features)
{
    //Service Declaration
    uint32_t cfg_flag = 1;

    for (uint8_t i = 0; i < DIS_VAL_MAX; i++)
    {
        if (((features >> i) & 1) == 1)
        {
            cfg_flag |= (3 << (i*2 + 1));
        }
    }

    return cfg_flag;
}

/**
 ****************************************************************************************
 * @brief Retrieve Service and attribute index form attribute handle
 *
 * @param[out] handle     Attribute handle
 * @param[out] p_svc_idx  DIF Service index
 * @param[out] p_att_idx  Attribute index
 *
 * @return Success if attribute and service index found, else Application error
 ****************************************************************************************
 */
uint16_t diss_get_att_idx(uint16_t handle, uint8_t *p_svc_idx, uint8_t *p_att_idx)
{
    uint16_t hdl_cursor = p_diss_env->start_hdl;
    uint16_t status = BLE_PRF_APP_ERROR;

    // Browse list of services
    // handle must be greater than current index
    for (*p_svc_idx = 0; (*p_svc_idx < p_diss_env->nb_svc) && (handle >= hdl_cursor); (*p_svc_idx)++)
    {
        // check if handle is within service range
        if (handle <= (hdl_cursor + DIS_IDX_NB))
        {
            *p_att_idx = handle - hdl_cursor;

            // search succeed
            status = BLE_GAP_ERR_NO_ERROR;
            break;
        }

        hdl_cursor += DIS_IDX_NB;
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
static void diss_cb_att_read_get(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset,
                                   uint16_t max_length)
{
    uint16_t status = BLE_GAP_ERR_NO_ERROR;
    uint8_t p_value[DIS_VAL_MAX_LEN];

    // subpackage
    uint8_t report_data[max_length];
    uint16_t report_length = 0;
    uint16_t report_total_length;

    // first packet
    memset(p_value, 0, sizeof(p_value));

    if(p_diss_env != NULL)
    {
        uint8_t svc_idx = 0, att_idx = 0;
        status = diss_get_att_idx(hdl, &svc_idx, &att_idx);
        if(status == BLE_GAP_ERR_NO_ERROR)
        {
            if(p_diss_env->p_cb->cb_value_get != NULL)
            {
                status = p_diss_env->p_cb->cb_value_get(conidx, att_idx, p_value, DIS_VAL_MAX_LEN, &report_total_length);
            }
        }
    }

    if(report_total_length > max_length)
    {
        report_length = max_length;
    }
    else
    {
        report_length = report_total_length;
    }
    memcpy(report_data, p_value+offset, report_length);

    //CLOGD("offset:%d,report_length:%d  %d, value:%x%x%x%x", offset, report_total_length, report_length, report_data[0],report_data[1],report_data[2],report_data[3]);

    status = ble_gatt_srv_att_read_get_cfm(conidx, user_lid, token, status, report_total_length, report_length, report_data);
}

/**
 ****************************************************************************************
 * @brief This function is called during a write procedure to get information about a
 *        specific attribute handle.
 *
 *        @see gatt_srv_att_info_get_cfm shall be called to provide attribute information
 *
 * @param[in] conidx        Connection index
 * @param[in] user_lid      GATT user local identifier
 * @param[in] token         Procedure token that must be returned in confirmation function
 * @param[in] hdl           Attribute handle
 ****************************************************************************************
 */
static void diss_cb_att_info_get (uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl)
{
    ble_gatt_srv_att_info_get_cfm(conidx, user_lid, token, BLE_PRF_APP_ERROR, 0);
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
static void diss_cb_att_val_set(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset,
                                void* p_data)
{
    ble_gatt_srv_att_val_set_cfm(conidx, user_lid, token, BLE_PRF_APP_ERROR);
}


uint16_t diss_cb_value_get(uint8_t conidx, uint8_t att_idx, uint8_t *p_value, uint16_t max_len, uint16_t *ret_len)
{
    /// default callback.
	return 0;
}


/// Message callback handle from APP
const diss_cb_t diss_msg_cb =
{
    .cb_value_get = diss_cb_value_get,
};

/// Service callback hander
static const ble_gatt_srv_cb_t diss_cb =
{
        .cb_event_sent    = NULL,
        .cb_att_read_get  = diss_cb_att_read_get,
        .cb_att_event_get = NULL,
        .cb_att_info_get  = diss_cb_att_info_get,
        .cb_att_val_set   = diss_cb_att_val_set,
};


/*
 * PROFILE DEFAULT HANDLERS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Initialization of the DISS module.
 * This function performs all the initializations of the Profile module.
 *  - Creation of database (if it's a service)
 *  - Allocation of profile required memory
 *  - Initialization of task descriptor to register application
 *      - Task State array
 *      - Number of tasks
 *      - Default task handler
 *
 * @param[in|out] p_start_hdl  Service start handle (0 - dynamically allocated), only applies for services.
 * @param[in]     sec_lvl      Security level (@see enum gatt_svc_info_bf)
 * @param[in]     user_prio    GATT User priority
 * @param[in]     p_param      Configuration parameters of profile collector or service (32 bits aligned)
 * @param[in]     p_cb         Callback structure that handles event from profile
 *
 * @return status code to know if profile initialization succeed or not.
 ****************************************************************************************
 */
static uint16_t diss_init(uint16_t *p_start_hdl, uint8_t sec_lvl, uint8_t user_prio,
                          struct diss_db_cfg *p_params, const diss_cb_t* p_cb)
{
    //------------------ create the attribute database for the profile -------------------

    // DB Creation Status
    uint16_t status = BLE_GAP_ERR_NO_ERROR;
    uint8_t user_lid = BLE_GATT_INVALID_USER_LID;

    do
    {
        // Service content flag
        uint32_t cfg_flag;

        if(p_cb == NULL)
        {
            p_cb = &(diss_msg_cb);
        }

        if((p_params == NULL) || (p_start_hdl == NULL) || (p_cb == NULL) || (p_cb->cb_value_get == NULL))
        {
            status = BLE_GAP_ERR_INVALID_PARAM;
            break;
        }

        // register DISS user
        status = ble_gatt_user_register(DIS_VAL_MAX_LEN, user_prio, &diss_cb, &user_lid);
        if(status != BLE_GAP_ERR_NO_ERROR) break;

        // Compute Attribute Table and save it in environment
        cfg_flag = diss_compute_cfg_flag(p_params->features);

        // Add GAP service
        status = ble_gatt_db_svc16_add(user_lid, sec_lvl, BLE_GATT_SVC_DEVICE_INFO, DIS_IDX_NB,
                                   (uint8_t *)&cfg_flag, &(diss_att_db[0]), DIS_IDX_NB, p_start_hdl);
        if(status != BLE_GAP_ERR_NO_ERROR) break;

        // allocate DISS required environment variable
        p_diss_env->start_hdl = *p_start_hdl;
        p_diss_env->features  = p_params->features;
        p_diss_env->user_lid  = user_lid;
        p_diss_env->nb_svc     = 1;
        p_diss_env->p_cb       = (diss_cb_t*)p_cb;

    } while(0);

    if((status != BLE_GAP_ERR_NO_ERROR) && (user_lid != BLE_GATT_INVALID_USER_LID))
    {
        ble_gatt_user_unregister(user_lid);
    }

    return (status);
}


uint16_t ble_diss_init(diss_cb_t *p_cb)
{
    struct diss_db_cfg db_cfg_params;
    uint16_t start_hdl=0;
    db_cfg_params.features = DIS_ALL_FEAT_SUP;
    return diss_init(&start_hdl, 0, 0, &db_cfg_params, p_cb);
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
static uint16_t diss_destroy(uint8_t reason)
{
    uint16_t status = BLE_GAP_ERR_NO_ERROR;

    status = ble_gatt_user_unregister(p_diss_env->user_lid);

    return (status);
}



#endif //BLE_DIS_SERVER

/// @} DISS

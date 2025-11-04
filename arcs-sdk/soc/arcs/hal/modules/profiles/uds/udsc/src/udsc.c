/**
 ****************************************************************************************
 *
 * @file udsc.c
 *
 * @brief User Data Service Collector implementation.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 ****************************************************************************************
 */


/**
 ****************************************************************************************
 * @addtogroup UDSC
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "btip_config.h"

#if (BLE_UDS_CLIENT)

#include "udsc.h"
#include "gap.h"
#include "gatt.h"
#include "ble_prf.h"

#include "co_utils.h"
#include "co_endian.h"
#include "co_time.h"

#include <string.h>
#include "ke_mem.h"


/*
 * DEFINES
 ****************************************************************************************
 */

/// Maximum number of Client task instances

/// value type
enum udsc_val_type
{
    /// User value
    UDSC_USER_VAL = 0,
    /// Client char config
    UDSC_CCC_CFG  = 1,
    /// Control point
    UDSC_CNTL_PT  = 2,
};

/// Dummy bit field
enum udsc_dummy_bf
{
    /// Value identifier
    UDSC_VAL_ID_MASK    = 0x00FF,
    UDSC_VAL_ID_LSB     = 0,
    /// VAL_TYPE
    UDSC_VAL_TYPE_MASK  = 0xFF00,
    UDSC_VAL_TYPE_LSB   = 8,
};


/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */


/// Environment variable for each Connections
typedef struct udsc_cnx_env
{
    /// Control point timer
    co_time_timer_t     timer;
    /// Peer database discovered handle mapping
    udsc_uds_content_t  uds;
    /// counter used to check service uniqueness
    uint8_t             nb_svc;
    /// Client is in discovering state
    bool                discover;
    /// Control point operation on-going (@see enum cpp_ctnl_pt_code)
    uint8_t             ctrl_pt_op;
} udsc_cnx_env_t;

/// Client environment variable
typedef struct udsc_env
{
    /// profile environment
    prf_hdr_t            prf_env;
    /// Environment variable pointer for each connections
    udsc_cnx_env_t*      p_env[BLE_CONNECTION_MAX];
    /// GATT User local identifier
    uint8_t              user_lid;
} udsc_env_t;


/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */

/// State machine used to retrieve User Data Service Service characteristics information
const prf_char_def_t udsc_uds_char[UDSC_CHAR_UDS_MAX] =
{
    [UDSC_CHAR_UDS_DB_CHG_INC]                      = { GATT_CHAR_DB_CHG_INCREMENT,                 ATT_REQ(PRES, MAND), (PROP(RD) | PROP(WR) | PROP(N))  },

    [UDSC_CHAR_UDS_USER_INDEX]                      = { GATT_CHAR_USER_INDEX,                       ATT_REQ(PRES, MAND), (PROP(RD))                       },

    [UDSC_CHAR_UDS_USER_CTRL_PT]                    = { GATT_CHAR_USER_CONTROL_POINT,               ATT_REQ(PRES, MAND), (PROP(WR) | PROP(I))             },

    /// Strings: utf8s
    [UDSC_CHAR_UDS_FIRST_NAME]                      = { GATT_CHAR_FIRST_NAME,                       ATT_REQ(PRES, OPT), (PROP(RD) | PROP(WR))             },

    [UDSC_CHAR_UDS_LAST_NAME]                       = { GATT_CHAR_LAST_NAME,                        ATT_REQ(PRES, OPT), (PROP(RD) | PROP(WR))             },

    [UDSC_CHAR_UDS_EMAIL_ADDRESS]                   = { GATT_CHAR_EMAIL_ADDRESS,                    ATT_REQ(PRES, OPT), (PROP(RD) | PROP(WR))             },

    [UDSC_CHAR_UDS_LANGUAGE]                        = { GATT_CHAR_LANGUAGE,                         ATT_REQ(PRES, OPT), (PROP(RD) | PROP(WR))             },

    /// Date
    [UDSC_CHAR_UDS_DATE_OF_BIRTH]                   = { GATT_CHAR_DATE_OF_BIRTH,                    ATT_REQ(PRES, OPT), (PROP(RD) | PROP(WR))             },

    [UDSC_CHAR_UDS_DATE_OF_THR_ASSESS]              = { GATT_CHAR_DATE_OF_THR_ASSESS,               ATT_REQ(PRES, OPT), (PROP(RD) | PROP(WR))             },

    /// uint16
    [UDSC_CHAR_UDS_WEIGHT]                          = { GATT_CHAR_WEIGHT,                           ATT_REQ(PRES, OPT), (PROP(RD) | PROP(WR))             },

    [UDSC_CHAR_UDS_HEIGHT]                          = { GATT_CHAR_HEIGHT,                           ATT_REQ(PRES, OPT), (PROP(RD) | PROP(WR))             },

    [UDSC_CHAR_UDS_WAIST_CIRCUMFERENCE]             = { GATT_CHAR_WAIST_CIRCUMFERENCE,              ATT_REQ(PRES, OPT), (PROP(RD) | PROP(WR))             },

    [UDSC_CHAR_UDS_HIP_CIRCUMFERENCE]               = { GATT_CHAR_HIP_CIRCUMFERENCE,                ATT_REQ(PRES, OPT), (PROP(RD) | PROP(WR))             },

    /// uint8
    [UDSC_CHAR_UDS_AGE]                             = { GATT_CHAR_AGE,                              ATT_REQ(PRES, OPT), (PROP(RD) | PROP(WR))             },

    [UDSC_CHAR_UDS_GENDER]                          = { GATT_CHAR_GENDER,                           ATT_REQ(PRES, OPT), (PROP(RD) | PROP(WR))             },

    [UDSC_CHAR_UDS_VO2_MAX]                         = { GATT_CHAR_VO2_MAX,                          ATT_REQ(PRES, OPT), (PROP(RD) | PROP(WR))             },

    [UDSC_CHAR_UDS_MAX_HEART_RATE]                  = { GATT_CHAR_MAX_HEART_RATE,                   ATT_REQ(PRES, OPT), (PROP(RD) | PROP(WR))             },

    [UDSC_CHAR_UDS_RESTING_HEART_RATE]              = { GATT_CHAR_RESTING_HEART_RATE,               ATT_REQ(PRES, OPT), (PROP(RD) | PROP(WR))             },

    [UDSC_CHAR_UDS_MAX_RECO_HEART_RATE]             = { GATT_CHAR_MAX_RECO_HEART_RATE,              ATT_REQ(PRES, OPT), (PROP(RD) | PROP(WR))             },

    [UDSC_CHAR_UDS_AEROBIC_THR]                     = { GATT_CHAR_AEROBIC_THR,                      ATT_REQ(PRES, OPT), (PROP(RD) | PROP(WR))             },

    [UDSC_CHAR_UDS_ANAERO_THR]                      = { GATT_CHAR_ANAERO_THR,                       ATT_REQ(PRES, OPT), (PROP(RD) | PROP(WR))             },

    [UDSC_CHAR_UDS_SPORT_TYPE_FOR_AERO_ANAREO_THRS] = { GATT_CHAR_SPORT_TYPE_FOR_AERO_ANAREO_THRS,  ATT_REQ(PRES, OPT), (PROP(RD) | PROP(WR))             },

    [UDSC_CHAR_UDS_FAT_BURN_HEART_RATE_LOW_LIM]     = { GATT_CHAR_FAT_BURN_HEART_RATE_LOW_LIM,      ATT_REQ(PRES, OPT), (PROP(RD) | PROP(WR))             },

    [UDSC_CHAR_UDS_FAT_BURN_HEART_RATE_UP_LIM]      = { GATT_CHAR_FAT_BURN_HEART_RATE_UP_LIM,       ATT_REQ(PRES, OPT), (PROP(RD) | PROP(WR))             },

    [UDSC_CHAR_UDS_AEROBIC_HEART_RATE_LOW_LIM]      = { GATT_CHAR_AEROBIC_HEART_RATE_LOW_LIM,       ATT_REQ(PRES, OPT), (PROP(RD) | PROP(WR))             },

    [UDSC_CHAR_UDS_AEROBIC_HEART_RATE_UP_LIM]       = { GATT_CHAR_AEROBIC_HEART_RATE_UP_LIM,        ATT_REQ(PRES, OPT), (PROP(RD) | PROP(WR))             },

    [UDSC_CHAR_UDS_ANAERO_HEART_RATE_LOW_LIM]       = { GATT_CHAR_ANAERO_HEART_RATE_LOW_LIM,        ATT_REQ(PRES, OPT), (PROP(RD) | PROP(WR))             },

    [UDSC_CHAR_UDS_ANAERO_HEART_RATE_UP_LIM]        = { GATT_CHAR_ANAERO_HEART_RATE_UP_LIM,         ATT_REQ(PRES, OPT), (PROP(RD) | PROP(WR))             },

    [UDSC_CHAR_UDS_TWO_ZONE_HEART_RATE_LIMITS]      = { GATT_CHAR_TWO_ZONE_HEART_RATE_LIMIT,        ATT_REQ(PRES, OPT), (PROP(RD) | PROP(WR))             },

    /// set
    [UDSC_CHAR_UDS_FIVE_ZONE_HEART_RATE_LIMITS]     = { GATT_CHAR_FIVE_ZONE_HEART_RATE_LIMITS,      ATT_REQ(PRES, OPT), (PROP(RD) | PROP(WR))             },

    [UDSC_CHAR_UDS_THREE_ZONE_HEART_RATE_LIMITS]    = { GATT_CHAR_THREE_ZONE_HEART_RATE_LIMITS,     ATT_REQ(PRES, OPT), (PROP(RD) | PROP(WR))             },

    /// user defined
    [UDSC_CHAR_UDS_USER_DEFINED_1]                  = { GATT_CHAR_FIVE_ZONE_HEART_RATE_LIMITS,      ATT_REQ(PRES, OPT), (PROP(RD) | PROP(WR))             },

    [UDSC_CHAR_UDS_USER_DEFINED_2]                  = { GATT_CHAR_THREE_ZONE_HEART_RATE_LIMITS,     ATT_REQ(PRES, OPT), (PROP(RD) | PROP(WR))             },
    [UDSC_CHAR_UDS_USER_DEFINED_3]                  = { GATT_CHAR_FIVE_ZONE_HEART_RATE_LIMITS,      ATT_REQ(PRES, OPT), (PROP(RD) | PROP(WR))             },

    [UDSC_CHAR_UDS_USER_DEFINED_4]                  = { GATT_CHAR_THREE_ZONE_HEART_RATE_LIMITS,     ATT_REQ(PRES, OPT), (PROP(RD) | PROP(WR))             },
};

/// State machine used to retrieve User Data Service Service characteristic description information
const prf_desc_def_t udsc_uds_char_desc[UDSC_DESC_UDS_MAX] =
{
    /// Database Change Increment Client config
    [UDSC_DESC_UDS_DB_CHG_INC_CCC] =    { GATT_DESC_CLIENT_CHAR_CFG,     ATT_REQ(PRES, MAND), UDSC_CHAR_UDS_DB_CHG_INC    },

    /// User Control Point Client config
    [UDSC_DESC_UDS_USER_CTRL_PT_CCC] =  { GATT_DESC_CLIENT_CHAR_CFG,     ATT_REQ(PRES, MAND), UDSC_CHAR_UDS_USER_CTRL_PT  },

//    /// String Extended Properties
//    [UDSC_DESC_UDS_FIRST_NAME_EXT] =    { GATT_DESC_CHAR_EXT_PROPERTIES, ATT_REQ(PRES, OPT),  UDSC_CHAR_UDS_FIRST_NAME    },
//
//    /// String Extended Properties
//    [UDSC_DESC_UDS_LAST_NAME_EXT] =     { GATT_DESC_CHAR_EXT_PROPERTIES, ATT_REQ(PRES, OPT),  UDSC_CHAR_UDS_LAST_NAME     },
//
//    /// String Extended Properties
//    [UDSC_DESC_UDS_EMAIL_ADDRESS_EXT] = { GATT_DESC_CHAR_EXT_PROPERTIES, ATT_REQ(PRES, OPT),  UDSC_CHAR_UDS_EMAIL_ADDRESS },
//
//    /// String Extended Properties
//    [UDSC_DESC_UDS_LANGUAGE_EXT] =      { GATT_DESC_CHAR_EXT_PROPERTIES, ATT_REQ(PRES, OPT),  UDSC_CHAR_UDS_LANGUAGE      },
};
/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Send discovery results to application.
 *
 * @param p_udsc_env    Client Role task environment
 * @param conidx        Connection index
 * @param status        Response status code
 *****************************************************************************************
 */
__STATIC void udsc_enable_cmp(udsc_env_t* p_udsc_env, uint8_t conidx, uint16_t status)
{
    const udsc_cb_t* p_cb = (const udsc_cb_t*) p_udsc_env->prf_env.p_cb;

    if(p_udsc_env != NULL)
    {
        udsc_cnx_env_t* p_con_env = p_udsc_env->p_env[conidx];
        p_cb->cb_enable_cmp(conidx, status, &(p_con_env->uds));

        if (status != GAP_ERR_NO_ERROR)
        {
            // clean-up environment variable allocated for task instance
            ke_free(p_con_env);
            p_udsc_env->p_env[conidx] = NULL;
        }
        else
        {
             p_con_env->discover = false;

             // Register profile handle to catch gatt indications
             gatt_cli_event_register(conidx, p_udsc_env->user_lid, p_con_env->uds.svc.shdl,
                                     p_con_env->uds.svc.ehdl);
        }
    }
}

/**
 ****************************************************************************************
 * @brief Send read result to application,.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the request
 * @param[in] val_id        Value Identifier (@see enum udsc_val_id)
 * @param[in] p_data        Pointer of data buffer
 ****************************************************************************************
 */
__STATIC void udsc_read_val_cmp(uint8_t conidx, uint16_t status, uint8_t val_id, co_buf_t* p_data)
{
    udsc_env_t* p_udsc_env = PRF_ENV_GET(UDSC, udsc);

    if(p_udsc_env != NULL)
    {
        const udsc_cb_t* p_cb = (const udsc_cb_t*) p_udsc_env->prf_env.p_cb;
        union uds_value  value;
        co_buf_t* p_utf8 = NULL;
        union uds_value* p_value = &value;
        memset(p_value, 0, sizeof(union uds_value));

        if(status == GAP_ERR_NO_ERROR)
        {
            // unpack data
            switch (val_id)
            {
                // Strings: utf8s
                case UDS_CHAR_ID_FIRST_NAME:
                case UDS_CHAR_ID_LAST_NAME:
                case UDS_CHAR_ID_EMAIL_ADDRESS:
                case UDS_CHAR_ID_LANGUAGE:
                {
                    p_value = NULL;
                    p_utf8  = p_data;
                } break;

                // Date
                case UDS_CHAR_ID_DATE_OF_BIRTH:
                case UDS_CHAR_ID_DATE_OF_THR_ASSESS:
                {
                    prf_unpack_date(p_data, &(p_value->date));
                } break;

                // uint16
                case UDS_CHAR_ID_WEIGHT:
                case UDS_CHAR_ID_HEIGHT:
                case UDS_CHAR_ID_WAIST_CIRCUMFERENCE:
                case UDS_CHAR_ID_HIP_CIRCUMFERENCE:
                {
                    // Characteristic Uint16
                    p_value->uint16 = co_btohs(co_read16p(co_buf_data(p_data)));
                } break;

                // uint8
                case UDS_CHAR_ID_USER_INDEX:
                case UDS_CHAR_ID_AGE:
                case UDS_CHAR_ID_GENDER:
                case UDS_CHAR_ID_VO2_MAX:
                case UDS_CHAR_ID_MAX_HEART_RATE:
                case UDS_CHAR_ID_RESTING_HEART_RATE:
                case UDS_CHAR_ID_MAX_RECO_HEART_RATE:
                case UDS_CHAR_ID_AEROBIC_THR:
                case UDS_CHAR_ID_ANAERO_THR:
                case UDS_CHAR_ID_SPORT_TYPE_FOR_AERO_ANAREO_THRS:
                case UDS_CHAR_ID_FAT_BURN_HEART_RATE_LOW_LIM:
                case UDS_CHAR_ID_FAT_BURN_HEART_RATE_UP_LIM:
                case UDS_CHAR_ID_AEROBIC_HEART_RATE_LOW_LIM:
                case UDS_CHAR_ID_AEROBIC_HEART_RATE_UP_LIM:
                case UDS_CHAR_ID_ANAERO_HEART_RATE_LOW_LIM:
                case UDS_CHAR_ID_ANAERO_HEART_RATE_UP_LIM:
                case UDS_CHAR_ID_TWO_ZONE_HEART_RATE_LIMITS:
                {
                    // Characteristic Uint8
                    p_value->uint8 = co_buf_data(p_data)[0];
                } break;

                // set
                case UDS_CHAR_ID_FIVE_ZONE_HEART_RATE_LIMITS :
                {
                    co_buf_copy_data_to_mem(p_data, p_value->set, 4);
                } break;

                // set
                case UDS_CHAR_ID_THREE_ZONE_HEART_RATE_LIMITS:
                {
                    co_buf_copy_data_to_mem(p_data, p_value->set, 2);
                } break;

                // Uint32
                case UDS_CHAR_ID_DB_CHG_INC:
                {
                    p_value->db_chg_inc = co_btohl(co_read32p(co_buf_data(p_data)));
                } break;

                default:  { ASSERT_ERR(0); } break;
            }
        }
        else
        {
            p_value = NULL;
        }

        p_cb->cb_read_cmp(conidx, status, val_id, p_value, p_utf8);
    }
}

/**
 ****************************************************************************************
 * @brief Send read result to application,.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the request
 * @param[in] val_id        Value Identifier (@see enum udsc_val_id)
 * @param[in] p_data        Pointer of data buffer
 ****************************************************************************************
 */
__STATIC void udsc_read_cfg_cmp(uint8_t conidx, uint16_t status, uint8_t val_id, co_buf_t* p_data)
{
    udsc_env_t* p_udsc_env = PRF_ENV_GET(UDSC, udsc);

    if(p_udsc_env != NULL)
    {
        const udsc_cb_t* p_cb = (const udsc_cb_t*) p_udsc_env->prf_env.p_cb;
        uint16_t cfg_val = 0;

        if(status == GAP_ERR_NO_ERROR)
        {
            cfg_val = co_btohs(co_read16p(co_buf_data(p_data)));
        }

        p_cb->cb_read_cfg_cmp(conidx, status, val_id, cfg_val);
    }
}


/**
 ****************************************************************************************
 * @brief Function to called once timer expires
 *
 * @param[in] conidx Connection index
 ****************************************************************************************
 */
__STATIC void udsc_timer_handler(uint32_t conidx)
{
    // Get the address of the environment
    udsc_env_t *p_udsc_env = PRF_ENV_GET(UDSC, udsc);

    if (p_udsc_env != NULL)
    {
        udsc_cnx_env_t* p_con_env = p_udsc_env->p_env[conidx];
        ASSERT_ERR(p_con_env != NULL);
        if(p_con_env->ctrl_pt_op != UDS_OP_CODE_RESERVED_00)
        {
            const udsc_cb_t* p_cb = (const udsc_cb_t*) p_udsc_env->prf_env.p_cb;
            uint8_t op_code = p_con_env->ctrl_pt_op;
            p_con_env->ctrl_pt_op = UDS_OP_CODE_RESERVED_00;

            p_cb->cb_ctnl_pt_req_cmp((uint8_t)conidx, PRF_ERR_PROC_TIMEOUT, op_code, 0, 0, NULL);
        }
    }
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
__STATIC void udsc_svc_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t hdl, uint8_t disc_info,
                          uint8_t nb_att, const gatt_svc_att_t* p_atts)
{
    // Get the address of the environment
    udsc_env_t* p_udsc_env = PRF_ENV_GET(UDSC, udsc);

    if(p_udsc_env != NULL)
    {
        udsc_cnx_env_t* p_con_env = p_udsc_env->p_env[conidx];

        ASSERT_INFO(p_con_env != NULL, conidx, user_lid);

        if (p_con_env->nb_svc == 0)
        {
            //Even if we get multiple responses we only store 1 range
            if((disc_info == GATT_SVC_CMPLT) || (disc_info == GATT_SVC_START))
            {
                p_con_env->uds.svc.shdl = hdl;
            }

            if((disc_info == GATT_SVC_CMPLT) || (disc_info == GATT_SVC_END))
            {
                p_con_env->uds.svc.ehdl = hdl + nb_att -1;
            }

            // Retrieve characteristics
            prf_extract_svc_info(hdl, nb_att, p_atts,
                                 UDSC_CHAR_UDS_MAX, &udsc_uds_char[0],      &(p_con_env->uds.chars[0]),
                                 UDSC_DESC_UDS_MAX, &udsc_uds_char_desc[0], &(p_con_env->uds.descs[0]));
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
__STATIC void udsc_discover_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    // Get the address of the environment
    udsc_env_t* p_udsc_env = PRF_ENV_GET(UDSC, udsc);

    if(p_udsc_env != NULL)
    {
        udsc_cnx_env_t* p_con_env = p_udsc_env->p_env[conidx];

        if (p_con_env->nb_svc ==  1)
        {
            status = prf_check_svc_validity(UDSC_CHAR_UDS_MAX, p_con_env->uds.chars, udsc_uds_char,
                                            UDSC_DESC_UDS_MAX, p_con_env->uds.descs, udsc_uds_char_desc);
        }
        // too much services
        else if (p_con_env->nb_svc > 1)
        {
            status = PRF_ERR_MULTIPLE_SVC;
        }
        // no services found
        else
        {
            status = PRF_ERR_STOP_DISC_CHAR_MISSING;
        }

        udsc_enable_cmp(p_udsc_env, conidx, status);
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
__STATIC void udsc_att_val_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t hdl, uint16_t offset,
                              co_buf_t* p_data)
{
    switch(GETF(dummy, UDSC_VAL_TYPE))
    {
        case UDSC_USER_VAL: { udsc_read_val_cmp(conidx, GAP_ERR_NO_ERROR, (uint8_t) GETF(dummy, UDSC_VAL_ID), p_data); } break;
        case UDSC_CCC_CFG:  { udsc_read_cfg_cmp(conidx, GAP_ERR_NO_ERROR, (uint8_t) GETF(dummy, UDSC_VAL_ID), p_data); } break;
        default:            { ASSERT_ERR(0);                                                                           } break;
    }
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
__STATIC void udsc_read_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    if(status != GAP_ERR_NO_ERROR)
    {
        switch(GETF(dummy, UDSC_VAL_TYPE))
        {
            case UDSC_USER_VAL: { udsc_read_val_cmp(conidx, status, (uint8_t) GETF(dummy, UDSC_VAL_ID), NULL); } break;
            case UDSC_CCC_CFG:  { udsc_read_cfg_cmp(conidx, status, (uint8_t) GETF(dummy, UDSC_VAL_ID), NULL); } break;
            default:            { ASSERT_ERR(0);                                                               } break;
        }
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
__STATIC void udsc_write_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    udsc_env_t* p_udsc_env = PRF_ENV_GET(UDSC, udsc);

    if(p_udsc_env != NULL)
    {
        const udsc_cb_t* p_cb = (const udsc_cb_t*) p_udsc_env->prf_env.p_cb;

        switch(GETF(dummy, UDSC_VAL_TYPE))
        {
            case UDSC_USER_VAL: { p_cb->cb_write_cmp(conidx, status, GETF(dummy, UDSC_VAL_ID));     } break;
            case UDSC_CCC_CFG:  { p_cb->cb_write_cfg_cmp(conidx, status, GETF(dummy, UDSC_VAL_ID)); } break;
            case UDSC_CNTL_PT:
            {
                if(status != GAP_ERR_NO_ERROR)
                {
                    uint8_t opcode = p_udsc_env->p_env[conidx]->ctrl_pt_op;
                    p_udsc_env->p_env[conidx]->ctrl_pt_op = UDS_OP_CODE_RESERVED_00;
                    p_cb->cb_ctnl_pt_req_cmp(conidx, status, opcode, 0, 0, NULL);
                }
                else
                {
                    // Start Timeout Procedure - wait for Indication reception
                    co_time_timer_set(&(p_udsc_env->p_env[conidx]->timer), UDS_CP_TIMEOUT);
                }
            } break;
            default: { ASSERT_ERR(0); } break;
        }
    }
}


/**
 ****************************************************************************************
 * @brief This function is called when a notification or an indication is received onto
 *        register handle range (@see gatt_cli_event_register).
 *
 *        @see gatt_cli_val_event_cfm must be called to confirm event reception.
 *
 * @param[in] conidx        Connection index
 * @param[in] user_lid      GATT user local identifier
 * @param[in] token         Procedure token that must be returned in confirmation function
 * @param[in] evt_type      Event type triggered (@see enum gatt_evt_type)
 * @param[in] complete      True if event value if complete value has been received
 *                          False if data received is equals to maximum attribute protocol value.
 *                          In such case GATT Client User should perform a read procedure.
 * @param[in] hdl           Attribute handle
 * @param[in] p_data        Pointer to buffer that contains attribute value
 ****************************************************************************************
 */
__STATIC void udsc_att_val_evt_cb(uint8_t conidx, uint8_t user_lid, uint16_t token, uint8_t evt_type, bool complete,
                        uint16_t hdl, co_buf_t* p_data)
{
    udsc_env_t* p_udsc_env = PRF_ENV_GET(UDSC, udsc);

    if(p_udsc_env != NULL)
    {
        udsc_cnx_env_t* p_con_env = p_udsc_env->p_env[conidx];
        udsc_uds_content_t* p_uds = &(p_con_env->uds);
        const udsc_cb_t* p_cb = (const udsc_cb_t*) p_udsc_env->prf_env.p_cb;

        if (hdl == p_uds->chars[UDSC_CHAR_UDS_DB_CHG_INC].val_hdl)
        {
            uint32_t value = co_btohl(co_read32p(co_buf_data(p_data)));
            p_cb->cb_db_chg_inc(conidx, value);
        }
        else if (hdl == p_uds->chars[UDSC_CHAR_UDS_USER_CTRL_PT].val_hdl)
        {
            uint8_t op_code;
            uint8_t req_op_code;
            uint8_t resp_value;
            bool valid = (co_buf_data_len(p_data) >= 3);

            // Response Op code
            op_code = co_buf_data(p_data)[0];
            co_buf_head_release(p_data, 1);

            // Requested operation code
            req_op_code = co_buf_data(p_data)[0];
            co_buf_head_release(p_data, 1);

            // Response value
            resp_value = co_buf_data(p_data)[0];
            co_buf_head_release(p_data, 1);

            if(valid && (op_code == UDS_OP_CODE_RESPONSE_CODE) && (req_op_code == p_con_env->ctrl_pt_op))
            {
                uint8_t user_id = UDS_USER_ID_UNKNOWN_USER;

                if ((req_op_code == UDS_OP_CODE_REGISTER_NEW_USER) && (resp_value == UDS_OP_RESPONSE_SUCCESS))
                {
                    user_id = co_buf_data(p_data)[0];
                    co_buf_head_release(p_data, 1);
                }

                p_con_env->ctrl_pt_op = UDS_OP_CODE_RESERVED_00;
                // stop timer
                co_time_timer_stop(&(p_con_env->timer));
                // provide control point response
                p_cb->cb_ctnl_pt_req_cmp(conidx, GAP_ERR_NO_ERROR, req_op_code, resp_value, user_id, p_data);

            }
        }
    }

    // confirm event handling
    gatt_cli_att_event_cfm(conidx, user_lid, token);
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
__STATIC void udsc_svc_changed_cb(uint8_t conidx, uint8_t user_lid, bool out_of_sync, uint16_t start_hdl, uint16_t end_hdl)
{
    // Do Nothing
}

/// Client callback hander
__STATIC const gatt_cli_cb_t udsc_cb =
{
    .cb_discover_cmp    = udsc_discover_cmp_cb,
    .cb_read_cmp        = udsc_read_cmp_cb,
    .cb_write_cmp       = udsc_write_cmp_cb,
    .cb_att_val_get     = NULL,
    .cb_svc             = udsc_svc_cb,
    .cb_svc_info        = NULL,
    .cb_inc_svc         = NULL,
    .cb_char            = NULL,
    .cb_desc            = NULL,
    .cb_att_val         = udsc_att_val_cb,
    .cb_att_val_evt     = udsc_att_val_evt_cb,
    .cb_svc_changed     = udsc_svc_changed_cb,
};

/*
 * PROFILE NATIVE API
 ****************************************************************************************
 */

uint16_t udsc_enable(uint8_t conidx, uint8_t con_type, const udsc_uds_content_t* p_uds)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    udsc_env_t* p_udsc_env = PRF_ENV_GET(UDSC, udsc);

    if(p_udsc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_udsc_env->p_env[conidx] == NULL))
        {
            // allocate environment variable for task instance
            p_udsc_env->p_env[conidx] = (struct udsc_cnx_env *) ke_malloc(sizeof(struct udsc_cnx_env), KE_MEM_ATT_DB);

            if(p_udsc_env->p_env[conidx] != NULL)
            {
                memset(p_udsc_env->p_env[conidx], 0, sizeof(struct udsc_cnx_env));
                co_time_timer_init(&(p_udsc_env->p_env[conidx]->timer), (co_time_timer_cb)udsc_timer_handler,
                                   (uint8_t*) ((uint32_t) conidx));

                // Config connection, start discovering
                if (con_type == PRF_CON_DISCOVERY)
                {
                    uint16_t gatt_svc_uuid = GATT_SVC_USER_DATA;

                    // start discovery
                    status = gatt_cli_discover_svc(conidx, p_udsc_env->user_lid, 0, GATT_DISCOVER_SVC_PRIMARY_BY_UUID, true,
                                                   GATT_MIN_HDL, GATT_MAX_HDL, GATT_UUID_16, (uint8_t*) &gatt_svc_uuid);

                    // Go to DISCOVERING state
                    p_udsc_env->p_env[conidx]->discover   = true;
                    p_udsc_env->p_env[conidx]->ctrl_pt_op = UDS_OP_CODE_RESERVED_00;
                }
                // normal connection, get saved att details
                else
                {
                    memcpy(&(p_udsc_env->p_env[conidx]->uds), p_uds, sizeof(udsc_uds_content_t));
                    status = GAP_ERR_NO_ERROR;

                    // send APP confirmation that can start normal connection to TH
                    udsc_enable_cmp(p_udsc_env, conidx, GAP_ERR_NO_ERROR);
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

uint16_t udsc_read(uint8_t conidx, uint8_t val_id)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    udsc_env_t* p_udsc_env = PRF_ENV_GET(UDSC, udsc);

    if(p_udsc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_udsc_env->p_env[conidx] != NULL) && (!p_udsc_env->p_env[conidx]->discover))
        {
            udsc_cnx_env_t* p_con_env = p_udsc_env->p_env[conidx];
            uint16_t hdl;
            udsc_uds_content_t* p_uds = &(p_con_env->uds);

            hdl = p_uds->chars[val_id].val_hdl;

            if(hdl == GATT_INVALID_HDL)
            {
                status = PRF_ERR_INEXISTENT_HDL;
            }
            else
            {
                uint16_t dummy = 0;
                SETF(dummy, UDSC_VAL_ID,   val_id);
                SETF(dummy, UDSC_VAL_TYPE, UDSC_USER_VAL);

                // perform read request
                status = gatt_cli_read(conidx, p_udsc_env->user_lid, dummy, hdl, 0, 0);
            }
        }
    }

    return (status);
}

uint16_t udsc_read_cfg(uint8_t conidx, uint8_t val_id)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    udsc_env_t* p_udsc_env = PRF_ENV_GET(UDSC, udsc);

    if(p_udsc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_udsc_env->p_env[conidx] != NULL) && (!p_udsc_env->p_env[conidx]->discover))
        {
            udsc_cnx_env_t* p_con_env = p_udsc_env->p_env[conidx];
            uint16_t hdl;
            udsc_uds_content_t* p_uds = &(p_con_env->uds);

            switch(val_id)
            {
                case UDSC_CHAR_UDS_DB_CHG_INC:   { hdl = p_uds->descs[UDSC_DESC_UDS_DB_CHG_INC_CCC].desc_hdl;   } break;
                case UDSC_CHAR_UDS_USER_CTRL_PT: { hdl = p_uds->descs[UDSC_DESC_UDS_USER_CTRL_PT_CCC].desc_hdl; } break;
                default:                         { hdl = GATT_INVALID_HDL;                                      } break;
            }

            if(hdl == GATT_INVALID_HDL)
            {
                status = PRF_ERR_INEXISTENT_HDL;
            }
            else
            {
                uint16_t dummy = 0;
                SETF(dummy, UDSC_VAL_ID,   val_id);
                SETF(dummy, UDSC_VAL_TYPE, UDSC_CCC_CFG);

                // perform read request
                status = gatt_cli_read(conidx, p_udsc_env->user_lid, dummy, hdl, 0, 0);
            }
        }
    }

    return (status);

}

uint16_t udsc_write(uint8_t conidx, uint8_t val_id, const union uds_value* p_value, co_buf_t* p_utf8_name)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    udsc_env_t* p_udsc_env = PRF_ENV_GET(UDSC, udsc);

    do
    {
        udsc_cnx_env_t* p_con_env;
        uint16_t hdl;
        udsc_uds_content_t* p_uds;
        co_buf_t* p_buf = NULL;
        uint16_t dummy = 0;
        SETF(dummy, UDSC_VAL_ID,   val_id);
        SETF(dummy, UDSC_VAL_TYPE, UDSC_USER_VAL);

        if (   (p_udsc_env == NULL) ||(conidx >= BLE_CONNECTION_MAX) || (p_udsc_env->p_env[conidx] == NULL)
            || (p_udsc_env->p_env[conidx]->discover))
        {
            break;
        }

        p_con_env = p_udsc_env->p_env[conidx];
        p_uds = &(p_con_env->uds);
        hdl = p_uds->chars[val_id].val_hdl;

        if(hdl == GATT_INVALID_HDL)
        {
            status = PRF_ERR_INEXISTENT_HDL;
            break;
        }

        if(val_id <= UDS_CHAR_ID_LANGUAGE)
        {
            if(p_utf8_name == NULL)
            {
                status = GAP_ERR_INVALID_PARAM;
                break;
            }
            else if((co_buf_head_len(p_utf8_name) < GATT_BUFFER_HEADER_LEN) || (co_buf_tail_len(p_utf8_name) < GATT_BUFFER_TAIL_LEN))
            {
                status = GAP_ERR_INVALID_BUFFER;
                break;
            }

            p_buf = p_utf8_name;
            co_buf_acquire(p_buf);
        }
        else
        {
            if(p_value == NULL)
            {
                status = GAP_ERR_INVALID_PARAM;
                break;
            }

            if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, 0, L2CAP_LE_MTU_MIN + GATT_BUFFER_TAIL_LEN) != CO_BUF_ERR_NO_ERROR)
            {
                status = GAP_ERR_INSUFF_RESOURCES;
                break;
            }

            // Pack data
            switch (val_id)
            {
                // Date
                case UDS_CHAR_ID_DATE_OF_BIRTH:
                case UDS_CHAR_ID_DATE_OF_THR_ASSESS:
                {
                    prf_pack_date(p_buf, &(p_value->date));
                } break;

                // uint16
                case UDS_CHAR_ID_WEIGHT:
                case UDS_CHAR_ID_HEIGHT:
                case UDS_CHAR_ID_WAIST_CIRCUMFERENCE:
                case UDS_CHAR_ID_HIP_CIRCUMFERENCE:
                {
                    // Characteristic Uint16
                    co_write16p(co_buf_tail(p_buf),  co_htobs(p_value->uint16));
                    co_buf_tail_reserve(p_buf, 2);
                } break;

                // uint8
                case UDS_CHAR_ID_AGE:
                case UDS_CHAR_ID_GENDER:
                case UDS_CHAR_ID_VO2_MAX:
                case UDS_CHAR_ID_MAX_HEART_RATE:
                case UDS_CHAR_ID_RESTING_HEART_RATE:
                case UDS_CHAR_ID_MAX_RECO_HEART_RATE:
                case UDS_CHAR_ID_AEROBIC_THR:
                case UDS_CHAR_ID_ANAERO_THR:
                case UDS_CHAR_ID_SPORT_TYPE_FOR_AERO_ANAREO_THRS:
                case UDS_CHAR_ID_FAT_BURN_HEART_RATE_LOW_LIM:
                case UDS_CHAR_ID_FAT_BURN_HEART_RATE_UP_LIM:
                case UDS_CHAR_ID_AEROBIC_HEART_RATE_LOW_LIM:
                case UDS_CHAR_ID_AEROBIC_HEART_RATE_UP_LIM:
                case UDS_CHAR_ID_ANAERO_HEART_RATE_LOW_LIM:
                case UDS_CHAR_ID_ANAERO_HEART_RATE_UP_LIM:
                case UDS_CHAR_ID_TWO_ZONE_HEART_RATE_LIMITS:
                {
                    co_buf_tail(p_buf)[0] = p_value->uint8;
                    co_buf_tail_reserve(p_buf, 1);
                } break;

                // set
                case UDS_CHAR_ID_FIVE_ZONE_HEART_RATE_LIMITS :
                {
                    co_buf_tail_reserve(p_buf, 4);
                    co_buf_copy_data_from_mem(p_buf, p_value->set, 4);
                } break;

                // set
                case UDS_CHAR_ID_THREE_ZONE_HEART_RATE_LIMITS:
                {
                    co_buf_tail_reserve(p_buf, 2);
                    co_buf_copy_data_from_mem(p_buf, p_value->set, 2);
                } break;

                // Uint32
                case UDS_CHAR_ID_DB_CHG_INC:
                {
                    co_write32p(co_buf_tail(p_buf), co_htobl(p_value->uint16));
                    co_buf_tail_reserve(p_buf, 4);
                } break;

                default: {  /* Nothing to do  */ } break;
            }
        }

        // Return buffer that contains report data requested by peer device
        status = gatt_cli_write(conidx, p_udsc_env->user_lid, dummy, GATT_WRITE, hdl, 0, p_buf);
        if(p_buf != NULL)
        {
            co_buf_release(p_buf);
        }
    } while(0);

    return (status);
}

uint16_t udsc_write_cfg(uint8_t conidx, uint8_t val_id, uint16_t cfg_val)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    udsc_env_t* p_udsc_env = PRF_ENV_GET(UDSC, udsc);

    if(p_udsc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_udsc_env->p_env[conidx] != NULL) && (!p_udsc_env->p_env[conidx]->discover))
        {
            udsc_cnx_env_t* p_con_env = p_udsc_env->p_env[conidx];
            uint16_t hdl;
            uint16_t cfg_en_val = 0;
            udsc_uds_content_t* p_uds = &(p_con_env->uds);

            switch(val_id)
            {
                case UDSC_CHAR_UDS_DB_CHG_INC:   { hdl = p_uds->descs[UDSC_DESC_UDS_DB_CHG_INC_CCC].desc_hdl;
                                                   cfg_en_val =  PRF_CLI_START_NTF;                             } break;
                case UDSC_CHAR_UDS_USER_CTRL_PT: { hdl = p_uds->descs[UDSC_DESC_UDS_USER_CTRL_PT_CCC].desc_hdl;
                                                   cfg_en_val =  PRF_CLI_START_IND;                             } break;
                default:                         { hdl = GATT_INVALID_HDL;                                      } break;
            }

            if(hdl == GATT_INVALID_HDL)
            {
                status = PRF_ERR_INEXISTENT_HDL;
            }
            else if((cfg_val != PRF_CLI_STOP_NTFIND) && (cfg_val != cfg_en_val))
            {
                status = PRF_ERR_INVALID_PARAM;
            }
            else
            {
                uint16_t dummy = 0;
                SETF(dummy, UDSC_VAL_ID,   val_id);
                SETF(dummy, UDSC_VAL_TYPE, UDSC_CCC_CFG);

                // Force endianess
                cfg_val = co_htobs(cfg_val);
                status = prf_gatt_write(conidx, p_udsc_env->user_lid, dummy, GATT_WRITE,
                                        hdl, sizeof(uint16_t), (uint8_t *)&cfg_val);
            }
        }
    }

    return (status);
}

uint16_t udsc_ctnl_pt_req(uint8_t conidx, uint8_t req_op_code, uint8_t user_id, uint16_t consent, co_buf_t* p_param)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    udsc_env_t* p_udsc_env = PRF_ENV_GET(UDSC, udsc);

    if(p_udsc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_udsc_env->p_env[conidx] != NULL) && (!p_udsc_env->p_env[conidx]->discover))
        {
            udsc_cnx_env_t* p_con_env = p_udsc_env->p_env[conidx];
            udsc_uds_content_t* p_uds = &(p_con_env->uds);
            uint16_t hdl = p_uds->chars[UDSC_CHAR_UDS_USER_CTRL_PT].val_hdl;

            do
            {
                co_buf_t* p_buf = NULL;
                uint16_t dummy = 0;
                SETF(dummy, UDSC_VAL_ID,   UDS_CHAR_ID_USER_CTRL_PT);
                SETF(dummy, UDSC_VAL_TYPE, UDSC_CNTL_PT);


                if(hdl == GATT_INVALID_HDL)
                {
                    status = PRF_ERR_INEXISTENT_HDL;
                    break;
                }
                // reject if there is an ongoing control point operation
                if(p_con_env->ctrl_pt_op != UDS_OP_CODE_RESERVED_00)
                {
                    status = PRF_PROC_IN_PROGRESS;
                    break;
                }


                if(p_param != NULL)
                {
                    if((co_buf_head_len(p_param) < UDSC_BUFFER_HEADER_LEN) || (co_buf_tail_len(p_param) < UDSC_BUFFER_TAIL_LEN))
                    {
                        status = GAP_ERR_INVALID_BUFFER;
                        break;
                    }

                    p_buf = p_param;
                }
                else if(co_buf_alloc(&p_buf, UDSC_BUFFER_HEADER_LEN, 0, GATT_BUFFER_TAIL_LEN) != CO_BUF_ERR_NO_ERROR)
                {
                    status = GAP_ERR_INSUFF_RESOURCES;
                }

                // perform packing in reverse order
                switch(req_op_code)
                {
                    case UDS_OP_CODE_REGISTER_NEW_USER:
                    {
                        co_buf_head_reserve(p_buf, 2);
                        co_write16p(co_buf_data(p_buf), co_htobs(consent));
                    } break;

                    case UDS_OP_CODE_CONSENT:
                    {
                        // op-code, user_id and consent code
                        co_buf_head_reserve(p_buf, 2);
                        co_write16p(co_buf_data(p_buf), co_htobs(consent));
                        co_buf_head_reserve(p_buf, 1);
                        co_buf_data(p_buf)[0] = user_id;
                    } break;
                    default: { /* Nothing to do */ } break;
                }

                co_buf_head_reserve(p_buf, 1);
                co_buf_data(p_buf)[0] = req_op_code;

                status = gatt_cli_write(conidx, p_udsc_env->user_lid, dummy, GATT_WRITE, hdl, 0, p_buf);
                if(status == GAP_ERR_NO_ERROR)
                {
                    // save on-going operation
                    p_con_env->ctrl_pt_op = req_op_code;
                }
            } while (0);
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
 * @brief Send a UDSC_CMP_EVT message to the task which enabled the profile
 * @param[in] conidx Connection index
 * @param[in] operation Operation
 * @param[in] status Satus
 ****************************************************************************************
 */
__STATIC void udsc_send_cmp_evt(uint8_t conidx, uint8_t operation, uint16_t status)
{
    struct udsc_cmp_evt *p_evt;

    // Send the message
    p_evt = KE_MSG_ALLOC(UDSC_CMP_EVT, PRF_DST_TASK(UDSC), PRF_SRC_TASK(UDSC), udsc_cmp_evt);
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
__STATIC int udsc_enable_req_handler(ke_msg_id_t const msgid, struct udsc_enable_req const *p_param,
                                     ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = udsc_enable(p_param->conidx, p_param->con_type, &(p_param->uds));

    // send an error if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct udsc_enable_rsp *p_rsp = KE_MSG_ALLOC(UDSC_ENABLE_RSP, src_id, dest_id, udsc_enable_rsp);
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
 * @brief Handles reception of the @ref UDSC_RD_CHAR_CMD  message from the application.
 * @brief To read the Feature Characteristic in the peer server.
 *
 * @param[in] msgid Id of the message received (probably unused).
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int udsc_rd_char_cmd_handler(ke_msg_id_t const msgid, struct udsc_rd_char_cmd *p_param,
                                      ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = udsc_read(p_param->conidx, p_param->char_idx);

    // send error response if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct udsc_cmp_evt *p_evt = KE_MSG_ALLOC(UDSC_CMP_EVT, src_id, dest_id, udsc_cmp_evt);
        if(p_evt)
        {
            p_evt->conidx     = p_param->conidx;
            p_evt->operation  = UDSC_READ_CHAR_OP_CODE;
            p_evt->status     = status;

            ke_msg_send(p_evt);
        }
    }
    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief Handles reception of the @ref UDSC_RD_CHAR_CCC_CMD message from the application.
 * @brief To read the CCC value of the Measurement characteristic in the peer server.
 *
 * @param[in] msgid Id of the message received (probably unused).
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int udsc_rd_char_ccc_cmd_handler(ke_msg_id_t const msgid, struct udsc_rd_char_ccc_cmd *p_param,
                                          ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = udsc_read_cfg(p_param->conidx, p_param->char_idx);

    // send error response if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct udsc_cmp_evt *p_evt = KE_MSG_ALLOC(UDSC_CMP_EVT, src_id, dest_id, udsc_cmp_evt);
        if(p_evt)
        {
            p_evt->conidx     = p_param->conidx;
            p_evt->operation  = UDSC_READ_CCC_OP_CODE;
            p_evt->status     = status;

            ke_msg_send(p_evt);
        }
    }
    return (KE_MSG_CONSUMED);
}



/**
 ****************************************************************************************
 * @brief Handles reception of the @ref UDSC_WR_CHAR_CCC_CMD message.
 * Allows the application to write new CCC values to a Characteristic in the peer server
 * @param[in] msgid Id of the message received.
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int udsc_wr_char_ccc_cmd_handler(ke_msg_id_t const msgid, struct udsc_wr_char_ccc_cmd *p_param,
                                          ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = udsc_write_cfg(p_param->conidx, p_param->char_idx, p_param->ccc);

    // send error response if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct udsc_cmp_evt *p_evt = KE_MSG_ALLOC(UDSC_CMP_EVT, src_id, dest_id, udsc_cmp_evt);
        if(p_evt)
        {
            p_evt->conidx     = p_param->conidx;
            p_evt->operation  = UDSC_WRITE_CCC_OP_CODE;
            p_evt->status     = status;

            ke_msg_send(p_evt);
        }
    }
    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief Handles reception of the @ref UDSC_WR_CHAR_UTF8_CMD message.
 * Allows the application to write new CCC values to a Characteristic in the peer server
 * @param[in] msgid Id of the message received.
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int udsc_wr_char_utf8_cmd_handler(ke_msg_id_t const msgid, struct udsc_wr_char_utf8_cmd *p_param,
                                           ke_task_id_t const dest_id,  ke_task_id_t const src_id)
{
    uint16_t status;
    co_buf_t* p_utf8 = NULL;

    if(co_buf_alloc(&p_utf8, UDSC_BUFFER_HEADER_LEN, p_param->utf_name.length, UDSC_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
    {
        co_buf_copy_data_from_mem(p_utf8, p_param->utf_name.str, p_param->utf_name.length);

        status = udsc_write(p_param->conidx, p_param->char_idx, NULL, p_utf8);

        co_buf_release(p_utf8);
    }
    else
    {
        status = GAP_ERR_INSUFF_RESOURCES;
    }

    // send error response if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct udsc_cmp_evt *p_evt = KE_MSG_ALLOC(UDSC_CMP_EVT, src_id, dest_id, udsc_cmp_evt);
        if(p_evt)
        {
            p_evt->conidx     = p_param->conidx;
            p_evt->operation  = UDSC_WRITE_CHAR_OP_CODE;
            p_evt->status     = status;

            ke_msg_send(p_evt);
        }
    }

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief Handles reception of the @ref UDSC_WR_CHAR_CMD message.
 * Allows the application to write new CCC values to a Characteristic in the peer server
 * @param[in] msgid Id of the message received.
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int udsc_wr_char_cmd_handler(ke_msg_id_t const msgid, struct udsc_wr_char_cmd *p_param,
                                      ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = udsc_write(p_param->conidx, p_param->char_idx, &(p_param->value), NULL);

    // send error response if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct udsc_cmp_evt *p_evt = KE_MSG_ALLOC(UDSC_CMP_EVT, src_id, dest_id, udsc_cmp_evt);
        if(p_evt)
        {
            p_evt->conidx     = p_param->conidx;
            p_evt->operation  = UDSC_WRITE_CHAR_OP_CODE;
            p_evt->status     = status;

            ke_msg_send(p_evt);
        }
    }

    return (KE_MSG_CONSUMED);
}


/**
 ****************************************************************************************
 * @brief Handles reception of the @ref UDSC_WR_USER_CTRL_PT_CMD message.
 * Allows the application to write new CCC values to a Characteristic in the peer server
 * @param[in] msgid Id of the message received.
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int udsc_wr_user_ctrl_pt_cmd_handler(ke_msg_id_t const msgid, struct udsc_wr_user_ctrl_pt_cmd *p_param,
                                              ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status;
    co_buf_t* p_buf = NULL;

    if(co_buf_alloc(&p_buf, UDSC_BUFFER_HEADER_LEN, p_param->length, UDSC_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
    {
        co_buf_copy_data_from_mem(p_buf, p_param->parameter, p_param->length);

        status =  udsc_ctnl_pt_req(p_param->conidx, p_param->op_code, p_param->user_id, p_param->consent, p_buf);

        co_buf_release(p_buf);
    }
    else
    {
        status = GAP_ERR_INSUFF_RESOURCES;
    }

    // send error response if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct udsc_cmp_evt *p_evt = KE_MSG_ALLOC(UDSC_CMP_EVT, src_id, dest_id, udsc_cmp_evt);
        if(p_evt)
        {
            p_evt->conidx     = p_param->conidx;
            p_evt->operation  = UDSC_CTNL_PT_OP_CODE;
            p_evt->status     = status;

            ke_msg_send(p_evt);
        }
    }

    return (KE_MSG_CONSUMED);
}


/// Default State handlers definition
KE_MSG_HANDLER_TAB(udsc)
{
    // Note: all messages must be sorted in ID ascending order

    { UDSC_ENABLE_REQ,                   (ke_msg_func_t) udsc_enable_req_handler          },
    { UDSC_RD_CHAR_CMD,                  (ke_msg_func_t) udsc_rd_char_cmd_handler         },
    { UDSC_RD_CHAR_CCC_CMD,              (ke_msg_func_t) udsc_rd_char_ccc_cmd_handler     },
    { UDSC_WR_CHAR_CMD,                  (ke_msg_func_t) udsc_wr_char_cmd_handler         },
    { UDSC_WR_CHAR_UTF8_CMD,             (ke_msg_func_t) udsc_wr_char_utf8_cmd_handler    },
    { UDSC_WR_USER_CTRL_PT_CMD,          (ke_msg_func_t) udsc_wr_user_ctrl_pt_cmd_handler },
    { UDSC_WR_CHAR_CCC_CMD,              (ke_msg_func_t) udsc_wr_char_ccc_cmd_handler     },

};

/**
 ****************************************************************************************
 * @brief Completion of enable procedure
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 * @param[in] p_uds         Pointer to peer database description bond data
 ****************************************************************************************
 */
__STATIC void udsc_cb_enable_cmp(uint8_t conidx, uint16_t status, const udsc_uds_content_t* p_uds)
{
    // Send APP the details of the discovered attributes on UDSC
    struct udsc_enable_rsp *p_rsp = KE_MSG_ALLOC(UDSC_ENABLE_RSP, PRF_DST_TASK(UDSC), PRF_SRC_TASK(UDSC),
                                                 udsc_enable_rsp);
    if(p_rsp != NULL)
    {
        p_rsp->conidx = conidx;
        p_rsp->status = status;
        memcpy(&(p_rsp->uds), p_uds, sizeof(udsc_uds_content_t));
        ke_msg_send(p_rsp);
    }
}

/**
 ****************************************************************************************
 * @brief Completion of read information procedure.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 * @param[in] val_id        Value identifier (@see enum uds_val_id)
 * @param[in] p_value       Pointer that contains non-UTF-8 value.
 *                          (NULL for a UTF-8 value)
 * @param[in] p_utf8_name   Pointer to buffer that contains UTF-8 name.
 *                          (NULL for an non-UTF-8 value)
 *
 ****************************************************************************************
 */
__STATIC void udsc_cb_read_cmp(uint8_t conidx, uint16_t status, uint8_t val_id, const union uds_value* p_value,
                               co_buf_t* p_utf8_name)
{
    ke_task_id_t src_id  = PRF_SRC_TASK(UDSC);
    ke_task_id_t dest_id = PRF_DST_TASK(UDSC);

    if(status == GAP_ERR_NO_ERROR)
    {
        if(p_value != NULL)
        {
            struct udsc_rd_char_ind *p_ind = KE_MSG_ALLOC(UDSC_RD_CHAR_IND, dest_id, src_id, udsc_rd_char_ind);
            if(p_ind != NULL)
            {
                p_ind->conidx            = conidx;
                p_ind->char_idx          = val_id;
                memcpy(&(p_ind->value), p_value, sizeof(union uds_value));
                ke_msg_send(p_ind);
            }
        }
        else if(p_utf8_name != NULL)
        {
            uint16_t data_len = co_buf_data_len(p_utf8_name);
            struct udsc_rd_char_utf8_ind *p_ind = KE_MSG_ALLOC_DYN(UDSC_RD_CHAR_UTF8_IND, dest_id, src_id,
                                                                   udsc_rd_char_utf8_ind, data_len);
            if(p_ind != NULL)
            {
                p_ind->conidx            = conidx;
                p_ind->char_idx          = val_id;
                p_ind->utf_name.length   = data_len;
                co_buf_copy_data_to_mem(p_utf8_name, p_ind->utf_name.str, data_len);
                ke_msg_send(p_ind);
            }
        }
        else
        {
            ASSERT_ERR(0);
        }
    }

    udsc_send_cmp_evt(conidx, UDSC_READ_CHAR_OP_CODE, status);
}


/**
 ****************************************************************************************
 * @brief Completion of write information procedure.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 * @param[in] val_id        Value identifier (@see enum uds_val_id)
 *
 ****************************************************************************************
 */
__STATIC void udsc_cb_write_cmp(uint8_t conidx, uint16_t status, uint8_t val_id)
{
    udsc_send_cmp_evt(conidx, UDSC_WRITE_CHAR_OP_CODE, status);
}

/**
 ****************************************************************************************
 * @brief Completion of read Characteristic Configuration procedure.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 * @param[in] val_id        CCC value identifier (@see enum uds_val_id)
 *                              - UDS_CHAR_ID_DB_CHG_INC
 *                              - UDS_CHAR_ID_USER_CTRL_PT
 * @param[in] cfg_val       Configuration value
 *
 ****************************************************************************************
 */
__STATIC void udsc_cb_read_cfg_cmp(uint8_t conidx, uint16_t status, uint8_t val_id, uint16_t cfg_val)
{
    ke_task_id_t src_id  = PRF_SRC_TASK(UDSC);
    ke_task_id_t dest_id = PRF_DST_TASK(UDSC);

    if(status == GAP_ERR_NO_ERROR)
    {
        struct udsc_rd_char_ccc_ind *p_ind = KE_MSG_ALLOC(UDSC_RD_CHAR_CCC_IND, dest_id, src_id, udsc_rd_char_ccc_ind);
        if(p_ind != NULL)
        {
            p_ind->conidx            = conidx;
            p_ind->char_idx          = val_id;
            p_ind->ccc               = cfg_val;
            ke_msg_send(p_ind);
        }
    }

    udsc_send_cmp_evt(conidx, UDSC_READ_CCC_OP_CODE, status);
}

/**
 ****************************************************************************************
 * @brief Completion of write Characteristic Configuration procedure.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 * @param[in] val_id        CCC value identifier (@see enum uds_val_id)
 *                              - UDS_CHAR_ID_DB_CHG_INC
 *                              - UDS_CHAR_ID_USER_CTRL_PT
 *
 ****************************************************************************************
 */
__STATIC void udsc_cb_write_cfg_cmp(uint8_t conidx, uint16_t status, uint8_t val_id)
{
    udsc_send_cmp_evt(conidx, UDSC_WRITE_CCC_OP_CODE, status);
}

/**
 ****************************************************************************************
 * @brief Function called when Database change increment update is received
 *
 * @param[in] conidx         Connection index
 * @param[in] p_meas         Database change increment value
 ****************************************************************************************
 */
__STATIC void udsc_cb_db_chg_inc(uint8_t conidx, uint32_t value)
{
    ke_task_id_t src_id  = PRF_SRC_TASK(UDSC);
    ke_task_id_t dest_id = PRF_DST_TASK(UDSC);

    struct udsc_db_chg_inc_ind *p_ind = KE_MSG_ALLOC(UDSC_DB_CHG_INC_IND, dest_id, src_id, udsc_db_chg_inc_ind);
    if(p_ind != NULL)
    {
        p_ind->conidx            = conidx;
        p_ind->value             = value;
        ke_msg_send(p_ind);
    }
}

/**
 ****************************************************************************************
 * @brief Completion of control point request procedure
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the Request Send (@see enum hl_err)
 * @param[in] req_op_code   Requested Operation Code @see enum uds_ctrl_pt_response
 * @param[in] resp_value    Response Value @see enum uds_ctrl_pt_response
 * @param[in] user_id       User ID used for register new user command
 * @param[in] p_param       Pointer buffer value that contains response parameters
 ****************************************************************************************
 */
__STATIC void udsc_cb_ctnl_pt_req_cmp(uint8_t conidx, uint16_t status, uint8_t req_op_code, uint8_t resp_value,
                                      uint8_t user_id, co_buf_t* p_param)
{
    ke_task_id_t src_id  = PRF_SRC_TASK(UDSC);
    ke_task_id_t dest_id = PRF_DST_TASK(UDSC);

    if(status == GAP_ERR_NO_ERROR)
    {
        uint16_t data_len = co_buf_data_len(p_param);

        struct udsc_user_ctrl_pt_ind *p_ind = KE_MSG_ALLOC_DYN(UDSC_USER_CTRL_PT_IND, dest_id, src_id, udsc_user_ctrl_pt_ind, data_len);
        if(p_ind != NULL)
        {
            p_ind->conidx            = conidx;
            p_ind->resp_code         = UDS_OP_CODE_RESPONSE_CODE;
            p_ind->req_op_code       = req_op_code;
            p_ind->resp_value        = resp_value;
            p_ind->user_id           = user_id;
            p_ind->length            = data_len;
            co_buf_copy_data_to_mem(p_param, p_ind->parameter, data_len);;
            ke_msg_send(p_ind);
        }
    }

    udsc_send_cmp_evt(conidx, UDSC_CTNL_PT_OP_CODE, status);
}

/// Default Message handle
__STATIC const udsc_cb_t udsc_msg_cb =
{
     .cb_enable_cmp         = udsc_cb_enable_cmp,
     .cb_read_cmp           = udsc_cb_read_cmp,
     .cb_write_cmp          = udsc_cb_write_cmp,
     .cb_read_cfg_cmp       = udsc_cb_read_cfg_cmp,
     .cb_write_cfg_cmp      = udsc_cb_write_cfg_cmp,
     .cb_db_chg_inc         = udsc_cb_db_chg_inc,
     .cb_ctnl_pt_req_cmp    = udsc_cb_ctnl_pt_req_cmp,
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
__STATIC uint16_t udsc_init(prf_data_t* p_env, uint16_t* p_start_hdl, uint8_t sec_lvl, uint8_t user_prio,
                          const void* p_params, const udsc_cb_t* p_cb)
{
    uint8_t conidx;
    // DB Creation Status
    uint16_t status = GAP_ERR_NO_ERROR;
    uint8_t user_lid = GATT_INVALID_USER_LID;

    do
    {
        udsc_env_t* p_udsc_env;

        #if (BLE_HL_MSG_API)
        if(p_cb == NULL)
        {
            p_cb = &(udsc_msg_cb);
        }
        #endif // (BLE_HL_MSG_API)

        if(   (p_params == NULL) || (p_cb == NULL) || (p_cb->cb_enable_cmp == NULL) || (p_cb->cb_read_cmp == NULL)
           || (p_cb->cb_write_cmp == NULL) || (p_cb->cb_read_cfg_cmp == NULL) || (p_cb->cb_write_cfg_cmp == NULL)
           || (p_cb->cb_db_chg_inc == NULL) || (p_cb->cb_ctnl_pt_req_cmp == NULL))
        {
            status = GAP_ERR_INVALID_PARAM;
            break;
        }

        // register UDSC user
        status = gatt_user_cli_register(UDS_STRING_MAX_SIZE + GATT_WRITE_HEADER_LEN, user_prio, &udsc_cb, &user_lid);
        if(status != GAP_ERR_NO_ERROR) break;

        //-------------------- allocate memory required for the profile  ---------------------
        p_udsc_env = (udsc_env_t*) ke_malloc(sizeof(udsc_env_t), KE_MEM_ATT_DB);

        if(p_udsc_env != NULL)
        {
            // allocate UDSC required environment variable
            p_env->p_env = (prf_hdr_t *) p_udsc_env;

            // initialize environment variable
            p_udsc_env->prf_env.p_cb    = p_cb;
            #if (BLE_HL_MSG_API)
            p_env->desc.msg_handler_tab = udsc_msg_handler_tab;
            p_env->desc.msg_cnt         = ARRAY_LEN(udsc_msg_handler_tab);
            #endif // (BLE_HL_MSG_API)

            p_udsc_env->user_lid = user_lid;
            for (conidx = 0; conidx < BLE_CONNECTION_MAX; conidx++)
            {
                p_udsc_env->p_env[conidx] = NULL;
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
__STATIC uint16_t udsc_destroy(prf_data_t *p_env, uint8_t reason)
{
    uint16_t status = GAP_ERR_NO_ERROR;
    udsc_env_t* p_udsc_env = (udsc_env_t*) p_env->p_env;

    if(reason != PRF_DESTROY_RESET)
    {
        status = gatt_user_unregister(p_udsc_env->user_lid);
    }

    if(status == GAP_ERR_NO_ERROR)
    {
        uint8_t conidx;

        // cleanup environment variable for each task instances
        for (conidx = 0; conidx < BLE_CONNECTION_MAX; conidx++)
        {
            if (p_udsc_env->p_env[conidx] != NULL)
            {
                if(reason != PRF_DESTROY_RESET)
                {
                    co_time_timer_stop(&(p_udsc_env->p_env[conidx]->timer));
                }

                ke_free(p_udsc_env->p_env[conidx]);
            }
        }

        // free profile environment variables
        p_env->p_env = NULL;
        ke_free(p_udsc_env);
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
__STATIC void udsc_con_create(prf_data_t *p_env, uint8_t conidx, const gap_con_param_t* p_con_param)
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
__STATIC void udsc_con_cleanup(prf_data_t *p_env, uint8_t conidx, uint16_t reason)
{
    udsc_env_t* p_udsc_env = (udsc_env_t*) p_env->p_env;

    // clean-up environment variable allocated for task instance
    if (p_udsc_env->p_env[conidx] != NULL)
    {
        co_time_timer_stop(&(p_udsc_env->p_env[conidx]->timer));
        ke_free(p_udsc_env->p_env[conidx]);
        p_udsc_env->p_env[conidx] = NULL;
    }
}

/// UDSC Task interface required by profile manager
const prf_task_cbs_t udsc_itf =
{
    .cb_init          = (prf_init_cb) udsc_init,
    .cb_destroy       = udsc_destroy,
    .cb_con_create    = udsc_con_create,
    .cb_con_cleanup   = udsc_con_cleanup,
    .cb_con_upd       = NULL,
};

/**
 ****************************************************************************************
 * @brief Retrieve client profile interface
 *
 * @return Client profile interface
 ****************************************************************************************
 */
const prf_task_cbs_t* udsc_prf_itf_get(void)
{
    return &udsc_itf;
}


#endif //(BLE_UDS_CLIENT)

/// @} UDS

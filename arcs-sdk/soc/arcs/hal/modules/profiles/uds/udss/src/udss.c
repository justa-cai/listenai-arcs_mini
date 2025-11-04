/**
 ****************************************************************************************
 *
 * @file udss.c
 *
 * @brief User Data Service Profile implementation.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup UDSS
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "btip_config.h"

#if (BLE_UDS_SERVER)

#include "udss.h"
#include "gap.h"
#include "gatt.h"
#include "ble_prf.h"

#include "co_utils.h"
#include "co_endian.h"

#include <string.h>
#include "ke_mem.h"


/*
 * MACROS
 ****************************************************************************************
 */

/*
 * DEFINES
 ****************************************************************************************
 */

#define BITS_IN_BYTE                  (8)


/// Content of UDSS token
enum udss_token_bf
{
    /// GATT procedure token
    UDSS_TOKEN_GATT_TOKEN_MASK = 0x0000FFFF,
    UDSS_TOKEN_GATT_TOKEN_LSB  = 0,
    /// Connection index
    UDSS_TOKEN_CONIDX_MASK     = 0x00FF0000,
    UDSS_TOKEN_CONIDX_LSB      = 16,
    /// Value identifier
    UDSS_TOKEN_VAL_ID_MASK     = 0xFF000000,
    UDSS_TOKEN_VAL_ID_LSB      = 24,
};

/// User Data Service - Attribute List
enum udss_desc_idx
{
    /// User Data Service
    UDSS_IDX_SVC = 0,
    /// DataBase Change Increment
    UDSS_IDX_DB_CHG_INC_CHAR,
    /// value
    UDSS_IDX_DB_CHG_INC_VAL,
    /// CCC descriptor
    UDSS_IDX_DB_CHG_INC_DESC_CCC,
    /// User Index
    UDSS_IDX_USER_INDEX_CHAR,
    /// value
    UDSS_IDX_USER_INDEX_VAL,
    /// User Control Point
    UDSS_IDX_USER_CTRL_PT_CHAR,
    /// value
    UDSS_IDX_USER_CTRL_PT_VAL,
    /// CCC descriptor
    UDSS_IDX_USER_CTRL_PT_DESC_CCC,

    /// Strings: utf8s
    UDS_IDX_FIRST_NAME_CHAR,
    UDS_IDX_FIRST_NAME_VAL,
//    UDS_IDX_FIRST_NAME_EXT,
    UDS_IDX_LAST_NAME_CHAR,
    UDS_IDX_LAST_NAME_VAL,
//    UDS_IDX_LAST_NAME_EXT,
    UDS_IDX_EMAIL_ADDRESS_CHAR,
    UDS_IDX_EMAIL_ADDRESS_VAL,
//    UDS_IDX_EMAIL_ADDRESS_EXT,
    UDS_IDX_LANGUAGE_CHAR,
    UDS_IDX_LANGUAGE_VAL,
//    UDS_IDX_LANGUAGE_EXT,
    /// Date
    UDS_IDX_DATE_OF_BIRTH_CHAR,
    UDS_IDX_DATE_OF_BIRTH_VAL,
    UDS_IDX_DATE_OF_THR_ASSESS_CHAR,
    UDS_IDX_DATE_OF_THR_ASSESS_VAL,
    /// uint16
    UDS_IDX_WEIGHT_CHAR,
    UDS_IDX_WEIGHT_VAL,
    UDS_IDX_HEIGHT_CHAR,
    UDS_IDX_HEIGHT_VAL,
    UDS_IDX_WAIST_CIRCUMFERENCE_CHAR,
    UDS_IDX_WAIST_CIRCUMFERENCE_VAL,
    UDS_IDX_HIP_CIRCUMFERENCE_CHAR,
    UDS_IDX_HIP_CIRCUMFERENCE_VAL,
    /// uint8
    UDS_IDX_AGE_CHAR,
    UDS_IDX_AGE_VAL,
    UDS_IDX_GENDER_CHAR,
    UDS_IDX_GENDER_VAL,
    UDS_IDX_VO2_MAX_CHAR,
    UDS_IDX_VO2_MAX_VAL,
    UDS_IDX_MAX_HEART_RATE_CHAR,
    UDS_IDX_MAX_HEART_RATE_VAL,
    UDS_IDX_RESTING_HEART_RATE_CHAR,
    UDS_IDX_RESTING_HEART_RATE_VAL,
    UDS_IDX_MAX_RECO_HEART_RATE_CHAR,
    UDS_IDX_MAX_RECO_HEART_RATE_VAL,
    UDS_IDX_AEROBIC_THR_CHAR,
    UDS_IDX_AEROBIC_THR_VAL,
    UDS_IDX_ANAERO_THR_CHAR,
    UDS_IDX_ANAERO_THR_VAL,
    UDS_IDX_SPORT_TYPE_FOR_AERO_ANAREO_THRS_CHAR,
    UDS_IDX_SPORT_TYPE_FOR_AERO_ANAREO_THRS_VAL,
    UDS_IDX_FAT_BURN_HEART_RATE_LOW_LIM_CHAR,
    UDS_IDX_FAT_BURN_HEART_RATE_LOW_LIM_VAL,
    UDS_IDX_FAT_BURN_HEART_RATE_UP_LIM_CHAR,
    UDS_IDX_FAT_BURN_HEART_RATE_UP_LIM_VAL,
    UDS_IDX_AEROBIC_HEART_RATE_LOW_LIM_CHAR,
    UDS_IDX_AEROBIC_HEART_RATE_LOW_LIM_VAL,
    UDS_IDX_AEROBIC_HEART_RATE_UP_LIM_CHAR,
    UDS_IDX_AEROBIC_HEART_RATE_UP_LIM_VAL,
    UDS_IDX_ANAERO_HEART_RATE_LOW_LIM_CHAR,
    UDS_IDX_ANAERO_HEART_RATE_LOW_LIM_VAL,
    UDS_IDX_ANAERO_HEART_RATE_UP_LIM_CHAR,
    UDS_IDX_ANAERO_HEART_RATE_UP_LIM_VAL,
    UDS_IDX_TWO_ZONE_HEART_RATE_LIMIT_CHAR,
    UDS_IDX_TWO_ZONE_HEART_RATE_LIMIT_VAL,
    /// set
    UDS_IDX_FIVE_ZONE_HEART_RATE_LIMITS_CHAR,
    UDS_IDX_FIVE_ZONE_HEART_RATE_LIMITS_VAL,
    UDS_IDX_THREE_ZONE_HEART_RATE_LIMITS_CHAR,
    UDS_IDX_THREE_ZONE_HEART_RATE_LIMITS_VAL,

    UDSS_IDX_NB
};

/// Bit field of ntf/ind configuration
enum udss_ntf_ind_cfg_bf
{
    /// DB Change Increment notification
    UDSS_CFG_DB_CHG_INC_NTF_BIT = 0x01,
    UDSS_CFG_DB_CHG_INC_NTF_POS = 0,
    /// Control point indication
    UDSS_CFG_CTRL_PT_IND_BIT    = 0x02,
    UDSS_CFG_CTRL_PT_IND_POS    = 1,
};

/*
 * TYPES DEFINITION
 ****************************************************************************************
 */

/// ongoing operation information
typedef struct udss_buf_meta
{
    /// Operation
    uint8_t   operation;
    /// used to know on which device interval update has been requested, and to prevent
    /// indication to be triggered on this connection index
    uint8_t   conidx;
    /// Handle to notify / indicate
    uint16_t  hdl;
    /// Event type (Notify or indicate)
    uint8_t   evt_type;
} udss_buf_meta_t;


/// Cycling Power Profile Sensor environment variable per connection
typedef struct udss_cnx_env
{
    /// Measurement content mask
    uint16_t mask_meas_content;
} udss_cnx_env_t;

/// UDSS Environment Variable
typedef struct udss_env
{
    /// profile environment
    prf_hdr_t      prf_env;
    /// Operation Event TX wait queue
    co_list_t      wait_queue;
    /// Database configuration - Keeps enabled User Data Characteristics
    uint32_t       char_mask;
    /// Service Attribute Start Handle
    uint16_t       start_hdl;
    /// GATT user local identifier
    uint8_t        user_lid;
    /// Control point operation on-going (@see enum cpp_ctnl_pt_code)
    uint8_t        ctrl_pt_op;
    /// Operation On-going
    bool           op_ongoing;
    /// Prevent recursion in execute_operation function
    bool           in_exe_op;
    ///  Notify/Indication Flags
    uint8_t        ntf_ind_cfg[BLE_CONNECTION_MAX];

} udss_env_t;


/*
 * ATTRIBUTES DATABASE
 ****************************************************************************************
 */
/// default read perm
#define RD_P    (PROP(RD)  | SEC_LVL(RP, NO_AUTH))
/// default write perm
#define WR_P    (PROP(WR)  | SEC_LVL(WP, AUTH))
/// default notify perm
#define NTF_P   (PROP(N)   | SEC_LVL(NIP, AUTH))
/// ind perm
#define IND_P   (PROP(I)   | SEC_LVL(NIP, AUTH))

#define WR_P_UA (PROP(WR)  | PERM(WP, UNAUTH))

/// Full UDSS Database Description - Used to add attributes into the database
const gatt_att16_desc_t udss_att_db[] =
{
    //                                  ATT UUID                      | Permission        | EXT PERM | MAX ATT SIZE
    // User Data Service Service Declaration
    [UDSS_IDX_SVC]                                 = {GATT_DECL_PRIMARY_SERVICE,                 RD_P,                0                         },

    // DataBase Index Increment Characteristic Declaration
    [UDSS_IDX_DB_CHG_INC_CHAR]                     = {GATT_DECL_CHARACTERISTIC,                  RD_P,                0                         },
    // DataBase Index Increment Characteristic Value
    [UDSS_IDX_DB_CHG_INC_VAL]                      = {GATT_CHAR_DB_CHG_INCREMENT,                RD_P | WR_P | NTF_P, sizeof(uint32_t)          },
    // Client Characteristic Configuration Descriptor
    [UDSS_IDX_DB_CHG_INC_DESC_CCC]                 = {GATT_DESC_CLIENT_CHAR_CFG,                 RD_P | WR_P,         OPT(NO_OFFSET)            },

    // User Index Characteristic Declaration
    [UDSS_IDX_USER_INDEX_CHAR]                     = {GATT_DECL_CHARACTERISTIC,                  RD_P,                0                         },
    // User Index Characteristic Value
    [UDSS_IDX_USER_INDEX_VAL]                      = {GATT_CHAR_USER_INDEX,                      RD_P,                sizeof(uint8_t)           },

    // User Control Point Characteristic Declaration
    [UDSS_IDX_USER_CTRL_PT_CHAR]                   = {GATT_DECL_CHARACTERISTIC,                  RD_P,                0                         },
    // User Control Point Characteristic Value
    [UDSS_IDX_USER_CTRL_PT_VAL]                    = {GATT_CHAR_USER_CONTROL_POINT,              WR_P | IND_P,        UDS_USER_CTRL_PT_MAX_LEN  },
    // Client Characteristic Configuration Descriptor
    [UDSS_IDX_USER_CTRL_PT_DESC_CCC]               = {GATT_DESC_CLIENT_CHAR_CFG,                 RD_P | WR_P,         OPT(NO_OFFSET)            },

    // Strings: utf8s
    // User Data Characteristic Declaration
    [UDS_IDX_FIRST_NAME_CHAR]                      = {GATT_DECL_CHARACTERISTIC,                  RD_P,                0                         },
    // User Data Characteristic Value
    [UDS_IDX_FIRST_NAME_VAL]                       = {GATT_CHAR_FIRST_NAME,                      RD_P | WR_P,         UDS_STRING_MAX_SIZE      },
    // Characteristic Extended Properties Descriptor
//    [UDS_IDX_FIRST_NAME_EXT]                       = {GATT_DESC_CHAR_EXT_PROPERTIES,             RD_P,                GATT_EXT_WRITABLE_AUX     },

    // User Data Characteristic Declaration
    [UDS_IDX_LAST_NAME_CHAR]                       = {GATT_DECL_CHARACTERISTIC,                  RD_P,                0                         },
    // User Data Characteristic Value
    [UDS_IDX_LAST_NAME_VAL]                        = {GATT_CHAR_LAST_NAME,                       RD_P | WR_P ,        UDS_STRING_MAX_SIZE      },
    // Characteristic Extended Properties Descriptor
//    [UDS_IDX_LAST_NAME_EXT]                        = {GATT_DESC_CHAR_EXT_PROPERTIES,             RD_P,                GATT_EXT_WRITABLE_AUX     },

    // User Data Characteristic Declaration
    [UDS_IDX_EMAIL_ADDRESS_CHAR]                   = {GATT_DECL_CHARACTERISTIC,                  RD_P,                0                         },
    // User Data Characteristic Value
    [UDS_IDX_EMAIL_ADDRESS_VAL]                    = {GATT_CHAR_EMAIL_ADDRESS,                   RD_P | WR_P,         UDS_STRING_MAX_SIZE      },
    // Characteristic Extended Properties Descriptor
//    [UDS_IDX_EMAIL_ADDRESS_EXT]                    = {GATT_DESC_CHAR_EXT_PROPERTIES,             RD_P,                GATT_EXT_WRITABLE_AUX     },

    // User Data Characteristic Declaration
    [UDS_IDX_LANGUAGE_CHAR]                        = {GATT_DECL_CHARACTERISTIC,                  RD_P,                0                        },
    // User Data Characteristic Value
    [UDS_IDX_LANGUAGE_VAL]                         = {GATT_CHAR_LANGUAGE,                        RD_P | WR_P,         UDS_STRING_MAX_SIZE      },
    // Characteristic Extended Properties Descriptor
//    [UDS_IDX_LANGUAGE_EXT]                         = {GATT_DESC_CHAR_EXT_PROPERTIES,             RD_P,                GATT_EXT_WRITABLE_AUX     },

    // Date:
    // User Data Characteristic Declaration
    [UDS_IDX_DATE_OF_BIRTH_CHAR]                    = {GATT_DECL_CHARACTERISTIC,                  RD_P,                0                         },
    // User Data Characteristic Value
    [UDS_IDX_DATE_OF_BIRTH_VAL]                     = {GATT_CHAR_DATE_OF_BIRTH,                   RD_P | WR_P,         UDS_DATE_MAX_SIZE        },

    // User Data Characteristic Declaration
    [UDS_IDX_DATE_OF_THR_ASSESS_CHAR]               = {GATT_DECL_CHARACTERISTIC,                  RD_P,                0                         },
    // User Data Characteristic Value
    [UDS_IDX_DATE_OF_THR_ASSESS_VAL]                = {GATT_CHAR_DATE_OF_THR_ASSESS,              RD_P | WR_P ,        UDS_DATE_MAX_SIZE        },

    // uint16:
    // User Data Characteristic Declaration
    [UDS_IDX_WEIGHT_CHAR]                           = {GATT_DECL_CHARACTERISTIC,                  RD_P,                0                         },
    // User Data Characteristic Value
    [UDS_IDX_WEIGHT_VAL]                            = {GATT_CHAR_WEIGHT,                          RD_P | WR_P,         UDS_UINT16_MAX_SIZE      },

    // User Data Characteristic Declaration
    [UDS_IDX_HEIGHT_CHAR]                           = {GATT_DECL_CHARACTERISTIC,                  RD_P,                0                         },
    // User Data Characteristic Value
    [UDS_IDX_HEIGHT_VAL]                            = {GATT_CHAR_HEIGHT,                          RD_P | WR_P,         UDS_UINT16_MAX_SIZE      },

    // User Data Characteristic Declaration
    [UDS_IDX_WAIST_CIRCUMFERENCE_CHAR]              = {GATT_DECL_CHARACTERISTIC,                  RD_P,                0                         },
    // User Data Characteristic Value
    [UDS_IDX_WAIST_CIRCUMFERENCE_VAL]               = {GATT_CHAR_WAIST_CIRCUMFERENCE,             RD_P | WR_P,         UDS_UINT16_MAX_SIZE      },

    // User Data Characteristic Declaration
    [UDS_IDX_HIP_CIRCUMFERENCE_CHAR]                = {GATT_DECL_CHARACTERISTIC,                  RD_P,                0                         },
    // User Data Characteristic Value
    [UDS_IDX_HIP_CIRCUMFERENCE_VAL]                 = {GATT_CHAR_HIP_CIRCUMFERENCE,               RD_P | WR_P,         UDS_UINT16_MAX_SIZE      },

    // User Data Characteristic Declaration
    [UDS_IDX_AGE_CHAR]                              = {GATT_DECL_CHARACTERISTIC,                  RD_P,                0                         },
    // User Data Characteristic Value
    [UDS_IDX_AGE_VAL]                               = {GATT_CHAR_AGE,                             RD_P | WR_P,         UDS_UINT8_MAX_SIZE       },

    // User Data Characteristic Declaration
    [UDS_IDX_GENDER_CHAR]                           = {GATT_DECL_CHARACTERISTIC,                  RD_P,                0                         },
    // User Data Characteristic Value
    [UDS_IDX_GENDER_VAL]                            = {GATT_CHAR_GENDER,                          RD_P | WR_P,         UDS_UINT8_MAX_SIZE       },

    // User Data Characteristic Declaration
    [UDS_IDX_VO2_MAX_CHAR]                          = {GATT_DECL_CHARACTERISTIC,                  RD_P,                0                         },
    // User Data Characteristic Value
    [UDS_IDX_VO2_MAX_VAL]                           = {GATT_CHAR_VO2_MAX,                         RD_P | WR_P,         UDS_UINT8_MAX_SIZE       },

    // User Data Characteristic Declaration
    [UDS_IDX_MAX_HEART_RATE_CHAR]                   = {GATT_DECL_CHARACTERISTIC,                  RD_P,                0                         },
    // User Data Characteristic Value
    [UDS_IDX_MAX_HEART_RATE_VAL]                    = {GATT_CHAR_MAX_HEART_RATE,                  RD_P | WR_P,         UDS_UINT8_MAX_SIZE       },

    // User Data Characteristic Declaration
    [UDS_IDX_RESTING_HEART_RATE_CHAR]               = {GATT_DECL_CHARACTERISTIC,                  RD_P,                0                         },
    // User Data Characteristic Value
    [UDS_IDX_RESTING_HEART_RATE_VAL]                = {GATT_CHAR_RESTING_HEART_RATE,              RD_P | WR_P,         UDS_UINT8_MAX_SIZE       },

    // User Data Characteristic Declaration
    [UDS_IDX_MAX_RECO_HEART_RATE_CHAR]              = {GATT_DECL_CHARACTERISTIC,                  RD_P,                0                         },
    // User Data Characteristic Value
    [UDS_IDX_MAX_RECO_HEART_RATE_VAL]               = {GATT_CHAR_MAX_RECO_HEART_RATE,             RD_P | WR_P,         UDS_UINT8_MAX_SIZE       },

    // User Data Characteristic Declaration
    [UDS_IDX_AEROBIC_THR_CHAR]                      = {GATT_DECL_CHARACTERISTIC,                  RD_P,                0                         },
    // User Data Characteristic Value
    [UDS_IDX_AEROBIC_THR_VAL]                       = {GATT_CHAR_AEROBIC_THR,                     RD_P | WR_P,         UDS_UINT8_MAX_SIZE       },

    // User Data Characteristic Declaration
    [UDS_IDX_ANAERO_THR_CHAR]                       = {GATT_DECL_CHARACTERISTIC,                  RD_P,                0                         },
    // User Data Characteristic Value
    [UDS_IDX_ANAERO_THR_VAL]                        = {GATT_CHAR_ANAERO_THR,                      RD_P | WR_P,         UDS_UINT8_MAX_SIZE       },

    // User Data Characteristic Declaration
    [UDS_IDX_SPORT_TYPE_FOR_AERO_ANAREO_THRS_CHAR]  = {GATT_DECL_CHARACTERISTIC,                   RD_P,                0                         },

    // User Data Characteristic Value
    [UDS_IDX_SPORT_TYPE_FOR_AERO_ANAREO_THRS_VAL]   = {GATT_CHAR_SPORT_TYPE_FOR_AERO_ANAREO_THRS,  RD_P | WR_P ,        UDS_UINT8_MAX_SIZE       },

    // User Data Characteristic Declaration
    [UDS_IDX_FAT_BURN_HEART_RATE_LOW_LIM_CHAR]      = {GATT_DECL_CHARACTERISTIC,                   RD_P,                0                         },
    // User Data Characteristic Value
    [UDS_IDX_FAT_BURN_HEART_RATE_LOW_LIM_VAL]       = {GATT_CHAR_FAT_BURN_HEART_RATE_LOW_LIM,      RD_P | WR_P,         UDS_UINT8_MAX_SIZE       },

    // User Data Characteristic Declaration
    [UDS_IDX_FAT_BURN_HEART_RATE_UP_LIM_CHAR]       = {GATT_DECL_CHARACTERISTIC,                   RD_P,                0                         },

    // User Data Characteristic Value
    [UDS_IDX_FAT_BURN_HEART_RATE_UP_LIM_VAL]        = {GATT_CHAR_FAT_BURN_HEART_RATE_UP_LIM,       RD_P | WR_P,         UDS_UINT8_MAX_SIZE       },

    // User Data Characteristic Declaration
    [UDS_IDX_AEROBIC_HEART_RATE_LOW_LIM_CHAR]       = {GATT_DECL_CHARACTERISTIC,                   RD_P,                0                         },
    // User Data Characteristic Value
    [UDS_IDX_AEROBIC_HEART_RATE_LOW_LIM_VAL]        = {GATT_CHAR_AEROBIC_HEART_RATE_LOW_LIM ,      RD_P | WR_P,         UDS_UINT8_MAX_SIZE       },

    // User Data Characteristic Declaration
    [UDS_IDX_AEROBIC_HEART_RATE_UP_LIM_CHAR]        = {GATT_DECL_CHARACTERISTIC,                   RD_P,                0                         },
    // User Data Characteristic Value
    [UDS_IDX_AEROBIC_HEART_RATE_UP_LIM_VAL]         = {GATT_CHAR_AEROBIC_HEART_RATE_UP_LIM ,       RD_P | WR_P,         UDS_UINT8_MAX_SIZE       },

    // User Data Characteristic Declaration
    [UDS_IDX_ANAERO_HEART_RATE_LOW_LIM_CHAR]        = {GATT_DECL_CHARACTERISTIC,                   RD_P,                0                         },
    // User Data Characteristic Value
    [UDS_IDX_ANAERO_HEART_RATE_LOW_LIM_VAL]         = {GATT_CHAR_ANAERO_HEART_RATE_LOW_LIM,        RD_P | WR_P,         UDS_UINT8_MAX_SIZE       },

    // User Data Characteristic Declaration
    [UDS_IDX_ANAERO_HEART_RATE_UP_LIM_CHAR]         = {GATT_DECL_CHARACTERISTIC,                   RD_P,                0                         },
    // User Data Characteristic Value
    [UDS_IDX_ANAERO_HEART_RATE_UP_LIM_VAL]          = {GATT_CHAR_ANAERO_HEART_RATE_UP_LIM,         RD_P | WR_P,         UDS_UINT8_MAX_SIZE       },

    // User Data Characteristic Declaration
    [UDS_IDX_TWO_ZONE_HEART_RATE_LIMIT_CHAR]        = {GATT_DECL_CHARACTERISTIC,                   RD_P,                0                         },
    // User Data Characteristic Value
    [UDS_IDX_TWO_ZONE_HEART_RATE_LIMIT_VAL]         = {GATT_CHAR_TWO_ZONE_HEART_RATE_LIMIT ,       RD_P | WR_P,         UDS_UINT8_MAX_SIZE       },

    // set:
    // User Data Characteristic Declaration
    [UDS_IDX_FIVE_ZONE_HEART_RATE_LIMITS_CHAR]      = {GATT_DECL_CHARACTERISTIC,                   RD_P,                0                         },
    // User Data Characteristic Value
    [UDS_IDX_FIVE_ZONE_HEART_RATE_LIMITS_VAL]       = {GATT_CHAR_FIVE_ZONE_HEART_RATE_LIMITS ,     RD_P | WR_P,         4 * UDS_UINT8_MAX_SIZE   },

    // User Data Characteristic Declaration
    [UDS_IDX_THREE_ZONE_HEART_RATE_LIMITS_CHAR]     = {GATT_DECL_CHARACTERISTIC,                   RD_P,                0                         },
    // User Data Characteristic Value
    [UDS_IDX_THREE_ZONE_HEART_RATE_LIMITS_VAL]      = {GATT_CHAR_THREE_ZONE_HEART_RATE_LIMITS ,    RD_P | WR_P,         2 * UDS_UINT8_MAX_SIZE   },
};


/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */
__STATIC void udss_exe_operation(udss_env_t* p_udss_env);


/// Get database attribute index
__STATIC uint8_t udss_val_id_get(udss_env_t* p_udss_env, uint16_t hdl)
{
    uint8_t att_idx = hdl - p_udss_env->start_hdl;
    uint8_t val_id  = 0;

    switch(att_idx)
    {
        // DataBase Change Increment
        case UDSS_IDX_DB_CHG_INC_VAL:
        case UDSS_IDX_DB_CHG_INC_DESC_CCC:   { val_id = UDS_CHAR_ID_DB_CHG_INC;   } break;
        // User Index
        case UDSS_IDX_USER_INDEX_VAL:        { val_id = UDS_CHAR_ID_USER_INDEX;   } break;
        // User Control Point
        case UDSS_IDX_USER_CTRL_PT_VAL:
        case UDSS_IDX_USER_CTRL_PT_DESC_CCC: { val_id = UDS_CHAR_ID_USER_CTRL_PT; } break;

        default:
        {
            uint32_t mask = p_udss_env->char_mask; // bit field of enabled characteristics
            int i;

            att_idx -= UDS_IDX_FIRST_NAME_CHAR; // extract all permanent characteristics

            // 28 characteristics have 2 attributes(handles)
            for (i = 0; i < (UDS_CHAR_ID_THREE_ZONE_HEART_RATE_LIMITS - UDS_CHAR_ID_FIRST_NAME + 1); i++)
            {
                if (mask & 1)
                {
                    if (att_idx >= 2)
                    {
                        att_idx -= 2;
                    }
                    else
                    {
                        break;
                    }
                }
                val_id ++;
                mask >>= 1;
            }
        } break;
    }

    return val_id;
}

/**
 ****************************************************************************************
 * @brief Unpack control point data and process it
 *
 * @param[in] p_udss_env Environment
 * @param[in] conidx     connection index
 * @param[in] p_buf      pointer to input data
 ****************************************************************************************
 */
__STATIC uint16_t udss_unpack_ctnl_point_req(udss_env_t *p_udss_env, uint8_t conidx, co_buf_t* p_buf)
{
    uint8_t   op_code;
    uint16_t  status = GAP_ERR_NO_ERROR;
    uint8_t   user_id = 0;
    uint16_t  consent = 0;
    co_buf_t* p_param = NULL;
    uint8_t   ctnl_pt_rsp_status = UDS_OP_RESPONSE_SUCCESS;

    op_code = co_buf_data(p_buf)[0];

    if(co_buf_head_release(p_buf, 1) == CO_BUF_ERR_NO_ERROR)
    {
        switch(op_code)
        {
            case UDS_OP_RESPONSE_RESERVED_00:
            case UDS_OP_CODE_RESPONSE_CODE:
            {
                ctnl_pt_rsp_status = UDS_OP_RESPONSE_OP_CODE_NOT_SUPPORTED;
            } break;
            case UDS_OP_CODE_REGISTER_NEW_USER:
            {
                if(co_buf_data_len(p_buf) < 2)
                {
                    ctnl_pt_rsp_status = UDS_OP_RESPONSE_INVALID_PARAMETER;
                    break;
                }

                consent = co_btohs(co_read16p(co_buf_data(p_buf)));
                co_buf_head_release(p_buf, 2);
            } break;

            case UDS_OP_CODE_CONSENT:
            {
                if(co_buf_data_len(p_buf) < 3)
                {
                    ctnl_pt_rsp_status = UDS_OP_RESPONSE_INVALID_PARAMETER;
                    break;
                }

                // op-code, user_id and consent code
                user_id = co_buf_data(p_buf)[0];
                co_buf_head_release(p_buf, 1);
                consent = co_btohs(co_read16p(co_buf_data(p_buf)));
                co_buf_head_release(p_buf, 2);
            } break;

            case UDS_OP_CODE_DELETE_USER_DATA:
            {
                // nothing to do
            } break;

            default:
            {
                // unknown op-code - let app decide
                if(co_buf_data_len(p_buf) < UDS_USER_CTRL_PT_MAX_LEN - 1)
                {
                    p_param = p_buf;
                }
                // else does not provide anything else
            } break;
        }
    }

    // If no error raised, inform the application about the request
    if (ctnl_pt_rsp_status == UDS_OP_RESPONSE_SUCCESS)
    {
        const udss_cb_t* p_cb  = (const udss_cb_t*) p_udss_env->prf_env.p_cb;
        p_udss_env->ctrl_pt_op = op_code;

        // inform application about control point request
        p_cb->cb_ctnl_pt_req(conidx, op_code, user_id, consent, p_param);
    }
    else
    {
        co_buf_t* p_out_buf = NULL;

        if(co_buf_alloc(&p_out_buf, GATT_BUFFER_HEADER_LEN, 0, UDS_USER_CTRL_PT_MAX_LEN + GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
        {
            udss_buf_meta_t* p_meta = (udss_buf_meta_t*)co_buf_metadata(p_out_buf);

            p_udss_env->ctrl_pt_op    = UDS_OP_CODE_RESPONSE_CODE;
            co_buf_tail(p_out_buf)[0] = UDS_OP_CODE_RESPONSE_CODE;
            co_buf_tail_reserve(p_out_buf, 1);
            co_buf_tail(p_out_buf)[0] = op_code;
            co_buf_tail_reserve(p_out_buf, 1);
            co_buf_tail(p_out_buf)[0] = ctnl_pt_rsp_status;
            co_buf_tail_reserve(p_out_buf, 1);

            p_meta->conidx    = conidx;
            p_meta->operation = UDSS_CTRL_PT_RSP_SEND_OP_CODE;
            p_meta->hdl       = p_udss_env->start_hdl + UDSS_IDX_USER_CTRL_PT_VAL;
            p_meta->evt_type  = GATT_INDICATE;

            // put event on wait queue
            co_list_push_back(&(p_udss_env->wait_queue), &(p_out_buf->hdr));
            // execute operation
            udss_exe_operation(p_udss_env);
        }
        else
        {
            status = ATT_ERR_INSUFF_RESOURCE;
        }
    }

    return (status);
}

/**
 ****************************************************************************************
 * @brief  This function fully manages notification of measurement and vector
 ****************************************************************************************
 */
__STATIC void udss_exe_operation(udss_env_t* p_udss_env)
{
    if(!p_udss_env->in_exe_op)
    {
        p_udss_env->in_exe_op = true;

        while(!co_list_is_empty(&(p_udss_env->wait_queue)) && !(p_udss_env->op_ongoing))
        {
            const udss_cb_t* p_cb = (const udss_cb_t*) p_udss_env->prf_env.p_cb;
            uint16_t status = GAP_ERR_NO_ERROR;
            co_buf_t* p_buf = (co_buf_t*) co_list_pop_front(&(p_udss_env->wait_queue));
            udss_buf_meta_t* p_meta = (udss_buf_meta_t*) co_buf_metadata(p_buf);
            uint8_t conidx    = p_meta->conidx;
            uint8_t operation = p_meta->operation;

            status = gatt_srv_event_send(conidx, p_udss_env->user_lid, p_meta->operation, p_meta->evt_type,
                                         p_meta->hdl, p_buf);
            co_buf_release(p_buf);

            if(status == GAP_ERR_NO_ERROR)
            {
                p_udss_env->op_ongoing = true;
            }
            else
            {
                switch(operation)
                {
                    case UDSS_DB_CHG_INC_UPD_OP_CODE:
                    {
                        p_cb->cb_db_chg_inc_upd_cmp(conidx, status);
                    } break;

                    case UDSS_CTRL_PT_RSP_SEND_OP_CODE:
                    {
                        // Inform application that control point response has been sent
                        if (p_udss_env->ctrl_pt_op != UDS_OP_CODE_RESPONSE_CODE)
                        {
                            p_cb->cb_ctnl_pt_rsp_send_cmp(conidx, status);
                        }

                        // consider control point operation done
                        p_udss_env->ctrl_pt_op = UDS_OP_CODE_RESERVED_00;
                    } break;

                    default: { ASSERT_ERR(0); } break;
                }
            }
        }

        p_udss_env->in_exe_op = false;
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
__STATIC void udss_cb_att_read_get(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset,
                                   uint16_t max_length)
{
    udss_env_t *p_udss_env = PRF_ENV_GET(UDSS, udss);
    // retrieve value attribute
    uint16_t  ntf_ind_cfg = 0;
    uint16_t  status      = GAP_ERR_NO_ERROR;
    bool      send_cfm    = true;

    if(p_udss_env == NULL)
    {
        status = PRF_APP_ERROR;
    }
    else
    {
        uint8_t att_idx = hdl - p_udss_env->start_hdl;

        switch (att_idx)
        {
            case UDSS_IDX_DB_CHG_INC_DESC_CCC:
            {
                ntf_ind_cfg = GETB(p_udss_env->ntf_ind_cfg[conidx], UDSS_CFG_DB_CHG_INC_NTF)
                            ? PRF_CLI_START_NTF : PRF_CLI_STOP_NTFIND;
            } break;

            case UDSS_IDX_USER_CTRL_PT_DESC_CCC:
            {
                ntf_ind_cfg = GETB(p_udss_env->ntf_ind_cfg[conidx], UDSS_CFG_CTRL_PT_IND)
                            ? PRF_CLI_START_IND : PRF_CLI_STOP_NTFIND;

            } break;

            default:
            {
                const udss_cb_t* p_cb  = (const udss_cb_t*) p_udss_env->prf_env.p_cb;
                uint32_t udss_token = 0;
                // retrieve handle information
                uint8_t val_id = udss_val_id_get(p_udss_env, hdl);


                if (val_id == UDS_CHAR_ID_UNKNOWN)
                {
                    status = ATT_ERR_REQUEST_NOT_SUPPORTED;
                    break;
                }

                // does not accept offset for non-UTF-8 values
                if((val_id > UDS_CHAR_ID_LANGUAGE) && (offset > 0))
                {
                    status = PRF_APP_ERROR;
                }

                // Ask application to provide user data information
                send_cfm = false;
                SETF(udss_token, UDSS_TOKEN_CONIDX,     conidx);
                SETF(udss_token, UDSS_TOKEN_GATT_TOKEN, token);
                SETF(udss_token, UDSS_TOKEN_VAL_ID,     val_id);
                p_cb->cb_read_req(conidx, udss_token, val_id, offset, max_length);
            } break;
        }
    }

    if(send_cfm)
    {
        co_buf_t* p_buf;
        uint16_t  att_val_len = 2;

        if(status == GAP_ERR_NO_ERROR)
        {
            // provide CCC information
            if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, sizeof(uint16_t),  GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
            {
                co_write16p(co_buf_data(p_buf), co_htobs(ntf_ind_cfg));
            }
            else
            {
                status = ATT_ERR_INSUFF_RESOURCE;
            }
        }

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
__STATIC void udss_cb_att_val_set(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset,
                                  co_buf_t* p_data)
{
    bool send_cfm = true;
    udss_env_t *p_udss_env = PRF_ENV_GET(UDSS, udss);
    uint16_t  status      = PRF_APP_ERROR;

    if(p_udss_env != NULL)
    {
        uint8_t  cfg_upd_flag  = 0;
        uint16_t cfg_en_val    = 0;
        uint8_t  att_idx = hdl - p_udss_env->start_hdl;
        uint8_t  val_id        = udss_val_id_get(p_udss_env, hdl);;

        switch (att_idx)
        {
            case UDSS_IDX_DB_CHG_INC_DESC_CCC:
            {
                cfg_upd_flag = UDSS_CFG_DB_CHG_INC_NTF_BIT;
                cfg_en_val   = PRF_CLI_START_NTF;
            } break;
            case UDSS_IDX_USER_CTRL_PT_DESC_CCC:
            {
                cfg_upd_flag = UDSS_CFG_CTRL_PT_IND_BIT;
                cfg_en_val   = PRF_CLI_START_IND;
            } break;

            case UDSS_IDX_USER_CTRL_PT_VAL:
            {
                // Check if sending of indications has been enabled
                if (!GETB(p_udss_env->ntf_ind_cfg[conidx], UDSS_CFG_CTRL_PT_IND))
                {
                    // CPP improperly configured
                    status = PRF_CCCD_IMPR_CONFIGURED;
                }
                else if (p_udss_env->ctrl_pt_op != UDS_OP_CODE_RESERVED_00)
                {
                    // A procedure is already in progress
                    status = PRF_PROC_IN_PROGRESS;
                }
                else
                {
                    // Unpack Control Point parameters
                    status = udss_unpack_ctnl_point_req(p_udss_env, conidx, p_data);
                }
            } break;

            default:
            {
                union uds_value value;
                co_buf_t* p_utf8 = NULL;
                union uds_value* p_value = &value;
                memset(p_value, 0, sizeof(union uds_value));
                status = GAP_ERR_NO_ERROR;

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
                        // reject invalid value length
                        if(co_buf_data_len(p_data) < 4) { status = PRF_APP_ERROR; break; };
                        prf_unpack_date(p_data, &(p_value->date));
                    } break;

                    // uint16
                    case UDS_CHAR_ID_WEIGHT:
                    case UDS_CHAR_ID_HEIGHT:
                    case UDS_CHAR_ID_WAIST_CIRCUMFERENCE:
                    case UDS_CHAR_ID_HIP_CIRCUMFERENCE:
                    {
                        // reject invalid value length
                        if(co_buf_data_len(p_data) < 2) { status = PRF_APP_ERROR; break; };
                        // Characteristic Uint16
                        p_value->uint16 = co_btohs(co_read16p(co_buf_data(p_data)));
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
                        // reject invalid value length
                        if(co_buf_data_len(p_data) < 1) { status = PRF_APP_ERROR; break; };
                        // Characteristic Uint8
                        p_value->uint8 = co_buf_data(p_data)[0];
                    } break;

                    // set
                    case UDS_CHAR_ID_FIVE_ZONE_HEART_RATE_LIMITS :
                    {
                        // reject invalid value length
                        if(co_buf_data_len(p_data) < 4) { status = PRF_APP_ERROR; break; };
                        co_buf_copy_data_to_mem(p_data, p_value->set, 4);
                    } break;

                    // set
                    case UDS_CHAR_ID_THREE_ZONE_HEART_RATE_LIMITS:
                    {
                        if(co_buf_data_len(p_data) < 2) { status = PRF_APP_ERROR; break; };
                        co_buf_copy_data_to_mem(p_data, p_value->set, 2);
                    } break;

                    // Uint32
                    case UDS_CHAR_ID_DB_CHG_INC:
                    {
                        // reject invalid value length
                        if(co_buf_data_len(p_data) < 4) { status = PRF_APP_ERROR; break; };
                        p_value->db_chg_inc = co_btohl(co_read32p(co_buf_data(p_data)));
                    } break;

                    default: //UDS_CHAR_ID_UNKNOWN
                    {
                        status = ATT_ERR_REQUEST_NOT_SUPPORTED;
                    } break;
                }

                if(status == GAP_ERR_NO_ERROR)
                {
                    const udss_cb_t* p_cb  = (const udss_cb_t*) p_udss_env->prf_env.p_cb;
                    uint32_t udss_token = 0;

                    // Ask application to provide user data information
                    send_cfm = false;
                    SETF(udss_token, UDSS_TOKEN_CONIDX,     conidx);
                    SETF(udss_token, UDSS_TOKEN_GATT_TOKEN, token);
                    SETF(udss_token, UDSS_TOKEN_VAL_ID,     val_id);
                    p_cb->cb_write_req(conidx, udss_token, val_id, p_value, p_utf8);
                }
            } break;
        }

        if(cfg_upd_flag != 0)
        {
            uint16_t cfg = co_btohs(co_read16p(co_buf_data(p_data)));

            // parameter check
            if(   (co_buf_data_len(p_data) == sizeof(uint16_t))
               && ((cfg == PRF_CLI_STOP_NTFIND) || (cfg == cfg_en_val)))
            {
                const udss_cb_t* p_cb  = (const udss_cb_t*) p_udss_env->prf_env.p_cb;

                if(cfg == PRF_CLI_STOP_NTFIND)
                {
                    p_udss_env->ntf_ind_cfg[conidx] &= ~cfg_upd_flag;
                }
                else
                {
                    p_udss_env->ntf_ind_cfg[conidx] |= cfg_upd_flag;
                }

                // inform application about update
                p_cb->cb_bond_data_upd(conidx, val_id, cfg);
                status = GAP_ERR_NO_ERROR;
            }
            else
            {
                status = PRF_CCCD_IMPR_CONFIGURED;
            }
        }
    }

    if(send_cfm)
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
__STATIC void udss_cb_event_sent(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    // Consider job done
    udss_env_t *p_udss_env = PRF_ENV_GET(UDSS, udss);
    if(p_udss_env != NULL)
    {
        const udss_cb_t* p_cb  = (const udss_cb_t*) p_udss_env->prf_env.p_cb;
        p_udss_env->op_ongoing = false;

        switch(dummy)
        {
            case UDSS_DB_CHG_INC_UPD_OP_CODE:
            {
                p_cb->cb_db_chg_inc_upd_cmp(conidx, status);
            } break;
            default:
            {
                // Inform application that control point response has been sent
                if (p_udss_env->ctrl_pt_op != UDS_OP_CODE_RESPONSE_CODE)
                {
                    p_cb->cb_ctnl_pt_rsp_send_cmp(conidx, status);
                }

                p_udss_env->ctrl_pt_op = UDS_OP_CODE_RESERVED_00;
            } break;
        }

        // continue operation execution
        udss_exe_operation(p_udss_env);
    }
}

/// Service callback hander
__STATIC const gatt_srv_cb_t udss_cb =
{
        .cb_event_sent    = udss_cb_event_sent,
        .cb_att_read_get  = udss_cb_att_read_get,
        .cb_att_event_get = NULL,
        .cb_att_info_get  = NULL,
        .cb_att_val_set   = udss_cb_att_val_set,
};

/*
 * PROFILE NATIVE HANDLERS
 ****************************************************************************************
 */

uint16_t udss_enable(uint8_t conidx, uint16_t db_chg_inc_ccc, uint16_t user_ctrl_pt_ccc)
{
    udss_env_t* p_udss_env = PRF_ENV_GET(UDSS, udss);
    uint16_t status = PRF_ERR_REQ_DISALLOWED;

    if(p_udss_env != NULL)
    {
        // check state of the task
        if (gapc_get_conhdl(conidx) != GAP_INVALID_CONHDL)
        {
            SETB(p_udss_env->ntf_ind_cfg[conidx], UDSS_CFG_DB_CHG_INC_NTF, (db_chg_inc_ccc != 0));
            SETB(p_udss_env->ntf_ind_cfg[conidx], UDSS_CFG_CTRL_PT_IND,    (user_ctrl_pt_ccc != 0));
            status = GAP_ERR_NO_ERROR;
        }
    }

    return (status);
}


uint16_t udss_db_chg_inc_upd(uint8_t conidx, uint32_t value)
{
    udss_env_t* p_udss_env = PRF_ENV_GET(UDSS, udss);
    uint16_t status = PRF_ERR_REQ_DISALLOWED;

    if(p_udss_env != NULL)
    {
        co_buf_t* p_buf;

        if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, 4, GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
        {
            udss_buf_meta_t* p_buf_meta = (udss_buf_meta_t*) co_buf_metadata(p_buf);
            p_buf_meta->operation = UDSS_DB_CHG_INC_UPD_OP_CODE;
            p_buf_meta->conidx    = conidx;
            p_buf_meta->hdl       = p_udss_env->start_hdl + UDSS_IDX_DB_CHG_INC_VAL;
            p_buf_meta->evt_type  = GATT_NOTIFY;

            // set buffer data
            co_write32p(co_buf_data(p_buf), co_htobl(value));
            // put event on wait queue
            co_list_push_back(&(p_udss_env->wait_queue), &(p_buf->hdr));
            // execute operation
            udss_exe_operation(p_udss_env);
            status = GAP_ERR_NO_ERROR;
        }
        else
        {
            status = GAP_ERR_INSUFF_RESOURCES;
        }
    }

    return (status);
}

uint16_t udss_read_cfm(uint8_t conidx, uint32_t token, uint8_t val_id, uint16_t status, const union uds_value* p_value,
                       uint16_t total_len, co_buf_t* p_utf8_name)
{
    udss_env_t* p_udss_env = PRF_ENV_GET(UDSS, udss);

    if((conidx != GETF(token, UDSS_TOKEN_CONIDX)) || (val_id != GETF(token, UDSS_TOKEN_VAL_ID)))
    {
       status = GAP_ERR_INVALID_PARAM;
    }
    else if(p_udss_env != NULL)
    {
        do
        {
            co_buf_t* p_buf = NULL;
            uint16_t  data_len = 0;

            if(status == GAP_ERR_NO_ERROR)
            {
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
                    data_len = total_len;
                }
                else
                {
                    if(p_value == NULL)
                    {
                        status = GAP_ERR_INVALID_PARAM;
                        break;
                    }

                    if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, 0, L2CAP_LE_MTU_MIN + GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
                    {
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

                            default: //UDS_CHAR_ID_UNKNOWN
                            {
                                status = ATT_ERR_REQUEST_NOT_SUPPORTED;
                            } break;
                        }

                        data_len = co_buf_data_len(p_buf);
                    }
                    else
                    {
                        status = ATT_ERR_INSUFF_RESOURCE;
                    }
                }
            }

            // Return buffer that contains report data requested by peer device
            status = gatt_srv_att_read_get_cfm(conidx, p_udss_env->user_lid, GETF(token, UDSS_TOKEN_GATT_TOKEN),
                                               status, data_len, p_buf);
            if(p_buf != NULL)
            {
                co_buf_release(p_buf);
            }

        } while(0);
    }
    else
    {
        status = PRF_ERR_REQ_DISALLOWED;
    }

    return (status);
}

uint16_t udss_write_cfm(uint8_t conidx, uint32_t token, uint8_t val_id, uint16_t status)
{
    udss_env_t* p_udss_env = PRF_ENV_GET(UDSS, udss);

    if((conidx != GETF(token, UDSS_TOKEN_CONIDX)) || (val_id != GETF(token, UDSS_TOKEN_VAL_ID)))
    {
       status = GAP_ERR_INVALID_PARAM;
    }
    else if(p_udss_env != NULL)
    {
        // Return buffer that contains report data requested by peer device
        status = gatt_srv_att_val_set_cfm(conidx, p_udss_env->user_lid, GETF(token, UDSS_TOKEN_GATT_TOKEN), status);
    }
    else
    {
        status = PRF_ERR_REQ_DISALLOWED;
    }

    return (status);
}


uint16_t udss_ctnl_pt_rsp_send(uint8_t conidx, uint8_t op_code, uint8_t resp_val, co_buf_t* p_param)
{
    udss_env_t* p_udss_env = PRF_ENV_GET(UDSS, udss);
    uint16_t status = PRF_ERR_REQ_DISALLOWED;

    if(p_udss_env != NULL)
    {
        do
        {
            co_buf_t* p_buf = NULL;
            udss_buf_meta_t* p_buf_meta;

            // Check the current operation
            if (p_udss_env->ctrl_pt_op == UDS_OP_CODE_RESERVED_00)
            {
                // The confirmation has been sent without request indication, ignore
                break;
            }

            // Check if sending of indications has been enabled
            if (!GETB(p_udss_env->ntf_ind_cfg[conidx], UDSS_CFG_CTRL_PT_IND))
            {
                // mark operation done
                p_udss_env->ctrl_pt_op = UDS_OP_CODE_RESERVED_00;
                // CPP improperly configured
                status = PRF_CCCD_IMPR_CONFIGURED;
                break;
            }

            if(p_param != NULL)
            {
                if((co_buf_head_len(p_param) < UDSS_BUFFER_HEADER_LEN) || (co_buf_tail_len(p_param) < UDSS_BUFFER_TAIL_LEN))
                {
                    status = GAP_ERR_INVALID_BUFFER;
                    break;
                }

                p_buf = p_param;
            }
            else if(co_buf_alloc(&p_buf, UDSS_BUFFER_HEADER_LEN, 0, GATT_BUFFER_TAIL_LEN) != CO_BUF_ERR_NO_ERROR)
            {
                status = GAP_ERR_INSUFF_RESOURCES;
            }

            p_buf_meta = (udss_buf_meta_t*) co_buf_metadata(p_buf);
            p_buf_meta->operation = UDSS_CTRL_PT_RSP_SEND_OP_CODE;
            p_buf_meta->conidx    = conidx;
            p_buf_meta->hdl       = p_udss_env->start_hdl + UDSS_IDX_USER_CTRL_PT_VAL;
            p_buf_meta->evt_type  = GATT_INDICATE;

            // do it in reverse order
            co_buf_head_reserve(p_buf, 1);
            co_buf_data(p_buf)[0] = resp_val;
            co_buf_head_reserve(p_buf, 1);
            co_buf_data(p_buf)[0] = op_code;
            co_buf_head_reserve(p_buf, 1);
            co_buf_data(p_buf)[0] = UDS_OP_CODE_RESPONSE_CODE;

            // put event on wait queue
            co_list_push_back(&(p_udss_env->wait_queue), &(p_buf->hdr));
            // execute operation
            udss_exe_operation(p_udss_env);
            status = GAP_ERR_NO_ERROR;

        } while(0);
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
 * @brief Send a UDSS_CMP_EVT message to the application.
 * @param[in] conidx connection index
 * @param[in] operation Operation completed
 * @param[in] status status of the operation
 ****************************************************************************************
 */
__STATIC void udss_send_cmp_evt(uint8_t conidx, uint8_t operation, uint16_t status)
{
    struct udss_cmp_evt *p_evt;

    // Send the message
    p_evt = KE_MSG_ALLOC(UDSS_CMP_EVT, PRF_DST_TASK(UDSS), PRF_SRC_TASK(UDSS), udss_cmp_evt);

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
 * @brief Handles reception of the @ref UDSS_ENABLE_REQ message.
 * @param[in] msgid Id of the message received
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int udss_enable_req_handler(ke_msg_id_t const msgid, struct udss_enable_req *p_param,
                                     ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    struct udss_enable_rsp *p_cmp_evt;
    uint16_t status = udss_enable(p_param->conidx, p_param->db_chg_inc_ccc, p_param->user_ctrl_pt_ccc);

    // send completed information to APP task that contains error status
    p_cmp_evt = KE_MSG_ALLOC(UDSS_ENABLE_RSP, src_id, dest_id, udss_enable_rsp);

    if(p_cmp_evt)
    {
        p_cmp_evt->conidx     = p_param->conidx;
        p_cmp_evt->status     = status;
        ke_msg_send(p_cmp_evt);
    }

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief Handles reception of the @ref UDSS_DB_CHG_INC_UPD_CMD message. APP request to Send IND.
 * @param[in] msgid Id of the message received.
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int udss_db_chg_inc_upd_cmd_handler(ke_msg_id_t const msgid, struct udss_db_chg_inc_upd_cmd *p_param,
                                             ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = udss_db_chg_inc_upd(p_param->conidx, p_param->value);

    if(status != GAP_ERR_NO_ERROR)
    {
        udss_send_cmp_evt(p_param->conidx, UDSS_DB_CHG_INC_UPD_OP_CODE, status);
    }

    return (KE_MEM_KE_MSG);
}



/**
 ****************************************************************************************
 * @brief Handles reception of the @ref UDSS_USER_CTRL_PT_RSP_SEND_CMD message. APP request to Send IND.
 * @param[in] msgid Id of the message received.
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int udss_user_ctrl_pt_rsp_send_cmd_handler(ke_msg_id_t const msgid, struct udss_user_ctrl_pt_rsp_send_cmd *p_param,
                                                    ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    co_buf_t* p_buf = NULL;
    uint16_t status = GAP_ERR_NO_ERROR;
    uint16_t param_len = p_param->length;

    if(param_len > 0)
    {
        // ensure that no too much parameters data are sent
        if(param_len >UDS_USER_CTRL_PT_MAX_LEN)
        {
            param_len = UDS_USER_CTRL_PT_MAX_LEN;
        }

        if(co_buf_alloc(&p_buf, UDSS_BUFFER_HEADER_LEN, param_len, UDSS_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
        {
            co_buf_copy_data_from_mem(p_buf, p_param->parameter, param_len);
        }
        else
        {
            status = GAP_ERR_INSUFF_RESOURCES;
        }
    }

    if(status == GAP_ERR_NO_ERROR)
    {
        status = udss_ctnl_pt_rsp_send(p_param->conidx, p_param->req_op_code, p_param->resp_value, p_buf);
    }

    if(status != GAP_ERR_NO_ERROR)
    {
        udss_send_cmp_evt(p_param->conidx, UDSS_CTRL_PT_RSP_SEND_OP_CODE, status);
    }

    return (KE_MEM_KE_MSG);
}




/**
 ****************************************************************************************
 * @brief Handles reception of the @ref UDSS_WR_CHAR_CFM message. Confirmation from the APP.
 * @param[in] msgid Id of the message received.
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int udss_wr_char_cfm_handler(ke_msg_id_t const msgid, struct udss_wr_char_cfm *p_param,
                                      ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    udss_write_cfm(p_param->conidx, p_param->token, p_param->char_idx, p_param->status);

    return (KE_MSG_CONSUMED);
}


/**
 ****************************************************************************************
 * @brief Handles reception of the @ref UDSS_RD_CHAR_UTF8_CFM message. APP read response of one of the CCC read.
 * @param[in] msgid Id of the message received.
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int udss_rd_char_utf8_cfm_handler(ke_msg_id_t const msgid, struct udss_rd_char_utf8_cfm *p_param,
                                           ke_task_id_t const dest_id,  ke_task_id_t const src_id)
{
    co_buf_t* p_utf8 = NULL;

    if(p_param->status == GAP_ERR_NO_ERROR)
    {
        if(co_buf_alloc(&p_utf8, UDSS_BUFFER_HEADER_LEN, p_param->utf_name.length, GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
        {
            co_buf_copy_data_from_mem(p_utf8, p_param->utf_name.str, p_param->utf_name.length);

            udss_read_cfm(p_param->conidx, p_param->token, p_param->char_idx, p_param->status, NULL, p_param->token,
                          p_utf8);

            co_buf_release(p_utf8);
        }
    }
    else
    {
        udss_read_cfm(p_param->conidx, p_param->token, p_param->char_idx, p_param->status, NULL, 0, NULL);
    }

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief Handles reception of the @ref UDSS_RD_CHAR_CFM message. APP read response of one of the CCC read.
 * @param[in] msgid Id of the message received.
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int udss_rd_char_cfm_handler(ke_msg_id_t const msgid, struct udss_rd_char_cfm *p_param,
                                      ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    udss_read_cfm(p_param->conidx, p_param->token, p_param->char_idx, p_param->status, &(p_param->value), 0, NULL);

    return (KE_MSG_CONSUMED);
}

/// Default State handlers definition
KE_MSG_HANDLER_TAB(udss)
{
    // Note: all messages must be sorted in ID ascending order

    { UDSS_ENABLE_REQ,                (ke_msg_func_t) udss_enable_req_handler                },
    { UDSS_WR_CHAR_CFM,               (ke_msg_func_t) udss_wr_char_cfm_handler               },
    { UDSS_RD_CHAR_UTF8_CFM,          (ke_msg_func_t) udss_rd_char_utf8_cfm_handler          },
    { UDSS_RD_CHAR_CFM,               (ke_msg_func_t) udss_rd_char_cfm_handler               },
    { UDSS_DB_CHG_INC_UPD_CMD,        (ke_msg_func_t) udss_db_chg_inc_upd_cmd_handler        },
    { UDSS_USER_CTRL_PT_RSP_SEND_CMD, (ke_msg_func_t) udss_user_ctrl_pt_rsp_send_cmd_handler },
};


/**
 ****************************************************************************************
 * @brief Inform that bond data updated for the connection.
 *
 * @param[in] conidx        Connection index
 * @param[in] val_id        CCC value identifier (@see enum uds_val_id)
 *                              - UDS_CHAR_ID_DB_CHG_INC
 *                              - UDS_CHAR_ID_USER_CTRL_PT
 * @param[in] cfg_val       Stop/notify/indicate value to configure into the peer characteristic
 ****************************************************************************************
 */
__STATIC void udss_cb_bond_data_upd(uint8_t conidx, uint8_t val_id, uint16_t cfg_val)
{
    struct udss_wr_char_ccc_ind *p_evt;

    // Send the message
    p_evt = KE_MSG_ALLOC(UDSS_WR_CHAR_CCC_IND, PRF_DST_TASK(UDSS), PRF_SRC_TASK(UDSS), udss_wr_char_ccc_ind);

    if(p_evt != NULL)
    {
        p_evt->conidx     = conidx;
        p_evt->char_idx   = val_id;
        p_evt->ind_cfg    = cfg_val;
        ke_msg_send(p_evt);
    }
}

/**
 ****************************************************************************************
 * @brief Completion of Database change increment update procedure
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 ****************************************************************************************
 */
__STATIC void udss_cb_db_chg_inc_upd_cmp(uint8_t conidx, uint16_t status)
{
    udss_send_cmp_evt(conidx, UDSS_DB_CHG_INC_UPD_OP_CODE, status);
}

/**
 ****************************************************************************************
 * @brief Function called when peer device wants to read user data information
 *
 * Time information must be returned by application using @see udss_read_cfm
 *
 * @param[in] conidx        Connection index
 * @param[in] token         Token information that must be returned in confirmation
 * @param[in] val_id        Value identifier (@see enum uds_val_id)
 * @param[in] offset        Data offset (only valid for an UTF-8 string)
 * @param[in] max_len       Maximum string length to return (only valid for an UTF-8 string)
 ****************************************************************************************
 */
__STATIC void udss_cb_read_req(uint8_t conidx, uint32_t token, uint8_t val_id, uint16_t offset, uint16_t max_len)
{
    struct udss_rd_char_req_ind *p_req_ind;

    // Send the message
    p_req_ind = KE_MSG_ALLOC(UDSS_RD_CHAR_REQ_IND, PRF_DST_TASK(UDSS), PRF_SRC_TASK(UDSS), udss_rd_char_req_ind);

    if(p_req_ind != NULL)
    {
        p_req_ind->conidx     = conidx;
        p_req_ind->char_idx   = val_id;
        p_req_ind->token      = token;
        p_req_ind->offset     = offset;
        p_req_ind->max_len    = max_len;
        ke_msg_send(p_req_ind);
    }
}

/**
 ****************************************************************************************
 * @brief Function called when peer device wants to modify user data information
 *
 * Time information must be returned by application using @see udss_write_cfm
 *
 * @param[in] conidx        Connection index
 * @param[in] token         Token information that must be returned in confirmation
 * @param[in] val_id        Value identifier (@see enum uds_val_id)
 * @param[in] p_value       Pointer that contains non-UTF-8 value. (NULL for a UTF-8 value)
 * @param[in] p_utf8_name   Pointer to buffer that contains UTF-8 name. (NULL for an non-UTF-8 value)
 ****************************************************************************************
 */
__STATIC void udss_cb_write_req(uint8_t conidx, uint32_t token, uint8_t val_id, const union uds_value* p_value,
                                co_buf_t* p_utf8_name)
{
    if(p_value != NULL)
    {
        struct udss_wr_char_req_ind *p_req_ind;

        // Send the message
        p_req_ind = KE_MSG_ALLOC(UDSS_WR_CHAR_REQ_IND, PRF_DST_TASK(UDSS), PRF_SRC_TASK(UDSS), udss_wr_char_req_ind);

        if(p_req_ind != NULL)
        {
            p_req_ind->conidx     = conidx;
            p_req_ind->char_idx   = val_id;
            p_req_ind->token      = token;
            memcpy(&(p_req_ind->value), p_value, sizeof(union uds_value));
            ke_msg_send(p_req_ind);
        }
    }
    else if(p_utf8_name != NULL)
    {
        uint16_t length = co_buf_data_len(p_utf8_name);
        struct udss_wr_char_utf8_req_ind *p_req_ind;

        // Send the message
        p_req_ind = KE_MSG_ALLOC_DYN(UDSS_WR_CHAR_UTF8_REQ_IND, PRF_DST_TASK(UDSS),
                                     PRF_SRC_TASK(UDSS), udss_wr_char_utf8_req_ind, length);

        if(p_req_ind != NULL)
        {
            p_req_ind->conidx          = conidx;
            p_req_ind->char_idx        = val_id;
            p_req_ind->token           = token;
            p_req_ind->utf_name.length = length;
            co_buf_copy_data_to_mem(p_utf8_name, p_req_ind->utf_name.str, length);
            ke_msg_send(p_req_ind);
        }
    }
    else
    {
        ASSERT_ERR(0);
    }


}

/**
 ****************************************************************************************
 * @brief Inform that peer device requests an action using control point
 *
 * @note control point request must be answered using @see udss_ctnl_pt_rsp_send function
 *
 * @param[in] conidx        Connection index
 * @param[in] op_code       Operation Code (@see enum uds_ctrl_pt_response)
 * @param[in] user_id       User index
 * @param[in] consent       Used for sending consent command
 * @param[in] p_param       Pointer buffer value that contains response parameters
 ****************************************************************************************
 */
__STATIC void udss_cb_ctnl_pt_req(uint8_t conidx, uint8_t op_code, uint8_t user_id, uint16_t consent, co_buf_t* p_param)
{
    uint8_t length = ((p_param == NULL) ? 0 : co_buf_data_len(p_param));
    struct udss_user_ctrl_pt_req_recv_ind *p_ind;

    // Send the message
    p_ind = KE_MSG_ALLOC_DYN(UDSS_USER_CTRL_PT_REQ_RECV_IND, PRF_DST_TASK(UDSS),
                             PRF_SRC_TASK(UDSS), udss_user_ctrl_pt_req_recv_ind, length);

    if(p_ind != NULL)
    {
        p_ind->conidx     = conidx;
        p_ind->op_code    = op_code;
        p_ind->user_id    = user_id;
        p_ind->consent    = consent;
        p_ind->length     = length;
        co_buf_copy_data_to_mem(p_param, p_ind->parameter, length);
        ke_msg_send(p_ind);
    }
}

/**
 ****************************************************************************************
 * @brief Completion of control point response send procedure
 *
 * @param[in] status Status of the procedure execution (@see enum hl_err)
 ****************************************************************************************
 */
__STATIC void udss_cb_ctnl_pt_rsp_send_cmp(uint8_t conidx, uint16_t status)
{
    udss_send_cmp_evt(conidx, UDSS_CTRL_PT_RSP_SEND_OP_CODE, status);
}

/// Default Message handle
__STATIC const udss_cb_t udss_msg_cb =
{
        .cb_bond_data_upd        = udss_cb_bond_data_upd,
        .cb_db_chg_inc_upd_cmp   = udss_cb_db_chg_inc_upd_cmp,
        .cb_read_req             = udss_cb_read_req,
        .cb_write_req            = udss_cb_write_req,
        .cb_ctnl_pt_req          = udss_cb_ctnl_pt_req,
        .cb_ctnl_pt_rsp_send_cmp = udss_cb_ctnl_pt_rsp_send_cmp,
};
#endif // (BLE_HL_MSG_API)

/*
 * PROFILE DEFAULT HANDLERS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Initialization of the UDSS module.
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
__STATIC uint16_t udss_init(prf_data_t *p_env, uint16_t *p_start_hdl, uint8_t sec_lvl, uint8_t user_prio,
                          struct udss_db_cfg *p_params, const udss_cb_t* p_cb)
{
    //------------------ create the attribute database for the profile -------------------

    // DB Creation Status
    uint16_t status = GAP_ERR_NO_ERROR;
    uint8_t user_lid = GATT_INVALID_USER_LID;

    do
    {
        udss_env_t* p_udss_env;
        // Service attribute flags
        uint8_t cfg_flag[12];
        uint32_t mask = p_params->char_mask; // bitfield of enabled characteristics
        // Bitfield for the ATT database
        // mask of enabled attributes per Characteristic
        uint16_t flag;
        // keeps bit position of the next bit placing
        uint16_t bit_start;
        uint8_t byte_offs;
        uint8_t flag_shift;
        uint32_t bitmask;
        // temp index
        uint16_t i;

        #if (BLE_HL_MSG_API)
        if(p_cb == NULL)
        {
            p_cb = &(udss_msg_cb);
        }
        #endif // (BLE_HL_MSG_API)

        if(   (p_params == NULL) || (p_start_hdl == NULL) || (p_cb == NULL) || (p_cb->cb_bond_data_upd == NULL)
           || (p_cb->cb_db_chg_inc_upd_cmp == NULL)  || (p_cb->cb_read_req == NULL)  || (p_cb->cb_write_req == NULL)
           || (p_cb->cb_ctnl_pt_req == NULL) || (p_cb->cb_ctnl_pt_rsp_send_cmp == NULL))
        {
            status = GAP_ERR_INVALID_PARAM;
            break;
        }

        // register UDSS user
        status = gatt_user_srv_register(UDS_STRING_MAX_SIZE + GATT_WRITE_HEADER_LEN, user_prio, &udss_cb, &user_lid);
        if(status != GAP_ERR_NO_ERROR) break;


        memset(&cfg_flag[0], 0, sizeof(cfg_flag));
        // Mandatory Characteristics
        cfg_flag[0] = 0xff;
        cfg_flag[1] = 0x01;

        bit_start = 9;
//        // Optional
//        // first 4 characteristics have 3 attributes(handles)
//        flag = 0x07; // 3 attributes per characteristic
//
//        for (i = 0; i < (UDS_CHAR_ID_LANGUAGE - UDS_CHAR_ID_FIRST_NAME + 1); i++)
//        {
//            if (mask & 1)
//            {
//                //Find the byte to put the next enables attributes bitfield in 'flag'
//                byte_offs = bit_start/BITS_IN_BYTE;
//                // the start bit in this byte to start writing
//                flag_shift = bit_start - (byte_offs*BITS_IN_BYTE);
//                bitmask = flag << flag_shift; //prepare bitfield
//                cfg_flag[byte_offs] |= (uint8_t)(bitmask & 0xff);
//
//                if ((bitmask>>BITS_IN_BYTE) & 0xff)
//                {
//                    cfg_flag[byte_offs+1] |= (uint8_t)((bitmask >> BITS_IN_BYTE) & 0xff);
//                }
//            }
//
//            mask >>= 1;
//            //shift by the number of bits as the count of all attributes per characteristic
//            bit_start += (UDS_IDX_LAST_NAME_CHAR - UDS_IDX_FIRST_NAME_CHAR);
//        }

        // next 28 characteristics have 2 attributes(handles)
        flag = 0x03; // 2 attributes per characteristic

        for (i = 0; i < (UDS_CHAR_ID_THREE_ZONE_HEART_RATE_LIMITS - UDS_CHAR_ID_FIRST_NAME + 1); i++)
        {
            if (mask & 1)
            {
                //Find the byte to put the next enables attributes bitfield in 'flag'
                byte_offs = bit_start/BITS_IN_BYTE;
                // the start bit in this byte to start writing
                flag_shift = bit_start - (byte_offs*BITS_IN_BYTE);
                bitmask = flag << flag_shift; //prepare bitfield
                cfg_flag[byte_offs] |= (uint8_t)(bitmask & 0xff);

                if ((bitmask>>BITS_IN_BYTE) & 0xff)
                {
                    cfg_flag[byte_offs+1] |= (uint8_t)((bitmask>>BITS_IN_BYTE) & 0xff);
                }
            }

            mask >>= 1;
            //shift by the number of bits as the count of all attributes per characteristic
            bit_start += (UDS_IDX_HEIGHT_CHAR - UDS_IDX_WEIGHT_CHAR);
        }

        // Add GAP service
        status = gatt_db_svc16_add(user_lid, sec_lvl, GATT_SVC_USER_DATA, UDSS_IDX_NB,
                                   cfg_flag, &(udss_att_db[0]), UDSS_IDX_NB, p_start_hdl);
        if(status != GAP_ERR_NO_ERROR) break;

        //-------------------- allocate memory required for the profile  ---------------------
        p_udss_env = (udss_env_t *) ke_malloc(sizeof(udss_env_t), KE_MEM_ATT_DB);

        if(p_udss_env != NULL)
        {
            // allocate UDSS required environment variable
            p_env->p_env = (prf_hdr_t *) p_udss_env;
            p_udss_env->start_hdl       = *p_start_hdl;
            p_udss_env->user_lid        = user_lid;
            p_udss_env->char_mask       = p_params->char_mask;
            p_udss_env->op_ongoing      = false;
            p_udss_env->in_exe_op       = false;
            p_udss_env->ctrl_pt_op      = UDS_OP_CODE_RESERVED_00;
            memset(p_udss_env->ntf_ind_cfg, 0, sizeof(p_udss_env->ntf_ind_cfg));
            co_list_init(&(p_udss_env->wait_queue));

            // initialize profile environment variable
            p_udss_env->prf_env.p_cb     = p_cb;
            #if (BLE_HL_MSG_API)
            p_env->desc.msg_handler_tab  = udss_msg_handler_tab;
            p_env->desc.msg_cnt          = ARRAY_LEN(udss_msg_handler_tab);
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
__STATIC uint16_t udss_destroy(prf_data_t *p_env, uint8_t reason)
{
    uint16_t status = GAP_ERR_NO_ERROR;
    udss_env_t *p_udss_env = (udss_env_t *) p_env->p_env;

    if(reason != PRF_DESTROY_RESET)
    {
        status = gatt_user_unregister(p_udss_env->user_lid);
    }

    if(status == GAP_ERR_NO_ERROR)
    {
        if(reason != PRF_DESTROY_RESET)
        {
            // remove buffer in wait queue
            while(!co_list_is_empty(&p_udss_env->wait_queue))
            {
                co_buf_t* p_buf = (co_buf_t*) co_list_pop_front(&p_udss_env->wait_queue);
                co_buf_release(p_buf);
            }
        }

        // free profile environment variables
        p_env->p_env = NULL;
        ke_free(p_udss_env);
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
__STATIC void udss_con_create(prf_data_t *p_env, uint8_t conidx, const gap_con_param_t* p_con_param)
{
    udss_env_t *p_udss_env = (udss_env_t *) p_env->p_env;
    p_udss_env->ntf_ind_cfg[conidx] = 0;
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
__STATIC void udss_con_cleanup(prf_data_t *p_env, uint8_t conidx, uint16_t reason)
{
    udss_env_t *p_udss_env = (udss_env_t *) p_env->p_env;
    // clean-up environment variable allocated for task instance
    p_udss_env->ntf_ind_cfg[conidx] = 0;
}



/// UDSS Task interface required by profile manager
const prf_task_cbs_t udss_itf =
{
    .cb_init          = (prf_init_cb) udss_init,
    .cb_destroy       = udss_destroy,
    .cb_con_create    = udss_con_create,
    .cb_con_cleanup   = udss_con_cleanup,
    .cb_con_upd       = NULL,
};

/**
 ****************************************************************************************
 * @brief Retrieve service profile interface
 *
 * @return service profile interface
 ****************************************************************************************
 */
const prf_task_cbs_t *udss_prf_itf_get(void)
{
    return &udss_itf;
}


#endif //(BLE_UDS_SERVER)

/// @} UDSS

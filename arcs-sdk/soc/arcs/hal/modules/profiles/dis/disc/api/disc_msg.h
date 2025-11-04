/**
 ****************************************************************************************
 *
 * @file disc_msg.h
 *
 * @brief Header file - Device Information Service Client - Message API.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

#ifndef DISC_MSG_H_
#define DISC_MSG_H_

/**
 ****************************************************************************************
 * @addtogroup DISC Device Information Service Client
 * @brief Device Information Service Client - Message API
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "ble_prf.h"

/*
 * DEFINES
 ****************************************************************************************
 */

/*
 * ENUMERATIONS
 ****************************************************************************************
 */

/// Value identifier
enum disc_val_id
{
    /// Manufacturer Name String
    DISC_VAL_MANUFACTURER_NAME,
    /// Model Number String
    DISC_VAL_MODEL_NB_STR,
    /// Serial Number String
    DISC_VAL_SERIAL_NB_STR,
    /// Hardware Revision String
    DISC_VAL_HARD_REV_STR,
    /// Firmware Revision String
    DISC_VAL_FIRM_REV_STR,
    /// Software Revision String
    DISC_VAL_SW_REV_STR,
    /// System ID
    DISC_VAL_SYSTEM_ID,
    /// IEEE 11073-20601 Regulatory Certification Data List
    DISC_VAL_IEEE,
    /// PnP ID
    DISC_VAL_PNP_ID,

    DISC_VAL_MAX,
};

/// Operation codes
enum disc_op_codes
{
    /// Database Creation Procedure
    DISC_RESERVED_OP_CODE = 0,
    /// Read value Operation Code
    DISC_RD_VAL_CMD_OP_CODE = 1,
};

/*
 * API MESSAGES STRUCTURES
 ****************************************************************************************
 */

/**
 * Structure containing the characteristics handles, value handles and descriptors for
 * the Device Information Service
 */
typedef struct disc_dis_content
{
    /// service info
    prf_svc_t      svc;

    /// Value info:
    prf_char_t vals[DISC_VAL_MAX];
} disc_dis_content_t;

/// Parameters of the @ref DISC_ENABLE_REQ message
struct disc_enable_req
{
    /// Connection Index
    uint8_t            conidx;
    ///Connection type
    uint8_t            con_type;

    /// Existing handle values dis
    disc_dis_content_t dis;
};

/// Parameters of the @ref DISC_ENABLE_RSP message
struct disc_enable_rsp
{
    /// Connection Index
    uint8_t            conidx;
    ///status
    uint16_t           status;
    /// DIS handle values and characteristic properties
    disc_dis_content_t dis;
};

///Parameters of the @ref DISC_RD_VAL_CMD message
struct disc_rd_val_cmd
{
    /// Connection Index
    uint8_t  conidx;
    /// Value identifier (@see disc_val_id)
    uint8_t val_id;
};

///Parameters of the @ref DISC_RD_VAL_IND message
struct disc_rd_val_ind
{
    /// Connection Index
    uint8_t  conidx;
    /// Value identifier (@see disc_val_id)
    uint8_t  val_id;
    /// Attribute length
    uint16_t length;
    /// Attribute value
    uint8_t  value[__ARRAY_EMPTY];
};

///Parameters of the @ref DISC_CMP_EVT message
struct disc_cmp_evt
{
    /// Connection Index
    uint8_t  conidx;
    /// Operation
    uint8_t  operation;
    /// Status
    uint16_t status;
};

/// @} DISC

#endif // DISC_MSG_H_

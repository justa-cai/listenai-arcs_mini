/**
 ****************************************************************************************
 *
 * @file tipc_msg.h
 *
 * @brief Header file - Time Profile Client - Message API
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

#ifndef _TIPC_MSG_H_
#define _TIPC_MSG_H_

/**
 ****************************************************************************************
 * @addtogroup TIPC
 * @ingroup Profile
 * @brief Time Profile Client - Message API
 *
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "tip_common.h"
#include "btip_task.h" // Task definitions

/*
 * DEFINES
 ****************************************************************************************
 */

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */

/*@TRACE*/
enum tipc_msg_id
{
    /// Start the time profile - at connection
    TIPC_ENABLE_REQ              = MSG_ID(TIPC, 0x00),
    /// Confirm that cfg connection has finished with discovery results, or that normal cnx started
    TIPC_ENABLE_RSP              = MSG_ID(TIPC, 0x01),
    /// Generic message to read a CTS, NDCS or RTUS characteristic value
    TIPC_RD_CHAR_CMD             = MSG_ID(TIPC, 0x02),
    /// Received read value
    TIPC_RD_CHAR_IND             = MSG_ID(TIPC, 0x03),
    /// Generic message for configuring the Current Time Characteristic on the Server
    TIPC_CT_NTF_CFG_CMD          = MSG_ID(TIPC, 0x04),
    /// Generic message for writing the Time Update Control Point Characteristic Value on a peer device
    TIPC_WR_TIME_UPD_CTNL_PT_CMD = MSG_ID(TIPC, 0x05),
    /// Received Current Time value (Notification)
    TIPC_CT_IND                  = MSG_ID(TIPC, 0x06),
    /// Complete Event Information
    TIPC_CMP_EVT                 = MSG_ID(TIPC, 0x07),
};

/// Operation codes
enum tipc_op_codes
{
    /// Database Creation Procedure
    TIPC_RESERVED_OP_CODE                = 0,
    /// Read char. Operation Code
    TIPC_RD_CHAR_CMD_OP_CODE             = 1,
    /// Configure CCC of Current Time Characteristic Operation Code
    TIPC_CT_NTF_CFG_CMD_OP_CODE          = 2,
    /// Write the Time Update Control Point Characteristic Value Operation Code
    TIPC_WR_TIME_UPD_CTNL_PT_CMD_OP_CODE = 3,
};


/// Peer database discovered handle mapping
enum
{
    /// Current Time
    TIPC_CHAR_CTS_CURR_TIME,
    /// Local Time Info
    TIPC_CHAR_CTS_LOCAL_TIME_INFO,
    /// Reference Time Info
    TIPC_CHAR_CTS_REF_TIME_INFO,
    TIPC_CHAR_CTS_MAX,
};

/// Next DST Change Service Characteristics
enum
{
    /// Time With DST
    TIPC_CHAR_NDCS_TIME_WITH_DST,
    TIPC_CHAR_NDCS_MAX,
};

/// Reference Time Update Service Characteristics
enum
{
    /// Time Update Control Point
    TIPC_CHAR_RTUS_TIME_UPD_CTNL_PT,
    /// Time Update State
    TIPC_CHAR_RTUS_TIME_UPD_STATE,
    TIPC_CHAR_RTUS_MAX,
};

/// Current Time Service Characteristic Descriptors
enum
{
    /// Current Time client config
    TIPC_DESC_CTS_CURR_TIME_CLI_CFG,
    TIPC_DESC_CTS_MAX,
};

/*
 * STRUCTURES
 ****************************************************************************************
 */

/**
 * Structure containing the characteristics handles, value handles and descriptors for
 * the Current Time Service
 */
typedef struct tipc_cts_content
{
    /// service info
    prf_svc_t           svc;

    /// Characteristic info:
    ///  - Current Time
    ///  - Local Time Info
    ///  - Reference Time Info
    prf_char_t      chars[TIPC_CHAR_CTS_MAX];

    /// Descriptor handles:
    ///  - Current Time client cfg
    prf_desc_t descs[TIPC_DESC_CTS_MAX];
} tipc_cts_content_t;

/**
 * Structure containing the characteristics handles, value handles and descriptors for
 * the Next DST Change Service
 */
typedef struct tipc_ndcs_content
{
    /// service info
    prf_svc_t       svc;

    /// characteristic info:
    ///  - Time With DST
    prf_char_t  chars[TIPC_CHAR_NDCS_MAX];
} tipc_ndcs_content_t;

/**
 * Structure containing the characteristics handles, value handles and descriptors for
 * the Reference Time Update Service
 */
typedef struct tipc_rtus_content
{
    /// service info
    prf_svc_t       svc;

    /// characteristic info:
    ///  - Time Update Control Point
    ///  - Time Update State
    prf_char_t  chars[TIPC_CHAR_RTUS_MAX];
} tipc_rtus_content_t;


/// Parameters of the @ref TIPC_ENABLE_REQ message
struct tipc_enable_req
{
    /// Connection index
    uint8_t             conidx;
    /// Connection type
    uint8_t             con_type;

    /// Existing handle values cts
    tipc_cts_content_t  cts;
    /// Existing handle values ndcs
    tipc_ndcs_content_t ndcs;
    /// Existing handle values rtus
    tipc_rtus_content_t rtus;
};

/// Parameters of the @ref TIPC_ENABLE_RSP message
struct tipc_enable_rsp
{
    /// Connection index
    uint8_t             conidx;
    /// status
    uint16_t            status;
    /// Existing handle values cts
    tipc_cts_content_t  cts;
    /// Existing handle values ndcs
    tipc_ndcs_content_t ndcs;
    /// Existing handle values rtus
    tipc_rtus_content_t rtus;
};

/// Parameters of the @ref TIPC_CT_NTF_CFG_CMD message
struct tipc_ct_ntf_cfg_cmd
{
    /// Connection index
    uint8_t  conidx;
    /// Stop/notify/indicate value to configure into the peer characteristic
    uint16_t cfg_val;
};

/// Parameters of the @ref TIPC_RD_CHAR_CMD message
struct tipc_rd_char_cmd
{
    /// Connection index
    uint8_t conidx;
    /// Value identifier (@see enum tip_value_id)
    uint8_t val_id;
};

/// Parameters of the @ref TIPC_RD_CHAR_IND message
struct tipc_rd_char_ind
{
    /// Connection index
    uint8_t         conidx;
    /// Value identifier (@see enum tip_value_id)
    uint8_t         val_id;
    /// Read value data
    union tip_value value;
};

/// Parameters of the @ref TIPC_WR_TIME_UPD_CTNL_PT_CMD message
struct tipc_wr_time_upd_ctnl_pt_cmd
{
    /// Connection index
    uint8_t                 conidx;
    /// Value
    tip_time_upd_contr_pt_t value;
};

/// Parameters of the @ref TIPC_CT_IND message
struct tipc_ct_ind
{
    /// Connection index
    uint8_t         conidx;
    /// Current Time Value
    tip_curr_time_t ct_val;
};

///Parameters of the @ref TIPC_CMP_EVT message
struct tipc_cmp_evt
{
    /// Connection index
    uint8_t  conidx;
    /// Operation
    uint8_t  operation;
    /// Status
    uint16_t status;
};

/// @} TIPC

#endif /* _TIPC_MSG_H_ */

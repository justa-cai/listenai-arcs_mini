/**
 ****************************************************************************************
 *
 * @file spp.h
 *
 * @brief Header file - Serial Port Profile - Message API.
 *
 * Copyright (C) Listenai.com 2021
 *
 ****************************************************************************************
 */

#ifndef _SPP_MSG_H_
#define _SPP_MSG_H_

/**
 ****************************************************************************************
 * @addtogroup SPP
 * @ingroup Profile
 * @brief Serial Port Profile - Message API.
 *
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "btip_task.h" // Task definitions
#include "ble_prf.h"

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */

#define SPP_NAME_MAX_LEN            (16)

/*@TRACE*/
enum spp_msg_id
{
    /// Start the spp Profile - connect
    SPP_CONN_CMD           = PRF_MSG_ID(SPP, 0x00),
    /// Stop the spp Profile - disconnect
    SPP_DISC_CMD           = PRF_MSG_ID(SPP, 0x01),

    /// spp connect indicate
    SPP_CONN_IND           = PRF_MSG_ID(SPP, 0x02),
    /// spp disconnect indicate
    SPP_DISC_IND           = PRF_MSG_ID(SPP, 0x03),

    /// send data
    SPP_SEND_DATA          = PRF_MSG_ID(SPP, 0x04),
    /// receive data
    SPP_DATA_IND           = PRF_MSG_ID(SPP, 0x05),

    /// send rfcomm command
    SPP_RFC_CMD            = PRF_MSG_ID(SPP, 0x06),
    /// rfcomm indicate
    SPP_RFC_IND            = PRF_MSG_ID(SPP, 0x07),

    /// Complete Event Information
    SPP_SDP_SET            = PRF_MSG_ID(SPP, 0x08),

    /// Complete Event Information
    SPP_CMP_EVT            = PRF_MSG_ID(SPP, 0x09),
};

struct spp_prf_cfg {
    /// max connections
    uint8_t  max_con;
    /// serial port number base
    uint8_t  base_port;
    /// server enable
    uint8_t  svr_en;
    /// rfcomm channel, must greater than RFC_DYN_CHAN_BASE
    uint8_t  svr_chan;
    /// max packet size
    uint16_t packet_size;
};

struct spp_sdp_set {
    /// uuid 128
    uint8_t  uuid[16];
    /// service name
    uint8_t  name[SPP_NAME_MAX_LEN];
};

struct spp_conn_cmd {
    /// connect idx
    uint8_t  conidx;
    /// serial port number
    uint8_t  port;
    /// max packet size
    uint16_t packet_size;
    /// uuid 128
    uint8_t  uuid[16];
};

struct spp_disc_cmd {
    /// connect idx
    uint8_t  conidx;
    /// serial port number
    uint8_t  port;
};

struct spp_conn_ind {
    /// connect idx
    uint8_t  conidx;
    /// serial port number
    uint8_t  port;
};

struct spp_disc_ind {
    /// connect idx
    uint8_t  conidx;
    /// serial port number
    uint8_t  port;
};

/// send data to spp
/*@TRACE*/
struct spp_data_msg
{
    /// connect idx
    uint8_t     conidx;
    /// serial port number
    uint8_t  port;
    /// SDU Length
    uint16_t length;
    /// SDU Data
    uint8_t  data[__ARRAY_EMPTY];
};

struct spp_cmp_evt {
    /// connect idx
    uint8_t  conidx;
    /// serial port number
    uint8_t  port;
    /// command
    uint16_t  cmd;
    /// result
    uint16_t status;
};


/// @} SPP

#endif /* _SPP_MSG_H_ */

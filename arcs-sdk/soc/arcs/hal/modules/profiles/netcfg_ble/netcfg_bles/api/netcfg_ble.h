/**
 ****************************************************************************************
 *
 * @file netcfg_ble.h
 *
 * @brief NETCFG_BLE data type definition and function declaration.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

#ifndef INCLUDE_NETCFG_BLE_H_
#define INCLUDE_NETCFG_BLE_H_

#include "log_print.h"

#define NETCFG_BLE_PREFIX_ID    0x03e4
#define NETCFG_BLE_DBG          1

#define NETCFG_BLE_SSID_MAX_LEN 36
#define NETCFG_BLE_PWD_MAX_LEN  64

#ifndef __PACKED
#define __PACKED __attribute__ ((__packed__))
#endif

#define NETCFG_BLE_PAYLOAD_LEN_MAX	20
#define NETCFG_BLE_TAG "NETCFG_BLE:"
#define NETCFG_BLE_LOG_FLUSH() log_flush()
#define NETCFG_BLE_LOG(fmt, ...) CLOG(fmt, ##__VA_ARGS__)
#define NETCFG_BLE_PRINT(fmt, ...) logDbg(fmt, ##__VA_ARGS__)
#if NETCFG_BLE_DBG
#define NETCFG_BLE_LOGD(fmt, ...) CLOG(NETCFG_BLE_TAG fmt, ##__VA_ARGS__)
#else
#define NETCFG_BLE_LOGD(fmt, ...)
#endif

/// netcfg_ble command
enum netcfg_ble_op {
    /// version of new image
    NETCFG_BLE_OP_START = 0xA001,
    NETCFG_BLE_OP_SSID,
    NETCFG_BLE_OP_PWD,
    NETCFG_BLE_OP_DONE = NETCFG_BLE_OP_START + 0xf,

    NETCFG_BLE_OP_REBOOT,

    NETCFG_BLE_AUTH_INFO = 0xA012, // auth info
    NETCFG_BLE_OP_MAX,
};

/// netcfg_ble status
enum netcfg_ble_status {
    NETCFG_BLE_READY = 0x0100,
    NETCFG_BLE_START,
    NETCFG_BLE_INPROCESS,
    NETCFG_BLE_CERT_READY,
    NETCFG_BLE_SUCCESS,
    NETCFG_BLE_REBOOTING,
    NETCFG_BLE_IDLE,

    NETCFG_BLE_SSID,
    NETCFG_BLE_PWD,

    NETCFG_BLE_CERT_ERR,
    NETCFG_BLE_ERR,

    NETCFG_BLE_STAT_MAX,
};

typedef struct __PACKED
{
    uint16_t prefix_id;
    uint16_t opcode;
    uint16_t len;
} ls_netcfg_ble_cmd_head;

/// netcfg_ble command header
typedef struct __PACKED {
    /// netcfg_ble command opcode
    uint8_t raw_index;
    uint8_t raw_count;
    uint8_t raw_length;
    uint8_t data[0];
} ls_netcfg_ble_cmd_t, ls_netcfg_ble_data_frame_t;

/// netcfg_ble resp header
typedef struct {
    /// netcfg_ble command opcode
    uint16_t prefix_id;
    uint16_t opcode;
    uint16_t stat;
} ls_netcfg_ble_resp_t;

struct netcfg_ble_data {
    uint8_t ssid[NETCFG_BLE_SSID_MAX_LEN];
    uint8_t pwd[NETCFG_BLE_PWD_MAX_LEN];
};
#endif /* INCLUDE_NETCFG_BLE_H_ */

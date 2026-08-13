/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file lisa_ble_api_ipc.c
 * @brief IPC implementation of unified BLE API (CP core in dual-core mode).
 *        Sends BLE commands to AP core via MRPC btos_send_app_evt_api().
 */

#include "lisa_ble_api.h"

#include <string.h>
#include <stddef.h>
#include <stdint.h>

#include "bt_ipc_api.h"
#include "bt_os_task.h"

#if CONFIG_LISA_BLUETOOTH_BUILD_NETCFG_BLE_SERVER || CONFIG_BLE_PROFILE_NETCFG_BLES
#include "netcfg_bles.h"
#endif

/*
 * Internal data structures (match AP-side bt_app_if.h / bt_os_task event params)
 */
typedef struct {
    uint8_t  adv_id;
    uint8_t  adv_type;
} ble_adv_info_t;

typedef struct {
    uint8_t  conidx;
    uint8_t  report_idx;
    uint16_t len;
    uint8_t  value[];
} ble_hogpd_info_t;

typedef struct {
    uint8_t  conidx;
    uint8_t  op;
    uint16_t status;
    uint16_t len;
    uint8_t  value[];
} ble_net_cfg_info_t;

typedef struct {
    uint8_t  scan_id;
    uint8_t  scan_param_dft;
    uint8_t  scan_type;
    uint8_t  scan_phy;
    uint8_t  scan_intv;
    uint8_t  scan_win;
} ble_scan_info_t;

typedef struct {
    uint8_t  addr[6];
    uint8_t  addr_type;
    uint8_t  phy;
    uint16_t conn_intv_min;
    uint16_t conn_intv_max;
    uint16_t conn_latency;
    uint16_t conn_super_to;
} ble_conn_info_t;

typedef struct {
    uint8_t conidx;
    uint8_t reason;
} ble_disconn_info_t;

typedef struct {
    uint8_t  conhdl;
    uint16_t intv_min;
    uint16_t intv_max;
    uint16_t latency;
    uint16_t time_out;
} ble_conn_update_info_t;

/*
 * All functions use btos_send_app_evt_api() which goes through MRPC to AP core.
 * The AP core's bt_ipc_api.c receives the msg_id + param, reconstructs a
 * btos_event_t, and forwards it to the local BT OS task.
 */

uint8_t lisa_ble_adv_start(uint8_t adv_id, uint8_t adv_type)
{
    ble_adv_info_t info = {
        .adv_id = adv_id,
        .adv_type = adv_type,
    };
    return (uint8_t)btos_send_app_evt_api(OS_TASK_ID_BT, BT_OS_ADV_START_EVT,
                                          &info, sizeof(info),
                                          (uint32_t)BTOS_TASK_MAX_DELAY);
}

uint8_t lisa_ble_adv_stop(uint8_t adv_id)
{
    ble_adv_info_t info = {
        .adv_id = adv_id,
        .adv_type = 0,
    };
    return (uint8_t)btos_send_app_evt_api(OS_TASK_ID_BT, BT_OS_ADV_STOP_EVT,
                                          &info, sizeof(info),
                                          (uint32_t)BTOS_TASK_MAX_DELAY);
}

uint8_t lisa_ble_scan_start(uint8_t scan_id)
{
    ble_scan_info_t info = {
        .scan_id = scan_id,
        .scan_param_dft = 1,
    };
    return (uint8_t)btos_send_app_evt_api(OS_TASK_ID_BT, BT_OS_SCAN_START_EVT,
                                          &info, sizeof(info),
                                          (uint32_t)BTOS_TASK_MAX_DELAY);
}

uint8_t lisa_ble_scan_stop(uint8_t scan_id)
{
    ble_scan_info_t info = {
        .scan_id = scan_id,
    };
    return (uint8_t)btos_send_app_evt_api(OS_TASK_ID_BT, BT_OS_SCAN_STOP_EVT,
                                          &info, sizeof(info),
                                          (uint32_t)BTOS_TASK_MAX_DELAY);
}

uint8_t lisa_ble_scan_param(uint8_t type, uint8_t phy, uint16_t scan_intv, uint16_t scan_win)
{
    ble_scan_info_t info = {
        .scan_param_dft = 0,
        .scan_type = type,
        .scan_phy = phy,
        .scan_intv = scan_intv,
        .scan_win = scan_win,
    };
    return (uint8_t)btos_send_app_evt_api(OS_TASK_ID_BT, BT_OS_SCAN_START_EVT,
                                          &info, sizeof(info),
                                          (uint32_t)BTOS_TASK_MAX_DELAY);
}

uint8_t lisa_ble_connect(lisa_ble_addr_t *addr, uint8_t phy,
                        uint16_t conn_intv_min, uint16_t conn_intv_max,
                        uint16_t conn_latency, uint16_t conn_super_to)
{
    ble_conn_info_t info;
    memcpy(info.addr, addr->addr, 6);
    info.addr_type = addr->addr_type;
    info.phy = phy;
    info.conn_intv_min = conn_intv_min;
    info.conn_intv_max = conn_intv_max;
    info.conn_latency = conn_latency;
    info.conn_super_to = conn_super_to;

    return (uint8_t)btos_send_app_evt_api(OS_TASK_ID_BT, BT_OS_CONNECT_EVT,
                                          &info, sizeof(info),
                                          (uint32_t)BTOS_TASK_MAX_DELAY);
}

uint8_t lisa_ble_disconnect(uint8_t conidx, uint8_t reason)
{
    ble_disconn_info_t info = {
        .conidx = conidx,
        .reason = reason,
    };
    return (uint8_t)btos_send_app_evt_api(OS_TASK_ID_BT, BT_OS_DISCONNECT_EVT,
                                          &info, sizeof(info),
                                          (uint32_t)BTOS_TASK_MAX_DELAY);
}

uint8_t lisa_ble_conn_update(uint8_t conidx,
                            uint16_t conn_intv_min, uint16_t conn_intv_max,
                            uint16_t latency, uint16_t super_to)
{
    ble_conn_update_info_t info = {
        .conhdl = conidx,
        .intv_min = conn_intv_min,
        .intv_max = conn_intv_max,
        .latency = latency,
        .time_out = super_to,
    };
    return (uint8_t)btos_send_app_evt_api(OS_TASK_ID_BT, BT_OS_CONNECT_UPDATE_EVT,
                                          &info, sizeof(info),
                                          (uint32_t)BTOS_TASK_MAX_DELAY);
}

uint8_t lisa_ble_hid_send(uint8_t conidx, uint8_t report_idx, uint8_t length, uint8_t *value)
{
    /* For variable-length data, we need a stack buffer large enough */
    uint8_t buf[sizeof(ble_hogpd_info_t) + 256];
    ble_hogpd_info_t *hogpd = (ble_hogpd_info_t *)buf;

    hogpd->conidx = conidx;
    hogpd->report_idx = report_idx;
    hogpd->len = length;
    memcpy(hogpd->value, value, length);

    return (uint8_t)btos_send_app_evt_api(OS_TASK_ID_BT, BT_OS_HID_SEND_EVT,
                                          buf, sizeof(ble_hogpd_info_t) + length,
                                          (uint32_t)BTOS_TASK_MAX_DELAY);
}

uint8_t lisa_ble_voice_data_send(uint8_t conidx, uint8_t report_idx, uint8_t length, uint8_t *value)
{
    uint8_t buf[sizeof(ble_hogpd_info_t) + 256];
    ble_hogpd_info_t *hogpd = (ble_hogpd_info_t *)buf;

    hogpd->conidx = conidx;
    hogpd->report_idx = report_idx;
    hogpd->len = length;
    memcpy(hogpd->value, value, length);

    return (uint8_t)btos_send_app_evt_api(OS_TASK_ID_BT, BT_OS_VOICE_DATA_SEND_EVT,
                                          buf, sizeof(ble_hogpd_info_t) + length,
                                          (uint32_t)BTOS_TASK_MAX_DELAY);
}

uint8_t lisa_ble_netcfg_send_notify(uint8_t conidx, uint8_t op, uint8_t state, uint8_t length, uint8_t *value)
{
    uint8_t buf[sizeof(ble_net_cfg_info_t) + 256];
    ble_net_cfg_info_t *netcfg = (ble_net_cfg_info_t *)buf;

    netcfg->conidx = conidx;
    netcfg->op = op;
    netcfg->status = state;
    netcfg->len = length;
    if (length > 0 && value != NULL) {
        memcpy(netcfg->value, value, length);
    }

    return (uint8_t)btos_send_app_evt_api(OS_TASK_ID_BT, BT_OS_NET_CFG_SEND_EVT,
                                          buf, sizeof(ble_net_cfg_info_t) + length,
                                          (uint32_t)BTOS_TASK_MAX_DELAY);
}

uint8_t lisa_ble_netcfg_send_custom_data(uint8_t conidx, uint16_t length, uint8_t *value)
{
    if (length == 0 || value == NULL) {
        return 0xff;
    }

    if (length > LISA_BLE_NETCFG_CUSTOM_DATA_MAX_LEN) {
        return 0xff;
    }

#if CONFIG_LISA_BLUETOOTH_BUILD_NETCFG_BLE_SERVER || CONFIG_BLE_PROFILE_NETCFG_BLES
    return ble_netcfg_bles_send_notify_custom_data(conidx, length, value) == 0 ? 0 : 0xff;
#else
    (void)conidx;
    return 0xff;
#endif
}

/*
 * BLE netcfg handler on CP side (dual-core mode).
 * AP forwards WiFi credentials via MRPC; the MRPC server handler calls
 * lisa_ble_netcfg_ipc_handler() which invokes the user-registered callback.
 */
static lisa_ble_netcfg_handler_t s_netcfg_handler;

void lisa_ble_netcfg_set_handler(lisa_ble_netcfg_handler_t handler)
{
    s_netcfg_handler = handler;
}

void lisa_ble_netcfg_set_custom_op_handler(lisa_ble_netcfg_custom_op_handler_t handler)
{
    (void)handler;
}

int lisa_ble_netcfg_ipc_handler(const char *ssid, const char *pwd)
{
    if (!s_netcfg_handler)
        return -1;

    return s_netcfg_handler(ssid, pwd);
}

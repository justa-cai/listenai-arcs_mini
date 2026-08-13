/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file lisa_ble_api_local.c
 * @brief Local implementation of unified BLE API (AP core / single-core).
 *        Directly sends events to the BT OS task via btos_send_event().
 */

#include "lisa_ble_api.h"

#include <string.h>
#include <stddef.h>

#include "btos_def.h"
#include "btos_al.h"
#include "bt_os_task.h"
#include "ble_gap.h"

#if CONFIG_LISA_BLUETOOTH_BUILD_NETCFG_BLE_SERVER || CONFIG_BLE_PROFILE_NETCFG_BLES
#include "netcfg_bles.h"
#endif

/*
 * Internal data structures (match Makefile project bt_app_if.h)
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
    gap_bdaddr_t addr;
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
 * Helper: allocate btos_event, set msg_id, copy param, send
 */
static uint8_t bt_send_event(uint16_t msg_id, void *param, uint16_t param_len)
{
    btos_event_t ev;
    uint16_t ev_len = sizeof(btos_msg_t) + param_len;

    ev.msg_body = btos_malloc(ev_len);
    if (ev.msg_body == NULL) {
        return 0xff;
    }
    ev.msg_body->msg_id = msg_id;
    ev.msg_body->param_len = param_len;
    if (param_len > 0 && param != NULL) {
        memcpy(ev.msg_body->param, param, param_len);
    }

    return btos_send_event(OS_TASK_ID_BT, &ev, (uint32_t)BTOS_TASK_MAX_DELAY);
}

uint8_t lisa_ble_adv_start(uint8_t adv_id, uint8_t adv_type)
{
    ble_adv_info_t info = {
        .adv_id = adv_id,
        .adv_type = adv_type,
    };
    return bt_send_event(BT_OS_ADV_START_EVT, &info, sizeof(info));
}

uint8_t lisa_ble_adv_stop(uint8_t adv_id)
{
    ble_adv_info_t info = {
        .adv_id = adv_id,
        .adv_type = 0,
    };
    return bt_send_event(BT_OS_ADV_STOP_EVT, &info, sizeof(info));
}

uint8_t lisa_ble_scan_start(uint8_t scan_id)
{
    ble_scan_info_t info = {
        .scan_id = scan_id,
        .scan_param_dft = 1,
    };
    return bt_send_event(BT_OS_SCAN_START_EVT, &info, sizeof(info));
}

uint8_t lisa_ble_scan_stop(uint8_t scan_id)
{
    ble_scan_info_t info = {
        .scan_id = scan_id,
    };
    return bt_send_event(BT_OS_SCAN_STOP_EVT, &info, sizeof(info));
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
    return bt_send_event(BT_OS_SCAN_START_EVT, &info, sizeof(info));
}

uint8_t lisa_ble_connect(lisa_ble_addr_t *addr, uint8_t phy,
                        uint16_t conn_intv_min, uint16_t conn_intv_max,
                        uint16_t conn_latency, uint16_t conn_super_to)
{
    ble_conn_info_t info;
    memcpy(info.addr.addr, addr->addr, 6);
    info.addr.addr_type = addr->addr_type;
    info.phy = phy;
    info.conn_intv_min = conn_intv_min;
    info.conn_intv_max = conn_intv_max;
    info.conn_latency = conn_latency;
    info.conn_super_to = conn_super_to;

    return bt_send_event(BT_OS_CONNECT_EVT, &info, sizeof(info));
}

uint8_t lisa_ble_disconnect(uint8_t conidx, uint8_t reason)
{
    ble_disconn_info_t info = {
        .conidx = conidx,
        .reason = reason,
    };
    return bt_send_event(BT_OS_DISCONNECT_EVT, &info, sizeof(info));
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
    return bt_send_event(BT_OS_CONNECT_UPDATE_EVT, &info, sizeof(info));
}

uint8_t lisa_ble_hid_send(uint8_t conidx, uint8_t report_idx, uint8_t length, uint8_t *value)
{
    uint16_t param_len = sizeof(ble_hogpd_info_t) + length;
    uint16_t ev_len = sizeof(btos_msg_t) + param_len;
    btos_event_t ev;

    ev.msg_body = btos_malloc(ev_len);
    if (ev.msg_body == NULL) {
        return 0xff;
    }
    ev.msg_body->msg_id = BT_OS_HID_SEND_EVT;
    ev.msg_body->param_len = param_len;

    ble_hogpd_info_t *hogpd = (ble_hogpd_info_t *)ev.msg_body->param;
    hogpd->conidx = conidx;
    hogpd->report_idx = report_idx;
    hogpd->len = length;
    memcpy(hogpd->value, value, length);

    return btos_send_event(OS_TASK_ID_BT, &ev, (uint32_t)BTOS_TASK_MAX_DELAY);
}

uint8_t lisa_ble_voice_data_send(uint8_t conidx, uint8_t report_idx, uint8_t length, uint8_t *value)
{
    uint16_t param_len = sizeof(ble_hogpd_info_t) + length;
    uint16_t ev_len = sizeof(btos_msg_t) + param_len;
    btos_event_t ev;

    ev.msg_body = btos_malloc(ev_len);
    if (ev.msg_body == NULL) {
        return 0xff;
    }
    ev.msg_body->msg_id = BT_OS_VOICE_DATA_SEND_EVT;
    ev.msg_body->param_len = param_len;

    ble_hogpd_info_t *hogpd = (ble_hogpd_info_t *)ev.msg_body->param;
    hogpd->conidx = conidx;
    hogpd->report_idx = report_idx;
    hogpd->len = length;
    memcpy(hogpd->value, value, length);

    return btos_send_event(OS_TASK_ID_BT, &ev, (uint32_t)BTOS_TASK_MAX_DELAY);
}

uint8_t lisa_ble_netcfg_send_notify(uint8_t conidx, uint8_t op, uint8_t state, uint8_t length, uint8_t *value)
{
    uint16_t param_len = sizeof(ble_net_cfg_info_t) + length;
    uint16_t ev_len = sizeof(btos_msg_t) + param_len;
    btos_event_t ev;

    ev.msg_body = btos_malloc(ev_len);
    if (ev.msg_body == NULL) {
        return 0xff;
    }
    ev.msg_body->msg_id = BT_OS_NET_CFG_SEND_EVT;
    ev.msg_body->param_len = param_len;

    ble_net_cfg_info_t *netcfg = (ble_net_cfg_info_t *)ev.msg_body->param;
    netcfg->conidx = conidx;
    netcfg->op = op;
    netcfg->status = state;
    netcfg->len = length;
    if (length > 0 && value != NULL) {
        memcpy(netcfg->value, value, length);
    }

    return btos_send_event(OS_TASK_ID_BT, &ev, (uint32_t)BTOS_TASK_MAX_DELAY);
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

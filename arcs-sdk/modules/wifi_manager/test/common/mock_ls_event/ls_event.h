/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int ls_err_t;

typedef enum {
    EVENT_WIFI = 0,
    EVENT_BT,
    EVENT_APP_NETCFG,
    EVENT_MAX,
} event_module_t;

typedef ls_err_t (*event_cb_t)(void *arg, event_module_t event_module, int event_id, void *event_data);

typedef struct {
    int erro_code;
    int status_code;
    int reason_code;
} event_connect_fail_param_t;

#define EVENT_WIFI_CONNECTED        1
#define EVENT_WIFI_DISCONNECT       2
#define EVENT_WIFI_STA_CONNECT_FAIL 3
#define EVENT_WIFI_SCAN_DONE        4

#define LS_NEVER_TIMEOUT            0xFFFFFFFFU

ls_err_t ls_event_register_cb(event_module_t event_module_id, int event_id,
                              event_cb_t event_cb, void *event_cb_arg);
ls_err_t ls_event_unregister_cb(event_module_t event_module_id, int event_id,
                                event_cb_t event_cb);
ls_err_t ls_event_wait(event_module_t event_module_id, int event_id, uint32_t timeout);

#ifdef __cplusplus
}
#endif

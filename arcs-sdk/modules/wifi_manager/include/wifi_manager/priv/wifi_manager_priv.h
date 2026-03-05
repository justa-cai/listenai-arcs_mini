/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "wifi_manager/wifi_manager.h"
#include "wifi_manager/wifi_dev.h"
#include "wifi_manager/dlist.h"
#include "wifi_manager/wifi_manager_storage.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    wifi_mgr_sta_config_t config;
    wifi_mgr_connection_status_t sta_status;
} wifi_mgr_device_t;

typedef struct {
    wifi_mgr_autoconn_config_t config;
    bool enable;
} wifi_mgr_auto_conn_obj_t;

typedef enum {
    WIFI_MGR_AUTOCONN_MANUAL_IDLE = 0,
    WIFI_MGR_AUTOCONN_MANUAL_PAUSED_SYNC,
    WIFI_MGR_AUTOCONN_MANUAL_RESUME_ON_EVENT,
} wifi_mgr_manual_autoconn_state_t;

typedef struct {
    wifi_mgr_device_t sta_device;
    wifi_dev_event_cb_t wifi_event_cb;
    sys_dlist_t wifi_callback_list;
    wifi_mgr_auto_conn_obj_t *auto_connect_obj;
    wifi_mgr_manual_autoconn_state_t manual_autoconn_state;
    void* mutex;
    void* queue;
    bool thread_exit;
    void *thread;
    wifi_manager_mem_ops_t mem_ops;
    wifi_manager_os_ops_t os_ops;
    wifi_storage_ctx_t storage_ctx;
} wifi_mgr_obj_t;

#ifdef __cplusplus
}
#endif

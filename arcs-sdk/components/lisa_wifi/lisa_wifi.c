/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define TAG "lisa_wifi"

#include "lisa_log.h"
#include "rf_cali.h"
#include "ls_crypto.h"
#include "ls_event.h"
#include "ls_wifi_type.h"
#include "net_def.h"
#include "ls_misc.h"
#include "lisa_wifi.h"
#include "sys_init.h"
#include "lisa_queue.h"
#include "lisa_thread.h"
#include "lisa_typedef.h"

#include <string.h>
#include <stdbool.h>

// 保存用户设置的 ops
static lisa_wifi_ops_t g_user_ops = {0};

#define LISA_WIFI_CB_TASK_STACK_SIZE CONFIG_LISA_WIFI_CB_TASK_STACK_SIZE
#define LISA_WIFI_CB_TASK_PRIORITY   CONFIG_LISA_WIFI_CB_TASK_PRIORITY
#define LISA_WIFI_CB_QUEUE_LEN       CONFIG_LISA_WIFI_CB_QUEUE_LEN

typedef enum {
    LISA_WIFI_CB_EVENT_INIT_DONE = 0,
} lisa_wifi_cb_event_t;

typedef struct {
    lisa_wifi_cb_event_t event;
} lisa_wifi_cb_msg_t;

static lisa_queue_t *s_wifi_cb_queue;
static lisa_thread_t *s_wifi_cb_thread;
static bool s_wifi_cb_inited;

static void lisa_wifi_cb_task(void *arg)
{
    (void)arg;

    for (;;) {
        lisa_wifi_cb_msg_t msg = {0};
        if (lisa_queue_pop(s_wifi_cb_queue, &msg, sizeof(msg), LISA_OS_WAIT_FOREVER) != LISA_OK) {
            lisa_thread_mdelay(10);
            continue;
        }

        switch (msg.event) {
            case LISA_WIFI_CB_EVENT_INIT_DONE:
                if (g_user_ops.init_done) {
                    g_user_ops.init_done();
                }
                break;
            default:
                break;
        }
    }
}

static int lisa_wifi_cb_task_init(void)
{
    lisa_thread_attr_t thread_attr = {
        .name = (uint8_t *)"lisa_wifi",
        .stack_size = LISA_WIFI_CB_TASK_STACK_SIZE,
        .priority = LISA_WIFI_CB_TASK_PRIORITY,
    };

    if (s_wifi_cb_inited) {
        return 0;
    }

    s_wifi_cb_queue = lisa_queue_create(LISA_WIFI_CB_QUEUE_LEN,
                                        (uint8_t *)"lisa_wifi_cbq",
                                        sizeof(lisa_wifi_cb_msg_t));
    if (s_wifi_cb_queue == NULL) {
        LOGE("Failed to create WiFi callback queue");
        return -1;
    }

    s_wifi_cb_thread = lisa_thread_create(&thread_attr, lisa_wifi_cb_task, NULL);
    if (s_wifi_cb_thread == NULL) {
        LOGE("Failed to create WiFi callback task");
        lisa_queue_delete(s_wifi_cb_queue);
        s_wifi_cb_queue = NULL;
        return -1;
    }

    s_wifi_cb_inited = true;
    return 0;
}

static void lisa_wifi_cb_post(lisa_wifi_cb_event_t event)
{
    lisa_wifi_cb_msg_t msg = {.event = event};
    if (s_wifi_cb_queue == NULL) {
        return;
    }

    if (lisa_queue_push(s_wifi_cb_queue, &msg, sizeof(msg), 0) != LISA_OK) {
        LOGE("WiFi callback queue full, drop event %d", (int)event);
    }
}

#if CONFIG_WIFI_LWIP_DIFF_CORE
#include "ic_lock.h"
#if CONFIG_WIFI
#include "ipc_slave.h"
#include "net_al.h"
#include "wifi_api.h"
#elif CONFIG_LWIP
#include "ipc_master.h"
#include "wlif.h"
#endif
#endif

static void wifi_error_status_info(uint16_t erro, uint16_t status_code, uint16_t reason_code)
{
    switch(erro) {
        case WIFI_ERROR_STA_AUTH_FAIL:
            CLOGI("wifi auth fail \n");
            break;
        case WIFI_ERROR_STA_ASSOC_FAIL:
            CLOGI("wifi associate fail , statu code %d\n", status_code);
            break;
        case WIFI_ERROR_STA_CONNECT_NO_TARGET_AP:
            CLOGI("wifi connect fail, did not find targe AP \n");
            break;
        case WIFI_ERROR_STA_LINK_LOSS:
            CLOGI("wifi beacon lost \n");
            break;
        case WIFI_ERROR_WPA3_PWD_OR_AUTH_FAIL:
        case WIFI_ERROR_FOUND_SSID_BUT_KEY_MISMATCH:
             CLOGI("wifi password may wrong \n");
             break;
        case WIFI_ERROR_NO_FRAME_ALLC_FOR_AUTH_ASSO:
        case WIFI_ERROR_ADD_STA_FAIL:
            CLOGI("wifi connect fail caused by allocate fail \n");
            break;
        case WIFI_ERROR_AUTH_ASSOC_TIMEOUT:
            CLOGI("wifi auth/associate time out \n");
            break;
        case WIFI_ERROR_DEAUTH_BY_AP:
            CLOGI("Receive deauth from AP, reason code %d \n",reason_code);
            if (reason_code == 15)
                CLOGI("May password wrong \n");
            break;
        case WIFI_ERROR_DEAUTH_BY_LOCAL:
            CLOGI("wifi disconnect by local, reason code %d \n", reason_code);
            if (reason_code == 15)
                CLOGI("May password wrong \n");
            break;
        default:
            break;
    }
}


static int _app_wifi_event_cb(void *arg, event_module_t event_module, int event_id, void *event_data)
{
    event_ap_sta_add_param_t *sta_add_param;
    event_ap_sta_del_param_t *sta_del_param;
    event_disconnect_param_t *disc_evt;
    event_scan_done_param_t *scan_evt;

    switch (event_id) 
    {
        case EVENT_WIFI_INIT_DONE:
        LISA_LOGI(TAG, "EVENT_WIFI_INIT_DONE");
        lisa_wifi_cb_post(LISA_WIFI_CB_EVENT_INIT_DONE);
        break;
        case EVENT_WIFI_CONNECTED:
        LISA_LOGI(TAG, "EVENT_WIFI_CONNECTED");
        break;
        case EVENT_WIFI_GOT_IP:
        LISA_LOGI(TAG, "EVENT_WIFI_GOT_IP");
        break;
        case EVENT_WIFI_STA_DHCP_FAIL:
        LISA_LOGI(TAG, "EVENT_WIFI_STA_DHCP_FAIL");
        break;
        case EVENT_WIFI_SCAN_DONE:
        LISA_LOGI(TAG, "EVENT_WIFI_SCAN_DONE");
        break;
        case EVENT_WIFI_DISCONNECT:
        case EVENT_WIFI_STA_CONNECT_FAIL:
        disc_evt = (event_disconnect_param_t *)event_data;
        LISA_LOGI(TAG,
                  "event <%d %d> disconnected or connect fail, max retry reach %d",
                  (int)event_module,
                  (int)event_id,
                  disc_evt->max_retry_reach);
        wifi_error_status_info(disc_evt->erro_code, disc_evt->status_code, disc_evt->reason_code);
        break;
        case EVENT_WIFI_AP_STARTED:
        LISA_LOGI(TAG, "EVENT_WIFI_AP_STARTED");
        break;
        case EVENT_WIFI_AP_STA_ADD:
        sta_add_param = (event_ap_sta_add_param_t *)event_data;
        LISA_LOGI(TAG, "EVENT_WIFI_AP_STA_ADD, sta_idx: %d", sta_add_param->sta_idx);
        break;
        case EVENT_WIFI_AP_STA_DEL:
        sta_del_param = (event_ap_sta_del_param_t *)event_data;
        LISA_LOGI(TAG, "EVENT_WIFI_AP_STA_DEL, sta_idx: %d", sta_del_param->sta_idx);
        break;
        case EVENT_WIFI_AP_STOPPED:
        LISA_LOGI(TAG, "EVENT_WIFI_AP_STOPPED");
        break;
        default:
        LISA_LOGI(TAG, "rx event <%d %d>", event_module, event_id);
        break;
    }

    return 0;
}

static int single_core_wifi_init(lisa_wifi_ops_t *ops)
{
    int ret = 0;

    if (ops == NULL || ops->custom_mac == NULL) {
        LOGE("ops or ops->custom_mac is NULL");
        return -1;
    }

    // 保存用户的 ops
    memcpy(&g_user_ops, ops, sizeof(lisa_wifi_ops_t));

    ret = lisa_wifi_cb_task_init();
    if (ret != 0) {
        return ret;
    }

    struct wifi_ops wifi_ops = {
        .get_mac = ops->custom_mac,
        .temp_update = ls_temp_por_update,
    };

    memset(_sshram, 0, (_eshram - _sshram));

    ret = ls_rf_cali_proc();
    if (ret != 0) {
        LOGE("WiFi calibration failed with error: %d", ret);
        return ret;
    }

    ls_crypto_init();

    ret = ls_event_init();
    if (ret != 0) {
        LOGE("Failed to initialize event system: %d", ret);
        return ret;
    }

    ret = ls_event_register_cb(EVENT_WIFI, EVENT_ID_ALL, _app_wifi_event_cb, NULL);
    if (ret != 0) {
        LOGE("Failed to register WiFi event callback: %d", ret);
        return ret;
    }

    ret = wifi_ops_register(&wifi_ops);
    if (ret != 0) {
        LOGE("Failed to register WiFi ops: %d", ret);
        return ret;
    }

    ret = wifi_init();
    if (ret != 0) {
        LOGE("Failed to initialize WiFi: %d", ret);
        return ret;
    }

    vrtc_init();
}


static int diff_core_lwip_init(void)
{
    int ret = 0;

    ret = lisa_wifi_cb_task_init();
    if (ret != 0) {
        return ret;
    }

    ipc_master_wifi_init();

    // register event
    ls_event_init();
    ls_event_register_cb(EVENT_WIFI, EVENT_ID_ALL, _app_wifi_event_cb, NULL);

    return 0;
}


#if CONFIG_WIFI_LWIP_DIFF_CORE
#if CONFIG_WIFI

/**
 * @brief 双核模式下，WIFI协议栈侧，必须用这个实现，才能实现跨核获取mac地址（由LWIP侧提供）
 */
static int8_t custom_get_wifi_mac(uint8_t mac_addr[6])
{
    int8_t ret = 0;

    if (!mac_addr)
        return -1;
    ret = ls_get_mac_customized(mac_addr);
    return ret;
}

static int diff_core_wifi_init(void)
{
    int ret = 0;

    struct wifi_ops ops = {
        .get_mac = custom_get_wifi_mac,
        .temp_update = ls_temp_por_update,
    };
    ls_crypto_init();

    ret = ls_rf_cali_proc();
    if (ret != 0) {
        LOGE("WiFi calibration failed with error: %d", ret);
        return ret;
    }

    wifi_ops_register(&ops);

    ret = wifi_init();
    if (ret != 0) {
        LOGE("Failed to initialize WiFi: %d", ret);
        return ret;
    }

    vrtc_init();

    return ret;
}

/**
 * @brief 如果是双核模式，WIFI协议栈侧，不支持设置OPS
 */
int lisa_wifi_init(void){
    int ret = 0;

    ret = diff_core_wifi_init();
}
#elif CONFIG_LWIP

static lisa_wifi_ops_t m_ops;

/**
 * @brief 这个函数是被IPC调用的，用于WiFi协议栈侧获取MAC地址（用户设置自定义MAC地址）
 */
int8_t ls_get_mac_customized(uint8_t mac_addr[6])
{
    if (m_ops.custom_mac) {
        // 重定向到用户设置的custom_mac接口
        return m_ops.custom_mac(mac_addr);
    } else {
        LOGE("User not set custom_mac interface, peer core get mac failed");
        return -1;
    }
}

/**
 * @brief 如果是双核模式，LWIP侧，支持设置OPS
 */
int lisa_wifi_init(lisa_wifi_ops_t *ops)
{
    int ret = 0;
    if (ops == NULL) {
        LOGE("ops is NULL");
        return -1;
    }
    memcpy(&m_ops, ops, sizeof(lisa_wifi_ops_t));
    memcpy(&g_user_ops, ops, sizeof(lisa_wifi_ops_t));
    ret = diff_core_lwip_init();

    return ret;
}
#endif
#elif CONFIG_WIFI_LWIP_SAME_CORE
int lisa_wifi_init(lisa_wifi_ops_t *ops)
{
    int ret = 0;

    ret = single_core_wifi_init(ops);
    return ret;
}
#endif //CONFIG_WIFI_LWIP_DIFF_CORE



#if CONFIG_WIFI_LWIP_DIFF_CORE
#if CONFIG_WIFI
static int app_ipc_init(void)
{
    struct ipc_slave_cb_tag ipc_cb = {
        .ipc_wifi_tx = net_ipc_send,
        .ipc_wifi_rx_cfm = net_ipc_rx_cfm,
    };

    ipc_mem_init(1);
    ic_lock_init();
    ipc_slave_init(&ipc_cb);
    extern uint8_t _sshram[], _eshram[];
    memset(_sshram, 0, (_eshram - _sshram));

    return 0;
}

SYS_INIT(app_ipc_init, SYS_INIT_LEVEL_PRE_DEVICES_INIT, SYS_INIT_SUB_PRIORITY_MAX); /* 优先级 */
#elif CONFIG_LWIP
static int app_ipc_init(void)
{
    struct ipc_master_cb_tag ipc_cb = {
            .wifi_tx_data_cfm   = wlif_tx_cfm,
            .wifi_rx_data       = wlif_rx_buf_forward,
            .indication_handler = ipc_master_indication_handler
    };

    ic_lock_init();
    ipc_master_init(&ipc_cb);
    return 0;
}

SYS_INIT(app_ipc_init,SYS_INIT_LEVEL_PRE_DEVICES_INIT, SYS_INIT_SUB_PRIORITY_MAX);
#endif //CONFIG_WIFI
#endif //CONFIG_WIFI_LWIP_DIFF_CORE

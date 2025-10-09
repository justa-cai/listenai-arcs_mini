// Copyright (c) 2025, LISTENAI
//
// SPDX-License-Identifier: Apache-2.0

#include "wifi_manager/wifi_manager.h"
#include "wifi_manager/wifi_manager_wifi_ops.h"
#include "wifi_manager/dlist.h"
#include "wifi_manager/wifi_manager_storage.h"

#include <stdbool.h>
#include <string.h>
#include <errno.h>


#define TAG "wifi_mgr"
#include "lisa_log.h"

#define _WAIT_FOREVER  LISA_WAIT_FOREVER
#define WIFI_SCAN_MAX_NUMBER  (32)

#define LISA_WAIT_FOREVER (0xFFFFFFFFU)

#define LISA_NO_WAIT (0U)

#define LISA_OK (0)

#ifndef CONFIG_WIFI_MGR_CONNECT_TIMEOUT
#define CONFIG_WIFI_MGR_CONNECT_TIMEOUT (15000)
#endif

#define WIFI_MGR_MALLOC(size) (s_wifi_mgr_obj->mem_ops.malloc(size))
#define WIFI_MGR_CALLOC(nmemb, size) (s_wifi_mgr_obj->mem_ops.calloc(nmemb, size))
#define WIFI_MGR_FREE(ptr) (s_wifi_mgr_obj->mem_ops.free(ptr))

#define WIFI_MGR_CHECK(cond, ret_val) ({                                      \
    if (!(cond)) {                                                            \
        LISA_LOGE(TAG, "Check failed at %s(%d)", __FUNCTION__, __LINE__);            \
        return (ret_val);                                                     \
    }                                                                         \
})

#define WIFI_MGR_FREE_AND_NULL(ptr) ({                                        \
    WIFI_MGR_FREE(ptr);                                                     \
    (ptr) = NULL;                                                             \
})



#define WIFI_LIST_ID                            1
#define WIFI_INFO_COUNT_ID                      2
#define MAX_WIFI_AUTO_CONN_RETRY                1
#define MAX_AP_FAILURE_COUNT                    3

#define WIFI_MGR_QUEUE_COUNT                    10
#define WIFI_MGR_QUEUE_NAME                     "wifi_mgr_queue"

#define WIFI_MGR_THREAD_STACK_SIZE               CONFIG_WIFI_MGR_THREAD_STACK
#define WIFI_MGR_THREAD_PRIORITY                 CONFIG_WIFI_MGR_THREAD_PRIORITY

#define WIFI_MGR_CONNECTION_EVENT               (WIFI_MGR_WIFI_EVT_STA_CONNECTED | WIFI_MGR_WIFI_EVT_STA_DISCONNECTED | WIFI_MGR_WIFI_EVT_STA_CONNECTING | WIFI_MGR_WIFI_EVT_STA_CONNECTION_FAILED | WIFI_MGR_WIFI_EVT_SCAN_FAILED)

typedef struct {
    wifi_mgr_sta_config_t config;
    wifi_mgr_connection_status_t sta_status;
} wifi_mgr_device_t;

typedef struct wifi_mgr_event_cb {
    sys_dnode_t node;
    union {
        wifi_mgr_connection_cb_t connection_handler;
        wifi_mgr_scan_done_cb_t scandone_handler;
        void *handler;
    };
    uint32_t event;
    void *arg;
} wifi_mgr_event_cb_t;

typedef struct {
    sys_dnode_t node;
    char ssid[32];              // SSID
    char bssid[18];             // BSSID (XX:XX:XX:XX:XX:XX格式)
    uint32_t failure_count;     // 连续失败次数
} wifi_mgr_ap_failure_record_t;

typedef struct {
    char last_tried_ssid[32];   // 上次尝试的SSID
    char last_tried_bssid[18];  // 上次尝试的BSSID (XX:XX:XX:XX:XX:XX格式)
} wifi_mgr_last_tried_ap_t;

typedef struct {
    wifi_mgr_autoconn_config_t config;
    bool enable;
    wifi_mgr_last_tried_ap_t last_tried_ap;  // 记住上次尝试的具体AP
    sys_dlist_t failure_records_list;  // 失败记录链表
} wifi_mgr_auto_conn_obj_t;

typedef struct {
    wifi_mgr_device_t sta_device;
    wifi_mgr_wifi_event_cb_t wifi_event_cb;
    sys_dlist_t wifi_callback_list;
    wifi_mgr_auto_conn_obj_t *auto_connect_obj;
    void* mutex;
    void* queue;
    bool thread_exit;
    void *thread;
    wifi_manager_mem_ops_t mem_ops;
    wifi_manager_os_ops_t os_ops;
    wifi_manager_wifi_ops_t wifi_ops;
    wifi_storage_ctx_t storage_ctx;
} wifi_mgr_obj_t;

static wifi_mgr_obj_t *s_wifi_mgr_obj = NULL;

#define WIFI_MGR_MUTEX_LOCK() do {                      \
    s_wifi_mgr_obj->os_ops.mutex_lock(s_wifi_mgr_obj->mutex, _WAIT_FOREVER);    \
} while (0)

#define WIFI_MGR_MUTEX_UNLOCK() do {                    \
    s_wifi_mgr_obj->os_ops.mutex_unlock(s_wifi_mgr_obj->mutex);             \
} while (0)


static int wifi_auto_connect_stop(wifi_mgr_auto_conn_obj_t *auto_connect_obj);
static void wifi_mgr_thread(void *arg);

static bool is_ap_blacklisted(wifi_mgr_auto_conn_obj_t *auto_connect_obj, const char *ssid, const char *bssid)
{
    if (!auto_connect_obj || !ssid || !bssid) {
        return false;
    }
    
    wifi_mgr_ap_failure_record_t *entry;
    SYS_DLIST_FOR_EACH_CONTAINER(&auto_connect_obj->failure_records_list, entry, node) {
        if (strcmp(entry->ssid, ssid) == 0 && strcmp(entry->bssid, bssid) == 0) {
            if (entry->failure_count >= MAX_AP_FAILURE_COUNT) {
                return true;
            }
        }
    }
    return false;
}

static void increment_ap_failure_count(wifi_mgr_auto_conn_obj_t *auto_connect_obj, const char *ssid, const char *bssid)
{
    if (!auto_connect_obj || !ssid || !bssid) {
        return;
    }
    
    // 查找现有记录
    wifi_mgr_ap_failure_record_t *entry;
    SYS_DLIST_FOR_EACH_CONTAINER(&auto_connect_obj->failure_records_list, entry, node) {
        if (strcmp(entry->ssid, ssid) == 0 && strcmp(entry->bssid, bssid) == 0) {
            entry->failure_count++;
            LISA_LOGW(TAG, "AP %s (%s) failure count: %u", ssid, bssid, entry->failure_count);
            if (entry->failure_count >= MAX_AP_FAILURE_COUNT) {
                LISA_LOGI(TAG, "AP %s (%s) blacklisted after %u failures", ssid, bssid, entry->failure_count);
            }
            return;
        }
    }
    
    // 添加新记录
    wifi_mgr_ap_failure_record_t *new_record = WIFI_MGR_CALLOC(1, sizeof(wifi_mgr_ap_failure_record_t));
    if (!new_record) {
        LISA_LOGE(TAG, "Failed to allocate memory for failure record");
        return;
    }
    
    strncpy(new_record->ssid, ssid, sizeof(new_record->ssid) - 1);
    new_record->ssid[sizeof(new_record->ssid) - 1] = '\0';
    strncpy(new_record->bssid, bssid, sizeof(new_record->bssid) - 1);
    new_record->bssid[sizeof(new_record->bssid) - 1] = '\0';
    new_record->failure_count = 1;
    
    sys_dlist_prepend(&auto_connect_obj->failure_records_list, &new_record->node);
    
    LISA_LOGI(TAG, "Added failure record for AP %s (%s), count: 1", ssid, bssid);
}

static void reset_ap_failure_count(wifi_mgr_auto_conn_obj_t *auto_connect_obj, const char *ssid, const char *bssid)
{
    if (!auto_connect_obj || !ssid || !bssid) {
        return;
    }
    
    wifi_mgr_ap_failure_record_t *entry;
    SYS_DLIST_FOR_EACH_CONTAINER(&auto_connect_obj->failure_records_list, entry, node) {
        if (strcmp(entry->ssid, ssid) == 0 && strcmp(entry->bssid, bssid) == 0) {
            entry->failure_count = 0;
            LISA_LOGI(TAG, "Reset failure count for AP %s (%s)", ssid, bssid);
            return;
        }
    }
}



typedef enum{
    WIFI_MGR_QUEUE_MSG_EVENT_WIFI_AUTO_CONNECT_START,
    WIFI_MGR_QUEUE_MSG_EVENT_WIFI_AUTO_CONNECT_STOP,

}wifi_mgr_queue_message_event_e;

typedef struct{
    wifi_mgr_queue_message_event_e event;
    void* payload;
}wifi_mgr_queue_message_t;
static void start_auto_connect_timer(wifi_mgr_auto_conn_obj_t *auto_connect_obj)
{
    if (auto_connect_obj != NULL && auto_connect_obj->enable == true) {
        //TODO
    }
}

static void wifi_mgr_dispatch_user_callbacks(wifi_mgr_wifi_event_t event, void *event_data, uint32_t data_len)
{
    wifi_mgr_event_cb_t *entry, *next;
    SYS_DLIST_FOR_EACH_CONTAINER_SAFE(&s_wifi_mgr_obj->wifi_callback_list, entry, next, node) {
        if (!(entry->event & event)) {
            continue;
        }
        if (event & WIFI_MGR_WIFI_EVT_STA_CONNECTED) {

            wifi_mgr_connection_info_t connection_info = {0};
            connection_info.sta_info = event_data;
            connection_info.status = WIFI_MGR_STA_CONNECTED;
            entry->connection_handler(&connection_info, entry->arg);

        } else if (event & WIFI_MGR_WIFI_EVT_STA_CONNECTING) {
            wifi_mgr_connection_info_t connection_info = {0};
            connection_info.sta_info = event_data;
            connection_info.status = WIFI_MGR_STA_CONNECTING;
            entry->connection_handler(&connection_info, entry->arg);

        } else if (event & WIFI_MGR_WIFI_EVT_STA_DISCONNECTED) {
            // 只有在之前已连接的情况下才触发断开回调
            if (s_wifi_mgr_obj->sta_device.sta_status == WIFI_MGR_STA_CONNECTED) {
                s_wifi_mgr_obj->sta_device.sta_status = WIFI_MGR_STA_DISCONNECTED;
                wifi_mgr_connection_info_t connection_info = {0};
                connection_info.sta_info = event_data;
                connection_info.status = WIFI_MGR_STA_DISCONNECTED;
                connection_info.reason = 0;
                entry->connection_handler(&connection_info, entry->arg);
            } else {
                LISA_LOGD(TAG, "Received disconnect event but device was not connected, ignoring user callback");
            }

        } else if (event & WIFI_MGR_WIFI_EVT_SCAN_DONE) {

            wifi_mgr_scan_ap_list_t *aps_list = event_data;
            entry->scandone_handler(aps_list->ap_info, aps_list->count, entry->arg);
        } else if (event & WIFI_MGR_WIFI_EVT_STA_CONNECTION_FAILED) {
            int reason;
            if (event_data == NULL) {
                reason = -1;
            } else {
                reason = *((int *)(event_data));    //parse reason code from event_data
            }
            LISA_LOGE(TAG,"WIFI_DEV_EVT_STA_CONNECTION_FAILED, reason: %d", reason);
            // 连接失败不触发用户回调，因为设备从未真正连上过
            // 只记录日志，不通知用户
        } else if (event & WIFI_MGR_WIFI_EVT_SCAN_FAILED) {
            int reason = *((int *)(event_data));    //parse reason code from event_data.
            wifi_mgr_scan_info_t *aps_info = NULL;
            int ap_num = - reason;
            entry->scandone_handler(aps_info, ap_num, entry->arg);
        }
    }
}

static void wifi_event_handler(wifi_mgr_wifi_event_t events, void *event_data, uint32_t data_len, void *arg)
{
    LISA_LOGI(TAG, "%s: wifi_event: 0x%02X,event_data:%p ", __FUNCTION__, events,event_data);
    if (events & WIFI_MGR_WIFI_EVT_STA_CONNECTED) {
        wifi_mgr_sta_config_t *sta_config = event_data;
        s_wifi_mgr_obj->sta_device.sta_status = WIFI_MGR_STA_CONNECTED;
        memcpy(&s_wifi_mgr_obj->sta_device.config, sta_config, sizeof(wifi_mgr_sta_config_t));
        if (s_wifi_mgr_obj->auto_connect_obj != NULL) {  /* stop auto connection timer and delayable work */
            wifi_auto_connect_stop(s_wifi_mgr_obj->auto_connect_obj);
        }
    }
    
    wifi_mgr_dispatch_user_callbacks(events, event_data, data_len);
}

static int wifi_cb_list_add_item(wifi_mgr_wifi_event_t event, void *event_handler, void* arg)
{
    wifi_mgr_event_cb_t *item = WIFI_MGR_CALLOC(1, sizeof(wifi_mgr_event_cb_t));
    WIFI_MGR_CHECK(s_wifi_mgr_obj != NULL, -ENOMEM);

    WIFI_MGR_MUTEX_LOCK();
    item->arg = arg;
    item->event = event;
    item->handler = event_handler;
    sys_dlist_prepend(&s_wifi_mgr_obj->wifi_callback_list, &item->node);
    WIFI_MGR_MUTEX_UNLOCK();
    return 0;
}

static int wifi_cb_list_remove_item(wifi_mgr_wifi_event_t event, void *event_handler)
{
    WIFI_MGR_MUTEX_LOCK();
    wifi_mgr_event_cb_t *removed_node = NULL;
    wifi_mgr_event_cb_t *entry, *next;
    SYS_DLIST_FOR_EACH_CONTAINER_SAFE(&s_wifi_mgr_obj->wifi_callback_list, entry, next, node) {
        if (entry->event & event) {
            if (entry->handler == event_handler) {
                sys_dlist_remove(&entry->node);
                removed_node = entry;
                break;
            }
        }
    }
    if (removed_node == NULL) {
        WIFI_MGR_MUTEX_UNLOCK();
        return -ENOENT; /* Not found */
    }
    WIFI_MGR_FREE(removed_node);
    WIFI_MGR_MUTEX_UNLOCK();
    return 0;
}

static int ops_check(wifi_mgr_ops_t *ops)
{
    // 验证内存操作函数有效性
    WIFI_MGR_CHECK(ops->mem_ops->malloc != NULL, -EINVAL);
    WIFI_MGR_CHECK(ops->mem_ops->calloc != NULL, -EINVAL);
    WIFI_MGR_CHECK(ops->mem_ops->align_malloc != NULL, -EINVAL);
    WIFI_MGR_CHECK(ops->mem_ops->nocache_malloc != NULL, -EINVAL);
    WIFI_MGR_CHECK(ops->mem_ops->free != NULL, -EINVAL);
    
    // 验证系统操作函数有效性
    WIFI_MGR_CHECK(ops->os_ops->mutex_create != NULL, -EINVAL);
    WIFI_MGR_CHECK(ops->os_ops->mutex_lock != NULL, -EINVAL);
    WIFI_MGR_CHECK(ops->os_ops->mutex_unlock != NULL, -EINVAL);
    WIFI_MGR_CHECK(ops->os_ops->mutex_delete != NULL, -EINVAL);
    WIFI_MGR_CHECK(ops->os_ops->queue_create != NULL, -EINVAL);
    WIFI_MGR_CHECK(ops->os_ops->queue_push != NULL, -EINVAL);
    WIFI_MGR_CHECK(ops->os_ops->queue_pop != NULL, -EINVAL);
    WIFI_MGR_CHECK(ops->os_ops->queue_delete != NULL, -EINVAL);
    WIFI_MGR_CHECK(ops->os_ops->thread_create != NULL, -EINVAL);
    WIFI_MGR_CHECK(ops->os_ops->thread_delete != NULL, -EINVAL);

    /* 验证WiFi操作函数有效性 */
    WIFI_MGR_CHECK(ops->wifi_ops->init != NULL, -EINVAL);
    WIFI_MGR_CHECK(ops->wifi_ops->deinit != NULL, -EINVAL);
    WIFI_MGR_CHECK(ops->wifi_ops->add_callback != NULL, -EINVAL);
    WIFI_MGR_CHECK(ops->wifi_ops->remove_callback != NULL, -EINVAL);
    WIFI_MGR_CHECK(ops->wifi_ops->sta_connect != NULL, -EINVAL);
    WIFI_MGR_CHECK(ops->wifi_ops->sta_disconnect != NULL, -EINVAL);

    return 0;
}

int wifi_mgr_init(wifi_mgr_ops_t *ops)
{
    int ret = 0;
    uint32_t count = 0;
    
    WIFI_MGR_CHECK(s_wifi_mgr_obj == NULL, -EIO);
    WIFI_MGR_CHECK(ops != NULL, -EINVAL);

    ret = ops_check(ops);
    if (ret != 0) {
        LISA_LOGE(TAG, "ops_check failed");
        return ret;
    }
    
    s_wifi_mgr_obj = ops->mem_ops->calloc(1, sizeof(wifi_mgr_obj_t));
    if (s_wifi_mgr_obj == NULL) {
        LISA_LOGE(TAG, "mem_ops->calloc failed size: %ld", sizeof(wifi_mgr_obj_t));
        return -ENOMEM;
    }
    
    memcpy(&s_wifi_mgr_obj->mem_ops, ops->mem_ops, sizeof(wifi_manager_mem_ops_t));
    memcpy(&s_wifi_mgr_obj->os_ops, ops->os_ops, sizeof(wifi_manager_os_ops_t));
    memcpy(&s_wifi_mgr_obj->wifi_ops, ops->wifi_ops, sizeof(wifi_manager_wifi_ops_t));

    s_wifi_mgr_obj->sta_device.sta_status = WIFI_MGR_STA_DISCONNECTED;

    sys_dlist_init(&s_wifi_mgr_obj->wifi_callback_list);

    s_wifi_mgr_obj->wifi_ops.init();

    s_wifi_mgr_obj->wifi_event_cb.handler = &wifi_event_handler;
    s_wifi_mgr_obj->wifi_event_cb.events = WIFI_MGR_WIFI_EVT_STA_CONNECTED |
                                            WIFI_MGR_WIFI_EVT_STA_DISCONNECTED |
                                            WIFI_MGR_WIFI_EVT_STA_CONNECTING |
                                            WIFI_MGR_WIFI_EVT_STA_CONNECTION_FAILED |
                                            WIFI_MGR_WIFI_EVT_SCAN_FAILED |
                                            WIFI_MGR_WIFI_EVT_SCAN_DONE;
    s_wifi_mgr_obj->wifi_event_cb.arg = NULL;
    s_wifi_mgr_obj->wifi_ops.add_callback(&s_wifi_mgr_obj->wifi_event_cb);

    s_wifi_mgr_obj->auto_connect_obj = WIFI_MGR_MALLOC(sizeof(wifi_mgr_auto_conn_obj_t));
    if (s_wifi_mgr_obj->auto_connect_obj == NULL) {
        goto __cleanup;
    }
    memset(s_wifi_mgr_obj->auto_connect_obj, 0, sizeof(wifi_mgr_auto_conn_obj_t));
    s_wifi_mgr_obj->auto_connect_obj->config.interval_ms = 0xffff;
    sys_dlist_init(&s_wifi_mgr_obj->auto_connect_obj->failure_records_list);

    s_wifi_mgr_obj->mutex = s_wifi_mgr_obj->os_ops.mutex_create();
    s_wifi_mgr_obj->queue = s_wifi_mgr_obj->os_ops.queue_create(WIFI_MGR_QUEUE_COUNT, WIFI_MGR_QUEUE_NAME, sizeof(wifi_mgr_queue_message_t));
    
    wifi_storage_ops_t storage_ops = {
        .malloc = s_wifi_mgr_obj->mem_ops.malloc,
        .calloc = s_wifi_mgr_obj->mem_ops.calloc,
        .free = s_wifi_mgr_obj->mem_ops.free,
        .mutex_lock = s_wifi_mgr_obj->os_ops.mutex_lock,
        .mutex_unlock = s_wifi_mgr_obj->os_ops.mutex_unlock
    };
    ret = wifi_storage_init(&s_wifi_mgr_obj->storage_ctx, &storage_ops, s_wifi_mgr_obj->mutex);
    if (ret != 0) {
        LISA_LOGE(TAG, "wifi_storage_init failed");
        goto __cleanup;
    }
    
    wifi_manager_os_thread_attr_t thread_attr = {
        .name = "wifi_mgr_thread",
        .priority = WIFI_MGR_THREAD_PRIORITY,
        .stack_size = WIFI_MGR_THREAD_STACK_SIZE
    };
    s_wifi_mgr_obj->thread = s_wifi_mgr_obj->os_ops.thread_create(&thread_attr, wifi_mgr_thread, NULL);

    return 0;

__cleanup:
    LISA_LOGE(TAG, "free s_wifi_mgr_obj: %p", s_wifi_mgr_obj);
    if (s_wifi_mgr_obj) {
        ops->mem_ops->free(s_wifi_mgr_obj);
        s_wifi_mgr_obj = NULL;
    }
    return ret;
}

int wifi_mgr_deinit()
{
    WIFI_MGR_CHECK(s_wifi_mgr_obj != NULL, -EIO);
    WIFI_MGR_MUTEX_LOCK();

    s_wifi_mgr_obj->thread_exit = true;
    
    s_wifi_mgr_obj->os_ops.thread_delete(s_wifi_mgr_obj->thread);

    if (s_wifi_mgr_obj->auto_connect_obj) {
        wifi_auto_connect_stop(s_wifi_mgr_obj->auto_connect_obj);
        // 释放失败记录链表
        wifi_mgr_ap_failure_record_t *entry, *next;
        SYS_DLIST_FOR_EACH_CONTAINER_SAFE(&s_wifi_mgr_obj->auto_connect_obj->failure_records_list, entry, next, node) {
            sys_dlist_remove(&entry->node);
            WIFI_MGR_FREE(entry);
        }
        WIFI_MGR_FREE(s_wifi_mgr_obj->auto_connect_obj);
        s_wifi_mgr_obj->auto_connect_obj = NULL;
    }
    
    wifi_storage_deinit(&s_wifi_mgr_obj->storage_ctx);

    void (*free_func)(void *) = s_wifi_mgr_obj->mem_ops.free;
    
    WIFI_MGR_MUTEX_UNLOCK();
    
    if (s_wifi_mgr_obj->mutex) {
        s_wifi_mgr_obj->os_ops.mutex_delete(s_wifi_mgr_obj->mutex);
    }
    
    if (s_wifi_mgr_obj->queue) {
        s_wifi_mgr_obj->os_ops.queue_delete(s_wifi_mgr_obj->queue);
    }
    
    free_func(s_wifi_mgr_obj);
    s_wifi_mgr_obj = NULL;

    return 0;
}

int wifi_mgr_sta_enable()
{
    return s_wifi_mgr_obj->wifi_ops.sta_enable();
}

int wifi_mgr_sta_disable()
{
    return s_wifi_mgr_obj->wifi_ops.sta_disable();
}

bool wifi_mgr_sta_is_enable(void)
{
    return s_wifi_mgr_obj->wifi_ops.sta_is_enable();
}

int wifi_mgr_sta_connect(wifi_mgr_sta_config_t *sta_config, bool asynchronous)
{
    int ret = 0;
    WIFI_MGR_CHECK(sta_config != NULL, -EINVAL);

    uint32_t wait = asynchronous ? LISA_NO_WAIT : CONFIG_WIFI_MGR_CONNECT_TIMEOUT;
    LISA_LOGD(TAG, "start to connect ssid: %s, pwd: %s", sta_config->ssid, sta_config->pwd);
    ret = s_wifi_mgr_obj->wifi_ops.sta_connect(sta_config, wait);
    if (ret < 0) {
        LISA_LOGW(TAG,"station connect failed, ret: %d", ret);
    } else if (ret == 0) {
        if (asynchronous) {
            s_wifi_mgr_obj->sta_device.sta_status = WIFI_MGR_STA_CONNECTING;
        }
    }
    return ret;
}

int wifi_mgr_sta_add_connection_cb(wifi_mgr_connection_cb_t connection_cb, void *arg)
{
    WIFI_MGR_CHECK(connection_cb != NULL, -EINVAL);
    int ret = wifi_cb_list_add_item(WIFI_MGR_CONNECTION_EVENT,
        connection_cb, arg);
    return ret;
}

int wifi_mgr_sta_remove_connection_cb(wifi_mgr_connection_cb_t connection_cb)
{
    WIFI_MGR_CHECK(connection_cb != NULL, -EINVAL);
    int ret = wifi_cb_list_remove_item(WIFI_MGR_CONNECTION_EVENT, connection_cb);
    return ret;
}

int wifi_mgr_sta_disconnect(bool asynchronous)
{
    WIFI_MGR_CHECK(s_wifi_mgr_obj != NULL, -EIO);
    uint32_t wait = asynchronous ? LISA_NO_WAIT : LISA_WAIT_FOREVER;
    LISA_LOGD(TAG, "start to disconnect, wait: %lu", wait);
    int ret = s_wifi_mgr_obj->wifi_ops.sta_disconnect(wait);
    if (ret < 0) {
        LISA_LOGW(TAG, "station disconnect failed, ret: %d", ret);
    }
    return ret;
}

int wifi_mgr_sta_get_connected_info(wifi_mgr_sta_config_t *sta_info)
{
    WIFI_MGR_CHECK(sta_info != NULL, -EINVAL);
    WIFI_MGR_MUTEX_LOCK();
    if (s_wifi_mgr_obj->sta_device.sta_status != WIFI_MGR_STA_CONNECTED) {
        LISA_LOGE(TAG,"WiFi station is not connected");
        WIFI_MGR_MUTEX_UNLOCK();
        return -ENOENT; //Wifi station is not connected
    }
    memcpy(sta_info, &s_wifi_mgr_obj->sta_device.config, sizeof(wifi_mgr_sta_config_t));
    WIFI_MGR_MUTEX_UNLOCK();
    return 0;
}

int wifi_mgr_scan_ap(wifi_mgr_scan_info_t *ap_info, uint32_t size, bool asynchronous)
{
    if (!s_wifi_mgr_obj->wifi_ops.sta_is_enable()) {
        LISA_LOGW(TAG, "STA is disabled, scan not allowed");
        return -ENODEV;
    }
    
    uint32_t wait = asynchronous ? LISA_NO_WAIT : LISA_WAIT_FOREVER;
    int ret = s_wifi_mgr_obj->wifi_ops.scan_ap(ap_info, size, wait);
    
    if (ret < 0) {
        LISA_LOGW(TAG, "Scan failed, ret: %d", ret);
    }
    return ret;
}

int wifi_mgr_add_scan_done_cb(wifi_mgr_scan_done_cb_t scandone_cb, void *arg)
{
    WIFI_MGR_CHECK(scandone_cb != NULL, -EINVAL);
    int ret = wifi_cb_list_add_item(WIFI_MGR_WIFI_EVT_SCAN_DONE, scandone_cb, arg);
    return ret;
}

int wifi_mgr_remove_scan_done_cb(wifi_mgr_scan_done_cb_t scandone_cb)
{
    WIFI_MGR_CHECK(scandone_cb != NULL, -EINVAL);
    int ret = wifi_cb_list_remove_item(WIFI_MGR_WIFI_EVT_SCAN_DONE, scandone_cb);
    return ret;
}

int wifi_mgr_storage_search_ap(wifi_mgr_sta_config_t **matched_list,
                               wifi_mgr_storage_search_mode_t search_modes, void* target)
{
    WIFI_MGR_CHECK(s_wifi_mgr_obj != NULL, -EIO);
    return wifi_storage_search_ap(&s_wifi_mgr_obj->storage_ctx, matched_list, search_modes, target);
}

int wifi_mgr_storage_save_ap(wifi_mgr_sta_config_t *ap_info)
{
    WIFI_MGR_CHECK(s_wifi_mgr_obj != NULL, -EIO);
    return wifi_storage_save_ap(&s_wifi_mgr_obj->storage_ctx, ap_info);
}

int wifi_mgr_storage_save_ap_force(wifi_mgr_sta_config_t *ap_info)
{
    WIFI_MGR_CHECK(s_wifi_mgr_obj != NULL, -EIO);
    return wifi_storage_save_ap_force(&s_wifi_mgr_obj->storage_ctx, ap_info);
}

int wifi_mgr_storage_delete_ap(wifi_mgr_sta_config_t *ap_info)
{
    WIFI_MGR_CHECK(s_wifi_mgr_obj != NULL, -EIO);
    return wifi_storage_delete_ap(&s_wifi_mgr_obj->storage_ctx, ap_info);
}

static int find_next_ap_from_list(wifi_mgr_scan_info_t *aps_info, int ap_num, wifi_mgr_sta_config_t *next_ap)
{
    typedef struct {
        wifi_mgr_scan_info_t scan_info;
        wifi_mgr_sta_config_t sta_config;
        bool is_last_tried;
    } ap_candidate_t;
    
    ap_candidate_t *candidates = WIFI_MGR_MALLOC(sizeof(ap_candidate_t) * ap_num);
    if (candidates == NULL) {
        return -ENOMEM;
    }
    
    int candidate_count = 0;
    int last_tried_index = -1;
    
    // 收集所有可用的候选AP
    for (int idx = 0; idx < ap_num; idx++) {
        // 检查是否在黑名单中
        if (is_ap_blacklisted(s_wifi_mgr_obj->auto_connect_obj, aps_info[idx].ssid, aps_info[idx].bssid)) {
            LISA_LOGI(TAG, "Skipping blacklisted AP: %s (%s)", aps_info[idx].ssid, aps_info[idx].bssid);
            continue;
        }
        
        wifi_mgr_sta_config_t *hit_info_list = NULL;
        int hit_info_count = wifi_storage_search_ap(&s_wifi_mgr_obj->storage_ctx, &hit_info_list, SEARCH_BY_SSID, aps_info[idx].ssid);

        if ((hit_info_count <= 0) || (hit_info_list == NULL)) {
            if (hit_info_list != NULL) {
                s_wifi_mgr_obj->storage_ctx.ops.free(hit_info_list);
            }
            continue;
        }
        
        // 添加到候选列表
        memcpy(&candidates[candidate_count].scan_info, &aps_info[idx], sizeof(wifi_mgr_scan_info_t));
        memcpy(&candidates[candidate_count].sta_config, &hit_info_list[0], sizeof(wifi_mgr_sta_config_t));
        // 更新BSSID和RSSI信息（使用扫描结果中的实际值）
        memcpy(&candidates[candidate_count].sta_config.bssid, &aps_info[idx].bssid, sizeof(aps_info[idx].bssid));
        candidates[candidate_count].sta_config.rssi = aps_info[idx].rssi;
        
        // 检查是否为上次尝试的AP
        candidates[candidate_count].is_last_tried = 
            (strcmp(candidates[candidate_count].sta_config.ssid, s_wifi_mgr_obj->auto_connect_obj->last_tried_ap.last_tried_ssid) == 0) &&
            (strcmp(candidates[candidate_count].sta_config.bssid, s_wifi_mgr_obj->auto_connect_obj->last_tried_ap.last_tried_bssid) == 0);
        
        if (candidates[candidate_count].is_last_tried) {
            last_tried_index = candidate_count;
        }
        
        candidate_count++;
        s_wifi_mgr_obj->storage_ctx.ops.free(hit_info_list);
    }
    
    if (candidate_count == 0) {
        WIFI_MGR_FREE(candidates);
        LISA_LOGW(TAG, "No available APs (all blacklisted or not saved), scanning result count: %d", ap_num);
        return -ENODEV;
    }
    
    // 按信号强度排序（降序）
    for (int i = 0; i < candidate_count - 1; i++) {
        for (int j = 0; j < candidate_count - 1 - i; j++) {
            if (candidates[j].sta_config.rssi < candidates[j + 1].sta_config.rssi) {
                ap_candidate_t temp = candidates[j];
                candidates[j] = candidates[j + 1];
                candidates[j + 1] = temp;
            }
        }
    }
    
    // 重新查找上次尝试的AP在排序后的位置
    last_tried_index = -1;
    for (int i = 0; i < candidate_count; i++) {
        if (candidates[i].is_last_tried) {
            last_tried_index = i;
            break;
        }
    }
    
    // 选择下一个要尝试的AP
    int next_index;
    if (last_tried_index == -1) {
        // 没有上次尝试的记录或上次的AP不在当前扫描结果中，从信号最强的开始
        next_index = 0;
    } else {
        // 直接尝试下一个AP，简单轮询
        next_index = (last_tried_index + 1) % candidate_count;
        
        // 如果上次失败的AP是最后一个（信号最差），下次就回到第一个（信号最强）
        if (next_index == 0) {
            LISA_LOGI(TAG, "Last tried AP was the weakest, cycling back to strongest AP");
        }
    }
    
    memcpy(next_ap, &candidates[next_index].sta_config, sizeof(wifi_mgr_sta_config_t));
    
    LISA_LOGI(TAG, "Selected AP %d/%d: ssid:%s, rssi: %d, bssid:%s (last_tried_index=%d)", 
              next_index + 1, candidate_count, next_ap->ssid, next_ap->rssi, next_ap->bssid, last_tried_index);
    
    WIFI_MGR_FREE(candidates);
    return 0;
}

static void autoconnect_work_handler(void)
{
    /* Do nothing if sta is disabled */
    if(!s_wifi_mgr_obj->wifi_ops.sta_is_enable()){
        return;
    }
    /* Do nothing if sta is already connected */
    if(s_wifi_mgr_obj->sta_device.sta_status == WIFI_MGR_STA_CONNECTED){
        return;
    }

    /* do the processing that needs to be done periodically */
    int ap_num = 0;
    wifi_mgr_sta_config_t best_ap = {0};
    int ret;
    
    wifi_mgr_scan_info_t *aps_info = NULL;
    uint32_t nb_ap = WIFI_SCAN_MAX_NUMBER;
    aps_info = WIFI_MGR_MALLOC(sizeof(wifi_mgr_scan_info_t) * nb_ap);
    if (aps_info == NULL) {
        return;
    }

    ap_num = s_wifi_mgr_obj->wifi_ops.scan_ap(aps_info, nb_ap, LISA_WAIT_FOREVER);
    LISA_LOGI(TAG, "wifi scan ap_num: %d", ap_num);

    if (ap_num <= 0) {
        LISA_LOGW(TAG, "WiFi auto connection scan failed, ap_num:%d", ap_num);
        WIFI_MGR_FREE(aps_info);
        return;
    }

    // 在一个周期内尝试所有可用的AP，直到成功或全部尝试完
    bool connection_success = false;
    int max_attempts = 0;
    
    // 先计算有多少个可用AP
    for (int idx = 0; idx < ap_num; idx++) {
        if (!is_ap_blacklisted(s_wifi_mgr_obj->auto_connect_obj, aps_info[idx].ssid, aps_info[idx].bssid)) {
            wifi_mgr_sta_config_t *hit_info_list = NULL;
            int hit_info_count = wifi_storage_search_ap(&s_wifi_mgr_obj->storage_ctx, &hit_info_list, SEARCH_BY_SSID, aps_info[idx].ssid);
            if ((hit_info_count > 0) && (hit_info_list != NULL)) {
                max_attempts++;
            }
            if (hit_info_list != NULL) {
                s_wifi_mgr_obj->storage_ctx.ops.free(hit_info_list);
            }
        }
    }
    
    if (max_attempts == 0) {
        LISA_LOGW(TAG, "No available APs (all blacklisted or not saved), scanning result count: %d", ap_num);
        WIFI_MGR_FREE(aps_info);
        return;
    }
    
    LISA_LOGI(TAG, "Found %d available APs to try in this cycle", max_attempts);
    
    // 依次尝试所有可用的AP
    for (int attempt = 0; attempt < max_attempts && !connection_success; attempt++) {
        ret = find_next_ap_from_list(aps_info, ap_num, &best_ap);
        if (ret != 0) {
            LISA_LOGW(TAG, "No more APs to try");
            break;
        }
        
        LISA_LOGI(TAG, "Attempt %d/%d: Trying to connect to %s (%s)", attempt + 1, max_attempts, best_ap.ssid, best_ap.bssid);
        
        ret = s_wifi_mgr_obj->wifi_ops.sta_connect(&best_ap, CONFIG_WIFI_MGR_AUTO_CONNECT_TIMEOUT_MS);
        if (ret == 0) {
            LISA_LOGI(TAG, "Successfully connected to ssid: %s, bssid: %s, pwd: %s, channel: %d, rssi: %d", 
                     best_ap.ssid, best_ap.bssid, best_ap.pwd, best_ap.channel, best_ap.rssi);
            // 连接成功，清除上次尝试记录并重置失败计数
            reset_ap_failure_count(s_wifi_mgr_obj->auto_connect_obj, best_ap.ssid, best_ap.bssid);
            memset(&s_wifi_mgr_obj->auto_connect_obj->last_tried_ap, 0, sizeof(wifi_mgr_last_tried_ap_t));
            connection_success = true;
        } else {
            LISA_LOGE(TAG, "Failed to connect to ssid: %s, bssid: %s, pwd: %s, channel: %d, rssi: %d", 
                     best_ap.ssid, best_ap.bssid, best_ap.pwd, best_ap.channel, best_ap.rssi);
            // 增加失败计数并记录失败的AP
            increment_ap_failure_count(s_wifi_mgr_obj->auto_connect_obj, best_ap.ssid, best_ap.bssid);
            strncpy(s_wifi_mgr_obj->auto_connect_obj->last_tried_ap.last_tried_ssid, best_ap.ssid, sizeof(s_wifi_mgr_obj->auto_connect_obj->last_tried_ap.last_tried_ssid) - 1);
            strncpy(s_wifi_mgr_obj->auto_connect_obj->last_tried_ap.last_tried_bssid, best_ap.bssid, sizeof(s_wifi_mgr_obj->auto_connect_obj->last_tried_ap.last_tried_bssid) - 1);
            s_wifi_mgr_obj->auto_connect_obj->last_tried_ap.last_tried_ssid[sizeof(s_wifi_mgr_obj->auto_connect_obj->last_tried_ap.last_tried_ssid) - 1] = '\0';
            s_wifi_mgr_obj->auto_connect_obj->last_tried_ap.last_tried_bssid[sizeof(s_wifi_mgr_obj->auto_connect_obj->last_tried_ap.last_tried_bssid) - 1] = '\0';
        }
    }
    
    if (!connection_success) {
        LISA_LOGW(TAG, "Failed to connect to any AP in this cycle, tried %d APs", max_attempts);
    }

    WIFI_MGR_FREE(aps_info);
}


static int wifi_auto_connect_stop(wifi_mgr_auto_conn_obj_t *auto_connect_obj)
{

    return 0;
}

int wifi_mgr_auto_connect_start(wifi_mgr_autoconn_config_t *autoconn_config)
{
    wifi_mgr_queue_message_t msg;
    int ret;

    WIFI_MGR_CHECK(autoconn_config != NULL, -EINVAL);

    if(s_wifi_mgr_obj == NULL){
        return -EAGAIN;
    }

    msg.event = WIFI_MGR_QUEUE_MSG_EVENT_WIFI_AUTO_CONNECT_START;
    msg.payload = WIFI_MGR_MALLOC(sizeof(wifi_mgr_autoconn_config_t));
    if (msg.payload == NULL) {
        return -ENOMEM;
    }
    memcpy(msg.payload, autoconn_config, sizeof(wifi_mgr_autoconn_config_t));

    ret = s_wifi_mgr_obj->os_ops.queue_push(s_wifi_mgr_obj->queue, &msg, sizeof(wifi_mgr_autoconn_config_t), LISA_WAIT_FOREVER);

    return ret;
}

int wifi_mgr_auto_connect_stop()
{
    wifi_mgr_queue_message_t msg;
    int ret;

    if(s_wifi_mgr_obj == NULL){
        return -EAGAIN;
    }

    msg.event = WIFI_MGR_QUEUE_MSG_EVENT_WIFI_AUTO_CONNECT_STOP;
    msg.payload = NULL;

    ret = s_wifi_mgr_obj->os_ops.queue_push(s_wifi_mgr_obj->queue, &msg, sizeof(wifi_mgr_autoconn_config_t), LISA_WAIT_FOREVER);

    return ret;
}

wifi_mgr_connection_status_t wifi_mgr_sta_get_status()
{
    wifi_mgr_wifi_status_t wifi_sta_status;
    WIFI_MGR_MUTEX_LOCK();
    wifi_sta_status = s_wifi_mgr_obj->wifi_ops.sta_get_status();

    switch (wifi_sta_status) {
        case WIFI_MGR_WIFI_STATUS_STA_DISCONNECTED:
        if (s_wifi_mgr_obj->sta_device.sta_status != WIFI_MGR_STA_CONNECTING) {
            s_wifi_mgr_obj->sta_device.sta_status = WIFI_MGR_STA_DISCONNECTED;
        }
        break;

        case WIFI_MGR_WIFI_STATUS_STA_CONNECTING:
        s_wifi_mgr_obj->sta_device.sta_status = WIFI_MGR_STA_CONNECTING;
        break;

        case WIFI_MGR_WIFI_STATUS_STA_CONNECTED:
        s_wifi_mgr_obj->sta_device.sta_status = WIFI_MGR_STA_CONNECTED;
        break;

        default:
        break;
    }
    WIFI_MGR_MUTEX_UNLOCK();
    return s_wifi_mgr_obj->sta_device.sta_status;
}

static void wifi_mgr_thread(void *arg){
    int result;
    wifi_mgr_queue_message_t msg;
    wifi_mgr_autoconn_config_t* cfg = NULL;

    while(1){
        if(s_wifi_mgr_obj->thread_exit){
            LISA_LOGI(TAG, "wifi_mgr_thread exit\n");
            return;
        }
        result = s_wifi_mgr_obj->os_ops.queue_pop(s_wifi_mgr_obj->queue, &msg, sizeof(wifi_mgr_queue_message_t), s_wifi_mgr_obj->auto_connect_obj->config.interval_ms);
        if(result == LISA_OK){
            switch(msg.event){
                case WIFI_MGR_QUEUE_MSG_EVENT_WIFI_AUTO_CONNECT_START:
                    cfg = (wifi_mgr_autoconn_config_t*)msg.payload; // Fix, need to check if cfg is NULL
                    s_wifi_mgr_obj->auto_connect_obj->enable = true;
                    if(cfg != NULL){
                        memcpy(&s_wifi_mgr_obj->auto_connect_obj->config, cfg, sizeof(wifi_mgr_autoconn_config_t));
                    }
                    break;
                case WIFI_MGR_QUEUE_MSG_EVENT_WIFI_AUTO_CONNECT_STOP:
                    s_wifi_mgr_obj->auto_connect_obj->enable = false;
                    break;
                default:
                    break;
            }
            if(msg.payload){
                WIFI_MGR_FREE(msg.payload);
            }
        }
        
        if(s_wifi_mgr_obj->auto_connect_obj->enable){
            autoconnect_work_handler();
        }
    }
}

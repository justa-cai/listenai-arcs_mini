/**
****************************************************************************************
*
* @file netcfg.c
*
* @brief net config
*
* Copyright (C) ListenAI 2020-2099
*
*
****************************************************************************************
*/

/*
 * MACROS
 ****************************************************************************************
 */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "netcfg_ble.h"
#include "netcfg_bles.h"

#include "ble_gap.h"

#include "ble_gatt.h"
#include "ble_prf.h"

#include "ls_wifi_type.h"
#include "nvds_tag_def.h"
#include "wifi_api.h"

#include "btos_al.h"
#include "lisa_log.h"
#include "lisa_kv.h"
#include "../kv/kv_user.h"
#include "../cloud/config/aiui_cfg.h"
#include "assistant_controller.h"
#include "wifi_manager/wifi_manager.h"
#include "lisa_thread.h"
#include "lisa_mem.h"

static char *TAG = "netcfg_ble";

extern void HAL_PMU_Chip_Software_Reset_Enable(void);
void netcfg_bles_send_connect_status_dummy(uint32_t milli_seconds);

uint16_t netcfg_ble_notify_wifi(struct netcfg_ble_data *data);
uint8_t app_ble_netcfg_bles_send_notify(uint8_t conidx, uint16_t op, uint16_t status, uint16_t len, uint8_t *data);
void app_ble_adv_stop(uint8_t reason);

typedef struct {
    uint8_t conidx;
    uint16_t op;
    struct netcfg_ble_data *p_value;
} ble_notify_param_t;

static void ble_notify_thread(void *param)
{
    ble_notify_param_t *notify_param = (ble_notify_param_t *)param;
    uint16_t status = NETCFG_BLE_ERR;
    uint8_t notify_ret = 0;
    
    if (notify_param != NULL) {
        status = netcfg_ble_notify_wifi(notify_param->p_value);
        notify_ret = app_ble_netcfg_bles_send_notify(notify_param->conidx, notify_param->op, status, 0, NULL);
        LOGI("netcfg_bles_profile_set_cb notify status:0x%04X, ret:%d", status, notify_ret);
        
        lisa_mem_free(notify_param);
    }
    
    vTaskDelete(NULL);
}


static int netcfg_ble_wifi_connect(const int8_t *ssid, const int8_t *pwd)
{
    int result = -1;
    if (!ssid || !pwd) {
        return result;
    }

#if CONFIG_WIFI_MANAGER
    wifi_mgr_sta_config_t sta_config = { 0 };
    strcpy(sta_config.ssid, (const char *)ssid);
    strcpy(sta_config.pwd, (const char *)pwd);
    LOGI("netcfg_ble_wifi_connect wifi_mgr_sta_connect ssid:%s, pwd:%s", ssid, pwd);
    result = wifi_mgr_sta_connect(&sta_config, false);
#else
    wifi_connect_cfg_t sta_config = {
        .dhcp_mode = DHCP_CLIENT,
    };

    strcpy(sta_config.ssid, (const char *)ssid);
    strcpy(sta_config.key, (const char *)pwd);
    result = wifi_sta_connect(&sta_config);
#endif
    LISA_LOGI(TAG, "netcfg_ble_wifi_connect ssid:%s, pwd:%s", ssid, pwd);
    LISA_LOGI(TAG, "netcfg_ble_wifi_connect result:%d", result);
    return result;
}

uint16_t netcfg_ble_notify_wifi(struct netcfg_ble_data *data)
{
    int status = 0;

#if NETCFG_BLE_DBG
    NETCFG_BLE_LOGD("[%s]: Get ssid = %s, pwd = %s\n", __func__, data->ssid, data->pwd);
#endif
    LISA_LOGI(TAG, "Get ssid = %s, pwd = %s\n", data->ssid, data->pwd);

    /*
     * Fix me: Instead of directly call wifi connect, we need to create a task handling
     * the messages and wifi/ble status exchange here.
     * Will implement this later.
     */
    status = netcfg_ble_wifi_connect((const int8_t *)data->ssid, (const int8_t *)data->pwd);
    if (status != 0) {
        return NETCFG_BLE_ERR;
    }

    return NETCFG_BLE_SUCCESS;
}

/**
 * @brief Get product ID from KV storage or default macro
 * 
 * @param product_id Buffer to store product ID
 * @param max_len Maximum buffer length
 * @return int 0 on success, -1 on failure
 */
static int get_current_product_id(char *product_id, int max_len)
{
    if (!product_id || max_len <= 0) {
        return -1;
    }

    char *pid = NULL;
    int ret = lisa_kv_get_string(KV_KEY_USER_PID, &pid);
    
    if (ret != 0 || pid == NULL) {
        if (strlen(PRODUCT_ID) >= max_len) {
            return -1;
        }
        strcpy(product_id, PRODUCT_ID);
        return 0;
    } else {
        if (strlen(pid) >= max_len) {
            lisa_kv_free(pid);
            return -1;
        }
        strcpy(product_id, pid);
        lisa_kv_free(pid);
        return 0;
    }
}

/**
 * @brief Get device ID from KV storage or chip hardware ID
 * 
 * @param device_id Buffer to store device ID  
 * @param max_len Maximum buffer length
 * @return int 0 on success, -1 on failure
 */
static int get_current_device_id(char *device_id, int max_len)
{
    if (!device_id || max_len <= 0) {
        return -1;
    }
    char *kv_device_id = NULL;
    int ret = lisa_kv_get_string(KV_KEY_USER_DEVICE_ID, &kv_device_id);
    
    if (ret == 0 && kv_device_id != NULL && strlen(kv_device_id) > 0) {
        if (strlen(kv_device_id) >= max_len) {
            lisa_kv_free(kv_device_id);
            return -1;
        }
        strcpy(device_id, kv_device_id);
        lisa_kv_free(kv_device_id);
        return 0;
    } else {
        // KV中没有设备ID，从芯片读取硬件ID
        uint32_t *id_1 = (uint32_t *)0x48600208;
        uint32_t *id_2 = (uint32_t *)0x4860020c;
        uint8_t id_buffer[8];
        
        if (*id_1 == 0 && *id_2 == 0) {
            // 芯片ID也是全零，返回错误
            return -1;
        }
        
        if (max_len < 17) { // 16个字符 + 结束符
            return -1;
        }
        
        memcpy(id_buffer, id_1, sizeof(uint32_t));
        memcpy(id_buffer + 4, id_2, sizeof(uint32_t));
        
        snprintf(device_id, max_len, "%02x%02x%02x%02x%02x%02x%02x%02x", 
                id_buffer[0], id_buffer[1], id_buffer[2], id_buffer[3],
                id_buffer[4], id_buffer[5], id_buffer[6], id_buffer[7]);
                
        return 0;
    }
}

uint16_t netcfg_bles_profile_set_cb(uint8_t conidx, uint8_t att_idx, uint16_t op, uint8_t *p_value)
{
    uint16_t status = NETCFG_BLE_ERR;
    LISA_LOGI(TAG, "netcfg_bles_profile_set_cb op: 0x%04X", op);

    switch (op) {
    case NETCFG_BLE_OP_SSID: {
    } break;
    case NETCFG_BLE_OP_PWD: {
    } break;
    case NETCFG_BLE_OP_DONE: {
        ble_notify_param_t *notify_param = lisa_mem_alloc(sizeof(ble_notify_param_t));
        if (notify_param != NULL) {
            notify_param->conidx = conidx;
            notify_param->op = op;
            notify_param->p_value = (struct netcfg_ble_data *)p_value;
            
            lisa_thread_attr_t thread_attr;
            thread_attr.name = (uint8_t *)"ble_notify_thread";
            thread_attr.stack_size = 4 * 1024;
            thread_attr.priority = LISA_OS_PRIORITY_ABOVE_NORMAL;
            
            lisa_thread_create(&thread_attr, ble_notify_thread, notify_param);
            status = NETCFG_BLE_SUCCESS;
        } else {
            LISA_LOGE(TAG, "Failed to allocate memory for ble notify param");
            status = NETCFG_BLE_ERR;
        }
    } break;
    case NETCFG_BLE_OP_REBOOT: {
        ble_gap_disconnect(conidx, 0x13);
    } break;
    case NETCFG_BLE_AUTH_INFO:{
        // 0xA012
        LISA_LOGI(TAG, "netcfg_bles_profile_set_cb receive auth info request");
        char product_id[64] = {0};
        char device_id[32] = {0};
        
        if (get_current_product_id(product_id, sizeof(product_id)) != 0) {
            LISA_LOGW(TAG, "Failed to get product ID, using default");
            strcpy(product_id, "00000000-0000-0000-0000-000000000000");
        }
        
        if (get_current_device_id(device_id, sizeof(device_id)) != 0) {
            LISA_LOGW(TAG, "Failed to get device ID, using default");
            strcpy(device_id, "0000000000000000");
        }
        
        char auth_info[256] = {0};
        snprintf(auth_info, sizeof(auth_info),
                 "{\"code\":0,\"product_id\":\"%s\",\"device_id\":\"%s\"}", 
                 product_id, device_id);
        
        LISA_LOGI(TAG, "auth info: %s", auth_info);
        ble_netcfg_bles_send_notify_custom_data(conidx, strlen(auth_info), (uint8_t *)auth_info);
        LISA_LOGI(TAG, "netcfg_bles_profile_set_cb TEST done status: 0x%04X", status);
        assist_controller_trigger_event(CONTROLLER_EVENT_OPT_EXIT_BLE_CONFIG, NULL, 0);
        app_ble_adv_stop(0);
        status = NETCFG_BLE_SUCCESS;
        break;
    }
    default:
        break;
    }
    return status;
}


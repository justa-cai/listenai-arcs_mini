/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "lisa_bluetooth.h"
#include "lisa_ble_api.h"
#include "ble_adv_data.h"
#include "bt_stack_cfg.h"
#include "lisa_kv.h"
#include "kv_user.h"
#include "netcfg_ble.h"
#include "netcfg_bles.h"
#include "ble_gap.h"
#include "wifi_manager/wifi_manager.h"
#include "voice_msg.h"

#if defined(CONFIG_CLOUD_PRODUCT_ID_DEFAULT)
#define PRODUCT_ID CONFIG_CLOUD_PRODUCT_ID_DEFAULT
#else
#define PRODUCT_ID "cf75e7a9-66b6-41e3-918a-141800aebb5b"
#endif

#define TAG "app_ble"
#include "lisa_log.h"

/*
 * BLE ADV data
 */

static const uint8_t user_adv_data[] = {
    BLE_AD_FLAGS(GAP_AD_TYPE_FLAGS_GENERAL | GAP_AD_TYPE_FLAGS_BREDR_NOT_SUPPORTED),
    BLE_AD_COMPLETE_NAME(4, 'A', 'R', 'C', 'S'),  // Device name: ARCS
    BLE_AD_MFG_DATA(18,
        0xab, 0x0a, 0xa1, 0xdc, 0xa8, 0x76, 0x83, 0x65,
        0x73, 0x83, 0x72, 0x65, 0x82, 0x67, 0x83, 0x68,
        0x00, 0x78),
};

const uint8_t *lisa_bt_get_adv_data(uint8_t *len)
{
    *len = sizeof(user_adv_data);
    return user_adv_data;
}

/*
 * BLE netcfg - replaces SDK's app_net_cfg.c with AUTH_INFO support
 */

static lisa_ble_netcfg_handler_t s_netcfg_handler;

void lisa_ble_netcfg_set_handler(lisa_ble_netcfg_handler_t handler)
{
    s_netcfg_handler = handler;
}

void netcfg_bles_con_cleanup(uint8_t conidx, uint16_t reason)
{
    netcfg_bles_set_state(NETCFG_BLE_IDLE);
}

static int netcfg_ble_wifi_connect(const int8_t *ssid, const int8_t *pwd)
{
    if (!ssid || !pwd)
        return -1;

    if (!s_netcfg_handler)
        return -1;

    return s_netcfg_handler((const char *)ssid, (const char *)pwd);
}

uint16_t netcfg_ble_notify_wifi(struct netcfg_ble_data *data)
{
    int status;

    LISA_LOGI(TAG, "Get ssid = %s, pwd = %s", data->ssid, data->pwd);

    status = netcfg_ble_wifi_connect((const int8_t *)data->ssid, (const int8_t *)data->pwd);
    if (status != 0)
        return NETCFG_BLE_ERR;

    return NETCFG_BLE_SUCCESS;
}

static int get_current_product_id(char *product_id, int max_len)
{
    if (!product_id || max_len <= 0)
        return -1;

    char *pid = NULL;
    int ret = lisa_kv_get_string(KV_KEY_USER_PID, &pid);

    if (ret != 0 || pid == NULL) {
        if (strlen(PRODUCT_ID) >= max_len)
            return -1;
        strcpy(product_id, PRODUCT_ID);
        return 0;
    }

    if (strlen(pid) >= max_len) {
        lisa_kv_free(pid);
        return -1;
    }
    strcpy(product_id, pid);
    lisa_kv_free(pid);
    return 0;
}

int get_current_device_id(char *device_id, int max_len)
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
        uint32_t *id_1 = (uint32_t *)0x48600208;
        uint32_t *id_2 = (uint32_t *)0x4860020c;
        uint8_t id_buffer[8];

        if (*id_1 == 0 && *id_2 == 0) {
            return -1;
        }

        if (max_len < 17) {
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
    case NETCFG_BLE_OP_SSID:
    case NETCFG_BLE_OP_PWD:
        break;
    case NETCFG_BLE_OP_DONE:
        status = netcfg_ble_notify_wifi((struct netcfg_ble_data *)p_value);
        lisa_ble_netcfg_send_notify(conidx, 0, 0, 0, NULL);
        break;
    case NETCFG_BLE_OP_REBOOT:
        ble_gap_disconnect(conidx, 0x13);
        break;
    case NETCFG_BLE_AUTH_INFO: {
        LISA_LOGI(TAG, "receive auth info request");
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
        lisa_ble_adv_stop(0);
        status = NETCFG_BLE_SUCCESS;
        break;
    }
    default:
        break;
    }
    return status;
}

/*
 * BLE netcfg init - register WiFi connect handler
 */

static int netcfg_wifi_connect_handler(const char *ssid, const char *pwd)
{
    wifi_mgr_sta_config_t sta_config = { 0 };
    strncpy(sta_config.ssid, ssid, sizeof(sta_config.ssid) - 1);
    strncpy(sta_config.pwd, pwd, sizeof(sta_config.pwd) - 1);
    LISA_LOGI(TAG, "netcfg wifi connect ssid:%s", ssid);
    int result = wifi_mgr_sta_connect(&sta_config, false);
    LISA_LOGI(TAG, "netcfg wifi connect result:%d", result);
    if (result == 0) {
        voice_msg_pub(VOICE_MSG_BLE_CONNECT_DONE, NULL, 0);
    }
    return result;
}

void app_ble_netcfg_init(void)
{
    lisa_ble_netcfg_set_handler(netcfg_wifi_connect_handler);
}

/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "FreeRTOS.h"
#include "task.h"
#include "lisa_bluetooth.h"
#include "lisa_ble_api.h"
#include "ble_adv_data.h"
#include "bt_stack_cfg.h"
#include "bt_app_hal.h"
#include "lisa_kv.h"
#include "kv_user.h"
#include "netcfg_ble.h"
#include "netcfg_bles.h"
#include "ble_gap.h"
#include "ble_plf_config.h"
#include "voice_msg.h"
#include "sys_network_manager.h"
#include "sys_wifi.h"

#if CONFIG_LISA_MODEM
#include "lisa_modem_module.h"
#endif

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

static uint8_t user_adv_data[] = {
    BLE_AD_FLAGS(GAP_AD_TYPE_FLAGS_GENERAL | GAP_AD_TYPE_FLAGS_BREDR_NOT_SUPPORTED),
    BLE_AD_COMPLETE_NAME(4, 'A', 'R', 'C', 'S'),  // Device name: ARCS
    BLE_AD_MFG_DATA(19,
        0xab, 0x0a, 0xa1, 0xdc, 0xa8, 0x76, 0x83, 0x65,
        0x73, 0x83, 0x72, 0x65, 0x82, 0x67, 0x83, 0x68,
        0x00, 0x78, 0x00),
};

const uint8_t *lisa_bt_get_adv_data(uint8_t *len)
{
#if CONFIG_LISA_MODEM
    user_adv_data[sizeof(user_adv_data) - 1] = lisa_modem_is_present() ? 0x01 : 0x00;
#endif
    *len = sizeof(user_adv_data);
    return user_adv_data;
}

/*
 * BLE netcfg - replaces SDK's app_net_cfg.c with AUTH_INFO support
 */

static lisa_ble_netcfg_handler_t s_netcfg_handler;
static bool s_ble_connected;
static uint8_t s_ble_conidx;
static volatile bool s_ble_netcfg_adv_pending;
static void app_ble_send_wifi_provision_fail(uint8_t conidx, uint8_t reason);

#define APP_BLE_NETCFG_ADV_DELAY_MS 1000U

void lisa_ble_netcfg_set_handler(lisa_ble_netcfg_handler_t handler)
{
    s_netcfg_handler = handler;
}

static void app_ble_on_connected(uint8_t conidx, uint16_t conhdl, const gap_bdaddr_t *peer_addr)
{
    (void)conhdl;
    (void)peer_addr;

    s_ble_connected = true;
    s_ble_conidx = conidx;
    LISA_LOGI(TAG, "BLE connected, conidx=%u", conidx);
}

void netcfg_bles_con_cleanup(uint8_t conidx, uint16_t reason)
{
    LISA_LOGI(TAG, "BLE disconnected, conidx=%u, reason=0x%04X", conidx, reason);
    if (s_ble_connected && s_ble_conidx == conidx) {
        s_ble_connected = false;
    }
    netcfg_bles_set_state(NETCFG_BLE_IDLE);
}

static void app_ble_on_disconnected(uint8_t conidx, uint16_t conhdl, uint16_t reason)
{
    (void)conhdl;
    netcfg_bles_con_cleanup(conidx, reason);
}

static void app_ble_on_bond(uint8_t conidx, uint8_t info, uint8_t value)
{
    LISA_LOGI(TAG, "BLE bond event, conidx=%u, info=%u, value=%u", conidx, info, value);
}

void app_ble_netcfg_prepare(void)
{
    lisa_ble_adv_stop(0);
    if (s_ble_connected) {
        LISA_LOGI(TAG, "Disconnect current BLE link before netcfg, conidx=%u", s_ble_conidx);
        lisa_ble_disconnect(s_ble_conidx, 0x13);
    }

    LISA_LOGI(TAG, "Clear BLE bond info before netcfg");
    ble_gap_delete_bond(NULL);
    bt_stack_nvs_del(NVS_ID_PEER_ADDRESS);
    netcfg_bles_set_state(NETCFG_BLE_IDLE);
}

static void app_ble_netcfg_adv_start_task(void *arg)
{
    uint8_t ret = 0;

    (void)arg;

    app_ble_netcfg_prepare();
    vTaskDelay(pdMS_TO_TICKS(APP_BLE_NETCFG_ADV_DELAY_MS));

    ret = app_ble_adv_start(0, BLE_ADV_GEN);
    if (ret != pdTRUE) {
        LISA_LOGW(TAG, "Delayed BLE adv start failed: %u", (unsigned)ret);
    }

    s_ble_netcfg_adv_pending = false;
    vTaskDelete(NULL);
}

void app_ble_netcfg_adv_start_delayed(void)
{
    if (s_ble_netcfg_adv_pending) {
        return;
    }

    s_ble_netcfg_adv_pending = true;
    if (xTaskCreate(app_ble_netcfg_adv_start_task, "ble_adv_delay", 2048, NULL, 5, NULL) != pdPASS) {
        s_ble_netcfg_adv_pending = false;
        LISA_LOGW(TAG, "Failed to create delayed BLE adv task");
    }
}

static int netcfg_ble_wifi_connect(const int8_t *ssid, const int8_t *pwd)
{
    if (!ssid || !pwd)
        return -1;

    if (!s_netcfg_handler)
        return -1;

    return s_netcfg_handler((const char *)ssid, (const char *)pwd);
}

static bool netcfg_ble_data_is_empty(const struct netcfg_ble_data *data)
{
    return data && data->ssid[0] == '\0' && data->pwd[0] == '\0';
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
    case NETCFG_BLE_OP_SKIP_WIFI:
        LISA_LOGI(TAG, "Skip Wi-Fi connect by BLE netcfg command");
        lisa_ble_netcfg_send_notify(conidx, 0, 0, 0, NULL);
        status = NETCFG_BLE_SUCCESS;
        break;
    case NETCFG_BLE_OP_DONE:
        if (netcfg_ble_data_is_empty((struct netcfg_ble_data *)p_value)) {
            LISA_LOGI(TAG, "Skip Wi-Fi connect for empty BLE netcfg data");
            lisa_ble_netcfg_send_notify(conidx, 0, 0, 0, NULL);
            status = NETCFG_BLE_SUCCESS;
            break;
        }
        status = netcfg_ble_notify_wifi((struct netcfg_ble_data *)p_value);
        lisa_ble_netcfg_send_notify(conidx, 0, 0, 0, NULL);
        if (status != NETCFG_BLE_SUCCESS) {
            sys_wifi_connect_result_t result = sys_wifi_get_connect_result();
            app_ble_send_wifi_provision_fail(conidx, (uint8_t)result);
        }
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
        voice_msg_pub(VOICE_MSG_BLE_AUTH_INFO_DONE, NULL, 0);
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
    LISA_LOGI(TAG, "netcfg wifi apply ssid:%s", ssid);

    int result = sys_network_connect_wifi(ssid, pwd, NULL);

    LISA_LOGI(TAG, "netcfg wifi apply result:%d", result);
    if (result == 0) {
        voice_msg_pub(VOICE_MSG_BLE_CONNECT_DONE, NULL, 0);
    }
    return result;
}

static void app_ble_send_wifi_provision_fail(uint8_t conidx, uint8_t reason)
{
    uint8_t payload[3] = {0x0A, 0x01, reason};

    LISA_LOGI(TAG, "BLE netcfg fail result: 0x%02X 0x%02X 0x%02X",
              payload[0], payload[1], payload[2]);
    ble_netcfg_bles_send_notify_custom_data(conidx, sizeof(payload), payload);
    lisa_ble_adv_start(0, LISA_BLE_ADV_GEN);
}

void app_ble_netcfg_init(void)
{
    s_ble_connected = false;
    s_ble_conidx = 0;
    lisa_ble_register_conn_cb(app_ble_on_connected);
    lisa_ble_register_disc_cb(app_ble_on_disconnected);
    lisa_ble_register_bond_cb(app_ble_on_bond);
    lisa_ble_netcfg_set_handler(netcfg_wifi_connect_handler);
}

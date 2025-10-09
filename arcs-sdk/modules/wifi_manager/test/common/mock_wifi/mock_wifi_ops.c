/**
 * @file mock_wifi_ops.c
 * @brief WiFi管理器测试中使用的WiFi操作实现
 */
#include "mock_wifi_ops.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

DEFINE_FAKE_VALUE_FUNC(int, mock_wifi_init);
DEFINE_FAKE_VALUE_FUNC(int, mock_wifi_deinit);
DEFINE_FAKE_VALUE_FUNC(int, mock_wifi_sta_enable);
DEFINE_FAKE_VALUE_FUNC(int, mock_wifi_sta_disable);
DEFINE_FAKE_VALUE_FUNC(bool, mock_wifi_sta_is_enable);
DEFINE_FAKE_VALUE_FUNC(int, mock_wifi_add_callback, wifi_mgr_wifi_event_cb_t *);
DEFINE_FAKE_VALUE_FUNC(int, mock_wifi_remove_callback, wifi_mgr_wifi_event_cb_t *);
DEFINE_FAKE_VALUE_FUNC(int, mock_wifi_scan_ap, wifi_mgr_wifi_scan_info_t *, uint32_t, uint32_t);
DEFINE_FAKE_VALUE_FUNC(int, mock_wifi_sta_connect, wifi_mgr_wifi_sta_config_t *, uint32_t);
DEFINE_FAKE_VALUE_FUNC(int, mock_wifi_sta_disconnect, uint32_t);
DEFINE_FAKE_VALUE_FUNC(wifi_mgr_wifi_status_t, mock_wifi_sta_get_status);
DEFINE_FAKE_VALUE_FUNC(int, mock_wifi_get_mac, uint8_t *);

static wifi_manager_wifi_ops_t wifi_ops = {
    .init = mock_wifi_init,
    .deinit = mock_wifi_deinit,
    .sta_enable = mock_wifi_sta_enable,
    .sta_disable = mock_wifi_sta_disable,
    .sta_is_enable = mock_wifi_sta_is_enable,
    .add_callback = mock_wifi_add_callback,
    .remove_callback = mock_wifi_remove_callback,
    .scan_ap = mock_wifi_scan_ap,
    .sta_connect = mock_wifi_sta_connect,
    .sta_disconnect = mock_wifi_sta_disconnect,
    .sta_get_status = mock_wifi_sta_get_status,
    .get_mac = mock_wifi_get_mac,
};

static bool mock_wifi_is_enable = false;


static int custom_mock_wifi_sta_enable(void)
{
    mock_wifi_is_enable = true;
    return 0;
}

static int custom_mock_wifi_sta_disable(void)
{
    mock_wifi_is_enable = false;
    return 0;
}

static bool custom_mock_wifi_sta_is_enable(void)
{
    return mock_wifi_is_enable;
}

void mock_wifi_ops_reset(void)
{
    mock_wifi_ops_init();
}

void mock_wifi_ops_init(void)
{
    RESET_FAKE(mock_wifi_init);
    RESET_FAKE(mock_wifi_deinit);
    RESET_FAKE(mock_wifi_sta_enable);
    RESET_FAKE(mock_wifi_sta_disable);
    RESET_FAKE(mock_wifi_sta_is_enable);
    RESET_FAKE(mock_wifi_add_callback);
    RESET_FAKE(mock_wifi_remove_callback);
    RESET_FAKE(mock_wifi_scan_ap);
    RESET_FAKE(mock_wifi_sta_connect);
    RESET_FAKE(mock_wifi_sta_disconnect);
    RESET_FAKE(mock_wifi_sta_get_status);
    RESET_FAKE(mock_wifi_get_mac);

    mock_wifi_sta_enable_fake.custom_fake = custom_mock_wifi_sta_enable;
    mock_wifi_sta_disable_fake.custom_fake = custom_mock_wifi_sta_disable;
    mock_wifi_sta_is_enable_fake.custom_fake = custom_mock_wifi_sta_is_enable;
}

wifi_manager_wifi_ops_t* mock_wifi_ops_get(void)
{
    return &wifi_ops;
} 
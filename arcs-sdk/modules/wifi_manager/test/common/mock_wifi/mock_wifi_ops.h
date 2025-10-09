/**
 * @file mock_wifi_ops.h
 * @brief WiFi管理器测试中使用的WiFi操作接口
 */
#ifndef MOCK_WIFI_OPS_H
#define MOCK_WIFI_OPS_H

#include <stdint.h>
#include <stdbool.h>
#include "wifi_manager/wifi_manager_wifi_ops.h"
#include "fff.h"

DECLARE_FAKE_VALUE_FUNC(int, mock_wifi_init);
DECLARE_FAKE_VALUE_FUNC(int, mock_wifi_deinit);
DECLARE_FAKE_VALUE_FUNC(int, mock_wifi_sta_enable);
DECLARE_FAKE_VALUE_FUNC(int, mock_wifi_sta_disable);
DECLARE_FAKE_VALUE_FUNC(bool, mock_wifi_sta_is_enable);
DECLARE_FAKE_VALUE_FUNC(int, mock_wifi_add_callback, wifi_mgr_wifi_event_cb_t *);
DECLARE_FAKE_VALUE_FUNC(int, mock_wifi_remove_callback, wifi_mgr_wifi_event_cb_t *);
DECLARE_FAKE_VALUE_FUNC(int, mock_wifi_scan_ap, wifi_mgr_wifi_scan_info_t *, uint32_t, uint32_t);
DECLARE_FAKE_VALUE_FUNC(int, mock_wifi_sta_connect, wifi_mgr_wifi_sta_config_t *, uint32_t);
DECLARE_FAKE_VALUE_FUNC(int, mock_wifi_sta_disconnect, uint32_t);
DECLARE_FAKE_VALUE_FUNC(wifi_mgr_wifi_status_t, mock_wifi_sta_get_status);
DECLARE_FAKE_VALUE_FUNC(int, mock_wifi_get_mac, uint8_t *);

void mock_wifi_ops_init(void);
void mock_wifi_ops_reset(void);
wifi_manager_wifi_ops_t* mock_wifi_ops_get(void);

#endif /* MOCK_WIFI_OPS_H */ 
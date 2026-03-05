/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "unity.h"

#include <stdbool.h>
#include <string.h>

#include "fff.h"
#include "mock_ls_event.h"
#include "mock_mem_ops.h"
#include "mock_os_ops.h"
#include "mock_wifi_api.h"
#include "mock_wifi_manager_storage.h"
#include "mock_esp_heap_caps.h"
#include "wifi_manager/wifi_manager.h"
#include "wifi_arcs.h"

DEFINE_FFF_GLOBALS;

static bool s_cb_called = false;
static wifi_mgr_sta_config_t s_last_sta_info;
static wifi_mgr_connection_status_t s_last_status = WIFI_MGR_STA_MAX;
static int s_last_reason = 0;
static bool s_event_cb_called = false;
static wifi_mgr_wifi_event_t s_last_event = 0;
static wifi_mgr_connect_fail_info_t s_last_fail_info;
static bool s_last_fail_info_valid = false;

static void test_connection_cb(wifi_mgr_connection_info_t *info, void *arg)
{
    (void)arg;
    s_cb_called = true;
    s_last_status = info->status;
    s_last_reason = info->reason;
    if (info->sta_info != NULL) {
        memcpy(&s_last_sta_info, info->sta_info, sizeof(s_last_sta_info));
    } else {
        memset(&s_last_sta_info, 0, sizeof(s_last_sta_info));
    }
}

static void test_wifi_event_cb(wifi_mgr_wifi_event_t event, void *event_data, uint32_t event_data_len, void *arg)
{
    (void)event_data;
    (void)arg;
    s_event_cb_called = true;
    s_last_event = event;
    s_last_fail_info_valid = false;
    if (event_data == NULL) {
        return;
    }
    if (event_data_len >= sizeof(wifi_mgr_disconnect_event_info_t)) {
        wifi_mgr_disconnect_event_info_t *disc_info = event_data;
        s_last_fail_info = disc_info->fail_info;
        s_last_fail_info_valid = true;
    } else if (event_data_len >= sizeof(wifi_mgr_connect_fail_info_t)) {
        memcpy(&s_last_fail_info, event_data, sizeof(s_last_fail_info));
        s_last_fail_info_valid = true;
    }
}

static void *test_thread_create(wifi_manager_os_thread_attr_t *attr,
                                wifi_manager_os_thread_entry_t entry, void *arg)
{
    (void)attr;
    (void)entry;
    (void)arg;
    return (void *)0x1;
}

static void test_thread_delete(void *thread)
{
    (void)thread;
}

static void setup_wifi_manager(void)
{
    wifi_mgr_ops_t ops = {
        .mem_ops = mock_mem_ops_get(),
        .os_ops = mock_os_ops_get(),
        .wifi_ops = wifi_manager_arcs_wifi_ops_get(),
    };

    mock_mem_ops_init();
    mock_os_ops_init();
    mock_wifi_storage_init();
    mock_esp_heap_caps_init();
    mock_ls_event_init();
    mock_wifi_api_init();
    mock_ls_event_reset();
    mock_wifi_api_reset();

    wifi_storage_init_fake.return_val = 0;

    mock_thread_create_fake.custom_fake = test_thread_create;
    mock_thread_delete_fake.custom_fake = test_thread_delete;

    TEST_ASSERT_EQUAL(0, wifi_mgr_init(&ops));
    TEST_ASSERT_EQUAL(0, wifi_mgr_sta_add_connection_cb(test_connection_cb, NULL));
}

static void teardown_wifi_manager(void)
{
    wifi_mgr_deinit();
    s_cb_called = false;
    memset(&s_last_sta_info, 0, sizeof(s_last_sta_info));
    s_last_status = WIFI_MGR_STA_MAX;
    s_last_reason = 0;
    s_event_cb_called = false;
    s_last_event = 0;
    memset(&s_last_fail_info, 0, sizeof(s_last_fail_info));
    s_last_fail_info_valid = false;
}

void setUp(void)
{
    setup_wifi_manager();
}

void tearDown(void)
{
    teardown_wifi_manager();
}

void test_wifi_connected_event_populates_sta_info(void)
{
    uint8_t pmk[32];
    for (size_t i = 0; i < sizeof(pmk); i++) {
        pmk[i] = (uint8_t)(i + 1);
    }

    wifi_link_status_t link_status = {
        .bssid = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66},
        .channel = 7,
        .state = STA_CONNECTED,
    };

    mock_wifi_api_set_pmk(pmk, true);
    mock_wifi_api_set_link_status(&link_status);

    mock_ls_event_trigger(EVENT_WIFI, EVENT_WIFI_CONNECTED, NULL);

    TEST_ASSERT_TRUE(s_cb_called);
    TEST_ASSERT_EQUAL(WIFI_MGR_STA_CONNECTED, s_last_status);
    TEST_ASSERT_TRUE(s_last_sta_info.pmk_valid);
    TEST_ASSERT_EQUAL_MEMORY(pmk, s_last_sta_info.pmk, sizeof(pmk));
    TEST_ASSERT_EQUAL(7, s_last_sta_info.channel);
    TEST_ASSERT_EQUAL_STRING("11:22:33:44:55:66", s_last_sta_info.bssid);
}

void test_wifi_connected_event_keeps_requested_ssid_and_pwd(void)
{
    wifi_mgr_sta_config_t old_cfg = {0};
    strcpy(old_cfg.ssid, "ssid_b");
    strcpy(old_cfg.pwd, "pwd_b");
    strcpy(old_cfg.bssid, "bb:bb:bb:bb:bb:bb");
    old_cfg.encryption_mode = WIFI_MGR_WIFI_AUTH_WPA2_PSK;

    (void)wifi_mgr_sta_connect(&old_cfg, true);

    wifi_link_status_t link_status = {
        .bssid = {0xcc, 0xdd, 0xee, 0xff, 0x00, 0x11},
        .channel = 6,
        .state = STA_CONNECTED,
    };
    strcpy(link_status.ssid, "ssid_from_driver");

    mock_wifi_api_set_link_status(&link_status);

    s_cb_called = false;
    memset(&s_last_sta_info, 0, sizeof(s_last_sta_info));
    s_last_status = WIFI_MGR_STA_MAX;

    mock_ls_event_trigger(EVENT_WIFI, EVENT_WIFI_CONNECTED, NULL);

    TEST_ASSERT_TRUE(s_cb_called);
    TEST_ASSERT_EQUAL(WIFI_MGR_STA_CONNECTED, s_last_status);
    TEST_ASSERT_EQUAL_STRING("ssid_b", s_last_sta_info.ssid);
    TEST_ASSERT_EQUAL_STRING("pwd_b", s_last_sta_info.pwd);
    TEST_ASSERT_EQUAL_STRING("cc:dd:ee:ff:00:11", s_last_sta_info.bssid);
    TEST_ASSERT_EQUAL(6, s_last_sta_info.channel);
}

void test_wifi_connect_sets_bssid_and_freq(void)
{
    wifi_mgr_sta_config_t cfg = {0};
    strcpy(cfg.ssid, "TestSSID");
    strcpy(cfg.pwd, "TestPwd");
    strcpy(cfg.bssid, "11:22:33:44:55:66");
    cfg.channel = 6;
    cfg.encryption_mode = WIFI_MGR_WIFI_AUTH_WPA2_PSK;
    cfg.pmk_valid = 0;

    (void)wifi_mgr_sta_connect(&cfg, false);
    TEST_ASSERT_TRUE(mock_wifi_api_has_last_connect_cfg());

    const wifi_connect_cfg_t *last_cfg = mock_wifi_api_get_last_connect_cfg();
    TEST_ASSERT_NOT_NULL(last_cfg);
    TEST_ASSERT_EQUAL_HEX8(0x11, last_cfg->bssid[0]);
    TEST_ASSERT_EQUAL_HEX8(0x22, last_cfg->bssid[1]);
    TEST_ASSERT_EQUAL_HEX8(0x33, last_cfg->bssid[2]);
    TEST_ASSERT_EQUAL_HEX8(0x44, last_cfg->bssid[3]);
    TEST_ASSERT_EQUAL_HEX8(0x55, last_cfg->bssid[4]);
    TEST_ASSERT_EQUAL_HEX8(0x66, last_cfg->bssid[5]);
    TEST_ASSERT_EQUAL(2437, last_cfg->freq[0]);
    TEST_ASSERT_EQUAL(2437, last_cfg->freq[1]);
}

void test_wifi_disconnect_before_connected_reports_connect_fail(void)
{
    wifi_mgr_wifi_event_cb_t cb = {
        .handler = test_wifi_event_cb,
        .events = WIFI_MGR_WIFI_EVT_STA_CONNECTED |
                  WIFI_MGR_WIFI_EVT_STA_DISCONNECTED |
                  WIFI_MGR_WIFI_EVT_STA_CONNECTION_FAILED,
        .arg = NULL,
    };

    wifi_manager_arcs_wifi_ops_get()->add_callback(&cb);

    s_event_cb_called = false;
    s_last_event = 0;
    mock_ls_event_trigger(EVENT_WIFI, EVENT_WIFI_DISCONNECT, NULL);

    TEST_ASSERT_TRUE(s_event_cb_called);
    TEST_ASSERT_EQUAL(WIFI_MGR_WIFI_EVT_STA_CONNECTION_FAILED, s_last_event);

    wifi_manager_arcs_wifi_ops_get()->remove_callback(&cb);
}

void test_wifi_disconnect_after_connected_reports_disconnected(void)
{
    wifi_mgr_wifi_event_cb_t cb = {
        .handler = test_wifi_event_cb,
        .events = WIFI_MGR_WIFI_EVT_STA_CONNECTED |
                  WIFI_MGR_WIFI_EVT_STA_DISCONNECTED |
                  WIFI_MGR_WIFI_EVT_STA_CONNECTION_FAILED,
        .arg = NULL,
    };

    wifi_manager_arcs_wifi_ops_get()->add_callback(&cb);

    mock_ls_event_trigger(EVENT_WIFI, EVENT_WIFI_CONNECTED, NULL);

    s_event_cb_called = false;
    s_last_event = 0;
    mock_ls_event_trigger(EVENT_WIFI, EVENT_WIFI_STA_CONNECT_FAIL, NULL);

    TEST_ASSERT_TRUE(s_event_cb_called);
    TEST_ASSERT_EQUAL(WIFI_MGR_WIFI_EVT_STA_DISCONNECTED, s_last_event);

    wifi_manager_arcs_wifi_ops_get()->remove_callback(&cb);
}

void test_wifi_disconnect_while_switching_ap_reports_previous_connection(void)
{
    s_cb_called = false;
    memset(&s_last_sta_info, 0, sizeof(s_last_sta_info));
    s_last_status = WIFI_MGR_STA_MAX;

    wifi_mgr_sta_config_t ap_a = {
        .ssid = "ssid_a",
        .pwd = "pwd_a",
        .bssid = "aa:aa:aa:aa:aa:aa",
        .channel = 1,
        .encryption_mode = WIFI_MGR_WIFI_AUTH_WPA2_PSK,
    };
    wifi_mgr_sta_config_t ap_b = {
        .ssid = "ssid_b",
        .pwd = "pwd_b",
        .bssid = "bb:bb:bb:bb:bb:bb",
        .channel = 6,
        .encryption_mode = WIFI_MGR_WIFI_AUTH_WPA2_PSK,
    };

    wifi_link_status_t link_status = {
        .bssid = {0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa},
        .channel = 1,
        .state = STA_CONNECTED,
    };

    // Connect to AP A and simulate successful connection event
    (void)wifi_mgr_sta_connect(&ap_a, true);
    mock_wifi_api_set_link_status(&link_status);
    mock_ls_event_trigger(EVENT_WIFI, EVENT_WIFI_CONNECTED, NULL);

    TEST_ASSERT_TRUE(s_cb_called);
    TEST_ASSERT_EQUAL_STRING("ssid_a", s_last_sta_info.ssid);

    // Reset callback state, start connecting to AP B
    s_cb_called = false;
    memset(&s_last_sta_info, 0, sizeof(s_last_sta_info));
    s_last_status = WIFI_MGR_STA_MAX;

    (void)wifi_mgr_sta_connect(&ap_b, true);

    // Firmware reports disconnect; should still report last connected AP A
    mock_ls_event_trigger(EVENT_WIFI, EVENT_WIFI_DISCONNECT, NULL);

    TEST_ASSERT_TRUE(s_cb_called);
    TEST_ASSERT_EQUAL(WIFI_MGR_STA_DISCONNECTED, s_last_status);
    TEST_ASSERT_EQUAL_STRING("ssid_a", s_last_sta_info.ssid);
}

void test_wifi_disconnect_event_propagates_fail_info(void)
{
    wifi_mgr_wifi_event_cb_t cb = {
        .handler = test_wifi_event_cb,
        .events = WIFI_MGR_WIFI_EVT_STA_CONNECTED |
                  WIFI_MGR_WIFI_EVT_STA_DISCONNECTED |
                  WIFI_MGR_WIFI_EVT_STA_CONNECTION_FAILED,
        .arg = NULL,
    };

    wifi_manager_arcs_wifi_ops_get()->add_callback(&cb);

    wifi_link_status_t link_status = {
        .bssid = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66},
        .channel = 11,
        .state = STA_CONNECTED,
    };
    event_connect_fail_param_t fail_evt = {
        .erro_code = 12,
        .status_code = 7,
        .reason_code = 203,
    };

    mock_wifi_api_set_link_status(&link_status);
    mock_ls_event_trigger(EVENT_WIFI, EVENT_WIFI_CONNECTED, NULL);

    s_cb_called = false;
    s_event_cb_called = false;
    s_last_status = WIFI_MGR_STA_MAX;
    s_last_reason = -1;
    s_last_fail_info_valid = false;
    memset(&s_last_fail_info, 0, sizeof(s_last_fail_info));

    mock_ls_event_trigger(EVENT_WIFI, EVENT_WIFI_DISCONNECT, &fail_evt);

    TEST_ASSERT_TRUE(s_cb_called);
    TEST_ASSERT_EQUAL(WIFI_MGR_STA_DISCONNECTED, s_last_status);
    TEST_ASSERT_EQUAL(fail_evt.reason_code, s_last_reason);

    TEST_ASSERT_TRUE(s_event_cb_called);
    TEST_ASSERT_EQUAL(WIFI_MGR_WIFI_EVT_STA_DISCONNECTED, s_last_event);
    TEST_ASSERT_TRUE(s_last_fail_info_valid);
    TEST_ASSERT_EQUAL(fail_evt.erro_code, s_last_fail_info.error_code);
    TEST_ASSERT_EQUAL(fail_evt.status_code, s_last_fail_info.status_code);
    TEST_ASSERT_EQUAL(fail_evt.reason_code, s_last_fail_info.reason_code);

    wifi_manager_arcs_wifi_ops_get()->remove_callback(&cb);
}

void test_wifi_connection_fail_propagates_fail_info(void)
{
    wifi_mgr_wifi_event_cb_t cb = {
        .handler = test_wifi_event_cb,
        .events = WIFI_MGR_WIFI_EVT_STA_CONNECTED |
                  WIFI_MGR_WIFI_EVT_STA_DISCONNECTED |
                  WIFI_MGR_WIFI_EVT_STA_CONNECTION_FAILED,
        .arg = NULL,
    };

    wifi_manager_arcs_wifi_ops_get()->add_callback(&cb);

    event_connect_fail_param_t fail_evt = {
        .erro_code = 5,
        .status_code = 9,
        .reason_code = 42,
    };

    s_cb_called = false;
    s_event_cb_called = false;
    s_last_status = WIFI_MGR_STA_MAX;
    s_last_reason = -1;
    s_last_fail_info_valid = false;
    memset(&s_last_fail_info, 0, sizeof(s_last_fail_info));

    mock_ls_event_trigger(EVENT_WIFI, EVENT_WIFI_STA_CONNECT_FAIL, &fail_evt);

    TEST_ASSERT_TRUE(s_cb_called);
    TEST_ASSERT_EQUAL(WIFI_MGR_STA_CONNECT_FAILED, s_last_status);
    TEST_ASSERT_EQUAL(fail_evt.reason_code, s_last_reason);

    TEST_ASSERT_TRUE(s_event_cb_called);
    TEST_ASSERT_EQUAL(WIFI_MGR_WIFI_EVT_STA_CONNECTION_FAILED, s_last_event);
    TEST_ASSERT_TRUE(s_last_fail_info_valid);
    TEST_ASSERT_EQUAL(fail_evt.erro_code, s_last_fail_info.error_code);
    TEST_ASSERT_EQUAL(fail_evt.status_code, s_last_fail_info.status_code);
    TEST_ASSERT_EQUAL(fail_evt.reason_code, s_last_fail_info.reason_code);

    wifi_manager_arcs_wifi_ops_get()->remove_callback(&cb);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_wifi_connected_event_populates_sta_info);
    RUN_TEST(test_wifi_connected_event_keeps_requested_ssid_and_pwd);
    RUN_TEST(test_wifi_connect_sets_bssid_and_freq);
    RUN_TEST(test_wifi_disconnect_before_connected_reports_connect_fail);
    RUN_TEST(test_wifi_disconnect_after_connected_reports_disconnected);
    RUN_TEST(test_wifi_disconnect_while_switching_ap_reports_previous_connection);
    RUN_TEST(test_wifi_disconnect_event_propagates_fail_info);
    RUN_TEST(test_wifi_connection_fail_propagates_fail_info);
    return UNITY_END();
}

/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mock_wifi_api.h"

#include <string.h>

static uint8_t s_pmk[32];
static bool s_pmk_valid = false;
static wifi_link_status_t s_link_status = {0};
static bool s_link_status_valid = false;
static wifi_connect_cfg_t s_last_connect_cfg;
static bool s_has_last_connect_cfg = false;

DEFINE_FAKE_VALUE_FUNC(ls_err_t, wifi_get_pmk, uint8_t *);
DEFINE_FAKE_VALUE_FUNC(ls_err_t, wifi_set_pmk, const uint8_t *);
DEFINE_FAKE_VALUE_FUNC(ls_err_t, wifi_get_link_status, wifi_link_status_t *);
DEFINE_FAKE_VALUE_FUNC(ls_err_t, wifi_sta_connect, wifi_connect_cfg_t *);
DEFINE_FAKE_VALUE_FUNC0(ls_err_t, wifi_sta_disconnect);
DEFINE_FAKE_VALUE_FUNC0(ls_err_t, wifi_sta_mode_enable);
DEFINE_FAKE_VALUE_FUNC0(ls_err_t, wifi_sta_mode_disable);
DEFINE_FAKE_VALUE_FUNC(ls_err_t, wifi_get_mode, int, wifi_mode_e *);
DEFINE_FAKE_VALUE_FUNC(ls_err_t, wifi_scan_start, wifi_scan_params_t *);
DEFINE_FAKE_VALUE_FUNC(ls_err_t, wifi_sta_scanlist_dump, wifi_scan_result_t *, int, int *);
DEFINE_FAKE_VALUE_FUNC(ls_err_t, wifi_get_sta_mac, uint8_t *);

static ls_err_t custom_wifi_get_pmk(uint8_t *pmk)
{
    if (!s_pmk_valid || pmk == NULL) {
        return -1;
    }
    memcpy(pmk, s_pmk, sizeof(s_pmk));
    return LS_OK;
}

static ls_err_t custom_wifi_set_pmk(const uint8_t *pmk)
{
    (void)pmk;
    return LS_OK;
}

static ls_err_t custom_wifi_get_link_status(wifi_link_status_t *link_status)
{
    if (!s_link_status_valid || link_status == NULL) {
        return -1;
    }
    *link_status = s_link_status;
    return LS_OK;
}

static ls_err_t custom_wifi_sta_connect(wifi_connect_cfg_t *cfg)
{
    if (cfg) {
        s_last_connect_cfg = *cfg;
        s_has_last_connect_cfg = true;
    } else {
        memset(&s_last_connect_cfg, 0, sizeof(s_last_connect_cfg));
        s_has_last_connect_cfg = false;
    }
    return LS_OK;
}

static ls_err_t custom_wifi_sta_disconnect(void)
{
    return LS_OK;
}

static ls_err_t custom_wifi_sta_mode_enable(void)
{
    return LS_OK;
}

static ls_err_t custom_wifi_sta_mode_disable(void)
{
    return LS_OK;
}

static ls_err_t custom_wifi_get_mode(int vif_idx, wifi_mode_e *mode)
{
    (void)vif_idx;
    if (mode) {
        *mode = WIFI_MODE_STA;
    }
    return LS_OK;
}

static ls_err_t custom_wifi_scan_start(wifi_scan_params_t *param)
{
    (void)param;
    return LS_OK;
}

static ls_err_t custom_wifi_sta_scanlist_dump(wifi_scan_result_t *results, int size, int *out_num)
{
    (void)results;
    (void)size;
    if (out_num) {
        *out_num = 0;
    }
    return LS_OK;
}

static ls_err_t custom_wifi_get_sta_mac(uint8_t *mac_addr)
{
    if (mac_addr) {
        memset(mac_addr, 0, 6);
    }
    return LS_OK;
}

void mock_wifi_api_init(void)
{
    RESET_FAKE(wifi_get_pmk);
    RESET_FAKE(wifi_set_pmk);
    RESET_FAKE(wifi_get_link_status);
    RESET_FAKE(wifi_sta_connect);
    RESET_FAKE(wifi_sta_disconnect);
    RESET_FAKE(wifi_sta_mode_enable);
    RESET_FAKE(wifi_sta_mode_disable);
    RESET_FAKE(wifi_get_mode);
    RESET_FAKE(wifi_scan_start);
    RESET_FAKE(wifi_sta_scanlist_dump);
    RESET_FAKE(wifi_get_sta_mac);

    wifi_get_pmk_fake.custom_fake = custom_wifi_get_pmk;
    wifi_set_pmk_fake.custom_fake = custom_wifi_set_pmk;
    wifi_get_link_status_fake.custom_fake = custom_wifi_get_link_status;
    wifi_sta_connect_fake.custom_fake = custom_wifi_sta_connect;
    wifi_sta_disconnect_fake.custom_fake = custom_wifi_sta_disconnect;
    wifi_sta_mode_enable_fake.custom_fake = custom_wifi_sta_mode_enable;
    wifi_sta_mode_disable_fake.custom_fake = custom_wifi_sta_mode_disable;
    wifi_get_mode_fake.custom_fake = custom_wifi_get_mode;
    wifi_scan_start_fake.custom_fake = custom_wifi_scan_start;
    wifi_sta_scanlist_dump_fake.custom_fake = custom_wifi_sta_scanlist_dump;
    wifi_get_sta_mac_fake.custom_fake = custom_wifi_get_sta_mac;
}

void mock_wifi_api_set_pmk(const uint8_t *pmk, bool valid)
{
    if (pmk && valid) {
        memcpy(s_pmk, pmk, sizeof(s_pmk));
        s_pmk_valid = true;
    } else {
        memset(s_pmk, 0, sizeof(s_pmk));
        s_pmk_valid = false;
    }
}

void mock_wifi_api_set_link_status(const wifi_link_status_t *status)
{
    if (status) {
        s_link_status = *status;
        s_link_status_valid = true;
    } else {
        memset(&s_link_status, 0, sizeof(s_link_status));
        s_link_status_valid = false;
    }
}

void mock_wifi_api_reset(void)
{
    mock_wifi_api_init();
    memset(s_pmk, 0, sizeof(s_pmk));
    s_pmk_valid = false;
    memset(&s_link_status, 0, sizeof(s_link_status));
    s_link_status_valid = false;
    memset(&s_last_connect_cfg, 0, sizeof(s_last_connect_cfg));
    s_has_last_connect_cfg = false;
}

const wifi_connect_cfg_t *mock_wifi_api_get_last_connect_cfg(void)
{
    return s_has_last_connect_cfg ? &s_last_connect_cfg : NULL;
}

bool mock_wifi_api_has_last_connect_cfg(void)
{
    return s_has_last_connect_cfg;
}

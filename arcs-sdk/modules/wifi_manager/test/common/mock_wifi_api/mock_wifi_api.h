/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "wifi_api.h"
#include "fff.h"

#ifdef __cplusplus
extern "C" {
#endif

DECLARE_FAKE_VALUE_FUNC(ls_err_t, wifi_get_pmk, uint8_t *);
DECLARE_FAKE_VALUE_FUNC(ls_err_t, wifi_set_pmk, const uint8_t *);
DECLARE_FAKE_VALUE_FUNC(ls_err_t, wifi_get_link_status, wifi_link_status_t *);
DECLARE_FAKE_VALUE_FUNC(ls_err_t, wifi_sta_connect, wifi_connect_cfg_t *);
DECLARE_FAKE_VALUE_FUNC0(ls_err_t, wifi_sta_disconnect);
DECLARE_FAKE_VALUE_FUNC0(ls_err_t, wifi_sta_mode_enable);
DECLARE_FAKE_VALUE_FUNC0(ls_err_t, wifi_sta_mode_disable);
DECLARE_FAKE_VALUE_FUNC(ls_err_t, wifi_get_mode, int, wifi_mode_e *);
DECLARE_FAKE_VALUE_FUNC(ls_err_t, wifi_scan_start, wifi_scan_params_t *);
DECLARE_FAKE_VALUE_FUNC(ls_err_t, wifi_sta_scanlist_dump, wifi_scan_result_t *, int, int *);
DECLARE_FAKE_VALUE_FUNC(ls_err_t, wifi_get_sta_mac, uint8_t *);

void mock_wifi_api_init(void);
void mock_wifi_api_set_pmk(const uint8_t *pmk, bool valid);
void mock_wifi_api_set_link_status(const wifi_link_status_t *status);
void mock_wifi_api_reset(void);
const wifi_connect_cfg_t *mock_wifi_api_get_last_connect_cfg(void);
bool mock_wifi_api_has_last_connect_cfg(void);

#ifdef __cplusplus
}
#endif

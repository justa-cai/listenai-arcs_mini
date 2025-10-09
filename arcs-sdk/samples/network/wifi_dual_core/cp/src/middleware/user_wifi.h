/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "wifi_api.h"
#include "ls_event.h"
#include "lisa_kv.h"
#ifdef __cplusplus
extern "C" {
#endif

void user_wifi_pre_init(void);

void user_wifi_start(void);

void customer_wifi_start(void);

void customer_wifi_event_start(int (*wifi_cb)(void *, event_module_t, int, void *));

void user_shell_start(void);

int user_wifi_connect(wifi_connect_cfg_t *config);

void handle_wifi_save_ip();

void handle_wifi_got_lisa_ip();

void wifi_ssid_ip_save();

void wifi_ssid_ip_restore();

#ifdef __cplusplus
}
#endif

/**
 * Copyright (c) 2023, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "wifi_manager_storage.h"
#include "fff.h"

#ifdef __cplusplus
extern "C" {
#endif

DECLARE_FAKE_VALUE_FUNC(int, wifi_storage_init, wifi_storage_ctx_t *, wifi_storage_ops_t *, void *);
DECLARE_FAKE_VALUE_FUNC(int, wifi_storage_deinit, wifi_storage_ctx_t *);
DECLARE_FAKE_VALUE_FUNC(int, wifi_storage_save_ap, wifi_storage_ctx_t *, wifi_mgr_sta_config_t *);
DECLARE_FAKE_VALUE_FUNC(int, wifi_storage_delete_ap, wifi_storage_ctx_t *, wifi_mgr_sta_config_t *);
DECLARE_FAKE_VALUE_FUNC(int, wifi_storage_search_ap, wifi_storage_ctx_t *, wifi_mgr_sta_config_t **,
                        wifi_mgr_storage_search_mode_t, void *);


void mock_wifi_storage_init(void);

void mock_wifi_storage_reset(void);

#ifdef __cplusplus
}
#endif

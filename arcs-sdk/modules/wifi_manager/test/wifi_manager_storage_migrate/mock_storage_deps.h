/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "fff.h"
#include "wifi_manager/wifi_manager_storage.h"

#ifdef __cplusplus
extern "C" {
#endif

DECLARE_FAKE_VALUE_FUNC(int, wifi_storage_blob_legacy_load_list, wifi_storage_ctx_t *, wifi_storage_item_t **, uint32_t *);
DECLARE_FAKE_VALUE_FUNC(int, wifi_storage_kv_load_list, wifi_storage_ctx_t *, wifi_storage_item_t **, uint32_t *);
DECLARE_FAKE_VALUE_FUNC(int, wifi_storage_kv_save_list, wifi_storage_ctx_t *, const wifi_storage_item_t *, uint32_t);

void mock_storage_deps_reset(void);

#ifdef __cplusplus
}
#endif

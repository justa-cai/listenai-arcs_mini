/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "mock_storage_deps.h"

DEFINE_FAKE_VALUE_FUNC(int, wifi_storage_blob_legacy_load_list, wifi_storage_ctx_t *, wifi_storage_item_t **, uint32_t *);
DEFINE_FAKE_VALUE_FUNC(int, wifi_storage_kv_load_list, wifi_storage_ctx_t *, wifi_storage_item_t **, uint32_t *);
DEFINE_FAKE_VALUE_FUNC(int, wifi_storage_kv_save_list, wifi_storage_ctx_t *, const wifi_storage_item_t *, uint32_t);

void mock_storage_deps_reset(void)
{
    RESET_FAKE(wifi_storage_blob_legacy_load_list);
    RESET_FAKE(wifi_storage_kv_load_list);
    RESET_FAKE(wifi_storage_kv_save_list);
}

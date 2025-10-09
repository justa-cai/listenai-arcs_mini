/**
 * Copyright (c) 2023, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include "mock_wifi_manager_storage.h"


DEFINE_FAKE_VALUE_FUNC(int, wifi_storage_init, wifi_storage_ctx_t *, wifi_storage_ops_t *, void *);
DEFINE_FAKE_VALUE_FUNC(int, wifi_storage_deinit, wifi_storage_ctx_t *);
DEFINE_FAKE_VALUE_FUNC(int, wifi_storage_save_ap, wifi_storage_ctx_t *, wifi_mgr_sta_config_t *);
DEFINE_FAKE_VALUE_FUNC(int, wifi_storage_save_ap_force, wifi_storage_ctx_t *, wifi_mgr_sta_config_t *);
DEFINE_FAKE_VALUE_FUNC(int, wifi_storage_delete_ap, wifi_storage_ctx_t *, wifi_mgr_sta_config_t *);
DEFINE_FAKE_VALUE_FUNC(int, wifi_storage_search_ap, wifi_storage_ctx_t *, wifi_mgr_sta_config_t **,
                        wifi_mgr_storage_search_mode_t, void *);


void mock_wifi_storage_init(void)
{
    RESET_FAKE(wifi_storage_init);
    RESET_FAKE(wifi_storage_deinit);
    RESET_FAKE(wifi_storage_save_ap);
    RESET_FAKE(wifi_storage_save_ap_force);
    RESET_FAKE(wifi_storage_delete_ap);
    RESET_FAKE(wifi_storage_search_ap);
}


void mock_wifi_storage_reset(void)
{
    mock_wifi_storage_init();
}


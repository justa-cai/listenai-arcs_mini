/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "ls_event.h"
#include "fff.h"

#ifdef __cplusplus
extern "C" {
#endif

DECLARE_FAKE_VALUE_FUNC(ls_err_t, ls_event_register_cb, event_module_t, int, event_cb_t, void *);
DECLARE_FAKE_VALUE_FUNC(ls_err_t, ls_event_unregister_cb, event_module_t, int, event_cb_t);
DECLARE_FAKE_VALUE_FUNC(ls_err_t, ls_event_wait, event_module_t, int, uint32_t);

void mock_ls_event_init(void);
void mock_ls_event_reset(void);
void mock_ls_event_trigger(event_module_t module, int event_id, void *event_data);

#ifdef __cplusplus
}
#endif

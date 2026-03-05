/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mock_ls_event.h"

#include <string.h>

#define MOCK_EVENT_MAX 8

typedef struct {
    event_module_t module;
    int event_id;
    event_cb_t cb;
    void *cb_arg;
} mock_event_entry_t;

static mock_event_entry_t s_entries[MOCK_EVENT_MAX];

DEFINE_FAKE_VALUE_FUNC(ls_err_t, ls_event_register_cb, event_module_t, int, event_cb_t, void *);
DEFINE_FAKE_VALUE_FUNC(ls_err_t, ls_event_unregister_cb, event_module_t, int, event_cb_t);
DEFINE_FAKE_VALUE_FUNC(ls_err_t, ls_event_wait, event_module_t, int, uint32_t);

static ls_err_t custom_event_register(event_module_t event_module_id, int event_id,
                                      event_cb_t event_cb, void *event_cb_arg)
{
    for (int i = 0; i < MOCK_EVENT_MAX; i++) {
        if (s_entries[i].cb == NULL) {
            s_entries[i].module = event_module_id;
            s_entries[i].event_id = event_id;
            s_entries[i].cb = event_cb;
            s_entries[i].cb_arg = event_cb_arg;
            return 0;
        }
    }
    return -1;
}

static ls_err_t custom_event_unregister(event_module_t event_module_id, int event_id,
                                        event_cb_t event_cb)
{
    for (int i = 0; i < MOCK_EVENT_MAX; i++) {
        if (s_entries[i].cb == event_cb &&
            s_entries[i].module == event_module_id &&
            s_entries[i].event_id == event_id) {
            memset(&s_entries[i], 0, sizeof(s_entries[i]));
            return 0;
        }
    }
    return -1;
}

static ls_err_t custom_event_wait(event_module_t event_module_id, int event_id, uint32_t timeout)
{
    (void)event_module_id;
    (void)event_id;
    (void)timeout;
    return 0;
}

void mock_ls_event_init(void)
{
    RESET_FAKE(ls_event_register_cb);
    RESET_FAKE(ls_event_unregister_cb);
    RESET_FAKE(ls_event_wait);

    ls_event_register_cb_fake.custom_fake = custom_event_register;
    ls_event_unregister_cb_fake.custom_fake = custom_event_unregister;
    ls_event_wait_fake.custom_fake = custom_event_wait;
}

void mock_ls_event_reset(void)
{
    mock_ls_event_init();
    memset(s_entries, 0, sizeof(s_entries));
}

void mock_ls_event_trigger(event_module_t module, int event_id, void *event_data)
{
    for (int i = 0; i < MOCK_EVENT_MAX; i++) {
        if (s_entries[i].cb &&
            s_entries[i].module == module &&
            s_entries[i].event_id == event_id) {
            s_entries[i].cb(s_entries[i].cb_arg, module, event_id, event_data);
        }
    }
}

/*
* SPDX-License-Identifier: Apache-2.0
*/

#pragma once

#include <stdint.h>
#include <stddef.h>

typedef void* gcl_cb_list_t;

typedef void(*gcl_event_cb_t) (uint32_t event, void *event_data, uint32_t event_data_len, void *arg);

gcl_cb_list_t gcl_cb_list_create(void);

int gcl_cb_event_dispatch(gcl_cb_list_t gcl_cb_list, uint32_t event, void *event_data, uint32_t event_data_len);

int gcl_cb_list_add_callback(gcl_cb_list_t gcl_cb_list, uint32_t events, gcl_event_cb_t callback, void *arg);
int gcl_cb_list_remove_callback(gcl_cb_list_t gcl_cb_list, gcl_event_cb_t callback);
void gcl_cb_list_clean_callbacks(gcl_cb_list_t gcl_cb_list);

void gcl_cb_list_delete(gcl_cb_list_t gcl_cb_list);

static inline int gcl_cb_event_dispatch_nodata(gcl_cb_list_t gcl_cb_list, uint32_t event)
{
    return gcl_cb_event_dispatch(gcl_cb_list, event, NULL, 0);
}

/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __BT_PAIRED_STORAGE_H__
#define __BT_PAIRED_STORAGE_H__

#include "lisa_bluetooth.h"

#ifdef __cplusplus
extern "C" {
#endif

int bt_paired_storage_load_list(bt_paired_info_t *list, uint8_t max_count, uint8_t *out_count);
int bt_paired_storage_find_name(const gap_bdaddr_t *addr, char *name, size_t name_len);
int bt_paired_storage_upsert(const bt_paired_info_t *item);
int bt_paired_storage_remove(const gap_bdaddr_t *addr);
int bt_paired_storage_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* __BT_PAIRED_STORAGE_H__ */

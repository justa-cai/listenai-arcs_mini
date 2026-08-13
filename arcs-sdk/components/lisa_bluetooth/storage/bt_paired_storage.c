/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bt_paired_storage.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#ifndef CONFIG_LISA_BLUETOOTH_STORAGE_KV
#define CONFIG_LISA_BLUETOOTH_STORAGE_KV 0
#endif

#if CONFIG_LISA_BLUETOOTH_STORAGE_KV
#include "lisa_kv.h"
#endif

#define BT_PAIRED_STORAGE_VERSION 1
#define BT_PAIRED_COUNT_KEY "bt-paired-count"
#define BT_PAIRED_ITEM_KEY_MAX_LEN 24

typedef struct {
    uint8_t version;
    uint8_t transport;
    uint8_t addr_type;
    uint8_t name_len;
    uint8_t addr[6];
    uint8_t name[BT_PAIRED_NAME_MAX_LEN];
} bt_paired_storage_item_t;

static bool bt_paired_storage_addr_equal(const gap_bdaddr_t *a, const gap_bdaddr_t *b)
{
    return a != NULL && b != NULL &&
           a->addr_type == b->addr_type &&
           memcmp(a->addr, b->addr, sizeof(a->addr)) == 0;
}

static void bt_paired_storage_item_to_public(const bt_paired_storage_item_t *src,
                                             bt_paired_info_t *dst)
{
    memset(dst, 0, sizeof(*dst));
    memcpy(dst->addr.addr, src->addr, sizeof(dst->addr.addr));
    dst->addr.addr_type = src->addr_type;
    dst->transport = src->transport;
    dst->name_len = src->name_len;
    if (dst->name_len > sizeof(dst->name)) {
        dst->name_len = sizeof(dst->name);
    }
    memcpy(dst->name, src->name, dst->name_len);
}

static void bt_paired_storage_public_to_item(const bt_paired_info_t *src,
                                             bt_paired_storage_item_t *dst)
{
    memset(dst, 0, sizeof(*dst));
    dst->version = BT_PAIRED_STORAGE_VERSION;
    dst->transport = src->transport;
    dst->addr_type = src->addr.addr_type;
    memcpy(dst->addr, src->addr.addr, sizeof(dst->addr));
    dst->name_len = src->name_len;
    if (dst->name_len > sizeof(dst->name)) {
        dst->name_len = sizeof(dst->name);
    }
    memcpy(dst->name, src->name, dst->name_len);
}

static int bt_paired_storage_make_item_key(char *key, size_t key_len, uint8_t index)
{
    int ret = snprintf(key, key_len, "bt-paired-item-%u", index);

    if (ret < 0 || (size_t)ret >= key_len) {
        return -ENAMETOOLONG;
    }

    return 0;
}

#if CONFIG_LISA_BLUETOOTH_STORAGE_KV
static int bt_paired_storage_ensure_init(void)
{
    static bool inited;

    if (inited) {
        return 0;
    }

    if (lisa_kv_init() != 0) {
        return -EIO;
    }

    inited = true;
    return 0;
}

static int bt_paired_storage_load_count(uint8_t *out_count)
{
    int count = 0;

    if (out_count == NULL) {
        return -EINVAL;
    }

    if (bt_paired_storage_ensure_init() != 0) {
        return -EIO;
    }

    if (lisa_kv_get_int(BT_PAIRED_COUNT_KEY, &count) != 0 || count <= 0) {
        *out_count = 0;
        return 0;
    }

    if (count > BT_PAIRED_MAX_COUNT) {
        count = BT_PAIRED_MAX_COUNT;
    }

    *out_count = (uint8_t)count;
    return 0;
}

static int bt_paired_storage_read_item(uint8_t index, bt_paired_storage_item_t *item)
{
    char key[BT_PAIRED_ITEM_KEY_MAX_LEN];
    uint8_t *data = NULL;
    int len = 0;

    if (item == NULL) {
        return -EINVAL;
    }

    if (bt_paired_storage_make_item_key(key, sizeof(key), index) != 0) {
        return -ENAMETOOLONG;
    }

    if (lisa_kv_get_blob(key, &data, &len) != 0) {
        return -ENOENT;
    }

    if (len != sizeof(*item)) {
        lisa_kv_free(data);
        return -EIO;
    }

    memcpy(item, data, sizeof(*item));
    lisa_kv_free(data);

    if (item->version != BT_PAIRED_STORAGE_VERSION) {
        return -EIO;
    }

    return 0;
}

static int bt_paired_storage_write_item(uint8_t index, const bt_paired_storage_item_t *item)
{
    char key[BT_PAIRED_ITEM_KEY_MAX_LEN];

    if (item == NULL) {
        return -EINVAL;
    }

    if (bt_paired_storage_make_item_key(key, sizeof(key), index) != 0) {
        return -ENAMETOOLONG;
    }

    return lisa_kv_set_blob(key, (uint8_t *)item, sizeof(*item));
}

static int bt_paired_storage_delete_item(uint8_t index)
{
    char key[BT_PAIRED_ITEM_KEY_MAX_LEN];

    if (bt_paired_storage_make_item_key(key, sizeof(key), index) != 0) {
        return -ENAMETOOLONG;
    }

    return lisa_kv_del(key);
}
#endif

int bt_paired_storage_load_list(bt_paired_info_t *list, uint8_t max_count, uint8_t *out_count)
{
#if CONFIG_LISA_BLUETOOTH_STORAGE_KV
    uint8_t count = 0;
    uint8_t copied = 0;
    bool count_only;
    int ret;

    if (out_count == NULL || (list == NULL && max_count != 0)) {
        return -EINVAL;
    }
    count_only = (list == NULL && max_count == 0);

    ret = bt_paired_storage_load_count(&count);
    if (ret != 0) {
        return ret;
    }

    if (count_only) {
        *out_count = count;
        return 0;
    }

    for (uint8_t idx = 0; idx < count && copied < max_count; idx++) {
        bt_paired_storage_item_t item;

        if (bt_paired_storage_read_item(idx, &item) == 0) {
            bt_paired_storage_item_to_public(&item, &list[copied++]);
        }
    }

    *out_count = copied;
    return 0;
#else
    if (out_count == NULL || (list == NULL && max_count != 0)) {
        return -EINVAL;
    }
    (void)list;
    (void)max_count;
    *out_count = 0;
    return 0;
#endif
}

int bt_paired_storage_find_name(const gap_bdaddr_t *addr, char *name, size_t name_len)
{
#if CONFIG_LISA_BLUETOOTH_STORAGE_KV
    uint8_t count = 0;
    int ret;

    if (addr == NULL || name == NULL || name_len == 0) {
        return -EINVAL;
    }

    name[0] = '\0';
    ret = bt_paired_storage_load_count(&count);
    if (ret != 0) {
        return ret;
    }

    for (uint8_t idx = 0; idx < count; idx++) {
        bt_paired_storage_item_t item;
        gap_bdaddr_t item_addr;

        if (bt_paired_storage_read_item(idx, &item) != 0) {
            continue;
        }

        memset(&item_addr, 0, sizeof(item_addr));
        memcpy(item_addr.addr, item.addr, sizeof(item_addr.addr));
        item_addr.addr_type = item.addr_type;
        if (!bt_paired_storage_addr_equal(addr, &item_addr)) {
            continue;
        }

        if (item.name_len == 0) {
            return -ENODATA;
        }

        size_t copy_len = item.name_len;
        if (copy_len >= name_len) {
            copy_len = name_len - 1;
        }
        memcpy(name, item.name, copy_len);
        name[copy_len] = '\0';
        return 0;
    }

    return -ENOENT;
#else
    if (addr == NULL || name == NULL || name_len == 0) {
        return -EINVAL;
    }
    name[0] = '\0';
    return -ENOENT;
#endif
}

int bt_paired_storage_upsert(const bt_paired_info_t *item)
{
#if CONFIG_LISA_BLUETOOTH_STORAGE_KV
    bt_paired_storage_item_t items[BT_PAIRED_MAX_COUNT];
    bt_paired_storage_item_t new_item;
    uint8_t count = 0;
    int found = -1;
    int ret;

    if (item == NULL) {
        return -EINVAL;
    }

    ret = bt_paired_storage_load_count(&count);
    if (ret != 0) {
        return ret;
    }

    memset(items, 0, sizeof(items));
    for (uint8_t idx = 0; idx < count; idx++) {
        (void)bt_paired_storage_read_item(idx, &items[idx]);
        gap_bdaddr_t item_addr = {0};
        memcpy(item_addr.addr, items[idx].addr, sizeof(item_addr.addr));
        item_addr.addr_type = items[idx].addr_type;
        if (bt_paired_storage_addr_equal(&item->addr, &item_addr) &&
            items[idx].transport == item->transport) {
            found = idx;
        }
    }

    bt_paired_storage_public_to_item(item, &new_item);
    if (found >= 0) {
        items[found] = new_item;
    } else {
        if (count >= BT_PAIRED_MAX_COUNT) {
            return -ENOSPC;
        }
        items[count++] = new_item;
    }

    if (lisa_kv_set_int(BT_PAIRED_COUNT_KEY, count) != 0) {
        return -EIO;
    }

    for (uint8_t idx = 0; idx < count; idx++) {
        if (bt_paired_storage_write_item(idx, &items[idx]) != 0) {
            return -EIO;
        }
    }

    return 0;
#else
    (void)item;
    return 0;
#endif
}

int bt_paired_storage_remove(const gap_bdaddr_t *addr)
{
#if CONFIG_LISA_BLUETOOTH_STORAGE_KV
    bt_paired_storage_item_t items[BT_PAIRED_MAX_COUNT];
    uint8_t count = 0;
    uint8_t out_count = 0;
    int ret;

    if (addr == NULL) {
        return bt_paired_storage_clear();
    }

    ret = bt_paired_storage_load_count(&count);
    if (ret != 0) {
        return ret;
    }

    memset(items, 0, sizeof(items));
    for (uint8_t idx = 0; idx < count; idx++) {
        bt_paired_storage_item_t item;
        gap_bdaddr_t item_addr = {0};

        if (bt_paired_storage_read_item(idx, &item) != 0) {
            continue;
        }

        memcpy(item_addr.addr, item.addr, sizeof(item_addr.addr));
        item_addr.addr_type = item.addr_type;
        if (!bt_paired_storage_addr_equal(addr, &item_addr)) {
            items[out_count++] = item;
        }
    }

    if (lisa_kv_set_int(BT_PAIRED_COUNT_KEY, out_count) != 0) {
        return -EIO;
    }

    for (uint8_t idx = 0; idx < out_count; idx++) {
        if (bt_paired_storage_write_item(idx, &items[idx]) != 0) {
            return -EIO;
        }
    }

    for (uint8_t idx = out_count; idx < count; idx++) {
        (void)bt_paired_storage_delete_item(idx);
    }

    return 0;
#else
    (void)addr;
    return 0;
#endif
}

int bt_paired_storage_clear(void)
{
#if CONFIG_LISA_BLUETOOTH_STORAGE_KV
    uint8_t count = 0;

    if (bt_paired_storage_load_count(&count) != 0) {
        return -EIO;
    }

    for (uint8_t idx = 0; idx < count; idx++) {
        (void)bt_paired_storage_delete_item(idx);
    }

    (void)lisa_kv_del(BT_PAIRED_COUNT_KEY);
    return 0;
#else
    return 0;
#endif
}

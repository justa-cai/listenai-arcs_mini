/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bt_storage_port.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "nvs.h"

#ifndef CONFIG_LISA_BLUETOOTH_STORAGE_KV
#define CONFIG_LISA_BLUETOOTH_STORAGE_KV 0
#endif

#if CONFIG_LISA_BLUETOOTH_STORAGE_KV
#include "lisa_kv.h"
#endif

#define BT_STORAGE_KV_KEY_MAX_LEN 16

static int bt_storage_kv_make_key(char *key, size_t key_len, uint8_t param_id)
{
    int ret = snprintf(key, key_len, "bt-nvds-%02x", param_id);

    if (ret < 0 || (size_t)ret >= key_len) {
        return -ENAMETOOLONG;
    }

    return 0;
}

#if CONFIG_LISA_BLUETOOTH_STORAGE_KV
static int bt_storage_kv_ensure_init(void)
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
#endif

uint8_t bt_storage_port_get(uint8_t param_id, uint8_t *lengthPtr, uint8_t *buf)
{
#if CONFIG_LISA_BLUETOOTH_STORAGE_KV
    char key[BT_STORAGE_KV_KEY_MAX_LEN];
    uint8_t *data = NULL;
    int data_len = 0;
    uint8_t max_len;

    if (lengthPtr == NULL || buf == NULL) {
        return NVDS_FAIL;
    }

    max_len = *lengthPtr;
    if (bt_storage_kv_ensure_init() != 0 ||
        bt_storage_kv_make_key(key, sizeof(key), param_id) != 0) {
        return NVDS_FAIL;
    }

    if (lisa_kv_get_blob(key, &data, &data_len) == 0) {
        size_t copy_len;

        if (data_len < 0 || data_len > UINT8_MAX) {
            lisa_kv_free(data);
            return NVDS_FAIL;
        }

        *lengthPtr = (uint8_t)data_len;
        copy_len = (uint8_t)data_len > max_len ? max_len : (size_t)data_len;
        memcpy(buf, data, copy_len);
        lisa_kv_free(data);
        return NVDS_OK;
    }

    return NVDS_FAIL;
#else
    (void)param_id;
    (void)lengthPtr;
    (void)buf;
    return NVDS_FAIL;
#endif
}

uint8_t bt_storage_port_set(uint8_t param_id, uint8_t length, uint8_t *buf)
{
#if CONFIG_LISA_BLUETOOTH_STORAGE_KV
    char key[BT_STORAGE_KV_KEY_MAX_LEN];
    uint8_t status;

    if (buf == NULL || length == 0) {
        return NVDS_FAIL;
    }

    if (bt_storage_kv_ensure_init() != 0 ||
        bt_storage_kv_make_key(key, sizeof(key), param_id) != 0) {
        return NVDS_FAIL;
    }

    status = lisa_kv_set_blob(key, buf, length) == 0 ? NVDS_OK : NVDS_FAIL;
    return status;
#else
    (void)param_id;
    (void)length;
    (void)buf;
    return NVDS_FAIL;
#endif
}

uint8_t bt_storage_port_del(uint8_t param_id)
{
#if CONFIG_LISA_BLUETOOTH_STORAGE_KV
    char key[BT_STORAGE_KV_KEY_MAX_LEN];

    if (bt_storage_kv_ensure_init() != 0 ||
        bt_storage_kv_make_key(key, sizeof(key), param_id) != 0) {
        return NVDS_FAIL;
    }

    uint8_t status = lisa_kv_del(key) == 0 ? NVDS_OK : NVDS_FAIL;
    return status;
#else
    (void)param_id;
    return NVDS_FAIL;
#endif
}

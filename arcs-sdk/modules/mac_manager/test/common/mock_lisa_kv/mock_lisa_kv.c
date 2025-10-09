/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mock_lisa_kv.h"

#include <stdlib.h>
#include <string.h>

DEFINE_FAKE_VOID_FUNC(lisa_kv_free, void *);
DEFINE_FAKE_VALUE_FUNC(int, lisa_kv_get_blob, const char *, uint8_t **, int *);
DEFINE_FAKE_VALUE_FUNC(int, lisa_kv_set_blob, const char *, uint8_t *, int);
DEFINE_FAKE_VALUE_FUNC(int, lisa_kv_del, const char *);


uint8_t *saved_data = NULL;
int saved_len = 0;

int mem_lisa_kv_get_blob(const char *key, uint8_t **data, int *len)
{
    uint8_t *got_data = malloc(saved_len);
    memcpy(got_data, saved_data, saved_len);
    *data = got_data;
    *len = saved_len;
    return 0;
}

int mem_lisa_kv_set_blob(const char *key, uint8_t *data, int len)
{
    saved_data = malloc(len);
    memcpy(saved_data, data, len);
    saved_len = len;
    return 0;
}

void mem_lisa_kv_free(void *value)
{
    if (value == NULL) {
        return;
    }

    free(value);
}

int mem_lisa_kv_del(const char *key)
{
    if (saved_data) {
        free(saved_data);
    }
    saved_data = NULL;
    saved_len = 0;
    return 0;
}

void mock_lisa_kv_init(void)
{
    RESET_FAKE(lisa_kv_free);
    RESET_FAKE(lisa_kv_get_blob);
    RESET_FAKE(lisa_kv_set_blob);
    RESET_FAKE(lisa_kv_del);

    lisa_kv_get_blob_fake.custom_fake = mem_lisa_kv_get_blob;
    lisa_kv_set_blob_fake.custom_fake = mem_lisa_kv_set_blob;
    lisa_kv_free_fake.custom_fake = mem_lisa_kv_free;
    lisa_kv_del_fake.custom_fake = mem_lisa_kv_del;
}

void mock_lisa_kv_reset(void)
{
    mock_lisa_kv_init();
    mem_lisa_kv_del("mac");
}
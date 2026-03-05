/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mock_lisa_kv.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MOCK_KV_MAX_ENTRIES 64

typedef struct {
    char *key;
    char *value;
} mock_kv_string_entry_t;

typedef struct {
    char *key;
    uint8_t *data;
    int len;
} mock_kv_blob_entry_t;

static mock_kv_string_entry_t s_string_entries[MOCK_KV_MAX_ENTRIES];
static mock_kv_blob_entry_t s_blob_entries[MOCK_KV_MAX_ENTRIES];

DEFINE_FAKE_VOID_FUNC(lisa_kv_free, void *);
DEFINE_FAKE_VALUE_FUNC(int, lisa_kv_get_blob, const char *, uint8_t **, int *);
DEFINE_FAKE_VALUE_FUNC(int, lisa_kv_set_blob, const char *, uint8_t *, int);
DEFINE_FAKE_VALUE_FUNC(int, lisa_kv_del, const char *);
DEFINE_FAKE_VALUE_FUNC(int, lisa_kv_get_int, const char *, int *);
DEFINE_FAKE_VALUE_FUNC(int, lisa_kv_set_int, const char *, int);
DEFINE_FAKE_VALUE_FUNC(int, lisa_kv_get_string, const char *, char **);
DEFINE_FAKE_VALUE_FUNC(int, lisa_kv_set_string, const char *, const char *);
DEFINE_FAKE_VALUE_FUNC(int, lisa_kv_get_bool, const char *, bool *);
DEFINE_FAKE_VALUE_FUNC(int, lisa_kv_set_bool, const char *, bool);

static void mock_kv_clear_entries(void)
{
    for (int i = 0; i < MOCK_KV_MAX_ENTRIES; i++) {
        free(s_string_entries[i].key);
        free(s_string_entries[i].value);
        s_string_entries[i].key = NULL;
        s_string_entries[i].value = NULL;

        free(s_blob_entries[i].key);
        free(s_blob_entries[i].data);
        s_blob_entries[i].key = NULL;
        s_blob_entries[i].data = NULL;
        s_blob_entries[i].len = 0;
    }
}

static char *mock_kv_strdup(const char *value)
{
    size_t len = value ? strlen(value) : 0;
    char *copy = malloc(len + 1);
    if (!copy) {
        return NULL;
    }
    if (len > 0) {
        memcpy(copy, value, len);
    }
    copy[len] = '\0';
    return copy;
}

static int mock_kv_find_string_index(const char *key)
{
    for (int i = 0; i < MOCK_KV_MAX_ENTRIES; i++) {
        if (s_string_entries[i].key && strcmp(s_string_entries[i].key, key) == 0) {
            return i;
        }
    }
    return -1;
}

static int mock_kv_find_blob_index(const char *key)
{
    for (int i = 0; i < MOCK_KV_MAX_ENTRIES; i++) {
        if (s_blob_entries[i].key && strcmp(s_blob_entries[i].key, key) == 0) {
            return i;
        }
    }
    return -1;
}

static int mock_kv_find_free_string_slot(void)
{
    for (int i = 0; i < MOCK_KV_MAX_ENTRIES; i++) {
        if (s_string_entries[i].key == NULL) {
            return i;
        }
    }
    return -1;
}

static int mock_kv_find_free_blob_slot(void)
{
    for (int i = 0; i < MOCK_KV_MAX_ENTRIES; i++) {
        if (s_blob_entries[i].key == NULL) {
            return i;
        }
    }
    return -1;
}

static int mem_lisa_kv_set_string(const char *key, const char *value)
{
    int idx = mock_kv_find_string_index(key);
    if (idx < 0) {
        idx = mock_kv_find_free_string_slot();
    }
    if (idx < 0) {
        return -1;
    }

    free(s_string_entries[idx].key);
    free(s_string_entries[idx].value);

    s_string_entries[idx].key = mock_kv_strdup(key);
    s_string_entries[idx].value = mock_kv_strdup(value ? value : "");
    if (!s_string_entries[idx].key || !s_string_entries[idx].value) {
        return -1;
    }
    return 0;
}

static int mem_lisa_kv_get_string(const char *key, char **value)
{
    int idx = mock_kv_find_string_index(key);
    if (idx < 0) {
        return -1;
    }
    *value = mock_kv_strdup(s_string_entries[idx].value);
    return *value ? 0 : -1;
}

static int mem_lisa_kv_set_int(const char *key, int value)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d", value);
    return mem_lisa_kv_set_string(key, buffer);
}

static int mem_lisa_kv_get_int(const char *key, int *value)
{
    char *stored = NULL;
    if (mem_lisa_kv_get_string(key, &stored) != 0) {
        return -1;
    }
    *value = atoi(stored);
    free(stored);
    return 0;
}

static int mem_lisa_kv_set_bool(const char *key, bool value)
{
    return mem_lisa_kv_set_int(key, value ? 1 : 0);
}

static int mem_lisa_kv_get_bool(const char *key, bool *value)
{
    int stored = 0;
    if (mem_lisa_kv_get_int(key, &stored) != 0) {
        return -1;
    }
    *value = stored != 0;
    return 0;
}

static int mem_lisa_kv_set_blob(const char *key, uint8_t *data, int len)
{
    int idx = mock_kv_find_blob_index(key);
    if (idx < 0) {
        idx = mock_kv_find_free_blob_slot();
    }
    if (idx < 0) {
        return -1;
    }

    free(s_blob_entries[idx].key);
    free(s_blob_entries[idx].data);

    s_blob_entries[idx].key = mock_kv_strdup(key);
    s_blob_entries[idx].data = NULL;
    s_blob_entries[idx].len = 0;

    if (data && len > 0) {
        s_blob_entries[idx].data = malloc(len);
        if (!s_blob_entries[idx].data) {
            return -1;
        }
        memcpy(s_blob_entries[idx].data, data, len);
        s_blob_entries[idx].len = len;
    }

    return 0;
}

static int mem_lisa_kv_get_blob(const char *key, uint8_t **data, int *len)
{
    int idx = mock_kv_find_blob_index(key);
    if (idx < 0) {
        return -1;
    }
    if (s_blob_entries[idx].data == NULL || s_blob_entries[idx].len <= 0) {
        return -1;
    }
    *data = malloc(s_blob_entries[idx].len);
    if (!*data) {
        return -1;
    }
    memcpy(*data, s_blob_entries[idx].data, s_blob_entries[idx].len);
    *len = s_blob_entries[idx].len;
    return 0;
}

static int mem_lisa_kv_del(const char *key)
{
    int idx = mock_kv_find_string_index(key);
    if (idx >= 0) {
        free(s_string_entries[idx].key);
        free(s_string_entries[idx].value);
        s_string_entries[idx].key = NULL;
        s_string_entries[idx].value = NULL;
    }

    idx = mock_kv_find_blob_index(key);
    if (idx >= 0) {
        free(s_blob_entries[idx].key);
        free(s_blob_entries[idx].data);
        s_blob_entries[idx].key = NULL;
        s_blob_entries[idx].data = NULL;
        s_blob_entries[idx].len = 0;
    }

    return 0;
}

static void mem_lisa_kv_free(void *value)
{
    free(value);
}

void mock_lisa_kv_init(void)
{
    RESET_FAKE(lisa_kv_free);
    RESET_FAKE(lisa_kv_get_blob);
    RESET_FAKE(lisa_kv_set_blob);
    RESET_FAKE(lisa_kv_del);
    RESET_FAKE(lisa_kv_get_int);
    RESET_FAKE(lisa_kv_set_int);
    RESET_FAKE(lisa_kv_get_string);
    RESET_FAKE(lisa_kv_set_string);
    RESET_FAKE(lisa_kv_get_bool);
    RESET_FAKE(lisa_kv_set_bool);

    mock_kv_clear_entries();

    lisa_kv_get_blob_fake.custom_fake = mem_lisa_kv_get_blob;
    lisa_kv_set_blob_fake.custom_fake = mem_lisa_kv_set_blob;
    lisa_kv_free_fake.custom_fake = mem_lisa_kv_free;
    lisa_kv_del_fake.custom_fake = mem_lisa_kv_del;
    lisa_kv_get_int_fake.custom_fake = mem_lisa_kv_get_int;
    lisa_kv_set_int_fake.custom_fake = mem_lisa_kv_set_int;
    lisa_kv_get_string_fake.custom_fake = mem_lisa_kv_get_string;
    lisa_kv_set_string_fake.custom_fake = mem_lisa_kv_set_string;
    lisa_kv_get_bool_fake.custom_fake = mem_lisa_kv_get_bool;
    lisa_kv_set_bool_fake.custom_fake = mem_lisa_kv_set_bool;
}

void mock_lisa_kv_reset(void)
{
    mock_lisa_kv_init();
}

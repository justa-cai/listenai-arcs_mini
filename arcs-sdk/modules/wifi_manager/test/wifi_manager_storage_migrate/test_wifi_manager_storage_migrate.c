/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "unity.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "fff.h"
#include "wifi_manager/wifi_manager_storage.h"
#include "wifi_manager_storage_internal.h"
#include "mock_lisa_kv.h"
#include "mock_storage_deps.h"

DEFINE_FFF_GLOBALS;

static wifi_storage_ctx_t s_ctx;
static int s_mutex;
static int s_alloc_count;
static int s_free_count;

static int kv_load_no_data(wifi_storage_ctx_t *ctx, wifi_storage_item_t **out_list, uint32_t *out_count)
{
    (void)ctx;
    if (out_list != NULL) {
        *out_list = NULL;
    }
    if (out_count != NULL) {
        *out_count = 0;
    }
    return -ENOENT;
}

static void *test_malloc(size_t size)
{
    return malloc(size);
}

static void *test_calloc(size_t nmemb, size_t size)
{
    void *ptr = calloc(nmemb, size);
    if (ptr != NULL) {
        s_alloc_count++;
    }
    return ptr;
}

static void test_free(void *ptr)
{
    if (ptr != NULL) {
        s_free_count++;
    }
    free(ptr);
}

static int test_mutex_lock(void *mutex, uint32_t timeout)
{
    (void)mutex;
    (void)timeout;
    return 0;
}

static int test_mutex_unlock(void *mutex)
{
    (void)mutex;
    return 0;
}

static void init_ctx(void)
{
    wifi_storage_ops_t ops = {
        .malloc = test_malloc,
        .calloc = test_calloc,
        .free = test_free,
        .mutex_lock = test_mutex_lock,
        .mutex_unlock = test_mutex_unlock,
    };
    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.ops = ops;
    s_ctx.mutex = &s_mutex;
}

void setUp(void)
{
    mock_lisa_kv_reset();
    mock_storage_deps_reset();
    wifi_storage_kv_load_list_fake.custom_fake = kv_load_no_data;
    s_alloc_count = 0;
    s_free_count = 0;
    init_ctx();
}

void tearDown(void)
{
}

/* ========== wifi_storage_migrate_load_count tests ========== */

void test_migrate_load_count_null_param_should_return_einval(void)
{
    int ret = wifi_storage_migrate_load_count(NULL);
    TEST_ASSERT_EQUAL(-EINVAL, ret);
}

void test_migrate_load_count_kv_count_found_should_return_count(void)
{
    TEST_ASSERT_EQUAL(0, lisa_kv_set_int(WIFI_STORAGE_KV_COUNT_KEY, 5));

    uint32_t count = 0;
    int ret = wifi_storage_migrate_load_count(&count);

    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_EQUAL(5u, count);
}

void test_migrate_load_count_blob_count_found_should_return_count(void)
{
    TEST_ASSERT_EQUAL(0, lisa_kv_set_int(WIFI_STORAGE_BLOB_COUNT_KEY, 3));

    uint32_t count = 0;
    int ret = wifi_storage_migrate_load_count(&count);

    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_EQUAL(3u, count);
}

void test_migrate_load_count_kv_preferred_over_blob(void)
{
    TEST_ASSERT_EQUAL(0, lisa_kv_set_int(WIFI_STORAGE_KV_COUNT_KEY, 10));
    TEST_ASSERT_EQUAL(0, lisa_kv_set_int(WIFI_STORAGE_BLOB_COUNT_KEY, 2));

    uint32_t count = 0;
    int ret = wifi_storage_migrate_load_count(&count);

    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_EQUAL(10u, count);
}

void test_migrate_load_count_no_data_should_return_enoent(void)
{
    uint32_t count = 99;
    int ret = wifi_storage_migrate_load_count(&count);

    TEST_ASSERT_EQUAL(-ENOENT, ret);
    TEST_ASSERT_EQUAL(0u, count);
}

void test_migrate_load_count_negative_count_should_return_zero(void)
{
    TEST_ASSERT_EQUAL(0, lisa_kv_set_int(WIFI_STORAGE_KV_COUNT_KEY, -5));

    uint32_t count = 99;
    int ret = wifi_storage_migrate_load_count(&count);

    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_EQUAL(0u, count);
}

/* ========== wifi_storage_migrate_load_list tests ========== */

static wifi_storage_item_t s_mock_list[2];
static wifi_storage_item_t *s_allocated_list = NULL;

static int blob_load_success(wifi_storage_ctx_t *ctx, wifi_storage_item_t **out_list, uint32_t *out_count)
{
    wifi_storage_item_t *list = ctx->ops.calloc(2, sizeof(wifi_storage_item_t));
    if (list == NULL) {
        return -ENOMEM;
    }
    memcpy(list, s_mock_list, sizeof(s_mock_list));
    *out_list = list;
    *out_count = 2;
    return 0;
}

static int blob_load_with_alloc(wifi_storage_ctx_t *ctx, wifi_storage_item_t **out_list, uint32_t *out_count)
{
    s_allocated_list = ctx->ops.calloc(1, sizeof(wifi_storage_item_t));
    if (s_allocated_list == NULL) {
        return -ENOMEM;
    }
    strcpy(s_allocated_list->ap_info.ssid, "AllocatedSSID");
    s_allocated_list->enable = 1;
    *out_list = s_allocated_list;
    *out_count = 1;
    return 0;
}

static int blob_load_with_empty_ssid(wifi_storage_ctx_t *ctx, wifi_storage_item_t **out_list, uint32_t *out_count)
{
    wifi_storage_item_t *list = ctx->ops.calloc(2, sizeof(wifi_storage_item_t));
    if (list == NULL) {
        return -ENOMEM;
    }
    strcpy(list[0].ap_info.ssid, "TestSSID1");
    list[0].enable = 1;
    list[0].version = WIFI_STORAGE_VERSION_CURRENT;
    list[1].version = WIFI_STORAGE_VERSION_LEGACY;
    list[1].enable = 1;
    *out_list = list;
    *out_count = 2;
    return 0;
}

static int kv_load_with_empty_ssid(wifi_storage_ctx_t *ctx, wifi_storage_item_t **out_list, uint32_t *out_count)
{
    wifi_storage_item_t *list = ctx->ops.calloc(1, sizeof(wifi_storage_item_t));
    if (list == NULL) {
        return -ENOMEM;
    }
    list[0].enable = 1;
    /* ssid left empty */
    *out_list = list;
    *out_count = 1;
    return 0;
}

static int blob_load_single_valid(wifi_storage_ctx_t *ctx, wifi_storage_item_t **out_list, uint32_t *out_count)
{
    wifi_storage_item_t *list = ctx->ops.calloc(1, sizeof(wifi_storage_item_t));
    if (list == NULL) {
        return -ENOMEM;
    }
    strcpy(list[0].ap_info.ssid, "LegacyValid");
    list[0].enable = 1;
    *out_list = list;
    *out_count = 1;
    return 0;
}

void test_migrate_load_list_null_params_should_free_list_and_succeed(void)
{
    wifi_storage_blob_legacy_load_list_fake.custom_fake = blob_load_with_alloc;
    wifi_storage_kv_save_list_fake.return_val = 0;

    int ret = wifi_storage_migrate_load_list(&s_ctx, NULL, NULL);

    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_EQUAL(1, wifi_storage_kv_save_list_fake.call_count);
    TEST_ASSERT_EQUAL(s_alloc_count, s_free_count);
}

void test_migrate_load_list_success_should_return_list_and_save_to_kv(void)
{
    memset(s_mock_list, 0, sizeof(s_mock_list));
    strcpy(s_mock_list[0].ap_info.ssid, "TestSSID1");
    s_mock_list[0].enable = 1;
    strcpy(s_mock_list[1].ap_info.ssid, "TestSSID2");
    s_mock_list[1].enable = 1;

    wifi_storage_blob_legacy_load_list_fake.custom_fake = blob_load_success;
    wifi_storage_kv_save_list_fake.return_val = 0;

    wifi_storage_item_t *out_list = NULL;
    uint32_t out_count = 0;
    int ret = wifi_storage_migrate_load_list(&s_ctx, &out_list, &out_count);

    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_EQUAL(2u, out_count);
    TEST_ASSERT_NOT_NULL(out_list);
    TEST_ASSERT_EQUAL_STRING("TestSSID1", out_list[0].ap_info.ssid);
    TEST_ASSERT_EQUAL_STRING("TestSSID2", out_list[1].ap_info.ssid);
    TEST_ASSERT_EQUAL(1, wifi_storage_kv_save_list_fake.call_count);
}

void test_migrate_load_list_blob_load_fail_should_return_error(void)
{
    wifi_storage_blob_legacy_load_list_fake.return_val = -ENOENT;

    wifi_storage_item_t *out_list = NULL;
    uint32_t out_count = 0;
    int ret = wifi_storage_migrate_load_list(&s_ctx, &out_list, &out_count);

    TEST_ASSERT_EQUAL(-ENOENT, ret);
    TEST_ASSERT_NULL(out_list);
    TEST_ASSERT_EQUAL(0u, out_count);
    TEST_ASSERT_EQUAL(0, wifi_storage_kv_save_list_fake.call_count);
}

void test_migrate_load_list_kv_save_fail_should_free_list(void)
{
    s_allocated_list = NULL;
    wifi_storage_blob_legacy_load_list_fake.custom_fake = blob_load_with_alloc;
    wifi_storage_kv_save_list_fake.return_val = -EIO;

    wifi_storage_item_t *out_list = NULL;
    uint32_t out_count = 0;
    int ret = wifi_storage_migrate_load_list(&s_ctx, &out_list, &out_count);

    TEST_ASSERT_NOT_EQUAL(0, ret);
    TEST_ASSERT_NULL(out_list);
    TEST_ASSERT_EQUAL(0u, out_count);
    TEST_ASSERT_EQUAL(s_alloc_count, s_free_count);
}

void test_migrate_load_list_should_filter_empty_ssid(void)
{
    wifi_storage_blob_legacy_load_list_fake.custom_fake = blob_load_with_empty_ssid;
    wifi_storage_kv_save_list_fake.return_val = 0;

    wifi_storage_item_t *out_list = NULL;
    uint32_t out_count = 0;
    int ret = wifi_storage_migrate_load_list(&s_ctx, &out_list, &out_count);

    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_EQUAL(1u, out_count);
    TEST_ASSERT_NOT_NULL(out_list);
    TEST_ASSERT_EQUAL_STRING("TestSSID1", out_list[0].ap_info.ssid);
    TEST_ASSERT_EQUAL(1, wifi_storage_kv_save_list_fake.call_count);
    TEST_ASSERT_EQUAL(1u, wifi_storage_kv_save_list_fake.arg2_val);

    const wifi_storage_item_t *saved_list = wifi_storage_kv_save_list_fake.arg1_val;
    TEST_ASSERT_NOT_NULL(saved_list);
    TEST_ASSERT_EQUAL_STRING("TestSSID1", saved_list[0].ap_info.ssid);
    TEST_ASSERT_EQUAL(1, saved_list[0].enable);
}

void test_migrate_load_list_should_drop_empty_ssid_in_kv(void)
{
    wifi_storage_blob_legacy_load_list_fake.custom_fake = blob_load_single_valid;
    wifi_storage_kv_load_list_fake.custom_fake = kv_load_with_empty_ssid;
    wifi_storage_kv_save_list_fake.return_val = 0;

    wifi_storage_item_t *out_list = NULL;
    uint32_t out_count = 0;
    int ret = wifi_storage_migrate_load_list(&s_ctx, &out_list, &out_count);

    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_EQUAL(1u, out_count);
    TEST_ASSERT_NOT_NULL(out_list);
    TEST_ASSERT_EQUAL_STRING("LegacyValid", out_list[0].ap_info.ssid);
    TEST_ASSERT_NOT_NULL(wifi_storage_kv_save_list_fake.arg1_val);
    TEST_ASSERT_EQUAL_STRING("LegacyValid", wifi_storage_kv_save_list_fake.arg1_val[0].ap_info.ssid);
    WIFI_STORAGE_FREE(&s_ctx, out_list);
}

void test_migrate_load_list_blob_eio_should_return_eio(void)
{
    wifi_storage_blob_legacy_load_list_fake.return_val = -EIO;

    wifi_storage_item_t *out_list = NULL;
    uint32_t out_count = 0;
    int ret = wifi_storage_migrate_load_list(&s_ctx, &out_list, &out_count);

    TEST_ASSERT_EQUAL(-EIO, ret);
    TEST_ASSERT_NULL(out_list);
    TEST_ASSERT_EQUAL(0u, out_count);
}

/* ========== wifi_storage_migrate_delete_legacy tests ========== */

void test_migrate_delete_legacy_should_delete_blob_keys(void)
{
    TEST_ASSERT_EQUAL(0, lisa_kv_set_int(WIFI_STORAGE_BLOB_COUNT_KEY, 2));
    TEST_ASSERT_EQUAL(0, lisa_kv_set_blob(WIFI_STORAGE_BLOB_LIST_KEY, (uint8_t *)"test", 4));

    int count_before = 0;
    TEST_ASSERT_EQUAL(0, lisa_kv_get_int(WIFI_STORAGE_BLOB_COUNT_KEY, &count_before));
    TEST_ASSERT_EQUAL(2, count_before);

    wifi_storage_migrate_delete_legacy();

    int count_after = 0;
    TEST_ASSERT_NOT_EQUAL(0, lisa_kv_get_int(WIFI_STORAGE_BLOB_COUNT_KEY, &count_after));

    uint8_t *blob = NULL;
    int blob_len = 0;
    TEST_ASSERT_NOT_EQUAL(0, lisa_kv_get_blob(WIFI_STORAGE_BLOB_LIST_KEY, &blob, &blob_len));
}

void test_migrate_delete_legacy_should_not_affect_kv_keys(void)
{
    TEST_ASSERT_EQUAL(0, lisa_kv_set_int(WIFI_STORAGE_KV_COUNT_KEY, 3));
    TEST_ASSERT_EQUAL(0, lisa_kv_set_int(WIFI_STORAGE_BLOB_COUNT_KEY, 1));

    wifi_storage_migrate_delete_legacy();

    int kv_count = 0;
    TEST_ASSERT_EQUAL(0, lisa_kv_get_int(WIFI_STORAGE_KV_COUNT_KEY, &kv_count));
    TEST_ASSERT_EQUAL(3, kv_count);
}

int main(void)
{
    UNITY_BEGIN();

    /* migrate_load_count tests */
    RUN_TEST(test_migrate_load_count_null_param_should_return_einval);
    RUN_TEST(test_migrate_load_count_kv_count_found_should_return_count);
    RUN_TEST(test_migrate_load_count_blob_count_found_should_return_count);
    RUN_TEST(test_migrate_load_count_kv_preferred_over_blob);
    RUN_TEST(test_migrate_load_count_no_data_should_return_enoent);
    RUN_TEST(test_migrate_load_count_negative_count_should_return_zero);

    /* migrate_load_list tests */
    RUN_TEST(test_migrate_load_list_null_params_should_free_list_and_succeed);
    RUN_TEST(test_migrate_load_list_success_should_return_list_and_save_to_kv);
    RUN_TEST(test_migrate_load_list_blob_load_fail_should_return_error);
    RUN_TEST(test_migrate_load_list_kv_save_fail_should_free_list);
    RUN_TEST(test_migrate_load_list_should_filter_empty_ssid);
    RUN_TEST(test_migrate_load_list_blob_eio_should_return_eio);

    /* migrate_delete_legacy tests */
    RUN_TEST(test_migrate_delete_legacy_should_delete_blob_keys);
    RUN_TEST(test_migrate_delete_legacy_should_not_affect_kv_keys);

    return UNITY_END();
}

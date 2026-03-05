/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "unity.h"
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include "wifi_manager/wifi_manager_storage.h"
#include "wifi_manager_storage_cache.h"
#include "wifi_manager_storage_internal.h"

static int s_loader_call_count;
static int s_free_count;

static void *test_malloc(size_t size)
{
    return malloc(size);
}

static void *test_calloc(size_t nmemb, size_t size)
{
    return calloc(nmemb, size);
}

static void test_free(void *ptr)
{
    if (ptr) {
        s_free_count++;
    }
    free(ptr);
}

static int loader_success(wifi_storage_ctx_t *ctx, wifi_storage_item_t **out_list, uint32_t *out_count)
{
    s_loader_call_count++;
    *out_list = WIFI_STORAGE_CALLOC(ctx, 1, sizeof(wifi_storage_item_t));
    if (*out_list == NULL) {
        return -ENOMEM;
    }
    (*out_list)[0].enable = 1;
    strcpy((*out_list)[0].ap_info.ssid, "cached");
    *out_count = 1;
    return 0;
}

static int loader_fail(wifi_storage_ctx_t *ctx, wifi_storage_item_t **out_list, uint32_t *out_count)
{
    (void)ctx;
    (void)out_list;
    (void)out_count;
    s_loader_call_count++;
    return -EIO;
}

static void init_ctx(wifi_storage_ctx_t *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->ops.malloc = test_malloc;
    ctx->ops.calloc = test_calloc;
    ctx->ops.free = test_free;
    wifi_storage_cache_init_ctx(ctx);
}

void setUp(void)
{
    s_loader_call_count = 0;
    s_free_count = 0;
}

void tearDown(void)
{
}

void test_cache_load_caches_and_reuses(void)
{
    wifi_storage_ctx_t ctx;
    init_ctx(&ctx);

    TEST_ASSERT_EQUAL(0, wifi_storage_cache_load(&ctx, loader_success));
    TEST_ASSERT_TRUE(wifi_storage_cache_valid(&ctx));

    wifi_storage_item_t *list = NULL;
    uint32_t count = 0;
    wifi_storage_cache_get(&ctx, &list, &count);
    TEST_ASSERT_NOT_NULL(list);
    TEST_ASSERT_EQUAL(1U, count);
    TEST_ASSERT_EQUAL_STRING("cached", list[0].ap_info.ssid);

    // Second load should not call loader again
    TEST_ASSERT_EQUAL(0, wifi_storage_cache_load(&ctx, loader_success));
    TEST_ASSERT_EQUAL(1, s_loader_call_count);

    wifi_storage_cache_drop(&ctx);
    TEST_ASSERT_EQUAL(1, s_free_count);
}

void test_cache_set_replaces_and_frees_old_list(void)
{
    wifi_storage_ctx_t ctx;
    init_ctx(&ctx);

    wifi_storage_item_t *first = WIFI_STORAGE_CALLOC(&ctx, 1, sizeof(wifi_storage_item_t));
    wifi_storage_item_t *second = WIFI_STORAGE_CALLOC(&ctx, 1, sizeof(wifi_storage_item_t));

    wifi_storage_cache_set(&ctx, first, 1);
    wifi_storage_cache_drop(&ctx); // free first

    wifi_storage_cache_set(&ctx, second, 1);
    TEST_ASSERT_TRUE(wifi_storage_cache_valid(&ctx));
    wifi_storage_item_t *list = NULL;
    uint32_t count = 0;
    wifi_storage_cache_get(&ctx, &list, &count);
    TEST_ASSERT_EQUAL(second, list);
    TEST_ASSERT_EQUAL(1U, count);

    wifi_storage_cache_drop(&ctx);
    TEST_ASSERT_EQUAL(2, s_free_count); // second freed by drop
}

void test_cache_load_error_does_not_mark_valid(void)
{
    wifi_storage_ctx_t ctx;
    init_ctx(&ctx);

    TEST_ASSERT_EQUAL(-EIO, wifi_storage_cache_load(&ctx, loader_fail));
    TEST_ASSERT_FALSE(wifi_storage_cache_valid(&ctx));
    wifi_storage_item_t *list = (wifi_storage_item_t *)0x1;
    uint32_t count = 123;
    wifi_storage_cache_get(&ctx, &list, &count);
    TEST_ASSERT_NULL(list);
    TEST_ASSERT_EQUAL(0U, count);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_cache_load_caches_and_reuses);
    RUN_TEST(test_cache_set_replaces_and_frees_old_list);
    RUN_TEST(test_cache_load_error_does_not_mark_valid);
    return UNITY_END();
}

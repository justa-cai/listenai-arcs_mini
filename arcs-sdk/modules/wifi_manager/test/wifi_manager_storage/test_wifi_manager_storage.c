/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "unity.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "fff.h"
#include "wifi_manager/wifi_manager_storage.h"
#include "wifi_manager_storage_internal.h"
#include "mock_lisa_kv.h"

DEFINE_FFF_GLOBALS;

typedef struct {
    char ssid[32];
    char pwd[64];
    char bssid[18];
    int channel;
    int rssi;
    wifi_mgr_wifi_encryption_mode_t encryption_mode;
} wifi_mgr_sta_config_legacy_t;

typedef struct {
    wifi_mgr_sta_config_legacy_t ap_info;
    uint8_t enable;
} wifi_storage_item_legacy_v1_t;

static wifi_storage_ctx_t s_ctx;
static int s_mutex;
static int s_alloc_count;
static int s_free_count;
static int (*s_kv_set_int_passthrough)(const char *key, int value);
static bool s_fail_bool_alloc;

static void set_kv_item(int index, const char *ssid, const char *bssid);
static void set_kv_count(int count);

static void *test_malloc(size_t size)
{
    return malloc(size);
}

static void *test_calloc(size_t nmemb, size_t size)
{
    return calloc(nmemb, size);
}

static void *test_calloc_fail_bool(size_t nmemb, size_t size)
{
    if (s_fail_bool_alloc && size == sizeof(bool)) {
        return NULL;
    }
    return calloc(nmemb, size);
}

static void *test_calloc_track(size_t nmemb, size_t size)
{
    void *ptr = calloc(nmemb, size);
    if (ptr != NULL) {
        s_alloc_count++;
    }
    return ptr;
}

static void test_free(void *ptr)
{
    free(ptr);
}

static void test_free_track(void *ptr)
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

static void init_storage(void)
{
    wifi_storage_ops_t ops = {
        .malloc = test_malloc,
        .calloc = test_calloc,
        .free = test_free,
        .mutex_lock = test_mutex_lock,
        .mutex_unlock = test_mutex_unlock,
    };
    memset(&s_ctx, 0, sizeof(s_ctx));
    TEST_ASSERT_EQUAL(0, wifi_storage_init(&s_ctx, &ops, &s_mutex));
}

static void init_storage_with_ops(wifi_storage_ops_t *ops)
{
    memset(&s_ctx, 0, sizeof(s_ctx));
    TEST_ASSERT_EQUAL(0, wifi_storage_init(&s_ctx, ops, &s_mutex));
}

void setUp(void)
{
    mock_lisa_kv_reset();
    s_alloc_count = 0;
    s_free_count = 0;
    s_fail_bool_alloc = false;
    s_kv_set_int_passthrough = lisa_kv_set_int_fake.custom_fake;
    init_storage();
}

void tearDown(void)
{
    wifi_storage_deinit(&s_ctx);
}

void test_wifi_storage_kv_round_trip(void)
{
    wifi_mgr_sta_config_t ap = {0};
    strcpy(ap.ssid, "TestSSID");
    strcpy(ap.pwd, "TestPwd");
    strcpy(ap.bssid, "aa:bb:cc:dd:ee:ff");
    ap.channel = 11;
    ap.rssi = -40;
    ap.encryption_mode = WIFI_MGR_WIFI_AUTH_WPA2_PSK;

    TEST_ASSERT_EQUAL(0, wifi_storage_save_ap(&s_ctx, &ap));

    wifi_mgr_sta_config_t *matched_list = NULL;
    int count = wifi_storage_search_ap(&s_ctx, &matched_list, SEARCH_BY_SSID, ap.ssid);

    TEST_ASSERT_EQUAL(1, count);
    TEST_ASSERT_NOT_NULL(matched_list);
    TEST_ASSERT_EQUAL_STRING(ap.ssid, matched_list[0].ssid);
    TEST_ASSERT_EQUAL_STRING(ap.pwd, matched_list[0].pwd);
    TEST_ASSERT_EQUAL_STRING(ap.bssid, matched_list[0].bssid);
    TEST_ASSERT_EQUAL(ap.channel, matched_list[0].channel);
    TEST_ASSERT_EQUAL(ap.encryption_mode, matched_list[0].encryption_mode);

    test_free(matched_list);

    int kv_count = 0;
    TEST_ASSERT_EQUAL(0, lisa_kv_get_int(WIFI_STORAGE_KV_COUNT_KEY, &kv_count));
    TEST_ASSERT_EQUAL(1, kv_count);
}

void test_wifi_storage_migrate_blob_to_kv(void)
{
    wifi_storage_item_legacy_v1_t legacy_items[2] = {0};
    strcpy(legacy_items[0].ap_info.ssid, "LegacySSID1");
    strcpy(legacy_items[0].ap_info.pwd, "LegacyPwd1");
    strcpy(legacy_items[0].ap_info.bssid, "00:11:22:33:44:55");
    legacy_items[0].ap_info.channel = 1;
    legacy_items[0].ap_info.rssi = -30;
    legacy_items[0].ap_info.encryption_mode = WIFI_MGR_WIFI_AUTH_WPA_PSK;
    legacy_items[0].enable = 1;

    strcpy(legacy_items[1].ap_info.ssid, "LegacySSID2");
    strcpy(legacy_items[1].ap_info.pwd, "LegacyPwd2");
    strcpy(legacy_items[1].ap_info.bssid, "66:77:88:99:aa:bb");
    legacy_items[1].ap_info.channel = 6;
    legacy_items[1].ap_info.rssi = -50;
    legacy_items[1].ap_info.encryption_mode = WIFI_MGR_WIFI_AUTH_WPA2_PSK;
    legacy_items[1].enable = 1;

    TEST_ASSERT_EQUAL(0, lisa_kv_set_int(WIFI_STORAGE_BLOB_COUNT_KEY, 2));
    TEST_ASSERT_EQUAL(0, lisa_kv_set_blob(WIFI_STORAGE_BLOB_LIST_KEY,
                                         (uint8_t *)legacy_items,
                                         sizeof(legacy_items)));

    wifi_storage_deinit(&s_ctx);
    init_storage();

    wifi_mgr_sta_config_t *matched_list = NULL;
    int count = wifi_storage_search_ap(&s_ctx, &matched_list, SEARCH_ALL, NULL);

    TEST_ASSERT_EQUAL(2, count);
    TEST_ASSERT_NOT_NULL(matched_list);
    TEST_ASSERT_EQUAL_STRING("LegacySSID2", matched_list[0].ssid);
    TEST_ASSERT_EQUAL_STRING("LegacySSID1", matched_list[1].ssid);

    test_free(matched_list);

    int kv_count = 0;
    TEST_ASSERT_EQUAL(0, lisa_kv_get_int(WIFI_STORAGE_KV_COUNT_KEY, &kv_count));
    TEST_ASSERT_EQUAL(2, kv_count);

    char *ssid0 = NULL;
    TEST_ASSERT_EQUAL(0, lisa_kv_get_string("wifi-kv-item-0-ssid", &ssid0));
    TEST_ASSERT_EQUAL_STRING("LegacySSID1", ssid0);
    lisa_kv_free(ssid0);
}

void test_wifi_storage_kv_preferred_over_blob(void)
{
    TEST_ASSERT_EQUAL(0, lisa_kv_set_int(WIFI_STORAGE_KV_COUNT_KEY, 1));
    TEST_ASSERT_EQUAL(0, lisa_kv_set_bool("wifi-kv-item-0-enable", true));
    TEST_ASSERT_EQUAL(0, lisa_kv_set_string("wifi-kv-item-0-ssid", "KVSSID"));
    TEST_ASSERT_EQUAL(0, lisa_kv_set_string("wifi-kv-item-0-pwd", "KVPWD"));
    TEST_ASSERT_EQUAL(0, lisa_kv_set_string("wifi-kv-item-0-bssid", "11:22:33:44:55:66"));
    TEST_ASSERT_EQUAL(0, lisa_kv_set_int("wifi-kv-item-0-channel", 3));
    TEST_ASSERT_EQUAL(0, lisa_kv_set_int("wifi-kv-item-0-rssi", -60));
    TEST_ASSERT_EQUAL(0, lisa_kv_set_int("wifi-kv-item-0-encryption", WIFI_MGR_WIFI_AUTH_OPEN));

    wifi_storage_item_legacy_v1_t legacy_items[1] = {0};
    strcpy(legacy_items[0].ap_info.ssid, "LegacySSID");
    strcpy(legacy_items[0].ap_info.bssid, "11:22:33:44:55:66");
    legacy_items[0].ap_info.channel = 8;
    legacy_items[0].enable = 1;
    TEST_ASSERT_EQUAL(0, lisa_kv_set_int(WIFI_STORAGE_BLOB_COUNT_KEY, 1));
    TEST_ASSERT_EQUAL(0, lisa_kv_set_blob(WIFI_STORAGE_BLOB_LIST_KEY,
                                         (uint8_t *)legacy_items,
                                         sizeof(legacy_items)));

    lisa_kv_get_blob_fake.call_count = 0;
    wifi_storage_deinit(&s_ctx);
    init_storage();

    wifi_mgr_sta_config_t *matched_list = NULL;
    int count = wifi_storage_search_ap(&s_ctx, &matched_list, SEARCH_ALL, NULL);

    TEST_ASSERT_EQUAL(1, count);
    TEST_ASSERT_NOT_NULL(matched_list);
    TEST_ASSERT_EQUAL_STRING("KVSSID", matched_list[0].ssid);
    TEST_ASSERT_EQUAL_STRING("11:22:33:44:55:66", matched_list[0].bssid);
    TEST_ASSERT_EQUAL(3, matched_list[0].channel);
    TEST_ASSERT_TRUE(lisa_kv_get_blob_fake.call_count > 0);

    test_free(matched_list);

    /* Legacy data should be removed after migration attempt */
    int blob_count = 0;
    TEST_ASSERT_NOT_EQUAL(0, lisa_kv_get_int(WIFI_STORAGE_BLOB_COUNT_KEY, &blob_count));
}

void test_wifi_storage_save_ap_ssid_only_should_update_when_ssid_exists(void)
{
    set_kv_item(0, "ExistingSSID", "00:11:22:33:44:55");
    set_kv_count(1);

    wifi_mgr_sta_config_t ap = {0};
    strcpy(ap.ssid, "ExistingSSID");
    strcpy(ap.pwd, "NewPwd");

    TEST_ASSERT_EQUAL(0, wifi_storage_save_ap(&s_ctx, &ap));

    wifi_mgr_sta_config_t *matched_list = NULL;
    int count = wifi_storage_search_ap(&s_ctx, &matched_list, SEARCH_BY_SSID, ap.ssid);
    TEST_ASSERT_EQUAL(1, count);
    TEST_ASSERT_EQUAL_STRING("00:11:22:33:44:55", matched_list[0].bssid);
    TEST_ASSERT_EQUAL_STRING("NewPwd", matched_list[0].pwd);
    test_free(matched_list);
}

void test_wifi_storage_save_ap_with_new_bssid_should_succeed(void)
{
    set_kv_item(0, "ExistingSSID", "00:11:22:33:44:55");
    set_kv_count(1);

    wifi_mgr_sta_config_t ap = {0};
    strcpy(ap.ssid, "ExistingSSID");
    strcpy(ap.bssid, "66:77:88:99:aa:bb");
    strcpy(ap.pwd, "NewPwd");

    TEST_ASSERT_EQUAL(0, wifi_storage_save_ap(&s_ctx, &ap));

    wifi_mgr_sta_config_t *matched_list = NULL;
    int count = wifi_storage_search_ap(&s_ctx, &matched_list, SEARCH_BY_SSID, ap.ssid);
    TEST_ASSERT_EQUAL(2, count);
    test_free(matched_list);
}

void test_wifi_storage_save_ap_with_bssid_should_update_ssid_only_record(void)
{
    set_kv_item(0, "SameSSID", "");
    set_kv_count(1);

    wifi_mgr_sta_config_t ap = {0};
    strcpy(ap.ssid, "SameSSID");
    strcpy(ap.bssid, "66:77:88:99:aa:bb");
    strcpy(ap.pwd, "NewPwd");

    TEST_ASSERT_EQUAL(0, wifi_storage_save_ap(&s_ctx, &ap));

    wifi_mgr_sta_config_t *matched_list = NULL;
    int count = wifi_storage_search_ap(&s_ctx, &matched_list, SEARCH_BY_SSID, ap.ssid);
    TEST_ASSERT_EQUAL(1, count);
    TEST_ASSERT_EQUAL_STRING("66:77:88:99:aa:bb", matched_list[0].bssid);
    TEST_ASSERT_EQUAL_STRING("NewPwd", matched_list[0].pwd);
    test_free(matched_list);
}

static int fail_kv_set_int_on_count_key(const char *key, int value)
{
    if (strcmp(key, WIFI_STORAGE_KV_COUNT_KEY) == 0) {
        return -EIO;
    }
    if (s_kv_set_int_passthrough != NULL) {
        return s_kv_set_int_passthrough(key, value);
    }
    return 0;
}

void test_wifi_storage_migrate_kv_save_fail_should_free_list(void)
{
    wifi_storage_item_legacy_v1_t legacy_items[1] = {0};
    strcpy(legacy_items[0].ap_info.ssid, "LegacySSID");
    legacy_items[0].enable = 1;

    TEST_ASSERT_EQUAL(0, lisa_kv_set_int(WIFI_STORAGE_BLOB_COUNT_KEY, 1));
    TEST_ASSERT_EQUAL(0, lisa_kv_set_blob(WIFI_STORAGE_BLOB_LIST_KEY,
                                         (uint8_t *)legacy_items,
                                         sizeof(legacy_items)));

    wifi_storage_ops_t ops = {
        .malloc = test_malloc,
        .calloc = test_calloc_track,
        .free = test_free_track,
        .mutex_lock = test_mutex_lock,
        .mutex_unlock = test_mutex_unlock,
    };
    wifi_storage_deinit(&s_ctx);
    init_storage_with_ops(&ops);

    lisa_kv_set_int_fake.custom_fake = fail_kv_set_int_on_count_key;
    wifi_storage_cache_drop(&s_ctx);
    lisa_kv_del(WIFI_STORAGE_KV_COUNT_KEY);

    wifi_storage_item_t *out_list = NULL;
    uint32_t out_count = 0;
    int ret = wifi_storage_migrate_load_list(&s_ctx, &out_list, &out_count);

    TEST_ASSERT_NOT_EQUAL(0, ret);
    TEST_ASSERT_NULL(out_list);
    TEST_ASSERT_EQUAL(0u, out_count);
    TEST_ASSERT_EQUAL(s_alloc_count, s_free_count);
}

void test_wifi_storage_init_should_load_cache_once(void)
{
    wifi_storage_deinit(&s_ctx);
    mock_lisa_kv_reset();
    wifi_storage_cache_drop(&s_ctx);

    set_kv_item(0, "CacheSSID0", "00:11:22:33:44:55");
    set_kv_item(1, "CacheSSID1", "66:77:88:99:aa:bb");
    set_kv_count(2);

    init_storage();

    TEST_ASSERT_TRUE(s_ctx.cache.loaded);
    TEST_ASSERT_EQUAL(2u, s_ctx.cache.count);
    TEST_ASSERT_EQUAL(2u, s_ctx.list_item_count);
    TEST_ASSERT_EQUAL_STRING("CacheSSID0", s_ctx.cache.list[0].ap_info.ssid);
    TEST_ASSERT_EQUAL_STRING("CacheSSID1", s_ctx.cache.list[1].ap_info.ssid);
}

void test_wifi_storage_init_should_migrate_blob_on_first_init(void)
{
    wifi_storage_deinit(&s_ctx);
    mock_lisa_kv_reset();
    wifi_storage_cache_drop(&s_ctx);

    wifi_storage_item_legacy_v1_t legacy_items[2] = {0};
    strcpy(legacy_items[0].ap_info.ssid, "LegacyInit1");
    legacy_items[0].enable = 1;
    strcpy(legacy_items[1].ap_info.ssid, "LegacyInit2");
    legacy_items[1].enable = 1;

    TEST_ASSERT_EQUAL(0, lisa_kv_set_int(WIFI_STORAGE_BLOB_COUNT_KEY, 2));
    TEST_ASSERT_EQUAL(0, lisa_kv_set_blob(WIFI_STORAGE_BLOB_LIST_KEY,
                                          (uint8_t *)legacy_items,
                                          sizeof(legacy_items)));

    init_storage();

    int kv_count = 0;
    TEST_ASSERT_EQUAL(0, lisa_kv_get_int(WIFI_STORAGE_KV_COUNT_KEY, &kv_count));
    TEST_ASSERT_EQUAL(2, kv_count);
    TEST_ASSERT_TRUE(s_ctx.cache.loaded);
    TEST_ASSERT_EQUAL(2u, s_ctx.cache.count);
    TEST_ASSERT_EQUAL_STRING("LegacyInit1", s_ctx.cache.list[0].ap_info.ssid);
    TEST_ASSERT_EQUAL_STRING("LegacyInit2", s_ctx.cache.list[1].ap_info.ssid);

    /* Verify legacy data is deleted after migration */
    int blob_count = 0;
    TEST_ASSERT_NOT_EQUAL(0, lisa_kv_get_int(WIFI_STORAGE_BLOB_COUNT_KEY, &blob_count));
    uint8_t *blob = NULL;
    int blob_len = 0;
    TEST_ASSERT_NOT_EQUAL(0, lisa_kv_get_blob(WIFI_STORAGE_BLOB_LIST_KEY, &blob, &blob_len));
}

void test_wifi_storage_init_should_merge_legacy_even_if_kv_exists(void)
{
    wifi_storage_deinit(&s_ctx);
    mock_lisa_kv_reset();
    wifi_storage_cache_drop(&s_ctx);

    /* Setup existing KV data */
    set_kv_item(0, "KVSSID", "");
    set_kv_item(1, "BSSIDPreferred", "11:22:33:44:55:66");
    TEST_ASSERT_EQUAL(0, lisa_kv_set_int("wifi-kv-item-0-channel", 5));
    TEST_ASSERT_EQUAL(0, lisa_kv_set_int("wifi-kv-item-1-channel", 9));
    set_kv_count(2);

    /* Setup legacy data with duplicates and a new item */
    wifi_storage_item_legacy_v1_t legacy_items[3] = {0};
    strcpy(legacy_items[0].ap_info.ssid, "KVSSID"); /* duplicate by SSID */
    legacy_items[0].enable = 1;

    strcpy(legacy_items[1].ap_info.ssid, "OtherName"); /* duplicate by BSSID */
    strcpy(legacy_items[1].ap_info.bssid, "11:22:33:44:55:66");
    legacy_items[1].enable = 1;

    strcpy(legacy_items[2].ap_info.ssid, "LegacyNew"); /* new item */
    strcpy(legacy_items[2].ap_info.bssid, "aa:bb:cc:dd:ee:ff");
    legacy_items[2].ap_info.channel = 13;
    legacy_items[2].enable = 1;

    TEST_ASSERT_EQUAL(0, lisa_kv_set_int(WIFI_STORAGE_BLOB_COUNT_KEY, 3));
    TEST_ASSERT_EQUAL(0, lisa_kv_set_blob(WIFI_STORAGE_BLOB_LIST_KEY,
                                          (uint8_t *)legacy_items,
                                          sizeof(legacy_items)));

    init_storage();

    wifi_mgr_sta_config_t *list = NULL;
    int count = wifi_storage_search_ap(&s_ctx, &list, SEARCH_ALL, NULL);
    TEST_ASSERT_EQUAL(3, count);
    bool found_kv_ssid = false;
    bool found_kv_bssid = false;
    bool found_new = false;
    for (int i = 0; i < count; i++) {
        if (strcmp(list[i].ssid, "KVSSID") == 0) {
            found_kv_ssid = true;
            TEST_ASSERT_EQUAL_STRING("", list[i].bssid);
            TEST_ASSERT_EQUAL(5, list[i].channel);
        }
        if (strcmp(list[i].bssid, "11:22:33:44:55:66") == 0) {
            found_kv_bssid = true;
            TEST_ASSERT_EQUAL_STRING("BSSIDPreferred", list[i].ssid);
            TEST_ASSERT_EQUAL(9, list[i].channel);
        }
        if (strcmp(list[i].ssid, "LegacyNew") == 0) {
            found_new = true;
            TEST_ASSERT_EQUAL_STRING("aa:bb:cc:dd:ee:ff", list[i].bssid);
            TEST_ASSERT_EQUAL(13, list[i].channel);
        }
    }
    TEST_ASSERT_TRUE(found_kv_ssid);
    TEST_ASSERT_TRUE(found_kv_bssid);
    TEST_ASSERT_TRUE(found_new);
    test_free(list);

    /* Legacy data should be deleted after migration */
    int blob_count = 0;
    TEST_ASSERT_NOT_EQUAL(0, lisa_kv_get_int(WIFI_STORAGE_BLOB_COUNT_KEY, &blob_count));
}

static void set_kv_item(int index, const char *ssid, const char *bssid)
{
    char key[64];
    wifi_storage_cache_drop(&s_ctx);
    snprintf(key, sizeof(key), "wifi-kv-item-%d-enable", index);
    TEST_ASSERT_EQUAL(0, lisa_kv_set_bool(key, true));
    snprintf(key, sizeof(key), "wifi-kv-item-%d-ssid", index);
    TEST_ASSERT_EQUAL(0, lisa_kv_set_string(key, ssid));
    snprintf(key, sizeof(key), "wifi-kv-item-%d-bssid", index);
    TEST_ASSERT_EQUAL(0, lisa_kv_set_string(key, bssid));
}

static void set_kv_count(int count)
{
    wifi_storage_cache_drop(&s_ctx);
    TEST_ASSERT_EQUAL(0, lisa_kv_set_int(WIFI_STORAGE_KV_COUNT_KEY, count));
}

void test_wifi_storage_delete_match_ssid_and_bssid(void)
{
    set_kv_item(0, "SameSSID", "00:11:22:33:44:55");
    set_kv_item(1, "SameSSID", "66:77:88:99:aa:bb");
    set_kv_count(2);

    wifi_mgr_sta_config_t ap = {0};
    strcpy(ap.ssid, "SameSSID");
    strcpy(ap.bssid, "66:77:88:99:aa:bb");

    TEST_ASSERT_EQUAL(0, wifi_storage_delete_ap(&s_ctx, &ap));

    wifi_mgr_sta_config_t *matched_list = NULL;
    int count = wifi_storage_search_ap(&s_ctx, &matched_list, SEARCH_BY_SSID, ap.ssid);
    TEST_ASSERT_EQUAL(1, count);
    TEST_ASSERT_EQUAL_STRING("00:11:22:33:44:55", matched_list[0].bssid);
    test_free(matched_list);
}

void test_wifi_storage_delete_match_ssid_only_should_remove_all_ssid(void)
{
    set_kv_item(0, "SameSSID", "00:11:22:33:44:55");
    set_kv_item(1, "SameSSID", "66:77:88:99:aa:bb");
    set_kv_count(2);

    wifi_mgr_sta_config_t ap = {0};
    strcpy(ap.ssid, "SameSSID");

    TEST_ASSERT_EQUAL(0, wifi_storage_delete_ap(&s_ctx, &ap));

    wifi_mgr_sta_config_t *matched_list = NULL;
    int count = wifi_storage_search_ap(&s_ctx, &matched_list, SEARCH_BY_SSID, ap.ssid);
    TEST_ASSERT_EQUAL(0, count);
    TEST_ASSERT_NULL(matched_list);
}

void test_wifi_storage_delete_match_bssid_only_should_remove_one(void)
{
    set_kv_item(0, "SSID1", "00:11:22:33:44:55");
    set_kv_item(1, "SSID2", "66:77:88:99:aa:bb");
    set_kv_count(2);

    wifi_mgr_sta_config_t ap = {0};
    strcpy(ap.bssid, "66:77:88:99:aa:bb");

    TEST_ASSERT_EQUAL(0, wifi_storage_delete_ap(&s_ctx, &ap));

    wifi_mgr_sta_config_t *matched_list = NULL;
    int count = wifi_storage_search_ap(&s_ctx, &matched_list, SEARCH_ALL, NULL);
    TEST_ASSERT_EQUAL(1, count);
    TEST_ASSERT_EQUAL_STRING("00:11:22:33:44:55", matched_list[0].bssid);
    test_free(matched_list);
}

void test_wifi_storage_search_ap_alloc_fail_should_return_enomem(void)
{
    wifi_storage_ops_t ops = {
        .malloc = test_malloc,
        .calloc = test_calloc_fail_bool,
        .free = test_free,
        .mutex_lock = test_mutex_lock,
        .mutex_unlock = test_mutex_unlock,
    };
    wifi_storage_deinit(&s_ctx);
    init_storage_with_ops(&ops);

    set_kv_item(0, "SSID1", "00:11:22:33:44:55");
    set_kv_count(1);

    s_fail_bool_alloc = true;
    wifi_mgr_sta_config_t *matched_list = NULL;
    int count = wifi_storage_search_ap(&s_ctx, &matched_list, SEARCH_ALL, NULL);
    s_fail_bool_alloc = false;

    TEST_ASSERT_EQUAL(-ENOMEM, count);
    TEST_ASSERT_NULL(matched_list);
}

void test_wifi_storage_search_ap_uses_cache_for_repeated_calls(void)
{
    set_kv_item(0, "CachedSSID", "11:22:33:44:55:66");
    set_kv_count(1);

    wifi_mgr_sta_config_t *matched_list = NULL;
    int count = wifi_storage_search_ap(&s_ctx, &matched_list, SEARCH_BY_SSID, "CachedSSID");
    TEST_ASSERT_EQUAL(1, count);
    test_free(matched_list);

    int get_int_calls = lisa_kv_get_int_fake.call_count;
    int get_string_calls = lisa_kv_get_string_fake.call_count;
    int get_blob_calls = lisa_kv_get_blob_fake.call_count;

    matched_list = NULL;
    count = wifi_storage_search_ap(&s_ctx, &matched_list, SEARCH_BY_SSID, "CachedSSID");
    TEST_ASSERT_EQUAL(1, count);
    test_free(matched_list);

    TEST_ASSERT_EQUAL(get_int_calls, lisa_kv_get_int_fake.call_count);
    TEST_ASSERT_EQUAL(get_string_calls, lisa_kv_get_string_fake.call_count);
    TEST_ASSERT_EQUAL(get_blob_calls, lisa_kv_get_blob_fake.call_count);
}

void test_wifi_storage_empty_cache_should_not_reload_kv(void)
{
    mock_lisa_kv_reset();
    wifi_storage_cache_drop(&s_ctx);

    /* Explicitly set empty KV count */
    TEST_ASSERT_EQUAL(0, lisa_kv_set_int(WIFI_STORAGE_KV_COUNT_KEY, 0));

    int get_int_calls_before = lisa_kv_get_int_fake.call_count;

    wifi_mgr_sta_config_t *matched_list = NULL;
    int count = wifi_storage_search_ap(&s_ctx, &matched_list, SEARCH_ALL, NULL);
    TEST_ASSERT_EQUAL(0, count);
    TEST_ASSERT_NULL(matched_list);

    int get_int_calls_after_first = lisa_kv_get_int_fake.call_count;

    matched_list = NULL;
    count = wifi_storage_search_ap(&s_ctx, &matched_list, SEARCH_ALL, NULL);
    TEST_ASSERT_EQUAL(0, count);
    TEST_ASSERT_NULL(matched_list);

    TEST_ASSERT_EQUAL(get_int_calls_after_first, lisa_kv_get_int_fake.call_count);
    TEST_ASSERT_TRUE(s_ctx.cache.loaded);
    TEST_ASSERT_TRUE(s_ctx.cache.loaded);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_wifi_storage_kv_round_trip);
    RUN_TEST(test_wifi_storage_migrate_blob_to_kv);
    RUN_TEST(test_wifi_storage_kv_preferred_over_blob);
    RUN_TEST(test_wifi_storage_save_ap_ssid_only_should_update_when_ssid_exists);
    RUN_TEST(test_wifi_storage_save_ap_with_new_bssid_should_succeed);
    RUN_TEST(test_wifi_storage_save_ap_with_bssid_should_update_ssid_only_record);
    RUN_TEST(test_wifi_storage_migrate_kv_save_fail_should_free_list);
    RUN_TEST(test_wifi_storage_init_should_load_cache_once);
    RUN_TEST(test_wifi_storage_init_should_migrate_blob_on_first_init);
    RUN_TEST(test_wifi_storage_init_should_merge_legacy_even_if_kv_exists);
    RUN_TEST(test_wifi_storage_delete_match_ssid_and_bssid);
    RUN_TEST(test_wifi_storage_delete_match_ssid_only_should_remove_all_ssid);
    RUN_TEST(test_wifi_storage_delete_match_bssid_only_should_remove_one);
    RUN_TEST(test_wifi_storage_search_ap_alloc_fail_should_return_enomem);
    RUN_TEST(test_wifi_storage_search_ap_uses_cache_for_repeated_calls);
    return UNITY_END();
}

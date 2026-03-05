#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include "wifi_manager_storage_internal.h"
#include "lisa_kv.h"

static bool wifi_storage_migrate_is_duplicate(const wifi_storage_item_t *list, uint32_t count,
                                              const wifi_storage_item_t *candidate)
{
    bool use_bssid = candidate->ap_info.bssid[0] != '\0';
    for (uint32_t i = 0; i < count; i++) {
        const wifi_storage_item_t *item = &list[i];
        if (use_bssid) {
            if (strcmp(item->ap_info.bssid, candidate->ap_info.bssid) == 0) {
                return true;
            }
        } else {
            if (strcmp(item->ap_info.ssid, candidate->ap_info.ssid) == 0) {
                return true;
            }
        }
    }
    return false;
}

static uint32_t wifi_storage_migrate_filter_nonempty_ssid(wifi_storage_item_t *list, uint32_t count)
{
    uint32_t valid_count = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (list[i].ap_info.ssid[0] == '\0') {
            continue;
        }
        if (valid_count != i) {
            list[valid_count] = list[i];
        }
        valid_count++;
    }
    return valid_count;
}

int wifi_storage_migrate_load_count(uint32_t *out_count)
{
    int count = 0;

    if (out_count == NULL) {
        return -EINVAL;
    }

    if (lisa_kv_get_int(WIFI_STORAGE_KV_COUNT_KEY, &count) == 0) {
        *out_count = count > 0 ? (uint32_t)count : 0;
        return 0;
    }

    if (lisa_kv_get_int(WIFI_STORAGE_BLOB_COUNT_KEY, &count) == 0) {
        *out_count = count > 0 ? (uint32_t)count : 0;
        return 0;
    }

    *out_count = 0;
    return -ENOENT;
}

int wifi_storage_migrate_load_list(wifi_storage_ctx_t *ctx, wifi_storage_item_t **out_list, uint32_t *out_count)
{
    wifi_storage_item_t *legacy_list = NULL;
    uint32_t legacy_count = 0;
    wifi_storage_item_t *kv_list = NULL;
    uint32_t kv_count = 0;
    wifi_storage_item_t *merged_list = NULL;
    uint32_t merged_count = 0;
    int ret = 0;

    ret = wifi_storage_blob_legacy_load_list(ctx, &legacy_list, &legacy_count);
    if (ret != 0) {
        return ret;
    }

    legacy_count = wifi_storage_migrate_filter_nonempty_ssid(legacy_list, legacy_count);

    ret = wifi_storage_kv_load_list(ctx, &kv_list, &kv_count);
    if (ret != 0 && ret != -ENOENT) {
        WIFI_STORAGE_FREE(ctx, legacy_list);
        return ret;
    }
    if (ret != 0) {
        kv_list = NULL;
        kv_count = 0;
    }
    kv_count = wifi_storage_migrate_filter_nonempty_ssid(kv_list, kv_count);

    /* Filter out duplicates against KV list and within legacy list */
    uint32_t valid_count = 0;
    for (uint32_t i = 0; i < legacy_count; i++) {
        if (wifi_storage_migrate_is_duplicate(kv_list, kv_count, &legacy_list[i])) {
            continue;
        }
        if (wifi_storage_migrate_is_duplicate(legacy_list, valid_count, &legacy_list[i])) {
            continue;
        }
        if (valid_count != i) {
            legacy_list[valid_count] = legacy_list[i];
        }
        valid_count++;
    }
    legacy_count = valid_count;

    merged_count = kv_count + legacy_count;
    if (merged_count > 0) {
        merged_list = WIFI_STORAGE_CALLOC(ctx, merged_count, sizeof(wifi_storage_item_t));
        if (merged_list == NULL) {
            WIFI_STORAGE_FREE(ctx, kv_list);
            WIFI_STORAGE_FREE(ctx, legacy_list);
            return -ENOMEM;
        }
        if (kv_count > 0) {
            memcpy(merged_list, kv_list, kv_count * sizeof(wifi_storage_item_t));
        }
        if (legacy_count > 0) {
            memcpy(merged_list + kv_count, legacy_list, legacy_count * sizeof(wifi_storage_item_t));
        }
    }

    ret = wifi_storage_kv_save_list(ctx, merged_list, merged_count);
    WIFI_STORAGE_FREE(ctx, kv_list);
    WIFI_STORAGE_FREE(ctx, legacy_list);
    if (ret != 0) {
        WIFI_STORAGE_FREE(ctx, merged_list);
        return ret;
    }

    if (out_list != NULL) {
        *out_list = merged_list;
    } else {
        WIFI_STORAGE_FREE(ctx, merged_list);
    }
    if (out_count != NULL) {
        *out_count = merged_count;
    }

    return 0;
}

void wifi_storage_migrate_delete_legacy(void)
{
    lisa_kv_del(WIFI_STORAGE_BLOB_COUNT_KEY);
    lisa_kv_del(WIFI_STORAGE_BLOB_LIST_KEY);
}

#include <errno.h>
#include <string.h>

#include "wifi_manager_storage_internal.h"
#include "lisa_kv.h"

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

static void wifi_storage_copy_ap_info(wifi_mgr_sta_config_t *dst, const void *src, size_t src_size)
{
    size_t copy_len = sizeof(*dst);
    if (copy_len > src_size) {
        copy_len = src_size;
    }
    memset(dst, 0, sizeof(*dst));
    memcpy(dst, src, copy_len);
}

int wifi_storage_blob_legacy_load_list(wifi_storage_ctx_t *ctx, wifi_storage_item_t **out_list, uint32_t *out_count)
{
    uint8_t *data = NULL;
    int len = 0;
    int count = 0;
    int r;

    if (out_list == NULL || out_count == NULL) {
        return -EINVAL;
    }

    *out_list = NULL;
    *out_count = 0;

    r = lisa_kv_get_int(WIFI_STORAGE_BLOB_COUNT_KEY, &count);
    if (r != 0 || count <= 0) {
        return -ENOENT;
    }

    r = lisa_kv_get_blob(WIFI_STORAGE_BLOB_LIST_KEY, &data, &len);
    if (r != 0 || data == NULL || len <= 0) {
        lisa_kv_free(data);
        return -ENOENT;
    }

    size_t expected_len = (size_t)count * sizeof(wifi_storage_item_t);
    size_t legacy_v1_len = (size_t)count * sizeof(wifi_storage_item_legacy_v1_t);

    wifi_storage_item_t *list = WIFI_STORAGE_CALLOC(ctx, count, sizeof(wifi_storage_item_t));
    if (list == NULL) {
        lisa_kv_free(data);
        return -ENOMEM;
    }

    if ((size_t)len == expected_len) {
        memcpy(list, data, (size_t)len);
    } else if ((size_t)len == legacy_v1_len) {
        const wifi_storage_item_legacy_v1_t *legacy = (const wifi_storage_item_legacy_v1_t *)data;
        for (int i = 0; i < count; i++) {
            list[i].version = WIFI_STORAGE_VERSION_LEGACY;
            list[i].enable = legacy[i].enable;
            wifi_storage_copy_ap_info(&list[i].ap_info, &legacy[i].ap_info, sizeof(legacy[i].ap_info));
            list[i].ap_info.pmk_valid = 0;
        }
    } else {
        WIFI_STORAGE_FREE(ctx, list);
        lisa_kv_free(data);
        return -EIO;
    }

    lisa_kv_free(data);

    for (int i = 0; i < count; i++) {
        if (list[i].version < WIFI_STORAGE_VERSION_WITH_PMK) {
            list[i].ap_info.pmk_valid = 0;
        }
    }

    *out_list = list;
    *out_count = (uint32_t)count;

    return 0;
}

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "wifi_manager_storage_internal.h"
#include "lisa_kv.h"

#define WIFI_STORAGE_KV_KEY_MAX_LEN 96

static int wifi_storage_kv_make_item_key(char *buf, size_t buf_len, uint32_t index, const char *field)
{
    int written = snprintf(buf, buf_len, "%s-%u-%s", WIFI_STORAGE_KV_ITEM_PREFIX, index, field);
    if (written < 0 || (size_t)written >= buf_len) {
        return -ENAMETOOLONG;
    }
    return 0;
}

static void wifi_storage_kv_copy_string(char *dst, size_t dst_len, const char *src)
{
    if (dst_len == 0) {
        return;
    }
    strncpy(dst, src, dst_len - 1);
    dst[dst_len - 1] = '\0';
}

static void wifi_storage_kv_read_string(const char *key, char *dst, size_t dst_len)
{
    char *value = NULL;
    if (lisa_kv_get_string(key, &value) == 0) {
        wifi_storage_kv_copy_string(dst, dst_len, value);
        lisa_kv_free(value);
    } else if (dst_len > 0) {
        dst[0] = '\0';
    }
}

static void wifi_storage_kv_read_int(const char *key, int *value, int default_value)
{
    if (lisa_kv_get_int(key, value) != 0) {
        *value = default_value;
    }
}

static void wifi_storage_kv_read_bool(const char *key, bool *value, bool default_value)
{
    if (lisa_kv_get_bool(key, value) != 0) {
        *value = default_value;
    }
}

static void wifi_storage_kv_read_item(uint32_t index, wifi_storage_item_t *item)
{
    char key[WIFI_STORAGE_KV_KEY_MAX_LEN];
    int int_value = 0;

    memset(item, 0, sizeof(*item));
    item->version = WIFI_STORAGE_VERSION_CURRENT;

    if (wifi_storage_kv_make_item_key(key, sizeof(key), index, "version") == 0) {
        wifi_storage_kv_read_int(key, &int_value, WIFI_STORAGE_VERSION_CURRENT);
        item->version = (uint8_t)int_value;
    }

    if (wifi_storage_kv_make_item_key(key, sizeof(key), index, "enable") == 0) {
        bool enable = false;
        wifi_storage_kv_read_bool(key, &enable, false);
        item->enable = enable;
    }

    if (wifi_storage_kv_make_item_key(key, sizeof(key), index, "ssid") == 0) {
        wifi_storage_kv_read_string(key, item->ap_info.ssid, sizeof(item->ap_info.ssid));
    }
    if (wifi_storage_kv_make_item_key(key, sizeof(key), index, "pwd") == 0) {
        wifi_storage_kv_read_string(key, item->ap_info.pwd, sizeof(item->ap_info.pwd));
    }
    if (wifi_storage_kv_make_item_key(key, sizeof(key), index, "bssid") == 0) {
        wifi_storage_kv_read_string(key, item->ap_info.bssid, sizeof(item->ap_info.bssid));
    }
    if (wifi_storage_kv_make_item_key(key, sizeof(key), index, "channel") == 0) {
        wifi_storage_kv_read_int(key, &item->ap_info.channel, 0);
    }
    if (wifi_storage_kv_make_item_key(key, sizeof(key), index, "rssi") == 0) {
        wifi_storage_kv_read_int(key, &item->ap_info.rssi, 0);
    }
    if (wifi_storage_kv_make_item_key(key, sizeof(key), index, "encryption") == 0) {
        wifi_storage_kv_read_int(key, &int_value, WIFI_MGR_WIFI_AUTH_AUTO);
        item->ap_info.encryption_mode = (wifi_mgr_wifi_encryption_mode_t)int_value;
    }

    if (wifi_storage_kv_make_item_key(key, sizeof(key), index, "pmk_valid") == 0) {
        bool pmk_valid = false;
        wifi_storage_kv_read_bool(key, &pmk_valid, false);
        item->ap_info.pmk_valid = pmk_valid;
    }

    if (item->ap_info.pmk_valid &&
        wifi_storage_kv_make_item_key(key, sizeof(key), index, "pmk") == 0) {
        uint8_t *pmk_blob = NULL;
        int pmk_len = 0;
        if (lisa_kv_get_blob(key, &pmk_blob, &pmk_len) == 0 && pmk_len == sizeof(item->ap_info.pmk)) {
            memcpy(item->ap_info.pmk, pmk_blob, sizeof(item->ap_info.pmk));
        } else {
            item->ap_info.pmk_valid = 0;
        }
        lisa_kv_free(pmk_blob);
    }

    if (item->version < WIFI_STORAGE_VERSION_WITH_PMK) {
        item->ap_info.pmk_valid = 0;
    }
}

static int wifi_storage_kv_write_string(uint32_t index, const char *field, const char *value)
{
    char key[WIFI_STORAGE_KV_KEY_MAX_LEN];
    if (wifi_storage_kv_make_item_key(key, sizeof(key), index, field) != 0) {
        return -ENAMETOOLONG;
    }
    return lisa_kv_set_string(key, value ? value : "");
}

static int wifi_storage_kv_write_int(uint32_t index, const char *field, int value)
{
    char key[WIFI_STORAGE_KV_KEY_MAX_LEN];
    if (wifi_storage_kv_make_item_key(key, sizeof(key), index, field) != 0) {
        return -ENAMETOOLONG;
    }
    return lisa_kv_set_int(key, value);
}

static int wifi_storage_kv_write_bool(uint32_t index, const char *field, bool value)
{
    char key[WIFI_STORAGE_KV_KEY_MAX_LEN];
    if (wifi_storage_kv_make_item_key(key, sizeof(key), index, field) != 0) {
        return -ENAMETOOLONG;
    }
    return lisa_kv_set_bool(key, value);
}

static int wifi_storage_kv_write_blob(uint32_t index, const char *field, const uint8_t *data, int len)
{
    char key[WIFI_STORAGE_KV_KEY_MAX_LEN];
    if (wifi_storage_kv_make_item_key(key, sizeof(key), index, field) != 0) {
        return -ENAMETOOLONG;
    }
    if (data == NULL || len <= 0) {
        return lisa_kv_del(key);
    }
    return lisa_kv_set_blob(key, (uint8_t *)data, len);
}

int wifi_storage_kv_load_list(wifi_storage_ctx_t *ctx, wifi_storage_item_t **out_list, uint32_t *out_count)
{
    int count = 0;
    (void)ctx;

    if (out_list == NULL || out_count == NULL) {
        return -EINVAL;
    }

    *out_list = NULL;
    *out_count = 0;

    if (lisa_kv_get_int(WIFI_STORAGE_KV_COUNT_KEY, &count) != 0) {
        return -ENOENT;
    }

    if (count <= 0) {
        return 0;
    }

    wifi_storage_item_t *list = WIFI_STORAGE_CALLOC(ctx, count, sizeof(wifi_storage_item_t));
    if (list == NULL) {
        return -ENOMEM;
    }

    for (int idx = 0; idx < count; idx++) {
        wifi_storage_kv_read_item((uint32_t)idx, &list[idx]);
    }

    *out_list = list;
    *out_count = (uint32_t)count;

    return 0;
}

int wifi_storage_kv_save_list(wifi_storage_ctx_t *ctx, const wifi_storage_item_t *list, uint32_t count)
{
    int ret = 0;
    (void)ctx;

    if (lisa_kv_set_int(WIFI_STORAGE_KV_COUNT_KEY, (int)count) != 0) {
        return -EIO;
    }

    for (uint32_t idx = 0; idx < count; idx++) {
        const wifi_storage_item_t *item = &list[idx];
        ret = wifi_storage_kv_write_int(idx, "version", item->version);
        if (ret != 0) {
            return ret;
        }
        ret = wifi_storage_kv_write_bool(idx, "enable", item->enable);
        if (ret != 0) {
            return ret;
        }
        ret = wifi_storage_kv_write_string(idx, "ssid", item->ap_info.ssid);
        if (ret != 0) {
            return ret;
        }
        ret = wifi_storage_kv_write_string(idx, "pwd", item->ap_info.pwd);
        if (ret != 0) {
            return ret;
        }
        ret = wifi_storage_kv_write_string(idx, "bssid", item->ap_info.bssid);
        if (ret != 0) {
            return ret;
        }
        ret = wifi_storage_kv_write_int(idx, "channel", item->ap_info.channel);
        if (ret != 0) {
            return ret;
        }
        ret = wifi_storage_kv_write_int(idx, "rssi", item->ap_info.rssi);
        if (ret != 0) {
            return ret;
        }
        ret = wifi_storage_kv_write_int(idx, "encryption", (int)item->ap_info.encryption_mode);
        if (ret != 0) {
            return ret;
        }

        ret = wifi_storage_kv_write_bool(idx, "pmk_valid", item->ap_info.pmk_valid);
        if (ret != 0) {
            return ret;
        }
        if (item->ap_info.pmk_valid) {
            ret = wifi_storage_kv_write_blob(idx, "pmk", item->ap_info.pmk, sizeof(item->ap_info.pmk));
        } else {
            ret = wifi_storage_kv_write_blob(idx, "pmk", NULL, 0);
        }
        if (ret != 0) {
            return ret;
        }
    }

    return 0;
}

int wifi_storage_kv_clear_list(wifi_storage_ctx_t *ctx)
{
    (void)ctx;
    return lisa_kv_del(WIFI_STORAGE_KV_COUNT_KEY);
}

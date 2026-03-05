#include <stdbool.h>
#include <string.h>
#include <errno.h>

#include "wifi_manager_storage_internal.h"

#define TAG "wifi_storage"
#include "lisa_log.h"

#define WIFI_STORAGE_CHECK(cond, ret_val) ({                                 \
    if (!(cond)) {                                                           \
        LISA_LOGE(TAG, "Check failed at %s(%d)", __FUNCTION__, __LINE__);           \
        return (ret_val);                                                    \
    }                                                                        \
})

#define WIFI_STORAGE_MUTEX_LOCK(ctx) do {                                   \
    (ctx)->ops.mutex_lock((ctx)->mutex, 0xFFFFFFFFU);                       \
} while (0)

#define WIFI_STORAGE_MUTEX_UNLOCK(ctx) do {                                 \
    (ctx)->ops.mutex_unlock((ctx)->mutex);                                  \
} while (0)

int wifi_storage_search_ap(wifi_storage_ctx_t *ctx, wifi_mgr_sta_config_t **matched_list,
                           wifi_mgr_storage_search_mode_t search_modes, void* target);
static int wifi_storage_ensure_cache(wifi_storage_ctx_t *ctx);

static int wifi_storage_load_list_or_empty(wifi_storage_ctx_t *ctx, wifi_storage_item_t **list, uint32_t *count)
{
    int ret = wifi_storage_ensure_cache(ctx);
    if (ret != 0) {
        return ret;
    }

    wifi_storage_cache_get(ctx, list, count);
    return 0;
}

static wifi_storage_item_t *wifi_storage_resize_list(wifi_storage_ctx_t *ctx,
                                                     wifi_storage_item_t *list,
                                                     uint32_t old_count,
                                                     uint32_t new_count)
{
    wifi_storage_item_t *new_list = NULL;

    if (new_count == 0) {
        WIFI_STORAGE_FREE(ctx, list);
        return NULL;
    }

    new_list = WIFI_STORAGE_CALLOC(ctx, new_count, sizeof(wifi_storage_item_t));
    if (new_list == NULL) {
        return NULL;
    }

    if (list != NULL && old_count > 0) {
        memcpy(new_list, list, old_count * sizeof(wifi_storage_item_t));
        WIFI_STORAGE_FREE(ctx, list);
    }

    return new_list;
}

static int wifi_storage_persist_list(wifi_storage_ctx_t *ctx, wifi_storage_item_t *info_list, uint32_t info_num)
{
    int ret;

    WIFI_STORAGE_MUTEX_LOCK(ctx);
    ret = wifi_storage_kv_save_list(ctx, info_list, info_num);
    if (ret == 0) {
        ctx->list_item_count = info_num;
        wifi_storage_cache_set(ctx, info_list, info_num);
    }
    WIFI_STORAGE_MUTEX_UNLOCK(ctx);

    return ret;
}

static int wifi_storage_replace_item(wifi_storage_ctx_t *ctx, wifi_storage_item_t *info_list,
                                    const wifi_mgr_sta_config_t *item)
{
    int ret = 0;
    uint32_t info_num = ctx->list_item_count;

    for (int idx = 0; idx < info_num; idx++) {
        if (info_list[idx].enable) {
            if (strcmp(info_list[idx].ap_info.ssid, item->ssid) == 0) {
                memcpy(&(info_list[idx].ap_info), item, sizeof(wifi_mgr_sta_config_t));
                info_list[idx].version = WIFI_STORAGE_VERSION_CURRENT;
                ret = 0;
                break;
            } else {
                ret = -EIO;
            }
        } else {
            ret = -EIO;
        }
    }

    if (wifi_storage_persist_list(ctx, info_list, info_num) != 0) {
        return -EIO;
    }

    return ret;
}

static int wifi_storage_add_item(wifi_storage_ctx_t *ctx, wifi_storage_item_t *info_list, uint32_t *info_num,
                                    const wifi_mgr_sta_config_t *item)
{
    bool found = false;
    for (int idx = 0; idx < *info_num; idx++) {
        if (!info_list[idx].enable) {
            memcpy(&info_list[idx].ap_info, item, sizeof(wifi_mgr_sta_config_t));
            info_list[idx].enable = true;
            info_list[idx].version = WIFI_STORAGE_VERSION_CURRENT;
            found = true;
            break;
        }
    }
    if (!found) {
        memcpy(&info_list[*info_num].ap_info, item, sizeof(wifi_mgr_sta_config_t));
        info_list[*info_num].enable = true;
        info_list[*info_num].version = WIFI_STORAGE_VERSION_CURRENT;
        (*info_num)++;
    }

    if (wifi_storage_persist_list(ctx, info_list, *info_num) != 0) {
        return -EIO;
    }

    LISA_LOGI(TAG,"[%s %d]Add wifi ap info_num:%d",__FUNCTION__,__LINE__, *info_num);
    return 0;
}

static int wifi_storage_remove_item(wifi_storage_ctx_t *ctx, wifi_storage_item_t *info_list, uint32_t info_num,
                                    const wifi_mgr_sta_config_t *item, bool match_ssid, bool match_bssid)
{
    bool found = false;
    for (int idx = 0; idx < info_num; idx++) {
        bool ssid_match = !match_ssid || strcmp(info_list[idx].ap_info.ssid, item->ssid) == 0;
        bool bssid_match = !match_bssid || strcmp(info_list[idx].ap_info.bssid, item->bssid) == 0;
        if (ssid_match && bssid_match) {
            found = true;
            info_list[idx].enable = false;
        }
    }
    if (!found) {
        LISA_LOGW(TAG, "Item not found");
        return -ENOENT;
    }

    if (wifi_storage_persist_list(ctx, info_list, info_num) != 0) {
        return -EIO;
    }

    return 0;
}

static void wifi_storage_update_ap_info(wifi_mgr_sta_config_t *dst, const wifi_mgr_sta_config_t *src)
{
    char bssid_backup[sizeof(dst->bssid)];
    bool keep_bssid = (src->bssid[0] == '\0');

    if (keep_bssid) {
        memcpy(bssid_backup, dst->bssid, sizeof(dst->bssid));
    }

    memcpy(dst, src, sizeof(*dst));

    if (keep_bssid) {
        memcpy(dst->bssid, bssid_backup, sizeof(dst->bssid));
    }
}

static int wifi_storage_calculate_modes(wifi_mgr_storage_search_mode_t search_modes)
{
    int ret = 0;
    if (search_modes & SEARCH_BY_SSID) {
        ret++;
    }
    if (search_modes & SEARCH_BY_BSSID) {
        ret++;
    }
    if (search_modes & SEARCH_BY_CHANNEL) {
        ret++;
    }
    if (search_modes & SEARCH_BY_PWD) {
        ret++;
    }
    if (search_modes & SEARCH_BY_ENCRYPTION) {
        ret++;
    }
    return ret;
}

static int wifi_storage_search_all_hit(wifi_storage_item_t *info_list, uint32_t info_num, bool *hit_array)
{
    int hit_count = 0;
    for (int idx = 0; idx < info_num; idx++) {
        if (info_list[idx].enable) {
            hit_array[idx] = true;
            hit_count++;
        } else {
            hit_array[idx] = false;
        }
    }
    return hit_count;
}

typedef bool (*wifi_field_matcher_t)(const wifi_storage_item_t *item, const void *target);

static int wifi_storage_search_hit(
    wifi_storage_item_t *info_list, uint32_t info_num,
    bool *hit_array, const void *target,
    wifi_field_matcher_t matcher)
{
    int hit_count = 0;
    for (int idx = 0; idx < info_num; idx++) {
        if (info_list[idx].enable) {
            if (matcher(&info_list[idx], target) && hit_array[idx] == true) {
                hit_array[idx] = true;
                hit_count++;
            } else {
                hit_array[idx] = false;
            }
        } else {
            hit_array[idx] = false;
        }
    }
    return hit_count;
}

static bool match_bssid(const wifi_storage_item_t *item, const void *target) {
    return strcmp(item->ap_info.bssid, (const char *)target) == 0;
}

static bool match_pwd(const wifi_storage_item_t *item, const void *target) {
    return strcmp(item->ap_info.pwd, (const char *)target) == 0;
}

static bool match_channel(const wifi_storage_item_t *item, const void *target) {
    return item->ap_info.channel == *(const uint8_t *)target;
}

static int wifi_storage_search_ssid_hit(wifi_storage_item_t *info_list, uint32_t info_num,
                                        bool *hit_array, char *target)
{
    int hit_count = 0;
    for (int idx = 0; idx < info_num; idx++) {
        if (info_list[idx].enable) {
            if (strcmp(info_list[idx].ap_info.ssid, target) == 0 &&
                hit_array[idx] == true) {
                hit_array[idx] = true;
                hit_count++;
            } else {
                hit_array[idx] = false;
            }
        } else {
            hit_array[idx] = false;
        }
    }
    return hit_count;
}

static int wifi_storage_search_encryption_hit(wifi_storage_item_t *info_list, uint32_t info_num,
                                                bool *hit_array, uint8_t target)
{
    int hit_count = 0;
    for (int idx = 0; idx < info_num; idx++) {
        if (info_list[idx].enable) {
            if (info_list[idx].ap_info.encryption_mode == target && hit_array[idx] == true) {
                hit_array[idx] = true;
                hit_count++;
            } else {
                hit_array[idx] = false;
            }
        } else {
            hit_array[idx] = false;
        }
    }
    return hit_count;
}

static int wifi_storage_create_matched_list(wifi_storage_item_t *info_list, uint32_t info_num,
                                            wifi_mgr_sta_config_t *matched_list, bool *hit_array)
{
    int matched_index = 0;
    for (int idx = info_num - 1; idx >= 0; idx--) {
        if(hit_array[idx]) {
            memcpy(&matched_list[matched_index++], &info_list[idx].ap_info, sizeof(wifi_mgr_sta_config_t));
        }
    }
    return 0;
}

static int wifi_storage_ensure_cache(wifi_storage_ctx_t *ctx)
{
    WIFI_STORAGE_CHECK(ctx != NULL, -EINVAL);

    if (wifi_storage_cache_valid(ctx)) {
        return 0;
    }

    wifi_storage_item_t *list = NULL;
    uint32_t count = 0;
    int ret = wifi_storage_kv_load_list(ctx, &list, &count);

    if (ret == -ENOENT) {
        wifi_storage_cache_set(ctx, NULL, 0);
        WIFI_STORAGE_MUTEX_LOCK(ctx);
        ctx->list_item_count = 0;
        WIFI_STORAGE_MUTEX_UNLOCK(ctx);
        return 0;
    }
    if (ret != 0) {
        return ret;
    }

    wifi_storage_cache_set(ctx, list, count);
    WIFI_STORAGE_MUTEX_LOCK(ctx);
    ctx->list_item_count = count;
    WIFI_STORAGE_MUTEX_UNLOCK(ctx);
    return 0;
}

int wifi_storage_init(wifi_storage_ctx_t *ctx, wifi_storage_ops_t *ops, void *mutex)
{
    WIFI_STORAGE_CHECK(ctx != NULL, -EINVAL);
    WIFI_STORAGE_CHECK(ops != NULL, -EINVAL);
    WIFI_STORAGE_CHECK(mutex != NULL, -EINVAL);
    memcpy(&ctx->ops, ops, sizeof(wifi_storage_ops_t));
    ctx->mutex = mutex;
    wifi_storage_cache_init_ctx(ctx);

    /* Migrate legacy data if it exists, even when KV already has entries */
    if (wifi_storage_migrate_load_list(ctx, NULL, NULL) == 0) {
        LOGI("Migrated from legacy blob to KV, remove old data");
        wifi_storage_migrate_delete_legacy();
    }

    /* Load from KV */
    wifi_storage_item_t *list = NULL;
    uint32_t count = 0;
    int ret = wifi_storage_kv_load_list(ctx, &list, &count);
    if (ret == 0) {
        wifi_storage_cache_set(ctx, list, count);
        WIFI_STORAGE_MUTEX_LOCK(ctx);
        ctx->list_item_count = count;
        WIFI_STORAGE_MUTEX_UNLOCK(ctx);
    } else {
        /* No data or error, start with empty cache */
        wifi_storage_cache_set(ctx, NULL, 0);
        WIFI_STORAGE_MUTEX_LOCK(ctx);
        ctx->list_item_count = 0;
        WIFI_STORAGE_MUTEX_UNLOCK(ctx);
    }

    return 0;
}

int wifi_storage_deinit(wifi_storage_ctx_t *ctx)
{
    WIFI_STORAGE_CHECK(ctx != NULL, -EINVAL);

    wifi_storage_cache_drop(ctx);
    
    WIFI_STORAGE_MUTEX_LOCK(ctx);
    void *mutex_backup = ctx->mutex;
    wifi_storage_ops_t ops_backup = ctx->ops;
    memset(ctx, 0, sizeof(wifi_storage_ctx_t));
    // Restore ops temporarily to unlock
    ctx->mutex = mutex_backup;
    ctx->ops = ops_backup;
    WIFI_STORAGE_MUTEX_UNLOCK(ctx);
    
    // Final clear
    memset(ctx, 0, sizeof(wifi_storage_ctx_t));
    
    return 0;
}

int wifi_storage_search_ap(wifi_storage_ctx_t *ctx, wifi_mgr_sta_config_t **matched_list,
                           wifi_mgr_storage_search_mode_t search_modes, void* target)
{
    WIFI_STORAGE_CHECK(ctx != NULL, -EINVAL);
    WIFI_STORAGE_CHECK(matched_list != NULL, -EINVAL);
    if (!(search_modes & SEARCH_ALL)) {
        WIFI_STORAGE_CHECK(target != NULL, -EINVAL);
    }

    uint32_t info_num = 0;
    wifi_storage_item_t *wifi_info_list = NULL;
    int load_ret = wifi_storage_load_list_or_empty(ctx, &wifi_info_list, &info_num);
    if (load_ret != 0 || info_num == 0 || wifi_info_list == NULL) {
        WIFI_STORAGE_MUTEX_LOCK(ctx);
        ctx->list_item_count = 0;
        WIFI_STORAGE_MUTEX_UNLOCK(ctx);
        return 0;
    }

    bool *hit_array = WIFI_STORAGE_CALLOC(ctx, info_num, sizeof(bool));
    if (hit_array == NULL) {
        return -ENOMEM;
    }
    for (int idx = 0; idx < info_num; idx++) {
        hit_array[idx] = true;
    }

    int hit_count = 0;
    if (search_modes & SEARCH_ALL) {
        hit_count = wifi_storage_search_all_hit(wifi_info_list, info_num, hit_array);
        goto __out;
    }

    char *target_ssid;
    char *target_bssid;
    char *target_pwd;
    int target_channel;
    int target_encryption;
    if (wifi_storage_calculate_modes(search_modes) > 1) {
        wifi_mgr_sta_config_t *target_temp = target;
        target_ssid = target_temp->ssid;
        target_bssid = target_temp->bssid;
        target_pwd = target_temp->pwd;
        target_channel = target_temp->channel;
        target_encryption = target_temp->encryption_mode;
    } else {
        target_ssid = target;
        target_bssid = target;
        target_pwd = target;
        target_channel = 0;
        target_encryption = 0;
        if (search_modes & SEARCH_BY_CHANNEL) {
            target_channel = *(int *)target;
        }
        if (search_modes & SEARCH_BY_ENCRYPTION) {
            target_encryption = *(int *)target;
        }
    }

    if (search_modes & SEARCH_BY_SSID) {
        hit_count = wifi_storage_search_ssid_hit(wifi_info_list, info_num, hit_array, target_ssid);
    }
    if (search_modes & SEARCH_BY_BSSID) {
        hit_count = wifi_storage_search_hit(wifi_info_list, info_num, hit_array, target_bssid, match_bssid);
    }
    if (search_modes & SEARCH_BY_PWD) {
        hit_count = wifi_storage_search_hit(wifi_info_list, info_num, hit_array, target_pwd, match_pwd);
    }
    if (search_modes & SEARCH_BY_CHANNEL) {
        hit_count = wifi_storage_search_hit(wifi_info_list, info_num, hit_array, &target_channel, match_channel);
    }
    if (search_modes & SEARCH_BY_ENCRYPTION) {
        hit_count = wifi_storage_search_encryption_hit(wifi_info_list, info_num, hit_array, target_encryption);
    }

__out:
    if (hit_count == 0) {
        *matched_list = NULL;
        WIFI_STORAGE_FREE(ctx, hit_array);
        return 0;
    }
    *matched_list = WIFI_STORAGE_CALLOC(ctx, hit_count, sizeof(wifi_mgr_sta_config_t));
    if (*matched_list == NULL) {
        WIFI_STORAGE_FREE(ctx, hit_array);
        return -ENOMEM;
    }
    wifi_storage_create_matched_list(wifi_info_list, info_num, *matched_list, hit_array);
    WIFI_STORAGE_FREE(ctx, hit_array);

    return hit_count;
}

int wifi_storage_save_ap(wifi_storage_ctx_t *ctx, wifi_mgr_sta_config_t *ap_info)
{
    WIFI_STORAGE_CHECK(ctx != NULL, -EINVAL);
    WIFI_STORAGE_CHECK(ap_info != NULL, -EINVAL);

    int ret = 0;
    int count = 0;
    wifi_mgr_sta_config_t *matched_list = NULL;
    bool match_bssid = ap_info->bssid[0] != '\0';
    wifi_mgr_storage_search_mode_t search_mode = match_bssid ? SEARCH_BY_BSSID : SEARCH_BY_SSID;
    void *search_target = match_bssid ? (void *)ap_info->bssid : (void *)ap_info->ssid;

    count = wifi_storage_search_ap(ctx, &matched_list, search_mode, search_target);
    if (count < 0) {
        ret = count;
        goto __out;
    }

    if (count > 0) {
        uint32_t current_count = 0;
        wifi_storage_item_t *wifi_info_list = NULL;
        ret = wifi_storage_load_list_or_empty(ctx, &wifi_info_list, &current_count);
        if (ret != 0 || current_count == 0 || wifi_info_list == NULL) {
            ret = -ENOENT;
            goto __out;
        }
        for (uint32_t idx = 0; idx < current_count; idx++) {
            if (!wifi_info_list[idx].enable) {
                continue;
            }
            if ((match_bssid && strcmp(wifi_info_list[idx].ap_info.bssid, ap_info->bssid) == 0) ||
                (!match_bssid && strcmp(wifi_info_list[idx].ap_info.ssid, ap_info->ssid) == 0)) {
                wifi_storage_update_ap_info(&wifi_info_list[idx].ap_info, ap_info);
                wifi_info_list[idx].enable = true;
                wifi_info_list[idx].version = WIFI_STORAGE_VERSION_CURRENT;
                if (match_bssid) {
                    break;
                }
            }
        }
        ret = wifi_storage_persist_list(ctx, wifi_info_list, current_count);
        if (ret != 0) {
            wifi_storage_cache_drop(ctx);
        }
        goto __out;
    }

    if (match_bssid) {
        int ssid_count = wifi_storage_search_ap(ctx, &matched_list, SEARCH_BY_SSID, ap_info->ssid);
        if (ssid_count < 0) {
            ret = ssid_count;
            goto __out;
        }
        if (ssid_count > 0) {
            uint32_t current_count = 0;
            wifi_storage_item_t *wifi_info_list = NULL;
            ret = wifi_storage_load_list_or_empty(ctx, &wifi_info_list, &current_count);
            if (ret != 0 || current_count == 0 || wifi_info_list == NULL) {
                ret = -ENOENT;
                goto __out;
            }
            bool updated = false;
            for (uint32_t idx = 0; idx < current_count; idx++) {
                if (!wifi_info_list[idx].enable) {
                    continue;
                }
                if (strcmp(wifi_info_list[idx].ap_info.ssid, ap_info->ssid) == 0 &&
                    wifi_info_list[idx].ap_info.bssid[0] == '\0') {
                    wifi_storage_update_ap_info(&wifi_info_list[idx].ap_info, ap_info);
                    wifi_info_list[idx].enable = true;
                    wifi_info_list[idx].version = WIFI_STORAGE_VERSION_CURRENT;
                    updated = true;
                }
            }
            if (updated) {
                ret = wifi_storage_persist_list(ctx, wifi_info_list, current_count);
                if (ret != 0) {
                    wifi_storage_cache_drop(ctx);
                }
                goto __out;
            }
        }
    }

    uint32_t current_count = 0;
    wifi_storage_item_t *wifi_info_list = NULL;
    ret = wifi_storage_load_list_or_empty(ctx, &wifi_info_list, &current_count);
    if (ret != 0) {
        goto __out;
    }
    wifi_info_list = wifi_storage_resize_list(ctx, wifi_info_list, current_count, current_count + 1);
    if (wifi_info_list == NULL) {
        ret = -ENOMEM;
        goto __out;
    }
    wifi_storage_cache_set(ctx, NULL, 0);
    wifi_storage_cache_set(ctx, wifi_info_list, current_count + 1);
    ret = wifi_storage_add_item(ctx, wifi_info_list, &current_count, ap_info);
    if (ret != 0) {
        wifi_storage_cache_drop(ctx);
    }
    LOGI("func:%s, line:%d", __FUNCTION__, __LINE__);

__out:
    if (matched_list != NULL) {
        WIFI_STORAGE_FREE(ctx, matched_list);
    }
    return ret;
}


int wifi_storage_delete_ap(wifi_storage_ctx_t *ctx, wifi_mgr_sta_config_t *ap_info)
{
    WIFI_STORAGE_CHECK(ctx != NULL, -EINVAL);
    WIFI_STORAGE_CHECK(ap_info != NULL, -EINVAL);

    int ret = 0;
    bool match_ssid = ap_info->ssid[0] != '\0';
    bool match_bssid = ap_info->bssid[0] != '\0';
    wifi_mgr_storage_search_mode_t search_mode;
    void *search_target = NULL;
    if (match_ssid && match_bssid) {
        search_mode = SEARCH_BY_SSID | SEARCH_BY_BSSID;
        search_target = ap_info;
    } else if (match_ssid) {
        search_mode = SEARCH_BY_SSID;
        search_target = ap_info->ssid;
    } else if (match_bssid) {
        search_mode = SEARCH_BY_BSSID;
        search_target = ap_info->bssid;
    } else {
        return -EINVAL;
    }
    
    wifi_mgr_sta_config_t *matched_list = NULL;
    LISA_LOGI(TAG,"Delete item, ssid: %s, pwd: %s, bssid: %s", ap_info->ssid, ap_info->pwd, ap_info->bssid);
    int count = wifi_storage_search_ap(ctx, &matched_list, search_mode, search_target);

    if (count == 0) {
        LISA_LOGE(TAG,"Item not found in the WiFi NVS Storage, ssid: %s, pwd: %s, bssid: %s",
            ap_info->ssid, ap_info->pwd, ap_info->bssid);
        ret = -ENOENT;
        goto __out;
    }
    if (count < 0) {
        LISA_LOGE(TAG,"Search err, ssid: %s, pwd: %s, bssid: %s", ap_info->ssid, ap_info->pwd, ap_info->bssid);
        ret = count;
        goto __out;
    }

    uint32_t info_num = 0;
    wifi_storage_item_t *wifi_info_list = NULL;
    ret = wifi_storage_load_list_or_empty(ctx, &wifi_info_list, &info_num);
    if (ret != 0 || info_num == 0 || wifi_info_list == NULL) {
        ret = -ENOENT;
        goto __out;
    }
    ret = wifi_storage_remove_item(ctx, wifi_info_list, info_num, ap_info, match_ssid, match_bssid);
    if (ret != 0) {
        wifi_storage_cache_drop(ctx);
    }

__out:
    if (matched_list != NULL) {
        WIFI_STORAGE_FREE(ctx, matched_list);
    }
    return ret;
} 

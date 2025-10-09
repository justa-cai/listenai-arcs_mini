/**
 * @file lisaui_asset_manager.c
 * @author TrekMax (QinYUN575@Foxmail.com)
 * @brief
 * @version 0.1
 * @date 2025-04-22
 *
 * @copyright Copyright (c) 2021 - 2025 shenzhen listenai co., ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "lisaui_asset_manager.h"
#include "lisaui_log.h"
// #include "lisaui_app_common.h"
#if CONFIG_LISAUI_ENV_ARCS_SDK
#include "lsfs.h"
#include "log_print.h"
#include "lv_port_mem.h"
#include "sysheap.h"
#include <stdio.h>
#endif

static const char *TAG = "lisaui_asset_manager";

lisaui_err_t lisaui_asset_manager_init(void)
{
    return LISAUI_ERR_OK;
}

lisaui_err_t lisaui_asset_manager_get_asset_by_emmc_path(const char *path, lisaui_resource_item_t *item)
{
    lisaui_err_t ret = LISAUI_ERR_OK;

    if (path == NULL || item == NULL) {
        LISAUI_LOGE(TAG, "Invalid parameter");
        return LISAUI_ERR_INVALID_PARAM;
    }
    item->type = LISAUI_RES_TYPE_NONE;
    item->load_dst = LISAUI_RES_LOAD_DST_PSRAM; // 默认加载到 PSRAM
    item->path = (char *)path;
    item->data = NULL;
    item->size = 0;

#if CONFIG_LISAUI_ENV_ARCS_SDK

    struct lsfs_file_t file;

    lsfs_file_t_init(&file);
    ret = lsfs_open(&file, item->path, LSFS_O_READ);
    if (ret != 0) {
        LISAUI_LOGI(TAG, "Failed to open file: %s, err:%d", item->path, ret);
        return LISAUI_ERR_RES_NOT_FOUND;
    }

    lsfs_seek(&file, 0, SEEK_END);
    long lSize = lsfs_tell(&file);
    LISAUI_LOGI(TAG, "load file [%s] size %ld", item->path, lSize);

    if (lSize > 0) {
        item->data = exram_malloc(32, (sizeof(char) * lSize));
        if (item->data == NULL) {
            LISAUI_LOGE(TAG, "malloc falid");
            lsfs_close(&file);
            return LISAUI_ERR_NO_MEMORY;
        }
        item->size = lSize;
        lsfs_seek(&file, 0, SEEK_SET);
        int br = lsfs_read(&file, item->data, lSize);
        if (br != lSize) {
            LISAUI_LOGE(TAG, "lsfs_read falid");
            lsfs_close(&file);
            return LISAUI_ERR_FAIL;
        }
    } else {
        LISAUI_LOGE(TAG, "File size is zero");
        lsfs_close(&file);
        return LISAUI_ERR_FAIL;
    }
    lsfs_close(&file);
    item->is_loaded = true;
#endif

    return LISAUI_ERR_RES_NOT_FOUND;
}

lisaui_err_t lisaui_asset_manager_free_asset(lisaui_resource_item_t *item)
{
    if (item == NULL) {
        LISAUI_LOGE(TAG, "Invalid parameter");
        return LISAUI_ERR_INVALID_PARAM;
    }
    if (item->data != NULL) {
        if (!item->is_loaded) {
            LISAUI_LOGI(TAG, "Free asset [%s] size %zu", item->path, item->size);
            return LISAUI_ERR_RES_NOT_LOADED;
        }
        if (item->load_dst == LISAUI_RES_LOAD_DST_PSRAM) {
            LISAUI_LOGI(TAG, "Free asset [%s] size %zu", item->path, item->size);
#if CONFIG_LISAUI_ENV_ARCS_SDK
            exram_free(item->data);
#endif
        } else if (item->load_dst == LISAUI_RES_LOAD_DST_FLASH) {
            LISAUI_LOGI(TAG, "Free asset [%s] size %zu", item->path, item->size);
        } else {
            LISAUI_LOGI(TAG, "Free asset [%s] size %zu", item->path, item->size);
        }
        item->data = NULL;
        item->size = 0;
        item->is_loaded = false;
    }
    return LISAUI_ERR_OK;
}

lisaui_err_t lisaui_asset_manager_get_asset_by_path(const char *path, lisaui_resource_item_t *item)
{
    if (path == NULL || item == NULL) {
        LISAUI_LOGE(TAG, "Invalid parameter");
        return LISAUI_ERR_INVALID_PARAM;
    }
    item->type = LISAUI_RES_TYPE_NONE;
    item->path = (char *)path;
    item->data = NULL;
    item->size = 0;

    return LISAUI_ERR_RES_NOT_FOUND;
}

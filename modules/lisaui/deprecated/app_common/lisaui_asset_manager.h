/**
 * @file lisaui_asset_manager.h
 * @author Tianshuang Ke (dske@listenai.com)
 * @brief 
 * @version 0.1
 * @date 2025-03-06
 * 
 * @copyright Copyright (c) 2021 - 2025 shenzhen listenai co., ltd.
 * 
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __LISAUI_ASSET_MANAGER_H__
#define __LISAUI_ASSET_MANAGER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "lisaui_type.h"

typedef enum _lisaui_res_type_e {
    LISAUI_RES_TYPE_NONE = 0,
    LISAUI_RES_TYPE_BITMAP_IMAGE,
    LISAUI_RES_TYPE_BITMAP_FONT,
    LISAUI_RES_TYPE_BIN_IMAGE,
    LISAUI_RES_TYPE_BIN_FONT,
    LISAUI_RES_TYPE_AUDIO,
    LISAUI_RES_TYPE_VIDEO,
    LISAUI_RES_TYPE_TEXT,
} lisaui_res_type_t;

typedef enum _lisaui_res_storage_e {
    LISAUI_RES_STORAGE_NONE = 0,
    LISAUI_RES_STORAGE_FLASH,
    LISAUI_RES_STORAGE_EMMC,
} lisaui_res_storage_t;

typedef enum _lisaui_res_load_dst_e {
    LISAUI_RES_LOAD_DST_NONE = 0,
    LISAUI_RES_LOAD_DST_SRAM,
    LISAUI_RES_LOAD_DST_PSRAM,

    // LISAUI_RES_LOAD_DST_EXRAM,
    LISAUI_RES_LOAD_DST_FLASH,
} lisaui_res_load_dst_t;

typedef struct _lisaui_resource_item_t {
    lisaui_res_type_t type;
    lisaui_res_load_dst_t load_dst;
    char *path;
    void *data;
    size_t size;
    bool is_loaded;
} lisaui_resource_item_t;

#define LISAUI_ASSET_SAFE_FREE(item) \
    do { \
        if (item != NULL) { \
            lisaui_asset_manager_free_asset(item); \
        } \
    } while (0)

/**
 * @brief 
 * 
 * @return lisaui_err_t 
 */
lisaui_err_t lisaui_asset_manager_init(void);
// lisaui_err_t lisaui_asset_manager_deinit(void);
void* lisaui_asset_manager_get_asset(const char *path);
lisaui_err_t lisaui_asset_manager_get_asset_by_path(const char *path, lisaui_resource_item_t *item);
lisaui_err_t lisaui_asset_manager_get_asset_by_emmc_path(const char *path, lisaui_resource_item_t *item);
lisaui_err_t lisaui_asset_manager_free_asset(lisaui_resource_item_t *item);


#ifdef __cplusplus
}
#endif

#endif /* __LISAUI_ASSET_MANAGER_H__ */
#pragma once

#include "wifi_manager/wifi_manager.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WiFi storage format version definitions
 */
#define WIFI_STORAGE_VERSION_LEGACY     0x01  /**< Legacy format (no PMK support) */
#define WIFI_STORAGE_VERSION_WITH_PMK   0x02  /**< Format with PMK support */

/**
 * @brief Current storage version based on configuration
 */
#define WIFI_STORAGE_VERSION_CURRENT    WIFI_STORAGE_VERSION_WITH_PMK

typedef struct wifi_storage_item {
    uint8_t version;              /**< Storage format version, see WIFI_STORAGE_VERSION_* */
    wifi_mgr_sta_config_t ap_info;
    uint8_t enable;
} wifi_storage_item_t;

typedef struct {
    wifi_storage_item_t *list;
    uint32_t count;
    bool loaded;
} wifi_storage_cache_t;

typedef struct {
    void *(*malloc)(size_t size);
    void *(*calloc)(size_t nmemb, size_t size);
    void (*free)(void *ptr);
    int (*mutex_lock)(void *mutex, uint32_t timeout);
    int (*mutex_unlock)(void *mutex);
} wifi_storage_ops_t;

typedef struct {
    uint32_t list_item_count;
    wifi_storage_ops_t ops;
    void *mutex;
    wifi_storage_cache_t cache;
} wifi_storage_ctx_t;

int wifi_storage_init(wifi_storage_ctx_t *ctx, wifi_storage_ops_t *ops, void *mutex);

int wifi_storage_deinit(wifi_storage_ctx_t *ctx);

int wifi_storage_save_ap(wifi_storage_ctx_t *ctx, wifi_mgr_sta_config_t *ap_info);

int wifi_storage_delete_ap(wifi_storage_ctx_t *ctx, wifi_mgr_sta_config_t *ap_info);

int wifi_storage_search_ap(wifi_storage_ctx_t *ctx, wifi_mgr_sta_config_t **matched_list, 
                           wifi_mgr_storage_search_mode_t search_modes, void* target);

#ifdef __cplusplus
}
#endif 

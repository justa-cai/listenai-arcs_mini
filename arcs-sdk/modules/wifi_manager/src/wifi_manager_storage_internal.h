#pragma once

#include <stdint.h>

#include "wifi_manager/wifi_manager_storage.h"
#include "wifi_manager_storage_cache.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WIFI_STORAGE_BLOB_LIST_KEY "wifi-list"
#define WIFI_STORAGE_BLOB_COUNT_KEY "wifi-info-count"

#define WIFI_STORAGE_KV_COUNT_KEY "wifi-kv-count"
#define WIFI_STORAGE_KV_ITEM_PREFIX "wifi-kv-item"

#define WIFI_STORAGE_MALLOC(ctx, size) ((ctx)->ops.malloc(size))
#define WIFI_STORAGE_CALLOC(ctx, nmemb, size) ((ctx)->ops.calloc(nmemb, size))
#define WIFI_STORAGE_FREE(ctx, ptr) ((ctx)->ops.free(ptr))

int wifi_storage_kv_load_list(wifi_storage_ctx_t *ctx, wifi_storage_item_t **out_list, uint32_t *out_count);
int wifi_storage_kv_save_list(wifi_storage_ctx_t *ctx, const wifi_storage_item_t *list, uint32_t count);
int wifi_storage_kv_clear_list(wifi_storage_ctx_t *ctx);

int wifi_storage_blob_legacy_load_list(wifi_storage_ctx_t *ctx, wifi_storage_item_t **out_list, uint32_t *out_count);

int wifi_storage_migrate_load_list(wifi_storage_ctx_t *ctx, wifi_storage_item_t **out_list, uint32_t *out_count);
int wifi_storage_migrate_load_count(uint32_t *out_count);
void wifi_storage_migrate_delete_legacy(void);

#ifdef __cplusplus
}
#endif

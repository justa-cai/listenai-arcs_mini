#pragma once

#include "wifi_manager/wifi_manager_storage.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*wifi_storage_loader_fn)(wifi_storage_ctx_t *ctx, wifi_storage_item_t **out_list, uint32_t *out_count);

void wifi_storage_cache_init_ctx(wifi_storage_ctx_t *ctx);
void wifi_storage_cache_drop(wifi_storage_ctx_t *ctx);
int wifi_storage_cache_load(wifi_storage_ctx_t *ctx, wifi_storage_loader_fn loader);
void wifi_storage_cache_set(wifi_storage_ctx_t *ctx, wifi_storage_item_t *list, uint32_t count);
void wifi_storage_cache_get(const wifi_storage_ctx_t *ctx, wifi_storage_item_t **out_list, uint32_t *out_count);
bool wifi_storage_cache_valid(const wifi_storage_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

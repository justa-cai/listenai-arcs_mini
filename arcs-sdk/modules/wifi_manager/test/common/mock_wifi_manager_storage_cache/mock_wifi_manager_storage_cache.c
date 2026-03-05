#include "wifi_manager_storage_cache.h"
#include "wifi_manager_storage_internal.h"
#include <errno.h>

void wifi_storage_cache_init_ctx(wifi_storage_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }
    ctx->cache.list = NULL;
    ctx->cache.count = 0;
    ctx->cache.loaded = false;
}

void wifi_storage_cache_drop(wifi_storage_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }
    if (ctx->cache.list != NULL && ctx->ops.free != NULL) {
        ctx->ops.free(ctx->cache.list);
    }
    ctx->cache.list = NULL;
    ctx->cache.count = 0;
    ctx->cache.loaded = false;
}

bool wifi_storage_cache_valid(const wifi_storage_ctx_t *ctx)
{
    return ctx != NULL && ctx->cache.loaded;
}

void wifi_storage_cache_set(wifi_storage_ctx_t *ctx, wifi_storage_item_t *list, uint32_t count)
{
    if (ctx == NULL) {
        return;
    }
    ctx->cache.list = list;
    ctx->cache.count = count;
    ctx->cache.loaded = true;
}

int wifi_storage_cache_load(wifi_storage_ctx_t *ctx, wifi_storage_loader_fn loader)
{
    if (ctx == NULL || loader == NULL) {
        return -EINVAL;
    }
    if (wifi_storage_cache_valid(ctx)) {
        return 0;
    }
    if (ctx->cache.list != NULL) {
        wifi_storage_cache_drop(ctx);
    }

    wifi_storage_item_t *list = NULL;
    uint32_t count = 0;
    int ret = loader(ctx, &list, &count);
    if (ret == 0) {
        wifi_storage_cache_set(ctx, list, count);
    }
    return ret;
}

void wifi_storage_cache_get(const wifi_storage_ctx_t *ctx, wifi_storage_item_t **out_list, uint32_t *out_count)
{
    if (out_list != NULL) {
        *out_list = (ctx != NULL) ? ctx->cache.list : NULL;
    }
    if (out_count != NULL) {
        *out_count = (ctx != NULL) ? ctx->cache.count : 0;
    }
}

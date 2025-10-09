#include "cache.h"
#include "func_proxy.h"
#include "lv_func_proxy_server.h"
#include <string.h>


extern void fill_normal(lv_color_t *dest_buf, const lv_area_t *dest_area, lv_coord_t dest_stride, lv_color_t color,
                        lv_opa_t opa, const lv_opa_t *mask, lv_coord_t mask_stride);
static int32_t fill_normal_handler(func_proxy_data_t *in, func_proxy_data_t *out)
{
    // 参数检查
    if (!in || !in->data || in->size != sizeof(struct sw_fill_normal_args) || !out || !out->data ||
        out->size < sizeof(struct sw_fill_normal_result)) {
        return -1;
    }

    struct sw_fill_normal_args *args = (struct sw_fill_normal_args *)in->data;

    dcache_invalidate_range((uint32_t)args->dest_buf,
                            (uint32_t)args->dest_buf +
                                args->dest_stride * lv_area_get_height(&args->dest_area) * sizeof(lv_color_t));
    dcache_invalidate_range((uint32_t)args->mask,
                            (uint32_t)args->mask + args->mask_stride * lv_area_get_height(&args->dest_area));
    fill_normal(args->dest_buf, &args->dest_area, args->dest_stride, args->color, args->opa, args->mask,
                args->mask_stride);
    dcache_clean_range((uint32_t)args->dest_buf, (uint32_t)args->dest_buf + args->dest_stride *
                                                                                lv_area_get_height(&args->dest_area) *
                                                                                sizeof(lv_color_t));

    return 0;
}

extern void map_normal(lv_color_t *dest_buf, const lv_area_t *dest_area, lv_coord_t dest_stride,
                       const lv_color_t *src_buf, lv_coord_t src_stride, lv_opa_t opa, const lv_opa_t *mask,
                       lv_coord_t mask_stride);
static int32_t map_normal_handler(func_proxy_data_t *in, func_proxy_data_t *out)
{
    // 参数检查
    if (!in || !in->data || in->size != sizeof(struct sw_map_normal_args) || !out || !out->data ||
        out->size < sizeof(struct sw_map_normal_result)) {
        return -1;
    }

    struct sw_map_normal_args *args = (struct sw_map_normal_args *)in->data;

    dcache_invalidate_range((uint32_t)args->dest_buf,
                            (uint32_t)args->dest_buf +
                                args->dest_stride * lv_area_get_height(&args->dest_area) * sizeof(lv_color_t));
    dcache_invalidate_range((uint32_t)args->mask,
                            (uint32_t)args->mask + args->mask_stride * lv_area_get_height(&args->dest_area));
    dcache_invalidate_range((uint32_t)args->src_buf,
                            (uint32_t)args->src_buf +
                                args->src_stride * lv_area_get_height(&args->dest_area) * sizeof(lv_color_t));
    map_normal(args->dest_buf, &args->dest_area, args->dest_stride, args->src_buf, args->src_stride, args->opa,
               args->mask, args->mask_stride);
    dcache_clean_range((uint32_t)args->dest_buf, (uint32_t)args->dest_buf + args->dest_stride *
                                                                                lv_area_get_height(&args->dest_area) *
                                                                                sizeof(lv_color_t));

    return 0;
}

extern void fill_blended(lv_color_t *dest_buf, const lv_area_t *dest_area, lv_coord_t dest_stride, lv_color_t color,
                         lv_opa_t opa, const lv_opa_t *mask, lv_coord_t mask_stride, lv_blend_mode_t blend_mode);
static int32_t fill_blended_handler(func_proxy_data_t *in, func_proxy_data_t *out)
{
    // 参数检查
    if (!in || !in->data || in->size != sizeof(struct sw_fill_blended_args) || !out || !out->data ||
        out->size < sizeof(struct sw_fill_blended_result)) {
        return -1;
    }

    struct sw_fill_blended_args *args = (struct sw_fill_blended_args *)in->data;

    dcache_invalidate_range((uint32_t)args->dest_buf,
                            (uint32_t)args->dest_buf +
                                args->dest_stride * lv_area_get_height(&args->dest_area) * sizeof(lv_color_t));
    dcache_invalidate_range((uint32_t)args->mask,
                            (uint32_t)args->mask + args->mask_stride * lv_area_get_height(&args->dest_area));
    fill_blended(args->dest_buf, &args->dest_area, args->dest_stride, args->color, args->opa, args->mask,
                 args->mask_stride, args->blend_mode);
    dcache_clean_range((uint32_t)args->dest_buf, (uint32_t)args->dest_buf + args->dest_stride *
                                                                                lv_area_get_height(&args->dest_area) *
                                                                                sizeof(lv_color_t));
    return 0;
}

extern void map_blended(lv_color_t *dest_buf, const lv_area_t *dest_area, lv_coord_t dest_stride,
                        const lv_color_t *src_buf, lv_coord_t src_stride, lv_opa_t opa, const lv_opa_t *mask,
                        lv_coord_t mask_stride, lv_blend_mode_t blend_mode);
static int32_t map_blended_handler(func_proxy_data_t *in, func_proxy_data_t *out)
{
    // 参数检查
    if (!in || !in->data || in->size != sizeof(struct sw_map_blended_args) || !out || !out->data ||
        out->size < sizeof(struct sw_map_blended_result)) {
        return -1;
    }

    struct sw_map_blended_args *args = (struct sw_map_blended_args *)in->data;

    dcache_invalidate_range((uint32_t)args->dest_buf,
                            (uint32_t)args->dest_buf +
                                args->dest_stride * lv_area_get_height(&args->dest_area) * sizeof(lv_color_t));
    dcache_invalidate_range((uint32_t)args->mask,
                            (uint32_t)args->mask + args->mask_stride * lv_area_get_height(&args->dest_area));
    dcache_invalidate_range((uint32_t)args->src_buf,
                            (uint32_t)args->src_buf +
                                args->src_stride * lv_area_get_height(&args->dest_area) * sizeof(lv_color_t));
    map_blended(args->dest_buf, &args->dest_area, args->dest_stride, args->src_buf, args->src_stride, args->opa,
                args->mask, args->mask_stride, args->blend_mode);
    dcache_clean_range((uint32_t)args->dest_buf, (uint32_t)args->dest_buf + args->dest_stride *
                                                                                lv_area_get_height(&args->dest_area) *
                                                                                sizeof(lv_color_t));

    return 0;
}

void lv_rpc_server_init(void)
{
    func_proxy_init();
    func_proxy_register(FUNC_NAME_FILL_NORMAL, fill_normal_handler);
    func_proxy_register(FUNC_NAME_MAP_NORMAL, map_normal_handler);
    func_proxy_register(FUNC_NAME_FILL_BLEND, fill_blended_handler);
    func_proxy_register(FUNC_NAME_MAP_BLEND, map_blended_handler);
}

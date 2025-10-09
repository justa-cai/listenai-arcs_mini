#include "func_proxy.h"
#include "lisa_log.h"
#include "lv_func_proxy.h"
#include "cache.h"

#if CONFIG_LV_CUSTOM_MONITOR
#include "csk_monitor_lv.h"
volatile uint64_t g_rpc_fill_normal_total_time;
volatile uint64_t g_rpc_fill_normal_proxy_time;
volatile uint32_t g_rpc_fill_normal_call_cnt;

volatile uint64_t g_rpc_map_normal_total_time;
volatile uint64_t g_rpc_map_normal_proxy_time;
volatile uint32_t g_rpc_map_normal_call_cnt;

volatile uint64_t g_rpc_fill_blended_total_time;
volatile uint64_t g_rpc_fill_blended_proxy_time;
volatile uint32_t g_rpc_fill_blended_call_cnt;

volatile uint64_t g_rpc_map_blended_total_time;
volatile uint64_t g_rpc_map_blended_proxy_time;
volatile uint32_t g_rpc_map_blended_call_cnt;
#endif

int lv_rpc_fill_normal(lv_color_t * dest_buf, const lv_area_t * dest_area,
                                              lv_coord_t dest_stride, lv_color_t color, lv_opa_t opa,
                                              const lv_opa_t * mask, lv_coord_t mask_stride)
{
#if CONFIG_LV_CUSTOM_MONITOR
    uint64_t start_time = get_time_cycle();
#endif

    struct sw_fill_normal_args fill_args;
    fill_args.dest_buf = dest_buf;
    memcpy(&fill_args.dest_area, dest_area, sizeof(lv_area_t));
    fill_args.dest_stride = dest_stride;
    fill_args.color.full = color.full;
    fill_args.opa = opa;
    fill_args.mask = (lv_opa_t *)mask;
    fill_args.mask_stride = mask_stride;

    dcache_flush_range((uint32_t )dest_buf, (uint32_t)dest_buf + dest_stride * lv_area_get_height(dest_area) * sizeof(lv_color_t));
    dcache_flush_range((uint32_t )mask, (uint32_t)mask + mask_stride * lv_area_get_height(dest_area));

    func_proxy_data_t in = {
        .data = &fill_args,
        .size = sizeof(fill_args)
    };

    struct sw_fill_normal_result result;
    func_proxy_data_t out = {
        .data = &result,
        .size = sizeof(result)
    };

#if CONFIG_LV_CUSTOM_MONITOR
    uint64_t proxy_time = get_time_cycle();
#endif

    int32_t ret = func_proxy_call(FUNC_NAME_FILL_NORMAL, &in, &out);
    if(ret){
        LOGE("[%s]error %d", __FUNCTION__, ret);
        return -1;
    }

#if CONFIG_LV_CUSTOM_MONITOR
    g_rpc_fill_normal_total_time += calc_time_elapsed_cycle(start_time);
    g_rpc_fill_normal_proxy_time += calc_time_elapsed_cycle(proxy_time);
    g_rpc_fill_normal_call_cnt++;
#endif

    return 0;
}


int lv_rpc_map_normal(lv_color_t * dest_buf, const lv_area_t * dest_area,
                                                   lv_coord_t dest_stride, const lv_color_t * src_buf,
                                                   lv_coord_t src_stride, lv_opa_t opa, const lv_opa_t * mask,
                                                   lv_coord_t mask_stride)
{
#if CONFIG_LV_CUSTOM_MONITOR
    uint64_t start_time = get_time_cycle();
#endif

    struct sw_map_normal_args map_args;
    map_args.dest_buf = dest_buf;
    memcpy(&map_args.dest_area, dest_area, sizeof(lv_area_t));
    map_args.dest_stride = dest_stride;
    map_args.src_buf = (lv_color_t *)src_buf;
    map_args.src_stride = src_stride;
    map_args.opa = opa;
    map_args.mask = (lv_opa_t *)mask;
    map_args.mask_stride = mask_stride;

    dcache_flush_range((uint32_t )dest_buf, (uint32_t)dest_buf + dest_stride * lv_area_get_height(dest_area) * sizeof(lv_color_t));
    dcache_flush_range((uint32_t )mask, (uint32_t)mask + mask_stride * lv_area_get_height(dest_area));
    dcache_flush_range((uint32_t )src_buf, (uint32_t)src_buf + src_stride * lv_area_get_height(dest_area) * sizeof(lv_color_t));

    func_proxy_data_t in = {
        .data = &map_args,
        .size = sizeof(map_args)
    };

    struct sw_map_normal_result result;
    func_proxy_data_t out = {
        .data = &result,
        .size = sizeof(result)
    };

#if CONFIG_LV_CUSTOM_MONITOR
    uint64_t proxy_time = get_time_cycle();
#endif

    int32_t ret = func_proxy_call(FUNC_NAME_MAP_NORMAL, &in, &out);
    if(ret){
        LOGE("[%s]error %d", __FUNCTION__, ret);
        return -1;
    }

#if CONFIG_LV_CUSTOM_MONITOR
    g_rpc_map_normal_total_time += calc_time_elapsed_cycle(start_time);
    g_rpc_map_normal_proxy_time += calc_time_elapsed_cycle(proxy_time);
    g_rpc_map_normal_call_cnt++;
#endif

    return 0;
}

int lv_rpc_fill_blended(lv_color_t * dest_buf, const lv_area_t * dest_area,
                                              lv_coord_t dest_stride, lv_color_t color, lv_opa_t opa,
                                              const lv_opa_t * mask, lv_coord_t mask_stride, lv_blend_mode_t blend_mode)
{
#if CONFIG_LV_CUSTOM_MONITOR
    uint64_t start_time = get_time_cycle();
#endif

    struct sw_fill_blended_args fill_args;
    fill_args.dest_buf = dest_buf;
    memcpy(&fill_args.dest_area, dest_area, sizeof(lv_area_t));
    fill_args.dest_stride = dest_stride;
    fill_args.color = color;
    fill_args.opa = opa;
    fill_args.mask = (lv_opa_t *)mask;
    fill_args.mask_stride = mask_stride;
    fill_args.blend_mode = blend_mode;

    dcache_flush_range((uint32_t )dest_buf, (uint32_t)dest_buf + dest_stride * lv_area_get_height(dest_area) * sizeof(lv_color_t));
    dcache_flush_range((uint32_t )mask, (uint32_t)mask + mask_stride * lv_area_get_height(dest_area));

    func_proxy_data_t in = {
        .data = &fill_args,
        .size = sizeof(fill_args)
    };

    struct sw_fill_blended_result result;
    func_proxy_data_t out = {
        .data = &result,
        .size = sizeof(result)
    };

#if CONFIG_LV_CUSTOM_MONITOR
    uint64_t proxy_time = get_time_cycle();
#endif

    int32_t ret = func_proxy_call(FUNC_NAME_FILL_BLEND, &in, &out);
    if(ret){
        LOGE("[%s]error %d", __FUNCTION__, ret);
        return -1;
    }

#if CONFIG_LV_CUSTOM_MONITOR
    g_rpc_fill_blended_total_time += calc_time_elapsed_cycle(start_time);
    g_rpc_fill_blended_proxy_time += calc_time_elapsed_cycle(proxy_time);
    g_rpc_fill_blended_call_cnt++;
#endif

    return 0;
}


int lv_rpc_map_blended(lv_color_t * dest_buf, const lv_area_t * dest_area,
                                                   lv_coord_t dest_stride, const lv_color_t * src_buf,
                                                   lv_coord_t src_stride, lv_opa_t opa, const lv_opa_t * mask,
                                                   lv_coord_t mask_stride, lv_blend_mode_t blend_mode)
{
#if CONFIG_LV_CUSTOM_MONITOR
    uint64_t start_time = get_time_cycle();
#endif
    struct sw_map_blended_args map_args;
    map_args.dest_buf = dest_buf;
    memcpy(&map_args.dest_area, dest_area, sizeof(lv_area_t));
    map_args.dest_stride = dest_stride;
    map_args.src_buf = (lv_color_t *)src_buf;
    map_args.src_stride = src_stride;
    map_args.opa = opa;
    map_args.mask = (lv_opa_t *)mask;
    map_args.mask_stride = mask_stride;
    map_args.blend_mode = blend_mode;

    dcache_flush_range((uint32_t )dest_buf, (uint32_t)dest_buf + dest_stride * lv_area_get_height(dest_area) * sizeof(lv_color_t));
    dcache_flush_range((uint32_t )mask, (uint32_t)mask + mask_stride * lv_area_get_height(dest_area));
    dcache_flush_range((uint32_t )src_buf, (uint32_t)src_buf + src_stride * lv_area_get_height(dest_area) * sizeof(lv_color_t));

    func_proxy_data_t in = {
        .data = &map_args,
        .size = sizeof(map_args)
    };

    struct sw_map_blended_result result;
    func_proxy_data_t out = {
        .data = &result,
        .size = sizeof(result)
    };

#if CONFIG_LV_CUSTOM_MONITOR
    uint64_t proxy_time = get_time_cycle();
#endif

    int32_t ret = func_proxy_call(FUNC_NAME_MAP_BLEND, &in, &out);
    if(ret){
        LOGE("[%s]error %d", __FUNCTION__, ret);
        return -1;
    }

#if CONFIG_LV_CUSTOM_MONITOR
    g_rpc_map_blended_total_time += calc_time_elapsed_cycle(start_time);
    g_rpc_map_blended_proxy_time += calc_time_elapsed_cycle(proxy_time);
    g_rpc_map_blended_call_cnt++;
#endif

    return 0;
}

void lv_rpc_init(void)
{
    func_proxy_init();
}

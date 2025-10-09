#include "func_proxy.h"
#include "log_print.h"
#include "lv_func_proxy.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cache.h"

#if CONFIG_LV_CUSTOM_MONITOR
#include "csk_monitor_lv.h"
#endif

void lv_rpc_init(void)
{
    func_proxy_init();
}

int lv_rpc_map_normal(const lv_area_t * disp_area, lv_color_t * disp_buf,
                                             const lv_area_t * draw_area,
                                             const lv_area_t * map_area, const lv_color_t * map_buf, lv_opa_t opa,
                                             const lv_opa_t * mask, lv_draw_mask_res_t mask_res)
{
#if CONFIG_LV_CUSTOM_MONITOR
    uint64_t start_time = get_time_cycle();
#endif

    struct rpc_map_normal_args map_args;
    memcpy(&map_args.disp_area, disp_area, sizeof(lv_area_t));
    map_args.disp_buf = disp_buf;
    memcpy(&map_args.draw_area, draw_area, sizeof(lv_area_t));
    memcpy(&map_args.map_area, map_area, sizeof(lv_area_t));
    map_args.map_buf = (lv_color_t *)map_buf;
    map_args.opa = opa;
    map_args.mask = (lv_opa_t *)mask;
    map_args.mask_res = mask_res;

//第一种方式虽然flush更多区域，但代码开销更小
#if 1
    dcache_flush_range((uint32_t )disp_buf, (uint32_t)disp_buf + lv_area_get_width(disp_area) * lv_area_get_height(disp_area) * sizeof(lv_color_t));
    dcache_flush_range((uint32_t )map_buf, (uint32_t)map_buf + lv_area_get_width(map_area) * lv_area_get_height(map_area) * sizeof(lv_color_t));
#else
    uint32_t area_height = lv_area_get_height(draw_area);
    uint32_t area_width_size = lv_area_get_width(draw_area) * sizeof(lv_color_t);
    for(int i = 0; i < area_height; i++){
        dcache_flush_range((uint32_t )disp_buf, (uint32_t)(disp_buf + area_width_size));
        disp_buf += lv_area_get_width(disp_area);
    }
    for(int i = 0; i < area_height; i++){
        dcache_flush_range((uint32_t )map_buf, (uint32_t)(map_buf + area_width_size));
        map_buf += lv_area_get_width(map_area);
    }
#endif

    dcache_flush_range((uint32_t )mask, (uint32_t)mask + lv_area_get_width(draw_area) * lv_area_get_height(draw_area) * sizeof(lv_opa_t));

    func_proxy_data_t in = {
        .data = &map_args,
        .size = sizeof(map_args)
    };

    struct rpc_map_normal_result result;
    func_proxy_data_t out = {
        .data = &result,
        .size = sizeof(result)
    };

#if CONFIG_LV_CUSTOM_MONITOR
    uint64_t proxy_time = get_time_cycle();
#endif

    int ret = func_proxy_call(FUNC_NAME_MAP_NORMAL, &in, &out);
    if(ret){
        CLOGE("[%s]error %d", __FUNCTION__, ret);
        return -1;
    }

#if CONFIG_LV_CUSTOM_MONITOR
    g_rpc_map_total_time += calc_time_elapsed_cycle(start_time);
    g_rpc_map_proxy_time += calc_time_elapsed_cycle(proxy_time);
    g_rpc_map_call_cnt++;
#endif

    return 0;
}

int lv_rpc_fill_normal(const lv_area_t * disp_area, lv_color_t * disp_buf,
                                              const lv_area_t * draw_area,
                                              lv_color_t color, lv_opa_t opa,
                                              const lv_opa_t * mask, lv_draw_mask_res_t mask_res)
{
#if CONFIG_LV_CUSTOM_MONITOR
    uint64_t start_time = get_time_cycle();
#endif

    struct rpc_fill_normal_args fill_args;
    memcpy((void *)&fill_args.disp_area, (void *)disp_area, sizeof(lv_area_t));
    fill_args.disp_buf = disp_buf;
    memcpy((void *)&fill_args.draw_area, (void *)draw_area, sizeof(lv_area_t));
    fill_args.color.full = color.full;
    fill_args.opa = opa;
    fill_args.mask = (lv_opa_t *)mask;
    fill_args.mask_res = mask_res;

//第一种方式虽然flush更多区域，但代码开销更小
#if 1
    dcache_flush_range((uint32_t )disp_buf, (uint32_t)disp_buf + lv_area_get_width(disp_area) * lv_area_get_height(disp_area) * sizeof(lv_color_t));
#else
    uint32_t area_height = lv_area_get_height(draw_area);
    uint32_t area_width_size = lv_area_get_width(draw_area) * sizeof(lv_color_t);
    for(int i = 0; i < area_height; i++){
        dcache_flush_range((uint32_t )disp_buf, (uint32_t)(disp_buf + area_width_size));
        disp_buf += lv_area_get_width(disp_area);
    }
#endif

    dcache_flush_range((uint32_t )mask, (uint32_t)mask + lv_area_get_width(draw_area) * lv_area_get_height(draw_area) * sizeof(lv_opa_t));

    func_proxy_data_t in = {
        .data = &fill_args,
        .size = sizeof(fill_args)
    };

    struct rpc_fill_normal_result result;
    func_proxy_data_t out = {
        .data = &result,
        .size = sizeof(result)
    };

#if CONFIG_LV_CUSTOM_MONITOR
    uint64_t proxy_time = get_time_cycle();
#endif

    int32_t ret = func_proxy_call(FUNC_NAME_FILL_NORMAL, &in, &out);
    if(ret){
        CLOGE("[%s]error %d", __FUNCTION__, ret);
        return -1;
    }

#if CONFIG_LV_CUSTOM_MONITOR
    g_rpc_fill_total_time += calc_time_elapsed_cycle(start_time);
    g_rpc_fill_proxy_time += calc_time_elapsed_cycle(proxy_time);
    g_rpc_fill_call_cnt++;
#endif

    return 0;
}

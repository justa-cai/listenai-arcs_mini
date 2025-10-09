#include "log_print.h"
#include "cache.h"
#include "func_proxy.h"
#include "lv_func_proxy_server.h"
#include <string.h>
#include "appinc.h"

extern void fill_normal(const lv_area_t * disp_area, lv_color_t * disp_buf,
                                              const lv_area_t * draw_area,
                                              lv_color_t color, lv_opa_t opa,
                                              const lv_opa_t * mask, lv_draw_mask_res_t mask_res);
static int32_t fill_normal_handler(func_proxy_data_t *in, func_proxy_data_t *out)
{
    // 参数检查
    if (!in || !in->data || in->size != sizeof(struct rpc_fill_normal_args) ||
        !out || !out->data || out->size < sizeof(struct rpc_fill_normal_result)) {
        CLOGE("[%s] invalid parameters", __func__);
        return -1;
    }

    struct rpc_fill_normal_args *args = (struct rpc_fill_normal_args *)in->data;

    uint32_t addr = (uint32_t )args->disp_buf;

//第一种方式虽然flush更多区域，但代码开销更小
#if 1
    dcache_invalidate_range((uint32_t )args->disp_buf, 
        (uint32_t)args->disp_buf + lv_area_get_width(&args->disp_area) * lv_area_get_height(&args->disp_area) * sizeof(lv_color_t));
#else
    uint32_t area_height = lv_area_get_height(&args->draw_area);
    uint32_t area_width_size = lv_area_get_width(&args->draw_area) * sizeof(lv_color_t);
    for(int i = 0; i < area_height; i++){
        dcache_invalidate_range(addr, addr + area_width_size);
        addr += lv_area_get_width(&args->disp_area) * sizeof(lv_color_t);
    }
#endif

    dcache_invalidate_range((uint32_t )args->mask, (uint32_t)args->mask + lv_area_get_width(&args->draw_area) * lv_area_get_height(&args->draw_area));

    fill_normal((const lv_area_t *)&args->disp_area, args->disp_buf, (const lv_area_t *)&args->draw_area, args->color,
                    args->opa, args->mask, args->mask_res);
    

    addr = (uint32_t )args->disp_buf;
    dcache_clean_range(addr, addr + lv_area_get_width(&args->disp_area) * lv_area_get_height(&args->disp_area) * sizeof(lv_color_t));
    return 0;
}

extern void map_normal(const lv_area_t * disp_area, lv_color_t * disp_buf,
                                             const lv_area_t * draw_area,
                                             const lv_area_t * map_area, const lv_color_t * map_buf, lv_opa_t opa,
                                             const lv_opa_t * mask, lv_draw_mask_res_t mask_res);
static int32_t map_normal_handler(func_proxy_data_t *in, func_proxy_data_t *out)
{
    // 参数检查
    if (!in || !in->data || in->size != sizeof(struct rpc_map_normal_args) ||
        !out || !out->data || out->size < sizeof(struct rpc_map_normal_result)) {
        CLOGE("[%s] invalid parameters", __func__);
        return -1;
    }

    struct rpc_map_normal_args *args = (struct rpc_map_normal_args *)in->data;

    uint32_t addr = (uint32_t )args->disp_buf;
//第一种方式虽然flush更多区域，但代码开销更小
#if 1
    dcache_invalidate_range((uint32_t )args->disp_buf, 
        (uint32_t)args->disp_buf + lv_area_get_width(&args->disp_area) * lv_area_get_height(&args->disp_area) * sizeof(lv_color_t));
    dcache_invalidate_range((uint32_t )args->map_buf, 
        (uint32_t)args->map_buf + lv_area_get_width(&args->map_area) * lv_area_get_height(&args->map_area) * sizeof(lv_color_t));
#else
    uint32_t area_height = lv_area_get_height(&args->draw_area);
    uint32_t area_width_size = lv_area_get_width(&args->draw_area) * sizeof(lv_color_t);
    for(int i = 0; i < area_height; i++){
        dcache_invalidate_range(addr, addr + area_width_size);
        addr += lv_area_get_width(&args->disp_area) * sizeof(lv_color_t);
    }

    addr = (uint32_t )args->map_buf;
    for(int i = 0; i < area_height; i++){
        dcache_invalidate_range(addr, addr + area_width_size);
        addr += lv_area_get_width(&args->map_area) * sizeof(lv_color_t);
    }
#endif

    dcache_invalidate_range((uint32_t )args->mask, 
        (uint32_t)args->mask + lv_area_get_width(&args->draw_area) * lv_area_get_height(&args->draw_area) * sizeof(lv_opa_t));
    map_normal(&args->disp_area, args->disp_buf, &args->draw_area, &args->map_area,
               args->map_buf, args->opa, args->mask, args->mask_res);

    addr = (uint32_t )args->disp_buf;
    dcache_clean_range(addr, addr + lv_area_get_width(&args->disp_area) * lv_area_get_height(&args->disp_area) * sizeof(lv_color_t));
    
    // addr = (uint32_t )args->map_buf;
    // dcache_clean_range(addr, addr + lv_area_get_width(&args->map_area) * lv_area_get_height(&args->map_area) * sizeof(lv_color_t));

    return 0;    
}

void lv_rpc_server_init(void)
{
    func_proxy_init();

    func_proxy_register(FUNC_NAME_FILL_NORMAL, fill_normal_handler);
    func_proxy_register(FUNC_NAME_MAP_NORMAL, map_normal_handler);
}

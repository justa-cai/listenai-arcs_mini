#ifndef LV_FUNC_PROXY_SERVER_H
#define LV_FUNC_PROXY_SERVER_H

#include <stdint.h>
#include "./lv_draw/lv_draw_blend.h"

#define FUNC_NAME_FILL_NORMAL  "fill_normal"
struct rpc_fill_normal_args {
    lv_area_t disp_area;
    lv_color_t *disp_buf;
    lv_area_t draw_area;
    lv_color_t color;
    lv_opa_t opa;
    lv_opa_t *mask;
    lv_draw_mask_res_t mask_res;
};

struct rpc_fill_normal_result {
    int32_t reserved;
};

#define FUNC_NAME_MAP_NORMAL  "map_normal"
struct rpc_map_normal_args {
    lv_area_t disp_area;
    lv_color_t * disp_buf;
    lv_area_t draw_area;
    lv_area_t map_area;
    lv_color_t * map_buf;
    lv_opa_t opa;
    lv_opa_t * mask;
    lv_draw_mask_res_t mask_res;
};

struct rpc_map_normal_result {
    int32_t reserved;
};

void lv_rpc_server_init(void);

#endif /* LV_FUNC_PROXY_SERVER_H */

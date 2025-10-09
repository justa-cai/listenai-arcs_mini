#ifndef LVGL_FUNC_PROXY_SERVER_H
#define LVGL_FUNC_PROXY_SERVER_H

#include <stdint.h>
#include "../src/draw/sw/lv_draw_sw.h"


#define FUNC_NAME_FILL_NORMAL  "fill_normal"
struct sw_fill_normal_args {
    lv_color_t *dest_buf;
    lv_area_t dest_area;
    lv_coord_t dest_stride;
    lv_color_t color;
    lv_opa_t opa;
    lv_opa_t *mask;
    lv_coord_t mask_stride;
};

struct sw_fill_normal_result {
    int32_t reserved;
};

#define FUNC_NAME_MAP_NORMAL  "map_normal"
struct sw_map_normal_args {
    lv_color_t *dest_buf;
    lv_area_t dest_area;
    lv_coord_t dest_stride;
    lv_color_t * src_buf;
    lv_coord_t src_stride;
    lv_opa_t opa;
    lv_opa_t *mask;
    lv_coord_t mask_stride;    
};

struct sw_map_normal_result {
    int32_t reserved;
};

#define FUNC_NAME_FILL_BLEND  "fill_blend"

struct sw_fill_blended_args {
    lv_color_t *dest_buf;
    lv_area_t dest_area;
    lv_coord_t dest_stride;
    lv_color_t color;
    lv_opa_t opa;
    lv_opa_t *mask;
    lv_coord_t mask_stride;
    lv_blend_mode_t blend_mode;
};

struct sw_fill_blended_result {
    int32_t reserved;
};

#define FUNC_NAME_MAP_BLEND  "map_blended"

struct sw_map_blended_args {
    lv_color_t *dest_buf;
    lv_area_t dest_area;
    lv_coord_t dest_stride;
    lv_color_t * src_buf;
    lv_coord_t src_stride;
    lv_opa_t opa;
    lv_opa_t *mask;
    lv_coord_t mask_stride;
    lv_blend_mode_t blend_mode;
};

struct sw_map_blended_result {
    int32_t reserved;
};

void lv_rpc_server_init(void);

#endif /* LVGL_FUNC_PROXY_SERVER_H */

/**
 * SPDX-License-Identifier: Apache-2.0
 */
#include "lvgl.h"

#include "lisaui_common.h"
#include "lisaui_type.h"
#include "lisaui_log.h"

static const char *TAG = "app_common";
lisaui_err_t lisaui_common_set_style_container(lv_obj_t *obj, lv_color_t bg_color, lv_opa_t bg_opa, lv_color_t border_color,
                                         lv_coord_t border_width, lv_coord_t radius)
{
    if (obj == NULL) {
        return LISAUI_ERR_INVALID_PARAM;
    }

    lv_obj_set_style_bg_color(obj, bg_color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(obj, bg_opa, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(obj, border_width, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(obj, radius, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    return LISAUI_ERR_OK;
}



lisaui_err_t lisaui_common_load_base_scr(void){

    static lv_obj_t *display = NULL;

    if(display == NULL){
        display = lv_obj_create(NULL); // launcher 页面根容器
    }
    lv_disp_load_scr(display);
    return LISAUI_ERR_OK;
}
                                         

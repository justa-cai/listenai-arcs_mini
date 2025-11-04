#ifndef _LISAUI_COMMON_H_
#define _LISAUI_COMMON_H_
#include "lvgl.h"
#include "lisaui_type.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LISAUI_STATUS_BAR_HEIGHT       42


lisaui_err_t lisaui_common_set_style_container(lv_obj_t *obj, lv_color_t bg_color, lv_opa_t bg_opa, lv_color_t border_color,
    lv_coord_t border_width, lv_coord_t radius);
    lisaui_err_t lisaui_common_load_base_scr(void);

#ifdef __cplusplus
}
#endif
#endif

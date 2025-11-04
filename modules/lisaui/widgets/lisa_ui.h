#ifndef __LISA_UI_H__
#define __LISA_UI_H__

#include "lvgl.h"

extern void lisa_ui_init(void);

/* 字体 */
LV_FONT_DECLARE(lv_font_notosans_cs_medium_14);
LV_FONT_DECLARE(lv_font_chinese_18);

/* 图片资源 */
extern const lv_img_dsc_t ui_img__status_battery_1_png;
extern const lv_img_dsc_t ui_img__status_battery_2_png;
extern const lv_img_dsc_t ui_img__status_battery_3_png;
extern const lv_img_dsc_t ui_img__status_battery_4_png;
extern const lv_img_dsc_t ui_img__status_battery_5_png;
extern const lv_img_dsc_t ui_img__status_charging_1_png;
extern const lv_img_dsc_t ui_img__status_charging_2_png;
extern const lv_img_dsc_t ui_img__status_charging_3_png;
extern const lv_img_dsc_t ui_img__status_charging_4_png;
extern const lv_img_dsc_t ui_img__status_charging_5_png;
extern const lv_img_dsc_t ui_img_icon_taskbar_png;

/* 颜色 */
#define LISA_UI_TEXT_COLOR_DEFAULT lv_color_hex(0xFFFFFF)

#include "lisa_log.h"

extern const lv_obj_class_t lisa_ui_base_class;

#endif /* __LISA_UI_H__ */

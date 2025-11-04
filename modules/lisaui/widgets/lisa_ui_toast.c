#include "lvgl.h"
#include "lisa_ui.h"
#include "lisa_ui_assets.h"

static lv_obj_t *toast_obj = NULL;
static lv_timer_t *toast_timer = NULL;

static void toast_hide_cb(lv_timer_t *timer)
{
    if (toast_obj) {
        lv_obj_del(toast_obj);
        toast_obj = NULL;
    }
    if (toast_timer) {
        lv_timer_del(toast_timer);
        toast_timer = NULL;
    }
}

void lisa_ui_toast_show(const char *txt)
{
    if (toast_obj) {
        toast_hide_cb(NULL);
    }

    toast_obj = lv_obj_create(lv_layer_sys());
    lv_obj_add_flag(toast_obj, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_size(toast_obj, lv_pct(80), LV_SIZE_CONTENT);
    lv_obj_set_style_radius(toast_obj, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(toast_obj, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(toast_obj, LV_OPA_80, LV_PART_MAIN);
    lv_obj_set_style_pad_all(toast_obj, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(toast_obj, 12, LV_PART_MAIN);

    lv_obj_t *label = lv_label_create(toast_obj);
    lv_label_set_text(label, txt);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_chinese_18, LV_PART_MAIN);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    lv_obj_align(toast_obj, LV_ALIGN_CENTER, 0, 0);

    toast_timer = lv_timer_create(toast_hide_cb, 2000, NULL);
}

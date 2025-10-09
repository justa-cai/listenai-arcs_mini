#include "lvgl.h"

struct lisa_ui_icon_btn {
    lv_obj_t obj;
    lv_obj_t *btn;
    lv_obj_t *icon;
    lv_obj_t *label;
};

const lv_obj_class_t lv_lisa_ui_icon_btn_class = {
    .base_class = &lv_btn_class,
    .instance_size = sizeof(struct lisa_ui_icon_btn)
};

#define MY_CLASS &lv_lisa_ui_icon_btn_class

LV_FONT_DECLARE(lv_font_chinese_18);

lv_obj_t *lisa_ui_icon_btn_create(lv_obj_t *parent, const void *icon_path, const char *label_text)
{
    lv_obj_t *obj = lv_obj_class_create_obj(MY_CLASS, parent);
    lv_obj_class_init_obj(obj);

    lv_obj_set_size(obj, LV_PCT(45), 60);
    lv_obj_set_style_radius(obj, 10, 0);
    lv_obj_set_style_shadow_width(obj, 0, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_outline_width(obj, 0, 0);

    lv_obj_set_layout(obj, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(obj, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(obj, 10, LV_PART_MAIN);

    struct lisa_ui_icon_btn *icon_btn = (struct lisa_ui_icon_btn *)obj;

    icon_btn->icon = lv_img_create(obj);
    lv_img_set_src(icon_btn->icon, icon_path);

    icon_btn->label = lv_label_create(obj);
    lv_label_set_text(icon_btn->label, label_text);
    lv_obj_align_to(icon_btn->label, icon_btn->icon, LV_ALIGN_OUT_RIGHT_MID, LV_DPX(20), 0);
    lv_obj_set_style_text_font(icon_btn->label, &lv_font_chinese_18, LV_PART_MAIN | LV_STATE_DEFAULT);

    return obj;
}

int lisa_ui_icon_btn_set_icon(lv_obj_t *obj, const char *icon_path)
{
    struct lisa_ui_icon_btn *icon_btn = (struct lisa_ui_icon_btn *)obj;
    lv_img_set_src(icon_btn->icon, icon_path);

    return 0;
}

int lisa_ui_icon_btn_set_label(lv_obj_t *obj, const char *label_text)
{
    struct lisa_ui_icon_btn *icon_btn = (struct lisa_ui_icon_btn *)obj;
    lv_label_set_text(icon_btn->label, label_text);

    return 0;
}

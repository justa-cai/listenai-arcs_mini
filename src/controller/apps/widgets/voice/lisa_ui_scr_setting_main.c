#include "lvgl.h"
#include "lisa_ui.h"
#include "lisa_ui_icon_btn.h"
#include "lisa_ui_scr_setting_main.h"
#include "lisa_ui_src_base.h"

extern const lv_obj_class_t lisa_ui_base_class;

struct lisa_ui_setting {
    lisa_ui_scr_base_t base;
    lv_obj_t *list;
    lisa_ui_setting_item_click_cb_t cb;
    void *user_data;
};

typedef struct lisa_ui_setting lisa_ui_setting_t;
static void lisa_ui_setting_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj);

const lv_obj_class_t lv_lisa_ui_setting_class = {
    .constructor_cb = lisa_ui_setting_constructor,
    .base_class = &lisa_ui_base_class,
    .instance_size = sizeof(lisa_ui_setting_t)
};

#define MY_CLASS &lv_lisa_ui_setting_class

static void lisa_ui_setting_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    lv_obj_t *container = lisa_ui_scr_base_container_get(obj);
    lisa_ui_setting_t *setting = (lisa_ui_setting_t *)obj;

    setting->list = lv_list_create(container);
}

lv_obj_t *lisa_ui_setting_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(MY_CLASS, parent);
    lv_obj_class_init_obj(obj);
    lisa_ui_setting_t *setting = (lisa_ui_setting_t *)obj;
    setting->cb = NULL;
    setting->user_data = NULL;


    lv_obj_set_style_bg_opa(setting->list, LV_OPA_30, LV_PART_MAIN);
    lv_obj_set_width(setting->list, LV_PCT(100));
    lv_obj_set_height(setting->list, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(setting->list, 15, LV_PART_MAIN);

    return obj;
}

static void list_btn_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);

    if (code == LV_EVENT_CLICKED) {
        lisa_ui_setting_t *setting = (lisa_ui_setting_t *)lv_event_get_user_data(e);
        if (setting->cb) {
            const char *name = lv_list_get_btn_text(setting->list, obj);
            setting->cb(name, setting->user_data);
        }
    }
}

int lisa_ui_setting_item_add(lv_obj_t *obj, const char *label, const void *icon)
{
    lisa_ui_setting_t *setting = (lisa_ui_setting_t *)obj;

    lv_obj_t *btn = lv_list_add_btn(setting->list, icon, label);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_text_font(btn, &lv_font_chinese_18, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 10, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_add_event_cb(btn, list_btn_event_handler, LV_EVENT_CLICKED, obj);

    static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_border_side(&style, LV_BORDER_SIDE_BOTTOM);
    lv_style_set_border_width(&style, 1);
    lv_style_set_border_color(&style, lv_color_hex(0xFABE00));
    lv_obj_add_style(btn, &style, LV_STATE_PRESSED);

    lv_obj_set_style_flex_cross_place(btn, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);

    return 0;
}

int lisa_ui_setting_item_remove(lv_obj_t *obj, int index)
{
    lv_obj_t *icon_btn = lv_obj_get_child(obj, index);
    lv_obj_del(icon_btn);

    return 0;
}

void lisa_ui_setting_item_click_cb_set(lv_obj_t *obj, lisa_ui_setting_item_click_cb_t cb, void *user_data)
{
    lisa_ui_setting_t *setting = (lisa_ui_setting_t *)obj;
    setting->cb = cb;
    setting->user_data = user_data;
}

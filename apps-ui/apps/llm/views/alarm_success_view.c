/**
 * @file alarm_success_view.c
 * @brief Alarm success page view implementation
 */

#include "alarm_success_view.h"
#include "lisa_ui.h"
#include "lisa_ui_assets.h"
#include "lisa_ui_fonts.h"

#define TAG "alarm_success_view"

typedef struct {
    lv_obj_t obj;
    lv_obj_t *title_label;
    lv_obj_t *time_label;
    lv_obj_t *date_label;
} alarm_success_view_t;

static void alarm_success_view_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    LV_UNUSED(class_p);
    
    alarm_success_view_t *view = (alarm_success_view_t *)obj;
    
    lv_obj_set_size(obj, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(obj, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    
    view->title_label = lv_label_create(obj);
    lv_label_set_text(view->title_label, _("Alarm set successfully"));
    lv_obj_set_style_text_color(view->title_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(view->title_label, &lv_font_chinese_16, LV_PART_MAIN);
    lv_obj_align(view->title_label, LV_ALIGN_TOP_MID, 0, 30);
    
    view->time_label = lv_label_create(obj);
    lv_label_set_text(view->time_label, "00:00");
    lv_obj_set_style_text_color(view->time_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(view->time_label, &lv_font_rubik_bold_64, LV_PART_MAIN);
    lv_obj_align(view->time_label, LV_ALIGN_CENTER, 0, -30);
    
    view->date_label = lv_label_create(obj);
    lv_label_set_text(view->date_label, "");
    lv_obj_set_style_text_color(view->date_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(view->date_label, &lv_font_chinese_16, LV_PART_MAIN);
    lv_obj_align_to(view->date_label, view->time_label, LV_ALIGN_OUT_BOTTOM_MID, -50, 30);
}

static void alarm_success_view_destructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    LV_UNUSED(class_p);
    LV_UNUSED(obj);
}

const lv_obj_class_t alarm_success_view_class = {
    .constructor_cb = alarm_success_view_constructor,
    .destructor_cb = alarm_success_view_destructor,
    .instance_size = sizeof(alarm_success_view_t),
    .base_class = &lv_obj_class
};

lv_obj_t *alarm_success_view_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(&alarm_success_view_class, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

void alarm_success_view_set_time(lv_obj_t *obj, const char *time_str)
{
    if (!obj || !time_str) return;
    
    alarm_success_view_t *view = (alarm_success_view_t *)obj;
    if (view->time_label) {
        lv_label_set_text(view->time_label, time_str);
    }
}

void alarm_success_view_set_date(lv_obj_t *obj, const char *date_str)
{
    if (!obj || !date_str) return;
    
    alarm_success_view_t *view = (alarm_success_view_t *)obj;
    if (view->date_label) {
        lv_label_set_text(view->date_label, date_str);
    }
}

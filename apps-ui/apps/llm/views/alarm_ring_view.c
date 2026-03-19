/**
 * @file alarm_ring_view.c
 * @brief Alarm ring page view implementation
 */

#include "alarm_ring_view.h"
#include "lisa_ui.h"
#include "lisa_ui_assets.h"
#include "lisa_ui_fonts.h"
#include <stdio.h>

#define TAG "alarm_ring_view"

/* 声明闹钟PNG图标 */
LV_IMG_DECLARE(icons_Icon_alarm_png);

typedef struct {
    lv_obj_t obj;
    lv_obj_t *title_label;
    lv_obj_t *time_label;
    lv_obj_t *date_label;
    lv_obj_t *ring_icon;
    lv_obj_t *stop_btn;
    lv_timer_t *blink_timer;
    bool is_blinking;
} alarm_ring_view_t;

static void blink_timer_cb(lv_timer_t *timer)
{
    lv_obj_t *obj = (lv_obj_t *)timer->user_data;
    if (!obj) return;
    
    alarm_ring_view_t *view = (alarm_ring_view_t *)obj;
    if (!view->ring_icon) return;
    
    view->is_blinking = !view->is_blinking;
    
    if (view->is_blinking) {
        lv_obj_set_style_opa(view->ring_icon, LV_OPA_30, LV_PART_MAIN);
    } else {
        lv_obj_set_style_opa(view->ring_icon, LV_OPA_COVER, LV_PART_MAIN);
    }
}

static void alarm_ring_view_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    LV_UNUSED(class_p);
    
    alarm_ring_view_t *view = (alarm_ring_view_t *)obj;
    
    view->blink_timer = NULL;
    view->is_blinking = false;
    
    lv_obj_set_size(obj, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(obj, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    
    view->title_label = lv_label_create(obj);
    if (!view->title_label) {
        LISA_UI_LOGE("Failed to create title label");
        return;
    }
    lv_label_set_text(view->title_label, _("Alarm Reminder"));
    lv_obj_set_style_text_color(view->title_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(view->title_label, &lv_font_chinese_16, LV_PART_MAIN);
    lv_obj_align(view->title_label, LV_ALIGN_TOP_MID, 0, 20);
    
    view->time_label = lv_label_create(obj);
    if (!view->time_label) {
        LISA_UI_LOGE("Failed to create time label");
        return;
    }
    lv_label_set_text(view->time_label, "00:00");
    lv_obj_set_style_text_color(view->time_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(view->time_label, &lv_font_rubik_bold_64, LV_PART_MAIN);
    lv_obj_align(view->time_label, LV_ALIGN_CENTER, 0, -40);
    
    view->date_label = lv_label_create(obj);
    if (!view->date_label) {
        LISA_UI_LOGE("Failed to create date label");
        return;
    }
    lv_label_set_text(view->date_label, "");
    lv_obj_set_style_text_color(view->date_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(view->date_label, &lv_font_chinese_16, LV_PART_MAIN);
    lv_obj_align_to(view->date_label, view->time_label, LV_ALIGN_OUT_BOTTOM_MID, -50, 5);
    
    view->ring_icon = lv_img_create(obj);
    if (!view->ring_icon) {
        LISA_UI_LOGE("Failed to create ring icon");
        return;
    }
    lv_img_set_src(view->ring_icon, &icons_Icon_alarm_png);
    lv_obj_align(view->ring_icon, LV_ALIGN_TOP_RIGHT, -20, 20);
    
    view->stop_btn = lv_btn_create(obj);
    if (!view->stop_btn) {
        LISA_UI_LOGE("Failed to create stop button");
        return;
    }
    lv_obj_set_size(view->stop_btn, 200, 40);
    lv_obj_set_style_bg_color(view->stop_btn, lv_color_hex(0xFF6B6B), LV_PART_MAIN);
    lv_obj_set_style_radius(view->stop_btn, 25, LV_PART_MAIN);
    lv_obj_set_style_border_width(view->stop_btn, 0, LV_PART_MAIN);
    lv_obj_align(view->stop_btn, LV_ALIGN_BOTTOM_MID, 0, -10);
    
    lv_obj_t *btn_label = lv_label_create(view->stop_btn);
    if (!btn_label) {
        LISA_UI_LOGE("Failed to create button label");
        return;
    }
    lv_label_set_text(btn_label, "唤醒或按键可终止闹铃");
    lv_obj_set_style_text_color(btn_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(btn_label, &lv_font_chinese_16, LV_PART_MAIN);
    lv_obj_align(btn_label, LV_ALIGN_CENTER, 0, -3);
    
    view->blink_timer = lv_timer_create(blink_timer_cb, 500, obj);
    if (!view->blink_timer) {
        LISA_UI_LOGE("Failed to create blink timer");
        return;
    }
    view->is_blinking = false;
}

static void alarm_ring_view_destructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    LV_UNUSED(class_p);
    
    alarm_ring_view_t *view = (alarm_ring_view_t *)obj;
    if (view->blink_timer) {
        lv_timer_del(view->blink_timer);
        view->blink_timer = NULL;
    }
}

const lv_obj_class_t alarm_ring_view_class = {
    .constructor_cb = alarm_ring_view_constructor,
    .destructor_cb = alarm_ring_view_destructor,
    .instance_size = sizeof(alarm_ring_view_t),
    .base_class = &lv_obj_class
};

lv_obj_t *alarm_ring_view_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(&alarm_ring_view_class, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

void alarm_ring_view_set_time(lv_obj_t *obj, const char *time_str)
{
    if (!obj || !time_str) return;
    
    alarm_ring_view_t *view = (alarm_ring_view_t *)obj;
    if (view->time_label) {
        lv_label_set_text(view->time_label, time_str);
    }
}

void alarm_ring_view_set_date(lv_obj_t *obj, const char *date_str)
{
    if (!obj || !date_str) return;
    
    alarm_ring_view_t *view = (alarm_ring_view_t *)obj;
    if (view->date_label) {
        lv_label_set_text(view->date_label, date_str);
    }
}

void alarm_ring_view_set_stop_cb(lv_obj_t *obj, lv_event_cb_t cb, void *user_data)
{
    if (!obj || !cb) return;
    
    alarm_ring_view_t *view = (alarm_ring_view_t *)obj;
    if (view->stop_btn) {
        lv_obj_add_event_cb(view->stop_btn, cb, LV_EVENT_CLICKED, user_data);
    }
}

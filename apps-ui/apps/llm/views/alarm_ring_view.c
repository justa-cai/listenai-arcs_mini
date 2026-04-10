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
#define ALARM_RING_TEXT_VERTICAL_PADDING 16
#define ALARM_RING_TEXT_MAX_WIDTH_PERCENT 80
#define ALARM_RING_TEXT_SCROLL_SPEED 20
#define ALARM_RING_TOAST_HIDE_MS 5000

/* 声明闹钟PNG图标 */
LV_IMG_DECLARE(icons_Icon_alarm_png);
LV_IMG_DECLARE(icons_icon_finger_png);

typedef struct {
    lv_obj_t obj;
    lv_obj_t *title_label;
    lv_obj_t *time_label;
    lv_obj_t *date_label;
    lv_obj_t *text_label;
    lv_obj_t *finger_icon;
    lv_obj_t *hint_label;
    lv_obj_t *ring_icon;
    lv_obj_t *stop_btn;
    lv_obj_t *toast_obj;
    lv_obj_t *toast_label;
    lv_timer_t *toast_timer;
    lv_timer_t *blink_timer;
    bool is_blinking;
} alarm_ring_view_t;

static void alarm_ring_view_update_text_label_layout(alarm_ring_view_t *view, const char *text)
{
    lv_point_t text_size = {0};
    lv_coord_t pad_left;
    lv_coord_t pad_right;
    lv_coord_t letter_space;
    lv_coord_t line_space;
    lv_coord_t single_line_height;
    lv_coord_t max_width;
    lv_coord_t target_width;

    if (!view || !view->text_label || !text) {
        return;
    }

    lv_obj_update_layout((lv_obj_t *)view);

    pad_left = lv_obj_get_style_pad_left(view->text_label, LV_PART_MAIN);
    pad_right = lv_obj_get_style_pad_right(view->text_label, LV_PART_MAIN);
    letter_space = lv_obj_get_style_text_letter_space(view->text_label, LV_PART_MAIN);
    line_space = lv_obj_get_style_text_line_space(view->text_label, LV_PART_MAIN);
    single_line_height = lv_font_get_line_height(&lv_font_chinese_16) + ALARM_RING_TEXT_VERTICAL_PADDING;
    max_width = lv_obj_get_content_width((lv_obj_t *)view) * ALARM_RING_TEXT_MAX_WIDTH_PERCENT / 100;
    if (max_width <= 0) {
        max_width = 1;
    }

    lv_txt_get_size(&text_size, text, &lv_font_chinese_16, letter_space, line_space, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    target_width = text_size.x + pad_left + pad_right;

    if (target_width > max_width) {
        lv_label_set_long_mode(view->text_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_size(view->text_label, max_width, single_line_height);
    } else {
        lv_label_set_long_mode(view->text_label, LV_LABEL_LONG_CLIP);
        lv_obj_set_size(view->text_label, target_width, single_line_height);
    }

    lv_obj_set_style_anim_speed(view->text_label, ALARM_RING_TEXT_SCROLL_SPEED, LV_PART_MAIN);
    lv_obj_set_style_text_align(view->text_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
}

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

static void alarm_ring_view_toast_hide(alarm_ring_view_t *view)
{
    if (!view || !view->toast_obj) {
        return;
    }

    lv_obj_add_flag(view->toast_obj, LV_OBJ_FLAG_HIDDEN);
    if (view->toast_timer) {
        lv_timer_pause(view->toast_timer);
    }
}

static void toast_timer_cb(lv_timer_t *timer)
{
    lv_obj_t *obj = (lv_obj_t *)timer->user_data;
    if (!obj) {
        return;
    }

    alarm_ring_view_t *view = (alarm_ring_view_t *)obj;
    alarm_ring_view_toast_hide(view);
}

static void alarm_ring_view_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    LV_UNUSED(class_p);
    
    alarm_ring_view_t *view = (alarm_ring_view_t *)obj;
    
    view->blink_timer = NULL;
    view->toast_timer = NULL;
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
    lv_obj_align_to(view->date_label, view->time_label, LV_ALIGN_OUT_BOTTOM_MID, -25, 5);

    view->text_label = lv_label_create(obj);
    if (!view->text_label) {
        LISA_UI_LOGE("Failed to create text label");
        return;
    }
    lv_label_set_text(view->text_label, "");
    lv_obj_set_style_text_color(view->text_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(view->text_label, &lv_font_chinese_16, LV_PART_MAIN);
    lv_coord_t single_line_height = lv_font_get_line_height(&lv_font_chinese_16) + ALARM_RING_TEXT_VERTICAL_PADDING;
    lv_obj_set_size(view->text_label, LV_SIZE_CONTENT, single_line_height);
    lv_label_set_long_mode(view->text_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_anim_speed(view->text_label, ALARM_RING_TEXT_SCROLL_SPEED, LV_PART_MAIN);
    lv_obj_set_style_text_align(view->text_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    /* 添加背景框样式 */
    lv_obj_set_style_bg_color(view->text_label, lv_color_hex(0x2A2A2A), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(view->text_label, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(view->text_label, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_left(view->text_label, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_right(view->text_label, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_top(view->text_label, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(view->text_label, 8, LV_PART_MAIN);

    lv_obj_align_to(view->text_label, view->time_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 40);
    lv_obj_add_flag(view->text_label, LV_OBJ_FLAG_HIDDEN);

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

    lv_obj_add_flag(view->stop_btn, LV_OBJ_FLAG_HIDDEN);

    /* 创建手指图标 - 紧靠屏幕左下角 */
    view->finger_icon = lv_img_create(obj);
    if (!view->finger_icon) {
        LISA_UI_LOGE("Failed to create finger icon");
        return;
    }
    lv_img_set_src(view->finger_icon, &icons_icon_finger_png);
    lv_obj_align(view->finger_icon, LV_ALIGN_BOTTOM_LEFT, 0, -10);

    /* 创建提示文本 - 在手指图标右侧 */
    view->hint_label = lv_label_create(obj);
    if (!view->hint_label) {
        LISA_UI_LOGE("Failed to create hint label");
        return;
    }
    lv_label_set_text(view->hint_label, "单击: 稍后提醒\n长按: 关闭闹钟");
    lv_obj_set_style_text_color(view->hint_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(view->hint_label, &lv_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_style_text_align(view->hint_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_width(view->hint_label, LV_PCT(80));
    lv_label_set_long_mode(view->hint_label, LV_LABEL_LONG_WRAP);
    lv_obj_align_to(view->hint_label, view->finger_icon, LV_ALIGN_OUT_RIGHT_MID, 8, 0);

    view->toast_obj = lv_obj_create(obj);
    if (!view->toast_obj) {
        LISA_UI_LOGE("Failed to create toast object");
        return;
    }
    lv_obj_add_flag(view->toast_obj, LV_OBJ_FLAG_FLOATING | LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(view->toast_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(view->toast_obj, LV_PCT(95), LV_SIZE_CONTENT);
    lv_obj_set_style_radius(view->toast_obj, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(view->toast_obj, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(view->toast_obj, LV_OPA_80, LV_PART_MAIN);
    lv_obj_set_style_border_width(view->toast_obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(view->toast_obj, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(view->toast_obj, 12, LV_PART_MAIN);
    lv_obj_align(view->toast_obj, LV_ALIGN_CENTER, 0, 0);

    view->toast_label = lv_label_create(view->toast_obj);
    if (!view->toast_label) {
        LISA_UI_LOGE("Failed to create toast label");
        return;
    }
    lv_label_set_text(view->toast_label, "");
    lv_obj_set_width(view->toast_label, LV_PCT(100));
    lv_obj_set_style_text_color(view->toast_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(view->toast_label, &lv_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_style_text_align(view->toast_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(view->toast_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(view->toast_label, LV_ALIGN_CENTER, 0, 0);

    view->toast_timer = lv_timer_create(toast_timer_cb, ALARM_RING_TOAST_HIDE_MS, obj);
    if (!view->toast_timer) {
        LISA_UI_LOGE("Failed to create toast timer");
        return;
    }
    lv_timer_pause(view->toast_timer);
    
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
    if (view->toast_timer) {
        lv_timer_del(view->toast_timer);
        view->toast_timer = NULL;
    }
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

void alarm_ring_view_set_text(lv_obj_t *obj, const char *text)
{
    if (!obj) return;

    alarm_ring_view_t *view = (alarm_ring_view_t *)obj;
    if (!view || !view->text_label) {
        return;
    }

    if (!text || text[0] == '\0') {
        lv_label_set_text(view->text_label, "");
        lv_obj_add_flag(view->text_label, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_clear_flag(view->text_label, LV_OBJ_FLAG_HIDDEN);

    lv_label_set_text(view->text_label, text);
    alarm_ring_view_update_text_label_layout(view, text);
    lv_obj_align(view->text_label, LV_ALIGN_CENTER, 0, 50);
}

void alarm_ring_view_set_hint(lv_obj_t *obj, const char *hint)
{
    if (!obj || !hint) return;

    alarm_ring_view_t *view = (alarm_ring_view_t *)obj;
    if (!view || !view->hint_label) {
        return;
    }

    lv_label_set_text(view->hint_label, hint);
}

void alarm_ring_view_show_toast(lv_obj_t *obj, const char *text)
{
    if (!obj || !text) {
        return;
    }

    alarm_ring_view_t *view = (alarm_ring_view_t *)obj;
    if (!view->toast_obj || !view->toast_label || !view->toast_timer) {
        return;
    }

    lv_label_set_text(view->toast_label, text);
    lv_obj_clear_flag(view->toast_obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(view->toast_obj);
    lv_timer_reset(view->toast_timer);
    lv_timer_resume(view->toast_timer);
}

void alarm_ring_view_set_stop_cb(lv_obj_t *obj, lv_event_cb_t cb, void *user_data)
{
    if (!obj || !cb) return;
    
    alarm_ring_view_t *view = (alarm_ring_view_t *)obj;
    if (view->stop_btn) {
        lv_obj_add_event_cb(view->stop_btn, cb, LV_EVENT_CLICKED, user_data);
    }
}

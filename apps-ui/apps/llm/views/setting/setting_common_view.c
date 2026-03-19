/**
 * @file setting_common_view.c
 * @brief Common settings view implementation (Volume, Brightness)
 */

#define TAG "setting_common_view"

#include "setting_common_view.h"
#include "lisa_ui.h"
#include "lisa_ui_llm_base.h"
#include "lisa_ui_nav_scr.h"
#include "lisa_ui_nav_scr_ids.h"
#include "lisa_ui_assets.h"
#include "stdio.h"
#include "lisa_ui_fonts.h"

typedef struct {
    lisa_ui_llm_base_t base;
    lv_obj_t *back_btn;
    lv_obj_t *volume_label;
    lv_obj_t *brightness_label;
    lv_obj_t *volume_slider;
    lv_obj_t *brightness_slider;
    lisa_ui_setting_common_back_cb_t back_cb;
    void *back_user_data;
    lisa_ui_setting_common_volume_changed_cb_t volume_changed_cb;
    void *volume_user_data;
    lisa_ui_setting_common_brightness_changed_cb_t brightness_changed_cb;
    void *brightness_user_data;
} lisa_ui_setting_common_view_t;

static void lisa_ui_setting_common_view_class_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj);

const lv_obj_class_t lisa_ui_setting_common_view_class = {
    .base_class = &lisa_ui_llm_base_class,
    .constructor_cb = lisa_ui_setting_common_view_class_constructor,
    .instance_size = sizeof(lisa_ui_setting_common_view_t),
};

static void back_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        lisa_ui_setting_common_view_t *view = (lisa_ui_setting_common_view_t *)lv_event_get_user_data(e);
        LISA_UI_LOGD("Back button clicked");
        if (view && view->back_cb) {
            view->back_cb(view->back_user_data);
        }
    }
}

static void volume_slider_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *slider = lv_event_get_target(e);
        int32_t value = lv_slider_get_value(slider);
        lisa_ui_setting_common_view_t *view = (lisa_ui_setting_common_view_t *)lv_event_get_user_data(e);
        
        if (view && view->volume_label && lv_obj_is_valid(view->volume_label)) {
            static char volume_text[32];
            snprintf(volume_text, sizeof(volume_text), _("volume: %d%%"), (int)value);
            lv_label_set_text(view->volume_label, volume_text);
        }
        
        LISA_UI_LOGD("Volume changed to: %d", (int)value);
        
        /* 调用业务逻辑回调 */
        if (view && view->volume_changed_cb) {
            view->volume_changed_cb((uint8_t)value, view->volume_user_data);
        }
    }
}

static void brightness_slider_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *slider = lv_event_get_target(e);
        int32_t value = lv_slider_get_value(slider);
        lisa_ui_setting_common_view_t *view = (lisa_ui_setting_common_view_t *)lv_event_get_user_data(e);
        
        if (view && view->brightness_label && lv_obj_is_valid(view->brightness_label)) {
            static char brightness_text[32];
            snprintf(brightness_text, sizeof(brightness_text), _("brightness: %d%%"), (int)value);
            lv_label_set_text(view->brightness_label, brightness_text);
        }
        
        LISA_UI_LOGD("Brightness changed to: %d", (int)value);
        
        /* 调用业务逻辑回调 */
        if (view && view->brightness_changed_cb) {
            view->brightness_changed_cb((uint8_t)value, view->brightness_user_data);
        }
    }
}

static void lisa_ui_setting_common_view_class_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    LV_UNUSED(class_p);
    
    lisa_ui_setting_common_view_t *view = (lisa_ui_setting_common_view_t *)obj;
    
    // Hide the bar to allow content to start from the very top
    lv_obj_t *bar = lisa_ui_llm_base_bar_get(obj);
    if (bar) {
        lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);
    }
    
    lv_obj_t *container = lisa_ui_llm_base_container_get(obj);
    if (NULL == container) {
        LISA_UI_LOGE("Failed to get base container");
        return;
    }
    
    // Back button at the very top
    view->back_btn = lv_btn_create(container);
    lv_obj_set_size(view->back_btn, 30, 30);
    lv_obj_align(view->back_btn, LV_ALIGN_TOP_LEFT, 5, 0);
    lv_obj_set_style_bg_opa(view->back_btn, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(view->back_btn, 0, LV_PART_MAIN);
    
    lv_obj_t *back_icon = lv_img_create(view->back_btn);
    lv_img_set_src(back_icon, &icons_icon_back_png);
    lv_obj_center(back_icon);
    lv_obj_add_event_cb(view->back_btn, back_btn_event_cb, LV_EVENT_CLICKED, view);
    
    /* 在返回按钮旁边创建标题文本 */
    lv_obj_t *title = lv_label_create(container);
    lv_label_set_text(title, _("common setting"));
    lv_obj_set_style_text_font(title, &lv_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 45, 8);  /* 紧贴返回按钮右侧 */
    
    view->volume_label = lv_label_create(container);
    lv_obj_set_style_text_color(view->volume_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(view->volume_label, &lv_font_chinese_16, LV_PART_MAIN);
    lv_obj_align(view->volume_label, LV_ALIGN_TOP_LEFT, 20, 80);
    
    view->volume_slider = lv_slider_create(container);
    lv_obj_set_width(view->volume_slider, 280);
    lv_obj_align_to(view->volume_slider, view->volume_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);
    lv_slider_set_range(view->volume_slider, 0, 100);
    lv_slider_set_value(view->volume_slider, 50, LV_ANIM_OFF);
    lv_obj_add_event_cb(view->volume_slider, volume_slider_event_cb, LV_EVENT_VALUE_CHANGED, view);
    
    view->brightness_label = lv_label_create(container);
    lv_obj_set_style_text_color(view->brightness_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(view->brightness_label, &lv_font_chinese_16, LV_PART_MAIN);
    lv_obj_align_to(view->brightness_label, view->volume_slider, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 30);
    
    view->brightness_slider = lv_slider_create(container);
    lv_obj_set_width(view->brightness_slider, 280);
    lv_obj_align_to(view->brightness_slider, view->brightness_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);
    lv_slider_set_range(view->brightness_slider, 0, 100);
    lv_slider_set_value(view->brightness_slider, 50, LV_ANIM_OFF);
    lv_obj_add_event_cb(view->brightness_slider, brightness_slider_event_cb, LV_EVENT_VALUE_CHANGED, view);
}

lv_obj_t *lisa_ui_setting_common_view_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(&lisa_ui_setting_common_view_class, parent);
    lv_obj_class_init_obj(obj);
    
    lisa_ui_setting_common_view_t *view = (lisa_ui_setting_common_view_t *)obj;
    view->back_cb = NULL;
    view->back_user_data = NULL;
    view->volume_changed_cb = NULL;
    view->volume_user_data = NULL;
    view->brightness_changed_cb = NULL;
    view->brightness_user_data = NULL;
    
    return obj;
}

void lisa_ui_setting_common_view_set_volume(lv_obj_t *obj, uint8_t volume)
{
    if (!obj || volume > 100) {
        return;
    }
    
    lisa_ui_setting_common_view_t *view = (lisa_ui_setting_common_view_t *)obj;
    
    if (view->volume_slider && lv_obj_is_valid(view->volume_slider)) {
        lv_slider_set_value(view->volume_slider, volume, LV_ANIM_OFF);
    }
    
    if (view->volume_label && lv_obj_is_valid(view->volume_label)) {
        static char volume_text[32];
        snprintf(volume_text, sizeof(volume_text),  _("volume: %d%%"), volume);
        lv_label_set_text(view->volume_label, volume_text);
    }
}

void lisa_ui_setting_common_view_set_brightness(lv_obj_t *obj, uint8_t brightness)
{
    if (!obj || brightness > 100) {
        return;
    }
    
    lisa_ui_setting_common_view_t *view = (lisa_ui_setting_common_view_t *)obj;
    
    if (view->brightness_slider && lv_obj_is_valid(view->brightness_slider)) {
        lv_slider_set_value(view->brightness_slider, brightness, LV_ANIM_OFF);
    }
    
    if (view->brightness_label && lv_obj_is_valid(view->brightness_label)) {
        static char brightness_text[32];
        snprintf(brightness_text, sizeof(brightness_text), _("brightness: %d%%"), brightness);
        lv_label_set_text(view->brightness_label, brightness_text);
    }
}

void lisa_ui_setting_common_view_set_back_cb(lv_obj_t *obj, lisa_ui_setting_common_back_cb_t cb, void *user_data)
{
    if (!obj) {
        return;
    }
    
    lisa_ui_setting_common_view_t *view = (lisa_ui_setting_common_view_t *)obj;
    view->back_cb = cb;
    view->back_user_data = user_data;
}

void lisa_ui_setting_common_view_set_volume_changed_cb(lv_obj_t *obj, lisa_ui_setting_common_volume_changed_cb_t cb, void *user_data)
{
    if (!obj) {
        return;
    }
    
    lisa_ui_setting_common_view_t *view = (lisa_ui_setting_common_view_t *)obj;
    view->volume_changed_cb = cb;
    view->volume_user_data = user_data;
}

void lisa_ui_setting_common_view_set_brightness_changed_cb(lv_obj_t *obj, lisa_ui_setting_common_brightness_changed_cb_t cb, void *user_data)
{
    if (!obj) {
        return;
    }
    
    lisa_ui_setting_common_view_t *view = (lisa_ui_setting_common_view_t *)obj;
    view->brightness_changed_cb = cb;
    view->brightness_user_data = user_data;
}

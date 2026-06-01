#include "log_upload_view.h"

#include <stdio.h>

#include "lisa_ui_assets.h"
#include "lisa_ui_fonts.h"
#include "lisa_ui_llm_base.h"

LV_IMG_DECLARE(icons_icon_finger_png);

typedef struct {
    lisa_ui_llm_base_t base;
    lv_obj_t *status_label;
    lv_obj_t *result_label;
    lv_obj_t *finger_icon;
    lv_obj_t *finger_hint_label;
    char status_text[64];
} lisa_ui_log_upload_view_t;

static void log_upload_view_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj);
static void log_upload_view_destructor(const lv_obj_class_t *class_p, lv_obj_t *obj);

const lv_obj_class_t lisa_ui_log_upload_view_class = {
    .base_class = &lisa_ui_llm_base_class,
    .instance_size = sizeof(lisa_ui_log_upload_view_t),
    .constructor_cb = log_upload_view_constructor,
    .destructor_cb = log_upload_view_destructor,
};

static void log_upload_view_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    lv_obj_t *bar;
    lv_obj_t *container;
    lisa_ui_log_upload_view_t *view = (lisa_ui_log_upload_view_t *)obj;

    LV_UNUSED(class_p);

    bar = lisa_ui_llm_base_bar_get(obj);
    if (bar) {
        lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);
    }

    container = lisa_ui_llm_base_container_get(obj);
    if (!container) {
        return;
    }

    view->status_label = lv_label_create(container);
    lv_label_set_text(view->status_label, "准备日志中…");
    lv_label_set_long_mode(view->status_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(view->status_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(view->status_label, &lv_font_chinese_16, 0);
    lv_obj_set_width(view->status_label, lv_pct(100));
    lv_obj_set_style_text_align(view->status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(view->status_label, LV_ALIGN_CENTER, 0, -16);

    view->result_label = lv_label_create(container);
    lv_label_set_text(view->result_label, "");
    lv_label_set_long_mode(view->result_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(view->result_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(view->result_label, &lv_font_chinese_16, 0);
    lv_obj_set_style_text_opa(view->result_label, LV_OPA_70, 0);
    lv_obj_set_width(view->result_label, lv_pct(100));
    lv_obj_set_style_text_align(view->result_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(view->result_label, LV_ALIGN_CENTER, 0, 8);

    view->finger_icon = lv_img_create(obj);
    lv_img_set_src(view->finger_icon, &icons_icon_finger_png);
    lv_obj_add_flag(view->finger_icon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(view->finger_icon, LV_OBJ_FLAG_IGNORE_LAYOUT | LV_OBJ_FLAG_FLOATING);
    lv_obj_align(view->finger_icon, LV_ALIGN_BOTTOM_LEFT, 0, -10);
    lv_obj_move_foreground(view->finger_icon);

    view->finger_hint_label = lv_label_create(obj);
    lv_label_set_text(view->finger_hint_label, "");
    lv_obj_set_style_text_color(view->finger_hint_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(view->finger_hint_label, &lv_font_chinese_16, 0);
    lv_obj_set_style_text_align(view->finger_hint_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_width(view->finger_hint_label, LV_PCT(70));
    lv_label_set_long_mode(view->finger_hint_label, LV_LABEL_LONG_WRAP);
    lv_obj_add_flag(view->finger_hint_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(view->finger_hint_label, LV_OBJ_FLAG_IGNORE_LAYOUT | LV_OBJ_FLAG_FLOATING);
    lv_obj_align_to(view->finger_hint_label, view->finger_icon, LV_ALIGN_OUT_RIGHT_MID, 8, 0);
    lv_obj_move_foreground(view->finger_hint_label);
}

static void log_upload_view_destructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    LV_UNUSED(class_p);
    LV_UNUSED(obj);
}

lv_obj_t *lisa_ui_log_upload_view_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(&lisa_ui_log_upload_view_class, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}


void lisa_ui_log_upload_view_update(lv_obj_t *obj, const log_upload_state_t *state)
{
    lisa_ui_log_upload_view_t *view;

    if (!obj || !state || !lv_obj_has_class(obj, &lisa_ui_log_upload_view_class)) {
        return;
    }

    view = (lisa_ui_log_upload_view_t *)obj;

    switch (state->state) {
    case LOG_UPLOAD_STATE_STARTING:
        lv_label_set_text_static(view->status_label, "准备日志中…");
        lv_label_set_text_static(view->result_label, "请稍后");
        break;
    case LOG_UPLOAD_STATE_UPLOADING:
        lv_label_set_text_static(view->status_label, "日志上传中，请稍后");
        lv_label_set_text_static(view->result_label, "");
        break;
    case LOG_UPLOAD_STATE_SUCCESSED:
        lv_label_set_text_static(view->status_label, "日志上传成功");
        lv_label_set_text_static(view->result_label, "即将返回首页");
        break;
    case LOG_UPLOAD_STATE_FAILED:
        lv_label_set_text_static(view->status_label, "日志上传失败");
        snprintf(view->status_text, sizeof(view->status_text), "错误码: %d", state->result);
        lv_label_set_text_static(view->result_label, view->status_text);
        break;
    default:
        lv_label_set_text_static(view->status_label, "日志上传");
        lv_label_set_text_static(view->result_label, "");
        break;
    }
}

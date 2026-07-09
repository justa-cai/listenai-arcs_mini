/**
 * @file ota_view.c
 * @brief OTA status view implementation
 */

#define TAG "ota_view"

#include "ota_view.h"
#include "lisa_ui.h"
#include "lisa_ui_assets.h"
#include "lisa_ui_llm_base.h"
#include "lisa_ui_fonts.h"
#include <stdio.h>

LV_IMG_DECLARE(icons_icon_finger_png);

typedef struct {
    lisa_ui_llm_base_t base;
    lv_obj_t *status_label;
    lv_obj_t *progress_label;
    lv_obj_t *package_info_label;
    lv_obj_t *eta_label;
    lv_obj_t *finger_icon;
    lv_obj_t *finger_hint_label;
    char status_text[64];
    char progress_text[224];
    char package_info_text[384];
    char eta_text[48];
} lisa_ui_ota_view_t;

static void ota_view_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj);
static void ota_view_destructor(const lv_obj_class_t *class_p, lv_obj_t *obj);
static void ota_view_show_checking(lisa_ui_ota_view_t *view, const ota_state_t *state);
static void ota_view_show_package_info(lisa_ui_ota_view_t *view, const ota_state_t *state);
static void ota_view_show_action_hint(lisa_ui_ota_view_t *view, const char *text);
static void ota_view_hide_action_hint(lisa_ui_ota_view_t *view);

const lv_obj_class_t lisa_ui_ota_view_class = {
    .base_class = &lisa_ui_llm_base_class,
    .instance_size = sizeof(lisa_ui_ota_view_t),
    .constructor_cb = ota_view_constructor,
    .destructor_cb = ota_view_destructor,
};

static void ota_view_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    LV_UNUSED(class_p);

    lisa_ui_ota_view_t *view = (lisa_ui_ota_view_t *)obj;

    lv_obj_t *bar = lisa_ui_llm_base_bar_get(obj);
    if (bar) {
        lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_t *container = lisa_ui_llm_base_container_get(obj);
    if (!container) {
        return;
    }

    view->status_label = lv_label_create(container);
    lv_label_set_text(view->status_label, "正在获取更新信息…");
    lv_label_set_long_mode(view->status_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(view->status_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(view->status_label, &lv_font_chinese_16, 0);
    lv_obj_set_width(view->status_label, lv_pct(100));
    lv_obj_set_style_text_align(view->status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(view->status_label, LV_ALIGN_CENTER, 0, -16);

    view->progress_label = lv_label_create(container);
    lv_label_set_text(view->progress_label, "");
    lv_label_set_long_mode(view->progress_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(view->progress_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(view->progress_label, &lv_font_chinese_16, 0);
    lv_obj_set_style_text_opa(view->progress_label, LV_OPA_70, 0);
    lv_obj_set_width(view->progress_label, lv_pct(100));
    lv_obj_set_style_text_align(view->progress_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(view->progress_label, LV_ALIGN_CENTER, 0, 8);

    view->package_info_label = lv_label_create(container);
    lv_label_set_text(view->package_info_label, "");
    lv_label_set_long_mode(view->package_info_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(view->package_info_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(view->package_info_label, &lv_font_chinese_16, 0);
    lv_obj_set_style_text_opa(view->package_info_label, LV_OPA_COVER, 0);
    lv_obj_set_width(view->package_info_label, lv_pct(100));
    lv_obj_set_style_text_align(view->package_info_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(view->package_info_label, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_add_flag(view->package_info_label, LV_OBJ_FLAG_HIDDEN);

    view->eta_label = lv_label_create(container);
    lv_label_set_text(view->eta_label, "");
    lv_label_set_long_mode(view->eta_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(view->eta_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(view->eta_label, &lv_font_chinese_16, 0);
    lv_obj_set_style_text_opa(view->eta_label, LV_OPA_50, 0);
    lv_obj_set_width(view->eta_label, lv_pct(100));
    lv_obj_set_style_text_align(view->eta_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(view->eta_label, LV_ALIGN_BOTTOM_MID, 0, -12);

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

static void ota_view_show_checking(lisa_ui_ota_view_t *view, const ota_state_t *state)
{
    const char *status_text = "正在检查更新…";

    if (view == NULL) {
        return;
    }

    if (state != NULL) {
        status_text = (state->target == OTA_TARGET_APP) ? "正在检查系统更新…" : "正在检查资源更新…";
    }

    lv_label_set_text_static(view->status_label, status_text);
    lv_label_set_text_static(view->progress_label, "");
    lv_label_set_text_static(view->eta_label, "");
}

static void ota_view_show_action_hint(lisa_ui_ota_view_t *view, const char *text)
{
    if (view == NULL || view->finger_icon == NULL || view->finger_hint_label == NULL) {
        return;
    }

    lv_label_set_text_static(view->finger_hint_label, text ? text : "");
    lv_obj_align(view->finger_icon, LV_ALIGN_BOTTOM_LEFT, 0, -10);
    lv_obj_align_to(view->finger_hint_label, view->finger_icon, LV_ALIGN_OUT_RIGHT_MID, 8, 0);
    lv_obj_move_foreground(view->finger_icon);
    lv_obj_move_foreground(view->finger_hint_label);
    lv_obj_clear_flag(view->finger_icon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(view->finger_hint_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(view->eta_label, LV_OBJ_FLAG_HIDDEN);
}

static void ota_view_hide_action_hint(lisa_ui_ota_view_t *view)
{
    if (view == NULL) {
        return;
    }

    if (view->finger_icon != NULL) {
        lv_obj_add_flag(view->finger_icon, LV_OBJ_FLAG_HIDDEN);
    }

    if (view->finger_hint_label != NULL) {
        lv_obj_add_flag(view->finger_hint_label, LV_OBJ_FLAG_HIDDEN);
    }

    if (view->eta_label != NULL) {
        lv_obj_clear_flag(view->eta_label, LV_OBJ_FLAG_HIDDEN);
    }
}

static void ota_view_show_package_info(lisa_ui_ota_view_t *view, const ota_state_t *state)
{
    if (view == NULL || state == NULL) {
        return;
    }

    if (state->target_version[0] != '\0') {
        snprintf(view->package_info_text, sizeof(view->package_info_text), "固件更新: %s -> %s\n%s",
                 state->current_version[0] ? state->current_version : "-",
                 state->target_version,
                 state->update_notes[0] ? state->update_notes : "请确认是否立即更新系统。");
    } else {
        snprintf(view->package_info_text, sizeof(view->package_info_text), "固件更新: %s\n\n%s",
                 state->current_version[0] ? state->current_version : "-",
                 state->update_notes[0] ? state->update_notes : "请确认是否立即更新系统。");
    }

    lv_obj_add_flag(view->status_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(view->progress_label, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text_static(view->package_info_label, view->package_info_text);
    lv_obj_clear_flag(view->package_info_label, LV_OBJ_FLAG_HIDDEN);
    ota_view_show_action_hint(view, "单击：更新\n长按：下次再说");
}

static void ota_view_destructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    (void)class_p;
    (void)obj;
}

lv_obj_t *lisa_ui_ota_view_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(&lisa_ui_ota_view_class, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

static const char *ota_target_name(ota_target_e target)
{
    switch (target) {
    case OTA_TARGET_APP:
        return "固件";
    case OTA_TARGET_WAKE_WORD:
        return "唤醒词";
    case OTA_TARGET_PROMPT_TONE:
        return "提示音";
    case OTA_TARGET_EMOJI:
        return "表情";
    default:
        return "资源";
    }
}

static void ota_view_format_size(char *buf, size_t buf_size, uint32_t bytes)
{
    if (bytes >= 1024 * 1024) {
        snprintf(buf, buf_size, "%.1fMB", (float)bytes / (1024.0f * 1024.0f));
    } else {
        snprintf(buf, buf_size, "%uKB", bytes / 1024);
    }
}

void lisa_ui_ota_view_update(lv_obj_t *obj, const ota_state_t *state)
{
    if (!obj || !lv_obj_has_class(obj, &lisa_ui_ota_view_class)) {
        return;
    }

    lisa_ui_ota_view_t *view = (lisa_ui_ota_view_t *)obj;
    if (state->state != OTA_STATE_PACKAGE_INFO) {
        ota_view_hide_action_hint(view);
        lv_obj_add_flag(view->package_info_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(view->status_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(view->progress_label, LV_OBJ_FLAG_HIDDEN);
    }

    switch (state->state) {
    case OTA_STATE_CHECKING:
        ota_view_show_checking(view, state);
        break;
    case OTA_STATE_PACKAGE_INFO:
        ota_view_show_package_info(view, state);
        break;
    case OTA_STATE_UPDATING: {
        // 第一行：N/M 正在更新XXX…
        if (state->target == OTA_TARGET_APP) {
            snprintf(view->status_text, sizeof(view->status_text), "正在下载系统更新…");
        } else if (state->update_total > 1) {
            snprintf(view->status_text, sizeof(view->status_text), "%u/%u 正在更新%s…",
                     state->update_index, state->update_total, ota_target_name(state->target));
        } else {
            snprintf(view->status_text, sizeof(view->status_text), "正在更新%s…",
                     ota_target_name(state->target));
        }
        lv_label_set_text_static(view->status_label, view->status_text);

        // 第二行：已下载 / 总大小
        char processed_str[16];
        char total_str[16];
        if (state->bytes_total > 0) {
            ota_view_format_size(processed_str, sizeof(processed_str), state->bytes_processed);
            ota_view_format_size(total_str, sizeof(total_str), state->bytes_total);
            snprintf(view->progress_text, sizeof(view->progress_text), "%s / %s", processed_str, total_str);
            lv_label_set_text_static(view->progress_label, view->progress_text);
        } else if (state->bytes_processed > 0) {
            ota_view_format_size(processed_str, sizeof(processed_str), state->bytes_processed);
            snprintf(view->progress_text, sizeof(view->progress_text), "已下载 %s", processed_str, total_str);
            lv_label_set_text_static(view->progress_label, view->progress_text);
        } else {
            lv_label_set_text_static(view->progress_label, "");
        }

        // 第三行：预计剩余时间
        if (state->bytes_processed < state->bytes_total) {
            if (state->bytes_processed > 0 && state->elapsed_ms > 0) {
                uint32_t remaining_bytes = state->bytes_total - state->bytes_processed;
                float speed = (float)state->bytes_processed / (float)state->elapsed_ms;
                uint32_t eta_sec = (uint32_t)((float)remaining_bytes / speed / 1000.0f);
                uint32_t eta_min = (eta_sec + 59) / 60;

                if (eta_min <= 1) {
                    snprintf(view->eta_text, sizeof(view->eta_text), "预计不到1分钟");
                } else {
                    snprintf(view->eta_text, sizeof(view->eta_text), "预计%u分钟", eta_min);
                }
            } else {
                snprintf(view->eta_text, sizeof(view->eta_text), "预计约1分钟");
            }
            lv_label_set_text_static(view->eta_label, view->eta_text);
        } else {
            lv_label_set_text_static(view->eta_label, "");
        }
        break;
    }
    case OTA_STATE_SUCCESSED:
        if (state->target == OTA_TARGET_APP) {
            lv_label_set_text_static(view->status_label, "下载完毕");
        } else {
            lv_label_set_text_static(view->status_label, "更新完毕");
        }
        lv_label_set_text_static(view->progress_label, "");
        if (state->reboot == OTA_REBOOT_STRATEGY_AUTO) {
            lv_label_set_text_static(view->eta_label, "正在重启…");
        } else {
            lv_label_set_text_static(view->eta_label, "请手动重启设备");
        }
        break;
    case OTA_STATE_APP_FAILED:
        lv_label_set_text_static(view->status_label, "系统更新失败");
        lv_label_set_text_static(view->progress_label, "");
        lv_label_set_text_static(view->eta_label, "");
        break;
    case OTA_STATE_RESOURCE_FAILED:
        lv_label_set_text_static(view->status_label, "资源更新失败");
        lv_label_set_text_static(view->progress_label, "");
        if (state->reboot == OTA_REBOOT_STRATEGY_AUTO) {
            lv_label_set_text_static(view->eta_label, "正在重启…");
        } else {
            lv_label_set_text_static(view->eta_label, "请手动重启设备");
        }
        break;
    case OTA_STATE_UP_TO_DATE:
        // lv_label_set_text_static(view->status_label, "已完成更新检查");
        // lv_label_set_text_static(view->progress_label, "");
        // lv_label_set_text_static(view->eta_label, "");
        break;
    default:
        break;
    }
}

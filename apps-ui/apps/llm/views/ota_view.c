/**
 * @file ota_view.c
 * @brief OTA status view implementation
 */

#define TAG "ota_view"

#include "ota_view.h"
#include "lisa_ui.h"
#include "lisa_ui_llm_base.h"
#include "lisa_ui_fonts.h"
#include <stdio.h>

typedef struct {
    lisa_ui_llm_base_t base;
    lv_obj_t *status_label;
    lv_obj_t *progress_label;
    lv_obj_t *eta_label;
    char status_text[48];
    char progress_text[32];
    char eta_text[32];
} lisa_ui_ota_view_t;

static void ota_view_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj);
static void ota_view_destructor(const lv_obj_class_t *class_p, lv_obj_t *obj);

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
    lv_label_set_text(view->status_label, "正在检查更新…");
    lv_obj_set_style_text_color(view->status_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(view->status_label, &lv_font_chinese_16, 0);
    lv_obj_set_width(view->status_label, lv_pct(100));
    lv_obj_set_style_text_align(view->status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(view->status_label, LV_ALIGN_CENTER, 0, -16);

    view->progress_label = lv_label_create(container);
    lv_label_set_text(view->progress_label, "");
    lv_obj_set_style_text_color(view->progress_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(view->progress_label, &lv_font_chinese_16, 0);
    lv_obj_set_style_text_opa(view->progress_label, LV_OPA_70, 0);
    lv_obj_set_width(view->progress_label, lv_pct(100));
    lv_obj_set_style_text_align(view->progress_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(view->progress_label, LV_ALIGN_CENTER, 0, 8);

    view->eta_label = lv_label_create(container);
    lv_label_set_text(view->eta_label, "");
    lv_obj_set_style_text_color(view->eta_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(view->eta_label, &lv_font_chinese_16, 0);
    lv_obj_set_style_text_opa(view->eta_label, LV_OPA_50, 0);
    lv_obj_set_width(view->eta_label, lv_pct(100));
    lv_obj_set_style_text_align(view->eta_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(view->eta_label, LV_ALIGN_BOTTOM_MID, 0, -12);
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
    switch (state->state) {
    case OTA_STATE_UPDATING: {
        // 第一行：N/M 正在更新XXX…
        if (state->update_total > 0) {
            snprintf(view->status_text, sizeof(view->status_text), "%u/%u 正在更新%s…",
                     state->update_index, state->update_total, ota_target_name(state->target));
        } else {
            snprintf(view->status_text, sizeof(view->status_text), "正在更新%s…",
                     ota_target_name(state->target));
        }
        lv_label_set_text_static(view->status_label, view->status_text);

        // 第二行：已下载 / 总大小
        if (state->bytes_total > 0) {
            char processed_str[16], total_str[16];
            ota_view_format_size(processed_str, sizeof(processed_str), state->bytes_processed);
            ota_view_format_size(total_str, sizeof(total_str), state->bytes_total);
            snprintf(view->progress_text, sizeof(view->progress_text), "%s / %s", processed_str, total_str);
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
        lv_label_set_text_static(view->status_label, "更新完毕");
        lv_label_set_text_static(view->progress_label, "");
        if (state->reboot == OTA_REBOOT_STRATEGY_AUTO) {
            lv_label_set_text_static(view->eta_label, "正在重启…");
        } else {
            lv_label_set_text_static(view->eta_label, "请手动重启设备");
        }
        break;
    case OTA_STATE_FAILED:
        lv_label_set_text_static(view->status_label, "更新失败");
        lv_label_set_text_static(view->progress_label, "");
        if (state->reboot == OTA_REBOOT_STRATEGY_AUTO) {
            lv_label_set_text_static(view->eta_label, "正在重启…");
        } else {
            lv_label_set_text_static(view->eta_label, "请手动重启设备");
        }
        break;
    default:
        break;
    }
}

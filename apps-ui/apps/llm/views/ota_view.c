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
    char progress_text[32];
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
    lv_label_set_text(view->status_label, "正在更新…");
    lv_obj_set_style_text_color(view->status_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(view->status_label, &lv_font_notosans_cs_medium_18, 0);
    lv_obj_align(view->status_label, LV_ALIGN_CENTER, 0, -30);

    view->progress_label = lv_label_create(container);
    lv_label_set_text(view->progress_label, "");
    lv_obj_set_style_text_color(view->progress_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(view->progress_label, &lv_font_notosans_cs_medium_14, 0);
    lv_obj_align(view->progress_label, LV_ALIGN_CENTER, 0, 30);
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

void lisa_ui_ota_view_update(lv_obj_t *obj, const ota_state_t *state)
{
    if (!obj || !lv_obj_has_class(obj, &lisa_ui_ota_view_class)) {
        return;
    }

    lisa_ui_ota_view_t *view = (lisa_ui_ota_view_t *)obj;
    switch (state->state) {
    case OTA_STATE_UPDATING:
        lv_label_set_text_static(view->status_label, "正在更新…");
        if (state->bytes_total == 0) {
            lv_label_set_text_static(view->progress_label, "");
        } else {
            snprintf(view->progress_text, sizeof(view->progress_text), "%.1f%%",
                     (float)state->bytes_processed / (float)state->bytes_total * 100.0f);
            lv_label_set_text_static(view->progress_label, view->progress_text);
        }
        break;
    case OTA_STATE_SUCCESSED:
        lv_label_set_text_static(view->status_label, "更新完毕");
        if (state->reboot == OTA_REBOOT_STRATEGY_AUTO) {
            lv_label_set_text_static(view->progress_label, "正在重启…");
        } else {
            lv_label_set_text_static(view->progress_label, "请手动重启设备");
        }
        break;
    case OTA_STATE_FAILED:
        lv_label_set_text_static(view->status_label, "更新失败");
        if (state->reboot == OTA_REBOOT_STRATEGY_AUTO) {
            lv_label_set_text_static(view->progress_label, "正在重启…");
        } else {
            lv_label_set_text_static(view->progress_label, "请手动重启设备");
        }
        break;
    }
}

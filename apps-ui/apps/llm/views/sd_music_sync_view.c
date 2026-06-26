#include "sd_music_sync_view.h"

#include <stdbool.h>
#include <stdio.h>

#include "lisa_ui_fonts.h"
#include "lisa_ui_llm_base.h"

typedef struct {
    lisa_ui_llm_base_t base;
    lv_obj_t *title_label;
    lv_obj_t *detail_label;
    lv_obj_t *progress_label;
    char detail_text[96];
    char progress_text[192];
} lisa_ui_sd_music_sync_view_t;

static void sd_music_sync_view_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj);
static void sd_music_sync_view_destructor(const lv_obj_class_t *class_p, lv_obj_t *obj);

const lv_obj_class_t lisa_ui_sd_music_sync_view_class = {
    .base_class = &lisa_ui_llm_base_class,
    .instance_size = sizeof(lisa_ui_sd_music_sync_view_t),
    .constructor_cb = sd_music_sync_view_constructor,
    .destructor_cb = sd_music_sync_view_destructor,
};

static lv_obj_t *create_center_label(lv_obj_t *parent, const char *text, lv_coord_t y)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_font(label, &lv_font_chinese_16, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(label, lv_pct(100));
    lv_obj_align(label, LV_ALIGN_CENTER, 0, y);
    return label;
}

static void align_sync_labels(lisa_ui_sd_music_sync_view_t *view,
                              lv_coord_t title_y,
                              lv_coord_t detail_y,
                              lv_coord_t progress_y)
{
    lv_obj_align(view->title_label, LV_ALIGN_CENTER, 0, title_y);
    lv_obj_align(view->detail_label, LV_ALIGN_CENTER, 0, detail_y);
    lv_obj_align(view->progress_label, LV_ALIGN_CENTER, 0, progress_y);
}

static void sd_music_sync_view_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    lisa_ui_sd_music_sync_view_t *view = (lisa_ui_sd_music_sync_view_t *)obj;
    lv_obj_t *bar;
    lv_obj_t *container;

    LV_UNUSED(class_p);

    bar = lisa_ui_llm_base_bar_get(obj);
    if (bar) {
        lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);
    }

    container = lisa_ui_llm_base_container_get(obj);
    if (!container) {
        return;
    }

    view->title_label = create_center_label(container, "检测到TF卡插入，扫描中..", -12);
    view->detail_label = create_center_label(container, "请勿拔卡或关机", 14);
    lv_obj_set_style_text_opa(view->detail_label, LV_OPA_70, 0);

    view->progress_label = create_center_label(container, "", 0);
    lv_obj_set_style_text_opa(view->progress_label, LV_OPA_70, 0);
}

static void sd_music_sync_view_destructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    LV_UNUSED(class_p);
    LV_UNUSED(obj);
}

lv_obj_t *lisa_ui_sd_music_sync_view_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(&lisa_ui_sd_music_sync_view_class, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

void lisa_ui_sd_music_sync_view_update(lv_obj_t *obj,
                                        const voice_msg_sd_music_sync_state_t *state)
{
    lisa_ui_sd_music_sync_view_t *view;

    if (!obj || !state || !lv_obj_has_class(obj, &lisa_ui_sd_music_sync_view_class)) {
        return;
    }

    view = (lisa_ui_sd_music_sync_view_t *)obj;

    switch ((voice_msg_sd_music_sync_state_e)state->state) {
    case VOICE_MSG_SD_MUSIC_SYNC_STATE_START:
        align_sync_labels(view, -12, 14, 0);
        lv_label_set_text_static(view->title_label, "检测到TF卡插入，扫描中..");
        lv_label_set_text_static(view->detail_label, "请勿拔卡或关机");
        lv_label_set_text_static(view->progress_label, "");
        break;
    case VOICE_MSG_SD_MUSIC_SYNC_STATE_UPLOADING:
    {
        unsigned int total_count = (unsigned int)state->total_count;

        if (total_count < (unsigned int)state->uploaded_count) {
            total_count = (unsigned int)state->uploaded_count;
        }

        align_sync_labels(view, -12, 20, 0);
        lv_label_set_text_static(view->title_label, "正在处理TF卡音频文件列表");
        if (total_count > 0) {
            snprintf(view->detail_text, sizeof(view->detail_text),
                     "已成功处理 %u / %u 个",
                     (unsigned int)state->uploaded_count,
                     total_count);
        } else {
            snprintf(view->detail_text, sizeof(view->detail_text),
                     "已成功处理 %u 个",
                     (unsigned int)state->uploaded_count);
        }
        lv_label_set_text_static(view->detail_label, view->detail_text);
        lv_label_set_text_static(view->progress_label, "");
        break;
    }
    case VOICE_MSG_SD_MUSIC_SYNC_STATE_SUCCESSED:
        if (state->result == VOICE_MSG_SD_MUSIC_SYNC_RESULT_DATA_UNCHANGED) {
            align_sync_labels(view, 0, 0, 0);
            lv_label_set_text_static(view->title_label, "检测到TF卡插入，但数据无变化");
            lv_label_set_text_static(view->detail_label, "");
            lv_label_set_text_static(view->progress_label, "");
        } else if (state->result == VOICE_MSG_SD_MUSIC_SYNC_RESULT_NO_MP3) {
            align_sync_labels(view, 0, 0, 0);
            lv_label_set_text_static(view->title_label, "TF卡内未检测到MP3音频文件，请添加后重试");
            lv_label_set_text_static(view->detail_label, "");
            lv_label_set_text_static(view->progress_label, "");
        } else if (state->skipped_count > 0) {
            unsigned int total_count = (unsigned int)state->total_count;

            if (total_count < (unsigned int)state->uploaded_count) {
                total_count = (unsigned int)state->uploaded_count;
            }

            align_sync_labels(view, -62, -36, 28);
            lv_label_set_text_static(view->title_label, "处理完成");
            snprintf(view->detail_text, sizeof(view->detail_text),
                     "成功处理 %u 个 / 共 %u 个",
                     (unsigned int)state->uploaded_count,
                     total_count + (unsigned int)state->skipped_count);
            lv_label_set_text_static(view->detail_label, view->detail_text);
            lv_obj_set_style_text_align(view->progress_label, LV_TEXT_ALIGN_LEFT, 0);
            snprintf(view->progress_text, sizeof(view->progress_text),
                     "未处理 %u 个，原因可能有：\n"
                     "　1. 文件名是否超过 64 字\n"
                     "　2. 文件夹名是否超过 32 字\n"
                     "　3. 文件夹层数是否超过 3 层\n"
                     "　4. 文件是否都在audio文件夹内",
                     (unsigned int)state->skipped_count);
            lv_label_set_text(view->progress_label, view->progress_text);
        } else {
            align_sync_labels(view, -12, 14, 0);
            lv_label_set_text_static(view->title_label, "处理完成");
            snprintf(view->detail_text, sizeof(view->detail_text),
                     "成功处理 %u 个音频文件",
                     (unsigned int)state->uploaded_count);
            lv_label_set_text_static(view->detail_label, view->detail_text);
            lv_label_set_text_static(view->progress_label, "");
        }
        break;
    case VOICE_MSG_SD_MUSIC_SYNC_STATE_FAILED:
    {
        bool has_http_error = state->http_status_code || state->http_error_code;

        align_sync_labels(view, has_http_error ? -12 : 0,
                          has_http_error ? 14 : 0, 0);
        lv_label_set_text_static(view->title_label, "TF卡文件列表上报失败");
        if (state->http_status_code) {
            snprintf(view->progress_text, sizeof(view->progress_text),
                     "HTTP响应码：%u",
                     (unsigned int)state->http_status_code);
            lv_label_set_text_static(view->detail_label, view->progress_text);
        } else if (state->http_error_code) {
            snprintf(view->progress_text, sizeof(view->progress_text),
                     "HTTP错误码：%d",
                     (int)state->http_error_code);
            lv_label_set_text_static(view->detail_label, view->progress_text);
        } else {
            lv_label_set_text_static(view->detail_label, "");
        }
        lv_label_set_text_static(view->progress_label, "");
        break;
    }
    default:
        align_sync_labels(view, 0, 0, 0);
        lv_label_set_text_static(view->title_label, "");
        lv_label_set_text_static(view->detail_label, "");
        lv_label_set_text_static(view->progress_label, "");
        break;
    }
}

#include <stdio.h>
#include <string.h>

#include "home_camera_preview_presenter.h"
#include "home_presenter.h"

#include "lisa_ui_nav_scr.h"
#include "lisa_ui_nav_scr_ids.h"
#include "lisa_ui_toast.h"
#include "lisa_ui_llm_primary.h"
#include "model_camera.h"
#include "model_voice.h"
#include "voice_msg.h"
#ifdef LISA_UI_PLATFORM_ARCS
#include "app_player.h"
#include "app_tone.h"
#include "tone.h"
#include "voice_player_comm.h"
#endif

#define CAMERA_PREVIEW_COUNTDOWN_INTERVAL_MS 1000U
#define CAMERA_PREVIEW_UPLOAD_TEXT_OFFSET_Y 6

static bool camera_preview_is_button_source(const model_camera_preview_t *preview)
{
    return model_camera_preview_source_get(preview) == MODEL_CAMERA_PREVIEW_SOURCE_BUTTON;
}

static bool camera_preview_is_voice_source(const model_camera_preview_t *preview)
{
    return model_camera_preview_source_get(preview) == MODEL_CAMERA_PREVIEW_SOURCE_MCP;
}

static void camera_preview_update_hint(struct home_nav_scr_data *scr_data)
{
    if (!scr_data || !scr_data->view) {
        return;
    }

    if (model_camera_preview_keep_preview_alive(&scr_data->camera_preview) &&
        camera_preview_is_button_source(&scr_data->camera_preview)) {
        lisa_ui_llm_primary_finger_hint_show(scr_data->view, "单击：拍照识图\n长按：退出");
    } else {
        lisa_ui_llm_primary_finger_hint_hide(scr_data->view);
    }
}

static void camera_preview_update_text_offset(struct home_nav_scr_data *scr_data)
{
    lv_obj_t *content_label = NULL;
    lv_coord_t offset_y = 0;

    if (!scr_data || !scr_data->view) {
        return;
    }

    content_label = lisa_ui_llm_primary_content_label_get(scr_data->view);
    if (!content_label) {
        return;
    }

    if (model_camera_preview_is_captured(&scr_data->camera_preview) ||
        (model_camera_preview_is_preview_active(&scr_data->camera_preview) &&
         camera_preview_is_voice_source(&scr_data->camera_preview))) {
        offset_y = CAMERA_PREVIEW_UPLOAD_TEXT_OFFSET_Y;
    }

    lv_obj_set_style_translate_y(content_label, offset_y, LV_PART_MAIN);
}

static void camera_preview_update_text(struct home_nav_scr_data *scr_data)
{
    char content[64] = {0};

    if (!scr_data || !scr_data->view) {
        return;
    }

    camera_preview_update_hint(scr_data);
    camera_preview_update_text_offset(scr_data);

    if (camera_preview_is_button_source(&scr_data->camera_preview)) {
        if (model_camera_preview_is_captured(&scr_data->camera_preview)) {
            lisa_ui_llm_primary_set_status_text(scr_data->view, "拍照完成");
            lisa_ui_llm_primary_set_content_text(scr_data->view, "\n正在上传照片...");
        } else {
            lisa_ui_llm_primary_set_status_text(scr_data->view, "拍照预览");
            lisa_ui_llm_primary_set_content_text(scr_data->view, "");
        }
        return;
    }

    if (model_camera_preview_is_captured(&scr_data->camera_preview)) {
        lisa_ui_llm_primary_set_status_text(scr_data->view, "拍照完成");
        lisa_ui_llm_primary_set_content_text(scr_data->view, "\n正在上传照片...");
        return;
    }

    lisa_ui_llm_primary_set_status_text(scr_data->view, "拍照预览");
    if (model_camera_preview_countdown_remaining_get(&scr_data->camera_preview) > 0) {
        snprintf(content, sizeof(content), "\n%u秒后自动拍照",
                 (unsigned int)model_camera_preview_countdown_remaining_get(
                     &scr_data->camera_preview));
    } else {
        strncpy(content, "\n正在拍照...", sizeof(content) - 1);
    }
    lisa_ui_llm_primary_set_content_text(scr_data->view, content);
}

bool camera_preview_is_camera_work_type(uint8_t work_type)
{
    return work_type == WORK_TYPE_IMG_REC || work_type == WORK_TYPE_CAMERA;
}

static void camera_preview_stop_countdown_timer(struct home_nav_scr_data *scr_data)
{
    if (!scr_data) {
        return;
    }

    if (scr_data->camera_preview_countdown_timer) {
        lv_timer_del(scr_data->camera_preview_countdown_timer);
        scr_data->camera_preview_countdown_timer = NULL;
    }
}

static bool camera_preview_timer_should_stop(const struct home_nav_scr_data *scr_data)
{
    return !scr_data ||
           (!scr_data->img_rec_running &&
            !model_camera_preview_keep_preview_alive(&scr_data->camera_preview));
}

static void camera_preview_capture_timer_abort(struct home_nav_scr_data *scr_data,
                                               lv_timer_t *timer)
{
    scr_data->img_rec_running = false;
    scr_data->img_rec_triggered = false;
    model_camera_preview_reset(&scr_data->camera_preview);
    camera_preview_stop_countdown_timer(scr_data);
    model_camera_preview_publish_state(&scr_data->camera_preview);
    /* 仅发布 STATE(phase=NONE) 时，UI 此前在 keep_preview_alive 分支已忽略了
     * session_finished / tts_stoped 的收尾，会卡在交互态。补发 EXIT 让 home_reset
     * 接管，否则首页停在"我在听"。 */
    voice_msg_pub(VOICE_MSG_APP_CAMERA_PREVIEW_EXIT, NULL, 0);
    model_camera_stop();
    lv_timer_pause(timer);
}

void camera_preview_capture_timer_cb(lv_timer_t *timer)
{
    if (!timer || !timer->user_data) {
        return;
    }

    struct home_nav_scr_data *scr_data = timer->user_data;
    uint16_t width = 0;
    uint16_t height = 0;
    uint32_t image_size = 0;
    int ret = 0;

    if (!scr_data->view) {
        lv_timer_pause(timer);
        return;
    }

    if (camera_preview_timer_should_stop(scr_data)) {
        lv_timer_pause(timer);
        return;
    }

    scr_data->img.header.cf = LV_IMG_CF_TRUE_COLOR;
    scr_data->img.header.always_zero = 0;
    scr_data->img.header.reserved = 0;

    ret = model_camera_get_framesize(&width, &height);
    if (ret != 0 || width == 0 || height == 0) {
        LISA_UI_LOGE("Get camera frame size failed: ret=%d, w=%u, h=%u", ret, width, height);
        camera_preview_capture_timer_abort(scr_data, timer);
        return;
    }

    image_size = (uint32_t)width * (uint32_t)height * 2U;
    if (image_size == 0) {
        LISA_UI_LOGE("Invalid image size, w=%u, h=%u", width, height);
        camera_preview_capture_timer_abort(scr_data, timer);
        return;
    }

    if (scr_data->cap_buf == NULL || scr_data->cap_buf_size < image_size) {
        if (scr_data->cap_buf != NULL) {
            lisa_ui_free(scr_data->cap_buf);
            scr_data->cap_buf = NULL;
            scr_data->cap_buf_size = 0;
        }

        scr_data->cap_buf = lisa_ui_malloc(image_size);
        if (!scr_data->cap_buf) {
            LISA_UI_LOGE("Failed to alloc capture buffer: %u", image_size);
            camera_preview_capture_timer_abort(scr_data, timer);
            return;
        }
        scr_data->cap_buf_size = image_size;
    }

    if (camera_preview_timer_should_stop(scr_data)) {
        lv_timer_pause(timer);
        return;
    }

    ret = model_camera_capture(scr_data->cap_buf, image_size);
    if (ret != 0) {
        LISA_UI_LOGE("Camera capture failed: %d", ret);
        camera_preview_capture_timer_abort(scr_data, timer);
        return;
    }

    /* capture 输出已是显示方向（service_camera 采集时同遍历旋转 90° CW） */
    scr_data->img.data = scr_data->cap_buf;
    scr_data->img.data_size = image_size;
    scr_data->img.header.w = width;
    scr_data->img.header.h = height;

    if (camera_preview_timer_should_stop(scr_data)) {
        lv_timer_pause(timer);
        return;
    }

    lisa_ui_llm_primary_img_show(scr_data->view, &scr_data->img);
}

void camera_preview_hide(struct home_nav_scr_data *scr_data)
{
    if (!scr_data || !scr_data->view) {
        return;
    }

    if (scr_data->camera_capture_timer) {
        lv_timer_pause(scr_data->camera_capture_timer);
    }

    camera_preview_stop_countdown_timer(scr_data);
    model_camera_preview_reset(&scr_data->camera_preview);

    lisa_ui_llm_primary_img_hide(scr_data->view);
    lisa_ui_llm_primary_finger_hint_hide(scr_data->view);
    camera_preview_update_text_offset(scr_data);
    if (camera_preview_is_camera_work_type(scr_data->work_type)) {
        scr_data->work_type = WORK_TYPE_VOICE;
    }
    scr_data->img_rec_running = false;
    scr_data->img_rec_in_progress = false;
    scr_data->img_rec_triggered = false;
    scr_data->img_rec_is_mcp = 0;
    scr_data->img_rec_is_button = 0;
    model_camera_preview_publish_state(&scr_data->camera_preview);
    model_camera_stop();
}

static void camera_preview_capture_now(struct home_nav_scr_data *scr_data)
{
    model_camera_preview_submit_kind_t submit_kind = MODEL_CAMERA_PREVIEW_SUBMIT_NONE;
    uint16_t width = 0;
    uint16_t height = 0;
    uint16_t image_width = 0;
    uint16_t image_height = 0;
    uint32_t image_size = 0;
    int frame_ret = 0;
    int upload_ret = 0;

    if (!scr_data || !scr_data->view ||
        !model_camera_preview_is_preview_active(&scr_data->camera_preview)) {
        return;
    }

    if (model_camera_preview_is_captured(&scr_data->camera_preview)) {
        LISA_UI_LOGI("photo already captured, ignore duplicate capture");
        return;
    }

    if (scr_data->camera_capture_timer) {
        lv_timer_pause(scr_data->camera_capture_timer);
    }

    camera_preview_stop_countdown_timer(scr_data);
    if (model_camera_preview_request_capture(&scr_data->camera_preview) ==
        MODEL_CAMERA_PREVIEW_CAPTURE_NONE) {
        return;
    }

#ifdef LISA_UI_PLATFORM_ARCS
    {
        /* 使用 prompt tone 机制播放快门音：
         * voice_player_play_prompt_tone_url 会在播放前 push INTENT_PROMPT_TONE，
         * 主动触发 MUSIC 的 on_preempted → app_player_pause，避免 tone 在音频
         * 焦点层直接抢占导致 focus 状态不一致（FOREGROUND→BACKGROUND→FOREGROUND
         * →BACKGROUND 快速切换），进而引发 PA ON/OFF 风暴影响唤醒算法。
         *
         * TONE_ID_73 资源缺失时 voice_player_play_prompt_tone_url 内部校验
         * url 为 NULL 会直接跳过，不会触发任何 tone 事件。 */
        const char *tone_url = app_tone_get_url(TONE_ID_73);
        if (tone_url) {
            voice_player_play_prompt_tone_url(tone_url);
        } else {
            LISA_UI_LOGW("Camera capture tone (id=%d) url is null, skip",
                         TONE_ID_73);
        }
    }
#endif

    if (!model_camera_preview_is_captured(&scr_data->camera_preview)) {
        LISA_UI_LOGI("photo capture canceled before submit");
        return;
    }

    scr_data->work_type = WORK_TYPE_CAMERA;
    camera_preview_update_text(scr_data);
    model_camera_preview_publish_state(&scr_data->camera_preview);
    LISA_UI_LOGI("photo capture start, source=%s",
                 model_camera_preview_source_name(
                     model_camera_preview_source_get(&scr_data->camera_preview)));

    frame_ret = model_camera_get_framesize(&width, &height);
    if (frame_ret != 0 || width == 0 || height == 0) {
        LISA_UI_LOGE("Get frame size failed before photo capture: ret=%d, w=%u, h=%u",
                     frame_ret, width, height);
        lisa_ui_toast_show(_("recognition error"));
        camera_preview_hide(scr_data);
        return;
    }
    
    /* capture 输出即显示方向（采集时已旋转），framesize 就是实际尺寸 */
    image_width = width;
    image_height = height;
    image_size = (uint32_t)image_width * (uint32_t)image_height * 2U;
    LISA_UI_LOGI("photo capture frame ready, source=%s, raw=%ux%u, rotated=%ux%u, size=%u",
                 model_camera_preview_source_name(
                     model_camera_preview_source_get(&scr_data->camera_preview)),
                 width, height, image_width, image_height, image_size);
    if (image_size == 0 || scr_data->cap_buf == NULL || scr_data->cap_buf_size < image_size) {
        LISA_UI_LOGE("Invalid photo buffer size: cap=%u, need=%u", scr_data->cap_buf_size,
                     image_size);
        lisa_ui_toast_show(_("recognition error"));
        camera_preview_hide(scr_data);
        return;
    }

    submit_kind = model_camera_preview_prepare_submit(&scr_data->camera_preview);
    if (submit_kind == MODEL_CAMERA_PREVIEW_SUBMIT_NONE) {
        camera_preview_hide(scr_data);
        return;
    }

    /* 当前帧已经复制到 cap_buf，后续 JPEG 编码和上传不再需要 DVP 持续采集。
     * 及时停流可归还双帧缓冲，避免上传等待期间持续丢预览帧。 */
    int camera_stop_ret = model_camera_stop();
    if (camera_stop_ret != 0) {
        LISA_UI_LOGW("Stop camera after photo capture failed: %d", camera_stop_ret);
    }

    camera_preview_update_text(scr_data);
    model_camera_preview_publish_state(&scr_data->camera_preview);

    if (submit_kind == MODEL_CAMERA_PREVIEW_SUBMIT_BUTTON) {
        scr_data->work_type = WORK_TYPE_IMG_REC;
        scr_data->img_rec_is_mcp = 0;
        scr_data->img_rec_is_button = 1;
        scr_data->img_rec_running = 0;
        scr_data->img_rec_triggered = 0;
        lisa_ui_llm_primary_finger_hint_hide(scr_data->view);
        standby_text_timer_update(scr_data);

        if (!model_voice_cloud_is_connected()) {
            lisa_ui_toast_show(_("Please wake me"));
            scr_data->work_type = WORK_TYPE_CAMERA;
            model_camera_preview_restore_after_submit_failure(&scr_data->camera_preview,
                                                             submit_kind);
            scr_data->img_rec_is_button = 0;
            camera_preview_update_text(scr_data);
            model_camera_preview_publish_state(&scr_data->camera_preview);
            if (scr_data->camera_capture_timer) {
                lv_timer_resume(scr_data->camera_capture_timer);
            }
            return;
        }

        LISA_UI_LOGI("button photo submit recognition, size=%u", image_size);
        upload_ret = model_voice_img_recognition(scr_data->cap_buf, image_size, image_width,
                                                 image_height);
        if (upload_ret == 0) {
            scr_data->img_rec_in_progress = 1;
            lisa_ui_llm_primary_set_status_text(scr_data->view, _("recognizing"));
            LISA_UI_LOGI("Button photo recognition started");
            return;
        }

        LISA_UI_LOGE("Button photo recognition submit failed: %d", upload_ret);
        lisa_ui_toast_show(_("recognition error"));
        scr_data->work_type = WORK_TYPE_CAMERA;
        model_camera_preview_restore_after_submit_failure(&scr_data->camera_preview, submit_kind);
        scr_data->img_rec_in_progress = 0;
        scr_data->img_rec_is_button = 0;
        camera_preview_update_text(scr_data);
        model_camera_preview_publish_state(&scr_data->camera_preview);
        if (scr_data->camera_capture_timer) {
            lv_timer_resume(scr_data->camera_capture_timer);
        }
        return;
    }

    scr_data->img_rec_in_progress = 1;

    LISA_UI_LOGI("voice photo submit recognition, size=%u", image_size);
    upload_ret = model_voice_img_recognition(scr_data->cap_buf, image_size, image_width,
                                             image_height);
    if (upload_ret != 0) {
        LISA_UI_LOGE("Photo upload submit failed: %d", upload_ret);
        lisa_ui_toast_show(_("recognition error"));
        scr_data->img_rec_in_progress = 0;
        camera_preview_hide(scr_data);
        return;
    }

    LISA_UI_LOGI("voice photo recognition submitted, waiting for cloud response");
}

static void camera_preview_countdown_timer_cb(lv_timer_t *timer)
{
    struct home_nav_scr_data *scr_data = timer ? timer->user_data : NULL;
    model_camera_preview_countdown_result_t countdown_result =
        MODEL_CAMERA_PREVIEW_COUNTDOWN_STOP;

    if (!scr_data) {
        camera_preview_stop_countdown_timer(scr_data);
        return;
    }

    countdown_result = model_camera_preview_countdown_tick(&scr_data->camera_preview);
    if (countdown_result == MODEL_CAMERA_PREVIEW_COUNTDOWN_STOP) {
        camera_preview_stop_countdown_timer(scr_data);
        return;
    }

    if (countdown_result == MODEL_CAMERA_PREVIEW_COUNTDOWN_UPDATE) {
        LISA_UI_LOGI("voice photo countdown tick: %us",
                     model_camera_preview_countdown_remaining_get(&scr_data->camera_preview));
        camera_preview_update_text(scr_data);
        return;
    }

    camera_preview_stop_countdown_timer(scr_data);
    LISA_UI_LOGI("voice photo countdown finished, capture now");
    camera_preview_capture_now(scr_data);
}

static void camera_preview_handle_start(void *arg, const model_camera_preview_req_t *req)
{
    struct home_nav_scr_data *scr_data = arg;
    uint16_t width = 0;
    uint16_t height = 0;
    int ret = 0;

    if (!scr_data || !scr_data->view || !req) {
        return;
    }

    if (lisa_ui_nav_scr_get_top_id() != LISA_UI_NAV_SCR_ID_HOME) {
        lisa_ui_nav_scr_nav_to(LISA_UI_NAV_SCR_ID_HOME);
    }

    home_handle_activity(scr_data);

    if (scr_data->img_rec_in_progress) {
        LISA_UI_LOGI("image recognition in progress, ignore photo preview");
        return;
    }

    if (scr_data->img_rec_running || model_camera_preview_is_active(&scr_data->camera_preview) ||
        camera_preview_is_camera_work_type(scr_data->work_type)) {
        camera_preview_hide(scr_data);
    }

    if (!model_camera_is_inited()) {
        ret = model_camera_init();
        if (ret != 0) {
            LISA_UI_LOGE("Camera init failed for photo preview: %d", ret);
            lisa_ui_toast_show(_("recognition error"));
            return;
        }
    }

    ret = model_camera_get_framesize(&width, &height);
    if (ret != 0 || width == 0 || height == 0) {
        LISA_UI_LOGE("Camera unavailable for photo preview: ret=%d, w=%u, h=%u", ret, width,
                     height);
        lisa_ui_toast_show(_("recognition error"));
        return;
    }

    lisa_ui_llm_primary_img_hint_hide(scr_data->view);
    scr_data->finished = 0;
    scr_data->work_type = WORK_TYPE_CAMERA;
    model_camera_preview_start_preview(&scr_data->camera_preview, req->source,
                                       req->auto_capture_delay_ms,
                                       CAMERA_PREVIEW_COUNTDOWN_INTERVAL_MS);

    camera_preview_stop_countdown_timer(scr_data);
    camera_preview_update_text(scr_data);
    standby_text_timer_update(scr_data);
    LISA_UI_LOGI("photo preview started, source=%s, delay_ms=%u, countdown_s=%u",
                 model_camera_preview_source_name(
                     model_camera_preview_source_get(&scr_data->camera_preview)),
                 req->auto_capture_delay_ms,
                 model_camera_preview_countdown_remaining_get(&scr_data->camera_preview));

    if (scr_data->camera_capture_timer == NULL) {
        scr_data->camera_capture_timer = lv_timer_create(camera_preview_capture_timer_cb, 60, scr_data);
    } else {
        lv_timer_resume(scr_data->camera_capture_timer);
    }

    if (model_camera_preview_should_start_countdown(&scr_data->camera_preview)) {
        scr_data->camera_preview_countdown_timer =
            lv_timer_create(camera_preview_countdown_timer_cb, CAMERA_PREVIEW_COUNTDOWN_INTERVAL_MS,
                            scr_data);
    }

    model_camera_preview_publish_state(&scr_data->camera_preview);
}

static void camera_preview_handle_capture(void *arg)
{
    struct home_nav_scr_data *scr_data = arg;

    if (!scr_data || !model_camera_preview_is_preview_active(&scr_data->camera_preview)) {
        return;
    }

    camera_preview_capture_now(scr_data);
}

static void camera_preview_handle_exit(void *arg)
{
    struct home_nav_scr_data *scr_data = arg;

    if (!scr_data ||
        (!model_camera_preview_is_active(&scr_data->camera_preview) &&
         !camera_preview_is_camera_work_type(scr_data->work_type))) {
        return;
    }

    if (model_camera_preview_keep_preview_alive(&scr_data->camera_preview)) {
        LISA_UI_LOGI("photo preview exit, hide preview immediately");
        home_reset(scr_data);
        return;
    }

    /* 拍照完成后照片应保持可见，等 TTS 播完再隐藏。
     * 用 work_type 兜底：即使 model 状态已被局部重置，只要处于拍照结果流中就保留照片。 */
    if (model_camera_preview_is_result_active(&scr_data->camera_preview) ||
        camera_preview_is_camera_work_type(scr_data->work_type)) {
        LISA_UI_LOGI("voice photo: defer hide until result TTS finishes,"
                     " result_active=%d work_type=%u",
                     model_camera_preview_is_result_active(&scr_data->camera_preview),
                     scr_data->work_type);
        scr_data->camera_preview_result_pending = 1;
        return;
    }

    home_reset(scr_data);
}

static void camera_preview_handle_result_tts_ready(void *arg)
{
    struct home_nav_scr_data *scr_data = arg;

    if (!scr_data) {
        return;
    }

    if (!model_camera_preview_handle_result_tts_ready(&scr_data->camera_preview)) {
        return;
    }

    scr_data->img_rec_in_progress = 0;
    LISA_UI_LOGI("voice photo: result TTS is ready");
    model_camera_preview_publish_state(&scr_data->camera_preview);
}

const struct model_camera_preview_cb home_camera_preview_presenter_cbs = {
    .on_start = camera_preview_handle_start,
    .on_capture = camera_preview_handle_capture,
    .on_exit = camera_preview_handle_exit,
    .on_result_tts_ready = camera_preview_handle_result_tts_ready,
};

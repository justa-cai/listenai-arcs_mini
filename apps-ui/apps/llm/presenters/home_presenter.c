#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lisa_ui.h"
#include "lisa_ui_nav_scr.h"
#include "lisa_ui_nav_scr_ids.h"
#include "lisa_ui_anim_ext.h"
#include "lisa_ui_assets.h"
#include "lisa_ui_toast.h"

#include "model_voice.h"
#include "model_alarm.h"
#include "lisa_ui_llm_primary.h"
#include "emoji_anim.h"
#include "model_camera.h"
#include "model_wifi.h"
#include "model_battery.h"
#include "voice_cloud.h"
#include "display/lv_img_net_loader.h"
#ifdef CONFIG_BATTERY_COLLECTION
#include "battery/battery.h"
#endif

#define WORK_TYPE_VOICE 0
#define WORK_TYPE_IMG_REC 1

struct home_nav_scr_data {
    lv_obj_t *view;
    lv_timer_t *anim_timer;
    lv_timer_t *wifi_status_timer;
    lv_timer_t *camera_capture_timer;
    lv_timer_t *img_hide_timer;
    lv_timer_t *standby_text_timer;
    uint32_t standby_text_index;
    lv_img_dsc_t img;
    lv_img_dsc_t *net_img;
    uint8_t *cap_buf;
    uint32_t cap_buf_size;
    uint8_t img_rec_running;
    uint8_t img_rec_in_progress;
    uint8_t img_rec_triggered;
    uint8_t img_rec_is_mcp;
    uint8_t img_rec_is_button;
    uint8_t mcp_emoji_running;
    uint8_t mcp_loading;
    uint8_t finished;
    uint8_t speaking;
    uint8_t work_type;
};

static void img_hide_timer_cb(lv_timer_t *timer);
static void hide_img_now(struct home_nav_scr_data *scr_data);

static void show_emoji_anim(struct home_nav_scr_data *d, const char *emoji_name, uint8_t imm);
static void model_voice_on_emoji(void *arg, const char *name);
static void model_voice_on_mcp_emoji(void *arg, const char *name);
static void model_voice_on_mcp_loading(void *arg, bool is_loading, const char *loading_text);
static void model_voice_on_start(void *arg);
static void model_voice_on_finished(void *arg);
static void model_voice_on_tts_text_start(void *arg);
static void model_voice_on_tts_text_end(void *arg);
static void model_voice_on_tts_text_update(const char *text, void *arg);
static void model_voice_on_iat_text_start(void *arg);
static void model_voice_on_iat_text_end(void *arg);
static void model_voice_on_iat_text_update(const char *text, void *arg);
static void model_voice_on_connected(void *arg);
static void model_voice_on_disconnected(void *arg);
static void model_voice_on_tts_player_playing(void *arg);
static void model_voice_on_tts_player_stoped(void *arg);
static void model_voice_on_image_rec(void *arg);
static void model_voice_on_info_show(void *arg);
static void model_voice_on_image_preview(void *arg);
static void model_voice_on_image_url(void *arg, const char *url);
static void model_voice_on_standby_text_update(void *arg, const char *text, bool is_cloud_text);
static void model_voice_on_show_qrcode(void *arg);
static void model_voice_on_standby_texts_changed(void *arg);
static void model_voice_on_wakeup_mode_changed(void *arg, model_voice_wakeup_mode_t mode);
static void standby_text_timer_cb(lv_timer_t *timer);
static void standby_text_timer_update(struct home_nav_scr_data *scr_data);
#ifdef CONFIG_OTA
static void model_voice_on_ota_state_change(const ota_state_t *state, void *arg);
#endif
static void home_update_full_duplex_icon(struct home_nav_scr_data *scr_data);
static void home_update_alarm_icon(struct home_nav_scr_data *scr_data);
static void home_update_battery_icon(struct home_nav_scr_data *scr_data);
#ifdef CONFIG_BATTERY_COLLECTION
static void home_update_battery_icon_from_hw(struct home_nav_scr_data *scr_data);
#endif

static int rotate_rgb565_cw90_inplace(uint8_t *rgb565_buf, uint16_t width, uint16_t height)
{
    if (!rgb565_buf || width == 0 || height == 0) {
        return -1;
    }

    uint32_t pixel_count = (uint32_t)width * (uint32_t)height;
    uint32_t buffer_size = pixel_count * sizeof(uint16_t);
    uint16_t *src = (uint16_t *)rgb565_buf;
    uint16_t *rotated = lisa_ui_malloc(buffer_size);

    if (!rotated) {
        return -2;
    }

    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint32_t dst_x = (uint32_t)height - 1 - y;
            uint32_t dst_y = x;
            rotated[dst_y * height + dst_x] = src[y * width + x];
        }
    }

    memcpy(rgb565_buf, rotated, buffer_size);
    lisa_ui_free(rotated);

    return 0;
}

static const void *battery_power_icons[] = {
    &icons_ic_status_power0_png,
    &icons_ic_status_power1_png,
    &icons_ic_status_power2_png,
    &icons_ic_status_power3_png,
    &icons_ic_status_power4_png,
    &icons_ic_status_power5_png,
    &icons_ic_status_power6_png,
    &icons_ic_status_power7_png,
    &icons_ic_status_power8_png,
    &icons_ic_status_power9_png,
};

static const void *battery_charging_icons[] = {
    &icons_ic_status_charging0_png,
    &icons_ic_status_charging1_png,
    &icons_ic_status_charging2_png,
    &icons_ic_status_charging3_png,
    &icons_ic_status_charging4_png,
    &icons_ic_status_charging5_png,
    &icons_ic_status_charging6_png,
    &icons_ic_status_charging7_png,
    &icons_ic_status_charging8_png,
    &icons_ic_status_charging9_png,
};

const struct model_voice_cb model_voice_cbs = {
    .on_tts_stoped = model_voice_on_tts_player_stoped,
    .on_tts_playing = model_voice_on_tts_player_playing,
    .on_emoji = model_voice_on_emoji,
    .on_mcp_emoji = model_voice_on_mcp_emoji,
    .on_mcp_loading = model_voice_on_mcp_loading,
    .on_connected = model_voice_on_connected,
    .on_disconnected = model_voice_on_disconnected,
    .on_start = model_voice_on_start,
    .on_finished = model_voice_on_finished,
    .on_tts_text_start = model_voice_on_tts_text_start,
    .on_tts_text_update = model_voice_on_tts_text_update,
    .on_tts_text_end = model_voice_on_tts_text_end,
    .on_iat_text_start = model_voice_on_iat_text_start,
    .on_iat_text_update = model_voice_on_iat_text_update,
    .on_iat_text_end = model_voice_on_iat_text_end,
    .on_image_rec = model_voice_on_image_rec,
    .on_info_show = model_voice_on_info_show,
    .on_image_preview = model_voice_on_image_preview,
    .on_image_url = model_voice_on_image_url,
    .on_standby_texts_changed = model_voice_on_standby_texts_changed,
    .on_standby_text_update = model_voice_on_standby_text_update,
    .on_show_qrcode = model_voice_on_show_qrcode,
    .on_wakeup_mode_changed = model_voice_on_wakeup_mode_changed,
    #ifdef CONFIG_OTA
    .on_ota_state_change = model_voice_on_ota_state_change,
    #endif
};

static void home_update_full_duplex_icon(struct home_nav_scr_data *scr_data)
{
    if (!scr_data || !scr_data->view) {
        return;
    }

    model_voice_wakeup_mode_t mode = model_voice_wakeup_mode_get();
    bool is_full_duplex = (mode == MODEL_VOICE_WAKEUP_MODE_VOICE_MULTI);
    lisa_ui_llm_primary_set_full_duplex_icon_visible(scr_data->view, is_full_duplex);
}

static void home_update_alarm_icon(struct home_nav_scr_data *scr_data)
{
    if (!scr_data || !scr_data->view) {
        return;
    }

    uint32_t count = model_alarm_count_get();
    lisa_ui_llm_primary_set_alarm_icon_visible(scr_data->view, count > 0);
}

static const void *home_select_battery_icon(const model_battery_info_t *info)
{
    if (!info) {
        return NULL;
    }

    if (info->status == MODEL_BATTERY_STATUS_UNKNOWN ||
        info->status == MODEL_BATTERY_STATUS_NO_BATTERY) {
        return NULL;
    }

    uint8_t level = info->level;
    uint8_t idx = level / 10;
    if (idx > 9) {
        idx = 9;
    }

    if (info->status == MODEL_BATTERY_STATUS_CHARGING ||
        info->status == MODEL_BATTERY_STATUS_CHARGE_DONE) {
        return battery_charging_icons[idx];
    }

    return battery_power_icons[idx];
}

#ifdef CONFIG_BATTERY_COLLECTION
static model_battery_status_t home_convert_hw_battery_status(battery_status_t status)
{
    switch (status) {
    case BATTERY_STATUS_NO_BATTERY:
        return MODEL_BATTERY_STATUS_NO_BATTERY;
    case BATTERY_STATUS_NOT_CONNECT:
        return MODEL_BATTERY_STATUS_NOT_CONNECT;
    case BATTERY_STATUS_CHARGING:
        return MODEL_BATTERY_STATUS_CHARGING;
    case BATTERY_STATUS_CHARGE_DONE:
        return MODEL_BATTERY_STATUS_CHARGE_DONE;
    case BATTERY_STATUS_UNKNOWN:
    default:
        return MODEL_BATTERY_STATUS_UNKNOWN;
    }
}

static void home_update_battery_icon_from_hw(struct home_nav_scr_data *scr_data)
{
    if (!scr_data || !scr_data->view) {
        return;
    }

    model_battery_info_t info = {
        .level = battery_get_pct(),
        .status = home_convert_hw_battery_status(battery_get_status()),
    };

    const void *icon = home_select_battery_icon(&info);
    lisa_ui_llm_primary_set_battery_img(scr_data->view, icon);
}
#endif

static void home_update_battery_icon(struct home_nav_scr_data *scr_data)
{
    if (!scr_data || !scr_data->view) {
        return;
    }

    model_battery_info_t info;
    if (model_battery_get_info(&info) != 0) {
        return;
    }

#ifdef CONFIG_BATTERY_COLLECTION
    if (info.status == MODEL_BATTERY_STATUS_UNKNOWN) {
        home_update_battery_icon_from_hw(scr_data);
        return;
    }
#endif

    const void *icon = home_select_battery_icon(&info);
    lisa_ui_llm_primary_set_battery_img(scr_data->view, icon);
}

static void model_battery_on_battery_status_update(const model_battery_info_t *info, void *arg)
{
    struct home_nav_scr_data *scr_data = arg;
    if (!scr_data || !scr_data->view) {
        return;
    }

    const void *icon = home_select_battery_icon(info);
    lisa_ui_llm_primary_set_battery_img(scr_data->view, icon);
}

static void standby_text_timer_update(struct home_nav_scr_data *scr_data)
{
    if (!scr_data || !scr_data->view) {
        return;
    }

    uint32_t count = model_voice_standby_text_count_get();
    uint32_t interval_ms = model_voice_standby_text_interval_ms_get();

    bool allow_play = true;
    if (scr_data->img_rec_running || scr_data->work_type == WORK_TYPE_IMG_REC) {
        allow_play = false;
    }
    if (model_voice_cloud_is_running()) {
        allow_play = false;
    }
    if (model_voice_tts_is_playing()) {
        allow_play = false;
    }

    if (!allow_play || count == 0 || interval_ms == 0) {
        if (scr_data->standby_text_timer) {
            lv_timer_pause(scr_data->standby_text_timer);
        }
        return;
    }

    if (!scr_data->standby_text_timer) {
        scr_data->standby_text_timer = lv_timer_create(standby_text_timer_cb, interval_ms, scr_data);
        if (!scr_data->standby_text_timer) {
            return;
        }
    }

    lv_timer_set_period(scr_data->standby_text_timer, interval_ms);
    lv_timer_resume(scr_data->standby_text_timer);
}

static void standby_text_timer_cb(lv_timer_t *timer)
{
    struct home_nav_scr_data *scr_data = timer ? timer->user_data : NULL;
    if (!scr_data || !scr_data->view) {
        return;
    }

    standby_text_timer_update(scr_data);

    uint32_t count = model_voice_standby_text_count_get();
    if (count == 0) {
        return;
    }

    scr_data->standby_text_index = (scr_data->standby_text_index + 1) % count;
    const char *text = model_voice_standby_text_get(scr_data->standby_text_index);
    if (text) {
        lisa_ui_llm_primary_set_content_text(scr_data->view, text);
    }
}

static void model_voice_on_standby_texts_changed(void *arg)
{
    struct home_nav_scr_data *scr_data = arg;
    if (!scr_data || !scr_data->view) {
        return;
    }

    scr_data->standby_text_index = 0;
    standby_text_timer_update(scr_data);

    if (scr_data->img_rec_running || scr_data->work_type == WORK_TYPE_IMG_REC) {
        return;
    }
    if (model_voice_cloud_is_running()) {
        return;
    }
    if (model_voice_tts_is_playing()) {
        return;
    }

    const char *text = model_voice_standby_text_get(0);
    if (text) {
        lisa_ui_llm_primary_set_content_text(scr_data->view, text);
    }
}

static void camera_capture_timer_callback(lv_timer_t *timer)
{
    if (!timer || !timer->user_data) {
        return;
    }

    struct home_nav_scr_data *d = timer->user_data;
    uint16_t width = 0;
    uint16_t height = 0;
    uint32_t image_size = 0;
    int ret = 0;

    if (!d->view) {
        lv_timer_pause(timer);
        return;
    }

    /* 如果识别已触发，停止预览拍照 */
    if (!d->img_rec_running) {
        lv_timer_pause(timer);
        return;
    }

    d->img.header.cf = LV_IMG_CF_TRUE_COLOR;
    d->img.header.always_zero = 0;
    d->img.header.reserved = 0;

    ret = model_camera_get_framesize(&width, &height);
    if (ret != 0 || width == 0 || height == 0) {
        LISA_UI_LOGE("Get camera frame size failed: ret=%d, w=%u, h=%u", ret, width, height);
        d->img_rec_running = false;
        d->img_rec_triggered = false;
        lv_timer_pause(timer);
        return;
    }

    image_size = (uint32_t)width * (uint32_t)height * 2U;
    if (image_size == 0) {
        LISA_UI_LOGE("Invalid image size, w=%u, h=%u", width, height);
        d->img_rec_running = false;
        d->img_rec_triggered = false;
        lv_timer_pause(timer);
        return;
    }

    if (d->cap_buf == NULL || d->cap_buf_size < image_size) {
        if (d->cap_buf != NULL) {
            lisa_ui_free(d->cap_buf);
            d->cap_buf = NULL;
            d->cap_buf_size = 0;
        }

        d->cap_buf = lisa_ui_malloc(image_size);
        if (!d->cap_buf) {
            LISA_UI_LOGE("Failed to alloc capture buffer: %u", image_size);
            d->img_rec_running = false;
            d->img_rec_triggered = false;
            lv_timer_pause(timer);
            return;
        }
        d->cap_buf_size = image_size;
    }

    ret = model_camera_capture(d->cap_buf, image_size);
    if (ret != 0) {
        LISA_UI_LOGE("Camera capture failed: %d", ret);
        d->img_rec_running = false;
        d->img_rec_triggered = false;
        lv_timer_pause(timer);
        return;
    }

    d->img.data = d->cap_buf;
    d->img.data_size = image_size;
    d->img.header.w = width;
    d->img.header.h = height;

    if (!d->img_rec_is_button) {
        lisa_ui_llm_primary_img_show(d->view, &d->img);
    }
}

static void img_hide_timer_cb(lv_timer_t *timer)
{
    struct home_nav_scr_data *scr_data = timer->user_data;

    if (scr_data) {
        hide_img_now(scr_data);
    }

    lv_timer_del(timer);

    scr_data->img_hide_timer = NULL;
}

static void hide_img_now(struct home_nav_scr_data *scr_data)
{
    if (!scr_data || !scr_data->view) {
        return;
    }

    lisa_ui_llm_primary_img_hide(scr_data->view);
    if (scr_data->work_type == WORK_TYPE_IMG_REC) {
        scr_data->work_type = WORK_TYPE_VOICE;
    }
    scr_data->img_rec_running = false;
    scr_data->img_rec_in_progress = false;
    scr_data->img_rec_triggered = false;
}

static void model_voice_on_image_preview(void *arg)
{
    struct home_nav_scr_data *scr_data = arg;
    uint16_t width = 0;
    uint16_t height = 0;
    int ret = 0;

    if (!scr_data || !scr_data->view) {
        return;
    }

    if (lisa_ui_nav_scr_get_top_id() != LISA_UI_NAV_SCR_ID_HOME) {
        return;
    }

    /* 如果识图任务正在进行，忽略按键预览 */
    if (scr_data->img_rec_in_progress) {
        LISA_UI_LOGI("Image recognition in progress, ignore button press");
        return;
    }

    /* 如果识图正在进行，忽略按键事件 */
    if (scr_data->img_rec_running) {
        LISA_UI_LOGI("Image recognition in progress, ignore button press");
        return;
    }

    if (!model_voice_cloud_is_connected()) {
        lisa_ui_toast_show(_("service disconnected"));
        return;
    }

    if (!model_camera_is_inited()) {
        ret = model_camera_init();
        if (ret != 0) {
            LISA_UI_LOGE("Camera init failed: %d", ret);
            lisa_ui_toast_show(_("recognition error"));
            return;
        }
    }

    ret = model_camera_get_framesize(&width, &height);
    if (ret != 0 || width == 0 || height == 0) {
        LISA_UI_LOGE("Camera unavailable for preview: ret=%d, w=%u, h=%u", ret, width, height);
        lisa_ui_toast_show(_("recognition error"));
        return;
    }

    lisa_ui_llm_primary_img_hint_hide(scr_data->view);

    /* 识图过程中不显示本地唤醒提示文案 */
    lisa_ui_llm_primary_set_content_text(scr_data->view, "");
    scr_data->work_type = WORK_TYPE_IMG_REC;
    scr_data->img_rec_is_mcp = model_voice_img_rec_is_mcp() ? 1 : 0;
    scr_data->img_rec_is_button = model_voice_img_rec_is_mcp() ? 0 : 1;

    standby_text_timer_update(scr_data);

    if (scr_data->camera_capture_timer == NULL) {
        scr_data->camera_capture_timer = lv_timer_create(camera_capture_timer_callback, 60, scr_data);
    } else {
        lv_timer_resume(scr_data->camera_capture_timer);
    }

    scr_data->img_rec_running = true;
    scr_data->img_rec_in_progress = false;
    scr_data->img_rec_triggered = false;
    LISA_UI_LOGI("Image recognition started, flag set");
}

static void model_voice_on_image_url(void *arg, const char *url)
{
    struct home_nav_scr_data *scr_data = arg;
    if (!scr_data || !scr_data->view || !url || url[0] == '\0') {
        return;
    }

    if (lisa_ui_nav_scr_get_top_id() != LISA_UI_NAV_SCR_ID_HOME) {
        return;
    }

    if (scr_data->img_hide_timer) {
        lv_timer_del(scr_data->img_hide_timer);
        scr_data->img_hide_timer = NULL;
    }

    lv_img_dsc_t *img_dsc = lv_img_net_load(url);
    if (!img_dsc) {
        LISA_UI_LOGE("load net image failed: %s", url);
        return;
    }

    if (scr_data->net_img) {
        lv_img_net_free(scr_data->net_img);
        scr_data->net_img = NULL;
    }

    scr_data->net_img = img_dsc;
    lisa_ui_llm_primary_img_show(scr_data->view, scr_data->net_img);
    lisa_ui_llm_primary_img_hint_show(scr_data->view, "图片可在小聆AI小程序中查看");
    scr_data->work_type = WORK_TYPE_IMG_REC;
    scr_data->img_rec_running = false;
    scr_data->img_rec_in_progress = false;
    scr_data->img_rec_triggered = false;
    LISA_UI_LOGI("display net image success");
}

static void model_voice_on_info_show(void *arg)
{
    if (lisa_ui_nav_scr_get_top_id() != LISA_UI_NAV_SCR_ID_INFO) {
        lisa_ui_nav_scr_nav_to(LISA_UI_NAV_SCR_ID_INFO);
    }
}

static void model_voice_on_show_qrcode(void *arg)
{
    LISA_UI_LOGI("Show qrcode callback, navigating to quota qrcode page");
    if (lisa_ui_nav_scr_get_top_id() != LISA_UI_NAV_SCR_ID_QUOTA_QRCODE) {
        lisa_ui_nav_scr_nav_to(LISA_UI_NAV_SCR_ID_QUOTA_QRCODE);
    } else {
        LISA_UI_LOGI("Already on quota qrcode page, skip navigation");
    }
}

#ifdef CONFIG_OTA
static void model_voice_on_ota_state_change(const ota_state_t *state, void *arg)
{
    if (!state) {
        return;
    }

    if (state->state == OTA_STATE_CHECKING) {
        // lisa_ui_toast_show("正在检查更新…");
    } else if (state->state == OTA_STATE_UPDATING) {
        if (lisa_ui_nav_scr_get_top_id() != LISA_UI_NAV_SCR_ID_OTA) {
            lisa_ui_nav_scr_nav_to(LISA_UI_NAV_SCR_ID_OTA);
        }
    } else if (state->state == OTA_STATE_UP_TO_DATE) {
        // lisa_ui_toast_show("已是最新版本");
    }
}
#endif

static void anim_timer_callback(lv_timer_t *timer)
{
    struct home_nav_scr_data *d = timer->user_data;
    lv_timer_pause(timer);

    show_emoji_anim(d, EMOJI_NAME_WAKEUP, 0);

    d->mcp_emoji_running = false;
}

static void wifi_status_timer_callback(lv_timer_t *timer)
{
    struct home_nav_scr_data *d = timer->user_data;

    model_wifi_status_t wifi_status = model_wifi_get_status();

    if (wifi_status == MODEL_WIFI_STATUS_CONNECTED) {
        lisa_ui_llm_primary_set_wifi_img(d->view, &icons_ic_status_wifi_lvl_3_png);
    } else {
        lisa_ui_llm_primary_set_wifi_img(d->view, &icons_ic_status_wifi_no_connect_png);
    }

    home_update_alarm_icon(d);
}

static void model_voice_on_image_rec(void *arg)
{
    struct home_nav_scr_data *scr_data = arg;
    uint16_t width = 0;
    uint16_t height = 0;
    uint16_t image_width = 0;
    uint16_t image_height = 0;
    uint32_t image_size = 0;

    if (lisa_ui_nav_scr_get_top_id() != LISA_UI_NAV_SCR_ID_HOME) {
        LISA_UI_LOGI("on image recv, skip");
        return;
    }

    /* 异步识图任务进行中时，忽略后续识图触发 */
    if (scr_data->img_rec_in_progress) {
        LISA_UI_LOGI("on image rec, in progress, ignore duplicate");
        return;
    }

    /* 双重检查锁定：先检查triggered标志，只有第一个请求能设置它 */
    if (scr_data->img_rec_triggered) {
        LISA_UI_LOGI("on image rec, already triggered, ignore duplicate");
        return;
    }
    scr_data->img_rec_triggered = true;

    if (scr_data->img_rec_running == false) {
        LISA_UI_LOGI("on image rec, img rec running: %d", scr_data->img_rec_running);
        scr_data->img_rec_triggered = false;
        return;
    }

    /* 立即暂停定时器并清除运行标志，防止重复触发识别 */
    if (scr_data->camera_capture_timer) {
        lv_timer_pause(scr_data->camera_capture_timer);
    }
    scr_data->img_rec_running = false;
    LISA_UI_LOGI("Image recognition triggered, flag cleared to prevent duplicate");

    if (scr_data->img_hide_timer) {
        lv_timer_del(scr_data->img_hide_timer);
        scr_data->img_hide_timer = NULL;
    }

    scr_data->img_rec_is_button = model_voice_img_rec_is_mcp() ? 0 : 1;

    standby_text_timer_update(scr_data);
 
    if (scr_data->camera_capture_timer != NULL) {
        lv_timer_pause(scr_data->camera_capture_timer);
        LISA_UI_LOGI("Camera capture timer paused");
    } else {
        LISA_UI_LOGI("Camera capture timer is NULL");
    }
    
    if (scr_data->cap_buf == NULL) {
        LISA_UI_LOGI("scr_data->cap_buf is null");
        scr_data->img_rec_triggered = false;
        return;
    }

    int frame_ret = model_camera_get_framesize(&width, &height);
    if (frame_ret != 0 || width == 0 || height == 0) {
        LISA_UI_LOGE("Get frame size failed before recognition: ret=%d, w=%u, h=%u", frame_ret, width, height);
        lisa_ui_toast_show(_("recognition error"));
        hide_img_now(scr_data);
        return;
    }

    image_size = (uint32_t)width * (uint32_t)height * 2U;
    if (image_size == 0 || scr_data->cap_buf_size < image_size) {
        LISA_UI_LOGE("Invalid image buffer size: cap=%u, need=%u", scr_data->cap_buf_size, image_size);
        lisa_ui_toast_show(_("recognition error"));
        hide_img_now(scr_data);
        return;
    }

    image_width = width;
    image_height = height;

    int rotate_ret = rotate_rgb565_cw90_inplace(scr_data->cap_buf, width, height);
    if (rotate_ret == 0) {
        image_width = height;
        image_height = width;
    } else {
        LISA_UI_LOGW("Rotate captured image failed: %d", rotate_ret);
    }
    image_size = (uint32_t)image_width * (uint32_t)image_height * 2U;
    
    /* Ensure the captured image is displayed before recognition */
    scr_data->img.header.cf = LV_IMG_CF_TRUE_COLOR;
    scr_data->img.header.always_zero = 0;
    scr_data->img.header.reserved = 0;
    scr_data->img.data = scr_data->cap_buf;
    scr_data->img.data_size = image_size;
    scr_data->img.header.w = image_width;
    scr_data->img.header.h = image_height;
    lisa_ui_llm_primary_img_show(scr_data->view, &scr_data->img);
    LISA_UI_LOGI("Image displayed for recognition");
    
    if (!model_voice_cloud_is_connected()) {
        lisa_ui_toast_show(_("Please wake me"));
        scr_data->img_rec_triggered = false;
        return;
    } else {
        int r = model_voice_img_recognition(scr_data->cap_buf, image_size, image_width, image_height);
        if (r == 0) {
            scr_data->img_rec_in_progress = true;
            lisa_ui_llm_primary_set_status_text(scr_data->view, _("recognizing"));
            LISA_UI_LOGI("Image recognition started");
        } else {
            lisa_ui_toast_show(_("recognition error"));
            scr_data->img_rec_triggered = false;
            scr_data->img_rec_running = true;
            if (scr_data->camera_capture_timer) {
                lv_timer_resume(scr_data->camera_capture_timer);
            }
        }
    }
}

static void model_voice_on_tts_player_playing(void *arg)
{
    struct home_nav_scr_data *scr_data = arg;

    scr_data->speaking = 1;

    if (scr_data->finished) {
        return;
    }

    lisa_ui_llm_primary_set_status_text(scr_data->view, _("speaking"));

    standby_text_timer_update(scr_data);
}

static void model_voice_on_tts_player_stoped(void *arg)
{
    struct home_nav_scr_data *scr_data = arg;

    scr_data->speaking = 0;

    if (scr_data->img_rec_is_button) {
        if (model_voice_cloud_is_running()) {
            voice_cloud_chat_stop();
        }

        hide_img_now(scr_data);
        scr_data->img_rec_is_button = 0;

        if (!scr_data->finished) {
            lv_timer_pause(scr_data->anim_timer);
            show_emoji_anim(scr_data, EMOJI_NAME_NEUTRAL, 0);
            lisa_ui_llm_primary_set_content_text(scr_data->view, model_voice_role_propmt_get());
            scr_data->finished = 1;
        }

        const char *init_status =
            model_voice_cloud_is_connected() ? _("Please wake me") : _("service disconnected");
        lisa_ui_llm_primary_set_status_text(scr_data->view, init_status);

        standby_text_timer_update(scr_data);
        return;
    }

    if (scr_data->work_type == WORK_TYPE_IMG_REC) {
        /* Hide image after TTS playback completes (both single and full duplex mode) */
        hide_img_now(scr_data);
        show_emoji_anim(scr_data, EMOJI_NAME_NEUTRAL, 0);

        /* In full duplex mode, set status to listening if session is still running */
        if (model_voice_cloud_is_running()) {
            lisa_ui_llm_primary_set_status_text(scr_data->view, _("listening"));
        } else {
            const char *init_status =
                model_voice_cloud_is_connected() ? _("Please wake me") : _("service disconnected");
            lisa_ui_llm_primary_set_status_text(scr_data->view, init_status);
        }
    } else {
        if (model_voice_cloud_is_running()) {
            lisa_ui_llm_primary_set_status_text(scr_data->view, _("listening"));
        } else {
            if (!scr_data->finished) {
                lv_timer_pause(scr_data->anim_timer);
                show_emoji_anim(scr_data, EMOJI_NAME_NEUTRAL, 0);
                lisa_ui_llm_primary_set_content_text(scr_data->view, model_voice_role_propmt_get());
                scr_data->finished = 1;
            }
            const char *init_status =
                model_voice_cloud_is_connected() ? _("Please wake me") : _("service disconnected");
            lisa_ui_llm_primary_set_status_text(scr_data->view, init_status);
        }
    }

    standby_text_timer_update(scr_data);
}

static void model_voice_on_emoji(void *arg, const char *name)
{
    struct home_nav_scr_data *scr_data = arg;
    if (scr_data->mcp_emoji_running) {
        LISA_UI_LOGI("ignore emoji display, mcp emoji is running");
        return;
    }

    if (scr_data->finished) {
        return;
    }

    LISA_UI_LOGI("on emoji, name: %s", name);

    show_emoji_anim(scr_data, name, 0);

    lv_timer_reset(scr_data->anim_timer);
    lv_timer_resume(scr_data->anim_timer);
}

static void model_voice_on_mcp_emoji(void *arg, const char *name)
{
    struct home_nav_scr_data *scr_data = arg;
    if (scr_data->finished) {
        return;
    }

    LISA_UI_LOGI("on emoji, name: %s", name);
    scr_data->mcp_emoji_running = true;
    show_emoji_anim(scr_data, name, 0);

    lv_timer_reset(scr_data->anim_timer);
    lv_timer_resume(scr_data->anim_timer);
}

static void model_voice_on_mcp_loading(void *arg, bool is_loading, const char *loading_text)
{
    struct home_nav_scr_data *scr_data = arg;

    if (!scr_data || !scr_data->view) {
        return;
    }

    if (is_loading) {
        scr_data->mcp_loading = 1;
        scr_data->mcp_emoji_running = 1;

        lv_timer_pause(scr_data->anim_timer);
        show_emoji_anim(scr_data, EMOJI_NAME_WAIT, 1);

        if (loading_text && loading_text[0] != '\0') {
            lisa_ui_llm_primary_set_content_text(scr_data->view, loading_text);
        }
        return;
    }

    scr_data->mcp_loading = 0;
    scr_data->mcp_emoji_running = 0;


}

static void model_voice_on_connected(void *arg)
{
    struct home_nav_scr_data *scr_data = arg;

    if (lisa_ui_nav_scr_get_top_id() != LISA_UI_NAV_SCR_ID_HOME) {
        lisa_ui_nav_scr_nav_to(LISA_UI_NAV_SCR_ID_HOME);
    }

    lisa_ui_llm_primary_set_status_text(scr_data->view, _("Please wake me"));

    lisa_ui_llm_primary_set_content_text(scr_data->view, model_voice_role_propmt_get());
}

static void model_voice_on_disconnected(void *arg)
{
    struct home_nav_scr_data *scr_data = arg;

    scr_data->finished = 1;

    lv_timer_pause(scr_data->anim_timer);

    show_emoji_anim(scr_data, EMOJI_NAME_NEUTRAL, 0);

    lisa_ui_llm_primary_set_status_text(scr_data->view, _("service disconnected"));

    lisa_ui_llm_primary_set_content_text(scr_data->view, model_voice_role_propmt_get());
}

static void model_voice_on_start(void *arg)
{
    struct home_nav_scr_data *scr_data = arg;

    lisa_ui_llm_primary_img_hide(scr_data->view);
    show_emoji_anim(scr_data, EMOJI_NAME_WAKEUP, 1);

    lisa_ui_llm_primary_set_status_text(scr_data->view, _("listening"));

    lisa_ui_llm_primary_set_content_text(scr_data->view, _("Please speak"));

    scr_data->finished = 0;
    scr_data->work_type = WORK_TYPE_VOICE;

    standby_text_timer_update(scr_data);

}

static void enter_standby(struct home_nav_scr_data *scr_data)
{
    LISA_UI_LOGI("enter_standby");

    scr_data->finished = 1;

    /* For image recognition, don't process finish event immediately */
    /* Wait for TTS to complete, then handle in on_tts_player_stoped */
    if (scr_data->work_type == WORK_TYPE_IMG_REC) {
        lisa_ui_llm_primary_img_hide(scr_data->view);
        scr_data->work_type = WORK_TYPE_VOICE;
        scr_data->img_rec_running = false;
        scr_data->img_rec_in_progress = false;
        scr_data->img_rec_triggered = false;
    }

    lisa_ui_llm_primary_set_content_text(scr_data->view, model_voice_role_propmt_get());
    lv_timer_pause(scr_data->anim_timer);
    show_emoji_anim(scr_data, EMOJI_NAME_NEUTRAL, 0);
    lisa_ui_llm_primary_set_status_text(scr_data->view, _("Please wake me"));
    standby_text_timer_update(scr_data);
}

static void finished_tiemr_callback(lv_timer_t *timer)
{
    struct home_nav_scr_data *scr_data = timer->user_data;

    LISA_UI_LOGI("finished_tiemr_callback, speaking: %d", scr_data->speaking);

    if (!scr_data->speaking) {
        enter_standby(scr_data);
    }

    lv_timer_del(timer);
}

static void model_voice_on_finished(void *arg)
{
    struct home_nav_scr_data *scr_data = arg;
    uint8_t mode = model_voice_wakeup_mode_get();

    LISA_UI_LOGI("on finished, speaking: %d, wakeup mode: %d", scr_data->speaking, mode);

    if (mode) {
        lv_timer_pause(scr_data->anim_timer);
        show_emoji_anim(scr_data, EMOJI_NAME_NEUTRAL, 0);
    }

    if (scr_data->speaking) {
        return;
    } else {
        if (mode) {
            LISA_UI_LOGI("on finished, single mode, check speaking status after 1000ms");
            lv_timer_create(finished_tiemr_callback, 1000, scr_data);
            return;
        }
    }

    enter_standby(scr_data);
}

static void model_voice_on_tts_text_start(void *arg)
{
    struct home_nav_scr_data *scr_data = arg;
}

static void model_voice_on_tts_text_end(void *arg)
{
    struct home_nav_scr_data *scr_data = arg;
}

static void model_voice_on_tts_text_update(const char *text, void *arg)
{
    struct home_nav_scr_data *scr_data = arg;
}

static void model_voice_on_iat_text_start(void *arg)
{
    struct home_nav_scr_data *scr_data = arg;

    if (scr_data->finished) {
        return;
    }

    lisa_ui_llm_primary_set_status_text(scr_data->view, _("listening"));
}

static void model_voice_on_iat_text_end(void *arg)
{
    struct home_nav_scr_data *scr_data = arg;

    if (scr_data->finished) {
        return;
    }

    if (!scr_data->speaking) {
        lisa_ui_llm_primary_set_status_text(scr_data->view, _("listening"));
    }
}

static void model_voice_on_iat_text_update(const char *text, void *arg)
{
    struct home_nav_scr_data *scr_data = arg;

    if (scr_data->finished) {
        return;
    }

    if (!text || text[0] == '\0') {
        return;
    }

    if (scr_data->work_type == WORK_TYPE_IMG_REC) {
        hide_img_now(scr_data);
    }

    lisa_ui_llm_primary_set_content_text(scr_data->view, text);
    lisa_ui_llm_primary_set_status_text(scr_data->view, _("listening"));
}

static void model_voice_on_standby_text_update(void *arg, const char *text, bool is_cloud_text)
{
    struct home_nav_scr_data *scr_data = arg;

    if (!scr_data || !scr_data->view || !text) {
        return;
    }

    if (scr_data->img_rec_running || scr_data->work_type == WORK_TYPE_IMG_REC) {
        return;
    }

    lisa_ui_llm_primary_set_content_text(scr_data->view, text);
}

static void model_voice_on_wakeup_mode_changed(void *arg, model_voice_wakeup_mode_t mode)
{
    struct home_nav_scr_data *scr_data = arg;

    if (!scr_data || !scr_data->view) {
        return;
    }

    bool is_full_duplex = (mode == MODEL_VOICE_WAKEUP_MODE_VOICE_MULTI);
    lisa_ui_llm_primary_set_full_duplex_icon_visible(scr_data->view, is_full_duplex);
}

#ifndef CONFIG_BOARD_ARCS_MINI
static void settings_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED) {
        lisa_ui_nav_scr_nav_to(LISA_UI_NAV_SCR_ID_SETTING);
    }
}
#endif

static void show_emoji_anim(struct home_nav_scr_data *d, const char *emoji_name, uint8_t imm)
{
    const lisa_ui_anim_ext_config_t *anim_config = emoji_anim_get_by_name(emoji_name);
    if (anim_config == NULL) {
        return;
    }

    lv_obj_t *anim = lisa_ui_llm_primary_emoji_anim_get(d->view);

    if (imm) {
        lisa_ui_anim_ext_next_imm(anim, anim_config);
    } else {
        lisa_ui_anim_ext_next(anim, anim_config);
    }
}

static void home_reset(struct home_nav_scr_data *scr_data)
{
    const char *iat_text = model_voice_last_iat_text_get();

    hide_img_now(scr_data);
    if (model_voice_cloud_is_running()) {
        if (iat_text && iat_text[0] != '\0') {
            lisa_ui_llm_primary_set_content_text(scr_data->view, iat_text);
        } else {
            lisa_ui_llm_primary_set_content_text(scr_data->view, _("Please speak"));
        }
        lisa_ui_llm_primary_set_status_text(scr_data->view, _("listening"));
    } else {
        lisa_ui_llm_primary_set_content_text(scr_data->view, model_voice_role_propmt_get());
        const char *init_status = model_voice_cloud_is_connected() ? _("Please wake me") : _("service disconnected");
        lisa_ui_llm_primary_set_status_text(scr_data->view, init_status);
    }
    home_update_full_duplex_icon(scr_data);
    home_update_alarm_icon(scr_data);
    home_update_battery_icon(scr_data);
    show_emoji_anim(scr_data, EMOJI_NAME_NEUTRAL, true);
    lv_timer_pause(scr_data->anim_timer);

    standby_text_timer_update(scr_data);
}

static int home_nav_scr_open(const struct lisa_ui_nav_scr *scr, void **data)
{
    LISA_UI_LOGD("nav scr open, id: %d", scr->unique_id);

    model_voice_init();
    #if !CONFIG_4G_MODULE
    int cam_init_ret = model_camera_init();
    if (cam_init_ret != 0) {
        LISA_UI_LOGW("Camera init failed on home open: %d", cam_init_ret);
    }
    #endif
    model_wifi_init();

    struct home_nav_scr_data *scr_data = lisa_ui_malloc(sizeof(struct home_nav_scr_data));
    if (!scr_data) {
        LISA_UI_LOGE("failed to allocate memory for home_nav_scr_data");
        return -1;
    }
    memset(scr_data, 0, sizeof(struct home_nav_scr_data));

    scr_data->view = lisa_ui_llm_primary_create(lv_scr_act());
#ifndef CONFIG_BOARD_ARCS_MINI
    lisa_ui_llm_primary_set_settings_icon_event_cb(scr_data->view, settings_btn_event_cb, NULL);
#endif

    scr_data->anim_timer = lv_timer_create(anim_timer_callback, 5000, scr_data);
    lv_timer_pause(scr_data->anim_timer);
    lisa_ui_llm_primary_set_wifi_img(scr_data->view, &icons_ic_status_wifi_no_connect_png);
    scr_data->wifi_status_timer = lv_timer_create(wifi_status_timer_callback, 200, scr_data);
    scr_data->standby_text_timer = NULL;

    model_battery_cb_register(model_battery_on_battery_status_update, scr_data);
    home_update_battery_icon(scr_data);

    *data = scr_data;

    model_voice_cb_register(&model_voice_cbs, scr_data);

    return 0;
}

static int home_nav_scr_show(const struct lisa_ui_nav_scr *scr, void *data)
{
    LISA_UI_LOGD("Home nav scr show, id: %d", scr->unique_id);

    struct home_nav_scr_data *scr_data = (struct home_nav_scr_data *)data;
    if (!scr_data || !scr_data->view) {
        LISA_UI_LOGE("Invalid home nav scr data");
        return -1;
    }

    lv_obj_clear_flag(scr_data->view, LV_OBJ_FLAG_HIDDEN);

    model_voice_on();
    home_reset(scr_data);

    return 0;
}

static int home_nav_scr_pause(const struct lisa_ui_nav_scr *scr, void *data)
{
    LISA_UI_LOGD("nav scr pause, id: %d", scr->unique_id);

    struct home_nav_scr_data *scr_data = (struct home_nav_scr_data *)data;
    if (scr_data && scr_data->view) {
        lv_obj_add_flag(scr_data->view, LV_OBJ_FLAG_HIDDEN);
    }

    if (scr_data && scr_data->wifi_status_timer) {
        lv_timer_pause(scr_data->wifi_status_timer);
    }

    if (scr_data && scr_data->camera_capture_timer) {
        lv_timer_pause(scr_data->camera_capture_timer);
    }

    /* 页面切换时重置识图标志，确保下次返回可以继续识图 */
    if (scr_data) {
        scr_data->img_rec_running = false;
        scr_data->img_rec_in_progress = false;
        scr_data->img_rec_triggered = false;
        LISA_UI_LOGI("Page paused, img_rec_running flag reset");
    }


    if (!model_voice_cloud_is_running()) {
        model_voice_off();
    }

    return 0;
}

static int home_nav_scr_resume(const struct lisa_ui_nav_scr *scr, void *data)
{
    LISA_UI_LOGD("nav scr resume, id: %d", scr->unique_id);

    struct home_nav_scr_data *scr_data = (struct home_nav_scr_data *)data;
    if (scr_data && scr_data->view) {
        lv_obj_clear_flag(scr_data->view, LV_OBJ_FLAG_HIDDEN);
    }

    if (scr_data && scr_data->wifi_status_timer) {
        lv_timer_resume(scr_data->wifi_status_timer);
    }

    model_voice_on();
    home_reset(scr_data);

    return 0;
}

static int home_nav_scr_close(const struct lisa_ui_nav_scr *scr, void *data)
{
    LISA_UI_LOGD("nav scr close, id: %d", scr->unique_id);

    model_voice_cb_unregister(&model_voice_cbs);
    model_battery_cb_unregister(model_battery_on_battery_status_update);

    struct home_nav_scr_data *scr_data = (struct home_nav_scr_data *)data;
    if (scr_data) {
        if (scr_data->anim_timer != NULL) {
            lv_timer_del(scr_data->anim_timer);
            scr_data->anim_timer = NULL;
        }
        if (scr_data->wifi_status_timer != NULL) {
            lv_timer_del(scr_data->wifi_status_timer);
            scr_data->wifi_status_timer = NULL;
        }
        if (scr_data->camera_capture_timer != NULL) {
            lv_timer_del(scr_data->camera_capture_timer);
            scr_data->camera_capture_timer = NULL;
        }
        if (scr_data->standby_text_timer != NULL) {
            lv_timer_del(scr_data->standby_text_timer);
            scr_data->standby_text_timer = NULL;
        }
        if (scr_data->cap_buf != NULL) {
            lisa_ui_free(scr_data->cap_buf);
            scr_data->cap_buf = NULL;
            scr_data->cap_buf_size = 0;
        }
        if (scr_data->net_img != NULL) {
            // lv_img_net_free(scr_data->net_img);
            scr_data->net_img = NULL;
        }
        if (scr_data->view) {
            lv_obj_del(scr_data->view);
        }
        lisa_ui_free(scr_data);
    }

    model_voice_off();

    return 0;
}

const struct lisa_ui_nav_scr home_nav_scr = {
    .unique_id = LISA_UI_NAV_SCR_ID_HOME,
    .open = home_nav_scr_open,
    .show = home_nav_scr_show,
    .pause = home_nav_scr_pause,
    .resume = home_nav_scr_resume,
    .close = home_nav_scr_close,
};

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "home_camera_preview_presenter.h"
#include "home_presenter.h"
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
#include "model_modem.h"
#include "model_wifi.h"
#include "model_battery.h"
#include "model_common.h"
#include "model_sd_music_sync.h"
#include "voice_cloud.h"
#include "display/lv_img_net_loader.h"

#ifdef LISA_UI_PLATFORM_ARCS
#include "app_datas.h"
#include "lisa_kv.h"
#include "kv_user.h"
#include "voice_msg.h"
#endif

#define HOME_STANDBY_SLEEP_DELAY_MS_DEFAULT (30 * 1000U)
#define HOME_STANDBY_DIM_BRIGHTNESS 30

/* MCP emoji loop 段最少播放时长，低于此时间不允许切换其他表情 */
#define MCP_EMOJI_MIN_LOOP_MS 3000

static void img_hide_timer_cb(lv_timer_t *timer);

static int show_emoji_anim(struct home_nav_scr_data *d, const char *emoji_name, uint8_t imm);
static void show_oneshot_emoji_once(struct home_nav_scr_data *d, const char *emoji_name);
static void home_stop_emoji_display(struct home_nav_scr_data *d);
static void model_voice_on_oneshot_emoji(void *arg, const char *name);
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
static void model_voice_on_image_rec_failed(void *arg);
static void model_voice_on_info_show(void *arg);
static void model_voice_on_image_preview(void *arg);
static void model_voice_on_image_url(void *arg, const char *url);
static void model_voice_on_standby_text_update(void *arg, const char *text, bool is_cloud_text);
static void model_voice_on_show_qrcode(void *arg);
static void model_voice_on_standby_texts_changed(void *arg);
static void model_voice_on_wakeup_mode_changed(void *arg, model_voice_wakeup_mode_t mode);
static void model_voice_on_battery_query(void *arg, uint8_t level, uint8_t status);
#ifdef CONFIG_LOG_UPLOAD
static void model_voice_on_log_upload_state_change(const log_upload_state_t *state, void *arg);
#endif
static void model_sd_music_sync_on_state_change(const voice_msg_sd_music_sync_state_t *state,
                                                 void *arg);
static void model_sd_music_on_card_removed(void *arg);
static void model_sd_music_on_play_failed(void *arg);
static void standby_text_timer_cb(lv_timer_t *timer);
static void standby_sleep_timer_cb(lv_timer_t *timer);
static void tts_text_stop_timer(struct home_nav_scr_data *scr_data);
static void home_enter_standby_state(struct home_nav_scr_data *scr_data, const char *status_text, uint8_t imm);
static bool home_try_enter_standby_after_tts(struct home_nav_scr_data *scr_data);
static void finished_tiemr_callback(lv_timer_t *timer);
#ifdef CONFIG_OTA
static void model_voice_on_ota_state_change(const ota_state_t *state, void *arg);
#endif
static void home_update_full_duplex_icon(struct home_nav_scr_data *scr_data);
static void home_update_alarm_icon(struct home_nav_scr_data *scr_data);
static void home_update_battery_icon(struct home_nav_scr_data *scr_data);
static void oneshot_emoji_timer_cb(lv_timer_t *timer);
static void home_schedule_standby_sleep(struct home_nav_scr_data *scr_data);
static void home_update_network_icon(struct home_nav_scr_data *scr_data);
static void battery_query_timer_cb(lv_timer_t *timer);

static void home_anim_timer_pause(struct home_nav_scr_data *scr_data)
{
    if (scr_data && scr_data->anim_timer) {
        lv_timer_pause(scr_data->anim_timer);
    }
}

static void home_anim_timer_reset_resume(struct home_nav_scr_data *scr_data)
{
    if (scr_data && scr_data->anim_timer) {
        lv_timer_reset(scr_data->anim_timer);
        lv_timer_resume(scr_data->anim_timer);
    }
}

static void home_update_emoji_visibility_for_image(struct home_nav_scr_data *d, lv_obj_t *anim)
{
    if (!anim) {
        return;
    }

    if (d && d->view && lisa_ui_llm_primary_img_is_visible(d->view)) {
        lv_obj_add_flag(anim, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(anim, LV_OBJ_FLAG_HIDDEN);
    }
}

static uint32_t home_get_standby_sleep_delay_ms(void)
{
    uint32_t delay_ms = HOME_STANDBY_SLEEP_DELAY_MS_DEFAULT;

#ifdef LISA_UI_PLATFORM_ARCS
    int kv_delay_ms = 0;

    if (lisa_kv_get_int(KV_KEY_USER_STANDBY_SLEEP_DELAY_MS, &kv_delay_ms) != 0) {
        lisa_kv_set_int(KV_KEY_USER_STANDBY_SLEEP_DELAY_MS, (int)delay_ms);
        return delay_ms;
    }

    if (kv_delay_ms > 0) {
        return (uint32_t)kv_delay_ms;
    }

    lisa_kv_set_int(KV_KEY_USER_STANDBY_SLEEP_DELAY_MS, (int)delay_ms);
#endif

    return delay_ms;
}

static void home_show_pending_alarm_toast(void)
{
    char toast_text[128] = {0};

    if (model_alarm_take_event_text(toast_text, sizeof(toast_text))) {
        lisa_ui_toast_show(toast_text);
    }
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

static const void *network_modem_icons[] = {
    &icons_ic_status_modem_lvl_0_png,
    &icons_ic_status_modem_lvl_1_png,
    &icons_ic_status_modem_lvl_2_png,
    &icons_ic_status_modem_lvl_3_png,
    &icons_ic_status_modem_lvl_4_png,
};

#ifdef CONFIG_BOARD_ARCS_MINI_DOLL_V2
static const void *battery_query_power_icons[] = {
    &query_ic_query_power0_doll_v2_png,
    &query_ic_query_power1_doll_v2_png,
    &query_ic_query_power2_doll_v2_png,
    &query_ic_query_power3_doll_v2_png,
    &query_ic_query_power4_doll_v2_png,
    &query_ic_query_power5_doll_v2_png,
    &query_ic_query_power6_doll_v2_png,
    &query_ic_query_power7_doll_v2_png,
    &query_ic_query_power8_doll_v2_png,
    &query_ic_query_power9_doll_v2_png,
};

static const void *battery_query_charging_icons[] = {
    &query_ic_query_charging0_doll_v2_png,
    &query_ic_query_charging1_doll_v2_png,
    &query_ic_query_charging2_doll_v2_png,
    &query_ic_query_charging3_doll_v2_png,
    &query_ic_query_charging4_doll_v2_png,
    &query_ic_query_charging5_doll_v2_png,
    &query_ic_query_charging6_doll_v2_png,
    &query_ic_query_charging7_doll_v2_png,
    &query_ic_query_charging8_doll_v2_png,
    &query_ic_query_charging9_doll_v2_png,
};
#endif

const struct model_voice_cb model_voice_cbs = {
    .on_tts_stoped = model_voice_on_tts_player_stoped,
    .on_tts_playing = model_voice_on_tts_player_playing,
    .on_emoji = model_voice_on_emoji,
    .on_oneshot_emoji = model_voice_on_oneshot_emoji,
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
    .on_image_rec_failed = model_voice_on_image_rec_failed,
    .on_info_show = model_voice_on_info_show,
    .on_image_preview = model_voice_on_image_preview,
    .on_image_url = model_voice_on_image_url,
    .on_standby_texts_changed = model_voice_on_standby_texts_changed,
    .on_standby_text_update = model_voice_on_standby_text_update,
    .on_show_qrcode = model_voice_on_show_qrcode,
    .on_battery_query = model_voice_on_battery_query,
    .on_wakeup_mode_changed = model_voice_on_wakeup_mode_changed,
#ifdef CONFIG_LOG_UPLOAD
    .on_log_upload_state_change = model_voice_on_log_upload_state_change,
#endif
    #ifdef CONFIG_OTA
    .on_ota_state_change = model_voice_on_ota_state_change,
    #endif
};

static const struct model_sd_music_sync_cb model_sd_music_sync_cbs = {
    .on_sd_music_sync_state_change = model_sd_music_sync_on_state_change,
    .on_sd_music_card_removed = model_sd_music_on_card_removed,
    .on_sd_music_play_failed = model_sd_music_on_play_failed,
};

static void home_update_full_duplex_icon(struct home_nav_scr_data *scr_data)
{
    const void *icon = NULL;
    bool visible = false;

    if (!scr_data || !scr_data->view) {
        return;
    }

#ifdef LISA_UI_PLATFORM_ARCS
    struct app_datas *app_datas = get_app_datas();
    if (app_datas && app_interaction_mode_is_continuous(app_datas->int_mode)) {
        visible = true;
        icon = app_interaction_mode_supports_barge_in(app_datas->int_mode) ?
            &icons_ic_status_duplex_interruptible_png :
            &icons_ic_status_duplex_non_interruptible_png;
    }
#else
    model_voice_wakeup_mode_t mode = model_voice_wakeup_mode_get();
    if (mode == MODEL_VOICE_WAKEUP_MODE_VOICE_MULTI) {
        visible = true;
        icon = &icons_ic_status_duplex_interruptible_png;
    }
#endif

    if (icon) {
        lisa_ui_llm_primary_set_full_duplex_icon_img(scr_data->view, icon);
    }
    lisa_ui_llm_primary_set_full_duplex_icon_visible(scr_data->view, visible);
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

static bool home_battery_status_is_charging(model_battery_status_t status)
{
    return status == MODEL_BATTERY_STATUS_CHARGING ||
           status == MODEL_BATTERY_STATUS_CHARGE_DONE;
}

static void home_stop_battery_query_timer(struct home_nav_scr_data *scr_data)
{
    if (!scr_data) {
        return;
    }

    if (scr_data->battery_query_timer) {
        lv_timer_del(scr_data->battery_query_timer);
        scr_data->battery_query_timer = NULL;
    }
}

#ifdef CONFIG_BOARD_ARCS_MINI_DOLL_V2
static const void *home_select_query_battery_icon(uint8_t level, uint8_t status)
{
    uint8_t idx = level / 10;
    if (idx > 9) {
        idx = 9;
    }

    if (status == VOICE_MSG_BATTERY_STATUS_CHARGING ||
        status == VOICE_MSG_BATTERY_STATUS_CHARGE_DONE) {
        return battery_query_charging_icons[idx];
    }

    if (status == VOICE_MSG_BATTERY_STATUS_NOT_CONNECT) {
        return battery_query_power_icons[idx];
    }

    return NULL;
}
#endif

static void home_store_emoji_name(char *dst, size_t dst_size, const char *emoji_name)
{
    if (!dst || dst_size == 0) {
        return;
    }

    if (!emoji_name) {
        dst[0] = '\0';
        return;
    }

    strncpy(dst, emoji_name, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

static bool home_is_same_emoji_anim(const struct home_nav_scr_data *scr_data, const char *emoji_name)
{
    const lisa_ui_anim_ext_config_t *current_anim = NULL;
    const lisa_ui_anim_ext_config_t *next_anim = NULL;

    if (!scr_data || !emoji_name || emoji_name[0] == '\0' || scr_data->current_emoji_name[0] == '\0') {
        return false;
    }

    if (strcmp(scr_data->current_emoji_name, emoji_name) == 0) {
        return true;
    }

    current_anim = emoji_anim_get_by_name(scr_data->current_emoji_name);
    next_anim = emoji_anim_get_by_name(emoji_name);

    return current_anim != NULL && current_anim == next_anim;
}

static const char *home_get_session_emoji_name(const struct home_nav_scr_data *scr_data)
{
    if (!scr_data) {
        return EMOJI_NAME_NEUTRAL;
    }

    if (scr_data->speaking || model_voice_tts_is_playing()) {
        if (!emoji_anim_has_name(EMOJI_NAME_SPEAKING)) {
            LOGW("NO SPEAKING");
            return scr_data->current_emoji_name[0] != '\0' ? scr_data->current_emoji_name : EMOJI_NAME_NEUTRAL;
        }
        return EMOJI_NAME_SPEAKING;
    }

    if (model_voice_cloud_is_running()) {
        return EMOJI_NAME_LISTENING;
    }

    return EMOJI_NAME_NEUTRAL;
}

static bool home_should_ignore_mcp_emoji(const char *emoji_name)
{
    if (!emoji_name || emoji_name[0] == '\0') {
        return true;
    }

    return strcmp(emoji_name, "聆听") == 0 ||
           strcmp(emoji_name, EMOJI_NAME_LISTENING) == 0 ||
           strcmp(emoji_name, "待机") == 0 ||
           strcmp(emoji_name, EMOJI_NAME_NEUTRAL) == 0;
}

static void home_restore_session_emoji(struct home_nav_scr_data *scr_data, uint8_t imm)
{
    if (!scr_data || !scr_data->view) {
        return;
    }

    /* MCP emoji 最少播放窗口内，阻止会话状态（说话/聆听/待机）自动切换表情 */
    if (scr_data->mcp_emoji_running) {
        uint32_t elapsed = lv_tick_get() - scr_data->mcp_emoji_start_tick;
        if (elapsed < MCP_EMOJI_MIN_LOOP_MS) {
            return;
        }
    }

    show_emoji_anim(scr_data, home_get_session_emoji_name(scr_data), imm);
}

static void home_stop_oneshot_emoji(struct home_nav_scr_data *scr_data)
{
    if (!scr_data) {
        return;
    }

    scr_data->oneshot_emoji_running = 0;
    if (scr_data->oneshot_emoji_timer) {
        lv_timer_del(scr_data->oneshot_emoji_timer);
        scr_data->oneshot_emoji_timer = NULL;
    }
}

static void home_stop_standby_sleep_timer(struct home_nav_scr_data *scr_data)
{
    if (!scr_data) {
        return;
    }

    if (scr_data->standby_sleep_timer) {
        lv_timer_del(scr_data->standby_sleep_timer);
        scr_data->standby_sleep_timer = NULL;
    }
}

static void home_camera_preview_uploading_clear(struct home_nav_scr_data *scr_data)
{
    lv_obj_t *content_label = NULL;
    const char *content_text = NULL;

    if (!scr_data || !model_camera_preview_is_result_active(&scr_data->camera_preview) ||
        !scr_data->view) {
        return;
    }

    content_label = lisa_ui_llm_primary_content_label_get(scr_data->view);
    if (!content_label) {
        return;
    }

    content_text = lv_textarea_get_text(content_label);
    if (!content_text || strcmp(content_text, "\n正在上传照片...") != 0) {
        return;
    }

    LISA_UI_LOGI("voice photo: clear uploading text");
    lisa_ui_llm_primary_set_content_text(scr_data->view, "");
    lv_obj_set_style_translate_y(content_label, 0, LV_PART_MAIN);
}

static void home_restore_last_iat_or_prompt(struct home_nav_scr_data *scr_data)
{
    const char *iat_text = NULL;

    if (!scr_data || !scr_data->view) {
        return;
    }

    iat_text = model_voice_last_iat_text_get();
    if (iat_text && iat_text[0] != '\0') {
        lisa_ui_llm_primary_set_content_text(scr_data->view, iat_text);
    } else {
        lisa_ui_llm_primary_set_content_text(scr_data->view, _("Please speak"));
    }
}

static bool home_cancel_voice_photo_preview_by_button(struct home_nav_scr_data *scr_data)
{
    if (!scr_data || !model_camera_preview_keep_preview_alive(&scr_data->camera_preview) ||
        model_camera_preview_source_get(&scr_data->camera_preview) != MODEL_CAMERA_PREVIEW_SOURCE_MCP) {
        return false;
    }

    LISA_UI_LOGI("cancel voice photo preview by image button");
    voice_msg_pub(VOICE_MSG_APP_CAMERA_PREVIEW_EXIT, NULL, 0);
    return true;
}

static bool home_should_keep_recognition_image(const struct home_nav_scr_data *scr_data)
{
    if (!scr_data || scr_data->work_type != WORK_TYPE_IMG_REC) {
        return false;
    }

    return scr_data->net_img != NULL;
}

static bool home_is_standby_idle(struct home_nav_scr_data *scr_data)
{
    if (!scr_data || !scr_data->view) {
        return false;
    }

    if (lisa_ui_nav_scr_get_top_id() != LISA_UI_NAV_SCR_ID_HOME) {
        return false;
    }

    if (!scr_data->finished || scr_data->speaking || model_voice_cloud_is_running()) {
        return false;
    }

    if (scr_data->img_rec_running || scr_data->img_rec_in_progress ||
        model_camera_preview_is_active(&scr_data->camera_preview) ||
        camera_preview_is_camera_work_type(scr_data->work_type)) {
        return false;
    }

    return true;
}

static void home_restore_standby_brightness(struct home_nav_scr_data *scr_data)
{
    if (!scr_data || !scr_data->standby_sleep_active) {
        return;
    }

    model_common_brightness_set_temp(scr_data->standby_sleep_restore_brightness);
    scr_data->standby_sleep_active = 0;
    show_emoji_anim(scr_data, EMOJI_NAME_NEUTRAL, 1);
}

void home_handle_activity(struct home_nav_scr_data *scr_data)
{
    if (!scr_data) {
        return;
    }

    if (scr_data->battery_query_timer) {
        home_stop_battery_query_timer(scr_data);
        if (scr_data->view) {
            lisa_ui_llm_primary_img_hide(scr_data->view);
        }
    }

    scr_data->standby_after_tts_pending = 0;
    home_stop_standby_sleep_timer(scr_data);
    home_restore_standby_brightness(scr_data);
}

static void standby_sleep_timer_cb(lv_timer_t *timer)
{
    struct home_nav_scr_data *scr_data = timer ? timer->user_data : NULL;

    if (!scr_data) {
        return;
    }

    if (scr_data->standby_sleep_timer == timer) {
        scr_data->standby_sleep_timer = NULL;
    }

    if (!home_is_standby_idle(scr_data) || scr_data->standby_sleep_active) {
        return;
    }

    scr_data->standby_sleep_restore_brightness = model_common_brightness_get();
    scr_data->standby_sleep_active = 1;

    LISA_UI_LOGI("standby idle timeout, dim brightness to %d, then loop sleepy emoji",
                 HOME_STANDBY_DIM_BRIGHTNESS);
    model_common_brightness_set_temp(HOME_STANDBY_DIM_BRIGHTNESS);
    show_emoji_anim(scr_data, EMOJI_NAME_SLEEPY, 1);
}

static void home_schedule_standby_sleep(struct home_nav_scr_data *scr_data)
{
    uint32_t standby_sleep_delay_ms = 0;

    if (!scr_data) {
        return;
    }

    home_stop_standby_sleep_timer(scr_data);

    if (!home_is_standby_idle(scr_data) || scr_data->standby_sleep_active) {
        return;
    }

    standby_sleep_delay_ms = home_get_standby_sleep_delay_ms();
    scr_data->standby_sleep_timer =
        lv_timer_create(standby_sleep_timer_cb, standby_sleep_delay_ms, scr_data);
    if (!scr_data->standby_sleep_timer) {
        return;
    }

    lv_timer_set_repeat_count(scr_data->standby_sleep_timer, 1);
}

static void home_update_battery_icon(struct home_nav_scr_data *scr_data)
{
    if (!scr_data || !scr_data->view) {
        return;
    }

    model_battery_info_t info;
    if (model_battery_get_info(&info) != 0) {
        return;
    }

    scr_data->last_battery_status = info.status;

    const void *icon = home_select_battery_icon(&info);
    lisa_ui_llm_primary_set_battery_img(scr_data->view, icon);
}

static const void *home_select_modem_icon(const model_modem_info_t *info)
{
    if (!info || !info->active || !info->connected) {
        return &icons_ic_status_modem_no_network_png;
    }

    uint8_t level = info->signal_level;
    if (level == 0) {
        return &icons_ic_status_modem_no_network_png;
    }

    if (level > (sizeof(network_modem_icons) / sizeof(network_modem_icons[0]))) {
        level = sizeof(network_modem_icons) / sizeof(network_modem_icons[0]);
    }

    return network_modem_icons[level - 1];
}

static const void *home_select_network_icon(const model_modem_info_t *info)
{
    model_wifi_status_t wifi_status = model_wifi_get_status();

    if (!info) {
        return &icons_ic_status_wifi_no_connect_png;
    }

    if (!info->active && wifi_status == MODEL_WIFI_STATUS_CONNECTED) {
        return &icons_ic_status_wifi_lvl_3_png;
    }

    if (info->active) {
        return home_select_modem_icon(info);
    }

    return info->is_modem_mode ? &icons_ic_status_modem_no_network_png
                           : &icons_ic_status_wifi_no_connect_png;
}

static void home_update_network_icon(struct home_nav_scr_data *scr_data)
{
    model_modem_info_t info;

    if (!scr_data || !scr_data->view) {
        return;
    }

    if (model_modem_poll() != 0 || model_modem_get_info(&info) != 0) {
        return;
    }

    lisa_ui_llm_primary_set_wifi_img(scr_data->view, home_select_network_icon(&info));
}

static void model_battery_on_battery_status_update(const model_battery_info_t *info, void *arg)
{
    struct home_nav_scr_data *scr_data = arg;
    bool was_charging;
    bool is_charging;

    if (!scr_data || !scr_data->view) {
        return;
    }

    was_charging = home_battery_status_is_charging(scr_data->last_battery_status);
    is_charging = home_battery_status_is_charging(info->status);
    if (scr_data->last_battery_status != MODEL_BATTERY_STATUS_UNKNOWN &&
        !was_charging && is_charging) {
        LISA_UI_LOGI("battery: non-charging -> charging, post oneshot emoji");
        model_voice_oneshot_emoji_post(EMOJI_NAME_BATTERY);
    }
    scr_data->last_battery_status = info->status;

    const void *icon = home_select_battery_icon(info);
    lisa_ui_llm_primary_set_battery_img(scr_data->view, icon);
}

void standby_text_timer_update(struct home_nav_scr_data *scr_data)
{
    if (!scr_data || !scr_data->view) {
        return;
    }

    uint32_t count = model_voice_standby_text_count_get();
    uint32_t interval_ms = model_voice_standby_text_interval_ms_get();

    bool allow_play = true;
    if (scr_data->img_rec_running || model_camera_preview_is_active(&scr_data->camera_preview) ||
        camera_preview_is_camera_work_type(scr_data->work_type)) {
        allow_play = false;
    }
    if (model_voice_cloud_is_running()) {
        allow_play = false;
    }
    if (model_voice_tts_is_playing()) {
        allow_play = false;
    }
    if (scr_data->music_text_active) {
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
    if (!model_voice_music_is_playing()) {
        scr_data->music_text_active = 0;
    }
    standby_text_timer_update(scr_data);

    if (scr_data->img_rec_running || model_camera_preview_is_active(&scr_data->camera_preview) ||
        camera_preview_is_camera_work_type(scr_data->work_type)) {
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

static void img_hide_timer_cb(lv_timer_t *timer)
{
    struct home_nav_scr_data *scr_data = timer->user_data;

    if (scr_data) {
        camera_preview_hide(scr_data);
    }

    lv_timer_del(timer);

    scr_data->img_hide_timer = NULL;
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

    if (home_cancel_voice_photo_preview_by_button(scr_data)) {
        return;
    }

    if (lisa_ui_nav_scr_get_top_id() != LISA_UI_NAV_SCR_ID_HOME) {
        return;
    }

    home_handle_activity(scr_data);

    if (model_camera_preview_is_active(&scr_data->camera_preview) ||
        scr_data->work_type == WORK_TYPE_CAMERA) {
        LISA_UI_LOGI("photo preview active, ignore image preview");
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
    scr_data->finished = 0;
    scr_data->work_type = WORK_TYPE_IMG_REC;
    scr_data->img_rec_is_mcp = model_voice_img_rec_is_mcp() ? 1 : 0;
    scr_data->img_rec_is_button = model_voice_img_rec_is_mcp() ? 0 : 1;

    standby_text_timer_update(scr_data);

    if (scr_data->camera_capture_timer == NULL) {
        scr_data->camera_capture_timer = lv_timer_create(camera_preview_capture_timer_cb, 60, scr_data);
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
    if (scr_data->standby_text_timer) {
        lv_timer_pause(scr_data->standby_text_timer);
    }
    lisa_ui_llm_primary_set_content_text(scr_data->view, "");
    LISA_UI_LOGI("display net image success");
}

static void model_voice_on_info_show(void *arg)
{
    int top_id = lisa_ui_nav_scr_get_top_id();

    if (top_id == LISA_UI_NAV_SCR_ID_INFO) {
        LISA_UI_LOGI("Info page already on top, reopen to refresh content");
        if (lisa_ui_nav_scr_nav_back() != 0) {
            LISA_UI_LOGW("Failed to refresh info page: nav_back failed");
        }
    }

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

static void battery_query_timer_cb(lv_timer_t *timer)
{
    struct home_nav_scr_data *scr_data = timer ? timer->user_data : NULL;

    if (!scr_data) {
        return;
    }

    if (scr_data->battery_query_timer == timer) {
        scr_data->battery_query_timer = NULL;
    }
    lv_timer_del(timer);

    if (!scr_data->view) {
        return;
    }

    lisa_ui_llm_primary_img_hide(scr_data->view);

    if (lisa_ui_nav_scr_get_top_id() != LISA_UI_NAV_SCR_ID_HOME) {
        lisa_ui_nav_scr_nav_to(LISA_UI_NAV_SCR_ID_HOME);
    }
}

static void model_voice_on_battery_query(void *arg, uint8_t level, uint8_t status)
{
#ifdef CONFIG_BOARD_ARCS_MINI_DOLL_V2
    struct home_nav_scr_data *scr_data = arg;
    const void *query_icon = NULL;

    if (!scr_data || !scr_data->view) {
        return;
    }

    query_icon = home_select_query_battery_icon(level, status);
    if (!query_icon) {
        return;
    }

    home_handle_activity(scr_data);
    home_stop_battery_query_timer(scr_data);

    if (lisa_ui_nav_scr_get_top_id() != LISA_UI_NAV_SCR_ID_HOME) {
        lisa_ui_nav_scr_nav_to(LISA_UI_NAV_SCR_ID_HOME);
    }

    camera_preview_hide(scr_data);
    lisa_ui_llm_primary_img_hint_hide(scr_data->view);
    lisa_ui_llm_primary_finger_hint_hide(scr_data->view);
    lisa_ui_llm_primary_query_img_show(scr_data->view, query_icon);

    scr_data->battery_query_timer = lv_timer_create(battery_query_timer_cb, 5000, scr_data);
    if (!scr_data->battery_query_timer) {
        lisa_ui_llm_primary_img_hide(scr_data->view);
        return;
    }
    lv_timer_set_repeat_count(scr_data->battery_query_timer, 1);
#else
    (void)arg;
    (void)level;
    (void)status;
#endif
}

#ifdef CONFIG_OTA
static void model_voice_on_ota_state_change(const ota_state_t *state, void *arg)
{
    if (!state) {
        return;
    }

    if (state->state == OTA_STATE_PACKAGE_INFO ||
        state->state == OTA_STATE_UPDATING) {
        if (lisa_ui_nav_scr_get_top_id() != LISA_UI_NAV_SCR_ID_OTA) {
            lisa_ui_nav_scr_nav_to(LISA_UI_NAV_SCR_ID_OTA);
        }
    } else if (state->state == OTA_STATE_UP_TO_DATE) {
        if (lisa_ui_nav_scr_get_top_id() != LISA_UI_NAV_SCR_ID_HOME) {
            lisa_ui_nav_scr_nav_to(LISA_UI_NAV_SCR_ID_HOME);
        }
    }
}
#endif

#ifdef CONFIG_LOG_UPLOAD
static void model_voice_on_log_upload_state_change(const log_upload_state_t *state, void *arg)
{
    struct home_nav_scr_data *scr_data = arg;
    int top_id;

    if (!state || !scr_data) {
        return;
    }

    if (state->state != LOG_UPLOAD_STATE_STARTING &&
        state->state != LOG_UPLOAD_STATE_UPLOADING) {
        return;
    }

    top_id = lisa_ui_nav_scr_get_top_id();
    if (top_id != LISA_UI_NAV_SCR_ID_HOME && top_id != LISA_UI_NAV_SCR_ID_LOG_UPLOAD) {
        lisa_ui_nav_scr_nav_to(LISA_UI_NAV_SCR_ID_HOME);
    }

    if (lisa_ui_nav_scr_get_top_id() != LISA_UI_NAV_SCR_ID_LOG_UPLOAD) {
        lisa_ui_nav_scr_nav_to(LISA_UI_NAV_SCR_ID_LOG_UPLOAD);
    }
}
#endif

static void model_sd_music_sync_on_state_change(const voice_msg_sd_music_sync_state_t *state,
                                                 void *arg)
{
    int top_id;

    (void)arg;

    if (!state) {
        return;
    }

    if (state->state == VOICE_MSG_SD_MUSIC_SYNC_STATE_FINISHED ||
        state->state == VOICE_MSG_SD_MUSIC_SYNC_STATE_IDLE) {
        if (lisa_ui_nav_scr_get_top_id() == LISA_UI_NAV_SCR_ID_SD_MUSIC_SYNC) {
            lisa_ui_nav_scr_nav_to(LISA_UI_NAV_SCR_ID_HOME);
        }
        return;
    }

    if (state->state != VOICE_MSG_SD_MUSIC_SYNC_STATE_START &&
        state->state != VOICE_MSG_SD_MUSIC_SYNC_STATE_UPLOADING &&
        state->state != VOICE_MSG_SD_MUSIC_SYNC_STATE_SUCCESSED &&
        state->state != VOICE_MSG_SD_MUSIC_SYNC_STATE_FAILED) {
        return;
    }

    top_id = lisa_ui_nav_scr_get_top_id();
    if (top_id != LISA_UI_NAV_SCR_ID_HOME &&
        top_id != LISA_UI_NAV_SCR_ID_SD_MUSIC_SYNC) {
        lisa_ui_nav_scr_nav_to(LISA_UI_NAV_SCR_ID_HOME);
    }

    if (lisa_ui_nav_scr_get_top_id() != LISA_UI_NAV_SCR_ID_SD_MUSIC_SYNC) {
        lisa_ui_nav_scr_nav_to(LISA_UI_NAV_SCR_ID_SD_MUSIC_SYNC);
    }
}

static void model_sd_music_on_card_removed(void *arg)
{
    (void)arg;

    lisa_ui_toast_show("设备已弹出TF卡");
}

static void model_sd_music_on_play_failed(void *arg)
{
    (void)arg;

    lisa_ui_toast_show("TF卡音频播放失败");
}

static void anim_timer_callback(lv_timer_t *timer)
{
    struct home_nav_scr_data *d = timer->user_data;
    lv_timer_pause(timer);

    if (d->mcp_emoji_pending) {
        LISA_UI_LOGI("play pending mcp emoji: %s", d->mcp_emoji_pending_name);
        d->mcp_emoji_pending = 0;
        d->mcp_emoji_start_tick = lv_tick_get();
        show_emoji_anim(d, d->mcp_emoji_pending_name, 0);
        lv_timer_reset(timer);
        lv_timer_resume(timer);
    } else {
        home_restore_session_emoji(d, 0);
        d->mcp_emoji_running = false;
    }
}

static void network_status_timer_callback(lv_timer_t *timer)
{
    struct home_nav_scr_data *d = timer->user_data;

    home_update_network_icon(d);
    home_update_alarm_icon(d);
}

static void model_voice_on_image_rec_failed(void *arg)
{
    struct home_nav_scr_data *scr_data = arg;

    if (!scr_data || !scr_data->view || !scr_data->img_rec_in_progress) {
        return;
    }

    LISA_UI_LOGW("voice photo: cloud recognition finished without result");
    scr_data->camera_preview_result_pending = 0;
    home_camera_preview_uploading_clear(scr_data);
    voice_msg_pub(VOICE_MSG_APP_CAMERA_PREVIEW_EXIT, NULL, 0);
    camera_preview_hide(scr_data);
    lisa_ui_toast_show(_("recognition error"));
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

    if (home_cancel_voice_photo_preview_by_button(scr_data)) {
        return;
    }

    if (model_camera_preview_is_active(&scr_data->camera_preview) ||
        scr_data->work_type == WORK_TYPE_CAMERA) {
        LISA_UI_LOGI("photo preview active, ignore image recognition");
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
        camera_preview_hide(scr_data);
        return;
    }

    image_size = (uint32_t)width * (uint32_t)height * 2U;
    if (image_size == 0 || scr_data->cap_buf_size < image_size) {
        LISA_UI_LOGE("Invalid image buffer size: cap=%u, need=%u", scr_data->cap_buf_size, image_size);
        lisa_ui_toast_show(_("recognition error"));
        camera_preview_hide(scr_data);
        return;
    }

    image_width = width;
    image_height = height;

    image_width = height;
    image_height = width;
    image_size = (uint32_t)image_width * (uint32_t)image_height * 2U;
    
    /* Ensure the captured image is displayed before recognition */
    scr_data->img.header.cf = LV_IMG_CF_TRUE_COLOR;
    scr_data->img.header.always_zero = 0;
    scr_data->img.header.reserved = 0;
    scr_data->img.data = scr_data->cap_buf;
    scr_data->img.data_size = image_size;
    scr_data->img.header.w = image_width;
    scr_data->img.header.h = image_height;
    lisa_ui_llm_primary_set_content_text(scr_data->view, "");
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
    lv_obj_t *content_label = NULL;
    const char *content_text = NULL;

    if (!scr_data || !scr_data->view) {
        return;
    }

    scr_data->speaking = 1;
    scr_data->finished = 0;
    if (!model_voice_music_is_playing()) {
        scr_data->music_text_active = 0;
    }
    home_stop_standby_sleep_timer(scr_data);
    home_restore_standby_brightness(scr_data);

    content_label = lisa_ui_llm_primary_content_label_get(scr_data->view);
    if (content_label) {
        content_text = lv_textarea_get_text(content_label);
    }

    if (scr_data && model_camera_preview_is_result_active(&scr_data->camera_preview)) {
        if (!model_camera_preview_is_result_tts_ready(&scr_data->camera_preview)) {
            LISA_UI_LOGI("voice photo: non-result TTS playing, keep uploading UI");
            standby_text_timer_update(scr_data);
            return;
        }

        model_camera_preview_mark_result_tts_started(&scr_data->camera_preview);
        model_camera_preview_publish_state(&scr_data->camera_preview);
        home_camera_preview_uploading_clear(scr_data);
        home_restore_session_emoji(scr_data, 1);
        LISA_UI_LOGI("voice photo: result TTS started, show speaking status");
        lisa_ui_llm_primary_set_status_text(scr_data->view, _("speaking"));
        standby_text_timer_update(scr_data);
        return;
    }

    if (content_text && strcmp(content_text, "\n正在上传照片...") == 0) {
        lisa_ui_llm_primary_set_content_text(scr_data->view, "");
        lv_obj_set_style_translate_y(content_label, 0, LV_PART_MAIN);
    }

    home_restore_session_emoji(scr_data, 1);
    lisa_ui_llm_primary_set_status_text(scr_data->view, _("speaking"));

    standby_text_timer_update(scr_data);
}

static void model_voice_on_tts_player_stoped(void *arg)
{
    struct home_nav_scr_data *scr_data = arg;

    scr_data->speaking = 0;

    /* TTS 停止时同步停止字幕滚动定时器 */
    tts_text_stop_timer(scr_data);

    if (scr_data && (model_camera_preview_is_result_active(&scr_data->camera_preview) ||
                     scr_data->camera_preview_result_pending)) {
        /* 在以下两种情况隐藏照片：
         * 1. result TTS URL 已单独到达并播放完毕 (is_result_tts_started)
         * 2. PREVIEW_EXIT 后 session 重播的 TTS 也已播完 (camera_preview_result_pending) */
        if (model_camera_preview_is_result_tts_started(&scr_data->camera_preview) ||
            scr_data->camera_preview_result_pending) {
            LISA_UI_LOGI("voice photo: result TTS finished, hide image");
            scr_data->camera_preview_result_pending = 0;
            home_camera_preview_uploading_clear(scr_data);
            camera_preview_hide(scr_data);
            if (model_voice_cloud_is_running()) {
                home_restore_session_emoji(scr_data, 0);
                lisa_ui_llm_primary_set_status_text(scr_data->view, _("listening"));
                home_restore_last_iat_or_prompt(scr_data);
                standby_text_timer_update(scr_data);
            } else {
                const char *init_status =
                    model_voice_cloud_is_connected() ? _("Please wake me") : _("service disconnected");
                home_enter_standby_state(scr_data, init_status, 1);
            }
        } else {
            /* TTS 因抢占等原因停止，但 result TTS 尚未开始（等待照片描述），
             * 保留照片可见，清理字幕缓冲区 */
            LISA_UI_LOGI("voice photo: TTS stopped before result, keep image");
            scr_data->tts_text_len = 0;
            scr_data->tts_text_displayed = 0;
            scr_data->tts_text_stream_done = 0;
        }
        return;
    }

    if (scr_data && model_camera_preview_keep_preview_alive(&scr_data->camera_preview)) {
        LISA_UI_LOGI("tts stopped while photo preview is active, keep preview");
        scr_data->standby_after_tts_pending = 0;
        return;
    }

    /* 会话结束时若 TTS 仍在播放，on_finished 会请求播完后再进入待机。
     * 再次唤醒会让旧 TTS 的 STOPPED 先于新 SESSION_STARTING 到达；这里
     * 复用现有延迟复检，避免先显示一帧“请唤醒我”再切回“我在听”。 */
    if (home_try_enter_standby_after_tts(scr_data)) {
        return;
    }

    if (scr_data->img_rec_is_button) {
        if (model_voice_cloud_is_running()) {
            voice_cloud_chat_stop();
        }

        camera_preview_hide(scr_data);
        scr_data->img_rec_is_button = 0;

        if (!model_voice_cloud_is_running()) {
            const char *init_status =
                model_voice_cloud_is_connected() ? _("Please wake me") : _("service disconnected");
            home_enter_standby_state(scr_data, init_status, 0);
        } else {
            standby_text_timer_update(scr_data);
            home_try_enter_standby_after_tts(scr_data);
        }
        return;
    }

    if (scr_data->work_type == WORK_TYPE_IMG_REC) {
        if (home_should_keep_recognition_image(scr_data)) {
            lisa_ui_llm_primary_set_content_text(scr_data->view, "");
        } else {
            camera_preview_hide(scr_data);
        }

        /* In full duplex mode, set status to listening if session is still running */
        if (model_voice_cloud_is_running()) {
            home_restore_session_emoji(scr_data, 0);
            lisa_ui_llm_primary_set_status_text(scr_data->view, _("listening"));
        } else {
            const char *init_status =
                model_voice_cloud_is_connected() ? _("Please wake me") : _("service disconnected");
            home_enter_standby_state(scr_data, init_status, 0);
        }
    } else {
        if (model_voice_cloud_is_running()) {
            if (scr_data->mcp_emoji_running) {
                scr_data->mcp_emoji_running = 0;
            }
            home_restore_session_emoji(scr_data, 1);
            lisa_ui_llm_primary_set_status_text(scr_data->view, _("listening"));
        } else {
            const char *init_status =
                model_voice_cloud_is_connected() ? _("Please wake me") : _("service disconnected");
            home_enter_standby_state(scr_data, init_status, 0);
        }
    }

    if (!home_try_enter_standby_after_tts(scr_data) && model_voice_cloud_is_running()) {
        standby_text_timer_update(scr_data);
    }
}

static void model_voice_on_emoji(void *arg, const char *name)
{
    struct home_nav_scr_data *scr_data = arg;
    bool is_wifi_emoji = false;

    if (!scr_data) {
        return;
    }

#ifdef CONFIG_BOARD_ARCS_MINI_DOLL_V2
    is_wifi_emoji = name && strcmp(name, EMOJI_NAME_WIFI) == 0;
#endif

    if (scr_data->mcp_emoji_running && !is_wifi_emoji) {
        LISA_UI_LOGI("ignore emoji display, mcp emoji is running");
        return;
    }

    if (scr_data->finished && !is_wifi_emoji) {
        return;
    }

    LISA_UI_LOGI("on emoji, name: %s", name);

#ifdef CONFIG_BOARD_ARCS_MINI_DOLL_V2
    if (is_wifi_emoji) {
        home_handle_activity(scr_data);
        if (lisa_ui_nav_scr_get_top_id() != LISA_UI_NAV_SCR_ID_HOME) {
            lisa_ui_nav_scr_nav_to(LISA_UI_NAV_SCR_ID_HOME);
        }
    }
#endif

#ifdef CONFIG_BOARD_ARCS_MINI_DOLL_V2
    lv_timer_set_period(scr_data->anim_timer, is_wifi_emoji ? 30000 : 5000);
#endif
    if (show_emoji_anim(scr_data, name, 0) == 0) {
        home_anim_timer_reset_resume(scr_data);
    }
}

static void model_voice_on_oneshot_emoji(void *arg, const char *name)
{
    struct home_nav_scr_data *scr_data = arg;

    if (!scr_data || !scr_data->view || !name || name[0] == '\0') {
        return;
    }

    LISA_UI_LOGI("on oneshot emoji, name: %s", name);
    show_oneshot_emoji_once(scr_data, name);
}

static void model_voice_on_mcp_emoji(void *arg, const char *name)
{
    struct home_nav_scr_data *scr_data = arg;

    if (!scr_data || !scr_data->view) {
        return;
    }

    if (home_should_ignore_mcp_emoji(name)) {
        LISA_UI_LOGW("ignore mcp emoji, name: %s", name ? name : "<null>");
        return;
    }

    if (scr_data->finished) {
        return;
    }

    if (scr_data->mcp_emoji_running) {
        uint32_t elapsed = lv_tick_get() - scr_data->mcp_emoji_start_tick;
        if (elapsed < MCP_EMOJI_MIN_LOOP_MS) {
            LISA_UI_LOGI("defer mcp emoji '%s', current mcp emoji only %lums elapsed (< %ums)",
                         name, (unsigned long)elapsed, (unsigned int)MCP_EMOJI_MIN_LOOP_MS);
            scr_data->mcp_emoji_pending = 1;
            home_store_emoji_name(scr_data->mcp_emoji_pending_name,
                                  sizeof(scr_data->mcp_emoji_pending_name), name);
            return;
        }
    }

    LISA_UI_LOGI("on emoji, name: %s", name);
    if (show_emoji_anim(scr_data, name, 0) != 0) {
        scr_data->mcp_emoji_running = false;
        return;
    }

    scr_data->mcp_emoji_running = true;
    scr_data->mcp_emoji_start_tick = lv_tick_get();
    scr_data->mcp_emoji_pending = 0;
    home_anim_timer_reset_resume(scr_data);
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

        home_anim_timer_pause(scr_data);
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
    home_enter_standby_state(scr_data, _("service disconnected"), 0);
}

static void model_voice_on_start(void *arg)
{
    struct home_nav_scr_data *scr_data = arg;

    if (scr_data && model_camera_preview_keep_preview_alive(&scr_data->camera_preview)) {
        LISA_UI_LOGI("cancel photo preview because voice start arrived");
    }

    if (scr_data && model_camera_preview_is_result_tts_ready(&scr_data->camera_preview)) {
        LISA_UI_LOGI("ignore internal voice start while photo result TTS is active");
        return;
    }

    home_handle_activity(scr_data);
    camera_preview_hide(scr_data);
    home_restore_session_emoji(scr_data, 1);

    lisa_ui_llm_primary_set_status_text(scr_data->view, _("listening"));

    lisa_ui_llm_primary_set_content_text(scr_data->view, _("Please speak"));

    /* 新会话已经接管 UI，旧会话遗留的延迟待机请求必须失效。 */
    scr_data->standby_after_tts_pending = 0;
    scr_data->finished = 0;
    scr_data->work_type = WORK_TYPE_VOICE;

    standby_text_timer_update(scr_data);
}

static void enter_standby(struct home_nav_scr_data *scr_data)
{
    home_enter_standby_state(scr_data, _("Please wake me"), 0);
}

static void home_enter_standby_state(struct home_nav_scr_data *scr_data, const char *status_text, uint8_t imm)
{
    bool keep_image;
    const char *music_text;

    if (!scr_data || !scr_data->view) {
        return;
    }

    LISA_UI_LOGI("enter_standby");

    keep_image = home_should_keep_recognition_image(scr_data);
    music_text = model_voice_music_text_get();

    scr_data->standby_after_tts_pending = 0;
    scr_data->finished = 1;
    tts_text_stop_timer(scr_data);
    scr_data->tts_text_len = 0;
    scr_data->tts_text_displayed = 0;
    scr_data->tts_text_stream_done = 0;

    if (!status_text || status_text[0] == '\0') {
        status_text =
            model_voice_cloud_is_connected() ? _("Please wake me") : _("service disconnected");
    }

    /* Clear transient camera state before switching back to standby UI. */
    if (model_camera_preview_is_active(&scr_data->camera_preview) ||
        (camera_preview_is_camera_work_type(scr_data->work_type) && !keep_image)) {
        camera_preview_hide(scr_data);
    }

    if (keep_image) {
        lisa_ui_llm_primary_set_content_text(scr_data->view, "");
    } else if (music_text) {
        /* 对话字幕会覆盖歌曲名；返回待机/主页时直接恢复缓存的歌曲名，
         * 不必等待播放器再次上报 PLAYING。 */
        lisa_ui_llm_primary_set_content_text(scr_data->view, music_text);
    } else {
        lisa_ui_llm_primary_set_content_text(scr_data->view, model_voice_role_propmt_get());
    }
    home_anim_timer_pause(scr_data);
    show_emoji_anim(scr_data, EMOJI_NAME_NEUTRAL, imm);
    lisa_ui_llm_primary_set_status_text(scr_data->view, status_text);
    standby_text_timer_update(scr_data);
    home_schedule_standby_sleep(scr_data);
}

static bool home_try_enter_standby_after_tts(struct home_nav_scr_data *scr_data)
{
    uint8_t mode;

    if (!scr_data || !scr_data->standby_after_tts_pending) {
        return false;
    }

    if (scr_data && (model_camera_preview_keep_preview_alive(&scr_data->camera_preview) ||
                     scr_data->camera_preview_result_pending)) {
        LISA_UI_LOGI("skip standby after tts while photo is visible");
        scr_data->standby_after_tts_pending = 0;
        return false;
    }

    if (scr_data->speaking || model_voice_cloud_is_running()) {
        return false;
    }

    scr_data->standby_after_tts_pending = 0;

    if (!model_voice_cloud_is_connected()) {
        home_enter_standby_state(scr_data, _("service disconnected"), 0);
        return true;
    }

    mode = model_voice_wakeup_mode_get();
    if (mode) {
        LISA_UI_LOGI("tts stopped after finish, check speaking status after 1000ms");
        lv_timer_create(finished_tiemr_callback, 1000, scr_data);
    } else {
        enter_standby(scr_data);
    }

    return true;
}

static void finished_tiemr_callback(lv_timer_t *timer)
{
    struct home_nav_scr_data *scr_data = timer->user_data;

    LISA_UI_LOGI("finished_tiemr_callback, speaking: %d", scr_data->speaking);

    if (scr_data && model_camera_preview_keep_preview_alive(&scr_data->camera_preview)) {
        LISA_UI_LOGI("finished timer ignored while photo preview is active");
        lv_timer_del(timer);
        return;
    }

    if (!scr_data->speaking && !model_voice_cloud_is_running()) {
        enter_standby(scr_data);
    }

    lv_timer_del(timer);
}

static void model_voice_on_finished(void *arg)
{
    struct home_nav_scr_data *scr_data = arg;
    uint8_t mode = model_voice_wakeup_mode_get();

    LISA_UI_LOGI("on finished, speaking: %d, wakeup mode: %d", scr_data->speaking, mode);

    // 如果在闹钟响铃页面，不进入待机
    if (lisa_ui_nav_scr_get_top_id() == LISA_UI_NAV_SCR_ID_ALARM_RING) {
        LISA_UI_LOGI("on finished, but on alarm ring page, skip enter_standby");
        return;
    }

    if (scr_data && model_camera_preview_keep_preview_alive(&scr_data->camera_preview)) {
        LISA_UI_LOGI("on finished while photo preview is active, keep current state");
        scr_data->standby_after_tts_pending = 0;
        return;
    }

    if (scr_data && model_camera_preview_is_result_active(&scr_data->camera_preview)) {
        LISA_UI_LOGI("on finished while voice photo result is active, keep current state");
        scr_data->standby_after_tts_pending = 0;
        return;
    }

    if (scr_data->speaking) {
        scr_data->standby_after_tts_pending = 1;
        LISA_UI_LOGI("on finished while speaking, defer standby until tts stops");
        return;
    }

    if (mode && model_voice_tts_is_pending()) {
        LISA_UI_LOGI("on finished ignored because tts is pending");
        return;
    }

    if (mode) {
        home_anim_timer_pause(scr_data);
        show_emoji_anim(scr_data, EMOJI_NAME_NEUTRAL, 0);
        LISA_UI_LOGI("on finished, single mode, check speaking status after 1000ms");
        lv_timer_create(finished_tiemr_callback, 1000, scr_data);
        return;
    }

    enter_standby(scr_data);
}

#define TTS_TEXT_MAX_CHARS 12

#ifndef CONFIG_TTS_TEXT_MS_PER_CHAR
#define CONFIG_TTS_TEXT_MS_PER_CHAR 250
#endif
#define TTS_TEXT_MS_PER_CHAR_DEFAULT CONFIG_TTS_TEXT_MS_PER_CHAR

static uint32_t home_get_tts_text_ms_per_char(void)
{
	uint32_t ms = TTS_TEXT_MS_PER_CHAR_DEFAULT;

#ifdef LISA_UI_PLATFORM_ARCS
	int kv_ms = 0;

	if (lisa_kv_get_int(KV_KEY_USER_TTS_TEXT_MS_PER_CHAR, &kv_ms) != 0) {
		lisa_kv_set_int(KV_KEY_USER_TTS_TEXT_MS_PER_CHAR, (int)ms);
		return ms;
	}

	if (kv_ms > 0) {
		return (uint32_t)kv_ms;
	}
#endif

	return ms;
}

static int utf8_char_len(char c)
{
	unsigned char uc = (unsigned char)c;
	if (uc < 0x80) return 1;
	if ((uc & 0xE0) == 0xC0) return 2;
	if ((uc & 0xF0) == 0xE0) return 3;
	if ((uc & 0xF8) == 0xF0) return 4;
	return 1;
}

static bool is_sentence_end_punct(const char *s)
{
	if (!s || !*s) return false;
	unsigned char c = (unsigned char)s[0];
	if (c == '\n') return true;
	if (c == '!' || c == '?') return true;
	if (c == 0xE3 && (unsigned char)s[1] == 0x80 && (unsigned char)s[2] == 0x82) return true; /* 。*/
	if (c == 0xEF && (unsigned char)s[1] == 0xBC && (unsigned char)s[2] == 0x81) return true; /* ！*/
	if (c == 0xEF && (unsigned char)s[1] == 0xBC && (unsigned char)s[2] == 0x9F) return true; /* ？*/
	return false;
}

static bool is_clause_punct(const char *s)
{
	if (!s) return false;
	unsigned char c = (unsigned char)s[0];
	if (c == 0xEF && (unsigned char)s[1] == 0xBC && (unsigned char)s[2] == 0x8C) return true; /* ，*/
	if (c == 0xE3 && (unsigned char)s[1] == 0x80 && (unsigned char)s[2] == 0x81) return true; /* 、*/
	if (c == 0xEF && (unsigned char)s[1] == 0xBC && (unsigned char)s[2] == 0x9B) return true; /* ；*/
	if (c == 0xEF && (unsigned char)s[1] == 0xBC && (unsigned char)s[2] == 0x9A) return true; /* ：*/
	return false;
}

static int utf8_char_count(const char *s, int byte_len)
{
	int count = 0;
	int i = 0;
	while (i < byte_len) {
		int clen = utf8_char_len(s[i]);
		if (i + clen > byte_len) break;
		i += clen;
		count++;
	}
	return count;
}

static bool is_formatting_char(const char *s)
{
	if (!s) return false;
	unsigned char c = (unsigned char)s[0];
	if (c == '*' || c == '-') return true;
	if (c == 0xE2 && (unsigned char)s[1] == 0x80) {
		unsigned char c2 = (unsigned char)s[2];
		if (c2 == 0x94) return true; /* — */
		if (c2 == 0xA6) return true; /* … */
	}
	if (c == 0xE3 && (unsigned char)s[1] == 0x80) {
		unsigned char c2 = (unsigned char)s[2];
		if (c2 == 0x8A || c2 == 0x8B) return true; /* 《 》 */
		if (c2 == 0x90 || c2 == 0x91) return true; /* 【 】 */
	}
	if (c == 0xEF && (unsigned char)s[1] == 0xBD && (unsigned char)s[2] == 0x9E) return true; /* ～ */
	return false;
}

static int spoken_char_count(const char *s, int byte_len)
{
	int count = 0;
	int i = 0;
	int in_fmt = 0;

	while (i < byte_len) {
		int clen = utf8_char_len(s[i]);
		if (i + clen > byte_len) break;

		if (is_formatting_char(s + i)) {
			if (!in_fmt) {
				count++;
				in_fmt = 1;
			}
		} else {
			count++;
			in_fmt = 0;
		}

		i += clen;
	}

	return count;
}

static int tts_text_find_boundary(const char *buf, int len)
{
	int first_clause_end = -1;
	int char_count = 0;
	int i = 0;

	while (i < len) {
		int clen = utf8_char_len(buf[i]);
		if (i + clen > len) break;

		if (is_sentence_end_punct(buf + i)) {
			if (char_count >= TTS_TEXT_MAX_CHARS && first_clause_end > 0) {
				return first_clause_end;
			}
			return i + clen;
		}
		if (first_clause_end < 0 && is_clause_punct(buf + i)) {
			first_clause_end = i + clen;
		}

		char_count++;
		i += clen;
	}

	if (char_count >= TTS_TEXT_MAX_CHARS && first_clause_end > 0) {
		return first_clause_end;
	}

	if (char_count >= TTS_TEXT_MAX_CHARS + 4) {
		i = 0;
		int count = 0;
		while (i < len && count < TTS_TEXT_MAX_CHARS) {
			int clen = utf8_char_len(buf[i]);
			if (i + clen > len) break;
			i += clen;
			count++;
		}
		return i;
	}

	return -1;
}

static void tts_text_stop_timer(struct home_nav_scr_data *scr_data)
{
	if (scr_data->tts_text_timer) {
		lv_timer_del(scr_data->tts_text_timer);
		scr_data->tts_text_timer = NULL;
	}
}

static void tts_text_timer_cb(lv_timer_t *timer);

static void tts_text_try_show_next(struct home_nav_scr_data *scr_data)
{
	if (scr_data->finished) return;

	int start = scr_data->tts_text_displayed;
	int avail = scr_data->tts_text_len - start;

	while (avail > 0 && scr_data->tts_text_buf[start] == '\n') {
		start++;
		avail--;
	}

	if (avail <= 0) {
		scr_data->tts_text_displayed = scr_data->tts_text_len;
		return;
	}

	int boundary = tts_text_find_boundary(scr_data->tts_text_buf + start, avail);
	int seg_len;

	if (boundary > 0) {
		seg_len = boundary;
	} else if (scr_data->tts_text_stream_done) {
		seg_len = avail;
	} else {
		return;
	}

	while (seg_len > 0 && scr_data->tts_text_buf[start + seg_len - 1] == '\n') {
		seg_len--;
	}
	if (seg_len <= 0) return;

	char save = scr_data->tts_text_buf[start + seg_len];
	scr_data->tts_text_buf[start + seg_len] = '\0';
	lisa_ui_llm_primary_set_content_text(scr_data->view, scr_data->tts_text_buf + start);
	scr_data->tts_text_buf[start + seg_len] = save;

	int char_cnt = spoken_char_count(scr_data->tts_text_buf + start, seg_len);
	uint32_t duration = char_cnt * home_get_tts_text_ms_per_char();
	if (duration < 1000) duration = 1000;

	scr_data->tts_text_displayed = start + seg_len;

	scr_data->tts_text_timer = lv_timer_create(tts_text_timer_cb, duration, scr_data);
	if (scr_data->tts_text_timer) {
		lv_timer_set_repeat_count(scr_data->tts_text_timer, 1);
	}
}

static void tts_text_timer_cb(lv_timer_t *timer)
{
	struct home_nav_scr_data *scr_data = timer->user_data;

	scr_data->tts_text_timer = NULL;
	tts_text_try_show_next(scr_data);
}

static void model_voice_on_tts_text_start(void *arg)
{
	struct home_nav_scr_data *scr_data = arg;

	if (!scr_data || scr_data->finished) {
		return;
	}

	if (model_camera_preview_is_result_tts_ready(&scr_data->camera_preview)) {
		LISA_UI_LOGI("voice photo: TTS text started");
		tts_text_stop_timer(scr_data);
		home_camera_preview_uploading_clear(scr_data);
		/* 将字幕下移以免遮挡照片，与 uploading 文本使用相同的 6px 偏移 */
		{
			lv_obj_t *label = lisa_ui_llm_primary_content_label_get(scr_data->view);
			if (label) {
				lv_obj_set_style_translate_y(label, 6, LV_PART_MAIN);
			}
		}
		lisa_ui_llm_primary_set_status_text(scr_data->view, _("speaking"));
		/* 用 \n 前缀推开首行，后续 update chunk 由定时器逐段显示 */
		scr_data->tts_text_buf[0] = '\n';
		scr_data->tts_text_len = 1;
		scr_data->tts_text_displayed = 0;
		scr_data->tts_text_stream_done = 0;
		return;
	}

	tts_text_stop_timer(scr_data);
	scr_data->tts_text_len = 0;
	scr_data->tts_text_displayed = 0;
	scr_data->tts_text_stream_done = 0;
}

static void model_voice_on_tts_text_end(void *arg)
{
	struct home_nav_scr_data *scr_data = arg;

	if (!scr_data) {
		return;
	}

	scr_data->tts_text_stream_done = 1;

	if (scr_data->tts_text_timer == NULL) {
		tts_text_try_show_next(scr_data);
	}
}

static void model_voice_on_tts_text_update(const char *text, void *arg)
{
	struct home_nav_scr_data *scr_data = arg;

	if (!scr_data || scr_data->finished || !text || text[0] == '\0') {
		return;
	}

	int text_len = strlen(text);
	while (text_len > 0 && *text == '\n') {
		text++;
		text_len--;
	}
	while (text_len > 0 && text[text_len - 1] == '\n') {
		text_len--;
	}
	if (text_len == 0) return;

	int remaining = (int)sizeof(scr_data->tts_text_buf) - scr_data->tts_text_len - 1;
	if (remaining <= 0) return;

	if (text_len > remaining) {
		text_len = remaining;
	}

	memcpy(scr_data->tts_text_buf + scr_data->tts_text_len, text, text_len);
	scr_data->tts_text_len += text_len;

	if (scr_data->tts_text_timer == NULL) {
		tts_text_try_show_next(scr_data);
	}
}

static void model_voice_on_iat_text_start(void *arg)
{
    struct home_nav_scr_data *scr_data = arg;

    if (scr_data->finished) {
        return;
    }

    if (scr_data && model_camera_preview_keep_preview_alive(&scr_data->camera_preview)) {
        LISA_UI_LOGI("ignore iat start while photo preview is active");
        return;
    }

    home_restore_session_emoji(scr_data, 1);
    lisa_ui_llm_primary_set_status_text(scr_data->view, _("listening"));
}

static void model_voice_on_iat_text_end(void *arg)
{
    struct home_nav_scr_data *scr_data = arg;

    if (scr_data->finished) {
        return;
    }

    if (scr_data && model_camera_preview_keep_preview_alive(&scr_data->camera_preview)) {
        LISA_UI_LOGI("ignore iat end while photo preview is active");
        return;
    }

    if (!scr_data->speaking) {
        home_restore_session_emoji(scr_data, 1);
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

    if (scr_data && model_camera_preview_keep_preview_alive(&scr_data->camera_preview)) {
        LISA_UI_LOGI("ignore iat update while photo preview is active");
        return;
    }

    if (camera_preview_is_camera_work_type(scr_data->work_type)) {
        camera_preview_hide(scr_data);
    }

    lisa_ui_llm_primary_set_content_text(scr_data->view, text);
    home_restore_session_emoji(scr_data, 1);
    lisa_ui_llm_primary_set_status_text(scr_data->view, _("listening"));
}

static void model_voice_on_standby_text_update(void *arg, const char *text, bool is_cloud_text)
{
    struct home_nav_scr_data *scr_data = arg;

    if (!scr_data || !scr_data->view) {
        return;
    }

    if (scr_data->img_rec_running || model_camera_preview_is_active(&scr_data->camera_preview) ||
        camera_preview_is_camera_work_type(scr_data->work_type)) {
        return;
    }

    if (!is_cloud_text) {
        if (!text || text[0] == '\0') {
            scr_data->music_text_active = 0;
            standby_text_timer_update(scr_data);
            return;
        }
        scr_data->music_text_active = 1;
        standby_text_timer_update(scr_data);
        lisa_ui_llm_primary_set_content_text(scr_data->view, text);
        return;
    }

    if (!text) {
        return;
    }

    lisa_ui_llm_primary_set_content_text(scr_data->view, text);
}

static void model_voice_on_wakeup_mode_changed(void *arg, model_voice_wakeup_mode_t mode)
{
    struct home_nav_scr_data *scr_data = arg;
    (void)mode;

    if (!scr_data || !scr_data->view) {
        return;
    }

    home_update_full_duplex_icon(scr_data);
}

#ifndef CONFIG_BOARD_ARCS_MINI
static void settings_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    struct home_nav_scr_data *scr_data = (struct home_nav_scr_data *)lv_event_get_user_data(e);

    if (code == LV_EVENT_CLICKED) {
        home_handle_activity(scr_data);
        lisa_ui_nav_scr_nav_to(LISA_UI_NAV_SCR_ID_SETTING);
    }
}
#endif

static int home_play_emoji_anim(struct home_nav_scr_data *d, const char *emoji_name, uint8_t imm)
{
    const lisa_ui_anim_ext_config_t *anim_config = emoji_anim_get_by_name(emoji_name);
    lisa_ui_anim_heap_check_t heap_check;
    int offset_x;
    int offset_y;
    lv_obj_t *anim;

    if (!d || !d->view || !emoji_name || emoji_name[0] == '\0' || anim_config == NULL) {
        return -1;
    }

    anim = lisa_ui_llm_primary_emoji_anim_get(d->view);
    if (anim == NULL) {
        return -1;
    }

    if (!lisa_ui_anim_ext_config_has_enough_heap(anim_config, &heap_check)) {
        LISA_UI_LOGW("Skip emoji [%s]: invalid resource or not enough LVGL heap, need_single=%u, largest=%u, "
                     "need_free=%u, free=%u",
                     emoji_name, (unsigned)heap_check.need_single, (unsigned)heap_check.largest_free,
                     (unsigned)heap_check.need_free, (unsigned)heap_check.free_size);
        home_stop_emoji_display(d);
        return -1;
    }

    // 获取并设置表情横向和纵向偏移值
    offset_x = emoji_anim_get_offset_x(emoji_name);
    offset_y = emoji_anim_get_offset_y(emoji_name);
    lisa_ui_llm_primary_set_emoji_offset(d->view, offset_x, offset_y);

    home_update_emoji_visibility_for_image(d, anim);

    if (imm) {
        lisa_ui_anim_ext_next_imm(anim, anim_config);
    } else {
        lisa_ui_anim_ext_next(anim, anim_config);
    }

    return 0;
}

static void home_stop_emoji_display(struct home_nav_scr_data *d)
{
    if (!d || !d->view) {
        return;
    }

    lv_obj_t *anim = lisa_ui_llm_primary_emoji_anim_get(d->view);
    if (anim != NULL) {
        lisa_ui_anim_ext_exit(anim);
        lv_obj_add_flag(anim, LV_OBJ_FLAG_HIDDEN);
    }

    home_stop_oneshot_emoji(d);
    d->mcp_emoji_running = 0;
    d->current_emoji_name[0] = '\0';
}

static int show_emoji_anim(struct home_nav_scr_data *d, const char *emoji_name, uint8_t imm)
{
    if (!d) {
        return -1;
    }

    if (!d->oneshot_emoji_running && home_is_same_emoji_anim(d, emoji_name)) {
        return 0;
    }

    if (d->oneshot_emoji_running) {
        home_stop_oneshot_emoji(d);
    }

    if (home_play_emoji_anim(d, emoji_name, imm) != 0) {
        return -1;
    }

    home_store_emoji_name(d->current_emoji_name, sizeof(d->current_emoji_name), emoji_name);
    return 0;
}

static void show_oneshot_emoji_once(struct home_nav_scr_data *d, const char *emoji_name)
{
    const lisa_ui_anim_ext_config_t *anim_config = emoji_anim_get_by_name(emoji_name);
    lisa_ui_anim_ext_config_t one_shot_anim;
    lisa_ui_anim_heap_check_t heap_check;
    const char *restore_emoji_name;
    lv_obj_t *anim;

    if (!d || !d->view || !emoji_name || emoji_name[0] == '\0' || anim_config == NULL) {
        return;
    }

    if (d->oneshot_emoji_running && d->oneshot_restore_emoji_name[0] != '\0') {
        restore_emoji_name = d->oneshot_restore_emoji_name;
    } else if (d->current_emoji_name[0] != '\0') {
        restore_emoji_name = d->current_emoji_name;
    } else {
        restore_emoji_name = EMOJI_NAME_NEUTRAL;
    }

    one_shot_anim = *anim_config;
    if (one_shot_anim.loop.frame_count > 0 && one_shot_anim.loop.loop < 0) {
        one_shot_anim.loop.loop = 0;
    }

    if (!lisa_ui_anim_ext_config_has_enough_heap(&one_shot_anim, &heap_check)) {
        LISA_UI_LOGW("Skip oneshot emoji [%s]: invalid resource or not enough LVGL heap, need_single=%u, "
                     "largest=%u, need_free=%u, free=%u",
                     emoji_name, (unsigned)heap_check.need_single, (unsigned)heap_check.largest_free,
                     (unsigned)heap_check.need_free, (unsigned)heap_check.free_size);
        home_stop_emoji_display(d);
        return;
    }

    home_stop_oneshot_emoji(d);
    home_store_emoji_name(d->oneshot_restore_emoji_name, sizeof(d->oneshot_restore_emoji_name), restore_emoji_name);

    anim = lisa_ui_llm_primary_emoji_anim_get(d->view);
    if (anim == NULL) {
        return;
    }

    d->oneshot_emoji_running = 1;
    home_update_emoji_visibility_for_image(d, anim);
    lisa_ui_anim_ext_next_imm(anim, &one_shot_anim);

    d->oneshot_emoji_timer = lv_timer_create(oneshot_emoji_timer_cb, 50, d);
}

static void oneshot_emoji_timer_cb(lv_timer_t *timer)
{
    struct home_nav_scr_data *scr_data = timer ? timer->user_data : NULL;
    lv_obj_t *anim;

    if (!scr_data) {
        return;
    }

    if (!scr_data->view) {
        home_stop_oneshot_emoji(scr_data);
        return;
    }

    anim = lisa_ui_llm_primary_emoji_anim_get(scr_data->view);
    if (lisa_ui_anim_is_playing(anim)) {
        return;
    }

    if (scr_data->oneshot_emoji_timer == timer) {
        scr_data->oneshot_emoji_timer = NULL;
    }
    scr_data->oneshot_emoji_running = 0;
    lv_timer_del(timer);

    if (scr_data->oneshot_restore_emoji_name[0] != '\0') {
        home_play_emoji_anim(scr_data, scr_data->oneshot_restore_emoji_name, 1);
    }
}

void home_reset(struct home_nav_scr_data *scr_data)
{
    const char *init_status =
        model_voice_cloud_is_connected() ? _("Please wake me") : _("service disconnected");

    home_handle_activity(scr_data);
    home_stop_oneshot_emoji(scr_data);
    scr_data->oneshot_restore_emoji_name[0] = '\0';
    camera_preview_hide(scr_data);
    if (model_voice_cloud_is_running()) {
        scr_data->finished = 0;
        scr_data->speaking = 0;
        home_restore_last_iat_or_prompt(scr_data);
        lisa_ui_llm_primary_set_status_text(scr_data->view, _("listening"));
    } else if (model_voice_tts_is_pending()) {
        scr_data->finished = 0;
        scr_data->speaking = 0;
    } else if (model_voice_tts_is_playing()) {
        scr_data->finished = 0;
        scr_data->speaking = 1;
        lisa_ui_llm_primary_set_status_text(scr_data->view, _("speaking"));
    } else {
        scr_data->speaking = 0;
        home_enter_standby_state(scr_data, init_status, 1);
    }
    home_update_full_duplex_icon(scr_data);
    home_update_alarm_icon(scr_data);
    home_update_battery_icon(scr_data);
    if (model_voice_cloud_is_running() || model_voice_tts_is_playing()) {
        home_restore_session_emoji(scr_data, true);
        home_anim_timer_pause(scr_data);
        standby_text_timer_update(scr_data);
    }
    home_update_network_icon(scr_data);
}

static int home_nav_scr_open(const struct lisa_ui_nav_scr *scr, void **data)
{
    LISA_UI_LOGD("nav scr open, id: %d", scr->unique_id);

    model_voice_init();
    model_camera_preview_evt_init();
    #if !CONFIG_LISA_MODEM
    int cam_init_ret = model_camera_init();
    if (cam_init_ret != 0) {
        LISA_UI_LOGW("Camera init failed on home open: %d", cam_init_ret);
    }
    #endif
    model_modem_init();
    model_wifi_init();

    struct home_nav_scr_data *scr_data = lisa_ui_malloc(sizeof(struct home_nav_scr_data));
    if (!scr_data) {
        LISA_UI_LOGE("failed to allocate memory for home_nav_scr_data");
        return -1;
    }
    memset(scr_data, 0, sizeof(struct home_nav_scr_data));
    scr_data->last_battery_status = MODEL_BATTERY_STATUS_UNKNOWN;
    model_camera_preview_init(&scr_data->camera_preview);

    scr_data->view = lisa_ui_llm_primary_create(lv_scr_act());
#ifndef CONFIG_BOARD_ARCS_MINI
    lisa_ui_llm_primary_set_settings_icon_event_cb(scr_data->view, settings_btn_event_cb, scr_data);
#endif

    scr_data->anim_timer = lv_timer_create(anim_timer_callback, 5000, scr_data);
    home_anim_timer_pause(scr_data);
    home_update_network_icon(scr_data);
    scr_data->network_status_timer = lv_timer_create(network_status_timer_callback, 1000, scr_data);
    scr_data->standby_text_timer = NULL;
    scr_data->standby_sleep_restore_brightness = model_common_brightness_get();

    model_battery_cb_register(model_battery_on_battery_status_update, scr_data);
    home_update_battery_icon(scr_data);

    *data = scr_data;

    model_voice_cb_register(&model_voice_cbs, scr_data);
    model_camera_preview_cb_register(&home_camera_preview_presenter_cbs, scr_data);
    model_sd_music_sync_cb_register(&model_sd_music_sync_cbs, scr_data);

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
    home_show_pending_alarm_toast();

    return 0;
}

static int home_nav_scr_pause(const struct lisa_ui_nav_scr *scr, void *data)
{
    LISA_UI_LOGD("nav scr pause, id: %d", scr->unique_id);

    struct home_nav_scr_data *scr_data = (struct home_nav_scr_data *)data;
    if (scr_data && scr_data->view) {
        lv_obj_add_flag(scr_data->view, LV_OBJ_FLAG_HIDDEN);
    }

    if (scr_data && scr_data->network_status_timer) {
        lv_timer_pause(scr_data->network_status_timer);
    }

    if (scr_data && scr_data->camera_capture_timer) {
        lv_timer_pause(scr_data->camera_capture_timer);
    }

    home_handle_activity(scr_data);
    if (scr_data) {
        camera_preview_hide(scr_data);
        LISA_UI_LOGI("Page paused, camera preview state reset");
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

    if (scr_data && scr_data->network_status_timer) {
        lv_timer_resume(scr_data->network_status_timer);
    }

    model_voice_on();
    home_reset(scr_data);
    home_show_pending_alarm_toast();

    return 0;
}

static int home_nav_scr_close(const struct lisa_ui_nav_scr *scr, void *data)
{
    LISA_UI_LOGD("nav scr close, id: %d", scr->unique_id);

    model_voice_cb_unregister(&model_voice_cbs);
    model_camera_preview_cb_unregister(&home_camera_preview_presenter_cbs);
    model_battery_cb_unregister(model_battery_on_battery_status_update);
    model_sd_music_sync_cb_unregister(&model_sd_music_sync_cbs);

    struct home_nav_scr_data *scr_data = (struct home_nav_scr_data *)data;
    if (scr_data) {
        home_handle_activity(scr_data);
        home_stop_oneshot_emoji(scr_data);
        if (scr_data->anim_timer != NULL) {
            lv_timer_del(scr_data->anim_timer);
            scr_data->anim_timer = NULL;
        }
        if (scr_data->network_status_timer != NULL) {
            lv_timer_del(scr_data->network_status_timer);
            scr_data->network_status_timer = NULL;
        }
        if (scr_data->camera_capture_timer != NULL) {
            lv_timer_del(scr_data->camera_capture_timer);
            scr_data->camera_capture_timer = NULL;
        }
        if (scr_data->camera_preview_countdown_timer != NULL) {
            lv_timer_del(scr_data->camera_preview_countdown_timer);
            scr_data->camera_preview_countdown_timer = NULL;
        }
        if (scr_data->battery_query_timer != NULL) {
            lv_timer_del(scr_data->battery_query_timer);
            scr_data->battery_query_timer = NULL;
        }
        if (scr_data->standby_text_timer != NULL) {
            lv_timer_del(scr_data->standby_text_timer);
            scr_data->standby_text_timer = NULL;
        }
        if (scr_data->standby_sleep_timer != NULL) {
            lv_timer_del(scr_data->standby_sleep_timer);
            scr_data->standby_sleep_timer = NULL;
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

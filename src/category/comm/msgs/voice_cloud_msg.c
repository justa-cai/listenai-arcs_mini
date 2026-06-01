#include "stdint.h"
#include "stddef.h"
#include "stdbool.h"

#include "FreeRTOS.h"
#include "timers.h"

#include "lisa_log.h"
#include "voice_msg.h"
#include "app_datas.h"
#include "voice_cloud.h"
#include "app_player.h"
#include "voice_camera_preview_state.h"
#include "voice_player_comm.h"
#include "tone.h"
#include "kv.h"
#include "kv_sys.h"
#include "lisa_kv.h"
#include "sys_init.h"
#include "sys_network_manager.h"

#define TAG "voice.app.cloud"
#define VOICE_IDLE_EXIT_TIMEOUT_MS_DEFAULT ((uint32_t)CONFIG_CLOUD_IDLE_EXIT_TIMEOUT_MS)

static bool s_voice_cloud_session_running = false;
static bool s_voice_cloud_session_restart_after_tts = false;
static bool s_voice_cloud_tts_active = false;
static bool s_voice_photo_result_restart_pending = false;
static TimerHandle_t s_voice_idle_exit_timer = NULL;
static uint32_t s_voice_idle_exit_timeout_ms = VOICE_IDLE_EXIT_TIMEOUT_MS_DEFAULT;
static voice_msg_camera_preview_state_t s_camera_preview_state = {0};

static bool voice_cloud_session_is_running(const char *reason)
{
    if (!s_voice_cloud_session_running) {
        return false;
    }

    if (voice_cloud_is_session_active()) {
        return true;
    }

    LOGW("voice session state desynced, clear local running flag (%s)",
         reason ? reason : "unknown");
    s_voice_cloud_session_running = false;
    return false;
}

static void voice_idle_exit_timeout_refresh_from_kv(void)
{
    int timeout_ms = 0;
    int ret = lisa_kv_get_int(KV_KEY_IDLE_EXIT_TIMEOUT_MS, &timeout_ms);
    if (ret == 0 && timeout_ms > 0) {
        s_voice_idle_exit_timeout_ms = (uint32_t)timeout_ms;
        LOGI("voice idle exit timeout use kv: %u ms", (unsigned)s_voice_idle_exit_timeout_ms);
        return;
    }

    s_voice_idle_exit_timeout_ms = VOICE_IDLE_EXIT_TIMEOUT_MS_DEFAULT;
    LOGI("voice idle exit timeout use default: %u ms", (unsigned)s_voice_idle_exit_timeout_ms);
}

static void voice_idle_exit_timer_cb(TimerHandle_t xTimer)
{
    (void)xTimer;

    if (!voice_cloud_session_is_running("idle exit timeout")) {
        return;
    }

    LOGI("voice idle exit timeout reached, trigger mcp chat exit");
    voice_cloud_chat_stop();
    app_player_play(tone_player, app_tone_get_url(TONE_ID_72));
    voice_msg_pub(VOICE_MSG_CLOUD_SESSION_FINISHED, NULL, 0);
}

static void voice_idle_exit_timer_init(void)
{
    voice_idle_exit_timeout_refresh_from_kv();
    if (s_voice_idle_exit_timer == NULL) {
        s_voice_idle_exit_timer =
            xTimerCreate("voice.idle.exit", pdMS_TO_TICKS(s_voice_idle_exit_timeout_ms), pdFALSE, NULL,
                         voice_idle_exit_timer_cb);
        assert(s_voice_idle_exit_timer != NULL);
    }
}

static void voice_idle_exit_timer_stop(void)
{
    if (s_voice_idle_exit_timer == NULL) {
        return;
    }

    if (xTimerIsTimerActive(s_voice_idle_exit_timer) == pdFALSE) {
        return;
    }

    LOGI("stop idle exit timer");
    if (xTimerStop(s_voice_idle_exit_timer, 0) != pdPASS) {
        LOGW("voice_idle_exit_timer_stop failed");
    }
}

static void voice_idle_exit_timer_start(void)
{
    if (!voice_cloud_session_is_running("idle timer start")) {
        return;
    }

    if (s_voice_idle_exit_timer == NULL) {
        return;
    }

    voice_idle_exit_timeout_refresh_from_kv();

    if (xTimerIsTimerActive(s_voice_idle_exit_timer) != pdFALSE) {
        if (xTimerStop(s_voice_idle_exit_timer, 0) != pdPASS) {
            LOGW("voice_idle_exit_timer_stop before start failed");
        }
    }

    if (xTimerChangePeriod(s_voice_idle_exit_timer, pdMS_TO_TICKS(s_voice_idle_exit_timeout_ms), 0) != pdPASS) {
        LOGW("voice_idle_exit_timer_change failed");
    }

    LOGI("start idle exit timer: %u ms", (unsigned)s_voice_idle_exit_timeout_ms);
}

static bool voice_tts_is_playing(void)
{
    return s_voice_cloud_tts_active;
}

static bool voice_interaction_mode_pauses_tts_uplink(void)
{
    struct app_datas *app_datas = get_app_datas();
    if (app_datas == NULL) {
        return false;
    }

    return app_datas->int_mode == APP_INTERACTION_MODE_MULTI_NO_INTERRUPT;
}

static void voice_tts_uplink_pause_if_needed(const char *reason)
{
    int stop_ret = 0;

    if (!voice_interaction_mode_pauses_tts_uplink()) {
        return;
    }

    if (!voice_cloud_session_is_running(reason)) {
        return;
    }

    LOGI("stop current session during TTS and restart after playback (%s)",
         reason ? reason : "unknown");
    stop_ret = voice_cloud_chat_stop();
    if (stop_ret != 0) {
        LOGW("stop current session during TTS failed: %d", stop_ret);
        return;
    }

    s_voice_cloud_session_running = false;
    voice_idle_exit_timer_stop();
    voice_msg_pub(VOICE_MSG_CLOUD_SESSION_FINISHED, NULL, 0);
}

static void voice_tts_uplink_resume_if_needed(const char *reason)
{
    if (!voice_interaction_mode_pauses_tts_uplink()) {
        return;
    }

    if (!voice_cloud_session_is_running("tts resume fallback")) {
        LOGI("skip uplink resume after TTS, wait for session restart (%s)",
             reason ? reason : "unknown");
        return;
    }

    LOGI("resume uplink after TTS (%s)", reason ? reason : "unknown");
    voice_cloud_upload_audio_resume();
}

static int voice_continuous_session_restart(const char *reason)
{
    struct app_datas *app_datas = get_app_datas();
    char *kv_wakeword = NULL;
    const char *wakeword = NULL;
    const char *keywords[1];
    struct voice_cloud_chat_config chat_config = {0};
    int stop_ret = 0;
    int ret = -1;

    if (app_datas == NULL) {
        LOGW("continuous session restart ignored, app_datas is null");
        return -1;
    }

    if (!app_datas->voice_cloud_connected) {
        LOGW("continuous session restart ignored, cloud disconnected");
        return -1;
    }

    if (!app_datas->can_wakeup) {
        LOGW("continuous session restart ignored, can_wakeup=0");
        return -1;
    }

    if (!voice_interaction_mode_pauses_tts_uplink()) {
        LOGW("continuous session restart ignored, int_mode=%d", app_datas->int_mode);
        return -1;
    }

    lisa_kv_get_string(KV_KEY_SYS_WAKEWORD, &kv_wakeword);
    wakeword = (kv_wakeword && kv_wakeword[0] != '\0') ? kv_wakeword : "小聆小聆";
    keywords[0] = wakeword;

    chat_config.full_duplex = true;
    chat_config.timeout_ms = app_datas->full_duplex_timeout_ms;
    chat_config.oneshot = false;
    chat_config.words = (char **)keywords;
    chat_config.words_cnt = 1;

    if (voice_cloud_is_session_active()) {
        LOGI("restart continuous session after TTS (%s), stop existing session first",
             reason ? reason : "unknown");
        stop_ret = voice_cloud_chat_stop();
        if (stop_ret != 0) {
            LOGW("stop existing continuous session failed before restart: %d", stop_ret);
            lisa_kv_free(kv_wakeword);
            return stop_ret;
        }
        s_voice_cloud_session_running = false;
        voice_idle_exit_timer_stop();
        voice_msg_pub(VOICE_MSG_CLOUD_SESSION_FINISHED, NULL, 0);
    }

    LOGI("restart continuous session after TTS (%s), timeout_ms=%u",
         reason ? reason : "unknown", (unsigned)chat_config.timeout_ms);
    ret = voice_cloud_chat_start(&chat_config);
    lisa_kv_free(kv_wakeword);
    return ret;
}

static bool voice_photo_result_restart_pending_consume(const char *reason)
{
    int restart_ret = 0;

    if (!s_voice_photo_result_restart_pending) {
        return false;
    }

    s_voice_photo_result_restart_pending = false;

    restart_ret = voice_continuous_session_restart(reason);
    if (restart_ret == 0) {
        return true;
    }

    LOGW("voice photo result continuous session restart failed: %d", restart_ret);
    return false;
}

static int camera_preview_result_bargein_session_start(void)
{
    struct app_datas *app_datas = get_app_datas();
    sys_network_status_t network_status;
    char *kv_wakeword = NULL;
    const char *wakeword = NULL;
    const char *keywords[1];
    struct voice_cloud_chat_config chat_config = {0};
    int stop_ret = 0;
    int ret = 0;

    if (app_datas == NULL) {
        LOGW("voice photo result barge-in start ignored, app_datas is null");
        return -1;
    }

    bool network_connected = (sys_network_get_status(&network_status) == 0) && network_status.connected;

    if (!network_connected || !app_datas->voice_cloud_connected) {
        LOGW("voice photo result barge-in start ignored, network=%d cloud=%d",
             network_connected, app_datas->voice_cloud_connected);
        return -1;
    }

    if (!app_datas->can_wakeup) {
        LOGW("voice photo result barge-in start ignored, can_wakeup=0");
        return -1;
    }

    if (!app_interaction_mode_supports_barge_in(app_datas->int_mode)) {
        LOGI("voice photo result barge-in start ignored, int_mode=%d", app_datas->int_mode);
        return 0;
    }

    lisa_kv_get_string(KV_KEY_SYS_WAKEWORD, &kv_wakeword);
    wakeword = (kv_wakeword && kv_wakeword[0] != '\0') ? kv_wakeword : "小聆小聆";
    keywords[0] = wakeword;
    LOGI("barge-in wakeword: %s", wakeword);

    chat_config.full_duplex = app_interaction_mode_is_continuous(app_datas->int_mode);
    chat_config.timeout_ms = app_datas->full_duplex_timeout_ms;
    chat_config.oneshot = app_interaction_mode_is_continuous(app_datas->int_mode) ? false : app_datas->oneshot;
    chat_config.words = (char **)keywords;
    chat_config.words_cnt = 1;

    if (voice_cloud_session_is_running("photo result barge-in")) {
        LOGI("voice photo result tts enter with existing session, restart silent barge-in session");
        stop_ret = voice_cloud_chat_stop();
        if (stop_ret != 0) {
            LOGW("voice photo result barge-in stop old session failed: %d, force clear state", stop_ret);
        }
        s_voice_cloud_session_running = false;
        voice_idle_exit_timer_stop();
        voice_msg_pub(VOICE_MSG_CLOUD_SESSION_FINISHED, NULL, 0);
    }

    LOGI("start silent barge-in session for photo result tts, timeout_ms=%u",
         (unsigned)chat_config.timeout_ms);
    ret = voice_cloud_chat_start(&chat_config);
    lisa_kv_free(kv_wakeword);
    return ret;
}

static void camera_preview_restore_session_if_needed(const char *reason)
{
    if (!voice_cloud_session_is_running(reason)) {
        return;
    }

    if (voice_tts_is_playing() && voice_interaction_mode_pauses_tts_uplink()) {
        LOGI("voice photo flow cleared, keep uplink paused during TTS (%s)", reason ? reason : "unknown");
        voice_cloud_upload_audio_pause();
    } else {
        voice_cloud_upload_audio_resume();
    }

    if (!voice_tts_is_playing()) {
        LOGI("voice photo flow cleared, resume idle exit timer (%s)", reason ? reason : "unknown");
        voice_idle_exit_timer_start();
    } else {
        LOGI("voice photo flow cleared, keep idle timer paused for TTS (%s)", reason ? reason : "unknown");
    }
}

static void camera_preview_apply_guard(const char *reason)
{
    if (voice_camera_preview_state_is_locked(&s_camera_preview_state)) {
        LOGI("voice photo flow locked, pause uplink and hold idle timer (%s)",
             reason ? reason : "unknown");
        voice_cloud_upload_audio_pause();
        voice_idle_exit_timer_stop();
        return;
    }

    if (voice_camera_preview_state_is_result_tts_active(&s_camera_preview_state)) {
        if (voice_interaction_mode_pauses_tts_uplink()) {
            LOGI("voice photo result TTS phase, pause uplink and hold idle timer (%s)",
                 reason ? reason : "unknown");
            voice_cloud_upload_audio_pause();
        } else {
            LOGI("voice photo result TTS phase, resume uplink and hold idle timer (%s)",
                 reason ? reason : "unknown");
            voice_cloud_upload_audio_resume();
        }
        voice_idle_exit_timer_stop();
        return;
    }

    camera_preview_restore_session_if_needed(reason);
}

static void voice_cloud_connected(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    LOGI("voice_cloud_connected");

    struct app_datas *app_datas = get_app_datas();
    assert(app_datas != NULL);

    app_datas->voice_cloud_connected = 1;

    lsc_music_active();
}

static void voice_cloud_disconnected(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    LOGI("voice_cloud_disconnected");

    struct app_datas *app_datas = get_app_datas();
    assert(app_datas != NULL);

    app_datas->voice_cloud_connected = 0;
    s_voice_cloud_session_running = false;
    s_voice_cloud_session_restart_after_tts = false;
    s_voice_cloud_tts_active = false;
    s_voice_photo_result_restart_pending = false;
    voice_idle_exit_timer_stop();
}

static void voice_cloud_session_starting(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    LOGI("voice_cloud_session_starting");
    s_voice_cloud_session_running = true;
    s_voice_cloud_session_restart_after_tts = false;
    s_voice_photo_result_restart_pending = false;
    if (voice_camera_preview_state_is_active(&s_camera_preview_state)) {
        camera_preview_apply_guard("session starting");
    } else {
        voice_idle_exit_timer_start();
    }
}

static void voice_cloud_tts_txt(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    if (msg_id == VOICE_MSG_CLOUD_TTS_TEXT_START) {
        LOGI("voice_cloud_tts_txt, start");
    } else if (msg_id == VOICE_MSG_CLOUD_TTS_TEXT_UPDATE) {
        LOGI("voice_cloud_tts_txt, update: %s", (char *)data);
    } else if (msg_id == VOICE_MSG_CLOUD_TTS_TEXT_END) {
        LOGI("voice_cloud_tts_txt, end");
    }
}

static void voice_cloud_tts_url_received(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)data;
    (void)len;
    (void)user_data;

    if (!voice_cloud_session_is_running("tts url received")) {
        return;
    }

    s_voice_cloud_tts_active = true;
    LOGI("tts url received, mark TTS active and stop idle exit timer");
    voice_tts_uplink_pause_if_needed("tts url received");
    voice_idle_exit_timer_stop();
}

static void voice_cloud_iat_txt(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    if (msg_id == VOICE_MSG_CLOUD_IAT_START) {
        LOGI("voice_cloud_iat_txt, start");
    } else if (msg_id == VOICE_MSG_CLOUD_IAT_UPDATE) {
        LOGI("voice_cloud_iat_txt, update: %s", (char *)data);
        if (data && len > 0 && ((char *)data)[0] != '\0') {
            voice_idle_exit_timer_stop();
            service_image_waiting_cancel();
        }
    } else if (msg_id == VOICE_MSG_CLOUD_IAT_END) {
        LOGI("voice_cloud_iat_txt, end");
    }
}

static void voice_cloud_tts_playing(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)data;
    (void)len;
    (void)user_data;

    if (!voice_cloud_session_is_running("tts playing")) {
        return;
    }

    s_voice_cloud_tts_active = true;
    LOGI("tts playing, mark TTS active and stop idle exit timer");
    voice_tts_uplink_pause_if_needed("tts playing");
    voice_idle_exit_timer_stop();
}

static void voice_cloud_tts_stoped(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    bool result_tts_active = false;

    (void)unused;
    (void)msg_id;
    (void)data;
    (void)len;
    (void)user_data;

    result_tts_active = voice_camera_preview_state_is_result_tts_active(&s_camera_preview_state);

    if (voice_camera_preview_state_is_active(&s_camera_preview_state)) {
        s_voice_cloud_tts_active = false;
        s_voice_cloud_session_restart_after_tts = false;
        if (result_tts_active && voice_interaction_mode_pauses_tts_uplink()) {
            s_voice_photo_result_restart_pending = true;
            LOGI("voice photo result TTS stopped, restart continuous session after photo flow exit");
        } else {
            s_voice_photo_result_restart_pending = false;
        }
        LOGI("tts stopped during voice photo flow, keep idle exit timer paused");
        camera_preview_apply_guard("tts stopped");
        return;
    }

    if (voice_photo_result_restart_pending_consume("voice photo result tts stopped")) {
        s_voice_cloud_tts_active = false;
        return;
    }

    if (s_voice_cloud_session_restart_after_tts) {
        int restart_ret = 0;

        s_voice_cloud_session_restart_after_tts = false;
        s_voice_cloud_tts_active = false;
        restart_ret = voice_continuous_session_restart("tts stopped");
        if (restart_ret == 0) {
            return;
        }

        LOGW("continuous session restart after TTS failed: %d", restart_ret);
    } else {
        s_voice_cloud_tts_active = false;
    }

    voice_tts_uplink_resume_if_needed("tts stopped");
    LOGI("tts stopped, resume idle exit timer");
    voice_idle_exit_timer_start();
}

static void voice_cloud_session_finished(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    LOGI("voice_cloud_session_finished, stop idle timer and mark session inactive");
    if (!voice_camera_preview_state_is_active(&s_camera_preview_state) &&
        voice_interaction_mode_pauses_tts_uplink() && s_voice_cloud_tts_active) {
        s_voice_cloud_session_restart_after_tts = true;
        LOGI("session finished during TTS, schedule continuous session restart after TTS");
    } else {
        s_voice_cloud_session_restart_after_tts = false;
    }
    s_voice_cloud_session_running = false;
    voice_idle_exit_timer_stop();

    // app_player focus is managed automatically
}

static void voice_cloud_mcp_chat_exit(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    LOGI("voice_cloud_mcp_chat_exit");

    s_voice_cloud_session_running = false;
    s_voice_cloud_session_restart_after_tts = false;
    s_voice_cloud_tts_active = false;
    s_voice_photo_result_restart_pending = false;
    voice_idle_exit_timer_stop();
    voice_cloud_chat_stop();
    voice_msg_pub(VOICE_MSG_CLOUD_SESSION_FINISHED, NULL, 0);
}

static void voice_camera_preview_state_changed(void *unused, uint32_t msg_id, void *data,
                                               uint32_t len, void *user_data)
{
    voice_msg_camera_preview_state_t prev_state = s_camera_preview_state;
    bool was_active = voice_camera_preview_state_is_active(&s_camera_preview_state);
    bool exited_result_tts = false;
    bool enter_result_tts = false;

    if (!voice_camera_preview_state_parse(&s_camera_preview_state, data, len)) {
        LOGW("invalid camera preview state payload");
        return;
    }

    enter_result_tts = !voice_camera_preview_state_is_result_tts_active(&prev_state) &&
                       voice_camera_preview_state_is_result_tts_active(&s_camera_preview_state);
    LOGI("camera preview state changed, active=%u mode=%u captured=%u phase=%u",
         s_camera_preview_state.active,
         s_camera_preview_state.mode,
         s_camera_preview_state.captured,
         s_camera_preview_state.phase);

    if (voice_camera_preview_state_is_active(&s_camera_preview_state)) {
        camera_preview_apply_guard("preview state changed");
        if (enter_result_tts) {
            int ret = camera_preview_result_bargein_session_start();
            LOGI("voice photo result tts enter, barge-in session start ret=%d", ret);
        }
        return;
    }

    if (was_active) {
        exited_result_tts = voice_camera_preview_state_is_result_tts_active(&prev_state);
        if (exited_result_tts && voice_interaction_mode_pauses_tts_uplink() &&
            !voice_cloud_session_is_running("voice photo flow exit")) {
            s_voice_photo_result_restart_pending = true;
            LOGI("voice photo result flow exited, continuous session restart pending");
        }

        if (exited_result_tts &&
            voice_photo_result_restart_pending_consume("voice photo flow exit")) {
            return;
        }

        camera_preview_restore_session_if_needed("preview flow exit");
    }
}

static void voice_camera_preview_exit(void *unused, uint32_t msg_id, void *data, uint32_t len,
                                      void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)data;
    (void)len;
    (void)user_data;

    if (!voice_camera_preview_state_is_active(&s_camera_preview_state)) {
        return;
    }

    LOGI("voice photo preview exit received, cancel pending recognition");
    voice_cloud_image_recognition_drop_pending_result();
    camera_preview_restore_session_if_needed("preview exit message");
}

int voice_cloud_evt_init(void)
{
    voice_idle_exit_timer_init();

    voice_msg_sub(VOICE_MSG_CLOUD_CONNECTED, voice_cloud_connected, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_DISCONNECTED, voice_cloud_disconnected, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_SESSION_STARTING, voice_cloud_session_starting, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_SESSION_FINISHED, voice_cloud_session_finished, NULL);

    voice_msg_sub(VOICE_MSG_CLOUD_TTS_URL, voice_cloud_tts_url_received, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_TTS_TEXT_START, voice_cloud_tts_txt, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_TTS_TEXT_UPDATE, voice_cloud_tts_txt, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_TTS_TEXT_END, voice_cloud_tts_txt, NULL);

    voice_msg_sub(VOICE_MSG_CLOUD_IAT_START, voice_cloud_iat_txt, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_IAT_UPDATE, voice_cloud_iat_txt, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_IAT_END, voice_cloud_iat_txt, NULL);
    voice_msg_sub(VOICE_MSG_PLAYER_TTS_PLAYING, voice_cloud_tts_playing, NULL);
    voice_msg_sub(VOICE_MSG_PLAYER_TTS_STOPED, voice_cloud_tts_stoped, NULL);
    voice_msg_sub(VOICE_MSG_APP_CAMERA_PREVIEW_STATE, voice_camera_preview_state_changed, NULL);
    voice_msg_sub(VOICE_MSG_APP_CAMERA_PREVIEW_EXIT, voice_camera_preview_exit, NULL);

    voice_msg_sub(VOICE_MSG_CLOUD_MCP_CHAT_EXIT, voice_cloud_mcp_chat_exit, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_SESSION_INTERRUPT, voice_cloud_mcp_chat_exit, NULL);

    return 0;
}

SYS_INIT(voice_cloud_evt_init, SYS_INIT_LEVEL_PRE_APPLICATION, 50);

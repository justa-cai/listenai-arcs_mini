#include "stdint.h"
#include "stddef.h"
#include "FreeRTOS.h"
#include "task.h"

#define TAG "voice.app.wakeup"
#include "voice_msg.h"
#include "app_datas.h"
#include "sys_init.h"
#include "sys_network_manager.h"

#include "voice_cloud.h"
#include "service_image.h"

#include "app_player.h"
#include "tone.h"
#include "voice_player_comm.h"
#include "lisa_log.h"

__attribute__((weak)) bool app_voice_interaction_blocked(void)
{
    return false;
}

enum {
    VOICE_WAKEUP_QR_STATUS_NOT_CONNECTED = 0,
    VOICE_WAKEUP_QR_STATUS_BIND = 3,
    VOICE_WAKEUP_QR_STATUS_AUTH_FAILED = 4,
};

static bool is_valid_keyword(char *keyword)
{
    if (keyword == NULL) {
        return false;
    }

    return true;
}

static bool voice_wakeup_should_open_info(uint32_t *status_out)
{
    uint32_t status = 0xFFFFFFFFU;

    switch (voice_cloud_get_state()) {
    case VOICE_CLOUD_STATE_CONNECTING:
        return false;
    case VOICE_CLOUD_STATE_NO_NETWORK:
    case VOICE_CLOUD_STATE_NO_INTERNET:
        status = VOICE_WAKEUP_QR_STATUS_NOT_CONNECTED;
        break;
    case VOICE_CLOUD_STATE_TOKEN_FAILED:
        status = VOICE_WAKEUP_QR_STATUS_AUTH_FAILED;
        break;
    case VOICE_CLOUD_STATE_CONNECT_FAILED:
        status = VOICE_WAKEUP_QR_STATUS_BIND;
        break;
    default:
        return false;
    }

    if (status_out) {
        *status_out = status;
    }

    return true;
}

static void voice_wakeup_prompt_cloud_info(uint32_t status)
{
#if CONFIG_WIFI_MANAGER
    static TickType_t s_last_netcfg_prompt_tick = 0;
    TickType_t now = xTaskGetTickCount();

    switch (status) {
    case VOICE_WAKEUP_QR_STATUS_AUTH_FAILED:
        voice_player_play_tone_url(app_tone_get_url(TONE_ID_105));
        break;
    case VOICE_WAKEUP_QR_STATUS_NOT_CONNECTED:
        if (s_last_netcfg_prompt_tick == 0 ||
            (now - s_last_netcfg_prompt_tick) >= pdMS_TO_TICKS(2000)) {
            voice_player_play_tone_url(app_tone_get_url(TONE_ID_70));
            s_last_netcfg_prompt_tick = now;
        }
        break;
    default:
        break;
    }

    voice_msg_pub(VOICE_MSG_CLOUD_OPEN_INFO, &status, sizeof(status));
#else
    (void)status;
#endif
}

static bool voice_wakeup_is_button_ptt_action(voice_msg_button_action_t action)
{
    return action == VOICE_MSG_BUTTON_ACTION_PRESS_DOWN ||
           action == VOICE_MSG_BUTTON_ACTION_SHORT_UP ||
           action == VOICE_MSG_BUTTON_ACTION_LONG_UP ||
           action == VOICE_MSG_BUTTON_ACTION_LONG_HOLD_UP;
}

static bool start_voice_cloud(struct app_datas *app_datas)
{
    sys_network_status_t status;

    if (app_datas == NULL) {
        LOGW("Invalid app_datas");
        return false;
    }

    if (sys_network_get_status(&status) != 0 || !status.connected) {
        LOGW("Network not connected");
        return false;
    }

    if (app_datas->voice_cloud_connected == 0) {
        LOGW("Voice cloud not connected");
        return false;
    }

    if (app_datas->can_wakeup == 0) {
        LOGI("ignore wakeup msg");
        return false;
    }

    const char *keywords[] = {"小聆小聆"};
    struct voice_cloud_chat_config chat_config = {
        .full_duplex = app_interaction_mode_is_continuous(app_datas->int_mode),
        .timeout_ms = app_datas->full_duplex_timeout_ms,
        .oneshot = app_interaction_mode_is_continuous(app_datas->int_mode) ? false : app_datas->oneshot,
        .words = (char **)keywords,
        .words_cnt = 1,
    };

    return voice_cloud_chat_start(&chat_config) == 0;
}

static bool voice_wakeup_tts_interrupt_needed(void)
{
    app_player_state_t state;

    if (tts_player == NULL) {
        return false;
    }

    state = app_player_get_state(tts_player);
    return state == APP_PLAYER_STATE_PREPARING ||
           state == APP_PLAYER_STATE_PREPARED ||
           state == APP_PLAYER_STATE_PLAYING ||
           state == APP_PLAYER_STATE_PAUSED;
}

static void interrupt_photo_flow(const char *source)
{
    LOGI("interrupt photo flow by %s", source ? source : "unknown");
    if (voice_wakeup_tts_interrupt_needed()) {
        if (app_player_stop(tts_player) == APP_PLAYER_OK) {
            LOGI("interrupt photo flow: stopped active tts");
        } else {
            LOGW("interrupt photo flow: stop tts failed");
        }
    }
    voice_msg_pub(VOICE_MSG_APP_CAMERA_PREVIEW_EXIT, NULL, 0);
}

static void voice_wakeup_keyword(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    LOGI("voice_wakeup_keyword, keyword: %s", (char *)data);
    struct app_datas *app_datas = get_app_datas();

    if (!is_valid_keyword((char *)data)) {
        LOGW("Invalid keyword: %s", (char *)data);
        return;
    }

    if ((app_datas->voice_work_mode & VOICE_WORK_MODE_VOICE_WAKEUP) == 0) {
        LOGW("ignore wakeup evt, voice work mode: %d", app_datas->voice_work_mode);
        return;
    }

    if (app_voice_interaction_blocked()) {
        LOGI("ignore wakeup keyword: voice interaction blocked");
        return;
    }

    uint32_t info_status = 0;
    if (voice_wakeup_should_open_info(&info_status)) {
        LOGI("voice wakeup ignored: cloud unavailable status=%u", (unsigned)info_status);
        voice_wakeup_prompt_cloud_info(info_status);
        return;
    }

    service_image_waiting_cancel();
    interrupt_photo_flow("voice wakeup");
    start_voice_cloud(app_datas);
}

static void voice_wakeup_button_event(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)len;
    (void)user_data;

    if (data == NULL || len < sizeof(voice_msg_button_evt_t)) {
        LOGW("Invalid button evt payload");
        return;
    }

    voice_msg_button_evt_t *evt = (voice_msg_button_evt_t *)data;
    LOGI("button evt id=%d action=%d", evt->button_id, evt->action);

    /* only k1 (button0) is configured for PTT */
    if (evt->button_id != 0) {
        LOGI("ignore button_id=%d", evt->button_id);
        return;
    }

    if (!voice_wakeup_is_button_ptt_action(evt->action)) {
        return;
    }

    if (app_voice_interaction_blocked()) {
        LOGI("ignore button wakeup: voice interaction blocked");
        return;
    }

    uint32_t info_status = 0;
    if (voice_wakeup_should_open_info(&info_status)) {
        if (evt->action == VOICE_MSG_BUTTON_ACTION_SHORT_UP) {
            LOGI("button wakeup ignored: cloud unavailable status=%u", (unsigned)info_status);
            voice_wakeup_prompt_cloud_info(info_status);
        }
        return;
    }

    if (evt->action == VOICE_MSG_BUTTON_ACTION_PRESS_DOWN) {
        voice_msg_pub(VOICE_MSG_WAKEUP_BUTTON_START, NULL, 0);
    } else if (evt->action == VOICE_MSG_BUTTON_ACTION_SHORT_UP
        || evt->action == VOICE_MSG_BUTTON_ACTION_LONG_UP
        || evt->action == VOICE_MSG_BUTTON_ACTION_LONG_HOLD_UP) {
        voice_msg_pub(VOICE_MSG_WAKEUP_BUTTON_STOP, NULL, 0);
    }
}

static void voice_msg_btn_wakeup_start(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    struct app_datas *app_datas = get_app_datas();
    if (app_datas == NULL) {
        return;
    }

    if ((app_datas->voice_work_mode & VOICE_WORK_MODE_BUTTON_WAKEUP) == 0) {
        LOGW("ignore button evt, voice work mode: %d", app_datas->voice_work_mode);
        return;
    }

    if (!app_datas->can_wakeup) {
        return;
    }

    if (app_voice_interaction_blocked()) {
        LOGI("ignore button wakeup start: voice interaction blocked");
        return;
    }

    if (!app_datas->voice_cloud_connected) {
        return;
    }

    service_image_waiting_cancel();
    interrupt_photo_flow("button wakeup");
    voice_cloud_audio_recognition_start();
}

static void voice_msg_btn_wakeup_stop(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    struct app_datas *app_datas = get_app_datas();
    if (app_datas == NULL) {
        return;
    }

    if ((app_datas->voice_work_mode & VOICE_WORK_MODE_BUTTON_WAKEUP) == 0) {
        LOGW("ignore button evt, voice work mode: %d", app_datas->voice_work_mode);
        return;
    }

    if (!app_datas->can_wakeup) {
        return;
    }

    if (app_voice_interaction_blocked()) {
        LOGI("ignore button wakeup stop: voice interaction blocked");
        return;
    }

    if (!app_datas->voice_cloud_connected) {
        return;
    }

    voice_cloud_audio_recognition_stop();
}

int voice_wakeup_evt_init(void)
{
    voice_msg_sub(VOICE_MSG_WAKEUP_KEYWORD, voice_wakeup_keyword, NULL);
    voice_msg_sub(VOICE_MSG_BUTTON_CHANGE, voice_wakeup_button_event, NULL);
    voice_msg_sub(VOICE_MSG_WAKEUP_BUTTON_START, voice_msg_btn_wakeup_start, NULL);
    voice_msg_sub(VOICE_MSG_WAKEUP_BUTTON_STOP, voice_msg_btn_wakeup_stop, NULL);

    return 0;
}

SYS_INIT(voice_wakeup_evt_init, SYS_INIT_LEVEL_PRE_APPLICATION, 50);

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

#include "app_player.h"
#include "tone.h"
#include "voice_player_comm.h"
#include "lisa_log.h"

static bool is_valid_keyword(char *keyword)
{
    if (keyword == NULL) {
        return false;
    }

    return true;
}

static bool voice_wakeup_should_prompt_wifi_provision(void)
{
    sys_network_status_t status;

    return sys_network_get_status(&status) == 0 && status.wifi_provision_required;
}

static void voice_wakeup_prompt_wifi_provision(void)
{
#if CONFIG_WIFI_MANAGER
    static TickType_t s_last_prompt_tick = 0;
    TickType_t now = xTaskGetTickCount();

    if (s_last_prompt_tick == 0 || (now - s_last_prompt_tick) >= pdMS_TO_TICKS(1000)) {
        app_player_play(tone_player, app_tone_get_url(TONE_ID_70));
        s_last_prompt_tick = now;
    }

    voice_msg_pub(VOICE_MSG_CLOUD_OPEN_INFO, NULL, 0);
#endif
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

static void interrupt_photo_flow(const char *source)
{
    LOGI("interrupt photo flow by %s", source ? source : "unknown");
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

    if (voice_wakeup_should_prompt_wifi_provision()) {
        LOGI("voice wakeup ignored: WiFi provisioning is required");
        voice_wakeup_prompt_wifi_provision();
        return;
    }

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

    if (voice_wakeup_should_prompt_wifi_provision()) {
        LOGI("button wakeup ignored: WiFi provisioning is required");
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

    if (!app_datas->voice_cloud_connected) {
        return;
    }

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

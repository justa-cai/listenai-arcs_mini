#include "stdint.h"
#include "stddef.h"
#include "stdbool.h"

#include "FreeRTOS.h"
#include "timers.h"

#include "lisa_log.h"
#include "voice_msg.h"
#include "app_datas.h"
#include "voice_cloud.h"
#include "player_mgr.h"
#include "tone.h"
#include "kv.h"
#include "lisa_kv.h"
#include "sys_init.h"

#define TAG "voice.app.cloud"
#define VOICE_IDLE_EXIT_TIMEOUT_MS_DEFAULT ((uint32_t)CONFIG_CLOUD_IDLE_EXIT_TIMEOUT_MS)

static bool s_voice_cloud_session_running = false;
static TimerHandle_t s_voice_idle_exit_timer = NULL;
static uint32_t s_voice_idle_exit_timeout_ms = VOICE_IDLE_EXIT_TIMEOUT_MS_DEFAULT;

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

    if (!s_voice_cloud_session_running) {
        return;
    }

    LOGI("voice idle exit timeout reached, trigger mcp chat exit");
    voice_cloud_chat_stop();
    voice_msg_pub(VOICE_MSG_CLOUD_SESSION_FINISHED, NULL, 0);
    player_mgr_play(LOCAL, app_tone_get_url(TONE_ID_72), 0);
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

    if (xTimerStop(s_voice_idle_exit_timer, 0) != pdPASS) {
        LOGW("voice_idle_exit_timer_stop failed");
    }
}

static void voice_idle_exit_timer_start(void)
{
    if (!s_voice_cloud_session_running) {
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
    voice_idle_exit_timer_stop();
}

static void voice_cloud_session_starting(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    LOGI("voice_cloud_session_starting");
    s_voice_cloud_session_running = true;
    voice_idle_exit_timer_start();
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

static void voice_cloud_tts_stoped(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)data;
    (void)len;
    (void)user_data;

    LOGI("tts stopped, start idle exit timer");
    voice_idle_exit_timer_start();
}

static void voice_cloud_session_finished(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    LOGI("voice_cloud_session_finished");
    s_voice_cloud_session_running = false;
    voice_idle_exit_timer_stop();

    // player_mgr_focus_release(AIP);
}

static void voice_cloud_mcp_chat_exit(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    LOGI("voice_cloud_mcp_chat_exit");

    s_voice_cloud_session_running = false;
    voice_idle_exit_timer_stop();
    voice_cloud_chat_stop();
    voice_msg_pub(VOICE_MSG_CLOUD_SESSION_FINISHED, NULL, 0);
}

int voice_cloud_evt_init(void)
{
    voice_idle_exit_timer_init();

    voice_msg_sub(VOICE_MSG_CLOUD_CONNECTED, voice_cloud_connected, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_DISCONNECTED, voice_cloud_disconnected, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_SESSION_STARTING, voice_cloud_session_starting, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_SESSION_FINISHED, voice_cloud_session_finished, NULL);

    voice_msg_sub(VOICE_MSG_CLOUD_TTS_TEXT_START, voice_cloud_tts_txt, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_TTS_TEXT_UPDATE, voice_cloud_tts_txt, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_TTS_TEXT_END, voice_cloud_tts_txt, NULL);

    voice_msg_sub(VOICE_MSG_CLOUD_IAT_START, voice_cloud_iat_txt, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_IAT_UPDATE, voice_cloud_iat_txt, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_IAT_END, voice_cloud_iat_txt, NULL);
    voice_msg_sub(VOICE_MSG_PLAYER_TTS_STOPED, voice_cloud_tts_stoped, NULL);

    voice_msg_sub(VOICE_MSG_CLOUD_MCP_CHAT_EXIT, voice_cloud_mcp_chat_exit, NULL);

    return 0;
}

SYS_INIT(voice_cloud_evt_init, SYS_INIT_LEVEL_PRE_APPLICATION, 50);

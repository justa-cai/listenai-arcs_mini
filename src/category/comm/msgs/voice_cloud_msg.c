#include "stdint.h"
#include "stddef.h"

#include "lisa_log.h"
#include "voice_msg.h"
#include "app_datas.h"
#include "sys_init.h"

#define TAG "voice.app.cloud"

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
            service_image_waiting_cancel();
        }
    } else if (msg_id == VOICE_MSG_CLOUD_IAT_END) {
        LOGI("voice_cloud_iat_txt, end");
    }
}

static void voice_cloud_session_finished(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    LOGI("voice_cloud_session_finished");

    // player_mgr_focus_release(AIP);
}

static void voice_cloud_mcp_chat_exit(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    LOGI("voice_cloud_mcp_chat_exit");

    voice_cloud_chat_stop();
    voice_msg_pub(VOICE_MSG_CLOUD_SESSION_FINISHED, NULL, 0);
}

int voice_cloud_evt_init(void)
{
    voice_msg_sub(VOICE_MSG_CLOUD_CONNECTED, voice_cloud_connected, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_DISCONNECTED, voice_cloud_disconnected, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_SESSION_FINISHED, voice_cloud_session_finished, NULL);

    voice_msg_sub(VOICE_MSG_CLOUD_TTS_TEXT_START, voice_cloud_tts_txt, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_TTS_TEXT_UPDATE, voice_cloud_tts_txt, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_TTS_TEXT_END, voice_cloud_tts_txt, NULL);

    voice_msg_sub(VOICE_MSG_CLOUD_IAT_START, voice_cloud_iat_txt, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_IAT_UPDATE, voice_cloud_iat_txt, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_IAT_END, voice_cloud_iat_txt, NULL);

    voice_msg_sub(VOICE_MSG_CLOUD_MCP_CHAT_EXIT, voice_cloud_mcp_chat_exit, NULL);

    return 0;
}

SYS_INIT(voice_cloud_evt_init, SYS_INIT_LEVEL_PRE_APPLICATION, 50);

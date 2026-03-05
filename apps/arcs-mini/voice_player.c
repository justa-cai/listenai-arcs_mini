#include <string.h>
#include <stdio.h>

#include "sys_init.h"
#include "voice_msg.h"
#include "app_datas.h"

#include "tone.h"
#include "player_mgr.h"
#include "lisa_player.h"
#include "service_alarm.h"
#include "alarm_ring.h"

#define TAG "platform"
#include "lisa_log.h"

static bool s_disconnect_tone_played = false;

void voice_player_play_msg(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{

    struct app_datas *app_datas = get_app_datas();

    if (app_datas == NULL) {
        LOGW("Invalid app_datas");
        return;
    }
    switch (msg_id) {
    case VOICE_MSG_WAKEUP_BUTTON_START: {
        if ((app_datas->voice_work_mode & VOICE_WORK_MODE_BUTTON_WAKEUP) == 0) {
            return;
        }

        if (!app_datas->can_wakeup) {
            return;
        }

        if (app_datas->voice_cloud_connected == 1) {
            player_mgr_play(AIP, app_tone_get_url(TONE_ID_0), 0);
        } else if (!app_datas->wifi_connected) {
            player_mgr_play(LOCAL, app_tone_get_url(TONE_ID_64), 0);
        } else if (app_datas->auth_failed) {
            player_mgr_play(LOCAL, app_tone_get_url(TONE_ID_105), 0);
        } else {
            player_mgr_play(LOCAL, app_tone_get_url(TONE_ID_85), 0);
        }
    } break;
    case VOICE_MSG_WAKEUP_KEYWORD: {
        if ((app_datas->voice_work_mode & VOICE_WORK_MODE_VOICE_WAKEUP) == 0) {
            return;
        }

        if (!app_datas->can_wakeup) {
            return;
        }

        if (app_datas->voice_cloud_connected == 1) {
            player_mgr_play(AIP, app_tone_get_url(TONE_ID_0), 0);
        } else if (!app_datas->wifi_connected) {
            player_mgr_play(LOCAL, app_tone_get_url(TONE_ID_64), 0);
        } else if (app_datas->auth_failed) {
            player_mgr_play(LOCAL, app_tone_get_url(TONE_ID_105), 0);
        } else {
            player_mgr_play(LOCAL, app_tone_get_url(TONE_ID_85), 0);
        }
    } break;
    case VOICE_MSG_CLOUD_CLOUD_AUTH_SUCCESS: {
        app_datas->auth_failed = 0;
        s_disconnect_tone_played = false;
        player_mgr_play(LOCAL, app_tone_get_url(TONE_ID_59), 0);
    } break;
    case VOICE_MSG_CLOUD_CLOUD_AUTH_FAILED: {
        app_datas->auth_failed = 1;
        player_mgr_play(LOCAL, app_tone_get_url(TONE_ID_105), 0);
    } break;
    case VOICE_MSG_CLOUD_DISCONNECTED: {
        if (!s_disconnect_tone_played) {
            if(app_datas->wifi_connected) {
                player_mgr_play(LOCAL, app_tone_get_url(TONE_ID_65), 0);
            } else {
                player_mgr_play(LOCAL, app_tone_get_url(TONE_ID_60), 0);
            }
            s_disconnect_tone_played = true;
        }
    } break;
    case VOICE_MSG_WIFI_CONNECTED:{
        s_disconnect_tone_played = false;
    } break;
    case VOICE_MSG_WIFI_DISCONNECTED: {
        if (!s_disconnect_tone_played) {
            player_mgr_play(LOCAL, app_tone_get_url(TONE_ID_60), 0);
            s_disconnect_tone_played = true;
        }
    } break;
    case VOICE_MSG_CLOUD_TTS_URL: {
        if (data == NULL) {
            LOGE("Invalid data");
        } else {
            LOGI("Ready to play tts url: %s", (char *)data);
            player_mgr_play(TTS, (char *)data, 0);
        }
    } break;
    default:
        break;
    }
}

static void voice_player_audio_item(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    struct voice_msg_audio_items *items = (struct voice_msg_audio_items *)data;
    if (items == NULL) {
        LOGE("Invalid data");
        return;
    }

    if (items->cnt == 0) {
        LOGE("Invalid cnt");
        return;
    }

    audio_out_t *list = (audio_out_t *)lisa_mem_calloc(items->cnt, sizeof(audio_out_t));
    if (list == NULL) {
        LOGE("Failed to allocate memory");
        return;
    }

    for (int i = 0; i < items->cnt; i++) {
        memcpy(list[i].mid, items->items[i].id, strlen(items->items[i].id) + 1);
    }

    player_mgr_play_array(CONTENT, list, items->cnt);

    lisa_mem_free(list);
}

static void voice_player_play_control(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    switch (msg_id) {
    case VOICE_MSG_PLAY_CONTROL_PLAY: {
        player_mgr_resume(CONTENT);
    } break;
    case VOICE_MSG_PLAY_CONTROL_PAUSE: {
        player_mgr_pause(CONTENT);
    } break;
    case VOICE_MSG_PLAY_CONTROL_NEXT: {
        player_mgr_play_next(CONTENT);
    } break;
    case VOICE_MSG_PLAY_CONTROL_PREVIOUS: {
        player_mgr_play_prev(CONTENT);
    } break;
    case VOICE_MSG_PLAY_CONTROL_REPLAY: {
    } break;
    default:
        break;
    }
}

static void on_tts_play_status(int player_id, uint16_t status, void *arg)
{
    uint32_t msg = 0;

    LOGI("player id: %d, status: %d", player_id, status);

    if (status == PLAYER_EVT_PLAYING) {
        voice_msg_pub(VOICE_MSG_PLAYER_TTS_PLAYING, NULL, 0);
    } else if (status == PLAYER_EVT_PAUSED) {
        voice_msg_pub(VOICE_MSG_PLAYER_TTS_PAUSED, NULL, 0);
    } else if (status == PLAYER_EVT_PLAYBACK_COMPLETE) {
        voice_msg_pub(VOICE_MSG_PLAYER_TTS_STOPED, NULL, 0);
        if (alarm_ring_is_active()) {
            alarm_ring_notify_playback_complete();
        }
    } else if (status == PLAYER_EVT_STOPED || status == PLAYER_EVT_ERROR) {
        voice_msg_pub(VOICE_MSG_PLAYER_TTS_STOPED, NULL, 0);
    }
}

static void on_local_play_status(int player_id, uint16_t status, void *arg)
{
    (void)player_id;
    (void)arg;

    if (status == PLAYER_EVT_PLAYBACK_COMPLETE && alarm_ring_is_active()) {
        alarm_ring_notify_playback_complete();
    }
}

static void voice_alarm_ring_play_once(const char *text)
{
    struct app_datas *app_datas = get_app_datas();
    if (app_datas == NULL || app_datas->voice_cloud_connected == 0 || !text || text[0] == 0) {
        player_mgr_play(LOCAL, app_tone_get_url(TONE_ID_94), 0);
        return;
    }

    char *tts_text = lisa_mem_alloc(256);
    if (tts_text == NULL) {
        LOGW("voice_alarm_ring_play_once, alloc failed, fallback local tone");
        player_mgr_play(LOCAL, app_tone_get_url(TONE_ID_94), 0);
        return;
    }

    snprintf(tts_text, 256, "你有一个 %s 的提醒, 请不要忘记哦!", text);
    voice_cloud_tts_synth(tts_text);
    lisa_mem_free(tts_text);
}

static void voice_alarm_ring_force_stop(void)
{
    player_mgr_stop(LOCAL);
    player_mgr_stop(TTS);
}

static void voice_player_mcp_chat_exit(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    LOGI("voice_player_mcp_chat_exit, stop content player");
    audio_player_stop_by_user();
}

void voice_player_ready(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    LISA_LOGI(TAG, "voice_player_ready");
    voice_msg_sub(VOICE_MSG_WAKEUP_BUTTON_START, voice_player_play_msg, NULL);
    voice_msg_sub(VOICE_MSG_WAKEUP_KEYWORD, voice_player_play_msg, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_CLOUD_AUTH_SUCCESS, voice_player_play_msg, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_CLOUD_AUTH_FAILED, voice_player_play_msg, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_DISCONNECTED, voice_player_play_msg, NULL);
    voice_msg_sub(VOICE_MSG_WIFI_DISCONNECTED, voice_player_play_msg, NULL);
    voice_msg_sub(VOICE_MSG_WIFI_CONNECTED, voice_player_play_msg, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_TTS_URL, voice_player_play_msg, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_AUDIO_ITEM, voice_player_audio_item, NULL);

    voice_msg_sub(VOICE_MSG_PLAY_CONTROL_PLAY, voice_player_play_control, NULL);
    voice_msg_sub(VOICE_MSG_PLAY_CONTROL_PAUSE, voice_player_play_control, NULL);
    voice_msg_sub(VOICE_MSG_PLAY_CONTROL_NEXT, voice_player_play_control, NULL);
    voice_msg_sub(VOICE_MSG_PLAY_CONTROL_PREVIOUS, voice_player_play_control, NULL);
    voice_msg_sub(VOICE_MSG_PLAY_CONTROL_REPLAY, voice_player_play_control, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_MCP_CHAT_EXIT, voice_player_mcp_chat_exit, NULL);

    alarm_ring_init(voice_alarm_ring_play_once, voice_alarm_ring_force_stop);
    player_mgr_register_status_cb(TTS, on_tts_play_status, NULL);
    player_mgr_register_status_cb(LOCAL, on_local_play_status, NULL);
}

static int voice_player_init(void)
{
    LISA_LOGI(TAG, "voice_player_init");

    voice_msg_sub(VOICE_MSG_PLATFORM_READY, voice_player_ready, NULL);

    return 0;
}

SYS_INIT(voice_player_init, SYS_INIT_LEVEL_PRE_APPLICATION, 50);

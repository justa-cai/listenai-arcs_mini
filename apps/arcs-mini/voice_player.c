#include <string.h>
#include <stdio.h>

#include "sys_init.h"
#include "voice_msg.h"
#include "app_datas.h"

#include "tone.h"
#include "app_tone.h"
#include "lisa_time.h"
#include "player_mgr.h"
#include "lisa_player.h"
#include "service_alarm.h"
#include "alarm_ring.h"

#define TAG "platform"
#include "lisa_log.h"

static bool s_disconnect_tone_played = false;
static bool s_content_hold_for_tts = false;

static uint32_t xorshift32(void)
{
    static uint32_t state = 0;
    if (state == 0) {
        state = lisa_rand32() | 1;
    }
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

/* 闹钟播放阶段管理 */
typedef enum {
    ALARM_PLAY_PHASE_TONE_FIRST = 0,  // 首次播放默认提示音
    ALARM_PLAY_PHASE_TTS,              // 播放云端TTS
    ALARM_PLAY_PHASE_TONE_LOOP,        // 循环播放默认提示音
} alarm_play_phase_t;

static struct {
    alarm_play_phase_t phase;
    char text[128];
} s_alarm_play_ctx = {0};

static char *voice_player_get_wakeup_tone_url(void)
{
    static bool s_wakeup_tone_inited = false;
    static uint16_t s_wakeup_tone_ids[5] = {0};
    static uint8_t s_wakeup_tone_cnt = 0;
    static bool s_last_wakeup_tone_valid = false;
    static uint16_t s_last_wakeup_tone_id = TONE_ID_0;

    
    if (!s_wakeup_tone_inited) {
        for (uint16_t tone_id = TONE_ID_0; tone_id <= TONE_ID_4; tone_id++) {
            if (app_tone_get_url(tone_id) != NULL) {
                s_wakeup_tone_ids[s_wakeup_tone_cnt++] = tone_id;
            }
        }

        if (s_wakeup_tone_cnt == 0) {
            s_wakeup_tone_ids[s_wakeup_tone_cnt++] = TONE_ID_0;
        }

        s_wakeup_tone_inited = true;
        LOGI("wakeup tone candidates count: %u", (unsigned int)s_wakeup_tone_cnt);
    }

    uint16_t tone_id = s_wakeup_tone_ids[0];
    if (s_wakeup_tone_cnt > 1) {
        uint8_t attempts = 0;
        do {
            tone_id = s_wakeup_tone_ids[xorshift32() % s_wakeup_tone_cnt];
        } while (s_last_wakeup_tone_valid && tone_id == s_last_wakeup_tone_id && ++attempts < 8);
    }

    LOGI("Selected wakeup tone id: %u", tone_id);

    char *tone_url = app_tone_get_url(tone_id);
    if (tone_url == NULL) {
        tone_id = TONE_ID_0;
        tone_url = app_tone_get_url(tone_id);
    }

    if (tone_url != NULL) {
        s_last_wakeup_tone_id = tone_id;
        s_last_wakeup_tone_valid = true;
    }

    return tone_url;
}

static void voice_player_play_wakeup_tone(void)
{
    char *tone_url = voice_player_get_wakeup_tone_url();
    if (tone_url == NULL) {
        LOGE("wakeup tone url is null");
        return;
    }

    player_mgr_play(AIP, tone_url, 0);
}

static void hold_content_until_tts_playing(void)
{
    if (!s_content_hold_for_tts) {
        return;
    }

    if (!player_mgr_is_playing(TTS) && player_mgr_is_playing(CONTENT)) {
        LOGI("re-pause content while waiting tts start");
        player_mgr_pause_temporary(CONTENT);
    }
}

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
            voice_player_play_wakeup_tone();
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
            voice_player_play_wakeup_tone();
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
            break;
        }
        

        LOGI("Ready to play tts url: %s", (char *)data);
        player_mgr_play(TTS, (char *)data, 0);

    } break;
    case VOICE_MSG_CLOUD_IAT_UPDATE: {
        if (data == NULL || len == 0 || ((char *)data)[0] == '\0') {
            break;
        }

        bool tts_playing = player_mgr_is_playing(TTS);
        bool content_playing = player_mgr_is_playing(CONTENT);
        if (!tts_playing && !content_playing) {
            break;
        }

        s_content_hold_for_tts = true;

        if (tts_playing) {
            LOGI("stop tts due to valid iat update");
            player_mgr_stop(TTS);
        }

        if (content_playing) {
            LOGI("pause content due to valid iat update");
            if (player_mgr_pause_temporary(CONTENT) == 0) {
            }
        }
    } break;
    case VOICE_MSG_CLOUD_SESSION_FINISHED: {
        s_content_hold_for_tts = false;
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
        s_content_hold_for_tts = false;
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

static void on_content_focus_status(int player_id, focus_state_e state, int by_which, void *arg)
{
    (void)player_id;
    (void)by_which;
    (void)arg;

    if (state == FOREGROUND) {
        hold_content_until_tts_playing();
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
    s_content_hold_for_tts = false;
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
    voice_msg_sub(VOICE_MSG_CLOUD_IAT_UPDATE, voice_player_play_msg, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_SESSION_FINISHED, voice_player_play_msg, NULL);
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
    player_mgr_register_focus_cb(CONTENT, on_content_focus_status, NULL);
}

static int voice_player_init(void)
{
    LISA_LOGI(TAG, "voice_player_init");

    voice_msg_sub(VOICE_MSG_PLATFORM_READY, voice_player_ready, NULL);

    return 0;
}

SYS_INIT(voice_player_init, SYS_INIT_LEVEL_PRE_APPLICATION, 50);

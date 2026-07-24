#include <stdio.h>
#include <string.h>

#define TAG "voice_player_alarm"
#include "lisa_log.h"
#include "sysheap.h"
#include "sysutils.h"

#include "alarm_ring.h"
#include "app_tone.h"
#include "tone.h"
#include "voice_cloud.h"
#include "voice_msg.h"
#include "voice_player/voice_player_tts.h"
#include "voice_player/tone_control/voice_player_tone.h"

#include "voice_player_alarm.h"

/*
 * 闹钟播放编排：首轮本地提示音 -> 可选云端 TTS -> 本地提示音循环。
 * alarm_ring 负责响铃生命周期，本模块只维护单次响铃的播放阶段。
 */

/* ==================== 私有配置、类型与状态 ==================== */

#define ALARM_TTS_BUF_SIZE 512

typedef enum {
    ALARM_PLAY_PHASE_TONE_FIRST = 0,
    ALARM_PLAY_PHASE_TTS,
    ALARM_PLAY_PHASE_TONE_LOOP,
} alarm_play_phase_t;

typedef enum {
    ALARM_PLAYBACK_NONE = 0,
    ALARM_PLAYBACK_TONE,
    ALARM_PLAYBACK_TTS,
} alarm_playback_source_t;

static struct {
    alarm_play_phase_t phase;
    alarm_playback_source_t waiting_for;
    char text[ALARM_TTS_BUF_SIZE];
} s_alarm_play_ctx __psram_bss__;

/* ==================== 内部工具：播放阶段管理 ==================== */

static void voice_player_alarm_reset(void)
{
    s_alarm_play_ctx.phase = ALARM_PLAY_PHASE_TONE_FIRST;
    s_alarm_play_ctx.waiting_for = ALARM_PLAYBACK_NONE;
    memset(s_alarm_play_ctx.text, 0, sizeof(s_alarm_play_ctx.text));
}

static void voice_player_alarm_play_tone(void)
{
    s_alarm_play_ctx.waiting_for = ALARM_PLAYBACK_TONE;
    voice_player_play_alarm_tone_url(app_tone_get_url(TONE_ID_94));
}

static void voice_player_alarm_save_text(const char *text)
{
    if (text == NULL) {
        return;
    }

    strncpy(s_alarm_play_ctx.text, text, sizeof(s_alarm_play_ctx.text) - 1);
    s_alarm_play_ctx.text[sizeof(s_alarm_play_ctx.text) - 1] = '\0';
}

static void voice_player_alarm_fallback_to_tone_loop(void)
{
    s_alarm_play_ctx.phase = ALARM_PLAY_PHASE_TONE_LOOP;
    voice_player_alarm_play_tone();
}

static void voice_player_alarm_play_tts(void)
{
    char *tts_text;

    if (!voice_cloud_is_connected() || s_alarm_play_ctx.text[0] == '\0') {
        LOGI("alarm TTS skipped: no cloud or no text");
        voice_player_alarm_fallback_to_tone_loop();
        return;
    }

    tts_text = lisa_mem_alloc(ALARM_TTS_BUF_SIZE);
    if (tts_text == NULL) {
        LOGW("alarm TTS alloc failed, fallback to tone loop");
        voice_player_alarm_fallback_to_tone_loop();
        return;
    }

    snprintf(tts_text, ALARM_TTS_BUF_SIZE,
             "你有一个 %s 的提醒, 请不要忘记哦!",
             s_alarm_play_ctx.text);
    s_alarm_play_ctx.waiting_for = ALARM_PLAYBACK_TTS;
    if (voice_cloud_tts_synth(tts_text) != 0) {
        LOGW("alarm TTS synth failed, fallback to tone loop");
        lisa_mem_free(tts_text);
        voice_player_alarm_fallback_to_tone_loop();
        return;
    }
    lisa_mem_free(tts_text);

    s_alarm_play_ctx.phase = ALARM_PLAY_PHASE_TONE_LOOP;
}

/* ==================== alarm_ring 回调 ==================== */

/* alarm_ring 每次需要播放一段音频时调用。 */
static void voice_player_alarm_play_once(const char *text)
{
    switch (s_alarm_play_ctx.phase) {
    case ALARM_PLAY_PHASE_TONE_FIRST:
        LOGI("alarm play phase: TONE_FIRST");
        voice_player_alarm_play_tone();
        s_alarm_play_ctx.phase = ALARM_PLAY_PHASE_TTS;
        voice_player_alarm_save_text(text);
        break;
    case ALARM_PLAY_PHASE_TTS:
        LOGI("alarm play phase: TTS");
        voice_player_alarm_play_tts();
        break;
    case ALARM_PLAY_PHASE_TONE_LOOP:
        LOGI("alarm play phase: TONE_LOOP");
        voice_player_alarm_play_tone();
        break;
    default:
        voice_player_alarm_reset();
        voice_player_alarm_play_tone();
        s_alarm_play_ctx.phase = ALARM_PLAY_PHASE_TTS;
        voice_player_alarm_save_text(text);
        break;
    }
}

/* alarm_ring 被外部强停时，只停止闹钟当前正在等待的播放源。 */
static void voice_player_alarm_force_stop(void)
{
    alarm_playback_source_t active_source = s_alarm_play_ctx.waiting_for;

    /* 先清状态，防止 stop 产生的完成事件被当成自然播放完成。 */
    voice_player_alarm_reset();

    switch (active_source) {
    case ALARM_PLAYBACK_TONE:
        voice_player_tone_stop();
        break;
    case ALARM_PLAYBACK_TTS:
        voice_player_tts_stop();
        break;
    case ALARM_PLAYBACK_NONE:
    default:
        break;
    }
}

/* 只接受当前闹钟阶段正在等待的自然完成事件，忽略其他播放器和过期事件。 */
static void voice_player_alarm_on_playback_completed(void *unused,
                                                     uint32_t msg_id,
                                                     void *data,
                                                     uint32_t len,
                                                     void *user_data)
{
    alarm_playback_source_t completed_source;

    (void)unused;
    (void)user_data;

    switch (msg_id) {
    case VOICE_MSG_PLAYER_TONE_COMPLETED: {
        const voice_player_tone_completed_t *completed = data;

        if (completed == NULL || len < sizeof(*completed) ||
            completed->source != VOICE_PLAYER_TONE_SOURCE_ALARM) {
            return;
        }
        completed_source = ALARM_PLAYBACK_TONE;
        break;
    }
    case VOICE_MSG_PLAYER_TTS_COMPLETED:
        completed_source = ALARM_PLAYBACK_TTS;
        break;
    default:
        return;
    }

    if (!alarm_ring_is_active() ||
        s_alarm_play_ctx.waiting_for != completed_source) {
        return;
    }

    s_alarm_play_ctx.waiting_for = ALARM_PLAYBACK_NONE;
    alarm_ring_notify_playback_complete();
}

/* ==================== 对外 API ==================== */

int voice_player_alarm_init(void)
{
    voice_player_alarm_reset();

    if (alarm_ring_init(voice_player_alarm_play_once,
                        voice_player_alarm_force_stop) != 0) {
        return -1;
    }
    if (voice_msg_sub(VOICE_MSG_PLAYER_TONE_COMPLETED,
                      voice_player_alarm_on_playback_completed,
                      NULL) != 0) {
        return -1;
    }
    if (voice_msg_sub(VOICE_MSG_PLAYER_TTS_COMPLETED,
                      voice_player_alarm_on_playback_completed,
                      NULL) != 0) {
        return -1;
    }

    return 0;
}

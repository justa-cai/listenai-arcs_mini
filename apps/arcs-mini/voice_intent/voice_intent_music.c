#define TAG "intent_music"

#include "voice_intent_music.h"

#include "lisa_log.h"
#include "voice_msg.h"
#include "app_player.h"
#include "voice_player_comm.h"
#include "voice_music_list.h"
#include "voice_intent_mgr.h"
#include "FreeRTOS.h"
#include "timers.h"

/* ---- MUSIC intent hooks ----

   Hook 调用时机（由 voice_intent_mgr 的栈操作自动触发）：

     on_enter      MUSIC 首次入栈并成为栈顶 → 开始播放
     on_preempted  MUSIC 被更高优先级意图压下 → 暂停
     on_resumed    MUSIC 重新变成栈顶 → 延迟恢复播放
     on_exit       MUSIC 出栈 → 停止播放

   on_resumed 使用定时器延迟恢复（~30ms），若在窗口内被 on_preempted
   则取消定时器，避免快速出/入栈（如 TTS_STOPED pop VOICE_SESSION 后
   SESSION_STARTING 立刻 push 新 session）导致的短暂音乐播放。

*/

#define MUSIC_RESUME_DELAY_MS 30

/* ---- 状态 / API ---- */

/* 用户通过 MCP 主动暂停时置 true。
 * on_resumed/on_enter 检测到此标记后跳过自动恢复但不清除标记，
 * 确保无论经历多少次抢占/恢复周期，音乐都保持暂停，直到用户显式 PLAY。
 * 仅 on_exit 和用户显式 PLAY/NEXT/PREVIOUS/REPLAY 时清除。 */
static bool s_user_paused = false;

static TimerHandle_t s_resume_timer = NULL;

void voice_intent_music_set_user_paused(bool paused)
{
    s_user_paused = paused;
}

bool voice_intent_music_is_user_paused(void)
{
    return s_user_paused;
}

/* ---- 工具函数 ---- */

static void music_intent_play_current(void)
{
    music_item_t track;
    if (voice_music_list_get_current(&track) == 0) {
        LOGI("MUSIC: play [%s]", track.m_name);
        if (voice_player_play_music_url(track.m_url) != APP_PLAYER_OK) {
            LOGW("MUSIC: play failed [%s]", track.m_name);
        }
    } else {
        LOGW("MUSIC: no current track to play");
    }
}

static void music_resume_timer_cb(TimerHandle_t xTimer)
{
    (void)xTimer;

    if (s_user_paused) {
        LOGI("MUSIC resume timer: user paused, skip");
        return;
    }
    if (app_player_get_state(music_player) == APP_PLAYER_STATE_IDLE) {
        LOGI("MUSIC resume timer: player idle, init from current track");
        music_intent_play_current();
        return;
    }
    LOGI("MUSIC resume timer: resume music");
    app_player_resume(music_player);
}

/* ---- intent hook 实现 ---- */

static void music_intent_on_enter(void)
{
    if (s_user_paused) {
        LOGI("MUSIC on_enter: user paused, skip");
        return;
    }
    if (app_player_get_state(music_player) == APP_PLAYER_STATE_PLAYING) {
        LOGI("MUSIC on_enter: already playing, skip");
        return;
    }
    music_intent_play_current();
}

static void music_intent_on_preempted(void)
{
    LOGI("MUSIC on_preempted: pause music");
    xTimerStop(s_resume_timer, 0);
    app_player_pause(music_player);
}

/* 半双工模式下 TTS 结束后 cloud 模块会立刻重启 continuous session，
 * 导致 VOICE_SESSION 快速出栈→入栈（TTS_STOPED pop → SESSION_STARTING push），
 * MUSIC 在两次栈操作间隙瞬间 resume 又马上被 preempt，用户会听到短暂音乐声。
 * 用定时器延迟 resume，若在窗口内被 on_preempted 则取消定时器。 */
static void music_intent_on_resumed(void)
{
    if (s_user_paused) {
        LOGI("MUSIC on_resumed: user paused, skip resume");
        return;
    }
    LOGI("MUSIC on_resumed: defer resume by %d ms", MUSIC_RESUME_DELAY_MS);
    xTimerStart(s_resume_timer, 0); 
}

static void music_intent_on_exit(void)
{
    s_user_paused = false;
    xTimerStop(s_resume_timer, 0);
    if (app_player_get_state(music_player) == APP_PLAYER_STATE_IDLE) {
        LOGI("MUSIC on_exit: already idle, skip");
        return;
    }
    LOGI("MUSIC on_exit: stop music");
    app_player_stop(music_player);
}

/* ---- ebus 事件处理 ---- */

static void on_music_playing(void *unused, uint32_t msg_id,
                              void *data, uint32_t len, void *user_data)
{
    (void)unused; (void)msg_id; (void)data; (void)len; (void)user_data;
    voice_intent_push(INTENT_MUSIC);
}

/* ---- 注册 ---- */

int voice_intent_music_register(void)
{
    if (s_resume_timer == NULL) {
        s_resume_timer = xTimerCreate("mus.resume",
                                      pdMS_TO_TICKS(MUSIC_RESUME_DELAY_MS),
                                      pdFALSE, NULL, music_resume_timer_cb);
        if (s_resume_timer == NULL) {
            LISA_LOGE(TAG, "failed to create resume timer");
            return -1;
        }
    }

    voice_msg_sub(VOICE_MSG_PLAYER_MUSIC_PLAYING, on_music_playing, NULL);

    return voice_intent_register_ops(&(voice_intent_ops_t){
        .type = INTENT_MUSIC,
        .on_enter = music_intent_on_enter,
        .on_preempted = music_intent_on_preempted,
        .on_resumed = music_intent_on_resumed,
        .on_exit = music_intent_on_exit,
    });
}

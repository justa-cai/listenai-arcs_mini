#define TAG "alarm_ring"

#include "alarm_ring.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "app_cloud.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "evs_utils.h"
#include "tone.h"
#include "lisa_timer.h"
#include "proc_mgr.h"
#include "audio_out.h"
#include "alarm.h"
#include "assistant_view.h"
#include "player/tts_player.h"
#include "app_player.h"
#include "recognizer.h"

extern tts_player_t *get_tts_player(void);

static sound_player_t *s_alarm_sound_player = NULL;
static lisa_timer_t *s_alarm_ring_timer = NULL;
static uint32_t s_alarm_ring_ticks = 0;
static bool s_alarm_ring_active = false;
static bool s_alarm_ring_url_ready = false;
static char s_alarm_tts_url[AUIDO_OUT_URL_LEN] = {0};
static bool s_alarm_timer_waiting = false;
static bool s_alarm_expect_tts = false;
static bool s_alarm_tts_playing_seen = false;
static int s_alarm_timer_wait_retry = 0;

#define ALARM_RING_REPEAT_MS 6000
#define ALARM_RING_DURATION_MS 60000
#define ALARM_TTS_DELAY_MS 1000
#define ALARM_TTS_MAX_RETRY 60
#define ALARM_TIMER_WAIT_MAX_RETRY 20

typedef struct {
    char *text;
    int retry;
} alarm_tts_ctx_t;

static bool alarm_tts_should_delay(void);

bool alarm_ring_is_active(void)
{
    return s_alarm_ring_active;
}

void alarm_ring_stop(void)
{
    s_alarm_ring_active = false;
    s_alarm_ring_url_ready = false;
    s_alarm_tts_url[0] = '\0';
    s_alarm_timer_waiting = false;
    s_alarm_expect_tts = false;
    s_alarm_tts_playing_seen = false;
    s_alarm_timer_wait_retry = 0;
    if (s_alarm_ring_timer) {
        lisa_timer_stop(s_alarm_ring_timer);
    }
    proc_mgr_expect_tts_url(false);
    if (s_alarm_sound_player && s_alarm_sound_player->stop) {
        s_alarm_sound_player->stop(s_alarm_sound_player, false);
    }
    assistant_view_notify_alarm_update(ls_alarm_count_get() > 0);
}

static void alarm_ring_tick(void *arg)
{
    (void)arg;
    if (!s_alarm_ring_active) {
        return;
    }

    s_alarm_ring_ticks++;
    if ((uint64_t)s_alarm_ring_ticks * ALARM_RING_REPEAT_MS >= ALARM_RING_DURATION_MS) {
        alarm_ring_stop();
        return;
    }

    if (s_alarm_sound_player) {
        if (s_alarm_ring_url_ready) {
            audio_out_t item = {0};
            strncpy(item.m_url, s_alarm_tts_url, sizeof(item.m_url) - 1);
            listen_soundplayer_play_fs(s_alarm_sound_player, &item, 1, false);
        } else {
            listen_soundplayer_play(s_alarm_sound_player, TONE_ID_94, 0);
        }
    }

    if (s_alarm_ring_timer) {
        lisa_timer_change_period(s_alarm_ring_timer, ALARM_RING_REPEAT_MS);
        lisa_timer_start(s_alarm_ring_timer);
    }
}

static void alarm_ring_timer_start(void)
{
    if (!s_alarm_ring_timer) {
        s_alarm_ring_timer = lisa_timer_create(ALARM_RING_REPEAT_MS, alarm_ring_tick, NULL);
    } else {
        lisa_timer_change_period(s_alarm_ring_timer, ALARM_RING_REPEAT_MS);
    }
    lisa_timer_start(s_alarm_ring_timer);
}


static void alarm_ring_send_tts(const char *msg)
{
    if (msg && msg[0] != '\0') {
        proc_mgr_expect_tts_url(true);
        extern void enter_audio_idle(void);
        enter_audio_idle();
        app_cloud_tts(msg);
        return;
    }

    if (s_alarm_sound_player) {
        listen_soundplayer_play(s_alarm_sound_player, TONE_ID_94, 0);
    }
}

static int alarm_ring_runnable(void *arg)
{
    alarm_tts_ctx_t *ctx = (alarm_tts_ctx_t *)arg;
    bool has_text = (ctx && ctx->text && ctx->text[0] != '\0');

    if (ctx) {
        if (has_text && alarm_tts_should_delay()) {
            ctx->retry++;
            LISA_LOGI(TAG, "Alarm TTS busy, wait (retry=%d)", ctx->retry);
            if (evs_handler_post_runnable_delay(alarm_ring_runnable, ctx, ALARM_TTS_DELAY_MS) == 0) {
                return 0;
            }
            LISA_LOGW(TAG, "Delay post failed, sending alarm TTS immediately");
        }
        alarm_ring_send_tts(ctx->text);
        lisa_mem_free(ctx->text);
        lisa_mem_free(ctx);
    } else {
        alarm_ring_send_tts(NULL);
    }

    return 0;
}

static void alarm_ring_prepare_send_tts(char *msg)
{
    alarm_tts_ctx_t *ctx = lisa_mem_calloc(1, sizeof(alarm_tts_ctx_t));
    if (!ctx) {
        LISA_LOGW(TAG, "alarm tts ctx alloc failed, send without delay");
        alarm_ring_send_tts(msg);
        lisa_mem_free(msg);
        return;
    }

    ctx->text = msg;
    ctx->retry = 0;
    evs_handler_post_runnable(alarm_ring_runnable, ctx);
}


static bool alarm_tts_should_delay(void)
{
    tts_player_t *tts_player = get_tts_player();
    if (!tts_player) {
        return false;
    }
    return (tts_player->m_play_state == APP_PLAYER_PREPARING ||
            tts_player->m_play_state == PLAYER_EVT_PLAYING ||
            tts_player->m_play_state == PLAYER_EVT_PREPARED);
}

static bool alarm_ring_should_delay_timer_start(void)
{
    if (s_alarm_expect_tts) {
        if (alarm_tts_should_delay()) {
            s_alarm_tts_playing_seen = true;
            return true;
        }
        if (!s_alarm_tts_playing_seen && s_alarm_timer_wait_retry++ < ALARM_TIMER_WAIT_MAX_RETRY) {
            return true;
        }
        return false;
    }

    return alarm_tts_should_delay();
}

static int alarm_ring_prepare_timer_start(void *arg)
{
    (void)arg;
    if (!s_alarm_ring_active) {
        s_alarm_timer_waiting = false;
        return 0;
    }

    if (alarm_ring_should_delay_timer_start()) {
        if (evs_handler_post_runnable_delay(alarm_ring_prepare_timer_start, NULL, ALARM_TTS_DELAY_MS) == 0) {
            return 0;
        }
        LISA_LOGW(TAG, "Delay alarm timer start failed, start immediately");
        if (s_alarm_expect_tts && !s_alarm_tts_playing_seen) {
            LISA_LOGW(TAG, "Alarm TTS not started, start timer anyway");
        }
    }

    s_alarm_ring_ticks = 0;
    alarm_ring_timer_start();
    s_alarm_timer_waiting = false;

    return 0;
}

// 构造闹铃tts需要的文本
static char *alarm_ring_build_tts_msg(const uint8_t *text)
{
    const char fmt[] = "您有一个关于 %s 的提醒，请不要忘记噢";
    size_t text_len = 0;

    if (text) {
        text_len = strnlen((const char *)text, LS_ALARM_TEXT_MAX_LEN - 1);
    }

    if (!text || text_len == 0) {
        return NULL;
    }

    char *msg = lisa_mem_alloc(sizeof(fmt) + text_len + 1);
    if (!msg) {
        return NULL;
    }

    snprintf(msg, sizeof(fmt) + text_len + 1, fmt, (const char *)text);
    return msg;
}


// 闹钟触发后的回调函数
void alarm_ring_on_alarm(uint64_t timestamp, const uint8_t *text)
{
    // 闹铃需要播报的文本
    char *msg = alarm_ring_build_tts_msg(text);


    if (s_alarm_ring_timer) {
        lisa_timer_stop(s_alarm_ring_timer);
    }

    // 检查 tts 是否正在播放音频或准备播放音频
    s_alarm_ring_active = true;
    s_alarm_ring_url_ready = false;
    s_alarm_tts_url[0] = '\0';
    s_alarm_expect_tts = (msg != NULL);
    s_alarm_tts_playing_seen = false;
    s_alarm_timer_wait_retry = 0;
    if (!s_alarm_timer_waiting) {
        s_alarm_timer_waiting = true;
        if (evs_handler_post_runnable(alarm_ring_prepare_timer_start, NULL) != 0) {
            LISA_LOGW(TAG, "alarm timer start post failed, start immediately");
            s_alarm_timer_waiting = false;
            s_alarm_ring_ticks = 0;
            alarm_ring_timer_start();
        }
    }


    // 等待可播报后发送 tts 请求
    alarm_ring_prepare_send_tts(msg);
}


// 保存发送 tts 请求后响应的 url
void alarm_ring_on_tts_url(const char *url)
{
    if (!s_alarm_ring_active || !url || url[0] == '\0') {
        LISA_LOGW(TAG, "alarm_ring_on_tts_url ignored, active=%d url=%p",
                  s_alarm_ring_active, url);
        return;
    }
    strncpy(s_alarm_tts_url, url, sizeof(s_alarm_tts_url) - 1);
    s_alarm_tts_url[sizeof(s_alarm_tts_url) - 1] = '\0';
    s_alarm_ring_url_ready = true;
    LISA_LOGI(TAG, "alarm_ring_on_tts_url set: %s", s_alarm_tts_url);
}


void alarm_ring_init(sound_player_t *sound_player)
{
    s_alarm_sound_player = sound_player;
    register_tts_url_callback(alarm_ring_on_tts_url);
}

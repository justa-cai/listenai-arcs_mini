#define TAG "alarm_ring"

#include "alarm_ring.h"

#include <string.h>

#include "lisa_timer.h"
#include "lisa_log.h"
#include "voice_msg.h"
#include "service_alarm.h"

#ifndef CONFIG_ALARM_RING_DURATION_MS
#define CONFIG_ALARM_RING_DURATION_MS 60000
#endif
#define ALARM_RING_DURATION_MS CONFIG_ALARM_RING_DURATION_MS

typedef enum {
    ALARM_RING_STATE_IDLE = 0,
    ALARM_RING_STATE_RINGING,
} alarm_ring_state_t;

typedef struct {
    alarm_ring_state_t state;
    char text[128];
    lisa_timer_t *timeout_timer;
    alarm_ring_play_once_cb_t play_cb;
    alarm_ring_force_stop_cb_t stop_cb;
    bool inited;
} alarm_ring_ctx_t;

static alarm_ring_ctx_t s_alarm_ring_ctx = {0};

static void alarm_ring_timeout_cb(struct lisa_timer *timer)
{
    (void)timer;

    LISA_LOGI(TAG, "alarm ring timeout 60s");
    alarm_ring_stop();
}

static void alarm_ring_on_wakeup(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)data;
    (void)len;
    (void)user_data;

    alarm_ring_stop();
}

static void alarm_ring_on_button_change(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)data;
    (void)len;
    (void)user_data;

    alarm_ring_stop();
}

static void alarm_ring_on_trigger(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)len;
    (void)user_data;

    const struct service_alarm *alarm = (const struct service_alarm *)data;
    if (!alarm) {
        return;
    }

    /* Stop current voice session first to avoid ASR/TTS racing with alarm ringing. */
    voice_msg_pub(VOICE_MSG_CLOUD_MCP_CHAT_EXIT, NULL, 0);
    
    alarm_ring_start((const char *)alarm->text);
}

int alarm_ring_init(alarm_ring_play_once_cb_t play_cb, alarm_ring_force_stop_cb_t stop_cb)
{
    s_alarm_ring_ctx.play_cb = play_cb;
    s_alarm_ring_ctx.stop_cb = stop_cb;

    if (s_alarm_ring_ctx.timeout_timer == NULL) {
        s_alarm_ring_ctx.timeout_timer = lisa_timer_create(ALARM_RING_DURATION_MS, alarm_ring_timeout_cb, NULL);
    }

    if (!s_alarm_ring_ctx.inited) {
        voice_msg_sub(VOICE_MSG_ALARM_TRIGGER, alarm_ring_on_trigger, NULL);
        voice_msg_sub(VOICE_MSG_WAKEUP_KEYWORD, alarm_ring_on_wakeup, NULL);
        voice_msg_sub(VOICE_MSG_WAKEUP_COMMAND, alarm_ring_on_wakeup, NULL);
        voice_msg_sub(VOICE_MSG_BUTTON_CHANGE, alarm_ring_on_button_change, NULL);
        s_alarm_ring_ctx.inited = true;
    }

    return 0;
}

int alarm_ring_start(const char *text)
{
    alarm_ring_stop();

    s_alarm_ring_ctx.state = ALARM_RING_STATE_RINGING;

    memset(s_alarm_ring_ctx.text, 0, sizeof(s_alarm_ring_ctx.text));
    if (text) {
        strncpy(s_alarm_ring_ctx.text, text, sizeof(s_alarm_ring_ctx.text) - 1);
    }

    if (s_alarm_ring_ctx.timeout_timer) {
        lisa_timer_change_period(s_alarm_ring_ctx.timeout_timer, ALARM_RING_DURATION_MS);
        lisa_timer_start(s_alarm_ring_ctx.timeout_timer);
    }

    if (s_alarm_ring_ctx.play_cb) {
        s_alarm_ring_ctx.play_cb(s_alarm_ring_ctx.text);
    }

    return 0;
}

void alarm_ring_stop(void)
{
    if (s_alarm_ring_ctx.state != ALARM_RING_STATE_RINGING) {
        return;
    }

    s_alarm_ring_ctx.state = ALARM_RING_STATE_IDLE;

    if (s_alarm_ring_ctx.timeout_timer) {
        lisa_timer_stop(s_alarm_ring_ctx.timeout_timer);
    }

    if (s_alarm_ring_ctx.stop_cb) {
        s_alarm_ring_ctx.stop_cb();
    }
}

void alarm_ring_notify_playback_complete(void)
{
    if (s_alarm_ring_ctx.state != ALARM_RING_STATE_RINGING) {
        return;
    }

    if (s_alarm_ring_ctx.play_cb) {
        s_alarm_ring_ctx.play_cb(s_alarm_ring_ctx.text);
    }
}

bool alarm_ring_is_active(void)
{
    return s_alarm_ring_ctx.state == ALARM_RING_STATE_RINGING;
}

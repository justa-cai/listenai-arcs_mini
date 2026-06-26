#define TAG "alarm_ring"

#include "alarm_ring.h"

#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "lisa_kv.h"
#include "lisa_timer.h"
#include "lisa_log.h"
#include "voice_msg.h"
#include "sysutils.h"

#include "kv_user.h"
#include "service_alarm.h"
#include "service_volume.h"
#include "alarm.h"

#ifndef CONFIG_ALARM_RING_DURATION_MS
#define CONFIG_ALARM_RING_DURATION_MS 60000
#endif
#define ALARM_RING_DURATION_MS CONFIG_ALARM_RING_DURATION_MS
#define ALARM_RING_TEMP_VOLUME_DEFAULT 60

typedef enum {
    ALARM_RING_STATE_IDLE = 0,
    ALARM_RING_STATE_RINGING,
} alarm_ring_state_t;

typedef struct {
    alarm_ring_state_t state;
    char text[LS_ALARM_TEXT_MAX_LEN];
    lisa_timer_t *timeout_timer;
    alarm_ring_play_once_cb_t play_cb;
    alarm_ring_force_stop_cb_t stop_cb;
    bool inited;
} alarm_ring_ctx_t;

static alarm_ring_ctx_t s_alarm_ring_ctx __psram_bss__ = {0};

static void alarm_ring_set_text(const char *text)
{
    memset(s_alarm_ring_ctx.text, 0, sizeof(s_alarm_ring_ctx.text));
    if (text) {
        strncpy(s_alarm_ring_ctx.text, text, sizeof(s_alarm_ring_ctx.text) - 1);
    }
}

static int alarm_ring_get_temp_volume(void)
{
    int volume = ALARM_RING_TEMP_VOLUME_DEFAULT;

    if (lisa_kv_get_int(KV_KEY_USER_ALARM_RING_TEMP_VOLUME, &volume) != 0) {
        lisa_kv_set_int(KV_KEY_USER_ALARM_RING_TEMP_VOLUME, volume);
    }

    return volume;
}

static void alarm_ring_timeout_cb(struct lisa_timer *timer)
{
    (void)timer;

    LISA_LOGI(TAG, "alarm ring timeout, auto-stopping alarm");

    // 停止响铃
    alarm_ring_stop();

    // 发送消息到业务线程处理闹钟后续操作，避免在定时器回调中执行 HTTP 请求
    voice_msg_pub(VOICE_MSG_ALARM_PROCESS_NEXT, NULL, 0);
}

static void alarm_ring_on_wakeup(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)data;
    (void)len;
    (void)user_data;

    LISA_LOGI(TAG, "wakeup detected while alarm ringing, stopping alarm");



    // 用户唤醒，停止闹钟并处理下次触发（等同于双击关闭）
    if (alarm_ring_is_active()) {
        alarm_ring_stop();
        extern void alarm_handle_stop_and_next(void);
        alarm_handle_stop_and_next();
        return ;
    }
    // 停止响铃
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


    /* 如果当前有闹钟正在响铃，重启响铃播放链路但不发布 ALARM_STOPPED。
     * 第二个闹钟仍然需要重新走 TONE_FIRST -> TTS -> TONE_LOOP；
     * 直接 alarm_ring_stop() 会 pop ALARM intent，导致底层 MUSIC 短暂恢复。 */
    if (alarm_ring_is_active()) {
        LISA_LOGI(TAG, "new alarm triggered while another is ringing, restart ring content");
        alarm_ring_set_text((const char *)alarm->text);
        if (s_alarm_ring_ctx.timeout_timer) {
            lisa_timer_change_period(s_alarm_ring_ctx.timeout_timer, ALARM_RING_DURATION_MS);
            lisa_timer_start(s_alarm_ring_ctx.timeout_timer);
        }
        if (s_alarm_ring_ctx.stop_cb) {
            s_alarm_ring_ctx.stop_cb();
        }
        vTaskDelay(pdMS_TO_TICKS(200));
        if (s_alarm_ring_ctx.play_cb) {
            s_alarm_ring_ctx.play_cb(s_alarm_ring_ctx.text);
        }
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(200));
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
        s_alarm_ring_ctx.inited = true;
    }

    return 0;
}

int alarm_ring_start(const char *text)
{
    // 更新文本内容
    alarm_ring_set_text(text);

    // 如果已经在响铃状态，只重置定时器重新计时，不停止音频
    if (s_alarm_ring_ctx.state == ALARM_RING_STATE_RINGING) {
        LISA_LOGI(TAG, "alarm already ringing, resetting timer");
        if (s_alarm_ring_ctx.timeout_timer) {
            lisa_timer_change_period(s_alarm_ring_ctx.timeout_timer, ALARM_RING_DURATION_MS);
            lisa_timer_start(s_alarm_ring_ctx.timeout_timer);
        }
        return 0;
    }

    s_alarm_ring_ctx.state = ALARM_RING_STATE_RINGING;

    service_volume_set_temp(alarm_ring_get_temp_volume());

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

    voice_msg_pub(VOICE_MSG_ALARM_STOPPED, NULL, 0);

    service_volume_restore_from_kv();
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

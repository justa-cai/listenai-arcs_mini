#define TAG "intent_alarm"

#include "voice_intent_alarm.h"

#include "lisa_log.h"
#include "voice_msg.h"
#include "alarm_ring.h"
#include "alarm_handler.h"
#include "voice_intent_mgr.h"
#include "voice_player/voice_player_tts.h"

/* ==================== 状态 ====================
 *
 * 重入保护：on_exit 中调用 alarm_ring_stop() 会同步发布
 * VOICE_MSG_ALARM_STOPPED，on_alarm_stopped 又会尝试
 * voice_intent_pop(ALARM)，导致 on_exit 重入。s_exit_in_progress 阻断环路。 */

static bool s_exit_in_progress = false;

/* ==================== intent 钩子 ==================== */

static void alarm_intent_on_preempted(void)
{
    /* PHOTO_FLOW 优先级高于 ALARM，push PHOTO_FLOW 时会抢占 ALARM。
     * 停止闹钟响铃 + 处理后续（删除单次闹钟或安排下次贪睡）。
     * alarm_ring_stop() → VOICE_MSG_ALARM_STOPPED → on_alarm_stopped → pop */
    if (alarm_ring_is_active()) {
        LOGI("ALARM preempted, stop alarm ring");
        alarm_ring_stop();
        alarm_handle_stop_and_next();
    }
}

static void alarm_intent_on_exit(void)
{
    if (s_exit_in_progress) {
        return;
    }
    s_exit_in_progress = true;

    if (alarm_ring_is_active()) {
        LOGI("ALARM exit, stop alarm ring");
        alarm_ring_stop();
        alarm_handle_stop_and_next();
    }

    s_exit_in_progress = false;
}

/* ==================== ebus 事件处理 ==================== */

static void on_alarm_trigger(void *unused, uint32_t msg_id,
                              void *data, uint32_t len, void *user_data)
{
    (void)unused; (void)msg_id; (void)data; (void)len; (void)user_data;

    if (voice_intent_contains(INTENT_PHOTO_FLOW)) {
        LOGI("ALARM triggered during photo flow, pop PHOTO_FLOW first");
        voice_intent_pop(INTENT_PHOTO_FLOW);
        /* on_exit 已完成 TTS 收尾，这里发布 PREVIEW_EXIT 触发 UI 退出 +
         * mcp_tool 发错误 MCP 响应，完成拍照流的完整收尾。 */
        voice_msg_pub(VOICE_MSG_APP_CAMERA_PREVIEW_EXIT, NULL, 0);
    }

    if (voice_intent_contains(INTENT_VOICE_SESSION)) {
        voice_player_tts_stop();
        voice_intent_pop(INTENT_VOICE_SESSION);
    }

    voice_intent_push(INTENT_ALARM);
}

static void on_alarm_stopped(void *unused, uint32_t msg_id,
                              void *data, uint32_t len, void *user_data)
{
    (void)unused; (void)msg_id; (void)data; (void)len; (void)user_data;

    /* on_exit 已处理，跳过重入 */
    if (s_exit_in_progress) {
        return;
    }

    if (voice_intent_contains(INTENT_ALARM)) {
        voice_intent_pop(INTENT_ALARM);
    }
}

/* ==================== 注册 ==================== */

int voice_intent_alarm_register(void)
{
    voice_msg_sub(VOICE_MSG_ALARM_TRIGGER, on_alarm_trigger, NULL);
    voice_msg_sub(VOICE_MSG_ALARM_STOPPED, on_alarm_stopped, NULL);

    return voice_intent_register_ops(&(voice_intent_ops_t){
        .type = INTENT_ALARM,
        .on_preempted = alarm_intent_on_preempted,
        .on_exit = alarm_intent_on_exit,
    });
}

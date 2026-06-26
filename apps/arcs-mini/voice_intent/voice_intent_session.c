#define TAG "intent_session"

#include "voice_intent_session.h"

#include "lisa_log.h"
#include "app_player.h"
#include "voice_msg.h"
#include "voice_player_comm.h"
#include "voice_intent_mgr.h"
#include "FreeRTOS.h"
#include "timers.h"

#define SESSION_FINISH_POP_DELAY_MS 3000

/* ==================== 状态 ==================== */

static int s_session_generation = 0;
static TimerHandle_t s_session_finish_timer = NULL;
static bool s_session_finished = false;

/* ==================== intent 钩子 ==================== */

static void voice_session_intent_on_enter(void)
{
	LOGI("VOICE_SESSION on_enter");
}

/* 被更高优先级意图（PHOTO_FLOW / ALARM）抢占时，停止 TTS。
 * URL 的保存和重播由 PHOTO_FLOW（voice_intent_photo_flow）自行管理。 */
static void voice_session_intent_on_preempted(void)
{
	LOGI("VOICE_SESSION on_preempted: stop tts");
	app_player_stop(tts_player);
}

static void voice_session_intent_on_resumed(void)
{
	LOGI("VOICE_SESSION on_resumed");
}

static void voice_session_intent_on_exit(void)
{
	LOGI("VOICE_SESSION on_exit");
}

/* ==================== ebus 事件处理 ====================
 *
 * VOICE_SESSION 生命周期：
 *   push: SESSION_STARTING 时入栈（先清理上一个残留）
 *   pop:  TTS_STOPED 时出栈（TTS 正常播完）
 *         SESSION_FINISHED 兜底：若 TTS 从未激活，延迟 3s 出栈 */

static void session_finish_timer_cb(TimerHandle_t xTimer)
{
	int gen = (int)(uintptr_t)pvTimerGetTimerID(xTimer);

	if (gen != s_session_generation) {
		LOGI("session finish timer: generation changed, skip");
		return;
	}
	if (!voice_intent_contains(INTENT_VOICE_SESSION)) {
		return;
	}
	if (voice_player_tts_is_active()) {
		LOGI("session finish timer: tts active, skip pop");
		return;
	}
	LOGI("session finish timer: pop stale VOICE_SESSION");
	voice_intent_pop(INTENT_VOICE_SESSION);
}

static void on_cloud_session_starting(void *unused, uint32_t msg_id,
                                       void *data, uint32_t len, void *user_data)
{
	(void)unused; (void)msg_id; (void)data; (void)len; (void)user_data;

	s_session_generation++;
	s_session_finished = false;

	if (voice_intent_contains(INTENT_VOICE_SESSION)) {
		LOGI("cleanup stale VOICE_SESSION from previous session");
		app_player_stop(tts_player);
		voice_intent_pop(INTENT_VOICE_SESSION);
	}

	voice_intent_push(INTENT_VOICE_SESSION);
}

static void on_cloud_session_finished(void *unused, uint32_t msg_id,
                                       void *data, uint32_t len, void *user_data)
{
	(void)unused; (void)msg_id; (void)data; (void)len; (void)user_data;

	if (!voice_intent_contains(INTENT_VOICE_SESSION)) {
		return;
	}

	s_session_finished = true;

	if (voice_player_tts_is_active()) {
		return;
	}
	LOGI("session finished, tts inactive, arm pop timer %d ms",
	     SESSION_FINISH_POP_DELAY_MS);
	vTimerSetTimerID(s_session_finish_timer, (void *)(uintptr_t)s_session_generation);
	xTimerChangePeriod(s_session_finish_timer,
	                   pdMS_TO_TICKS(SESSION_FINISH_POP_DELAY_MS), 0);
}

static void on_tts_stoped(void *unused, uint32_t msg_id,
                           void *data, uint32_t len, void *user_data)
{
	(void)unused; (void)msg_id; (void)data; (void)len; (void)user_data;

	if (voice_intent_top() != INTENT_VOICE_SESSION) {
		return;
	}
	if (!s_session_finished) {
		return;
	}
	voice_intent_pop(INTENT_VOICE_SESSION);
}

/* MCP chat 退出 → 弹出 VOICE_SESSION 意图 */
static void on_cloud_mcp_chat_exit(void *unused, uint32_t msg_id,
                                    void *data, uint32_t len, void *user_data)
{
	(void)unused; (void)msg_id; (void)data; (void)len; (void)user_data;

	if (!voice_intent_contains(INTENT_VOICE_SESSION)) {
		return;
	}
	LOGI("mcp chat exit: pop INTENT_VOICE_SESSION");
	voice_intent_pop(INTENT_VOICE_SESSION);
}

/* ==================== 注册 ==================== */

int voice_intent_session_register(void)
{
	if (s_session_finish_timer == NULL) {
		s_session_finish_timer = xTimerCreate("sess.fin",
		                                      pdMS_TO_TICKS(SESSION_FINISH_POP_DELAY_MS),
		                                      pdFALSE, NULL, session_finish_timer_cb);
		if (s_session_finish_timer == NULL) {
			LOGE("failed to create session finish timer");
		}
	}

	voice_msg_sub(VOICE_MSG_CLOUD_SESSION_STARTING, on_cloud_session_starting, NULL);
	voice_msg_sub(VOICE_MSG_CLOUD_SESSION_FINISHED, on_cloud_session_finished, NULL);
	voice_msg_sub(VOICE_MSG_PLAYER_TTS_STOPED, on_tts_stoped, NULL);
	voice_msg_sub(VOICE_MSG_CLOUD_MCP_CHAT_EXIT, on_cloud_mcp_chat_exit, NULL);

	return voice_intent_register_ops(&(voice_intent_ops_t){
		.type = INTENT_VOICE_SESSION,
		.on_enter = voice_session_intent_on_enter,
		.on_preempted = voice_session_intent_on_preempted,
		.on_resumed = voice_session_intent_on_resumed,
		.on_exit = voice_session_intent_on_exit,
	});
}

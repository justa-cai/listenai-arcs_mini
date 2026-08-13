#define TAG "intent_session"

#include "voice_intent_session.h"

#include "lisa_log.h"
#include "voice_msg.h"
#include "voice_intent_mgr.h"
#include "voice_intent_music.h"
#include "voice_player/voice_player_tts.h"
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

/* 被更高优先级意图抢占时停止活跃 TTS。唤醒入口可能已主动停止 TTS，
 * active 检查用于避免 PROMPT_TONE 再次 stop 并产生重复 STOPPED。 */
static void voice_session_intent_on_preempted(void)
{
	if (!voice_player_tts_is_active()) {
		LOGI("VOICE_SESSION on_preempted: tts inactive, skip stop");
		return;
	}

	LOGI("VOICE_SESSION on_preempted: stop tts");
	voice_player_tts_stop();
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
	/* Idle timeout may push PROMPT_TONE before SESSION_FINISHED. In that
	 * case VOICE_SESSION is no longer top, but it still needs to leave the
	 * stack so the prompt tone can resume active background music when it exits. */
	if (voice_intent_contains(INTENT_MUSIC) &&
	    !voice_intent_music_is_user_paused()) {
		LOGI("session finished with background MUSIC, pop VOICE_SESSION");
		voice_intent_pop(INTENT_VOICE_SESSION);
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

/* 云端会话退出 → 弹出 VOICE_SESSION 意图 */
static void on_cloud_session_exit(void *unused, uint32_t msg_id,
                                  void *data, uint32_t len, void *user_data)
{
	(void)unused; (void)msg_id; (void)data; (void)len; (void)user_data;

	if (!voice_intent_contains(INTENT_VOICE_SESSION)) {
		return;
	}
	if (voice_player_tts_is_active()) {
		s_session_finished = true;
		LOGI("cloud session exit: tts active, defer VOICE_SESSION pop until tts stopped");
		return;
	}
	LOGI("cloud session exit: pop INTENT_VOICE_SESSION");
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
	voice_msg_sub(VOICE_MSG_CLOUD_MCP_CHAT_EXIT, on_cloud_session_exit, NULL);
	voice_msg_sub(VOICE_MSG_CLOUD_SESSION_INTERRUPT, on_cloud_session_exit, NULL);

	return voice_intent_register_ops(&(voice_intent_ops_t){
		.type = INTENT_VOICE_SESSION,
		.on_enter = voice_session_intent_on_enter,
		.on_preempted = voice_session_intent_on_preempted,
		.on_resumed = voice_session_intent_on_resumed,
		.on_exit = voice_session_intent_on_exit,
	});
}

#define TAG "intent_photo_flow"

#include "voice_intent_photo_flow.h"

#include "lisa_log.h"
#include "voice_msg.h"
#include "voice_player_comm.h"
#include "voice_intent_mgr.h"

/* ---- 相机流状态 ----
 *
 *   进入 PHOTO_FLOW → push（preempt 会 stop TTS），save TTS URL
 *   拍照中           → 所有新 TTS URL 被门控丢弃
 *   退出 PHOTO_FLOW → replay 保存的 TTS URL，云端续流照片描述 */

#define TTS_URL_MAX 256

static voice_msg_camera_preview_state_t s_preview_state = {0};
static char s_saved_tts_url[TTS_URL_MAX];

/* 按键拍照流程跟踪：按键进入预览时 push PHOTO_FLOW pause 音乐，
 * 拍照提交后需等结果 TTS 播完才 pop PHOTO_FLOW 恢复音乐。 */
static bool s_button_flow_active = false;
static bool s_button_capture_submitted = false;
static bool s_button_result_tts_pending = false;

/* MCP 拍照：PHOTO_FLOW exit 时复播 TTS 后，需在 TTS 结束时主动弹出
 * VOICE_SESSION，避免 session 未结束时 MUSIC 一直被抢占无法恢复播放。 */
static bool s_mcp_replay_tts_active = false;

/* ---- 内部工具 ---- */

static bool photo_flow_is_active(void)
{
	if (s_preview_state.mode == CAMERA_PREVIEW_MODE_MCP &&
	    s_preview_state.phase != CAMERA_FLOW_PHASE_NONE) {
		return true;
	}

	/* 按键拍照：没有 phase 概念（始终 NONE），
	 * 通过 intent 栈内的 PHOTO_FLOW 判断是否在拍照流程中 */
	if (s_preview_state.mode == CAMERA_PREVIEW_MODE_BUTTON &&
	    voice_intent_contains(INTENT_PHOTO_FLOW)) {
		return true;
	}

	return false;
}

/* ==================== PHOTO_FLOW intent 钩子 ==================== */

static void photo_flow_intent_on_enter(void)
{
	LOGI("PHOTO_FLOW on_enter");
	app_player_stop(tts_player);
}

static void photo_flow_intent_on_exit(void)
{
	LOGI("PHOTO_FLOW on_exit");

	/* 无论是否有 URL 需要复播，都要清除 s_current_tts_url，
	 * 防止下一轮 sync 拍照时在 on_camera_preview_start 中
	 * 通过 voice_player_latest_tts_url_copy 拿到过期 URL。 */
	voice_player_latest_tts_url_clear();

	if (s_saved_tts_url[0] != '\0' && !voice_player_tts_is_active()) {
		LOGI("replay saved tts url: %s", s_saved_tts_url);
		voice_player_replay_tts_url(s_saved_tts_url);
		s_mcp_replay_tts_active = true;
		s_saved_tts_url[0] = '\0';
	} else {
		s_mcp_replay_tts_active = false;
		s_saved_tts_url[0] = '\0';
		/* sync 拍照无 TTS 需复播——但有照片预览 UI 处于
		 * camera_preview_result_pending 状态等待 TTS 结束事件。
		 * 此处主动弹出 VOICE_SESSION（如果还在栈内）并发布
		 * TTS_STOPED 事件，让 UI 收到后隐藏预览页面回到 home。 */
		if (voice_intent_contains(INTENT_VOICE_SESSION) &&
		    voice_intent_top() == INTENT_VOICE_SESSION) {
			LOGI("no tts to replay, pop VOICE_SESSION directly");
			voice_intent_pop(INTENT_VOICE_SESSION);
		}
		LOGI("no tts to replay, publish synthetic TTS_STOPED to dismiss photo UI");
		voice_msg_pub(VOICE_MSG_PLAYER_TTS_STOPED, NULL, 0);
	}
}

/* ---- 按键拍照流程管理 ---- */

static void button_photo_flow_exit(const char *reason)
{
	if (!s_button_flow_active) {
		return;
	}

	LOGI("button PHOTO_FLOW exit: %s", reason);

	if (voice_intent_contains(INTENT_PHOTO_FLOW)) {
		voice_intent_pop(INTENT_PHOTO_FLOW);
	}

	s_button_flow_active = false;
	s_button_capture_submitted = false;
	s_button_result_tts_pending = false;
}

/* ==================== ebus 事件处理 ==================== */

static void on_camera_preview_state_changed(void *unused, uint32_t msg_id,
                                             void *data, uint32_t len, void *user_data)
{
	const voice_msg_camera_preview_state_t *new_state = data;

	(void)unused; (void)msg_id; (void)user_data;

	if (!data || len < sizeof(s_preview_state)) {
		return;
	}

	/* 按键拍照：检测拍照提交，标记等待结果 TTS */
	if (s_button_flow_active &&
	    new_state->mode == CAMERA_PREVIEW_MODE_NONE &&
	    new_state->phase == CAMERA_FLOW_PHASE_NONE &&
	    new_state->captured) {
		if (!s_button_capture_submitted) {
			LOGI("button PHOTO_FLOW capture submitted, wait result TTS");
		}
		s_button_capture_submitted = true;
	}

	s_preview_state = *new_state;
}

static void on_camera_preview_start(void *unused, uint32_t msg_id,
                                     void *data, uint32_t len, void *user_data)
{
	voice_msg_camera_preview_req_t *req = (voice_msg_camera_preview_req_t *)data;

	(void)unused; (void)msg_id; (void)user_data;

	if (!req || len < sizeof(*req)) {
		return;
	}

	switch (req->mode) {
	case CAMERA_PREVIEW_MODE_MCP:
		/* push PHOTO_FLOW 之前先保存当前 TTS URL——preempt 会 stop TTS，
		 * 但不会替我们存 URL。pushup 场景下 VOICE_SESSION 已出栈，也没有
		 * on_preempted，所以统一在这里保存，保证 exit 时有 URL 可 replay。
		 *
		 * no_pushup_tts=1 表示 JSON args 中带 "sync":true，云端不会下发
		 * pushup TTS URL，跳过保存，避免误复播其他 session 的过期 URL。 */
		if (!req->no_pushup_tts) {
			voice_player_latest_tts_url_copy(s_saved_tts_url, sizeof(s_saved_tts_url));
		} else {
			s_saved_tts_url[0] = '\0';
		}
		LOGI("mcp photo preview start, push PHOTO_FLOW, saved tts=%s",
		     s_saved_tts_url[0] ? s_saved_tts_url : "(empty)");
		voice_intent_push(INTENT_PHOTO_FLOW);
		break;

	case CAMERA_PREVIEW_MODE_BUTTON:
		/* 按键进入预览时也需要 PHOTO_FLOW 来主动 pause MUSIC，
		 * 避免音乐在预览页面持续播放。不需要保存 TTS URL
		 * （按键拍照不走 pushup 流程）。 */
		LOGI("button photo preview start, push PHOTO_FLOW");
		s_button_flow_active = true;
		s_button_capture_submitted = false;
		s_button_result_tts_pending = false;
		voice_intent_push(INTENT_PHOTO_FLOW);
		break;

	default:
		break;
	}
}

static void on_camera_preview_exit(void *unused, uint32_t msg_id,
                                    void *data, uint32_t len, void *user_data)
{
	(void)unused; (void)msg_id; (void)data; (void)len; (void)user_data;

	if (s_button_flow_active) {
		/* 按键预览取消（未拍照），直接退出 PHOTO_FLOW */
		button_photo_flow_exit("button preview exit");
		return;
	}

	/* MCP 拍照取消（locked 状态）且 TTS 确实在播放时才 stop，
	 * 避免正常退出时 stop 非活跃 player 产生虚假 STOPPED 事件。 */
	if (s_preview_state.mode == CAMERA_PREVIEW_MODE_MCP &&
	    (s_preview_state.phase == CAMERA_FLOW_PHASE_PREVIEW ||
	     s_preview_state.phase == CAMERA_FLOW_PHASE_PROCESSING) &&
	    voice_player_tts_is_active()) {
		app_player_stop(tts_player);
	}

	if (voice_intent_contains(INTENT_PHOTO_FLOW)) {
		voice_intent_pop(INTENT_PHOTO_FLOW);
	}
}

/* ==================== 按键拍照 TTS 事件 ==================== */

static void on_tts_url(void *unused, uint32_t msg_id,
                       void *data, uint32_t len, void *user_data)
{
	(void)unused; (void)msg_id; (void)data; (void)len; (void)user_data;

	if (!s_button_flow_active || !s_button_capture_submitted) {
		return;
	}

	if (!s_button_result_tts_pending) {
		LOGI("button PHOTO_FLOW result TTS pending");
	}
	s_button_result_tts_pending = true;
}

static void on_tts_stoped(void *unused, uint32_t msg_id,
                           void *data, uint32_t len, void *user_data)
{
	(void)unused; (void)msg_id; (void)data; (void)len; (void)user_data;

	/* MCP 拍照复播 TTS 结束时，主动弹出 VOICE_SESSION 让 MUSIC 恢复播放。
	 * 常规语音对话中 session_finished + on_tts_stoped 会自然触发 pop，
	 * 但拍照流程中 session 可能尚未结束，需要这里主动弹出。 */
	if (s_mcp_replay_tts_active) {
		s_mcp_replay_tts_active = false;
		if (!s_button_flow_active &&
		    voice_intent_contains(INTENT_VOICE_SESSION) &&
		    voice_intent_top() == INTENT_VOICE_SESSION) {
			LOGI("mcp replay tts done, pop VOICE_SESSION to resume MUSIC");
			voice_intent_pop(INTENT_VOICE_SESSION);
		}
		return;
	}

	if (!s_button_flow_active || !s_button_capture_submitted) {
		return;
	}

	if (s_button_result_tts_pending) {
		button_photo_flow_exit("result TTS stopped");
	}
}

/* ==================== public API ==================== */

bool voice_intent_photo_flow_should_gate_tts(void)
{
	return photo_flow_is_active();
}

/* ==================== 注册 ==================== */

int voice_intent_photo_flow_register(void)
{
	voice_msg_sub(VOICE_MSG_APP_CAMERA_PREVIEW_START, on_camera_preview_start, NULL);
	voice_msg_sub(VOICE_MSG_APP_CAMERA_PREVIEW_STATE, on_camera_preview_state_changed, NULL);
	voice_msg_sub(VOICE_MSG_APP_CAMERA_PREVIEW_EXIT, on_camera_preview_exit, NULL);
	voice_msg_sub(VOICE_MSG_CLOUD_TTS_URL, on_tts_url, NULL);
	voice_msg_sub(VOICE_MSG_CLOUD_PUSHUP_TTS_URL, on_tts_url, NULL);
	voice_msg_sub(VOICE_MSG_PLAYER_TTS_STOPED, on_tts_stoped, NULL);

	return voice_intent_register_ops(&(voice_intent_ops_t){
		.type = INTENT_PHOTO_FLOW,
		.on_enter = photo_flow_intent_on_enter,
		.on_exit = photo_flow_intent_on_exit,
	});
}

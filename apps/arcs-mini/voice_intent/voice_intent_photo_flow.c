#define TAG "intent_photo_flow"

#include "voice_intent_photo_flow.h"

#include "lisa_log.h"
#include "voice_msg.h"
#include "voice_player_comm.h"
#include "voice_intent_mgr.h"

/* ---- 相机流状态 ----
 *
 *   进入 PHOTO_FLOW → save TTS URL + push（preempt 会 stop TTS）
 *   拍照中           → 所有新 TTS URL 被门控丢弃
 *   退出 PHOTO_FLOW → replay 保存的 TTS URL，云端续流照片描述 */

#define TTS_URL_MAX 256

static voice_msg_camera_preview_state_t s_preview_state = {0};
static char s_saved_tts_url[TTS_URL_MAX];

/* ---- 内部工具 ---- */

static bool photo_flow_is_active(void)
{
	return s_preview_state.mode == CAMERA_PREVIEW_MODE_MCP &&
	       s_preview_state.phase != CAMERA_FLOW_PHASE_NONE;
}

/* ==================== PHOTO_FLOW intent 钩子 ==================== */

static void photo_flow_intent_on_enter(void)
{
	LOGI("PHOTO_FLOW on_enter");
}

static void photo_flow_intent_on_exit(void)
{
	LOGI("PHOTO_FLOW on_exit");

	/* 拍照完成后复播之前保存的 TTS URL，让云端继续下发照片描述。 */
	if (s_saved_tts_url[0] != '\0' && !voice_player_tts_is_active()) {
		LOGI("replay saved tts url: %s", s_saved_tts_url);
		voice_player_replay_tts_url(s_saved_tts_url);
		s_saved_tts_url[0] = '\0';
	}
}

/* ==================== ebus 事件处理 ==================== */

static void on_camera_preview_state_changed(void *unused, uint32_t msg_id,
                                             void *data, uint32_t len, void *user_data)
{
	(void)unused; (void)msg_id; (void)user_data;

	if (data && len >= sizeof(s_preview_state)) {
		s_preview_state = *(const voice_msg_camera_preview_state_t *)data;
	}
}

static void on_camera_preview_start(void *unused, uint32_t msg_id,
                                     void *data, uint32_t len, void *user_data)
{
	voice_msg_camera_preview_req_t *req = (voice_msg_camera_preview_req_t *)data;

	(void)unused; (void)msg_id; (void)user_data;

	if (!req || len < sizeof(*req)) {
		return;
	}

	if (req->mode != CAMERA_PREVIEW_MODE_MCP) {
		return;
	}

	/* push PHOTO_FLOW 之前先保存当前 TTS URL——preempt 会 stop TTS，
	 * 但不会替我们存 URL。pushup 场景下 VOICE_SESSION 已出栈，也没有
	 * on_preempted，所以统一在这里保存，保证 exit 时有 URL 可 replay。 */
	voice_player_latest_tts_url_copy(s_saved_tts_url, sizeof(s_saved_tts_url));
	LOGI("mcp photo preview start, push PHOTO_FLOW, saved tts=%s",
	     s_saved_tts_url[0] ? s_saved_tts_url : "(empty)");

	voice_intent_push(INTENT_PHOTO_FLOW);
}

static void on_camera_preview_exit(void *unused, uint32_t msg_id,
                                    void *data, uint32_t len, void *user_data)
{
	(void)unused; (void)msg_id; (void)data; (void)len; (void)user_data;

	/* 拍照取消（locked 状态）且 TTS 确实在播放时才 stop，
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

	return voice_intent_register_ops(&(voice_intent_ops_t){
		.type = INTENT_PHOTO_FLOW,
		.on_enter = photo_flow_intent_on_enter,
		.on_exit = photo_flow_intent_on_exit,
	});
}

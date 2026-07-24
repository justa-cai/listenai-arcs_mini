#define TAG "intent_photo_flow"

#include "voice_intent_photo_flow.h"

#include "lisa_log.h"
#include "voice_msg.h"
#include "voice_intent_mgr.h"
#include "voice_player/voice_player_tts.h"

/* MCP 结果 TTS 的三种到达时机：
 *
 * URL 早于拍照指令：进入 PHOTO_FLOW 时保存当前 TTS，退出时复播并 WAIT_STOP
 * URL 在 PHOTO_FLOW 内到达：EXPECT_URL -> URL_RECEIVED，退出时确认播放并 WAIT_STOP
 * URL 晚于 PHOTO_FLOW：EXPECT_URL -> WAIT_URL_AFTER_EXIT -> WAIT_STOP
 *
 * WAIT_STOP 表示结果 TTS 已复播或直接播放，结束后需要收尾 VOICE_SESSION。 */
typedef enum {
    MCP_TTS_STATE_NONE = 0,
    MCP_TTS_STATE_EXPECT_URL,
    MCP_TTS_STATE_URL_RECEIVED,
    MCP_TTS_STATE_WAIT_URL_AFTER_EXIT,
    MCP_TTS_STATE_WAIT_STOP,
} mcp_tts_state_t;

/* 按键拍照由 PHOTO_FLOW 暂停音乐，结果 TTS 播完后再退出：
 * PREVIEW -> WAIT_TTS_URL -> WAIT_TTS_STOP -> IDLE。 */
typedef enum {
    BUTTON_PHOTO_STATE_IDLE = 0,
    BUTTON_PHOTO_STATE_PREVIEW,
    BUTTON_PHOTO_STATE_WAIT_TTS_URL,
    BUTTON_PHOTO_STATE_WAIT_TTS_STOP,
} button_photo_state_t;

typedef struct {
    voice_msg_camera_preview_state_t preview;
    mcp_tts_state_t mcp_tts;
    button_photo_state_t button;
} photo_flow_context_t;

static photo_flow_context_t s_photo_flow = {0};

static bool mcp_preview_was_canceled(void)
{
    return s_photo_flow.preview.mode == CAMERA_PREVIEW_MODE_MCP &&
           s_photo_flow.preview.phase == CAMERA_FLOW_PHASE_PREVIEW &&
           !s_photo_flow.preview.captured;
}

static void photo_flow_finish_without_tts(void)
{
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

static void mcp_photo_flow_exit(void)
{
    /* 预览阶段尚未拍照便退出，表示用户通过再次唤醒等方式取消拍照。
     * 此时不能复播进入 PHOTO_FLOW 前缓存的 TTS；正常拍照完成后退出时
     * captured 已置位，仍走下方复播流程。 */
    if (mcp_preview_was_canceled()) {
        LOGI("mcp photo preview canceled, discard prepared tts replay");
        voice_player_tts_discard_prepared_replay();
        s_photo_flow.mcp_tts = MCP_TTS_STATE_NONE;
        return;
    }

    if (voice_player_tts_replay_prepared()) {
        s_photo_flow.mcp_tts = MCP_TTS_STATE_WAIT_STOP;
        return;
    }

    if (voice_player_tts_is_active()) {
        /* 拍照指令先到时，后续 URL 已按普通 TTS 直接播放。 */
        LOGI("photo-first tts is already active, skip replay");
        s_photo_flow.mcp_tts = MCP_TTS_STATE_WAIT_STOP;
        return;
    }

    if (s_photo_flow.mcp_tts == MCP_TTS_STATE_EXPECT_URL ||
        s_photo_flow.mcp_tts == MCP_TTS_STATE_WAIT_URL_AFTER_EXIT) {
        /* 普通异步拍照的结果 URL 可能晚于上传完成到达。此时保持照片
         * 可见，等真实 TTS 播放结束事件收尾，不能伪造 STOPPED。 */
        LOGI("photo-first tts url pending, keep photo UI");
        s_photo_flow.mcp_tts = MCP_TTS_STATE_WAIT_URL_AFTER_EXIT;
        return;
    }

    s_photo_flow.mcp_tts = MCP_TTS_STATE_NONE;
    photo_flow_finish_without_tts();
}

/* ==================== PHOTO_FLOW intent 钩子 ==================== */

static void photo_flow_intent_on_enter(void)
{
    LOGI("PHOTO_FLOW on_enter");
}

static void photo_flow_intent_on_exit(void)
{
    LOGI("PHOTO_FLOW on_exit");
    mcp_photo_flow_exit();
}

/* ---- 按键拍照流程管理 ---- */

static void button_photo_flow_exit(const char *reason)
{
    if (s_photo_flow.button == BUTTON_PHOTO_STATE_IDLE) {
        return;
    }

    LOGI("button PHOTO_FLOW exit: %s", reason);

    if (voice_intent_contains(INTENT_PHOTO_FLOW)) {
        voice_intent_pop(INTENT_PHOTO_FLOW);
    }

    s_photo_flow.button = BUTTON_PHOTO_STATE_IDLE;
}

/* ==================== ebus 事件处理 ==================== */

static void on_camera_preview_state_changed(void *unused, uint32_t msg_id,
                                             void *data, uint32_t len, void *user_data)
{
    const voice_msg_camera_preview_state_t *new_state = data;

    (void)unused;
    (void)msg_id;
    (void)user_data;

    if (!data || len < sizeof(s_photo_flow.preview)) {
        return;
    }

    /* 按键拍照：检测拍照提交，标记等待结果 TTS。 */
    if (s_photo_flow.button == BUTTON_PHOTO_STATE_PREVIEW &&
        new_state->mode == CAMERA_PREVIEW_MODE_NONE &&
        new_state->phase == CAMERA_FLOW_PHASE_NONE &&
        new_state->captured) {
        LOGI("button PHOTO_FLOW capture submitted, wait result TTS");
        s_photo_flow.button = BUTTON_PHOTO_STATE_WAIT_TTS_URL;
    }

    s_photo_flow.preview = *new_state;
}

static void on_camera_preview_start(void *unused, uint32_t msg_id,
                                     void *data, uint32_t len, void *user_data)
{
    voice_msg_camera_preview_req_t *req = (voice_msg_camera_preview_req_t *)data;

    (void)unused;
    (void)msg_id;
    (void)user_data;

    if (!req || len < sizeof(*req)) {
        return;
    }

    switch (req->mode) {
    case CAMERA_PREVIEW_MODE_MCP:
        s_photo_flow.preview.active = 1;
        s_photo_flow.preview.mode = CAMERA_PREVIEW_MODE_MCP;
        s_photo_flow.preview.captured = 0;
        s_photo_flow.preview.phase = CAMERA_FLOW_PHASE_PREVIEW;
        s_photo_flow.mcp_tts = req->no_pushup_tts ? MCP_TTS_STATE_NONE : MCP_TTS_STATE_EXPECT_URL;

        /* push PHOTO_FLOW 之前原子完成 TTS 快照、队列清空和播放器停止。
         * pushup 场景下 VOICE_SESSION 已出栈，也没有 on_preempted。
         *
         * no_pushup_tts=1 表示 JSON args 中带 "sync":true，云端不会下发
         * pushup TTS URL，丢弃快照，避免误复播其他 session 的过期 URL。 */
        if (!req->no_pushup_tts) {
            voice_player_tts_snapshot_and_stop();
        } else {
            voice_player_tts_stop();
        }
        LOGI("mcp photo preview start, push PHOTO_FLOW");
        voice_intent_push(INTENT_PHOTO_FLOW);
        break;

    case CAMERA_PREVIEW_MODE_BUTTON:
        /* 按键进入预览时也需要 PHOTO_FLOW 来主动 pause MUSIC，
         * 避免音乐在预览页面持续播放。不需要保存 TTS URL
         * （按键拍照不走 pushup 流程）。 */
        LOGI("button photo preview start, push PHOTO_FLOW");
        s_photo_flow.button = BUTTON_PHOTO_STATE_PREVIEW;
        if (s_photo_flow.mcp_tts != MCP_TTS_STATE_WAIT_STOP) {
            s_photo_flow.mcp_tts = MCP_TTS_STATE_NONE;
        }
        voice_player_tts_stop();
        voice_intent_push(INTENT_PHOTO_FLOW);
        break;

    default:
        break;
    }
}

static void on_camera_preview_exit(void *unused, uint32_t msg_id,
                                    void *data, uint32_t len, void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)data;
    (void)len;
    (void)user_data;

    if (s_photo_flow.button != BUTTON_PHOTO_STATE_IDLE) {
        /* 按键预览取消（未拍照），直接退出 PHOTO_FLOW。 */
        button_photo_flow_exit("button preview exit");
        return;
    }

    /* MCP 拍照取消（locked 状态）且 TTS 确实在播放时才 stop，
     * 避免正常退出时 stop 非活跃 player 产生虚假 STOPPED 事件。 */
    if (s_photo_flow.preview.mode == CAMERA_PREVIEW_MODE_MCP &&
        (s_photo_flow.preview.phase == CAMERA_FLOW_PHASE_PREVIEW ||
         s_photo_flow.preview.phase == CAMERA_FLOW_PHASE_PROCESSING) &&
        voice_player_tts_is_active()) {
        voice_player_tts_stop();
    }

    if (voice_intent_contains(INTENT_PHOTO_FLOW)) {
        voice_intent_pop(INTENT_PHOTO_FLOW);
    }
}

/* ==================== TTS 事件 ==================== */

static void on_tts_url(void *unused, uint32_t msg_id,
                       void *data, uint32_t len, void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)data;
    (void)len;
    (void)user_data;

    if (s_photo_flow.mcp_tts == MCP_TTS_STATE_EXPECT_URL) {
        s_photo_flow.mcp_tts = MCP_TTS_STATE_URL_RECEIVED;
    } else if (s_photo_flow.mcp_tts == MCP_TTS_STATE_WAIT_URL_AFTER_EXIT) {
        LOGI("photo-first tts url received after PHOTO_FLOW exit");
        s_photo_flow.mcp_tts = MCP_TTS_STATE_WAIT_STOP;
    }

    if (s_photo_flow.button == BUTTON_PHOTO_STATE_WAIT_TTS_URL) {
        LOGI("button PHOTO_FLOW result TTS pending");
        s_photo_flow.button = BUTTON_PHOTO_STATE_WAIT_TTS_STOP;
    }
}

static void on_tts_stoped(void *unused, uint32_t msg_id,
                           void *data, uint32_t len, void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)data;
    (void)len;
    (void)user_data;

    /* MCP 拍照复播 TTS 结束时，主动弹出 VOICE_SESSION 让 MUSIC 恢复播放。
     * 常规语音对话中 session_finished + on_tts_stoped 会自然触发 pop，
     * 但拍照流程中 session 可能尚未结束，需要这里主动弹出。 */
    if (s_photo_flow.mcp_tts == MCP_TTS_STATE_WAIT_STOP) {
        s_photo_flow.mcp_tts = MCP_TTS_STATE_NONE;
        if (s_photo_flow.button == BUTTON_PHOTO_STATE_IDLE &&
            voice_intent_contains(INTENT_VOICE_SESSION) &&
            voice_intent_top() == INTENT_VOICE_SESSION) {
            LOGI("mcp replay tts done, pop VOICE_SESSION to resume MUSIC");
            voice_intent_pop(INTENT_VOICE_SESSION);
        }
        return;
    }

    if (s_photo_flow.button == BUTTON_PHOTO_STATE_WAIT_TTS_STOP) {
        button_photo_flow_exit("result TTS stopped");
    }
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

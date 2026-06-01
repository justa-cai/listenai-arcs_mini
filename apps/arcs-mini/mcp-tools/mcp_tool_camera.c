#include <stdbool.h>
#include <string.h>

#include "cJSON.h"
#include "alarm_handler.h"
#include "alarm_ring.h"
#include "async_task.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "mcp.h"
#include "service_camera.h"
#include "sys_init.h"
#include "voice_camera_preview_state.h"
#include "voice_msg.h"
#include "voice_player_comm.h"

static struct {
    bool pending;
    bool preview_entered;
    char id[64];
    uint8_t phase;
    char latest_tts_url[768];
} s_take_photo_call;

struct take_photo_async_response {
    bool is_error;
    char id[64];
    char reason[32];
};

static cJSON *take_photo_result_image(const char *url, bool is_error)
{
    cJSON *result = cJSON_CreateObject();
    if (!result) {
        return NULL;
    }

    cJSON *content_array = cJSON_CreateArray();
    cJSON *content_item = cJSON_CreateObject();
    if (!content_array || !content_item) {
        if (content_array) {
            cJSON_Delete(content_array);
        }
        if (content_item) {
            cJSON_Delete(content_item);
        }
        cJSON_Delete(result);
        return NULL;
    }

    cJSON_AddStringToObject(content_item, "type", "image");
    cJSON_AddStringToObject(content_item, "data", url ? url : "");
    cJSON_AddStringToObject(content_item, "mimeType", "url");
    cJSON_AddItemToArray(content_array, content_item);
    cJSON_AddItemToObject(result, "content", content_array);
    cJSON_AddBoolToObject(result, "isError", is_error);

    return result;
}

static void take_photo_pending_clear(void)
{
    memset(&s_take_photo_call, 0, sizeof(s_take_photo_call));
}

static void take_photo_replay_saved_latest_tts_url(const char *url, const char *reason)
{
    if (!url || url[0] == '\0') {
        /* 没有可回放的 TTS 时（如设备从未交互过），UI 的退出逻辑依赖 TTS 播放结束
         * 事件触发，会导致拍照流卡在上传状态。这里直接发布退出消息收尾。 */
        LOGI("take photo mcp: no saved latest tts url to replay (%s), exit camera preview",
             reason ? reason : "unknown");
        voice_msg_pub(VOICE_MSG_APP_CAMERA_PREVIEW_EXIT, NULL, 0);
        return;
    }

    if (voice_msg_pub(VOICE_MSG_CLOUD_TTS_URL, (void *)url, strlen(url) + 1) != 0) {
        LOGE("take photo mcp: replay saved latest tts url failed (%s), exit camera preview",
             reason ? reason : "unknown");
        voice_msg_pub(VOICE_MSG_APP_CAMERA_PREVIEW_EXIT, NULL, 0);
        return;
    }

    LOGI("take photo mcp: replay saved latest tts url (%s): %s",
         reason ? reason : "unknown", url);
}

static bool take_photo_tts_is_active(void)
{
    /* 不直接走 app_player_get_state：当 async_play 线程正在 HTTP recv 时持有
     * tts_player->operation_lock，会让本回调（voice.ebus）阻塞数秒，导致 MCP
     * 拍照响应时延远超预期。改用 voice_player 基于事件维护的非阻塞标志。 */
    return voice_player_tts_is_active();
}

static void take_photo_interrupt_tts_immediately(void)
{
    int ret = 0;

    if (tts_player == NULL) {
        LOGW("take photo mcp: tts player not ready");
        return;
    }

    ret = app_player_stop(tts_player);
    if (ret == APP_PLAYER_OK) {
        LOGI("take photo mcp: stopped tts immediately");
    } else {
        LOGW("take photo mcp: immediate stop tts failed, ret=%d", ret);
    }
}

static void take_photo_interrupt_alarm_if_ringing(void)
{
    /* 闹钟响时收到云端 pushup 的拍照请求，沿用唤醒打断闹钟的逻辑：
     * 停止响铃并处理后续（删除单次闹钟或安排下次贪睡），避免拍照过程中
     * 闹钟 tone 反复抢占焦点导致拍照流被打断、且拍完照闹钟仍持续响。 */
    if (!alarm_ring_is_active()) {
        return;
    }
    LOGI("take photo mcp: alarm ringing, stop alarm before preview");
    alarm_ring_stop();
    alarm_handle_stop_and_next();
}

static void take_photo_async_response_complete(void *user_data, bool completed, bool interrupted)
{
    struct take_photo_async_response *rsp = user_data;

    (void)completed;
    (void)interrupted;

    if (!rsp) {
        return;
    }

    lisa_mem_free(rsp);
}

static void take_photo_async_error_response_task(void *user_data, bool *should_stop)
{
    struct take_photo_async_response *rsp = user_data;
    cJSON *result = NULL;

    (void)should_stop;

    if (!rsp || rsp->id[0] == '\0') {
        return;
    }

    result = take_photo_result_image(NULL, rsp->is_error);
    if (!result) {
        LOGE("take photo cancel response failed: no memory");
        return;
    }

    LOGI("take photo async failed, id=%s, reason=%s", rsp->id,
         rsp->reason[0] != '\0' ? rsp->reason : "unknown");
    mcp_tool_call_result_response(rsp->id, result);
    cJSON_Delete(result);
}

static void take_photo_pending_start(const char *id, bool tts_was_active)
{
    take_photo_pending_clear();

    s_take_photo_call.pending = true;
    s_take_photo_call.phase = VOICE_MSG_CAMERA_FLOW_PHASE_PREVIEW;
    strncpy(s_take_photo_call.id, id, sizeof(s_take_photo_call.id) - 1);
    /* 只有 MCP 到达时 TTS 处于活跃状态才认为是语音拍照流程，需要保存 latest TTS URL
     * 以便拍完后回放。云端 pushup 静默拍照时 TTS player 空闲，s_current_tts_url 里
     * 残留的是上一次交互的 URL，不应当作"当前 TTS"再次回放。 */
    if (!tts_was_active) {
        LOGI("take photo mcp: tts not active, skip saving latest tts url");
        return;
    }
    if (voice_player_latest_tts_url_copy(s_take_photo_call.latest_tts_url,
                                         sizeof(s_take_photo_call.latest_tts_url))) {
        LOGI("take photo mcp: saved latest tts url: %s", s_take_photo_call.latest_tts_url);
    } else {
        LOGI("take photo mcp: latest tts url is empty");
    }
}

static void take_photo_pending_error_response(const char *reason)
{
    struct take_photo_async_response *rsp = NULL;

    if (!s_take_photo_call.pending || s_take_photo_call.id[0] == '\0') {
        return;
    }

    rsp = lisa_mem_calloc(1, sizeof(*rsp));
    if (!rsp) {
        LOGE("take photo cancel response alloc failed");
        return;
    }

    strncpy(rsp->id, s_take_photo_call.id, sizeof(rsp->id) - 1);
    rsp->is_error = true;
    if (reason && reason[0] != '\0') {
        strncpy(rsp->reason, reason, sizeof(rsp->reason) - 1);
    }
    take_photo_pending_clear();

    async_task_t *task = async_task_create("photo_rsp", 3072, 5,
                                           take_photo_async_error_response_task,
                                           take_photo_async_response_complete, rsp);
    if (!task) {
        LOGE("take photo cancel response task create failed");
        lisa_mem_free(rsp);
        return;
    }

    if (async_task_start(task) != 0) {
        LOGE("take photo cancel response task start failed");
        async_task_destroy(task);
        lisa_mem_free(rsp);
    }
}

static void take_photo_preview_state_changed(void *unused, uint32_t msg_id, void *data,
                                             uint32_t len, void *user_data)
{
    voice_msg_camera_preview_state_t state = {0};

    (void)unused;
    (void)msg_id;
    (void)user_data;

    if (!s_take_photo_call.pending ||
        !voice_camera_preview_state_parse(&state, data, len)) {
        return;
    }

    if (state.mode == VOICE_MSG_CAMERA_PREVIEW_MODE_MCP_PHOTO) {
        s_take_photo_call.phase = state.phase;
        s_take_photo_call.preview_entered = true;
    }

    /* 必须等到看到属于自己拍照流的状态（mode=MCP_PHOTO）之后，才把 phase=NONE 视为
     * 预览被取消。否则闹钟响时点 MCP 拍照，闹钟 UI 退出会先发布一个 mode=0/phase=NONE
     * 的状态变更，这会在我们新拍照流的状态到达之前被收到，从而被误判为"预览被取消"，
     * 触发错误响应让云端收到空数据。 */
    if (state.phase == VOICE_MSG_CAMERA_FLOW_PHASE_NONE &&
        s_take_photo_call.preview_entered) {
        take_photo_pending_error_response("preview stopped");
        return;
    }
}

static void take_photo_preview_exit(void *unused, uint32_t msg_id, void *data,
                                    uint32_t len, void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)data;
    (void)len;
    (void)user_data;

    if (!s_take_photo_call.pending) {
        return;
    }

    if (s_take_photo_call.phase == VOICE_MSG_CAMERA_FLOW_PHASE_PREVIEW ||
        s_take_photo_call.phase == VOICE_MSG_CAMERA_FLOW_PHASE_PROCESSING) {
        take_photo_pending_error_response("preview canceled");
    }
}

static void take_photo_mcp_call_response(void *unused, uint32_t msg_id, void *data,
                                         uint32_t len, void *user_data)
{
    cJSON *root = NULL;
    cJSON *id = NULL;
    cJSON *result = NULL;
    cJSON *is_error = NULL;
    bool should_replay_saved_tts = false;
    char latest_tts_url[sizeof(s_take_photo_call.latest_tts_url)] = {0};

    (void)unused;
    (void)msg_id;
    (void)user_data;

    if (!s_take_photo_call.pending || !data || len == 0) {
        return;
    }

    root = cJSON_ParseWithLength((const char *)data, len);
    if (!root) {
        return;
    }

    id = cJSON_GetObjectItem(root, "id");
    if (cJSON_IsString(id) && id->valuestring &&
        strcmp(id->valuestring, s_take_photo_call.id) == 0) {
        result = cJSON_GetObjectItem(root, "result");
        is_error = cJSON_IsObject(result) ? cJSON_GetObjectItem(result, "isError") : NULL;
        should_replay_saved_tts = !(cJSON_IsBool(is_error) && cJSON_IsTrue(is_error));
        if (should_replay_saved_tts && s_take_photo_call.latest_tts_url[0] != '\0') {
            strncpy(latest_tts_url, s_take_photo_call.latest_tts_url, sizeof(latest_tts_url) - 1);
        }

        LOGI("take photo async response completed, id=%s", s_take_photo_call.id);
        take_photo_pending_clear();
        if (should_replay_saved_tts) {
            take_photo_replay_saved_latest_tts_url(latest_tts_url, "mcp call response");
        } else {
            LOGI("take photo mcp: skip replay latest tts url because response is error, exit camera preview");
            voice_msg_pub(VOICE_MSG_APP_CAMERA_PREVIEW_EXIT, NULL, 0);
        }
    }

    cJSON_Delete(root);
}


static cJSON *take_photo_call(const char *id, const char *name, cJSON *args)
{
    (void)name;
    (void)args;
    voice_msg_camera_preview_req_t req = {0};
    bool tts_was_active = take_photo_tts_is_active();

    LOGI("take photo mcp received, tts_active=%d, interrupt voice session before preview",
         (int)tts_was_active);
    take_photo_interrupt_tts_immediately();
    take_photo_interrupt_alarm_if_ringing();
    voice_msg_pub(VOICE_MSG_CLOUD_SESSION_INTERRUPT, NULL, 0);

    if (!id || id[0] == '\0') {
        LOGE("take photo failed: invalid mcp id");
        return take_photo_result_image(NULL, true);
    }

    if (strlen(id) >= sizeof(req.context_id)) {
        LOGE("take photo failed: mcp id too long");
        return take_photo_result_image(NULL, true);
    }

    if (s_take_photo_call.pending) {
        LOGW("take photo failed: previous request pending, id=%s", s_take_photo_call.id);
        return take_photo_result_image(NULL, true);
    }

    if (!service_camera_is_inited()) {
        int ret = service_camera_init();
        if (ret != 0) {
            LOGE("take photo failed: camera init error %d", ret);
            return take_photo_result_image(NULL, true);
        }
    }

    LOGI("take photo request accepted, mode=mcp, delay_ms=%u", 3000U);

    req.mode = VOICE_MSG_CAMERA_PREVIEW_MODE_MCP_PHOTO;
    req.sync = 1;
    req.auto_capture_delay_ms = 3000;
    strncpy(req.context_id, id, sizeof(req.context_id) - 1);

    if (voice_msg_pub(VOICE_MSG_APP_CAMERA_PREVIEW_START, &req, sizeof(req)) != 0) {
        LOGE("take photo failed: preview start publish error");
        return take_photo_result_image(NULL, true);
    }

    take_photo_pending_start(id, tts_was_active);
    LOGI("take photo preview start published");

    return NULL;
}


static cJSON *take_photo_list(const char *name)
{
    cJSON *tool = mcp_tool_list_info_create_default(name, "拍照工具");
    if (!tool) {
        return NULL;
    }

    return tool;
}

MCP_TOOL_DEFINE(ls.built_in.take_photo, take_photo_list, take_photo_call);

static int take_photo_evt_init(void)
{
    voice_msg_sub(VOICE_MSG_APP_CAMERA_PREVIEW_STATE, take_photo_preview_state_changed, NULL);
    voice_msg_sub(VOICE_MSG_APP_CAMERA_PREVIEW_EXIT, take_photo_preview_exit, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_MCP_CALL_RESP, take_photo_mcp_call_response, NULL);

    return 0;
}

SYS_INIT(take_photo_evt_init, SYS_INIT_LEVEL_PRE_APPLICATION, 60);

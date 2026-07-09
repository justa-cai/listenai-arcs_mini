#include <stdbool.h>
#include <string.h>

#include "cJSON.h"
#include "async_task.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "mcp.h"
#include "service_camera.h"
#include "sys_init.h"
#include "voice_msg.h"
#include "voice_intent_photo_flow.h"

/* ==================== 状态 ==================== */

static struct {
    bool pending;
    bool preview_entered;
    char id[64];
    uint8_t phase;
} s_take_photo_call;

/* 异步错误响应：拍照流程异常时（预览取消、phase=NONE 等），不能在 ebus 回调里
 * 同步发送 MCP 响应（voice.ebus 线程可能被播放器锁阻塞），通过 async_task 投递
 * 到独立线程发送。 */
struct take_photo_async_response {
    bool is_error;
    char id[64];
    char reason[32];
};

/* ==================== 内部工具 ==================== */

/* 构造 MCP 拍照响应 JSON：{ content: [{ type: "image", data: url, mimeType: "url" }], isError } */
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

/* 清零拍照请求跟踪状态 */
static void take_photo_pending_clear(void)
{
    memset(&s_take_photo_call, 0, sizeof(s_take_photo_call));
}

/* async_task 销毁回调，释放 take_photo_async_response 内存 */
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

/* async_task 执行体：构造 isError=true 的 MCP 响应，通过 mcp_tool_call_result_response 发回云端 */
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

/* 标记拍照请求为 in-flight，记录 MCP call ID 以便后续匹配响应 */
static void take_photo_pending_start(const char *id)
{
    take_photo_pending_clear();

    s_take_photo_call.pending = true;
    s_take_photo_call.phase = CAMERA_FLOW_PHASE_PREVIEW;
    strncpy(s_take_photo_call.id, id, sizeof(s_take_photo_call.id) - 1);
}

/* 拍照流程异常中断时，异步发送错误 MCP 响应给云端 */
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

/* ==================== MCP tool ==================== */

static cJSON *take_photo_call(const char *id, const char *name, cJSON *args)
{
    (void)name;
    voice_msg_camera_preview_req_t req = {0};
    bool json_sync = false;

    LOGI("take photo mcp received");

    /* 解析 JSON args 中的 "sync":true 标志：
     * - 带 "sync":true  → sync 拍照，云端不下发 pushup TTS URL
     * - 不带 sync 标志 → 常规拍照，云端会下发 pushup TTS URL */
    if (args) {
        cJSON *sync_item = cJSON_GetObjectItem(args, "sync");
        if (sync_item && cJSON_IsBool(sync_item)) {
            json_sync = cJSON_IsTrue(sync_item);
        }
    }

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

    req.mode = CAMERA_PREVIEW_MODE_MCP;
    req.sync = 1;
    req.no_pushup_tts = json_sync ? 1 : 0;
    req.auto_capture_delay_ms = 3000;
    strncpy(req.context_id, id, sizeof(req.context_id) - 1);

    if (voice_msg_pub(VOICE_MSG_APP_CAMERA_PREVIEW_START, &req, sizeof(req)) != 0) {
        LOGE("take photo failed: preview start publish error");
        return take_photo_result_image(NULL, true);
    }

    take_photo_pending_start(id);
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

/* ==================== ebus 事件处理 ==================== */

static void take_photo_preview_state_changed(void *unused, uint32_t msg_id, void *data,
                                             uint32_t len, void *user_data)
{
    voice_msg_camera_preview_state_t state = {0};

    (void)unused;
    (void)msg_id;
    (void)user_data;

    if (!s_take_photo_call.pending) {
        return;
    }
    if (!data || len < sizeof(state)) {
        return;
    }
    state = *(const voice_msg_camera_preview_state_t *)data;

    if (state.mode == CAMERA_PREVIEW_MODE_MCP) {
        s_take_photo_call.phase = state.phase;
        s_take_photo_call.preview_entered = true;
    }

    /* 必须等到看到属于自己拍照流的状态（mode=MCP_PHOTO）之后，才把 phase=NONE 视为
     * 预览被取消。否则闹钟响时点 MCP 拍照，闹钟 UI 退出会先发布一个 mode=0/phase=NONE
     * 的状态变更，这会在我们新拍照流的状态到达之前被收到，从而被误判为"预览被取消"，
     * 触发错误响应让云端收到空数据。 */
    if (state.phase == CAMERA_FLOW_PHASE_NONE &&
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

    if (s_take_photo_call.phase == CAMERA_FLOW_PHASE_PREVIEW ||
        s_take_photo_call.phase == CAMERA_FLOW_PHASE_PROCESSING) {
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
        bool is_error_response = cJSON_IsBool(is_error) && cJSON_IsTrue(is_error);

        LOGI("take photo async response completed, id=%s, is_error=%d",
             s_take_photo_call.id, (int)is_error_response);
        take_photo_pending_clear();

        /* 无论成功还是失败都退出预览，让 PHOTO_FLOW 出栈 → gate 清零，
         * 确保云端后续 TTS 不被 should_gate_photo_result_tts 拦截。 */
        voice_msg_pub(VOICE_MSG_APP_CAMERA_PREVIEW_EXIT, NULL, 0);
    }

    cJSON_Delete(root);
}

static int take_photo_evt_init(void)
{
    voice_msg_sub(VOICE_MSG_APP_CAMERA_PREVIEW_STATE, take_photo_preview_state_changed, NULL);
    voice_msg_sub(VOICE_MSG_APP_CAMERA_PREVIEW_EXIT, take_photo_preview_exit, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_MCP_CALL_RESP, take_photo_mcp_call_response, NULL);

    return 0;
}

SYS_INIT(take_photo_evt_init, SYS_INIT_LEVEL_PRE_APPLICATION, 60);

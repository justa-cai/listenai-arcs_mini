#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "cJSON.h"
#include "lisa_log.h"
#include "mcp.h"
#include "service_camera.h"
#include "sysheap.h"
#include "voice_msg.h"

static cJSON *take_photo_result_text(const char *name, const char *text, bool is_error)
{
    cJSON *result = mcp_tool_call_result_create(name);
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

    cJSON_AddStringToObject(content_item, "type", "text");
    cJSON_AddStringToObject(content_item, "text", text);
    cJSON_AddItemToArray(content_array, content_item);
    cJSON_AddItemToObject(result, "content", content_array);
    cJSON_AddBoolToObject(result, "isError", is_error);

    return result;
}

static cJSON *take_photo_list(const char *name)
{
    cJSON *tool = mcp_tool_list_info_create_default(name, "拍照");
    if (!tool) {
        return NULL;
    }

    return tool;
}

static cJSON *take_photo_call(const char *id, const char *name, cJSON *args)
{
    (void)args;
    voice_msg_camera_preview_req_t req = {0};

    if (!id || id[0] == '\0') {
        LOGE("take photo failed: invalid mcp id");
        return take_photo_result_text(name, "拍照请求参数错误。", true);
    }

    if (!service_camera_is_inited()) {
        int ret = service_camera_init();
        if (ret != 0) {
            LOGE("take photo failed: camera init error %d", ret);
            return take_photo_result_text(name, "摄像头初始化失败，无法拍照。", true);
        }
    }

    LOGI("take photo request accepted, mode=mcp, delay_ms=%u", 3000U);

    req.mode = VOICE_MSG_CAMERA_PREVIEW_MODE_MCP_PHOTO;
    req.auto_capture_delay_ms = 3000;
    strncpy(req.context_id, id, sizeof(req.context_id) - 1);

    if (voice_msg_pub(VOICE_MSG_APP_CAMERA_PREVIEW_START, &req, sizeof(req)) != 0) {
        LOGE("take photo failed: preview start publish error");
        return take_photo_result_text(name, "拍照请求发送失败。", true);
    }

    LOGI("take photo preview start published");

    return take_photo_result_text(name, "已完成操作", false);
}

MCP_TOOL_DEFINE(ls.built_in.take_photo, take_photo_list, take_photo_call);

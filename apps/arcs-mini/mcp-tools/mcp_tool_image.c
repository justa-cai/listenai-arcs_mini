#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "cJSON.h"
#include "lisa_log.h"
#include "mcp.h"
#include "voice_msg.h"
#include "image_url_utils.h"
#include "service_image.h"

#define TAG "mcp_tool_image"

static cJSON *show_image_list(const char *name)
{
    cJSON *tool = mcp_tool_list_info_create_default(name, "显示网络图片，参数为图片URL");
    if (!tool) {
        return NULL;
    }

    mcp_tool_info_add_property(tool, "url", "图片URL，支持http/https", "string", true);
    return tool;
}

static cJSON *show_image_call(const char *id, const char *name, cJSON *args)
{
    (void)id;

    const cJSON *url_json = mcp_tool_call_args_get(args, "url");
    if (!url_json || !cJSON_IsString(url_json) || !url_json->valuestring || url_json->valuestring[0] == '\0') {
        LOGE("url parameter not found or invalid");
        return NULL;
    }

    const char *url = url_json->valuestring;
    char display_url[768] = {0};
    if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0) {
        LOGE("url format invalid: %s", url);
        return NULL;
    }

    if (build_image_display_url(url, display_url, sizeof(display_url)) != 0) {
        LOGE("build display url failed: %s", url);
        return NULL;
    }

    if (!service_image_waiting_get()) {
        LOGW("drop image url because not in waiting state");
    } else {
        LOGI("show image url: %s", display_url);
        voice_msg_pub(VOICE_MSG_CLOUD_MCP_LOADING, NULL, 0);
        voice_msg_pub(VOICE_MSG_CLOUD_MCP_IMAGE_URL, (void *)display_url, strlen(display_url) + 1);
    }

    cJSON *result = mcp_tool_call_result_create(name);
    if (!result) {
        return NULL;
    }

    cJSON *content_array = cJSON_CreateArray();
    cJSON *content_item = cJSON_CreateObject();
    cJSON_AddStringToObject(content_item, "type", "text");
    cJSON_AddStringToObject(content_item, "text", "已完成操作");
    cJSON_AddItemToArray(content_array, content_item);
    cJSON_AddItemToObject(result, "content", content_array);
    cJSON_AddBoolToObject(result, "isError", false);

    return result;
}

MCP_TOOL_DEFINE(ls.built_in.show_image, show_image_list, show_image_call);

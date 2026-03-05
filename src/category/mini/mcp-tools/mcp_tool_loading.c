#include <stdbool.h>
#include <string.h>

#include "lisa_log.h"
#include "voice_msg.h"
#include "service_image.h"

#include "mcp.h"
#include "cJSON.h"

static cJSON *show_loading_list(const char *name)
{
    cJSON *tool = mcp_tool_list_info_create_default(name, "显示等待状态");
    if (!tool) {
        return NULL;
    }

    return tool;
}

static cJSON *show_loading_call(const char *id, const char *name, cJSON *args)
{
    (void)id;

    const char *loading_text = "请稍等...";

    if (args && cJSON_IsObject(args)) {
        cJSON *text_item = cJSON_GetObjectItem(args, "text");
        if (text_item && cJSON_IsString(text_item) && text_item->valuestring) {
            loading_text = text_item->valuestring;
        }
    }

    if (loading_text) {
        LOGI("loading_text=%s", loading_text);
    } else {
        LOGW("default loading_text");
    }

    voice_msg_pub(VOICE_MSG_CLOUD_MCP_LOADING, (void *)loading_text, strlen(loading_text) + 1);
    service_image_waiting_start();

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

MCP_TOOL_DEFINE(ls.built_in.show_loading, show_loading_list, show_loading_call);

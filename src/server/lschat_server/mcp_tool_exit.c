#include <stdbool.h>

#include "lisa_log.h"
#include "voice_msg.h"
#include "player_mgr.h"

#include "mcp.h"
#include "cJSON.h"

static cJSON *exit_list(const char *name)
{
    cJSON *tool = mcp_tool_list_info_create_default(name, "退出对话");
    if (!tool) {
        return NULL;
    }

    return tool;
}

static cJSON *exit_call(const char *id, const char *name, cJSON *args)
{
    LOGI("mcp exit received");

    cJSON *result = mcp_tool_call_result_create(name);
    if (!result) {
        return NULL;
    }

    voice_msg_pub(VOICE_MSG_CLOUD_MCP_CHAT_EXIT, NULL, 0);

    cJSON *content_array = cJSON_CreateArray();
    cJSON *content_item = cJSON_CreateObject();
    cJSON_AddStringToObject(content_item, "type", "text");
    cJSON_AddStringToObject(content_item, "text", "已完成操作");
    cJSON_AddItemToArray(content_array, content_item);
    cJSON_AddItemToObject(result, "content", content_array);
    cJSON_AddBoolToObject(result, "isError", false);

    return result;
}

MCP_TOOL_DEFINE(ls.built_in.exit, exit_list, exit_call);

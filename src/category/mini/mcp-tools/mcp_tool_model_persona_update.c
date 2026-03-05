#include <stdbool.h>
#include <string.h>

#include "cJSON.h"
#include "lisa_log.h"

#include "mcp.h"
#include "voice_msg.h"

#define TAG "mcp_model_persona_update"

static cJSON *model_persona_update_list(const char *name)
{
    cJSON *tool = cJSON_CreateObject();
    if (!tool) {
        LOGE("Failed to create tool JSON object");
        return NULL;
    }

    cJSON_AddStringToObject(tool, "name", name);
    cJSON_AddStringToObject(tool, "description", "修改大模型设置或者更新助手人设配置");

    cJSON *inputSchema = cJSON_CreateObject();
    if (!inputSchema) {
        cJSON_Delete(tool);
        return NULL;
    }
    cJSON_AddStringToObject(inputSchema, "type", "object");

    cJSON *properties = cJSON_CreateObject();
    if (!properties) {
        cJSON_Delete(inputSchema);
        cJSON_Delete(tool);
        return NULL;
    }
    cJSON_AddItemToObject(inputSchema, "properties", properties);

    cJSON *required = cJSON_CreateArray();
    if (!required) {
        cJSON_Delete(tool);
        return NULL;
    }
    cJSON_AddItemToObject(inputSchema, "required", required);

    cJSON_AddItemToObject(tool, "inputSchema", inputSchema);

    return tool;
}

static cJSON *model_persona_update_call(const char *id, const char *name, cJSON *args)
{
    (void)args;

    LOGI("Tool call received, tool=%s, id=%s", name ? name : "NULL", id ? id : "NULL");
    voice_msg_pub(VOICE_MSG_CLOUD_OPEN_INFO, NULL, 0);
    LOGI("VOICE_MSG_CLOUD_OPEN_INFO published");

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
        cJSON_Delete(result);
        return NULL;
    }

    cJSON_AddStringToObject(content_item, "type", "text");
    cJSON_AddStringToObject(content_item, "text", "已完成操作");
    cJSON_AddItemToArray(content_array, content_item);
    cJSON_AddItemToObject(result, "content", content_array);
    cJSON_AddBoolToObject(result, "isError", false);

    LOGI("Tool call handled");

    return result;
}

MCP_TOOL_DEFINE(ls.built_in.model_persona_update, model_persona_update_list, model_persona_update_call);

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "lisa_log.h"
#include "mcp.h"
#include "app_datas.h"
#include "kv_user.h"
#include "lisa_kv.h"
#include "voice_cloud.h"

#include "apps/llm/models/model_voice.h"
#include "voice_msg.h"


#define TAG "mcp_tool_duplex"

static const char *interaction_mode_desc(app_interaction_mode_t mode)
{
    switch (mode) {
    case APP_INTERACTION_MODE_FULL_DUPLEX:
        return "全双工可打断模式";
    case APP_INTERACTION_MODE_MULTI_NO_INTERRUPT:
        return "全双工不可打断模式";
    case APP_INTERACTION_MODE_SINGLE:
        return "单工模式";
    default:
        return "未知模式";
    }
}

static bool interaction_mode_parse(cJSON *args, app_interaction_mode_t *mode)
{
    const cJSON *mode_json = mcp_tool_call_args_get(args, "mode");
    if (mode_json && cJSON_IsString(mode_json) && mode_json->valuestring) {
        const char *mode_str = mode_json->valuestring;
        if (!strcmp(mode_str, "single")) {
            *mode = APP_INTERACTION_MODE_SINGLE;
            return true;
        }
        if (!strcmp(mode_str, "duplex")) {
            *mode = APP_INTERACTION_MODE_FULL_DUPLEX;
            return true;
        }
        if (!strcmp(mode_str, "duplex_no_interrupt")) {
            *mode = APP_INTERACTION_MODE_MULTI_NO_INTERRUPT;
            return true;
        }
    }

    return false;
}

static cJSON *duplex_switch_list(const char *name)
{
    cJSON *tool = mcp_tool_list_info_create_default(name,
        "该工具用于切换交互模式：全双工可打断、全双工不可打断、单工模式");
    if (!tool) {
        return NULL;
    }

    cJSON *mode_property = cJSON_CreateObject();
    cJSON *mode_enum = cJSON_CreateArray();
    if (!mode_property || !mode_enum) {
        cJSON_Delete(mode_property);
        cJSON_Delete(mode_enum);
        cJSON_Delete(tool);
        return NULL;
    }

    cJSON_AddStringToObject(mode_property, "type", "string");
    cJSON_AddStringToObject(mode_property, "description", "Interaction mode value.");
    cJSON_AddItemToArray(mode_enum, cJSON_CreateString("single"));
    cJSON_AddItemToArray(mode_enum, cJSON_CreateString("duplex"));
    cJSON_AddItemToArray(mode_enum, cJSON_CreateString("duplex_no_interrupt"));
    cJSON_AddItemToObject(mode_property, "enum", mode_enum);
    mcp_tool_info_add_json_property(tool, "mode", mode_property, true);

    return tool;
}

static cJSON *duplex_switch_call(const char *id, const char *name, cJSON *args)
{
    app_interaction_mode_t interaction_mode = APP_INTERACTION_MODE_SINGLE;
    if (!interaction_mode_parse(args, &interaction_mode)) {
        LOGE("mode parameter not found or invalid");
        return NULL;
    }

    LOGI("switch interaction mode -> %d", interaction_mode);

    if (model_voice_interaction_mode_set((int)interaction_mode) != 0) {
        LOGE("model_voice_interaction_mode_set failed");
        return NULL;
    }

    voice_msg_pub(VOICE_MSG_CLOUD_MCP_CHAT_EXIT, NULL, 0);
    
    cJSON *result = mcp_tool_call_result_create(name);
    if (!result) {
        return NULL;
    }

    char text[96];
    snprintf(text, sizeof(text), "已切换到%s", interaction_mode_desc(interaction_mode));

    cJSON *content_array = cJSON_CreateArray();
    cJSON *content_item = cJSON_CreateObject();
    cJSON_AddStringToObject(content_item, "type", "text");
    cJSON_AddStringToObject(content_item, "text", text);
    cJSON_AddItemToArray(content_array, content_item);
    cJSON_AddItemToObject(result, "content", content_array);
    cJSON_AddBoolToObject(result, "isError", false);

    return result;
}

MCP_TOOL_DEFINE(ls.built_in.switch_full_duplex_v2, duplex_switch_list, duplex_switch_call);

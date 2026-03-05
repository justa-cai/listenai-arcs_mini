#include <stdbool.h>
#include <string.h>

#include "cJSON.h"
#include "lisa_log.h"

#include "mcp.h"
#include "voice_msg.h"

static cJSON *ls_play_audio_url_items_create(void)
{
    cJSON *items = cJSON_CreateObject();
    cJSON_AddStringToObject(items, "type", "array");

    cJSON *properties = cJSON_CreateObject();

    cJSON *url = cJSON_CreateObject();
    cJSON_AddStringToObject(url, "type", "string");
    cJSON_AddStringToObject(url, "description", "音频资源地址");
    cJSON_AddItemToObject(properties, "url", url);

    cJSON *name = cJSON_CreateObject();
    cJSON_AddStringToObject(name, "type", "string");
    cJSON_AddStringToObject(name, "description", "音频资源名称");
    cJSON_AddItemToObject(properties, "name", name);

    cJSON_AddItemToObject(items, "properties", properties);

    cJSON *required = cJSON_CreateArray();
    cJSON_AddItemToArray(required, cJSON_CreateString("url"));

    cJSON_AddItemToObject(items, "required", required);

    return items;
}

static cJSON *ls_play_audio_url_list(const char *name)
{
    cJSON *tool = mcp_tool_list_info_create_default(name, "播放音频URL");
    if (!tool) {
        return NULL;
    }

    cJSON *properties = mcp_tool_info_properties_get(tool);
    if (!properties) {
        cJSON_Delete(tool);
        return NULL;
    }

    cJSON *result = cJSON_CreateObject();
    cJSON *items = ls_play_audio_url_items_create();
    cJSON_AddItemToObject(result, "items", items);

    mcp_tool_info_add_json_property(tool, "result", items, true);

    return tool;
}

static cJSON *ls_play_audio_url_call(const char *id, const char *name, cJSON *args)
{
    const cJSON *url = mcp_tool_call_args_get(args, "url");
    if (!url) {
        LOGE("mcp tool call args get url failed");
        return NULL;
    }

    voice_msg_pub(VOICE_MSG_CLOUD_AUDIO_URL, cJSON_GetStringValue(url), strlen(cJSON_GetStringValue(url)) + 1);

    cJSON *result = mcp_tool_call_result_create(name);
    if (!result) {
        return NULL;
    }

    cJSON_AddStringToObject(result, "content", "播放成功");

    return result;
}

MCP_TOOL_DEFINE(ls.built_in.play_audio_link, ls_play_audio_url_list, ls_play_audio_url_call);

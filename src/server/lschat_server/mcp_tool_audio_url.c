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

#define _MIN(a, b) ((a) < (b) ? (a) : (b))

static cJSON *ls_play_audio_url_call(const char *id, const char *name, cJSON *args)
{
    if (args == NULL) {
        LOGE("mcp tool call args is NULL");
        return NULL;
    }

    /* 解析 result.items[] 数组 */
    cJSON *item_array = NULL;
    if (args->type != cJSON_Array) {
        cJSON *result = cJSON_GetObjectItem(args, "result");

        if (cJSON_IsObject(result) == false) {
            LOGE("mcp tool call args missing result object");
            return NULL;
        }

        item_array = cJSON_GetObjectItem(result, "items");
        if (cJSON_IsArray(item_array) == false) {
            LOGE("mcp tool call args items is not array");
            return NULL;
        }
    } else {
        item_array = args;
    }

    int cnt = cJSON_GetArraySize(item_array);
    if (cnt <= 0) {
        LOGE("mcp tool call args items array is empty");
        return NULL;
    }

    size_t music_items_size =
        sizeof(struct voice_msg_audio_items) + cnt * sizeof(struct voice_msg_audio_item);
    struct voice_msg_audio_items *music_items = lisa_mem_calloc(1, music_items_size);
    if (!music_items) {
        LOGE("Failed to allocate memory");
        return NULL;
    }
    music_items->cnt = cnt;

    for (int i = 0; i < cnt; i++) {
        cJSON *item = cJSON_GetArrayItem(item_array, i);

        const cJSON *item_name = cJSON_GetObjectItem(item, "name");
        const cJSON *item_url = cJSON_GetObjectItem(item, "url");

        music_items->items[i].playable = 1;

        if (cJSON_IsString(item_name)) {
            memcpy(music_items->items[i].name, item_name->valuestring,
                   _MIN(strlen(item_name->valuestring) + 1, sizeof(music_items->items[i].name)));
        }

        if (cJSON_IsString(item_url)) {
            memcpy(music_items->items[i].url, item_url->valuestring,
                   _MIN(strlen(item_url->valuestring) + 1, sizeof(music_items->items[i].url)));
        }
    }

    voice_msg_pub(VOICE_MSG_CLOUD_MCP_CHAT_EXIT, NULL, 0);

    voice_msg_pub(VOICE_MSG_CLOUD_AUDIO_ITEM, music_items, music_items_size);
    lisa_mem_free(music_items);

    cJSON *result = mcp_tool_call_result_create(name);
    if (!result) {
        return NULL;
    }

    cJSON_AddStringToObject(result, "content", "播放成功");

    return result;
}

MCP_TOOL_DEFINE(ls.built_in.play_audio_link, ls_play_audio_url_list, ls_play_audio_url_call);

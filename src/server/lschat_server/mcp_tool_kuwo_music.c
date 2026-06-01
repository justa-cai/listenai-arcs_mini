#include <stdbool.h>
#include <string.h>

#include "cJSON.h"
#include "lisa_log.h"

#include "mcp.h"
#include "voice_msg.h"

static cJSON *ls_play_kuwo_music_items_create(void)
{
    cJSON *items = cJSON_CreateObject();
    cJSON_AddStringToObject(items, "type", "array");

    cJSON *properties = cJSON_CreateObject();

    cJSON *name = cJSON_CreateObject();
    cJSON_AddStringToObject(name, "type", "string");
    cJSON_AddStringToObject(name, "description", "音乐名称");
    cJSON_AddItemToObject(properties, "name", name);

    cJSON *playable = cJSON_CreateObject();
    cJSON_AddStringToObject(playable, "type", "integer");
    cJSON_AddStringToObject(playable, "description", "是否可播放");
    cJSON_AddItemToObject(properties, "playable", playable);

    cJSON *itemid = cJSON_CreateObject();
    cJSON_AddStringToObject(itemid, "type", "string");
    cJSON_AddStringToObject(itemid, "description", "音乐唯一标识ID");
    cJSON_AddItemToObject(properties, "itemid", itemid);

    cJSON_AddItemToObject(items, "properties", properties);

    cJSON *required = cJSON_CreateArray();
    cJSON_AddItemToArray(required, cJSON_CreateString("itemid"));

    cJSON_AddItemToObject(items, "required", required);

    return items;
}

static cJSON *ls_play_kuwo_music_list(const char *name)
{
    cJSON *tool = mcp_tool_list_info_create_default(name, "播放酷我音乐");
    if (!tool) {
        return NULL;
    }

    cJSON *properties = mcp_tool_info_properties_get(tool);
    if (!properties) {
        cJSON_Delete(tool);
        return NULL;
    }

    cJSON *result = cJSON_CreateObject();
    cJSON *items = ls_play_kuwo_music_items_create();
    cJSON_AddItemToObject(result, "items", items);

    mcp_tool_info_add_json_property(tool, "result", items, true);

    return tool;
}

#define _MIN(a, b) ((a) < (b) ? (a) : (b))

static bool ls_play_kuwo_music_item_is_playable(const cJSON *item)
{
    const cJSON *itemid = cJSON_GetObjectItem(item, "itemid");
    if (!cJSON_IsString(itemid) || itemid->valuestring[0] == '\0') {
        return false;
    }

    const cJSON *playable = cJSON_GetObjectItem(item, "playable");
    return playable == NULL || playable->valueint == 1;
}

static cJSON *ls_play_kuwo_music_no_playable_result(const char *name, const char *reason)
{
    LOGW("No playable kuwo music items: %s", reason);

    cJSON *result = mcp_tool_call_result_create(name);
    if (!result) {
        return NULL;
    }

    cJSON_AddStringToObject(result, "content", "暂无可播放音乐");
    return result;
}


static cJSON *ls_play_kuwo_music_call(const char *id, const char *name, cJSON *args)
{
    if (args == NULL) {
        LOGE("mcp tool call args is NULL");
        return ls_play_kuwo_music_no_playable_result(name, "args is NULL");
    }

    /* NOTE: item_array有可能直接在args中,也有可能在args->result中 */
    cJSON *item_array = NULL;
    if (args->type != cJSON_Array) {
        cJSON *result = cJSON_GetObjectItem(args, "result");

        if (cJSON_IsObject(result) == false) {
            LOGE("mcp tool call args missing result object");
            return ls_play_kuwo_music_no_playable_result(name, "missing result object");
        }

        item_array = cJSON_GetObjectItem(result, "items");
        if (cJSON_IsArray(item_array) == false) {
            LOGE("mcp tool call args items is not array");
            return ls_play_kuwo_music_no_playable_result(name, "items is not array");
        }

    } else {
        item_array = args;
    }

    int cnt = cJSON_GetArraySize(item_array);
    if (cnt <= 0) {
        return ls_play_kuwo_music_no_playable_result(name, "items array is empty");
    }

    int playable_cnt = 0;
    for (int i = 0; i < cnt; i++) {
        cJSON *item = cJSON_GetArrayItem(item_array, i);
        if (ls_play_kuwo_music_item_is_playable(item)) {
            playable_cnt++;
        }
    }

    if (playable_cnt == 0) {
        return ls_play_kuwo_music_no_playable_result(name, "all items are unplayable or invalid");
    }

    size_t music_items_size =
        sizeof(struct voice_msg_audio_items) + playable_cnt * sizeof(struct voice_msg_audio_item);
    struct voice_msg_audio_items *music_items = lisa_mem_calloc(1, music_items_size);
    if (!music_items) {
        LOGE("Failed to allocate memory");
        return NULL;
    }
    music_items->cnt = playable_cnt;

    int music_index = 0;
    for (int i = 0; i < cnt; i++) {
        cJSON *item = cJSON_GetArrayItem(item_array, i);
        if (!ls_play_kuwo_music_item_is_playable(item)) {
            continue;
        }

        const cJSON *itemid = cJSON_GetObjectItem(item, "itemid");
        const cJSON *playable = cJSON_GetObjectItem(item, "playable");
        const cJSON *name = cJSON_GetObjectItem(item, "name");

        if (itemid) {
            memcpy(music_items->items[music_index].id, itemid->valuestring,
                   _MIN(strlen(itemid->valuestring) + 1, sizeof(music_items->items[music_index].id)));
        }

        if (playable) {
            music_items->items[music_index].playable = playable->valueint;
        } else {
            music_items->items[music_index].playable = 1;
        }

        if (cJSON_IsString(name)) {
            memcpy(music_items->items[music_index].name, name->valuestring,
                   _MIN(strlen(name->valuestring) + 1, sizeof(music_items->items[music_index].name)));
        }

        music_index++;
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

MCP_TOOL_DEFINE(ls.built_in.play_kuwo_music, ls_play_kuwo_music_list, ls_play_kuwo_music_call);

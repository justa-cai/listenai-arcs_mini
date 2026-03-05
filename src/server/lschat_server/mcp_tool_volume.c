#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "cJSON.h"
#include "lisa_log.h"
#include "mcp.h"
#include "player_mgr.h"

#define TAG "mcp_tool_volume"

static cJSON *volume_control_list(const char *name)
{
    cJSON *tool = mcp_tool_list_info_create_default(name,
        "音量控制与调节工具。当用户发出调节音量、控制声音大小的指令时调用。支持绝对值设置（如：调到50%）、相对值调节（如：大声点、调小2档）以及极值控制（如：静音、最大声）。");
    if (!tool) {
        return NULL;
    }

    mcp_tool_info_add_property(tool, "text", "用户输入的关于音量调节的意图", "string", true);

    cJSON *intent_property = cJSON_CreateObject();
    cJSON_AddStringToObject(intent_property, "type", "string");
    cJSON_AddStringToObject(intent_property, "description",
        "调节类型：set（设定到具体值或极值，如：调到10档、最大声、最小声）；adjustUp（调大声、加两档）；adjustDown（调小声、小一点）");
    cJSON *intent_enum = cJSON_CreateArray();
    cJSON_AddItemToArray(intent_enum, cJSON_CreateString("set"));
    cJSON_AddItemToArray(intent_enum, cJSON_CreateString("adjustUp"));
    cJSON_AddItemToArray(intent_enum, cJSON_CreateString("adjustDown"));
    cJSON_AddItemToObject(intent_property, "enum", intent_enum);
    mcp_tool_info_add_json_property(tool, "intent", intent_property, true);

    mcp_tool_info_add_property(tool, "value",
        "具体的数值或状态：1. 数字（如：1, 10, 50）；2. 标准状态位：max（最大声、满格）、min（最小声、静音）、mid（中等音量），如果没有明确参数，则留空，如（大点儿声音）",
        "string", false);

    cJSON *unit_property = cJSON_CreateObject();
    cJSON_AddStringToObject(unit_property, "type", "string");
    cJSON_AddStringToObject(unit_property, "description", "描述数值的单位。若指令中提及'档'、'级'、'%'则对应填写，否则留空。");
    cJSON *unit_enum = cJSON_CreateArray();
    cJSON_AddItemToArray(unit_enum, cJSON_CreateString("档"));
    cJSON_AddItemToArray(unit_enum, cJSON_CreateString("级"));
    cJSON_AddItemToArray(unit_enum, cJSON_CreateString("百分比"));
    cJSON_AddItemToArray(unit_enum, cJSON_CreateString(""));
    cJSON_AddItemToObject(unit_property, "enum", unit_enum);
    mcp_tool_info_add_json_property(tool, "unit", unit_property, false);

    return tool;
}
static int volume_adjust(int delta)
{
    int current_volume = player_mgr_get_volume();
    int new_volume = current_volume + delta;

    if (new_volume > 100) {
        new_volume = 100;
    } else if (new_volume < 0) {
        new_volume = 0;
    }

    return player_mgr_set_volume(new_volume);
}

static cJSON *volume_control_call(const char *id, const char *name, cJSON *args)
{
    const cJSON *text_json = mcp_tool_call_args_get(args, "text");
    const cJSON *intent_json = mcp_tool_call_args_get(args, "intent");
    const cJSON *value_json = mcp_tool_call_args_get(args, "value");
    const cJSON *unit_json = mcp_tool_call_args_get(args, "unit");

    if (!text_json || !cJSON_IsString(text_json)) {
        LOGE("text parameter not found or invalid");
        return NULL;
    }

    if (!intent_json || !cJSON_IsString(intent_json)) {
        LOGE("intent parameter not found or invalid");
        return NULL;
    }

    const char *text = text_json->valuestring;
    const char *intent = intent_json->valuestring;
    const char *value = value_json && cJSON_IsString(value_json) ? value_json->valuestring : NULL;
    const char *unit = unit_json && cJSON_IsString(unit_json) ? unit_json->valuestring : "";

    LOGI("Volume control - text: %s, intent: %s, value: %s, unit: %s",
         text, intent, value ? value : "null", unit);

    if (strcmp(intent, "set") == 0) {
        if (value) {
            if (strcmp(value, "max") == 0) {
                player_mgr_set_volume(100);
            } else if (strcmp(value, "min") == 0) {
                player_mgr_set_volume(0);
            } else if (strcmp(value, "mid") == 0) {
                player_mgr_set_volume(50);
            } else {
                int target_vol = atoi(value);
                player_mgr_set_volume(target_vol);
            }
        }
    } else if (strcmp(intent, "adjustUp") == 0) {
        int adjust_step = 10;
        if (value) {
            adjust_step = atoi(value);
            if (adjust_step <= 0) adjust_step = 10;
        }
        volume_adjust(adjust_step);
    } else if (strcmp(intent, "adjustDown") == 0) {
        int adjust_step = 10;
        if (value) {
            adjust_step = atoi(value);
            if (adjust_step <= 0) adjust_step = 10;
        }
        volume_adjust(-adjust_step);
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

static cJSON *volume_get_list(const char *name)
{
    cJSON *tool = mcp_tool_list_info_create_default(name, "获取当前音量。");
    if (!tool) {
        return NULL;
    }

    return tool;
}

static cJSON *volume_get_call(const char *id, const char *name, cJSON *args)
{
    int current_vol = player_mgr_get_volume();

    LOGI("Get volume: %d", current_vol);

    cJSON *result = mcp_tool_call_result_create(name);
    if (!result) {
        return NULL;
    }

    char volume_text[64];
    snprintf(volume_text, sizeof(volume_text), "当前音量为 %d", current_vol);

    cJSON *content_array = cJSON_CreateArray();
    cJSON *content_item = cJSON_CreateObject();
    cJSON_AddStringToObject(content_item, "type", "text");
    cJSON_AddStringToObject(content_item, "text", volume_text);
    cJSON_AddItemToArray(content_array, content_item);
    cJSON_AddItemToObject(result, "content", content_array);
    cJSON_AddBoolToObject(result, "isError", false);

    return result;
}

MCP_TOOL_DEFINE(ls.set_volume, volume_control_list, volume_control_call);
MCP_TOOL_DEFINE(ls.get_volume, volume_get_list, volume_get_call);

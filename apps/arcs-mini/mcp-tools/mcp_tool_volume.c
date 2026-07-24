#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define TAG "mcp_tool_volume"

#include "cJSON.h"
#include "lisa_log.h"
#include "mcp.h"
#include "service_volume.h"

static int volume_parse_value(const cJSON *value_json, int default_value)
{
    if (!value_json) {
        return default_value;
    }

    if (cJSON_IsNumber(value_json)) {
        return value_json->valueint;
    }

    if (cJSON_IsString(value_json) && value_json->valuestring) {
        return atoi(value_json->valuestring);
    }

    return default_value;
}

static const char *volume_get_string_value(const cJSON *value_json)
{
    return value_json && cJSON_IsString(value_json) ? value_json->valuestring : NULL;
}

static int volume_to_percent(int value, const char *unit)
{
    if (unit && (strcmp(unit, "档") == 0 || strcmp(unit, "级") == 0)) {
        return value * 10;
    }

    return value;
}

static int volume_parse_adjust_step(const char *value, const char *unit)
{
    int step = 10;

    if (value && value[0] != '\0') {
        int parsed = atoi(value);
        if (parsed > 0) {
            if (unit && strcmp(unit, "百分比") == 0) {
                step = parsed;
            } else {
                step = parsed * 10;
            }
        }
    }

    return step;
}

static int volume_parse_adjust_step_json(const cJSON *value_json, const char *unit)
{
    int step = 10;

    if (value_json) {
        int parsed = volume_parse_value(value_json, 0);
        if (parsed > 0) {
            if (unit && strcmp(unit, "百分比") == 0) {
                step = parsed;
            } else {
                step = parsed * 10;
            }
        }
    }

    return step;
}

static int volume_parse_set_target(const cJSON *value_json, const char *unit)
{
    const char *value = volume_get_string_value(value_json);

    if (value) {
        if (strcmp(value, "max") == 0) {
            return 100;
        }
        if (strcmp(value, "min") == 0) {
            return 0;
        }
        if (strcmp(value, "mid") == 0) {
            return 50;
        }
    }

    return volume_to_percent(volume_parse_value(value_json, service_volume_get()), unit);
}

static cJSON *volume_control_list(const char *name)
{
    cJSON *tool = mcp_tool_list_info_create_default(name,
        "用于调节设备音量，支持设置绝对数值、相对调大或调小，以及静音、最小声、中等音量、最大声等操作；不得用于屏幕亮度。强制调用顺序：首次响应必须先输出非空自然语言“我来处理设备控制。”，工具调用必须作为后续事件发送；禁止首事件直接调用工具，也禁止调用后再补前置文本。若需要先查询当前音量再设置，前置文本也必须出现在第一个工具调用之前。音量上限为100；查询结果低于100不代表已经达到最大音量。");
    if (!tool) {
        return NULL;
    }

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
    cJSON_AddStringToObject(unit_property, "description", "描述数值的单位。若指令中提及'档'、'级'、'%%'则对应填写，否则留空。");
    cJSON *unit_enum = cJSON_CreateArray();
    cJSON_AddItemToArray(unit_enum, cJSON_CreateString("档"));
    cJSON_AddItemToArray(unit_enum, cJSON_CreateString("级"));
    cJSON_AddItemToArray(unit_enum, cJSON_CreateString("百分比"));
    cJSON_AddItemToArray(unit_enum, cJSON_CreateString(""));
    cJSON_AddItemToObject(unit_property, "enum", unit_enum);
    mcp_tool_info_add_json_property(tool, "unit", unit_property, false);

    return tool;
}

static cJSON *volume_control_call(const char *id, const char *name, cJSON *args)
{

    const cJSON *intent_json = mcp_tool_call_args_get(args, "intent");
    const cJSON *value_json = mcp_tool_call_args_get(args, "value");
    const cJSON *unit_json = mcp_tool_call_args_get(args, "unit");

    if (!intent_json || !cJSON_IsString(intent_json)) {
        LOGE("intent parameter not found or invalid");
        return NULL;
    }

    const char *intent = intent_json->valuestring;
    const char *value = volume_get_string_value(value_json);
    const char *unit = unit_json && cJSON_IsString(unit_json) ? unit_json->valuestring : "";

    if (value_json && cJSON_IsNumber(value_json)) {
        LOGI("Volume control: intent=%s, value=%d, unit=%s",
             intent, value_json->valueint, unit);
    } else {
        LOGI("Volume control: intent=%s, value=%s, unit=%s",
             intent, value ? value : "null", unit);
    }

    if (strcmp(intent, "set") == 0) {
        if (value_json) {
            service_volume_set(volume_parse_set_target(value_json, unit));
        }
    } else if (strcmp(intent, "adjustUp") == 0) {
        service_volume_adjust(value ? volume_parse_adjust_step(value, unit) :
                                    volume_parse_adjust_step_json(value_json, unit));
    } else if (strcmp(intent, "adjustDown") == 0) {
        service_volume_adjust(value ? -volume_parse_adjust_step(value, unit) :
                                    -volume_parse_adjust_step_json(value_json, unit));
    }

    cJSON *result = mcp_tool_call_result_create(name);
    if (!result) {
        return NULL;
    }

    cJSON *content_array = cJSON_CreateArray();
    cJSON *content_item = cJSON_CreateObject();
    int current_vol = service_volume_get();
    char volume_text[64];
    snprintf(volume_text, sizeof(volume_text), "已设置音量为%d%%", current_vol);
    cJSON_AddStringToObject(content_item, "type", "text");
    cJSON_AddStringToObject(content_item, "text", volume_text);
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
    int current_vol = service_volume_get();

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

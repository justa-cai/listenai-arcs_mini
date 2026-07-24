#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define TAG "mcp.tool.brightness"

#include "cJSON.h"
#include "lisa_log.h"
#include "mcp.h"
#include "service_brightness.h"

static int brightness_clamp(int value)
{
    if (value < 0) {
        return 0;
    }
    if (value > 100) {
        return 100;
    }
    return value;
}

static cJSON *brightness_result(const char *name, const char *text, bool is_error)
{
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
        cJSON_Delete(content_item);
        cJSON_Delete(result);
        return NULL;
    }

    cJSON_AddStringToObject(content_item, "type", "text");
    cJSON_AddStringToObject(content_item, "text", text);
    cJSON_AddItemToArray(content_array, content_item);
    cJSON_AddItemToObject(result, "content", content_array);
    cJSON_AddBoolToObject(result, "isError", is_error);

    return result;
}

static cJSON *brightness_control_list(const char *name)
{
    cJSON *tool = mcp_tool_list_info_create_default(
        name,
        "用于调节设备屏幕亮度，支持设置绝对百分比、相对调亮或调暗，以及最亮、最暗等极值操作；不得用于音量或其他设备控制。强制调用顺序：首次响应必须先输出非空自然语言“我来处理设备控制。”，工具调用必须作为后续事件发送；禁止首事件直接调用工具，也禁止调用后再补前置文本。用户表达“调到50%%亮度、屏幕亮一点、调暗一档、最亮、最暗”等意图时调用本工具。");
    if (!tool) {
        return NULL;
    }

    cJSON *intent_property = cJSON_CreateObject();
    cJSON_AddStringToObject(intent_property, "type", "string");
    cJSON_AddStringToObject(intent_property, "description",
        "调节类型：set（设定到具体值或极值，如：调到50%%亮度、最亮、最暗）；adjustUp（调亮、加一档）；adjustDown（调暗、暗一点）");
    cJSON *intent_enum = cJSON_CreateArray();
    cJSON_AddItemToArray(intent_enum, cJSON_CreateString("set"));
    cJSON_AddItemToArray(intent_enum, cJSON_CreateString("adjustUp"));
    cJSON_AddItemToArray(intent_enum, cJSON_CreateString("adjustDown"));
    cJSON_AddItemToObject(intent_property, "enum", intent_enum);
    mcp_tool_info_add_json_property(tool, "intent", intent_property, true);

    mcp_tool_info_add_property(
        tool,
        "value",
        "具体的数值或状态：1.数字为 10、50 等时，代表绝对亮度百分比；数字为 1、2 等时，代表相对调整档位（每档 10%%）；2. 标准状态位：max 对应最亮，min 对应最暗；无明确参数时留空（如 “调亮一点儿”）。",
        "string",
        false);

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

static cJSON *brightness_control_call(const char *id, const char *name, cJSON *args)
{
    (void)id;

    const cJSON *intent_json = mcp_tool_call_args_get(args, "intent");
    const cJSON *value_json = mcp_tool_call_args_get(args, "value");
    const cJSON *unit_json = mcp_tool_call_args_get(args, "unit");


    if (!intent_json || !cJSON_IsString(intent_json)) {
        LOGE("intent parameter not found or invalid");
        return brightness_result(name, "intent 参数缺失或格式错误。", true);
    }

    const char *intent = intent_json->valuestring;
    const char *value = value_json && cJSON_IsString(value_json) ? value_json->valuestring : NULL;
    const char *unit = unit_json && cJSON_IsString(unit_json) ? unit_json->valuestring : "";

    LOGI("Brightness control , intent: %s, value: %s, unit: %s", intent, value ? value : "null", unit);

    if (strcmp(intent, "set") == 0) {
        if (value) {
            if (strcmp(value, "max") == 0) {
                service_brightness_set(100);
            } else if (strcmp(value, "min") == 0) {
                service_brightness_set(0);
            } else {
                int target = brightness_clamp(atoi(value));
                service_brightness_set(target);
            }
        }
    } else if (strcmp(intent, "adjustUp") == 0 || strcmp(intent, "adjustDown") == 0) {
        int step = 10;
        if (value) {
            int parsed = atoi(value);
            if (parsed > 0) {
                if (strcmp(unit, "百分比") == 0) {
                    step = parsed;
                } else {
                    step = parsed * 10;
                }
            }
        }

        int current = service_brightness_get();
        int target = current;
        if (strcmp(intent, "adjustUp") == 0) {
            target = brightness_clamp(current + step);
        } else {
            target = brightness_clamp(current - step);
        }
        service_brightness_set(target);
    } else {
        LOGE("Unsupported intent: %s", intent);
        return brightness_result(name, "intent 不支持，必须是 set/adjustUp/adjustDown。", true);
    }

    return brightness_result(name, "已完成操作", false);
}

MCP_TOOL_DEFINE(ls.display_set_brightness, brightness_control_list, brightness_control_call);

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
        "显示器亮度控制与调节工具。当用户发出调节亮度、控制屏幕亮度的指令时调用。支持绝对值设置（如：调到50%%亮度）、相对值调节（如：调亮一点、调暗1档）以及极值控制（如：最亮、最暗）。");
    if (!tool) {
        return NULL;
    }

    mcp_tool_info_add_property(tool, "text", "用户输入的关于亮度调节的意图", "string", true);

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

    const cJSON *text_json = mcp_tool_call_args_get(args, "text");
    const cJSON *intent_json = mcp_tool_call_args_get(args, "intent");
    const cJSON *value_json = mcp_tool_call_args_get(args, "value");
    const cJSON *unit_json = mcp_tool_call_args_get(args, "unit");

    if (!text_json || !cJSON_IsString(text_json)) {
        LOGE("text parameter not found or invalid");
        return brightness_result(name, "text 参数缺失或格式错误。", true);
    }

    if (!intent_json || !cJSON_IsString(intent_json)) {
        LOGE("intent parameter not found or invalid");
        return brightness_result(name, "intent 参数缺失或格式错误。", true);
    }

    const char *text = text_json->valuestring;
    const char *intent = intent_json->valuestring;
    const char *value = value_json && cJSON_IsString(value_json) ? value_json->valuestring : NULL;
    const char *unit = unit_json && cJSON_IsString(unit_json) ? unit_json->valuestring : "";

    LOGI("Brightness control - text: %s, intent: %s, value: %s, unit: %s",
         text, intent, value ? value : "null", unit);

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

#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "cJSON.h"
#include "lisa_log.h"
#include "mcp.h"
#include "service_led.h"

#define TAG "mcp_tool_led"

static cJSON *led_switch_list(const char *name)
{
    cJSON *tool = mcp_tool_list_info_create_default(name, "控制LED开关状态，可以是开启或关闭");
    if (!tool) {
        return NULL;
    }

    mcp_tool_info_add_property(tool, "value", "LED开关状态，打开为true，关闭为false", "boolean", true);

    return tool;
}

static cJSON *led_switch_call(const char *id, const char *name, cJSON *args)
{
    const cJSON *value_json = mcp_tool_call_args_get(args, "value");
    if (!value_json || !cJSON_IsBool(value_json)) {
        LOGE("value parameter not found or invalid");
        return NULL;
    }

    bool value = cJSON_IsTrue(value_json);

    LOGI("LED switch: %s", value ? "on" : "off");

    if (value) {
        service_led_on();
    } else {
        service_led_off();
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

static cJSON *led_blink_list(const char *name)
{
    cJSON *tool = mcp_tool_list_info_create_default(name, "控制LED闪烁模式，可以是关闭、普通、快速或慢速");
    if (!tool) {
        return NULL;
    }

    mcp_tool_info_add_property(tool, "mode", "LED闪烁模式，可以是 'off'、'normal'、'fast' 或 'slow'", "string", true);

    return tool;
}

static cJSON *led_blink_call(const char *id, const char *name, cJSON *args)
{
    const cJSON *mode_json = mcp_tool_call_args_get(args, "mode");
    if (!mode_json || !cJSON_IsString(mode_json)) {
        LOGE("mode parameter not found or invalid");
        return NULL;
    }

    const char *mode = mode_json->valuestring;

    LOGI("LED blink mode: %s", mode);

    if (strcmp(mode, "off") == 0) {
        service_led_off();
    } else if (strcmp(mode, "fast") == 0) {
        service_led_blink(200, 200);
    } else if (strcmp(mode, "slow") == 0) {
        service_led_blink(1000, 1000);
    } else if (strcmp(mode, "normal") == 0) {
        service_led_blink(500, 500);
    } else {
        LOGE("Invalid mode: %s", mode);
        return NULL;
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

MCP_TOOL_DEFINE(ls.led_switch, led_switch_list, led_switch_call);
MCP_TOOL_DEFINE(ls.led_blink, led_blink_list, led_blink_call);

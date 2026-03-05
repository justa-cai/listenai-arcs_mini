#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define TAG "mcp.tool.brightness"

#include "cJSON.h"
#include "lisa_log.h"
#include "mcp.h"
#include "service_brightness.h"

static cJSON *brightness_control_list(const char *name)
{
    cJSON *tool = mcp_tool_list_info_create_default(
        name, "设置显示器的亮度。亮度可以10%为步长进行设置 (0, 10, 20, ..., "
              "100)。当用户无具体设置数值时,得先调用display_get_brightness工具获取当前亮度，再做调整。");
    if (!tool) {
        return NULL;
    }

    mcp_tool_info_add_property(tool, "brightness", "亮度值，以10%为步长 (0-100)", "integer", true);

    return tool;
}

static cJSON *brightness_control_call(const char *id, const char *name, cJSON *args)
{
    const cJSON *brightness_json = mcp_tool_call_args_get(args, "brightness");
    if (!brightness_json) {
        LOGE("brightness parameter not found");
        return NULL;
    }

    int brightness = 0;
    if (cJSON_IsNumber(brightness_json)) {
        brightness = brightness_json->valueint;
    } else if (cJSON_IsString(brightness_json)) {
        brightness = atoi(brightness_json->valuestring);
    } else {
        LOGE("Invalid brightness parameter type");
        return NULL;
    }

    if (brightness < 0) {
        brightness = 0;
    }
    if (brightness > 100) {
        brightness = 100;
    }

    LOGI("Set brightness to %d", brightness);

    service_brightness_set(brightness);

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

MCP_TOOL_DEFINE(ls.display_set_brightness, brightness_control_list, brightness_control_call);

static cJSON *brightness_get_list(const char *name)
{
    cJSON *tool = mcp_tool_list_info_create_default(name, "获取显示器的当前亮度。");
    if (!tool) {
        return NULL;
    }

    return tool;
}

static cJSON *brightness_get_call(const char *id, const char *name, cJSON *args)
{
    int brightness = service_brightness_get();

    LOGI("Get brightness: %d", brightness);

    cJSON *result = mcp_tool_call_result_create(name);
    if (!result) {
        return NULL;
    }

    char brightness_text[64];
    snprintf(brightness_text, sizeof(brightness_text), "当前亮度为 %d", brightness);

    cJSON *content_array = cJSON_CreateArray();
    cJSON *content_item = cJSON_CreateObject();
    cJSON_AddStringToObject(content_item, "type", "text");
    cJSON_AddStringToObject(content_item, "text", brightness_text);
    cJSON_AddItemToArray(content_array, content_item);
    cJSON_AddItemToObject(result, "content", content_array);
    cJSON_AddBoolToObject(result, "isError", false);

    return result;
}

MCP_TOOL_DEFINE(ls.display_get_brightness, brightness_get_list, brightness_get_call);

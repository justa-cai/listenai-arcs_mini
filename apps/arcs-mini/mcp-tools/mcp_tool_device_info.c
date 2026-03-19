#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "lisa_log.h"
#include "mcp.h"
#include "project_version.h"
#include "service_brightness.h"
#include "service_volume.h"

#define TAG "mcp_device_info"

static cJSON *device_info_result_text(const char *name, const char *text, bool is_error)
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

static cJSON *device_info_list(const char *name)
{
    cJSON *tool = mcp_tool_list_info_create_default(
        name,
        "查询设备的状态信息。可以同时获取当前音量、当前屏幕亮度、以及设备固件版本号。当用户询问这些信息时调用。");
    if (!tool) {
        return NULL;
    }

    cJSON *fields_property = cJSON_CreateObject();
    cJSON *items = cJSON_CreateObject();
    cJSON *enum_array = cJSON_CreateArray();
    if (!fields_property || !items || !enum_array) {
        cJSON_Delete(fields_property);
        cJSON_Delete(items);
        cJSON_Delete(enum_array);
        cJSON_Delete(tool);
        return NULL;
    }

    cJSON_AddStringToObject(fields_property, "type", "array");
    cJSON_AddStringToObject(
        fields_property,
        "description",
        "需要查询的信息字段列表。可包含 'volume'（音量）, 'brightness'（亮度）, 'version'（版本号）中的一个或多个。");

    cJSON_AddStringToObject(items, "type", "string");
    cJSON_AddItemToArray(enum_array, cJSON_CreateString("volume"));
    cJSON_AddItemToArray(enum_array, cJSON_CreateString("brightness"));
    cJSON_AddItemToArray(enum_array, cJSON_CreateString("version"));
    cJSON_AddItemToObject(items, "enum", enum_array);
    cJSON_AddItemToObject(fields_property, "items", items);

    mcp_tool_info_add_json_property(tool, "fields", fields_property, true);
    return tool;
}

static cJSON *device_info_call(const char *id, const char *name, cJSON *args)
{
    (void)id;

    const cJSON *fields_json = mcp_tool_call_args_get(args, "fields");
    if (!fields_json || !cJSON_IsArray(fields_json)) {
        return device_info_result_text(name, "参数 fields 缺失或格式错误，必须是数组。", true);
    }

    bool need_volume = false;
    bool need_brightness = false;
    bool need_version = false;

    cJSON *field = NULL;
    cJSON_ArrayForEach(field, fields_json)
    {
        if (!cJSON_IsString(field) || !field->valuestring) {
            return device_info_result_text(name, "fields 中存在非字符串元素。", true);
        }

        if (strcmp(field->valuestring, "volume") == 0) {
            need_volume = true;
        } else if (strcmp(field->valuestring, "brightness") == 0) {
            need_brightness = true;
        } else if (strcmp(field->valuestring, "version") == 0) {
            need_version = true;
        } else {
            char error_text[128];
            snprintf(error_text, sizeof(error_text), "fields 包含不支持的字段: %s", field->valuestring);
            return device_info_result_text(name, error_text, true);
        }
    }

    if (!need_volume && !need_brightness && !need_version) {
        return device_info_result_text(name, "fields 不能为空。", true);
    }

    cJSON *payload = cJSON_CreateObject();
    if (!payload) {
        return NULL;
    }

    if (need_volume) {
        cJSON_AddNumberToObject(payload, "volume", service_volume_get());
    }
    if (need_brightness) {
        cJSON_AddNumberToObject(payload, "brightness", service_brightness_get());
    }
    if (need_version) {
        cJSON_AddStringToObject(payload, "version", PROJECT_VERSION_STR);
    }

    char *payload_text = cJSON_PrintUnformatted(payload);
    cJSON_Delete(payload);
    if (!payload_text) {
        return NULL;
    }

    cJSON *result = device_info_result_text(name, payload_text, false);
    cJSON_free(payload_text);

    LOGI("get_device_info called, volume=%d brightness=%d version=%d",
         need_volume, need_brightness, need_version);

    return result;
}

MCP_TOOL_DEFINE(ls.get_device_info, device_info_list, device_info_call);

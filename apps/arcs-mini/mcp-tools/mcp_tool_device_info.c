#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "battery_ui.h"
#include "lisa_log.h"
#include "mcp.h"
#include "project_version.h"
#include "service_brightness.h"
#include "service_volume.h"
#include "voice_msg.h"

#define TAG "mcp_device_info"
#define DEVICE_STANDBY_TOTAL_MINUTES (6 * 60)

static int device_info_get_standby_minutes(uint8_t battery_level)
{
    return (battery_level * DEVICE_STANDBY_TOTAL_MINUTES) / 100;
}

static void device_info_format_standby_time(char *buffer, size_t buffer_size, int standby_minutes)
{
    int hours = standby_minutes / 60;
    int minutes = standby_minutes % 60;

    snprintf(buffer, buffer_size, "大概是%d小时%d分钟", hours, minutes);
}

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
        "用于查询设备当前状态，可一次查询音量、屏幕亮度、固件版本、电量和待机时间中的一个或多个字段；不得用于修改这些状态。强制调用顺序：首次响应必须先输出非空自然语言“我来查询设备状态。”，工具调用必须作为后续事件发送；禁止首事件直接调用工具，也禁止调用后再补前置文本。仅请求用户实际关心的字段；查询电量或待机时间时设备会同时返回两者。");
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
        "需要查询的信息字段列表。可包含 'volume'（音量）, 'brightness'（亮度）, 'version'（版本号）, 'battery'（电量）, 'standby_time'（待机时间）中的一个或多个。查询电量或待机时间时会同时返回两者。");

    cJSON_AddStringToObject(items, "type", "string");
    cJSON_AddItemToArray(enum_array, cJSON_CreateString("volume"));
    cJSON_AddItemToArray(enum_array, cJSON_CreateString("brightness"));
    cJSON_AddItemToArray(enum_array, cJSON_CreateString("version"));
    cJSON_AddItemToArray(enum_array, cJSON_CreateString("battery"));
    cJSON_AddItemToArray(enum_array, cJSON_CreateString("standby_time"));
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
    bool need_battery = false;
    bool need_standby_time = false;

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
        } else if (strcmp(field->valuestring, "battery") == 0) {
            need_battery = true;
        } else if (strcmp(field->valuestring, "standby_time") == 0) {
            need_standby_time = true;
        } else {
            char error_text[128];
            snprintf(error_text, sizeof(error_text), "fields 包含不支持的字段: %s", field->valuestring);
            return device_info_result_text(name, error_text, true);
        }
    }

    if (!need_volume && !need_brightness && !need_version && !need_battery && !need_standby_time) {
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
    if (need_battery || need_standby_time) {
        voice_msg_battery_info_t battery_info;
        char standby_time_text[64];

        if (!battery_ui_get_info(&battery_info)) {
            cJSON_Delete(payload);
            return device_info_result_text(name, "获取电量失败。", true);
        }

        device_info_format_standby_time(
            standby_time_text,
            sizeof(standby_time_text),
            device_info_get_standby_minutes(battery_info.level));

#ifdef CONFIG_BOARD_ARCS_MINI_DOLL_V2
        voice_msg_pub(VOICE_MSG_APP_BATTERY_QUERY_SHOW, &battery_info, sizeof(battery_info));
#endif

        cJSON_AddNumberToObject(payload, "battery", battery_info.level);
        cJSON_AddStringToObject(payload, "standby_time", standby_time_text);
    }

    char *payload_text = cJSON_PrintUnformatted(payload);
    cJSON_Delete(payload);
    if (!payload_text) {
        return NULL;
    }

    cJSON *result = device_info_result_text(name, payload_text, false);
    cJSON_free(payload_text);

    LOGI("get_device_info called, volume=%d brightness=%d version=%d battery=%d standby_time=%d",
         need_volume, need_brightness, need_version, need_battery, need_standby_time);

    return result;
}

MCP_TOOL_DEFINE(ls.get_device_info, device_info_list, device_info_call);

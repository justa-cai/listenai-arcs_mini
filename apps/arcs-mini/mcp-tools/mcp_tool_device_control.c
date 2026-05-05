#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "lisa_log.h"
#include "mcp.h"
#include "power/power_manager.h"
#include "uboot_features_api.h"
#include "voice_msg.h"

#define TAG "mcp_tool_device_control"

static cJSON *device_control_result_text(const char *name, const char *text, bool is_error)
{
    cJSON *result = mcp_tool_call_result_create(name);
    cJSON *content_array = NULL;
    cJSON *content_item = NULL;

    if (!result) {
        return NULL;
    }

    content_array = cJSON_CreateArray();
    content_item = cJSON_CreateObject();
    if (!content_array || !content_item) {
        cJSON_Delete(content_array);
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

static cJSON *device_control_list(const char *name)
{
    cJSON *tool = mcp_tool_list_info_create_default(name, "设备控制，当前支持重启");
    cJSON *command_property = NULL;
    cJSON *command_enum = NULL;
    cJSON *delay_property = NULL;

    if (!tool) {
        return NULL;
    }

    command_property = cJSON_CreateObject();
    command_enum = cJSON_CreateArray();
    delay_property = cJSON_CreateObject();
    if (!command_property || !command_enum || !delay_property) {
        cJSON_Delete(command_property);
        cJSON_Delete(command_enum);
        cJSON_Delete(delay_property);
        cJSON_Delete(tool);
        return NULL;
    }

    cJSON_AddStringToObject(command_property, "type", "string");
    cJSON_AddStringToObject(command_property, "description",
                            "Device control command. Only `reboot` is supported for now.");
    cJSON_AddItemToArray(command_enum, cJSON_CreateString("reboot"));
    cJSON_AddItemToObject(command_property, "enum", command_enum);
    mcp_tool_info_add_json_property(tool, "command", command_property, true);

    cJSON_AddStringToObject(delay_property, "type", "integer");
    cJSON_AddNumberToObject(delay_property, "minimum", 0);
    cJSON_AddStringToObject(delay_property, "description", "Delay before reboot, in milliseconds.");
    mcp_tool_info_add_json_property(tool, "delay_ms", delay_property, true);

    return tool;
}

static cJSON *device_control_call(const char *id, const char *name, cJSON *args)
{
    const cJSON *command_json = mcp_tool_call_args_get(args, "command");
    const cJSON *delay_json = mcp_tool_call_args_get(args, "delay_ms");
    voice_msg_cloud_reboot_t reboot_msg = {0};
    uint32_t delay_ms = 0;
    char text[96] = {0};

    (void)id;

    if (!command_json || !cJSON_IsString(command_json) || !command_json->valuestring) {
        LOGE("device control failed: invalid command");
        return device_control_result_text(name, "command 参数缺失或格式错误。", true);
    }

    if (strcmp(command_json->valuestring, "reboot") != 0) {
        LOGE("device control failed: unsupported command %s", command_json->valuestring);
        return device_control_result_text(name, "仅支持 reboot 指令。", true);
    }

    if (!delay_json || !cJSON_IsNumber(delay_json) || delay_json->valuedouble < 0 ||
        delay_json->valuedouble > (double)UINT32_MAX ||
        delay_json->valuedouble != (double)(uint32_t)delay_json->valuedouble) {
        LOGE("device control failed: invalid delay_ms");
        return device_control_result_text(name, "delay_ms 参数缺失或格式错误。", true);
    }

    delay_ms = (uint32_t)delay_json->valuedouble;
    /* 新 boot 下可靠软重启，不用区分供电；老 boot 纯电池下自动重启会
     * 掉电变成静悄悄关机，还是提示用户手动重启。*/
    bool auto_reboot = uboot_features_has(UBOOT_FEATURE_POWER_GUARD) || power_is_usb_plugged();
    reboot_msg.auto_reboot = auto_reboot ? 1 : 0;
    reboot_msg.delay_ms = delay_ms;

    LOGI("device control reboot requested, auto_reboot=%u, delay_ms=%u",
         reboot_msg.auto_reboot, reboot_msg.delay_ms);

    if (voice_msg_pub(VOICE_MSG_CLOUD_RESOURCE_UPDATE_REBOOT, &reboot_msg, sizeof(reboot_msg)) != 0) {
        LOGE("device control failed: publish reboot msg error");
        return device_control_result_text(name, "发送重启请求失败。", true);
    }

    if (!auto_reboot) {
        snprintf(text, sizeof(text), "请手动重启设备完成更新");
    } else if (delay_ms > 0) {
        snprintf(text, sizeof(text), "设备将在%u毫秒后重启", delay_ms);
    } else {
        snprintf(text, sizeof(text), "设备即将重启");
    }

    return device_control_result_text(name, text, false);
}

MCP_TOOL_DEFINE(ls.built_in.device_control, device_control_list, device_control_call);

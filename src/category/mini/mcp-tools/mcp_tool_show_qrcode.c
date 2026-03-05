#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "lisa_log.h"
#include "voice_msg.h"
#include "sysheap.h"

#include "mcp.h"
#include "cJSON.h"

#define TAG "mcp_show_qrcode"

static cJSON *show_qrcode_list(const char *name)
{
    cJSON *tool = cJSON_CreateObject();
    if (!tool) {
        LOGE("Failed to create tool JSON object");
        return NULL;
    }

    cJSON_AddStringToObject(tool, "name", name);
    cJSON_AddStringToObject(tool, "description", "Display QR code");

    cJSON *inputSchema = cJSON_CreateObject();
    cJSON_AddStringToObject(inputSchema, "type", "object");

    cJSON *properties = cJSON_CreateObject();

    cJSON *url_prop = cJSON_CreateObject();
    cJSON_AddStringToObject(url_prop, "type", "string");
    cJSON_AddStringToObject(url_prop, "description", "QR code URL");
    cJSON_AddItemToObject(properties, "url", url_prop);

    cJSON *message_prop = cJSON_CreateObject();
    cJSON_AddStringToObject(message_prop, "type", "string");
    cJSON_AddStringToObject(message_prop, "description", "Text message");
    cJSON_AddItemToObject(properties, "message", message_prop);

    cJSON *err_code_prop = cJSON_CreateObject();
    cJSON_AddStringToObject(err_code_prop, "type", "string");
    cJSON_AddStringToObject(err_code_prop, "description", "Error code");
    cJSON_AddItemToObject(properties, "err_code", err_code_prop);

    cJSON_AddItemToObject(inputSchema, "properties", properties);

    cJSON *required = cJSON_CreateArray();
    cJSON_AddItemToArray(required, cJSON_CreateString("url"));
    cJSON_AddItemToObject(inputSchema, "required", required);

    cJSON_AddItemToObject(tool, "inputSchema", inputSchema);

    return tool;
}

static cJSON *show_qrcode_call(const char *id, const char *name, cJSON *args)
{
    const cJSON *url_json = mcp_tool_call_args_get(args, "url");
    const cJSON *message_json = mcp_tool_call_args_get(args, "message");
    const cJSON *err_code_json = mcp_tool_call_args_get(args, "err_code");

    if (!url_json || !url_json->valuestring) {
        LOGE("MCP tool call args get url failed");
        return NULL;
    }

    const char *url = url_json->valuestring;
    const char *message = (message_json && message_json->valuestring) ? message_json->valuestring : "";
    const char *err_code = (err_code_json && err_code_json->valuestring) ? err_code_json->valuestring : "";

    LOGI("MCP show_qrcode received - url: %s, message: %s, err_code: %s", url, message, err_code);


    cJSON *payload = cJSON_CreateObject();
    if (!payload) {
        LOGE("Failed to create payload JSON");
        return NULL;
    }

    cJSON_AddStringToObject(payload, "url", url);
    cJSON_AddStringToObject(payload, "message", message);
    cJSON_AddStringToObject(payload, "err_code", err_code);

    char *payload_str = cJSON_PrintUnformatted(payload);
    if (payload_str) {
        voice_msg_pub(VOICE_MSG_CLOUD_SHOW_QRCODE, payload_str, strlen(payload_str) + 1);
        cJSON_free(payload_str);
    }
    cJSON_Delete(payload);

    cJSON *result = mcp_tool_call_result_create(name);
    if (!result) {
        return NULL;
    }

    cJSON *content_array = cJSON_CreateArray();
    cJSON *content_item = cJSON_CreateObject();
    cJSON_AddStringToObject(content_item, "type", "text");
    cJSON_AddStringToObject(content_item, "text", "Operation completed");
    cJSON_AddItemToArray(content_array, content_item);
    cJSON_AddItemToObject(result, "content", content_array);
    cJSON_AddBoolToObject(result, "isError", false);

    return result;
}

MCP_TOOL_DEFINE(ls.built_in.show_qrcode, show_qrcode_list, show_qrcode_call);

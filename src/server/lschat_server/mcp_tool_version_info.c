#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "lisa_log.h"
#include "mcp.h"
#include "cJSON.h"

#include "project_version.h"
#include "sdk_version.h"

#define TAG "mcp_version_info"

static cJSON *version_info_list(const char *name)
{
    cJSON *tool = cJSON_CreateObject();
    if (!tool) {
        LOGE("Failed to create tool JSON object");
        return NULL;
    }

    cJSON_AddStringToObject(tool, "name", name);
    cJSON_AddStringToObject(tool, "description", "获取设备固件版本信息，包括版本号和提交信息。可以通过类似'设备版本是多少'、'固件版本'、'查看版本信息'等方式触发。");

    cJSON *inputSchema = cJSON_CreateObject();
    if (!inputSchema) {
        cJSON_Delete(tool);
        return NULL;
    }
    cJSON_AddStringToObject(inputSchema, "type", "object");

    cJSON *properties = cJSON_CreateObject();
    if (!properties) {
        cJSON_Delete(inputSchema);
        cJSON_Delete(tool);
        return NULL;
    }

    cJSON_AddItemToObject(inputSchema, "properties", properties);

    cJSON *required = cJSON_CreateArray();
    if (!required) {
        cJSON_Delete(tool);
        return NULL;
    }
    cJSON_AddItemToObject(inputSchema, "required", required);

    cJSON_AddItemToObject(tool, "inputSchema", inputSchema);

    return tool;
}

static cJSON *version_info_call(const char *id, const char *name, cJSON *args)
{
    (void)id;
    (void)args;

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
        cJSON_Delete(result);
        return NULL;
    }

    cJSON_AddStringToObject(content_item, "type", "text");
    
    char version_text[256];
    snprintf(version_text, sizeof(version_text), "当前固件版本号为%s, commit=%s", PROJECT_VERSION_STR, PROJECT_VERSION_COMMIT);
    cJSON_AddStringToObject(content_item, "text", version_text);
    cJSON_AddItemToArray(content_array, content_item);
    cJSON_AddItemToObject(result, "content", content_array);
    cJSON_AddBoolToObject(result, "isError", false);

    LOGI("Version info requested: %s, commit=%s", PROJECT_VERSION_STR, BUILD_VERSION);

    return result;
}

MCP_TOOL_DEFINE(ls.device_version_info, version_info_list, version_info_call);

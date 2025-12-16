#include "version_info.h"
#include "aiui_mcp.h"
#include "lisa_log.h"
#include "cJSON.h"
#include "project_version.h"
#include <string.h>
#include <stdio.h>

#define TAG "version_info"

const char* get_device_firmware_version(void)
{
    return PROJECT_VERSION_STR;
}

const char* get_device_firmware_commit(void)
{
    return PROJECT_VERSION_COMMIT;
}

static mcp_result_t version_info_handler(const mcp_context_t *ctx, mcp_response_t *response)
{
    LISA_LOGI(TAG, "%s---", __func__);

    if (!ctx || !response) {
        return MCP_RESULT_INVALID_PARAM;
    }

    // 获取版本信息
    const char *firmware_version = get_device_firmware_version();
    const char *firmware_commit = get_device_firmware_commit();

    // 构建响应文本
    char result_msg[512];
    snprintf(result_msg, sizeof(result_msg),
            "当前固件版本号为: %s (Commit: %s)", firmware_version, firmware_commit);

    // 创建content数组
    cJSON *content_array = cJSON_CreateArray();
    if (!content_array) {
        response->result = MCP_RESULT_ERROR;
        return MCP_RESULT_ERROR;
    }

    // 创建text item
    cJSON *text_item = cJSON_CreateObject();
    if (!text_item) {
        cJSON_Delete(content_array);
        response->result = MCP_RESULT_ERROR;
        return MCP_RESULT_ERROR;
    }

    cJSON_AddStringToObject(text_item, "type", "text");
    cJSON_AddStringToObject(text_item, "text", result_msg);
    cJSON_AddItemToArray(content_array, text_item);

    response->content = content_array;
    response->result = MCP_RESULT_SUCCESS;

    LISA_LOGI(TAG, "Device version info retrieved - Version: %s, Commit: %s",
              firmware_version, firmware_commit);

    return MCP_RESULT_SUCCESS;
}

/**
 * @brief 生成版本信息工具的参数 Schema
 */
cJSON* generate_version_info_schema(void)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        LISA_LOGE(TAG, "Failed to create root object for version_info schema");
        return NULL;
    }

    if (!cJSON_AddStringToObject(root, "type", "object")) {
        LISA_LOGE(TAG, "Failed to add type to version_info schema");
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *properties = cJSON_CreateObject();
    if (!properties) {
        LISA_LOGE(TAG, "Failed to create properties object");
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToObject(root, "properties", properties);

    // 无 required 参数
    cJSON *required = cJSON_CreateArray();
    if (!required) {
        LISA_LOGE(TAG, "Failed to create required array");
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToObject(root, "required", required);

    return root;
}

// 注册版本信息查询工具
MCP_REGISTER_TOOL_STATIC(device_version_info,
                          "ls.device_version_info",
                          "获取设备固件版本信息，包括版本号和提交信息。可以通过类似'设备版本是多少'、'固件版本'、'查看版本信息'等方式触发。",
                          "1.0",
                          generate_version_info_schema,
                          0,  // 无参数
                          version_info_handler,
                          false,
                          NULL);

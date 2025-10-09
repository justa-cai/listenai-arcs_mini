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
    if (!ctx || !response) {
        return MCP_RESULT_INVALID_PARAM;
    }

    // 创建版本信息JSON对象
    cJSON *version_info = cJSON_CreateObject();
    if (!version_info) {
        response->content = cJSON_CreateString("创建版本信息失败");
        response->result = MCP_RESULT_ERROR;
        return MCP_RESULT_ERROR;
    }

    // 添加版本信息
    cJSON_AddStringToObject(version_info, "firmware_version", get_device_firmware_version());
    cJSON_AddStringToObject(version_info, "firmware_commit", get_device_firmware_commit());

    // 创建响应消息
    cJSON *message = cJSON_CreateObject();
    if (!message) {
        cJSON_Delete(version_info);
        response->content = cJSON_CreateString("创建响应消息失败");
        response->result = MCP_RESULT_ERROR;
        return MCP_RESULT_ERROR;
    }

    cJSON_AddStringToObject(message, "message", "设备版本信息获取成功");
    cJSON_AddItemToObject(message, "version_info", version_info);

    response->content = message;
    response->result = MCP_RESULT_SUCCESS;

    LISA_LOGI(TAG, "Device version info retrieved - Version: %s, Commit: %s", 
              get_device_firmware_version(), get_device_firmware_commit());

    return MCP_RESULT_SUCCESS;
}

// 定义工具参数（此工具无需参数）
static mcp_param_def_t version_info_params[] = {
    MCP_PARAM_DEF_END
};

// 注册版本信息查询工具
MCP_REGISTER_TOOL_STATIC(device_version_info,
                         "获取设备固件版本信息，包括版本号和提交信息。可以通过类似'设备版本是多少'、'固件版本'、'查看版本信息'等方式触发。",
                         "1.0",
                         version_info_params,
                         0,  // 无参数
                         version_info_handler,
                         false,
                         NULL);

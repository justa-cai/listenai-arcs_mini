#include "volume_control.h"
#include "aiui_mcp.h"
#include "lisa_log.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include "player/listen_volume.h"

#define TAG "volume_control"

// 当前音量状态 (0% - 100%)
static int current_volume = 70;

static mcp_result_t volume_control_handler(const mcp_context_t *ctx, mcp_response_t *response)
{
    LISA_LOGI(TAG, "%s---", __func__);

    if (!ctx || !response) {
        return MCP_RESULT_INVALID_PARAM;
    }

    int volume = -1;
    
    // 解析参数
    for (uint32_t i = 0; i < ctx->param_count; i++) {
        if (strcmp(ctx->params[i].name, "volume") == 0) {
            if (cJSON_IsNumber(ctx->params[i].value)) {
                volume = ctx->params[i].value->valueint;
            }
        }
    }

    if (volume < 0 || volume > 100) {
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
        cJSON_AddStringToObject(text_item, "text", "错误：音量值必须在0-100之间");
        cJSON_AddItemToArray(content_array, text_item);

        response->content = content_array;
        response->result = MCP_RESULT_INVALID_PARAM;
        return MCP_RESULT_INVALID_PARAM;
    }

    int old_volume = current_volume;
    current_volume = volume;

    // 调用实际的音量控制函数
    listen_set_volume(current_volume);
    
    LISA_LOGI(TAG, "Volume changed from %d%% to %d%%", old_volume, current_volume);

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
    cJSON_AddStringToObject(text_item, "text", "已完成操作");
    cJSON_AddItemToArray(content_array, text_item);

    response->content = content_array;
    response->result = MCP_RESULT_SUCCESS;

    return MCP_RESULT_SUCCESS;
}

// 获取当前音量的处理函数
static mcp_result_t volume_get_handler(const mcp_context_t *ctx, mcp_response_t *response)
{
    LISA_LOGI(TAG, "%s---", __func__);

    if (!ctx || !response) {
        return MCP_RESULT_INVALID_PARAM;
    }

    // 获取当前音量
    current_volume = listen_get_volume();

    LISA_LOGI(TAG, "Current volume requested: %d%%", current_volume);

    // 构建响应内容
    char result_msg[256];
    snprintf(result_msg, sizeof(result_msg),
            "当前音量为: %d%%", current_volume);

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

    return MCP_RESULT_SUCCESS;
}

/**
 * @brief 生成设置音量工具的参数 Schema
 *
 * @return cJSON对象指针，失败返回NULL
 */
cJSON* generate_volume_control_schema(void)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        LISA_LOGE(TAG, "Failed to create root object for volume_control schema");
        return NULL;
    }

    if (!cJSON_AddStringToObject(root, "type", "object")) {
        LISA_LOGE(TAG, "Failed to add type to volume_control schema");
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *properties = cJSON_CreateObject();
    if (!properties) {
        LISA_LOGE(TAG, "Failed to create properties object");
        cJSON_Delete(root);
        return NULL;
    }

    // volume 参数
    cJSON *volume_prop = cJSON_CreateObject();
    if (!volume_prop) {
        LISA_LOGE(TAG, "Failed to create volume property");
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    if (!cJSON_AddStringToObject(volume_prop, "type", "integer") ||
        !cJSON_AddStringToObject(volume_prop, "description", "音量值，范围0-100") ||
        !cJSON_AddNumberToObject(volume_prop, "minimum", 0) ||
        !cJSON_AddNumberToObject(volume_prop, "maximum", 100)) {
        LISA_LOGE(TAG, "Failed to add volume property fields");
        cJSON_Delete(volume_prop);
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToObject(properties, "volume", volume_prop);

    cJSON_AddItemToObject(root, "properties", properties);

    // required 数组
    cJSON *required = cJSON_CreateArray();
    if (!required) {
        LISA_LOGE(TAG, "Failed to create required array");
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *volume_str = cJSON_CreateString("volume");
    if (!volume_str) {
        LISA_LOGE(TAG, "Failed to create volume string for required array");
        cJSON_Delete(required);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToArray(required, volume_str);
    cJSON_AddItemToObject(root, "required", required);

    return root;
}

/**
 * @brief 生成获取音量工具的参数 Schema
 *
 * @return cJSON对象指针，失败返回NULL
 */
cJSON* generate_volume_get_schema(void)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        LISA_LOGE(TAG, "Failed to create root object for volume_get schema");
        return NULL;
    }

    if (!cJSON_AddStringToObject(root, "type", "object")) {
        LISA_LOGE(TAG, "Failed to add type to volume_get schema");
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

// 使用静态段注册宏注册设置音量工具
MCP_REGISTER_TOOL_STATIC(set_volume,
                          "ls.set_volume",
                          "设置当前音量。音量范围为0-100。当用户无具体设置数值时,得先调用get_volume工具获取当前音量，再做调整。",
                          "1.0",
                          generate_volume_control_schema,
                          1,
                          volume_control_handler,
                          false,
                          NULL);

// 使用静态段注册宏注册获取音量工具
MCP_REGISTER_TOOL_STATIC(get_volume,
                          "ls.get_volume",
                          "获取当前音量.",
                          "1.0",
                          generate_volume_get_schema,
                          0,
                          volume_get_handler,
                          false,
                          NULL);

int get_current_volume(void)
{
    // 返回当前音量
    return listen_get_volume();
}

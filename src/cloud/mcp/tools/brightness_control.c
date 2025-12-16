#include "brightness_control.h"
#include "aiui_mcp.h"
#include "lisa_log.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include "lisa_display.h"

#define TAG "brightness_control"

static mcp_result_t brightness_control_handler(const mcp_context_t *ctx, mcp_response_t *response)
{
    LISA_LOGI(TAG, "%s---", __func__);

    if (!ctx || !response) {
        return MCP_RESULT_INVALID_PARAM;
    }

    int brightness = -1;
    
    // 解析参数
    for (uint32_t i = 0; i < ctx->param_count; i++) {
        if (strcmp(ctx->params[i].name, "brightness") == 0) {
            if (cJSON_IsNumber(ctx->params[i].value)) {
                brightness = ctx->params[i].value->valueint;
            }
        }
    }

    if (brightness < 0 || brightness > 100) {
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
        cJSON_AddStringToObject(text_item, "text", "错误：亮度必须在0-100之间");
        cJSON_AddItemToArray(content_array, text_item);

        response->content = content_array;
        response->result = MCP_RESULT_INVALID_PARAM;
        return MCP_RESULT_INVALID_PARAM;
    }
    
    // 确保亮度是10的倍数
    int rounded_brightness = (brightness + 5) / 10 * 10;
    if (rounded_brightness > 100) {
        rounded_brightness = 100;
    }

    // 调用实际的亮度控制 API
    int ret = lisa_display_set_brightness(lisa_display_get(), rounded_brightness);
    if (ret != 0) {
        LISA_LOGE(TAG, "Failed to set display brightness: %d", ret);
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
        cJSON_AddStringToObject(text_item, "text", "设置显示器亮度失败");
        cJSON_AddItemToArray(content_array, text_item);

        response->content = content_array;
        response->result = MCP_RESULT_ERROR;
        return MCP_RESULT_ERROR;
    }
    
    LISA_LOGI(TAG, "Brightness changed from %d%% to %d%%", get_display_brightness());
    
    // 创建content数组
    cJSON *content_array = cJSON_CreateArray();
    if (!content_array) {
        response->result = MCP_RESULT_SUCCESS;
        return MCP_RESULT_SUCCESS;
    }

    // 创建text item
    cJSON *text_item = cJSON_CreateObject();
    if (!text_item) {
        cJSON_Delete(content_array);
        response->result = MCP_RESULT_SUCCESS;
        return MCP_RESULT_SUCCESS;
    }

    cJSON_AddStringToObject(text_item, "type", "text");
    cJSON_AddStringToObject(text_item, "text", "已完成操作");
    cJSON_AddItemToArray(content_array, text_item);

    response->content = content_array;
    response->result = MCP_RESULT_SUCCESS;
    
    return MCP_RESULT_SUCCESS;
}

// 获取当前亮度的处理函数
static mcp_result_t brightness_get_handler(const mcp_context_t *ctx, mcp_response_t *response)
{
    LISA_LOGI(TAG, "%s---", __func__);

    if (!ctx || !response) {
        return MCP_RESULT_INVALID_PARAM;
    }

    // 获取当前亮度
    // 注意：由于硬件可能不支持直接读取当前亮度，我们直接返回缓存的当前亮度值
    // 这样可以保持与设置的值一致

    LISA_LOGI(TAG, "Current brightness requested: %d%%", get_display_brightness());

    // 构建响应内容
    char result_msg[256];
    snprintf(result_msg, sizeof(result_msg),
            "当前显示器亮度为: %d%%", get_display_brightness());

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
 * @brief 生成设置亮度工具的参数 Schema
 *
 * @return cJSON对象指针，失败返回NULL
 */
cJSON* generate_brightness_control_schema(void)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        LISA_LOGE(TAG, "Failed to create root object for brightness_control schema");
        return NULL;
    }

    if (!cJSON_AddStringToObject(root, "type", "object")) {
        LISA_LOGE(TAG, "Failed to add type to brightness_control schema");
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *properties = cJSON_CreateObject();
    if (!properties) {
        LISA_LOGE(TAG, "Failed to create properties object");
        cJSON_Delete(root);
        return NULL;
    }

    // brightness 参数
    cJSON *brightness_prop = cJSON_CreateObject();
    if (!brightness_prop) {
        LISA_LOGE(TAG, "Failed to create brightness property");
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    if (!cJSON_AddStringToObject(brightness_prop, "type", "integer") ||
        !cJSON_AddStringToObject(brightness_prop, "description", "亮度值，以10%为步长 (0-100)") ||
        !cJSON_AddNumberToObject(brightness_prop, "minimum", 0) ||
        !cJSON_AddNumberToObject(brightness_prop, "maximum", 100)) {
        LISA_LOGE(TAG, "Failed to add brightness property fields");
        cJSON_Delete(brightness_prop);
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToObject(properties, "brightness", brightness_prop);

    cJSON_AddItemToObject(root, "properties", properties);

    // required 数组
    cJSON *required = cJSON_CreateArray();
    if (!required) {
        LISA_LOGE(TAG, "Failed to create required array");
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *brightness_str = cJSON_CreateString("brightness");
    if (!brightness_str) {
        LISA_LOGE(TAG, "Failed to create brightness string for required array");
        cJSON_Delete(required);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToArray(required, brightness_str);
    cJSON_AddItemToObject(root, "required", required);

    return root;
}

/**
 * @brief 生成获取亮度工具的参数 Schema
 *
 * @return cJSON对象指针，失败返回NULL
 */
cJSON* generate_brightness_get_schema(void)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        LISA_LOGE(TAG, "Failed to create root object for brightness_get schema");
        return NULL;
    }

    if (!cJSON_AddStringToObject(root, "type", "object")) {
        LISA_LOGE(TAG, "Failed to add type to brightness_get schema");
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

// 使用静态段注册宏注册设置亮度工具
MCP_REGISTER_TOOL_STATIC(display_set_brightness,
                          "ls.display_set_brightness",
                          "设置显示器的亮度。亮度可以10%为步长进行设置 (0, 10, 20, ..., 100)。当用户无具体设置数值时,得先调用display_get_brightness工具获取当前音量，再做调整。",
                          "1.0",
                          generate_brightness_control_schema,
                          1,
                          brightness_control_handler,
                          false,
                          NULL);

// 使用静态段注册宏注册获取亮度工具
MCP_REGISTER_TOOL_STATIC(display_get_brightness,
                          "ls.display_get_brightness",
                          "获取显示器的当前亮度。",
                          "1.0",
                          generate_brightness_get_schema,
                          0,
                          brightness_get_handler,
                          false,
                          NULL);

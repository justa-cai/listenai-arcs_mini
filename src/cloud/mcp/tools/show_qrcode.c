#include "show_qrcode.h"
#include "aiui_mcp.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include "assistant_controller.h"

#define TAG "show_qrcode"

// 二维码URL最大长度
#define QRCODE_URL_MAX_LEN 512
// 消息文本最大长度
#define QRCODE_MESSAGE_MAX_LEN 256

// 外部函数声明：更新二维码显示
extern int assistant_view_update_qrcode(const char *url, const char *message, const char *err_code);

/**
 * @brief 显示二维码处理函数
 *
 * 该工具用于在UI上显示二维码。
 * 支持显示二维码URL、提示文本和错误码。
 */
static mcp_result_t show_qrcode_handler(const mcp_context_t *ctx, mcp_response_t *response)
{
    LISA_LOGI(TAG, "%s---", __func__);

    if (!ctx || !response) {
        return MCP_RESULT_INVALID_PARAM;
    }

    const char *url = NULL;
    const char *message = NULL;
    const char *err_code = NULL;

    // 解析参数
    for (uint32_t i = 0; i < ctx->param_count; i++) {
        if (strcmp(ctx->params[i].name, "url") == 0) {
            if (cJSON_IsString(ctx->params[i].value)) {
                url = cJSON_GetStringValue(ctx->params[i].value);
            }
        } else if (strcmp(ctx->params[i].name, "message") == 0) {
            if (cJSON_IsString(ctx->params[i].value)) {
                message = cJSON_GetStringValue(ctx->params[i].value);
            }
        } else if (strcmp(ctx->params[i].name, "err_code") == 0) {
            if (cJSON_IsString(ctx->params[i].value)) {
                err_code = cJSON_GetStringValue(ctx->params[i].value);
            }
        }
    }

    // 参数校验：url是必需的
    if (!url || strlen(url) == 0) {
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
        cJSON_AddStringToObject(text_item, "text", "错误：缺少必需参数 url");
        cJSON_AddItemToArray(content_array, text_item);

        response->content = content_array;
        response->result = MCP_RESULT_INVALID_PARAM;
        return MCP_RESULT_INVALID_PARAM;
    }

    // 验证URL长度
    size_t url_len = strlen(url);
    if (url_len >= QRCODE_URL_MAX_LEN) {
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg),
                "错误：URL长度超过限制 (%zu >= %d)", url_len, QRCODE_URL_MAX_LEN);
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
        response->result = MCP_RESULT_INVALID_PARAM;
        return MCP_RESULT_INVALID_PARAM;
    }

    // 验证message长度（如果提供）
    if (message && strlen(message) >= QRCODE_MESSAGE_MAX_LEN) {
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg),
                "错误：消息文本长度超过限制 (%zu >= %d)", 
                strlen(message), QRCODE_MESSAGE_MAX_LEN);
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
        response->result = MCP_RESULT_INVALID_PARAM;
        return MCP_RESULT_INVALID_PARAM;
    }

    LISA_LOGI(TAG, "Show QR code: url=%s, message=%s, err_code=%s", 
              url, message ? message : "(none)", err_code ? err_code : "(none)");

    // 调用assistant_view函数更新二维码显示
    int ret = assistant_view_update_qrcode(url, message, err_code);
    if (ret != 0) {
        LISA_LOGE(TAG, "Failed to update QR code, error: %d", ret);
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
        cJSON_AddStringToObject(text_item, "text", "错误：更新二维码显示失败");
        cJSON_AddItemToArray(content_array, text_item);

        response->content = content_array;
        response->result = MCP_RESULT_ERROR;
        return MCP_RESULT_ERROR;
    }

    LISA_LOGI(TAG, "QR code updated successfully");

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

/**
 * @brief 生成显示二维码工具的参数 Schema
 */
cJSON* generate_show_qrcode_schema(void)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        LISA_LOGE(TAG, "Failed to create root object for show_qrcode schema");
        return NULL;
    }

    if (!cJSON_AddStringToObject(root, "type", "object")) {
        LISA_LOGE(TAG, "Failed to add type to show_qrcode schema");
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *properties = cJSON_CreateObject();
    if (!properties) {
        LISA_LOGE(TAG, "Failed to create properties object");
        cJSON_Delete(root);
        return NULL;
    }

    // url 参数
    cJSON *url_prop = cJSON_CreateObject();
    if (!url_prop) {
        LISA_LOGE(TAG, "Failed to create url property");
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    if (!cJSON_AddStringToObject(url_prop, "type", "string") ||
        !cJSON_AddStringToObject(url_prop, "description", "二维码地址")) {
        LISA_LOGE(TAG, "Failed to add url property fields");
        cJSON_Delete(url_prop);
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToObject(properties, "url", url_prop);

    // message 参数
    cJSON *message_prop = cJSON_CreateObject();
    if (!message_prop) {
        LISA_LOGE(TAG, "Failed to create message property");
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    if (!cJSON_AddStringToObject(message_prop, "type", "string") ||
        !cJSON_AddStringToObject(message_prop, "description", "文本提示")) {
        LISA_LOGE(TAG, "Failed to add message property fields");
        cJSON_Delete(message_prop);
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToObject(properties, "message", message_prop);

    // err_code 参数
    cJSON *err_code_prop = cJSON_CreateObject();
    if (!err_code_prop) {
        LISA_LOGE(TAG, "Failed to create err_code property");
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    if (!cJSON_AddStringToObject(err_code_prop, "type", "string") ||
        !cJSON_AddStringToObject(err_code_prop, "description", "错误码")) {
        LISA_LOGE(TAG, "Failed to add err_code property fields");
        cJSON_Delete(err_code_prop);
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToObject(properties, "err_code", err_code_prop);

    cJSON_AddItemToObject(root, "properties", properties);

    // required 数组
    cJSON *required = cJSON_CreateArray();
    if (!required) {
        LISA_LOGE(TAG, "Failed to create required array");
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *url_str = cJSON_CreateString("url");
    if (!url_str) {
        LISA_LOGE(TAG, "Failed to create url string for required array");
        cJSON_Delete(required);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToArray(required, url_str);
    cJSON_AddItemToObject(root, "required", required);

    return root;
}

// 使用静态段注册宏注册显示二维码工具
MCP_REGISTER_TOOL_STATIC(show_qrcode,
                          "ls.built_in.show_qrcode",
                          "显示二维码",
                          "1.0",
                          generate_show_qrcode_schema,
                          3,
                          show_qrcode_handler,
                          false,
                          NULL);

#include "show_image.h"
#include "aiui_mcp.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include "display/lv_img_net_loader.h"
#include "assistant_controller.h"

#define TAG "show_image"

// 图片URL最大长度
#define IMAGE_URL_MAX_LEN 512

// 存储当前显示的图片URL（用于管理图片生命周期）
static char *current_image_url = NULL;

/**
 * @brief 显示图片处理函数
 *
 * 该工具用于在UI上显示来自网络URL的图片。
 * 支持HTTP/HTTPS协议的图片资源。
 */
static mcp_result_t show_image_handler(const mcp_context_t *ctx, mcp_response_t *response)
{
    LISA_LOGI(TAG, "%s---", __func__);

    if (!ctx || !response) {
        return MCP_RESULT_INVALID_PARAM;
    }

    const char *url = NULL;

    // 解析参数: url (图片资源地址)
    for (uint32_t i = 0; i < ctx->param_count; i++) {
        if (strcmp(ctx->params[i].name, "url") == 0) {
            if (cJSON_IsString(ctx->params[i].value)) {
                url = cJSON_GetStringValue(ctx->params[i].value);
            }
        }
    }

    // 参数校验
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
    if (url_len >= IMAGE_URL_MAX_LEN) {
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg),
                "错误：URL长度超过限制 (%zu >= %d)", url_len, IMAGE_URL_MAX_LEN);
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

    // 验证URL格式（必须是http://或https://）
    if (strncmp(url, "http://", 7) != 0 &&
        strncmp(url, "https://", 8) != 0 &&
        strncmp(url, "N:", 2) != 0) {
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
        cJSON_AddStringToObject(text_item, "text", "错误：URL必须以 http://, https:// 或 N: 开头");
        cJSON_AddItemToArray(content_array, text_item);

        response->content = content_array;
        response->result = MCP_RESULT_INVALID_PARAM;
        return MCP_RESULT_INVALID_PARAM;
    }

    LISA_LOGI(TAG, "Show image: Loading image from URL: %s", url);

    // 预加载图片以验证URL是否有效
    lv_img_dsc_t *img_dsc = lv_img_net_load(url);
    if (!img_dsc) {
        LISA_LOGE(TAG, "Failed to load image from URL: %s", url);
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
        cJSON_AddStringToObject(text_item, "text", "错误：无法加载图片，请检查URL是否有效");
        cJSON_AddItemToArray(content_array, text_item);

        response->content = content_array;
        response->result = MCP_RESULT_ERROR;
        return MCP_RESULT_ERROR;
    }

    LISA_LOGI(TAG, "Image loaded successfully: %u bytes", img_dsc->data_size);

    // 保存当前图片URL（用于后续清理）
    if (current_image_url) {
        lisa_mem_free(current_image_url);
    }
    current_image_url = (char *)lisa_mem_alloc(url_len + 1);
    if (current_image_url) {
        strncpy(current_image_url, url, url_len);
        current_image_url[url_len] = '\0';
    }

    // TODO: 这里需要与UI层集成，实际显示图片到界面
    // 目前只是预加载并缓存图片，实际显示需要UI层配合
    // 可以通过assistant_view或ebus事件系统来触发UI更新

    LISA_LOGI(TAG, "Image cached and ready for display");

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
 * @brief 生成显示图片工具的参数 Schema
 */
cJSON* generate_show_image_schema(void)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        LISA_LOGE(TAG, "Failed to create root object for show_image schema");
        return NULL;
    }

    if (!cJSON_AddStringToObject(root, "type", "object")) {
        LISA_LOGE(TAG, "Failed to add type to show_image schema");
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
        !cJSON_AddStringToObject(url_prop, "description", "图片资源地址")) {
        LISA_LOGE(TAG, "Failed to add url property fields");
        cJSON_Delete(url_prop);
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToObject(properties, "url", url_prop);

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

// 使用静态段注册宏注册显示图片工具
MCP_REGISTER_TOOL_STATIC(show_image,
                          "ls.built_in.show_image",
                          "显示图片",
                          "1.0",
                          generate_show_image_schema,
                          1,
                          show_image_handler,
                          false,
                          NULL);

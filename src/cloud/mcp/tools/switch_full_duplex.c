#include "switch_full_duplex.h"
#include "aiui_mcp.h"
#include "lisa_log.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include "lisa_aiui.h"

#define TAG "switch_full_duplex"

/**
 * @brief 全双工开关处理函数
 *
 * 该工具用于切换全双工模式。
 * true: 进入全双工模式（INTER_CONTINUE）
 * false: 退出全双工模式，返回半双工模式（INTER_ONESHOT）
 */
static mcp_result_t switch_full_duplex_handler(const mcp_context_t *ctx, mcp_response_t *response)
{
    LISA_LOGI(TAG, "%s---", __func__);

    if (!ctx || !response) {
        return MCP_RESULT_INVALID_PARAM;
    }

    bool value = false;
    bool value_found = false;

    // 解析参数: value (布尔值)
    for (uint32_t i = 0; i < ctx->param_count; i++) {
        if (strcmp(ctx->params[i].name, "value") == 0) {
            if (cJSON_IsBool(ctx->params[i].value)) {
                value = cJSON_IsTrue(ctx->params[i].value);
                value_found = true;
            }
        }
    }

    // 参数校验
    if (!value_found) {
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
        cJSON_AddStringToObject(text_item, "text", "错误：缺少必需参数 value");
        cJSON_AddItemToArray(content_array, text_item);

        response->content = content_array;
        response->result = MCP_RESULT_INVALID_PARAM;
        return MCP_RESULT_INVALID_PARAM;
    }

    LISA_LOGI(TAG, "Switch full duplex mode: %s", value ? "true (enable)" : "false (disable)");

    // 根据value设置交互模式
    lisa_aiui_interactive_mode_e mode;
    const char *mode_desc;

    if (value) {
        // true: 进入全双工模式
        mode = INTER_CONTINUE;
        mode_desc = "全双工模式";
    } else {
        // false: 退出全双工模式，返回半双工模式
        mode = INTER_ONESHOT;
        mode_desc = "半双工模式";
    }

    // 设置交互模式
    int ret = lisa_aiui_set_interactive_mode(mode);
    if (ret != 0) {
        LISA_LOGE(TAG, "Failed to set interactive mode, error: %d", ret);
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
        cJSON_AddStringToObject(text_item, "text", "错误：设置交互模式失败");
        cJSON_AddItemToArray(content_array, text_item);

        response->content = content_array;
        response->result = MCP_RESULT_ERROR;
        return MCP_RESULT_ERROR;
    }

    LISA_LOGI(TAG, "Successfully set interactive mode to: %s", mode_desc);

    char result_msg[128];
    snprintf(result_msg, sizeof(result_msg), "已切换到%s", mode_desc);

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
 * @brief 生成全双工开关工具的参数 Schema
 */
cJSON* generate_switch_full_duplex_schema(void)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        LISA_LOGE(TAG, "Failed to create root object for switch_full_duplex schema");
        return NULL;
    }

    if (!cJSON_AddStringToObject(root, "type", "object")) {
        LISA_LOGE(TAG, "Failed to add type to switch_full_duplex schema");
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *properties = cJSON_CreateObject();
    if (!properties) {
        LISA_LOGE(TAG, "Failed to create properties object");
        cJSON_Delete(root);
        return NULL;
    }

    // value 参数
    cJSON *value_prop = cJSON_CreateObject();
    if (!value_prop) {
        LISA_LOGE(TAG, "Failed to create value property");
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    if (!cJSON_AddStringToObject(value_prop, "type", "boolean") ||
        !cJSON_AddStringToObject(value_prop, "description", "开关，true为进入全双工模式，false为退出全双工模式")) {
        LISA_LOGE(TAG, "Failed to add value property fields");
        cJSON_Delete(value_prop);
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToObject(properties, "value", value_prop);

    cJSON_AddItemToObject(root, "properties", properties);

    // required 数组
    cJSON *required = cJSON_CreateArray();
    if (!required) {
        LISA_LOGE(TAG, "Failed to create required array");
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *value_str = cJSON_CreateString("value");
    if (!value_str) {
        LISA_LOGE(TAG, "Failed to create value string for required array");
        cJSON_Delete(required);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToArray(required, value_str);
    cJSON_AddItemToObject(root, "required", required);

    return root;
}

// 使用静态段注册宏注册全双工开关工具
MCP_REGISTER_TOOL_STATIC(switch_full_duplex,
                          "ls.built_in.switch_full_duplex",
                          "全双工开关",
                          "1.0",
                          generate_switch_full_duplex_schema,
                          1,
                          switch_full_duplex_handler,
                          false,
                          NULL);

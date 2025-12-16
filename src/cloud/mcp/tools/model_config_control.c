#include "aiui_mcp.h"
#include "lisa_log.h"
#include "cJSON.h"
#include <string.h>
#include "lisaui_user_data.h"

#define TAG "model_config"

/**
 * @brief 处理大模型配置修改请求
 */
static mcp_result_t model_config_handler(const mcp_context_t *ctx, mcp_response_t *response)
{
    LISA_LOGI(TAG, "%s---", __func__);

    if (!ctx || !response) {
        return MCP_RESULT_INVALID_PARAM;
    }

    // TODO: 实现大模型配置修改逻辑
    // 这里可以添加参数解析和配置更新逻辑
    
    LISA_LOGI(TAG, "Model configuration update requested");
    extern int change_info_page(lisaui_userdata_qrcode_inter_mode_e mode);
    change_info_page(LISAUI_USERDATA_QRCODE_INTER_CONFIGURE_DEVICE);
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
    cJSON_AddStringToObject(text_item, "text", "大模型配置已更新");
    cJSON_AddItemToArray(content_array, text_item);

    response->content = content_array;
    response->result = MCP_RESULT_SUCCESS;
    
    return MCP_RESULT_SUCCESS;
}

/**
 * @brief 处理人设修改请求
 */
static mcp_result_t model_persona_handler(const mcp_context_t *ctx, mcp_response_t *response)
{
    LISA_LOGI(TAG, "%s---", __func__);

    if (!ctx || !response) {
        return MCP_RESULT_INVALID_PARAM;
    }

    // 这里可以添加人设参数解析和更新逻辑
    extern int change_info_page(lisaui_userdata_qrcode_inter_mode_e mode);
    change_info_page(LISAUI_USERDATA_QRCODE_INTER_CONFIGURE_DEVICE);
    LISA_LOGI(TAG, "Model persona update requested");
    
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
    cJSON_AddStringToObject(text_item, "text", "人设配置已更新");
    cJSON_AddItemToArray(content_array, text_item);

    response->content = content_array;
    response->result = MCP_RESULT_SUCCESS;
    
    return MCP_RESULT_SUCCESS;
}

/**
 * @brief 处理通用配置修改请求
 */
static mcp_result_t general_config_handler(const mcp_context_t *ctx, mcp_response_t *response)
{
    LISA_LOGI(TAG, "%s---", __func__);

    if (!ctx || !response) {
        return MCP_RESULT_INVALID_PARAM;
    }

    // TODO: 实现通用配置修改逻辑
    // 这里可以添加通用配置参数解析和更新逻辑
    extern int change_info_page(lisaui_userdata_qrcode_inter_mode_e mode);
    change_info_page(LISAUI_USERDATA_QRCODE_INTER_CONFIGURE_DEVICE);
    LISA_LOGI(TAG, "General configuration update requested");
    
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
    cJSON_AddStringToObject(text_item, "text", "配置已更新");
    cJSON_AddItemToArray(content_array, text_item);

    response->content = content_array;
    response->result = MCP_RESULT_SUCCESS;
    
    return MCP_RESULT_SUCCESS;
}

/**
 * @brief 生成大模型配置工具的参数 Schema
 *
 * @return cJSON对象指针，失败返回NULL
 */
cJSON* generate_model_config_schema(void)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        LISA_LOGE(TAG, "Failed to create root object for model_config schema");
        return NULL;
    }

    if (!cJSON_AddStringToObject(root, "type", "object")) {
        LISA_LOGE(TAG, "Failed to add type to model_config schema");
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

/**
 * @brief 生成人设配置工具的参数 Schema
 *
 * @return cJSON对象指针，失败返回NULL
 */
cJSON* generate_model_persona_schema(void)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        LISA_LOGE(TAG, "Failed to create root object for model_persona schema");
        return NULL;
    }

    if (!cJSON_AddStringToObject(root, "type", "object")) {
        LISA_LOGE(TAG, "Failed to add type to model_persona schema");
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

/**
 * @brief 生成通用配置工具的参数 Schema
 *
 * @return cJSON对象指针，失败返回NULL
 */
cJSON* generate_general_config_schema(void)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        LISA_LOGE(TAG, "Failed to create root object for general_config schema");
        return NULL;
    }

    if (!cJSON_AddStringToObject(root, "type", "object")) {
        LISA_LOGE(TAG, "Failed to add type to general_config schema");
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

// 注册大模型配置工具
static const char *model_config_triggers[] = {
    "配置大模型",
    "修改大模型配置",
    "修改大模型设置",
    NULL
};

MCP_REGISTER_TOOL_STATIC(
    model_persona_update,
    "ls.built_in.model_persona_update",
    "更新助手人设配置",
    "1.0",
    generate_model_persona_schema,
    0,
    model_persona_handler,
    false,
    NULL
);

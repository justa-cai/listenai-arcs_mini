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
    if (!ctx || !response) {
        return MCP_RESULT_INVALID_PARAM;
    }

    // TODO: 实现大模型配置修改逻辑
    // 这里可以添加参数解析和配置更新逻辑
    
    LISA_LOGI(TAG, "Model configuration update requested");
    extern int change_info_page(lisaui_userdata_qrcode_inter_mode_e mode);
    change_info_page(LISAUI_USERDATA_QRCODE_INTER_CONFIGURE_DEVICE);
    response->content = cJSON_CreateString("大模型配置已更新");
    response->result = MCP_RESULT_SUCCESS;
    
    return MCP_RESULT_SUCCESS;
}

/**
 * @brief 处理人设修改请求
 */
static mcp_result_t model_persona_handler(const mcp_context_t *ctx, mcp_response_t *response)
{
    if (!ctx || !response) {
        return MCP_RESULT_INVALID_PARAM;
    }

    // TODO: 实现人设修改逻辑
    // 这里可以添加人设参数解析和更新逻辑
    extern int change_info_page(lisaui_userdata_qrcode_inter_mode_e mode);
    change_info_page(LISAUI_USERDATA_QRCODE_INTER_CONFIGURE_DEVICE);
    LISA_LOGI(TAG, "Model persona update requested");
    
    response->content = cJSON_CreateString("人设配置已更新");
    response->result = MCP_RESULT_SUCCESS;
    
    return MCP_RESULT_SUCCESS;
}

/**
 * @brief 处理通用配置修改请求
 */
static mcp_result_t general_config_handler(const mcp_context_t *ctx, mcp_response_t *response)
{
    if (!ctx || !response) {
        return MCP_RESULT_INVALID_PARAM;
    }

    // TODO: 实现通用配置修改逻辑
    // 这里可以添加通用配置参数解析和更新逻辑
    extern int change_info_page(lisaui_userdata_qrcode_inter_mode_e mode);
    change_info_page(LISAUI_USERDATA_QRCODE_INTER_CONFIGURE_DEVICE);
    LISA_LOGI(TAG, "General configuration update requested");
    
    response->content = cJSON_CreateString("配置已更新");
    response->result = MCP_RESULT_SUCCESS;
    
    return MCP_RESULT_SUCCESS;
}

// 注册大模型配置工具
static const char *model_config_triggers[] = {
    "配置大模型",
    "修改大模型配置",
    "修改大模型设置",
    NULL
};

MCP_REGISTER_TOOL_STATIC(
    model_config_update,
    "更新大模型配置，包括模型参数、生成参数等",
    "1.0",
    NULL,  // 参数定义，后续可以添加具体参数
    0,     // 参数数量
    model_config_handler,
    false,
    model_config_triggers
);

MCP_REGISTER_TOOL_STATIC(
    model_persona_update,
    "更新大模型的人设配置",
    "1.0",
    NULL,  // 参数定义，后续可以添加具体参数
    0,     // 参数数量
    model_persona_handler,
    false,
    NULL
);


MCP_REGISTER_TOOL_STATIC(
    general_config_update,
    "更新通用配置",
    "1.0",
    NULL,  // 参数定义，后续可以添加具体参数
    0,     // 参数数量
    general_config_handler,
    false,
    NULL
);

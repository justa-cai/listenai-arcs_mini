#include "brightness_control.h"
#include "aiui_mcp.h"
#include "lisa_log.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include "lisa_display.h"

#define TAG "brightness_control"

// 当前亮度状态 (0% - 100%)
static int current_brightness = 50;

static mcp_result_t brightness_control_handler(const mcp_context_t *ctx, mcp_response_t *response)
{
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
        response->content = cJSON_CreateString("错误：亮度必须在0-100之间");
        response->result = MCP_RESULT_INVALID_PARAM;
        return MCP_RESULT_INVALID_PARAM;
    }
    
    // 确保亮度是10的倍数
    int rounded_brightness = (brightness + 5) / 10 * 10;
    if (rounded_brightness > 100) {
        rounded_brightness = 100;
    }

    int old_brightness = current_brightness;
    current_brightness = rounded_brightness; // 使用四舍五入后的亮度值

    // 调用实际的亮度控制 API
    int ret = lisa_display_set_brightness(lisa_display_get(), current_brightness);
    if (ret != 0) {
        LISA_LOGE(TAG, "Failed to set display brightness: %d", ret);
        response->content = cJSON_CreateString("设置显示器亮度失败");
        response->result = -1;
        return -1;
    }
    
    LISA_LOGI(TAG, "Brightness changed from %d%% to %d%%", old_brightness, current_brightness);
    
    char result_msg[256];
    snprintf(result_msg, sizeof(result_msg), 
            "亮度已从 %d%% 调整到 %d%%", old_brightness, current_brightness);
    response->content = cJSON_CreateString(result_msg);
    response->result = MCP_RESULT_SUCCESS;
    
    return MCP_RESULT_SUCCESS;
}

// 获取当前亮度的处理函数
static mcp_result_t brightness_get_handler(const mcp_context_t *ctx, mcp_response_t *response)
{
    if (!ctx || !response) {
        return MCP_RESULT_INVALID_PARAM;
    }

    // 获取当前亮度
    // 注意：由于硬件可能不支持直接读取当前亮度，我们直接返回缓存的当前亮度值
    // 这样可以保持与设置的值一致
    
    LISA_LOGI(TAG, "Current brightness requested: %d%%", current_brightness);
    
    char result_msg[256];
    snprintf(result_msg, sizeof(result_msg), 
            "当前亮度为: %d%%", current_brightness);
    response->content = cJSON_CreateString(result_msg);
    response->result = MCP_RESULT_SUCCESS;
    
    return MCP_RESULT_SUCCESS;
}

// 定义设置亮度工具参数
static mcp_param_def_t brightness_control_params[] = {
    MCP_PARAM_DEF_INT_RANGE("brightness", true, "亮度值，以10%为步长 (0-100)", NULL, 0, 100),
    MCP_PARAM_DEF_END
};

// 定义获取亮度工具参数（无参数）
static mcp_param_def_t brightness_get_params[] = {
    MCP_PARAM_DEF_END
};

// 使用静态段注册宏注册设置亮度工具
MCP_REGISTER_TOOL_STATIC(display_set_brightness, 
                         "设置显示器的亮度。亮度可以10%为步长进行设置 (0, 10, 20, ..., 100)。当用户无具体设置数值时,得先调用display_get_brightness工具获取当前音量，再做调整。", 
                         "1.0", 
                         brightness_control_params, 
                         1, 
                         brightness_control_handler, 
                         false, 
                         NULL);

// 使用静态段注册宏注册获取亮度工具
MCP_REGISTER_TOOL_STATIC(display_get_brightness, 
                         "获取显示器的当前亮度。", 
                         "1.0", 
                         brightness_get_params, 
                         0, 
                         brightness_get_handler, 
                         false, 
                         NULL);

// 获取当前亮度的公共API
int get_current_brightness(void)
{
    // 返回缓存的当前亮度值
    // 注意：如果需要实时获取硬件亮度，需要硬件支持相应的API
    return current_brightness;
}

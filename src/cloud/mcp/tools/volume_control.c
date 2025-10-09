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
        response->content = cJSON_CreateString("错误：音量值必须在0-100之间");
        response->result = MCP_RESULT_INVALID_PARAM;
        return MCP_RESULT_INVALID_PARAM;
    }

    int old_volume = current_volume;
    current_volume = volume;

    // 调用实际的音量控制函数
    listen_set_volume(current_volume);
    
    LISA_LOGI(TAG, "Volume changed from %d%% to %d%%", old_volume, current_volume);
    
    char result_msg[256];
    snprintf(result_msg, sizeof(result_msg), 
            "音量已从 %d%% 设置到 %d%%", old_volume, current_volume);
    response->content = cJSON_CreateString(result_msg);
    response->result = MCP_RESULT_SUCCESS;
    
    return MCP_RESULT_SUCCESS;
}

// 获取当前音量的处理函数
static mcp_result_t volume_get_handler(const mcp_context_t *ctx, mcp_response_t *response)
{
    if (!ctx || !response) {
        return MCP_RESULT_INVALID_PARAM;
    }

    // 获取当前音量
    current_volume = listen_get_volume();
    
    LISA_LOGI(TAG, "Current volume requested: %d%%", current_volume);
    
    char result_msg[256];
    snprintf(result_msg, sizeof(result_msg), 
            "当前音量为: %d%%", current_volume);
    response->content = cJSON_CreateString(result_msg);
    response->result = MCP_RESULT_SUCCESS;
    
    return MCP_RESULT_SUCCESS;
}

// 定义设置音量工具参数
static mcp_param_def_t volume_control_params[] = {
    MCP_PARAM_DEF_INT_RANGE("volume", true, "音量值，范围0-100", NULL, 0, 100),
    MCP_PARAM_DEF_END
};

// 定义获取音量工具参数（无参数）
static mcp_param_def_t volume_get_params[] = {
    MCP_PARAM_DEF_END
};

// 使用静态段注册宏注册设置音量工具
MCP_REGISTER_TOOL_STATIC(audio_speaker_set_volume, 
                         "设置当前音量。音量范围为0-100。", 
                         "1.0", 
                         volume_control_params, 
                         1, 
                         volume_control_handler, 
                         false, 
                         NULL);

// 使用静态段注册宏注册获取音量工具
MCP_REGISTER_TOOL_STATIC(audio_speaker_get_volume, 
                         "获取当前音量.", 
                         "1.0", 
                         volume_get_params, 
                         0, 
                         volume_get_handler, 
                         false, 
                         NULL);

int get_current_volume(void)
{
    // 返回当前音量
    return listen_get_volume();
}

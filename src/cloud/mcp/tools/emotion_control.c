#include "emotion_control.h"
#include "aiui_mcp.h"
#include "lisa_log.h"
#include "cJSON.h"
#include "assistant_controller.h"  // 包含控制器事件定义
#include <string.h>
#include <stdio.h>
#include "rtos_al.h"

#define TAG "emotion_control"

// 表情状态管理
typedef struct {
    const char *chinese;
    const char *english;
    const char *emoji_id;  // 对应的UI表情ID
} emotion_info_t;



static const emotion_info_t emotion_map[] = {
    {"无表情", "neutral", "blink"},  // 默认眨眼
    {"生气", "angry", "angry"},     // 生气
    {"开心", "happy", "happy"},     // 开心映射到happy
    {"撒娇", "cute", "cute"}        // 撒娇映射到cute
};

#define EMOTION_NEUTRAL 0
#define EMOTION_ANGRY   1
#define EMOTION_HAPPY   2
#define EMOTION_CUTE    3
#define EMOTION_COUNT   4

static int current_emotion = EMOTION_NEUTRAL;
static rtos_timer emoji_reset_timer = NULL;

// 表情设置处理函数
static mcp_result_t emotion_set_handler(const mcp_context_t *ctx, mcp_response_t *response)
{
    if (!ctx || !response) {
        return MCP_RESULT_INVALID_PARAM;
    }

    const char *emotion = NULL;
    
    // 解析参数
    for (uint32_t i = 0; i < ctx->param_count; i++) {
        if (strcmp(ctx->params[i].name, "emotion") == 0) {
            if (cJSON_IsString(ctx->params[i].value)) {
                emotion = ctx->params[i].value->valuestring;
            }
        }
    }

    if (!emotion) {
        response->content = cJSON_CreateString("错误：缺少必需的 emotion 参数");
        response->result = MCP_RESULT_INVALID_PARAM;
        return MCP_RESULT_INVALID_PARAM;
    }

    int new_emotion = -1;
    
    // 支持中文和英文表情名称
    if (strcmp(emotion, "生气") == 0 || strcmp(emotion, "angry") == 0) {
        new_emotion = EMOTION_ANGRY;
    } else if (strcmp(emotion, "开心") == 0 || strcmp(emotion, "happy") == 0 ) {
        new_emotion = EMOTION_HAPPY;  // 开心也接受love作为输入
    } else if (strcmp(emotion, "撒娇") == 0 || strcmp(emotion, "cute") == 0) {
        new_emotion = EMOTION_CUTE;
    } else if (strcmp(emotion, "无表情") == 0 || strcmp(emotion, "neutral") == 0 || strcmp(emotion, "blink") == 0) {
        new_emotion = EMOTION_NEUTRAL;
    } else {
        response->content = cJSON_CreateString("错误：表情只能是 '生气'/'angry'、'开心'/'happy'、'撒娇'/'cute' 或 '无表情'/'neutral'");
        response->result = MCP_RESULT_INVALID_PARAM;
        return MCP_RESULT_INVALID_PARAM;
    }

    int old_emotion = current_emotion;
    current_emotion = new_emotion;

    // 调用实际的表情控制接口
    const char* emoji_id = emotion_map[current_emotion].emoji_id;  // 使用UI表情ID
    LISA_LOGI(TAG, "Set emotion: %s, emoji_id: %s", emotion, emoji_id);
    assist_controller_set_mcp_emoji(emoji_id);

    LISA_LOGI(TAG, "Emotion changed from %s to %s", 
              emotion_map[old_emotion].chinese,
              emotion_map[current_emotion].chinese);
    
    char result_msg[256];
    snprintf(result_msg, sizeof(result_msg), 
            "表情已从 %s 设置为 %s", 
            emotion_map[old_emotion].chinese,
            emotion_map[current_emotion].chinese);
    response->content = cJSON_CreateString(result_msg);
    response->result = MCP_RESULT_SUCCESS;
    
    return MCP_RESULT_SUCCESS;
}

// 表情获取处理函数
static mcp_result_t emotion_get_handler(const mcp_context_t *ctx, mcp_response_t *response)
{
    if (!ctx || !response) {
        return MCP_RESULT_INVALID_PARAM;
    }
    
    LISA_LOGI(TAG, "Current emotion requested: %s", emotion_map[current_emotion].chinese);
    
    char result_msg[256];
    snprintf(result_msg, sizeof(result_msg), 
            "当前表情为: %s (%s)", 
            emotion_map[current_emotion].chinese,
            emotion_map[current_emotion].english);
    response->content = cJSON_CreateString(result_msg);
    response->result = MCP_RESULT_SUCCESS;
    
    return MCP_RESULT_SUCCESS;
}

// 表情清除处理函数
static mcp_result_t emotion_clear_handler(const mcp_context_t *ctx, mcp_response_t *response)
{
    if (!ctx || !response) {
        return MCP_RESULT_INVALID_PARAM;
    }

    int old_emotion = current_emotion;
    current_emotion = EMOTION_NEUTRAL;
    
    LISA_LOGI(TAG, "Emotion cleared from %s to neutral", emotion_map[old_emotion].chinese);
    
    char result_msg[256];
    snprintf(result_msg, sizeof(result_msg), 
            "表情已从 %s 清除为 无表情", 
            emotion_map[old_emotion].chinese);
    response->content = cJSON_CreateString(result_msg);
    response->result = MCP_RESULT_SUCCESS;
    
    return MCP_RESULT_SUCCESS;
}

// 表情设置工具参数定义
static mcp_param_def_t emotion_set_params[] = {
    MCP_PARAM_DEF("emotion", MCP_PARAM_STRING, true, 
                  "表情类型，可以是 '生气'/'angry'、'开心'/'happy'、'撒娇'/'cute' 或 '无表情'/'neutral'", 
                  NULL),
    MCP_PARAM_DEF_END
};

// 表情获取工具参数定义（无参数）
static mcp_param_def_t emotion_get_params[] = {
    MCP_PARAM_DEF_END
};

// 表情清除工具参数定义（无参数）
static mcp_param_def_t emotion_clear_params[] = {
    MCP_PARAM_DEF_END
};

// 注册表情设置工具
MCP_REGISTER_TOOL_STATIC(emotion_set, 
                         "设置设备表情表达。支持表情：生气、开心、撒娇、无表情。可以通过类似你生气是什么表情/给我撒个娇/开心一下等方式触发。", 
                         "1.0", 
                         emotion_set_params, 
                         1, 
                         emotion_set_handler, 
                         false, 
                         NULL);

// 注册表情获取工具
MCP_REGISTER_TOOL_STATIC(emotion_get, 
                         "Get current device emotion expression.", 
                         "1.0", 
                         emotion_get_params, 
                         0, 
                         emotion_get_handler, 
                         false, 
                         NULL);

// 注册表情清除工具
MCP_REGISTER_TOOL_STATIC(emotion_clear, 
                         "Clear current emotion and set to neutral state.", 
                         "1.0", 
                         emotion_clear_params, 
                         0, 
                         emotion_clear_handler, 
                         false, 
                         NULL);

const char* get_current_emotion(void)
{
    return emotion_map[current_emotion].chinese;
}

const char* get_current_emotion_en(void)
{
    return emotion_map[current_emotion].english;
}

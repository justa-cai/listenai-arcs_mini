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

// 表情设置处理函数
static mcp_result_t emotion_set_handler(const mcp_context_t *ctx, mcp_response_t *response)
{
    LISA_LOGI(TAG, "%s---", __func__);

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
        cJSON_AddStringToObject(text_item, "text", "错误：缺少必需的 emotion 参数");
        cJSON_AddItemToArray(content_array, text_item);

        response->content = content_array;
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
        cJSON_AddStringToObject(text_item, "text", "错误：表情只能是 '生气'/'angry'、'开心'/'happy'、'撒娇'/'cute' 或 '无表情'/'neutral'");
        cJSON_AddItemToArray(content_array, text_item);

        response->content = content_array;
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
 * @brief 生成表情设置工具的参数 Schema
 *
 * @return cJSON对象指针，失败返回NULL
 */
cJSON* generate_emotion_set_schema(void)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        LISA_LOGE(TAG, "Failed to create root object for emotion_set schema");
        return NULL;
    }

    if (!cJSON_AddStringToObject(root, "type", "object")) {
        LISA_LOGE(TAG, "Failed to add type to emotion_set schema");
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *properties = cJSON_CreateObject();
    if (!properties) {
        LISA_LOGE(TAG, "Failed to create properties object");
        cJSON_Delete(root);
        return NULL;
    }

    // emotion 参数
    cJSON *emotion_prop = cJSON_CreateObject();
    if (!emotion_prop) {
        LISA_LOGE(TAG, "Failed to create emotion property");
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    if (!cJSON_AddStringToObject(emotion_prop, "type", "string") ||
        !cJSON_AddStringToObject(emotion_prop, "description", "表情类型，可以是 '生气'/'angry'、'开心'/'happy'、'撒娇'/'cute' 或 '无表情'/'neutral'")) {
        LISA_LOGE(TAG, "Failed to add emotion property fields");
        cJSON_Delete(emotion_prop);
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToObject(properties, "emotion", emotion_prop);

    cJSON_AddItemToObject(root, "properties", properties);

    // required 数组
    cJSON *required = cJSON_CreateArray();
    if (!required) {
        LISA_LOGE(TAG, "Failed to create required array");
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *emotion_str = cJSON_CreateString("emotion");
    if (!emotion_str) {
        LISA_LOGE(TAG, "Failed to create emotion string for required array");
        cJSON_Delete(required);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToArray(required, emotion_str);
    cJSON_AddItemToObject(root, "required", required);

    return root;
}

// 注册表情设置工具
MCP_REGISTER_TOOL_STATIC(emotion_set,
                          "ls.built_in.set_emotion",
                          "设置设备表情表达。支持表情：生气、开心、撒娇、无表情。可以通过类似你生气是什么表情/给我撒个娇/开心一下等方式触发。",
                          "1.0",
                          generate_emotion_set_schema,
                          1,
                          emotion_set_handler,
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

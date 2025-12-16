#include "play_control.h"
#include "aiui_mcp.h"
#include "lisa_log.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include "player/audio_player.h"
#include "proc_mgr.h"
#include "assistant_controller.h"

#define TAG "play_control"

// Function to get audio player instance - implemented in proc_mgr.c
extern audioplayer_t *get_audio_player(void);

/**
 * @brief 播放控制处理函数
 *
 * 支持的intent:
 * - RESUME_PLAY: 继续播放/播放/取消暂停
 * - PAUSE: 暂停播放
 * - CHOOSE_PREVIOUS: 上一首/前一个
 * - CHOOSE_NEXT: 下一首/下一个/切歌
 * - REPLAY: 重播/再唱一遍/重复播放
 */
static mcp_result_t playback_control_handler(const mcp_context_t *ctx, mcp_response_t *response)
{
    LISA_LOGI(TAG, "%s---", __func__);

    if (!ctx || !response) {
        return MCP_RESULT_INVALID_PARAM;
    }

    const char *intent = NULL;

    // 解析参数: intent
    for (uint32_t i = 0; i < ctx->param_count; i++) {
        if (strcmp(ctx->params[i].name, "intent") == 0) {
            if (cJSON_IsString(ctx->params[i].value)) {
                intent = cJSON_GetStringValue(ctx->params[i].value);
            }
        }
    }

    // 参数校验
    if (!intent || strlen(intent) == 0) {
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
        cJSON_AddStringToObject(text_item, "text", "错误：缺少必需参数 intent");
        cJSON_AddItemToArray(content_array, text_item);

        response->content = content_array;
        response->result = MCP_RESULT_INVALID_PARAM;
        return MCP_RESULT_INVALID_PARAM;
    }

    // 获取音频播放器实例
    audioplayer_t *player = get_audio_player();
    if (!player) {
        LISA_LOGE(TAG, "Failed to get audio player instance");
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
        cJSON_AddStringToObject(text_item, "text", "错误：播放器未初始化");
        cJSON_AddItemToArray(content_array, text_item);

        response->content = content_array;
        response->result = MCP_RESULT_ERROR;
        return MCP_RESULT_ERROR;
    }

    // 根据intent执行相应操作
    if (strcmp(intent, "RESUME_PLAY") == 0) {
        // 继续播放
        LISA_LOGI(TAG, "Playback control: RESUME_PLAY");
        if (player->resumeByVoice) {
            listen_audioplayer_puse(player);
		    music_control_msg(MUSIC_REPLAY);
        } else {
            LISA_LOGW(TAG, "resumeByVoice function not available");
        }
    }
    else if (strcmp(intent, "PAUSE") == 0) {
        // 暂停播放
        LISA_LOGI(TAG, "Playback control: PAUSE");
        if (player->pause) {
            listen_audioplayer_puse(player);
            assist_controller_trigger_event(CONTROLLER_EVENT_STATE_AUDIO_PRE_IDLE, NULL, 0);
        } else {
            LISA_LOGW(TAG, "pause function not available");
        }
    }
    else if (strcmp(intent, "CHOOSE_PREVIOUS") == 0) {
        // 上一首
        LISA_LOGI(TAG, "Playback control: CHOOSE_PREVIOUS");
        if (player->prev) {
            listen_audioplayer_puse(player);
		    music_control_msg(MUSIC_PLAY_PREV);
        } else {
            LISA_LOGW(TAG, "prev function not available");
        }
    }
    else if (strcmp(intent, "CHOOSE_NEXT") == 0) {
        // 下一首
        LISA_LOGI(TAG, "Playback control: CHOOSE_NEXT");
        if (player->next) {
            listen_audioplayer_puse(player);//先暂停，防止TTS播完，下一首还没开始，当前播放歌曲又短暂播放一会儿
		    music_control_msg(MUSIC_PLAY_NEXT);
        } else {
            LISA_LOGW(TAG, "next function not available");
        }
    }
    else if (strcmp(intent, "REPLAY") == 0) {
        // 重播
        LISA_LOGI(TAG, "Playback control: REPLAY");
        if (player->replay) {
            listen_audioplayer_puse(player);
		    music_control_msg(MUSIC_RESUME_PLAY);
        } else {
            LISA_LOGW(TAG, "replay function not available");
        }
    }
    else {
        // 未知intent
        LISA_LOGW(TAG, "Unknown playback control intent: %s", intent);

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
 * @brief 生成播放控制工具的参数 Schema
 *
 * @return cJSON对象指针，失败返回NULL
 */
cJSON* generate_playback_control_schema(void)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        LISA_LOGE(TAG, "Failed to create root object for playback_control schema");
        return NULL;
    }

    if (!cJSON_AddStringToObject(root, "type", "object")) {
        LISA_LOGE(TAG, "Failed to add type to playback_control schema");
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *properties = cJSON_CreateObject();
    if (!properties) {
        LISA_LOGE(TAG, "Failed to create properties object");
        cJSON_Delete(root);
        return NULL;
    }

    // intent 参数
    cJSON *intent_prop = cJSON_CreateObject();
    if (!intent_prop) {
        LISA_LOGE(TAG, "Failed to create intent property");
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    if (!cJSON_AddStringToObject(intent_prop, "type", "string") ||
        !cJSON_AddStringToObject(intent_prop, "description",
                           "具体的指令：RESUME_PLAY（播放、继续、取消暂停等继续播放意图）、"
                           "PAUSE（暂停、不想听了等暂停播放意图）、"
                           "CHOOSE_PREVIOUS（上一个、前一首等向上意图）、"
                           "CHOOSE_NEXT（下一个、切歌等向下意图）、"
                           "REPLAY（重播、再唱一遍等重复播放意图）")) {
        LISA_LOGE(TAG, "Failed to add intent property fields");
        cJSON_Delete(intent_prop);
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToObject(properties, "intent", intent_prop);

    cJSON_AddItemToObject(root, "properties", properties);

    // required 数组
    cJSON *required = cJSON_CreateArray();
    if (!required) {
        LISA_LOGE(TAG, "Failed to create required array");
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *intent_str = cJSON_CreateString("intent");
    if (!intent_str) {
        LISA_LOGE(TAG, "Failed to create intent string for required array");
        cJSON_Delete(required);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToArray(required, intent_str);
    cJSON_AddItemToObject(root, "required", required);

    return root;
}

// 使用静态段注册宏注册播放控制工具
MCP_REGISTER_TOOL_STATIC(playback_control,
                          "ls.playback_control",
                          "播放控制工具：用于控制播放器的相关功能，如\"继续播放\"、\"暂停\"、\"上一个\"、\"下一个\"、\"重播\"",
                          "1.0",
                          generate_playback_control_schema,
                          1,
                          playback_control_handler,
                          false,
                          NULL);

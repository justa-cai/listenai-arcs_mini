#include "kuwo_music.h"
#include "aiui_mcp.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include "player/audio_player.h"
#include "player/audio_out.h"
#include "assistant_controller.h"
#include "proc_mgr.h"

#define TAG "kuwo_music"

// Function to get audio player instance - implemented in proc_mgr.c
extern audioplayer_t *get_audio_player(void);

/**
 * @brief 播放酷我音乐处理函数
 *
 * 该工具用于播放来自酷我音乐服务的音乐列表。
 * 支持接收包含音乐ID、名称、可播放状态等信息的音乐数组。
 */
static mcp_result_t kuwo_music_handler(const mcp_context_t *ctx, mcp_response_t *response)
{
    LISA_LOGI(TAG, "%s---", __func__);

    if (!ctx || !response) {
        return MCP_RESULT_INVALID_PARAM;
    }

    cJSON *result_array = NULL;
    cJSON *result_obj = NULL;

    // 解析参数: result (可能是对象或数组)
    for (uint32_t i = 0; i < ctx->param_count; i++) {
        if (strcmp(ctx->params[i].name, "result") == 0) {
            if (cJSON_IsArray(ctx->params[i].value)) {
                // 旧格式: result 直接是数组
                result_array = ctx->params[i].value;
            } else if (cJSON_IsObject(ctx->params[i].value)) {
                // 新格式: result 是对象，包含 items 数组
                result_obj = ctx->params[i].value;
                cJSON *items = cJSON_GetObjectItem(result_obj, "items");
                if (items && cJSON_IsArray(items)) {
                    result_array = items;
                }
            }
        }
    }

    // 参数校验
    if (!result_array) {
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
        cJSON_AddStringToObject(text_item, "text", "错误：缺少必需参数 result 或 result.items");
        cJSON_AddItemToArray(content_array, text_item);

        response->content = content_array;
        response->result = MCP_RESULT_INVALID_PARAM;
        return MCP_RESULT_INVALID_PARAM;
    }

    int array_size = cJSON_GetArraySize(result_array);
    if (array_size <= 0) {
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
        cJSON_AddStringToObject(text_item, "text", "错误：音乐列表为空");
        cJSON_AddItemToArray(content_array, text_item);

        response->content = content_array;
        response->result = MCP_RESULT_INVALID_PARAM;
        return MCP_RESULT_INVALID_PARAM;
    }

    LISA_LOGI(TAG, "Kuwo music: Received %d music items", array_size);

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

    // 分配音频列表内存
    audio_out_t *audios = (audio_out_t *)lisa_mem_calloc(array_size, sizeof(audio_out_t));
    if (!audios) {
        LISA_LOGE(TAG, "Failed to allocate memory for audio list");
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
        cJSON_AddStringToObject(text_item, "text", "错误：内存分配失败");
        cJSON_AddItemToArray(content_array, text_item);

        response->content = content_array;
        response->result = MCP_RESULT_ERROR;
        return MCP_RESULT_ERROR;
    }

    int valid_count = 0;

    // 解析音乐列表
    for (int i = 0; i < array_size; i++) {
        cJSON *item = cJSON_GetArrayItem(result_array, i);
        if (!item) continue;

        // 获取 itemid (必需)
        cJSON *itemid = cJSON_GetObjectItem(item, "itemid");
        if (!itemid || !cJSON_IsString(itemid)) {
            LISA_LOGW(TAG, "Music item %d missing itemid, skipping", i);
            continue;
        }

        // 复制 itemid 到 mid 字段
        strncpy(audios[valid_count].mid, itemid->valuestring, AUIDO_OUT_MID_LEN - 1);
        audios[valid_count].mid[AUIDO_OUT_MID_LEN - 1] = '\0';

        // 获取 name (可选)
        cJSON *name = cJSON_GetObjectItem(item, "name");
        if (name && cJSON_IsString(name)) {
            // 过滤掉特殊字符 [ ] "
            int src_idx = 0, dst_idx = 0;
            const char *name_str = name->valuestring;
            while (name_str[src_idx] && dst_idx < AUIDO_OUT_NAME_LEN - 1) {
                char c = name_str[src_idx];
                if (c != '[' && c != ']' && c != '"') {
                    audios[valid_count].m_name[dst_idx++] = c;
                }
                src_idx++;
            }
            audios[valid_count].m_name[dst_idx] = '\0';
        }

        // 获取 playable (可选，用于日志)
        cJSON *playable = cJSON_GetObjectItem(item, "playable");
        int is_playable = 1;  // 默认可播放
        if (playable && cJSON_IsNumber(playable)) {
            is_playable = playable->valueint;
        }

        LISA_LOGI(TAG, "Music %d: itemid=%s, name=%s, playable=%d", 
                  valid_count, audios[valid_count].mid, 
                  audios[valid_count].m_name, is_playable);

        valid_count++;
    }

    if (valid_count == 0) {
        LISA_LOGW(TAG, "No valid music items found");
        lisa_mem_free(audios);
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

    // 调用播放器播放音乐列表
    if (player->on_directive) {
        player->on_directive(player, AUDIO_PLAY, audios, valid_count);
    } else {
        LISA_LOGW(TAG, "on_directive function not available");
        lisa_mem_free(audios);
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

    // 注意：不要在这里释放 audios，播放器会管理这个内存
    // lisa_mem_free(audios);

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
 * @brief 生成酷我音乐工具的参数 Schema
 *
 * @return cJSON对象指针，失败返回NULL
 */
cJSON* generate_kuwo_music_schema(void)
{
    cJSON *root = NULL;
    cJSON *properties = NULL;
    cJSON *result_prop = NULL;
    cJSON *items = NULL;
    cJSON *item_properties = NULL;
    cJSON *required = NULL;

    root = cJSON_CreateObject();
    if (!root) {
        LISA_LOGE(TAG, "Failed to create root object for kuwo_music schema");
        goto error;
    }

    if (!cJSON_AddStringToObject(root, "type", "object")) {
        LISA_LOGE(TAG, "Failed to add type to kuwo_music schema");
        goto error;
    }

    properties = cJSON_CreateObject();
    if (!properties) {
        LISA_LOGE(TAG, "Failed to create properties object");
        goto error;
    }

    // result 参数（数组）
    result_prop = cJSON_CreateObject();
    if (!result_prop) {
        LISA_LOGE(TAG, "Failed to create result property");
        goto error_with_properties;
    }
    if (!cJSON_AddStringToObject(result_prop, "type", "array") ||
        !cJSON_AddStringToObject(result_prop, "description", "音乐列表")) {
        LISA_LOGE(TAG, "Failed to add result property fields");
        goto error_with_result_prop;
    }

    // items 定义
    items = cJSON_CreateObject();
    if (!items) {
        LISA_LOGE(TAG, "Failed to create items object");
        goto error_with_result_prop;
    }
    if (!cJSON_AddStringToObject(items, "type", "object")) {
        LISA_LOGE(TAG, "Failed to add type to items");
        goto error_with_items;
    }

    // items.properties
    item_properties = cJSON_CreateObject();
    if (!item_properties) {
        LISA_LOGE(TAG, "Failed to create item_properties object");
        goto error_with_items;
    }

    // itemid 属性
    cJSON *itemid_prop = cJSON_CreateObject();
    if (!itemid_prop ||
        !cJSON_AddStringToObject(itemid_prop, "type", "string") ||
        !cJSON_AddStringToObject(itemid_prop, "description", "音乐唯一标识ID")) {
        LISA_LOGE(TAG, "Failed to create itemid property");
        if (itemid_prop) cJSON_Delete(itemid_prop);
        goto error_with_item_properties;
    }
    cJSON_AddItemToObject(item_properties, "itemid", itemid_prop);

    // name 属性
    cJSON *name_prop = cJSON_CreateObject();
    if (!name_prop ||
        !cJSON_AddStringToObject(name_prop, "type", "string") ||
        !cJSON_AddStringToObject(name_prop, "description", "音乐名称")) {
        LISA_LOGE(TAG, "Failed to create name property");
        if (name_prop) cJSON_Delete(name_prop);
        goto error_with_item_properties;
    }
    cJSON_AddItemToObject(item_properties, "name", name_prop);

    // playable 属性
    cJSON *playable_prop = cJSON_CreateObject();
    if (!playable_prop ||
        !cJSON_AddStringToObject(playable_prop, "type", "integer") ||
        !cJSON_AddStringToObject(playable_prop, "description", "是否可播放")) {
        LISA_LOGE(TAG, "Failed to create playable property");
        if (playable_prop) cJSON_Delete(playable_prop);
        goto error_with_item_properties;
    }
    cJSON_AddItemToObject(item_properties, "playable", playable_prop);

    cJSON_AddItemToObject(items, "properties", item_properties);

    // items.required
    cJSON *item_required = cJSON_CreateArray();
    if (!item_required) {
        LISA_LOGE(TAG, "Failed to create item_required array");
        goto error_with_items;
    }
    cJSON *itemid_str = cJSON_CreateString("itemid");
    if (!itemid_str) {
        LISA_LOGE(TAG, "Failed to create itemid string");
        cJSON_Delete(item_required);
        goto error_with_items;
    }
    cJSON_AddItemToArray(item_required, itemid_str);
    cJSON_AddItemToObject(items, "required", item_required);

    cJSON_AddItemToObject(result_prop, "items", items);
    cJSON_AddItemToObject(properties, "result", result_prop);
    cJSON_AddItemToObject(root, "properties", properties);

    // required 数组
    required = cJSON_CreateArray();
    if (!required) {
        LISA_LOGE(TAG, "Failed to create required array");
        goto error;
    }
    cJSON *result_str = cJSON_CreateString("result");
    if (!result_str) {
        LISA_LOGE(TAG, "Failed to create result string");
        cJSON_Delete(required);
        goto error;
    }
    cJSON_AddItemToArray(required, result_str);
    cJSON_AddItemToObject(root, "required", required);

    return root;

error_with_item_properties:
    cJSON_Delete(item_properties);
error_with_items:
    cJSON_Delete(items);
error_with_result_prop:
    cJSON_Delete(result_prop);
error_with_properties:
    cJSON_Delete(properties);
error:
    if (root) cJSON_Delete(root);
    return NULL;
}

// 使用静态段注册宏注册酷我音乐工具
MCP_REGISTER_TOOL_STATIC(ls_built_in_play_kuwo_music,
                         "ls.built_in.play_kuwo_music",
                         "播放酷我音乐",
                         "1.0",
                         generate_kuwo_music_schema,
                         1,
                         kuwo_music_handler,
                         false,
                         NULL);

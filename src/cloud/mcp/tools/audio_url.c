#include "audio_url.h"
#include "aiui_mcp.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include "player/audio_player.h"
#include "player/audio_out.h"
#include "assistant_controller.h"

#define TAG "audio_url"

// Function to get audio player instance - implemented in proc_mgr.c
extern audioplayer_t *get_audio_player(void);

/**
 * @brief 播放音频资源地址处理函数
 *
 * 该工具用于播放通过URL提供的音频资源列表。
 * 支持接收包含音频URL和名称的音频资源数组。
 */
static mcp_result_t audio_url_handler(const mcp_context_t *ctx, mcp_response_t *response)
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
        cJSON_AddStringToObject(text_item, "text", "错误：缺少必需参数 result");
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
        cJSON_AddStringToObject(text_item, "text", "错误：音频资源列表为空");
        cJSON_AddItemToArray(content_array, text_item);

        response->content = content_array;
        response->result = MCP_RESULT_INVALID_PARAM;
        return MCP_RESULT_INVALID_PARAM;
    }

    LISA_LOGI(TAG, "Audio URL: Received %d audio items", array_size);

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

    // 解析音频资源列表
    for (int i = 0; i < array_size; i++) {
        cJSON *item = cJSON_GetArrayItem(result_array, i);
        if (!item) continue;

        // 获取 url (必需)
        cJSON *url = cJSON_GetObjectItem(item, "url");
        if (!url || !cJSON_IsString(url)) {
            LISA_LOGW(TAG, "Audio item %d missing url, skipping", i);
            continue;
        }

        // 验证URL长度
        size_t url_len = strlen(url->valuestring);
        if (url_len == 0 || url_len >= AUIDO_OUT_URL_LEN) {
            LISA_LOGW(TAG, "Audio item %d has invalid url length: %zu, skipping", i, url_len);
            continue;
        }

        // 复制 url 到 m_url 字段
        strncpy(audios[valid_count].m_url, url->valuestring, AUIDO_OUT_URL_LEN - 1);
        audios[valid_count].m_url[AUIDO_OUT_URL_LEN - 1] = '\0';

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

        LISA_LOGI(TAG, "Audio %d: url=%s, name=%s", 
                  valid_count, audios[valid_count].m_url, 
                  audios[valid_count].m_name);

        valid_count++;
    }

    if (valid_count == 0) {
        LISA_LOGW(TAG, "No valid audio items found");
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
        cJSON_AddStringToObject(text_item, "text", "audio url param error");
        cJSON_AddItemToArray(content_array, text_item);

        response->content = content_array;
        response->result = MCP_RESULT_ERROR;
        return MCP_RESULT_ERROR;
    }

    // 调用播放器播放音频列表
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
        response->result = MCP_RESULT_ERROR;
        return MCP_RESULT_ERROR;
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
 * @brief 生成音频资源列表的 JSON Schema 描述字符串
 *
 * @return cJSON对象指针，失败返回NULL
 */
cJSON * generate_audio_url_schema(void)
{
    // 创建根对象
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        LISA_LOGE(TAG, "Failed to create root object for audio_url schema");
        return NULL;
    }

    if (!cJSON_AddStringToObject(root, "type", "object")) {
        LISA_LOGE(TAG, "Failed to add type to audio_url schema");
        cJSON_Delete(root);
        return NULL;
    }

    // 创建外层 properties 对象
    cJSON *root_properties = cJSON_CreateObject();
    if (!root_properties) {
        LISA_LOGE(TAG, "Failed to create root properties object");
        cJSON_Delete(root);
        return NULL;
    }

    // 创建 result 数组属性
    cJSON *result_prop = cJSON_CreateObject();
    if (!result_prop) {
        LISA_LOGE(TAG, "Failed to create result property");
        cJSON_Delete(root_properties);
        cJSON_Delete(root);
        return NULL;
    }

    if (!cJSON_AddStringToObject(result_prop, "type", "array") ||
        !cJSON_AddStringToObject(result_prop, "description", "音频资源列表")) {
        LISA_LOGE(TAG, "Failed to add result property fields");
        cJSON_Delete(result_prop);
        cJSON_Delete(root_properties);
        cJSON_Delete(root);
        return NULL;
    }

    // 创建 items 对象
    cJSON *items = cJSON_CreateObject();
    if (!items) {
        LISA_LOGE(TAG, "Failed to create items object");
        cJSON_Delete(result_prop);
        cJSON_Delete(root_properties);
        cJSON_Delete(root);
        return NULL;
    }

    if (!cJSON_AddStringToObject(items, "type", "object")) {
        LISA_LOGE(TAG, "Failed to add type to items");
        cJSON_Delete(items);
        cJSON_Delete(result_prop);
        cJSON_Delete(root_properties);
        cJSON_Delete(root);
        return NULL;
    }

    // 创建 items 内部的 properties 对象
    cJSON *item_properties = cJSON_CreateObject();
    if (!item_properties) {
        LISA_LOGE(TAG, "Failed to create item properties object");
        cJSON_Delete(items);
        cJSON_Delete(result_prop);
        cJSON_Delete(root_properties);
        cJSON_Delete(root);
        return NULL;
    }

    // 添加 url 属性
    cJSON *url_prop = cJSON_CreateObject();
    if (!url_prop) {
        LISA_LOGE(TAG, "Failed to create url property");
        cJSON_Delete(item_properties);
        cJSON_Delete(items);
        cJSON_Delete(result_prop);
        cJSON_Delete(root_properties);
        cJSON_Delete(root);
        return NULL;
    }
    if (!cJSON_AddStringToObject(url_prop, "type", "string") ||
        !cJSON_AddStringToObject(url_prop, "description", "音频资源地址")) {
        LISA_LOGE(TAG, "Failed to add url property fields");
        cJSON_Delete(url_prop);
        cJSON_Delete(item_properties);
        cJSON_Delete(items);
        cJSON_Delete(result_prop);
        cJSON_Delete(root_properties);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToObject(item_properties, "url", url_prop);

    // 添加 name 属性
    cJSON *name_prop = cJSON_CreateObject();
    if (!name_prop) {
        LISA_LOGE(TAG, "Failed to create name property");
        cJSON_Delete(item_properties);
        cJSON_Delete(items);
        cJSON_Delete(result_prop);
        cJSON_Delete(root_properties);
        cJSON_Delete(root);
        return NULL;
    }
    if (!cJSON_AddStringToObject(name_prop, "type", "string") ||
        !cJSON_AddStringToObject(name_prop, "description", "音频资源名称")) {
        LISA_LOGE(TAG, "Failed to add name property fields");
        cJSON_Delete(name_prop);
        cJSON_Delete(item_properties);
        cJSON_Delete(items);
        cJSON_Delete(result_prop);
        cJSON_Delete(root_properties);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToObject(item_properties, "name", name_prop);

    // 将 item_properties 添加到 items
    cJSON_AddItemToObject(items, "properties", item_properties);

    // 添加 items 的 required 数组
    cJSON *item_required = cJSON_CreateArray();
    if (!item_required) {
        LISA_LOGE(TAG, "Failed to create item required array");
        cJSON_Delete(items);
        cJSON_Delete(result_prop);
        cJSON_Delete(root_properties);
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *url_str = cJSON_CreateString("url");
    if (!url_str) {
        LISA_LOGE(TAG, "Failed to create url string for item required array");
        cJSON_Delete(item_required);
        cJSON_Delete(items);
        cJSON_Delete(result_prop);
        cJSON_Delete(root_properties);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToArray(item_required, url_str);
    cJSON_AddItemToObject(items, "required", item_required);

    // 将 items 添加到 result_prop
    cJSON_AddItemToObject(result_prop, "items", items);

    // 将 result 添加到 root_properties
    cJSON_AddItemToObject(root_properties, "result", result_prop);

    // 将 root_properties 添加到 root
    cJSON_AddItemToObject(root, "properties", root_properties);

    // 添加根级别的 required 数组
    cJSON *root_required = cJSON_CreateArray();
    if (!root_required) {
        LISA_LOGE(TAG, "Failed to create root required array");
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *result_str = cJSON_CreateString("result");
    if (!result_str) {
        LISA_LOGE(TAG, "Failed to create result string for root required array");
        cJSON_Delete(root_required);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToArray(root_required, result_str);
    cJSON_AddItemToObject(root, "required", root_required);

    return root;
}

// 使用静态段注册宏注册音频资源地址工具
MCP_REGISTER_TOOL_STATIC(play_audio_link,
                          "ls.built_in.play_audio_link",
                          "播放音频资源地址",
                          "1.0",
                          generate_audio_url_schema,
                          1,
                          audio_url_handler,
                          false,
                          NULL);

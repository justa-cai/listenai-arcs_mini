/**
 * @brief 随机播放音乐功能
 *
 * 功能：从在线音乐服务器随机选择一首歌进行播放
 * API: http://192.168.31.205:9101/api/list
 *
 * 触发方式：单击电源键调用 ls_builtin_play_random_music()
 */

#include "audio_url.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "player/audio_player.h"
#include "player/audio_out.h"
#include "player/tts_player.h"
#include "audio/app_player.h"
#include "lisa_http.h"
#include "lisa_time.h"
#include "evs_utils.h"
#include "music_manager.h"

#define TAG "music_random"

// 获取音频播放器实例
extern audioplayer_t *get_audio_player(void);
extern tts_player_t *get_tts_player(void);

// 音乐服务器地址
#define MUSIC_SERVER_BASE_URL "http://192.168.31.205:9101"
#define MUSIC_API_RANDOM "/api/random"  // 获取随机歌曲（返回ID）
#define MUSIC_API_DOWNLOAD_PATTERN "%s/api/download/%d"  // 下载URL模板
#define MUSIC_API_TIMEOUT_MS (10000)  // 10秒超时

// HTTP响应缓冲区大小 - 只需要存储单首歌曲信息
#define HTTP_RESPONSE_BUFFER_SIZE (512)

/**
 * @brief 延迟播放音乐的上下文
 */
typedef struct {
    audio_out_t *audios;
    int valid_count;
    int retry_count;
} random_play_context_t;

/**
 * @brief HTTP 数据接收上下文
 */
typedef struct {
    char *buffer;           // 接收缓冲区
    size_t buffer_size;      // 缓冲区大小
    size_t total_len;        // 总接收长度
} http_response_context_t;

// 前向声明
static int play_random_music_with_delay(void *user_data);
static int get_random_music_from_server(audio_out_t *audio_out);

/**
 * @brief HTTP 数据回调
 */
static void http_data_callback(lisa_http_data_t *data)
{
    http_response_context_t *ctx = (http_response_context_t *)data->user;

    if (!ctx || !data->buf || !ctx->buffer) {
        LISA_LOGE(TAG, "Invalid context or data in callback");
        return;
    }

    size_t copy_len = data->len;
    size_t new_total = ctx->total_len + copy_len;

    // 检查是否需要扩容 (保留1字节给null终止符)
    if (new_total + 1 >= ctx->buffer_size) {
        size_t new_size = ctx->buffer_size * 2;
        while (new_size < new_total + 1) {
            new_size *= 2;
        }

        char *new_buffer = (char *)lisa_mem_realloc(ctx->buffer, new_size);
        if (!new_buffer) {
            LISA_LOGE(TAG, "Failed to reallocate response buffer, keeping original");
            // 保留原buffer, 仅停止接收新数据
            return;
        }

        ctx->buffer = new_buffer;
        ctx->buffer_size = new_size;
        LISA_LOGD(TAG, "Response buffer expanded to %zu bytes", new_size);
    }

    // 追加数据
    memcpy(ctx->buffer + ctx->total_len, data->buf, copy_len);
    ctx->total_len = new_total;
    ctx->buffer[ctx->total_len] = '\0';

    return;  // 继续接收
}

/**
 * @brief 从音乐服务器获取随机一首歌
 *
 * @param audio_out 输出参数，指向随机选择的音频资源
 * @return 0=成功, <0=失败
 */
static int get_random_music_from_server(audio_out_t *audio_out)
{
    lisa_http_t *http = NULL;
    lisa_http_request_t req = {0};
    lisa_http_err_e http_err;
    http_response_context_t response_ctx = {0};
    int ret = -1;
    char *response_buf = NULL;
    cJSON *root = NULL;

    LISA_LOGI(TAG, ">>> get_random_music_from_server ENTRY");

    // 使用固定大小的响应缓冲区（堆分配）
    response_ctx.buffer_size = HTTP_RESPONSE_BUFFER_SIZE;
    response_ctx.buffer = (char *)lisa_mem_alloc(response_ctx.buffer_size);
    if (!response_ctx.buffer) {
        LISA_LOGE(TAG, "Failed to allocate response buffer");
        return -1;
    }
    response_ctx.total_len = 0;
    LISA_LOGI(TAG, ">> Response buffer allocated: ptr=%p, size=%u",
             response_ctx.buffer, response_ctx.buffer_size);

    // 使用栈上的小缓冲区来构造URL（只需要~50字节）
    char url_buf[64];
    int url_len = snprintf(url_buf, sizeof(url_buf), "%s%s",
                         MUSIC_SERVER_BASE_URL, MUSIC_API_RANDOM);

    if (url_len <= 0 || url_len >= sizeof(url_buf)) {
        LISA_LOGE(TAG, ">> URL length invalid: %d", url_len);
        lisa_mem_free(response_ctx.buffer);
        return -1;
    }

    url_buf[sizeof(url_buf) - 1] = '\0';  // 确保null终止

    LISA_LOGI(TAG, ">> URL constructed: len=%d, url='%s'", url_len, url_buf);

    memset(&req, 0, sizeof(req));
    req.method = LISA_HTTP_GET;
    req.url = (uint8_t *)url_buf;
    req.timeout = MUSIC_API_TIMEOUT_MS;
    req.user = &response_ctx;
    req.on_data = http_data_callback;
    req.headers = NULL;
    req.body = NULL;
    req.body_len = 0;

    LISA_LOGI(TAG, ">> HTTP request prepared: method=%d, timeout=%d",
             req.method, req.timeout);

    // 执行HTTP请求
    http = lisa_http_init(&req);
    if (!http) {
        LISA_LOGE(TAG, ">> Failed to initialize HTTP request");
        goto cleanup;
    }

    LISA_LOGI(TAG, ">> Starting HTTP perform...");
    http_err = lisa_http_perform(http);
    if (http_err != LISA_HTTP_OK) {
        LISA_LOGE(TAG, ">> HTTP request failed with error: %d", http_err);
        goto cleanup;
    }

    LISA_LOGI(TAG, ">> HTTP completed! received=%zu bytes", response_ctx.total_len);

    if (response_ctx.total_len == 0 || response_ctx.total_len >= response_ctx.buffer_size) {
        LISA_LOGE(TAG, ">> Invalid response length: %zu (max=%u)",
                 response_ctx.total_len, response_ctx.buffer_size);
        goto cleanup;
    }

    // 确保响应null终止
    response_ctx.buffer[response_ctx.total_len] = '\0';
    LISA_LOGI(TAG, ">> Response null-terminated, len=%zu", response_ctx.total_len);

    // 解析JSON响应 - 新API直接返回歌曲信息
    LISA_LOGI(TAG, ">> Starting JSON parse...");
    root = cJSON_Parse(response_ctx.buffer);
    if (!root) {
        LISA_LOGE(TAG, ">> Failed to parse JSON, buffer='%.*s'",
                 (int)response_ctx.total_len, response_ctx.buffer);
        goto cleanup;
    }

    LISA_LOGI(TAG, ">> JSON parsed successfully");

    // 提取歌曲ID (必需) - 新API返回ID而非URL
    cJSON *id_item = cJSON_GetObjectItem(root, "id");
    if (!id_item || !cJSON_IsNumber(id_item)) {
        LISA_LOGE(TAG, ">> Response missing 'id' field");
        goto cleanup;
    }

    int song_id = cJSON_GetNumberValue(id_item);
    if (song_id <= 0) {
        LISA_LOGE(TAG, ">> Invalid song ID: %d", song_id);
        goto cleanup;
    }

    LISA_LOGI(TAG, ">> Song ID extracted: %d", song_id);

    // 构造下载URL: http://192.168.31.205:9101/api/download/{id}
    snprintf(audio_out->m_url, AUIDO_OUT_URL_LEN, MUSIC_API_DOWNLOAD_PATTERN,
             MUSIC_SERVER_BASE_URL, song_id);
    audio_out->m_url[AUIDO_OUT_URL_LEN - 1] = '\0';

    LISA_LOGI(TAG, ">> Download URL constructed: '%s'", audio_out->m_url);

    // 提取名称 (可选)
    cJSON *name_item = cJSON_GetObjectItem(root, "name");
    if (name_item && cJSON_IsString(name_item)) {
        const char *name_str = cJSON_GetStringValue(name_item);
        if (name_str) {
            // 复制名称，过滤特殊字符
            int src_idx = 0, dst_idx = 0;
            while (name_str[src_idx] && dst_idx < AUIDO_OUT_NAME_LEN - 1) {
                char c = name_str[src_idx];
                // 过滤方括号和引号
                if (c != '[' && c != ']' && c != '"') {
                    audio_out->m_name[dst_idx++] = c;
                }
                src_idx++;
            }
            audio_out->m_name[dst_idx] = '\0';
            LISA_LOGI(TAG, ">> Name extracted: '%s'", name_str);
        }
    } else {
        // 没有名称，使用默认
        strncpy(audio_out->m_name, "Random Music", AUIDO_OUT_NAME_LEN - 1);
        audio_out->m_name[AUIDO_OUT_NAME_LEN - 1] = '\0';
        LISA_LOGI(TAG, ">> Using default name");
    }

    // 清空其他字段
    audio_out->mid[0] = '\0';
    audio_out->m_artist[0] = '\0';
    audio_out->m_all_rate[0] = '\0';
    audio_out->throw_time = 0;

    LISA_LOGI(TAG, ">>> get_random_music_from_server SUCCESS: url='%s'",
             audio_out->m_url);
    ret = 0;  // 成功

cleanup:
    LISA_LOGI(TAG, "<<< get_random_music_from_server EXIT: ret=%d", ret);
    if (root) {
        LISA_LOGI(TAG, ">> Cleanup: deleting JSON root");
        cJSON_Delete(root);
    }
    if (http) {
        LISA_LOGI(TAG, ">> Cleanup: cleaning HTTP");
        lisa_http_cleanup(http);
    }
    if (response_ctx.buffer) {
        LISA_LOGI(TAG, ">> Cleanup: freeing response buffer");
        lisa_mem_free(response_ctx.buffer);
    }

    return ret;
}

/**
 * @brief 延迟播放音乐的回调函数
 */
static int play_random_music_with_delay(void *user_data)
{
    random_play_context_t *ctx = (random_play_context_t *)user_data;
    if (!ctx) {
        LISA_LOGE(TAG, "Invalid context for delayed playback");
        return -1;
    }

    // 获取播放器和TTS状态
    audioplayer_t *player = get_audio_player();
    tts_player_t *tts_player = get_tts_player();

    if (!player) {
        LISA_LOGE(TAG, "Audio player not available for delayed playback");
        lisa_mem_free(ctx->audios);
        lisa_mem_free(ctx);
        return -1;
    }

    // 检查TTS状态
    if (tts_player && ((tts_player->m_play_state == APP_PLAYER_PREPARING) ||
                       (tts_player->m_play_state == PLAYER_EVT_PLAYING) ||
                       (tts_player->m_play_state == PLAYER_EVT_PREPARED))) {
        // TTS仍在播放
        ctx->retry_count++;

        // 防止无限等待，最多重试20次（约10秒）
        if (ctx->retry_count < 20) {
            LISA_LOGI(TAG, "TTS still playing (state=%d, retry=%d), continue waiting",
                      tts_player->m_play_state, ctx->retry_count);
            // 继续延迟500ms
            evs_handler_post_runnable_delay(play_random_music_with_delay, ctx, 500);
            return 0;
        } else {
            LISA_LOGW(TAG, "TTS playing timeout after %d retries, forcing music playback",
                      ctx->retry_count);
            // 超时，强制播放
        }
    } else {
        LISA_LOGI(TAG, "TTS playback finished, starting music (retry_count=%d)",
                  ctx->retry_count);
    }

    // TTS播放完成或超时，播放音乐
    if (player->on_directive) {
        player->on_directive(player, AUDIO_PLAY, ctx->audios, ctx->valid_count);
        LISA_LOGI(TAG, "Music playback started with %d items", ctx->valid_count);
    } else {
        LISA_LOGW(TAG, "on_directive not available, music not played");
        lisa_mem_free(ctx->audios);
    }

    // 释放上下文内存
    lisa_mem_free(ctx);
    return 0;
}

/**
 * @brief 随机播放音乐的核心逻辑
 *
 * @return 0=成功, <0=失败
 */
__attribute__((used)) int play_random_music(void)
{
    LISA_LOGI(TAG, "Random music playback triggered via music manager");
    return music_manager_fetch_and_play_random(NULL, 0);
}

/**
 * @brief 随机播放音乐的回调包装函数
 *
 * @param user_data 未使用
 * @return 0=成功, <0=失败
 */
static int play_random_music_callback(void *user_data)
{
    (void)user_data;  // 未使用
    return play_random_music();
}

/**
 * @brief 随机播放音乐并返回歌曲信息 - 同步版本
 *
 * @param song_name 输出参数，存储歌曲名称
 * @param name_len song_name 缓冲区大小
 * @return 0=成功, <0=失败
 */
__attribute__((used)) int ls_builtin_play_random_music_sync(char *song_name, int name_len)
{
    if (!song_name || name_len <= 0) {
        return -1;
    }

    LISA_LOGI(TAG, "Random music playback (sync version with music manager)");

    int ret = music_manager_fetch_and_play_random(song_name, name_len);
    
    if (ret == 0) {
        LISA_LOGI(TAG, "Random music playback started via music manager: %s", song_name);
    }

    return ret;
}

/**
 * @brief 随机播放音乐 - 从在线服务器随机选择一首歌曲播放
 * 可通过单击电源键触发
 *
 * 注意：此函数只是轻量级触发器，实际工作延迟到 evs 上下文执行
 * 以避免在 btn task 栈溢出
 */
__attribute__((used)) void ls_builtin_play_random_music(void)
{
    LISA_LOGI(TAG, "Random music playback triggered (posting to evs)");
    // 使用 evs_handler_post_runnable 延迟执行，避免在 btn task 中直接执行
    // btn task 栈空间有限，直接执行可能导致栈溢出
    evs_handler_post_runnable(play_random_music_callback, NULL);
}

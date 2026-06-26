/**
 * @file    sd_music_http.c
 * @brief   SD 卡云端上报层实现
 */

#include "sd_music_http.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "FreeRTOS.h"
#include "HTTPCUsr_api.h"
#include "lisa_http.h"
#include "task.h"

#define TAG "sd_http"
#include "lisa_log.h"
#include "lisa_mem.h"

#include "app_datas.h"
#include "lsc.h"
#include "voice_music_list.h"

/* ---- 配置 ---- */

#define SD_MUSIC_API_HOST          "https://%sapi.listenai.com"
#define SD_MUSIC_API_TF_CARD_EVENT "/v1/media/tf-card/events"
#define SD_MUSIC_API_TF_CARD_FILES "/v1/media/tf-card/files/import"
#define SD_MUSIC_HTTP_LOG_HEAD_MAX 512
#define SD_MUSIC_HTTP_LOG_TAIL_MAX 256
#define SD_MUSIC_HTTP_FILES_REPORT_RETRY_COUNT 3
#define SD_MUSIC_HTTP_FILES_REPORT_RETRY_DELAY_BASE_MS 300U

/* ---- URL 构建 ---- */

static void build_api_url(char *url, int url_len, const char *path)
{
    const char *host_suffix = "";
    struct app_datas *app_datas = get_app_datas();
    if (app_datas) {
        if (app_datas->device_mode == DEVICE_MODE_STAGING) {
            host_suffix = "staging-";
        } else if (app_datas->device_mode == DEVICE_MODE_INTEGRATION) {
            host_suffix = "integration-";
        }
    }
    snprintf(url, url_len, SD_MUSIC_API_HOST "%s", host_suffix, path);
}

/* ---- Auth Header ---- */

static char *g_headers = NULL;

static void sd_http_log_payload_preview(const char *label, const char *data, int data_len)
{
    if (!data || data_len <= 0) {
        LOGI("%s: len=%d, body=<empty>", label, data_len);
        return;
    }

    int head_len = (data_len > SD_MUSIC_HTTP_LOG_HEAD_MAX) ?
                   SD_MUSIC_HTTP_LOG_HEAD_MAX : data_len;
    LOGI("%s: len=%d, head[%d]=%.*s",
         label, data_len, head_len, head_len, data);

    if (data_len > head_len) {
        int tail_len = data_len - head_len;
        if (tail_len > SD_MUSIC_HTTP_LOG_TAIL_MAX) {
            tail_len = SD_MUSIC_HTTP_LOG_TAIL_MAX;
        }
        LOGI("%s: tail[%d]=%.*s",
             label, tail_len, tail_len, data + data_len - tail_len);
    }
}

static int sd_http_match_utf8_sequence(const uint8_t *data, size_t len)
{
    if (!data || len == 0) {
        return 0;
    }

    if (data[0] <= 0x7F) {
        return 1;
    }

    if (len >= 2 &&
        data[0] >= 0xC2 && data[0] <= 0xDF &&
        data[1] >= 0x80 && data[1] <= 0xBF) {
        return 2;
    }

    if (len >= 3) {
        if (data[0] == 0xE0 &&
            data[1] >= 0xA0 && data[1] <= 0xBF &&
            data[2] >= 0x80 && data[2] <= 0xBF) {
            return 3;
        }
        if (((data[0] >= 0xE1 && data[0] <= 0xEC) ||
             (data[0] >= 0xEE && data[0] <= 0xEF)) &&
            data[1] >= 0x80 && data[1] <= 0xBF &&
            data[2] >= 0x80 && data[2] <= 0xBF) {
            return 3;
        }
        if (data[0] == 0xED &&
            data[1] >= 0x80 && data[1] <= 0x9F &&
            data[2] >= 0x80 && data[2] <= 0xBF) {
            return 3;
        }
    }

    if (len >= 4) {
        if (data[0] == 0xF0 &&
            data[1] >= 0x90 && data[1] <= 0xBF &&
            data[2] >= 0x80 && data[2] <= 0xBF &&
            data[3] >= 0x80 && data[3] <= 0xBF) {
            return 4;
        }
        if (data[0] >= 0xF1 && data[0] <= 0xF3 &&
            data[1] >= 0x80 && data[1] <= 0xBF &&
            data[2] >= 0x80 && data[2] <= 0xBF &&
            data[3] >= 0x80 && data[3] <= 0xBF) {
            return 4;
        }
        if (data[0] == 0xF4 &&
            data[1] >= 0x80 && data[1] <= 0x8F &&
            data[2] >= 0x80 && data[2] <= 0xBF &&
            data[3] >= 0x80 && data[3] <= 0xBF) {
            return 4;
        }
    }

    return 0;
}

static bool sd_http_copy_valid_utf8(char *dst, size_t dst_size,
                                    const char *src)
{
    const uint8_t *input = (const uint8_t *)src;
    size_t src_len;
    size_t in = 0;
    size_t out = 0;
    bool changed = false;

    if (!dst || dst_size == 0) {
        return true;
    }

    dst[0] = '\0';
    if (!src || src[0] == '\0') {
        return false;
    }

    src_len = strlen(src);
    while (in < src_len && out + 1 < dst_size) {
        int seq_len = sd_http_match_utf8_sequence(input + in, src_len - in);

        if (seq_len <= 0) {
            changed = true;
            in++;
            continue;
        }

        if (out + (size_t)seq_len >= dst_size) {
            changed = true;
            break;
        }

        memcpy(dst + out, input + in, (size_t)seq_len);
        in += (size_t)seq_len;
        out += (size_t)seq_len;
    }

    if (in < src_len) {
        changed = true;
    }
    dst[out] = '\0';
    return changed;
}

static void http_on_data(lisa_http_data_t *data)
{
    if (!data || !data->buf) {
        LOGW("HTTP 响应为空");
        return;
    }

    if (data->len <= 0) {
        LOGW("HTTP 响应长度无效: %d", data->len);
        return;
    }

    sd_http_log_payload_preview("HTTP 响应", (const char *)data->buf, data->len);
}

static void *http_headers_cb(void)
{
    return (void *)g_headers;
}

static char *build_auth_header(void)
{
    if (g_headers) {
        lisa_mem_free(g_headers);
        g_headers = NULL;
    }

    const char *token = get_lsc_jwt_token();
    if (!token) {
        LOGW("JWT token 不可用，跳过云端上报");
        return NULL;
    }

    const char *fmt = "Content-Type:application/json&Authorization:Bearer %s";
    int len = strlen(fmt) + strlen(token) + 1;
    g_headers = lisa_mem_calloc(1, len);
    if (g_headers) {
        snprintf(g_headers, len, fmt, token);
    }
    return g_headers;
}

static char *sd_music_http_build_file_json(int index)
{
    music_item_t track;
    char file_path[sizeof(track.m_url)];
    char file_name[sizeof(track.m_name)];
    char title[sizeof(track.m_title)];
    char artist[sizeof(track.m_artist)];
    char album[sizeof(track.m_album)];

    if (voice_music_list_get_by_index(index, &track) != 0) {
        return NULL;
    }

    if (sd_http_copy_valid_utf8(file_path, sizeof(file_path), track.m_url)) {
        LOGW("文件上报字段已清洗 UTF-8: index=%d, field=file_path, value=%s",
             index, file_path);
    }
    if (sd_http_copy_valid_utf8(file_name, sizeof(file_name), track.m_name)) {
        LOGW("文件上报字段已清洗 UTF-8: index=%d, field=file_name, value=%s",
             index, file_name);
    }
    if (sd_http_copy_valid_utf8(title, sizeof(title), track.m_title)) {
        LOGW("文件上报字段已清洗 UTF-8: index=%d, field=title, value=%s",
             index, title);
    }
    if (sd_http_copy_valid_utf8(artist, sizeof(artist), track.m_artist)) {
        LOGW("文件上报字段已清洗 UTF-8: index=%d, field=artist, value=%s",
             index, artist);
    }
    if (sd_http_copy_valid_utf8(album, sizeof(album), track.m_album)) {
        LOGW("文件上报字段已清洗 UTF-8: index=%d, field=album, value=%s",
             index, album);
    }

    cJSON *file = cJSON_CreateObject();
    if (!file) {
        return NULL;
    }

    cJSON_AddStringToObject(file, "file_path", file_path);
    cJSON_AddStringToObject(file, "file_name", file_name);

    if (title[0] || artist[0] || album[0]) {
        cJSON *tags = cJSON_AddObjectToObject(file, "media_tags");
        if (tags) {
            if (title[0])  cJSON_AddStringToObject(tags, "title", title);
            if (artist[0]) cJSON_AddStringToObject(tags, "artist", artist);
            if (album[0])  cJSON_AddStringToObject(tags, "album", album);
        }
    }

    char *body = cJSON_PrintUnformatted(file);
    cJSON_Delete(file);
    return body;
}

typedef enum {
    SD_MUSIC_FILES_STREAM_PREFIX = 0,
    SD_MUSIC_FILES_STREAM_COMMA,
    SD_MUSIC_FILES_STREAM_FILE,
    SD_MUSIC_FILES_STREAM_SUFFIX,
    SD_MUSIC_FILES_STREAM_DONE,
} sd_music_files_stream_state_t;

typedef struct {
    char prefix[256];
    int prefix_len;
    int prefix_pos;
    int count;
    int file_index;
    char *file_json;
    int file_json_len;
    int file_json_pos;
    int suffix_pos;
    uint32_t total_len;
    uint32_t delivered_len;
    sd_music_files_stream_state_t state;
    char chunk[HTTP_CLIENT_BUFFER_SIZE + 1];
} sd_music_files_stream_ctx_t;

static sd_music_files_stream_ctx_t *g_files_stream_ctx = NULL;

static void sd_music_files_stream_release_file(sd_music_files_stream_ctx_t *ctx)
{
    if (ctx && ctx->file_json) {
        cJSON_free(ctx->file_json);
        ctx->file_json = NULL;
        ctx->file_json_len = 0;
        ctx->file_json_pos = 0;
    }
}

static int sd_music_files_stream_init(sd_music_files_stream_ctx_t *ctx,
                                      const char *card_id,
                                      const char *scan_id,
                                      int count,
                                      bool is_last_batch)
{
    uint32_t total_len;
    int prefix_len;

    if (!ctx) {
        return -1;
    }

    memset(ctx, 0, sizeof(*ctx));

    prefix_len = snprintf(ctx->prefix, sizeof(ctx->prefix),
                          "{\"card_id\":\"%s\",\"scan_id\":\"%s\","
                          "\"is_last_batch\":%s,\"files\":[",
                          card_id, scan_id, is_last_batch ? "true" : "false");
    if (prefix_len < 0 || prefix_len >= (int)sizeof(ctx->prefix)) {
        return -1;
    }

    total_len = (uint32_t)prefix_len + 2;
    for (int i = 0; i < count; i++) {
        char *file_json = sd_music_http_build_file_json(i);
        if (!file_json) {
            return -1;
        }

        total_len += (uint32_t)strlen(file_json);
        if (i > 0) {
            total_len += 1;
        }
        cJSON_free(file_json);
    }

    ctx->prefix_len = prefix_len;
    ctx->count = count;
    ctx->total_len = total_len;
    ctx->state = SD_MUSIC_FILES_STREAM_PREFIX;
    return 0;
}

static int sd_music_files_stream_copy(char *dst, int dst_size, int *dst_len,
                                      const char *src, int src_len, int *src_pos)
{
    int remaining = src_len - *src_pos;
    int room = dst_size - *dst_len;
    int to_copy;

    if (remaining <= 0 || room <= 0) {
        return 0;
    }

    to_copy = remaining;
    if (to_copy > room) {
        to_copy = room;
    }

    memcpy(dst + *dst_len, src + *src_pos, (size_t)to_copy);
    *dst_len += to_copy;
    *src_pos += to_copy;
    return to_copy;
}

static int sd_music_files_stream_fill_chunk(sd_music_files_stream_ctx_t *ctx)
{
    static const char suffix[] = "]}";
    int chunk_len = 0;

    if (!ctx) {
        return -1;
    }

    while (chunk_len < HTTP_CLIENT_BUFFER_SIZE && ctx->state != SD_MUSIC_FILES_STREAM_DONE) {
        switch (ctx->state) {
        case SD_MUSIC_FILES_STREAM_PREFIX:
            sd_music_files_stream_copy(ctx->chunk, HTTP_CLIENT_BUFFER_SIZE, &chunk_len,
                                       ctx->prefix, ctx->prefix_len, &ctx->prefix_pos);
            if (ctx->prefix_pos >= ctx->prefix_len) {
                ctx->state = (ctx->count > 0) ?
                             SD_MUSIC_FILES_STREAM_COMMA :
                             SD_MUSIC_FILES_STREAM_SUFFIX;
            }
            break;

        case SD_MUSIC_FILES_STREAM_COMMA:
            if (ctx->file_index > 0) {
                ctx->chunk[chunk_len++] = ',';
            }
            ctx->state = SD_MUSIC_FILES_STREAM_FILE;
            break;

        case SD_MUSIC_FILES_STREAM_FILE:
            if (!ctx->file_json) {
                ctx->file_json = sd_music_http_build_file_json(ctx->file_index);
                if (!ctx->file_json) {
                    return -1;
                }
                ctx->file_json_len = (int)strlen(ctx->file_json);
                ctx->file_json_pos = 0;
            }

            sd_music_files_stream_copy(ctx->chunk, HTTP_CLIENT_BUFFER_SIZE, &chunk_len,
                                       ctx->file_json, ctx->file_json_len,
                                       &ctx->file_json_pos);
            if (ctx->file_json_pos >= ctx->file_json_len) {
                sd_music_files_stream_release_file(ctx);
                ctx->file_index++;
                ctx->state = (ctx->file_index < ctx->count) ?
                             SD_MUSIC_FILES_STREAM_COMMA :
                             SD_MUSIC_FILES_STREAM_SUFFIX;
            }
            break;

        case SD_MUSIC_FILES_STREAM_SUFFIX:
            sd_music_files_stream_copy(ctx->chunk, HTTP_CLIENT_BUFFER_SIZE, &chunk_len,
                                       suffix, (int)strlen(suffix), &ctx->suffix_pos);
            if (ctx->suffix_pos >= (int)strlen(suffix)) {
                ctx->state = SD_MUSIC_FILES_STREAM_DONE;
            }
            break;

        case SD_MUSIC_FILES_STREAM_DONE:
        default:
            break;
        }
    }

    ctx->chunk[chunk_len] = '\0';
    ctx->delivered_len += (uint32_t)chunk_len;
    return chunk_len;
}

static PostData sd_music_http_files_get_post_data(void)
{
    PostData post_data = {.pData = NULL, .pLength = 0};
    int chunk_len;

    if (!g_files_stream_ctx) {
        return post_data;
    }

    chunk_len = sd_music_files_stream_fill_chunk(g_files_stream_ctx);
    if (chunk_len < 0) {
        post_data.pLength = -1;
        return post_data;
    }

    if (chunk_len > 0) {
        post_data.pData = g_files_stream_ctx->chunk;
        post_data.pLength = chunk_len;
    }

    return post_data;
}

static int sd_music_http_read_response(const char *label,
                                       HTTPParameters *http_param,
                                       uint32_t request_body_len,
                                       uint32_t delivered_body_len,
                                       int *http_status_code,
                                       int *http_error_code)
{
    lisa_http_data_t http_data;
    HTTP_CLIENT http_client = {0};
    char *buf = NULL;
    unsigned int readsize = 0;
    size_t buf_capacity = 0;
    int ret;

    ret = HTTPC_get_request_info(http_param, &http_client);
    if (ret != 0) {
        if (http_error_code) {
            *http_error_code = ret;
        }
        LOGE("HTTP 请求信息获取失败: ret=%d", ret);
        return -1;
    }

    if (http_status_code) {
        *http_status_code = (int)http_client.HTTPStatusCode;
    }

    LOGI("%s响应: status=%u, resp_body_len=%u, delivered_body_len=%u, expect_body_len=%u",
         label ? label : "HTTP",
         http_client.HTTPStatusCode,
         http_client.TotalResponseBodyLength,
         delivered_body_len,
         request_body_len);

    if (http_client.TotalResponseBodyLength != 0) {
        UINT32 received = 0;
        UINT32 to_read = 0;
        int read_ret = 0;

        buf_capacity = http_client.TotalResponseBodyLength + 1;
        buf = lisa_mem_calloc(1, buf_capacity);
        if (!buf) {
            LOGE("HTTP 响应缓冲分配失败, len=%u", http_client.TotalResponseBodyLength);
            return -1;
        }

        do {
            to_read = http_client.TotalResponseBodyLength - readsize;
            if (to_read > HTTP_CLIENT_BUFFER_SIZE) {
                to_read = HTTP_CLIENT_BUFFER_SIZE;
            }
            if (to_read == 0) {
                break;
            }

            read_ret = HTTPC_read(http_param, buf + readsize, to_read, (void *)&received);
            if (read_ret != 0) {
                if (received > 0) {
                    readsize += received;
                }
                break;
            }
            if (received == 0) {
                read_ret = -1;
                break;
            }
            readsize += received;
        } while (readsize < http_client.TotalResponseBodyLength);

        if (readsize < http_client.TotalResponseBodyLength) {
            if (http_error_code) {
                *http_error_code = read_ret;
            }
            LOGE("HTTP 响应读取失败: ret=%d, got=%u, expect=%u",
                 read_ret, readsize, http_client.TotalResponseBodyLength);
            lisa_mem_free(buf);
            return -1;
        }
    } else {
        UINT32 received = 0;

        buf_capacity = HTTP_CLIENT_BUFFER_SIZE + 1;
        buf = lisa_mem_calloc(1, buf_capacity);
        if (!buf) {
            LOGE("HTTP 响应缓冲分配失败");
            return -1;
        }

        do {
            if ((readsize + HTTP_CLIENT_BUFFER_SIZE + 1) > buf_capacity) {
                size_t new_capacity = buf_capacity * 2;
                char *new_buf = NULL;

                if (new_capacity < (readsize + HTTP_CLIENT_BUFFER_SIZE + 1)) {
                    new_capacity = readsize + HTTP_CLIENT_BUFFER_SIZE + 1;
                }

                new_buf = lisa_mem_realloc(buf, new_capacity);
                if (!new_buf) {
                    LOGE("HTTP 响应缓冲扩容失败");
                    lisa_mem_free(buf);
                    return -1;
                }

                memset(new_buf + buf_capacity, 0, new_capacity - buf_capacity);
                buf = new_buf;
                buf_capacity = new_capacity;
            }

            if (HTTPC_read(http_param, buf + readsize, HTTP_CLIENT_BUFFER_SIZE,
                           (void *)&received) != 0) {
                if (received > 0) {
                    readsize += received;
                }
                break;
            }
            if (received == 0) {
                break;
            }
            readsize += received;
        } while (1);
    }

    buf[readsize] = '\0';
    http_data.buf = buf;
    http_data.len = readsize;
    http_data.user = NULL;
    http_on_data(&http_data);
    lisa_mem_free(buf);

    if (http_client.HTTPStatusCode != 200) {
        LOGE("%s响应状态异常: status=%u",
             label ? label : "HTTP", http_client.HTTPStatusCode);
        return -1;
    }

    return 0;
}

static int sd_music_http_perform_json_post(const char *label,
                                           const char *url,
                                           const char *body,
                                           int body_len,
                                           int *http_status_code,
                                           int *http_error_code)
{
    HTTPParameters *http_param = NULL;
    bool opened = false;
    int ret = -1;

    http_param = (HTTPParameters *)lisa_mem_calloc(1, sizeof(HTTPParameters));
    if (!http_param) {
        LOGE("%s HTTP 参数分配失败", label ? label : "HTTP");
        return -1;
    }

    if (strlen(url) >= sizeof(http_param->Uri)) {
        LOGE("%s URL 过长", label ? label : "HTTP");
        goto exit;
    }

    strcpy(http_param->Uri, url);
    http_param->HttpVerb = VerbPost;
    http_param->nTimeout = 10000;
    http_param->pData = (void *)body;
    http_param->pLength = (uint32_t)body_len;

    ret = HTTPC_open(http_param);
    if (ret != 0) {
        if (http_error_code) {
            *http_error_code = ret;
        }
        LOGE("%s HTTP 打开失败: ret=%d", label ? label : "HTTP", ret);
        ret = -1;
        goto exit;
    }
    opened = true;

    ret = HTTPC_request(http_param, http_headers_cb);
    if (ret != 0) {
        if (http_error_code) {
            *http_error_code = ret;
        }
        LOGE("%s HTTP 请求失败: ret=%d", label ? label : "HTTP", ret);
        ret = -1;
        goto exit;
    }

    ret = sd_music_http_read_response(label, http_param,
                                      (uint32_t)body_len,
                                      (uint32_t)body_len,
                                      http_status_code,
                                      http_error_code);

exit:
    if (opened) {
        HTTPC_close(http_param);
    }
    lisa_mem_free(http_param);
    return ret == 0 ? 0 : -1;
}

static int sd_music_http_perform_files_stream(const char *url,
                                              sd_music_files_stream_ctx_t *stream,
                                              int *http_status_code,
                                              int *http_error_code)
{
    HTTPParameters *http_param = NULL;
    bool opened = false;
    int ret = -1;
    int first_len;

    first_len = sd_music_files_stream_fill_chunk(stream);
    if (first_len <= 0) {
        LOGE("文件上报首个请求分片生成失败");
        return -1;
    }

    int head_len = first_len > SD_MUSIC_HTTP_LOG_HEAD_MAX ?
                   SD_MUSIC_HTTP_LOG_HEAD_MAX : first_len;
    int first_strlen = (int)strlen(stream->chunk);
    LOGI("文件上报请求体: len=%u, head[%d]=%.*s",
         stream->total_len, head_len, head_len, stream->chunk);
    if (first_strlen != first_len) {
        LOGW("文件上报请求体包含 C 字符串截断字符: first_len=%d, strlen=%d",
             first_len, first_strlen);
    }

    if (stream->state == SD_MUSIC_FILES_STREAM_DONE &&
        first_len == (int)stream->total_len) {
        LOGI("文件上报使用普通 POST: body_len=%u, strlen=%d",
             stream->total_len, first_strlen);
        ret = sd_music_http_perform_json_post("文件上报", url,
                                              stream->chunk, first_len,
                                              http_status_code,
                                              http_error_code);
        sd_music_files_stream_release_file(stream);
        return ret;
    }

    http_param = (HTTPParameters *)lisa_mem_calloc(1, sizeof(HTTPParameters));
    if (!http_param) {
        LOGE("文件上报 HTTP 参数分配失败");
        return -1;
    }

    if (strlen(url) >= sizeof(http_param->Uri)) {
        LOGE("文件上报 URL 过长");
        goto exit;
    }

    strcpy(http_param->Uri, url);
    http_param->HttpVerb = VerbPost;
    http_param->nTimeout = 10000;
    http_param->pData = stream->chunk;
    http_param->pLength = stream->total_len;
    LOGI("文件上报使用流式 POST: first_len=%d, body_len=%u",
         first_len, stream->total_len);

    ret = HTTPC_open(http_param);
    if (ret != 0) {
        if (http_error_code) {
            *http_error_code = ret;
        }
        LOGE("文件上报 HTTP 打开失败: ret=%d", ret);
        ret = -1;
        goto exit;
    }
    opened = true;

    g_files_stream_ctx = stream;
    ret = HTTPC_request_r(http_param, http_headers_cb, sd_music_http_files_get_post_data);
    g_files_stream_ctx = NULL;
    if (ret != 0) {
        if (http_error_code) {
            *http_error_code = ret;
        }
        LOGE("文件上报 HTTP 请求失败: ret=%d", ret);
        ret = -1;
        goto exit;
    }

    if (stream->delivered_len != stream->total_len) {
        LOGE("文件上报请求体发送不完整: sent=%u, total=%u",
             stream->delivered_len, stream->total_len);
        ret = -1;
        goto exit;
    }

    ret = sd_music_http_read_response("文件上报", http_param,
                                      stream->total_len,
                                      stream->delivered_len,
                                      http_status_code,
                                      http_error_code);

exit:
    g_files_stream_ctx = NULL;
    sd_music_files_stream_release_file(stream);
    if (opened) {
        HTTPC_close(http_param);
    }
    lisa_mem_free(http_param);
    return ret == 0 ? 0 : -1;
}

/* ---- TF 卡事件上报 ---- */

int sd_music_http_report_card_event(const char *card_id, const char *event_type,
                                    uint64_t capacity, int *http_status_code,
                                    int *http_error_code)
{
    if (http_status_code) {
        *http_status_code = 0;
    }
    if (http_error_code) {
        *http_error_code = 0;
    }

    if (!card_id || card_id[0] == '\0') {
        LOGW("card_id 为空，跳过事件上报");
        return -1;
    }

    char *headers = build_auth_header();
    if (!headers) return -1;
    (void)headers;

    cJSON *root = cJSON_CreateObject();
    if (!root) return -1;
    cJSON_AddStringToObject(root, "event_type", event_type);
    cJSON_AddStringToObject(root, "card_id", card_id);
    if (strcmp(event_type, "inserted") == 0) {
        cJSON_AddNumberToObject(root, "card_capacity", (double)capacity);
    }
    char *body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!body) return -1;

    char url[256];
    build_api_url(url, sizeof(url), SD_MUSIC_API_TF_CARD_EVENT);

    LOGI("上报 TF 卡事件: %s, card_id=%s", event_type, card_id);

    int body_len = (int)strlen(body);
    sd_http_log_payload_preview("TF 卡事件请求体", body, body_len);

    int ret = sd_music_http_perform_json_post("TF 卡事件", url, body,
                                              body_len, http_status_code,
                                              http_error_code);
    if (ret == 0) {
        LOGI("TF 卡事件上报成功: %s", event_type);
    } else {
        LOGE("TF 卡事件上报请求失败: %s, http_status=%d, http_error=%d",
             event_type,
             http_status_code ? *http_status_code : 0,
             http_error_code ? *http_error_code : 0);
    }

    cJSON_free(body);
    return ret;
}

/* ---- 文件列表上报 ---- */

static int sd_music_http_report_files_once(const char *url,
                                           const char *card_id,
                                           const char *scan_id,
                                           int count,
                                           bool is_last_batch,
                                           int *http_status_code,
                                           int *http_error_code)
{
    const int max_attempts = SD_MUSIC_HTTP_FILES_REPORT_RETRY_COUNT + 1;

    for (int attempt = 0; attempt < max_attempts; attempt++) {
        sd_music_files_stream_ctx_t stream;
        uint32_t retry_delay_ms;

        if (http_status_code) {
            *http_status_code = 0;
        }
        if (http_error_code) {
            *http_error_code = 0;
        }

        if (sd_music_files_stream_init(&stream, card_id, scan_id, count,
                                       is_last_batch) != 0) {
            LOGE("文件上报请求体流初始化失败, count=%d", count);
            return -1;
        }

        LOGI("文件上报 — count=%d, body_len=%u%s, attempt=%d/%d",
             count, stream.total_len,
             is_last_batch ? " (最后一批)" : "",
             attempt + 1, max_attempts);

        if (sd_music_http_perform_files_stream(url, &stream,
                                               http_status_code,
                                               http_error_code) == 0) {
            if (attempt > 0) {
                LOGI("文件上报重试成功, attempt=%d/%d", attempt + 1, max_attempts);
            } else {
                LOGI("文件上报成功");
            }
            return 0;
        }

        LOGW("文件上报请求失败, attempt=%d/%d, http_status=%d, http_error=%d",
             attempt + 1, max_attempts,
             http_status_code ? *http_status_code : 0,
             http_error_code ? *http_error_code : 0);

        if (attempt + 1 >= max_attempts) {
            break;
        }

        retry_delay_ms = SD_MUSIC_HTTP_FILES_REPORT_RETRY_DELAY_BASE_MS *
                         (uint32_t)(attempt + 1);
        LOGI("文件上报将在 %u ms 后重试", (unsigned int)retry_delay_ms);
        vTaskDelay(pdMS_TO_TICKS(retry_delay_ms));
    }

    LOGE("文件上报重试 %d 次后仍失败, http_status=%d, http_error=%d",
         SD_MUSIC_HTTP_FILES_REPORT_RETRY_COUNT,
         http_status_code ? *http_status_code : 0,
         http_error_code ? *http_error_code : 0);
    return -1;
}

int sd_music_http_report_files(const char *card_id, const char *scan_id,
                                int count, bool is_last_batch,
                                int *http_status_code,
                                int *http_error_code)
{
    if (http_status_code) {
        *http_status_code = 0;
    }
    if (http_error_code) {
        *http_error_code = 0;
    }

    if (count < 0) return -1;
    if (count == 0 && !is_last_batch) return 0;
    if (!card_id || card_id[0] == '\0') {
        LOGW("card_id 为空，跳过文件导入");
        return -1;
    }
    if (!scan_id || scan_id[0] == '\0') {
        LOGW("scan_id 为空，跳过文件导入");
        return -1;
    }

    char *headers = build_auth_header();
    if (!headers) return -1;
    (void)headers;

    LOGI("开始上报文件列表 — %d 个文件, card_id=%s, scan_id=%s%s",
         count, card_id, scan_id,
         is_last_batch ? " (最后一批)" : "");

    char url[256];
    build_api_url(url, sizeof(url), SD_MUSIC_API_TF_CARD_FILES);

    return sd_music_http_report_files_once(url, card_id, scan_id, count,
                                           is_last_batch, http_status_code,
                                           http_error_code);
}

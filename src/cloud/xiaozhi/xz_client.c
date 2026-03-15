/**
 * @file xz_client.c
 * @brief 小智云端主客户端实现
 */

#define TAG "xz_client"

#include "xz_client.h"
#include "xz_websocket.h"
#include "xz_message.h"
#include "xz_audio.h"
#include "xz_opus_dec.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "lisa_time.h"
#include "evs_utils.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>

/* 前向声明 */
static int heartbeat_check(void *arg);

/** 心跳间隔 (毫秒) */
#define HEARTBEAT_INTERVAL_MS  30000  /* 30秒 - 防止服务器超时 */

/** 小智客户端结构 */
struct xz_client_s {
    /* WebSocket */
    xz_websocket_t ws;

    /* 音频发送器 */
    xz_audio_t audio;

    /* TTS 解码器 */
    xz_opus_dec_t opus_decoder;

    /* 服务器音频参数 */
    int server_sample_rate;
    int server_channels;
    int server_frame_duration;

    /* 会话信息 */
    char session_id[64];

    /* 配置 */
    char url[256];
    char token[256];
    char device_id[64];
    char client_id[64];
    uint32_t timeout_ms;

    /* 回调 */
    xz_client_callbacks_t callbacks;

    /* 状态 */
    bool connected;
    bool interacting;
    bool hello_received;

    /* 心跳 */
    uint32_t last_heartbeat_time;
};

/**
 * @brief WebSocket 发送回调 (用于音频发送器)
 */
static int audio_send_callback(const void *data, uint32_t len, void *user_data)
{
    xz_client_t client = (xz_client_t)user_data;
    if (!client || !client->ws || !client->connected) {
        return -1;
    }

    return xz_ws_send_binary(client->ws, data, len);
}

/**
 * @brief WebSocket 事件回调
 */
static void ws_event_callback(xz_websocket_t ws, xz_ws_event_type_e event, void *user)
{
    (void)ws;
    xz_client_t client = (xz_client_t)user;
    if (!client) {
        return;
    }

    switch (event) {
        case XZ_WS_EVENT_CONNECTED:
            client->connected = true;
            client->last_heartbeat_time = lisa_os_get_tick_ms();
            LISA_LOGI(TAG, "WebSocket connected");

            /* 发送 Hello 消息 */
            char hello_msg[256];
            if (xz_msg_create_hello(hello_msg, sizeof(hello_msg)) > 0) {
                xz_ws_send_text(client->ws, hello_msg);
            }

            /* 启动心跳定时器 */
            evs_handler_post_runnable_delay(heartbeat_check, client, 5000);

            if (client->callbacks.on_connected) {
                client->callbacks.on_connected(client->callbacks.user_data);
            }
            break;

        case XZ_WS_EVENT_DISCONNECTED:
            client->connected = false;
            client->interacting = false;
            LISA_LOGW(TAG, "WebSocket disconnected");

            if (client->callbacks.on_disconnected) {
                client->callbacks.on_disconnected(client->callbacks.user_data);
            }
            break;

        case XZ_WS_EVENT_ERROR:
            LISA_LOGE(TAG, "WebSocket error");

            if (client->callbacks.on_error) {
                client->callbacks.on_error(-1, "WebSocket error", client->callbacks.user_data);
            }
            break;
    }
}

/**
 * @brief TTS 状态回调 (用于消息解析)
 */
static void tts_state_callback(xz_tts_state_e state, const char *text, const uint8_t *data, uint32_t len, void *user)
{
    (void)data;
    (void)len;
    xz_client_t client = (xz_client_t)user;
    if (!client) {
        return;
    }

    /* 如果有文本内容，触发文本回调 */
    if (text && strlen(text) > 0 && client->callbacks.on_tts_text) {
        LISA_LOGI(TAG, "TTS text: %s", text);
        client->callbacks.on_tts_text(text, client->callbacks.user_data);
    }

    if (state == XZ_TTS_STATE_START) {
        if (client->callbacks.on_tts_start) {
            client->callbacks.on_tts_start(client->callbacks.user_data);
        }
    } else if (state == XZ_TTS_STATE_END) {
        if (client->callbacks.on_tts_end) {
            client->callbacks.on_tts_end(client->callbacks.user_data);
        }
    }
    /* TTS 数据在二进制帧中处理 */
}

/**
 * @brief Hello 消息回调 - 处理服务器返回的音频参数
 */
static void hello_callback(const char *session_id, const xz_server_audio_params_t *audio_params, void *user)
{
    xz_client_t client = (xz_client_t)user;
    if (!client) {
        return;
    }

    LISA_LOGI(TAG, "Hello received: session_id=%s, sample_rate=%d, channels=%d, format=%s",
              session_id ? session_id : "null",
              audio_params->sample_rate, audio_params->channels, audio_params->format);

    /* 保存 session_id */
    if (session_id) {
        strncpy(client->session_id, session_id, sizeof(client->session_id) - 1);
        client->session_id[sizeof(client->session_id) - 1] = '\0';
    }

    /* 保存服务器音频参数 */
    client->server_sample_rate = audio_params->sample_rate;
    client->server_channels = audio_params->channels;
    client->server_frame_duration = audio_params->frame_duration;

    /* 创建 Opus 解码器 */
    if (client->opus_decoder) {
        xz_opus_dec_destroy(client->opus_decoder);
        client->opus_decoder = NULL;
    }

    xz_opus_dec_config_t dec_config = {
        .sample_rate = audio_params->sample_rate,
        .channels = audio_params->channels,
        .frame_size = (audio_params->sample_rate * audio_params->frame_duration) / 1000,
    };

    client->opus_decoder = xz_opus_dec_create(&dec_config);
    if (!client->opus_decoder) {
        LISA_LOGE(TAG, "Failed to create Opus decoder");
    } else {
        LISA_LOGI(TAG, "Opus decoder created for TTS: %dHz", audio_params->sample_rate);
    }

    client->hello_received = true;
}

/**
 * @brief 服务器 Ping 消息回调 - 自动回复 Pong
 */
static void server_ping_callback(void *user)
{
    xz_client_t client = (xz_client_t)user;
    if (!client || !client->connected) {
        return;
    }

    /* 自动回复 Pong 消息 */
    char pong_msg[64];
    int len = xz_msg_create_pong(pong_msg, sizeof(pong_msg));
    if (len > 0) {
        xz_ws_send_text(client->ws, pong_msg);
        LISA_LOGI(TAG, "Replied to server Ping with Pong");
    }
}

/**
 * @brief WebSocket 数据回调
 */
static void ws_data_callback(xz_websocket_t ws, xz_ws_data_type_e type,
                             const void *data, uint32_t len, void *user)
{
    (void)ws;
    xz_client_t client = (xz_client_t)user;
    if (!client) {
        return;
    }

    /* PONG 响应 - 心跳确认 */
    if (type == XZ_WS_DATA_PONG) {
        (void)data;  /* WebSocket PONG 没有负载数据 */
        (void)len;
        client->last_heartbeat_time = lisa_os_get_tick_ms();
        LISA_LOGI(TAG, "PONG received - connection alive");
        return;
    }

    /* 文本消息 - JSON */
    if (type == XZ_WS_DATA_TEXT && data && len > 0) {
        /* 确保以 null 结尾 */
        char *json_str = lisa_mem_alloc(len + 1);
        if (json_str) {
            memcpy(json_str, data, len);
            json_str[len] = '\0';

            /* 解析消息 */
            xz_msg_callbacks_t msg_cbs = {
                .on_hello = hello_callback,
                .on_ping = server_ping_callback,
                .on_stt = client->callbacks.on_stt_text,
                .on_llm = client->callbacks.on_llm_content,
                .on_llm_emoji = client->callbacks.on_llm_emoji,
                .on_tts = tts_state_callback,
                .on_iot = client->callbacks.on_iot_command,
                .on_error = client->callbacks.on_error,
                .user_data = client,
            };

            xz_msg_parse(json_str, &msg_cbs);
            lisa_mem_free(json_str);
        }
    }
    /* 二进制消息 - TTS 音频数据 */
    else if (type == XZ_WS_DATA_BINARY && data && len > 0) {
        if (client->callbacks.on_tts_data) {
            client->callbacks.on_tts_data((const uint8_t *)data, len, client->callbacks.user_data);
        }
    }
}

xz_client_t xz_client_create(const xz_client_config_t *config)
{
    if (!config || !config->url || !config->token) {
        LISA_LOGE(TAG, "Invalid config");
        return NULL;
    }

    /* 分配结构 */
    struct xz_client_s *client = (struct xz_client_s *)lisa_mem_calloc(1, sizeof(struct xz_client_s));
    if (!client) {
        LISA_LOGE(TAG, "Failed to allocate memory");
        return NULL;
    }

    /* 保存配置 */
    strncpy(client->url, config->url, sizeof(client->url) - 1);
    strncpy(client->token, config->token, sizeof(client->token) - 1);
    if (config->device_id) {
        strncpy(client->device_id, config->device_id, sizeof(client->device_id) - 1);
    } else {
        strcpy(client->device_id, "arcs_mini");
    }
    if (config->client_id) {
        strncpy(client->client_id, config->client_id, sizeof(client->client_id) - 1);
    } else {
        strcpy(client->client_id, "default");
    }
    client->timeout_ms = config->timeout_ms > 0 ? config->timeout_ms : 10000;

    /* 创建 WebSocket */
    xz_ws_config_t ws_config = {
        .url = client->url,
        .token = client->token,
        .device_id = client->device_id,
        .client_id = client->client_id,
        .timeout_ms = client->timeout_ms,
        .user = client,
        .on_event = ws_event_callback,
        .on_data = ws_data_callback,
    };

    client->ws = xz_ws_create(&ws_config);
    if (!client->ws) {
        LISA_LOGE(TAG, "Failed to create WebSocket");
        lisa_mem_free(client);
        return NULL;
    }

    /* 创建音频发送器 */
    xz_audio_config_t audio_config = {
        .sample_rate = 16000,
        .channels = 1,
        .frame_duration_ms = 60,
        .bitrate = 24000,
    };

    client->audio = xz_audio_create(&audio_config);
    if (!client->audio) {
        LISA_LOGE(TAG, "Failed to create audio sender");
        xz_ws_destroy(client->ws);
        lisa_mem_free(client);
        return NULL;
    }

    /* 设置音频发送回调 */
    xz_audio_set_send_callback(client->audio, audio_send_callback, client);

    client->connected = false;
    client->interacting = false;
    client->hello_received = false;
    client->opus_decoder = NULL;
    client->server_sample_rate = 24000;  /* 默认 24kHz */
    client->server_channels = 1;
    client->server_frame_duration = 60;

    LISA_LOGI(TAG, "XiaoZhi client created: %s", client->url);
    return client;
}

void xz_client_destroy(xz_client_t client)
{
    if (!client) {
        return;
    }

    if (client->connected) {
        xz_client_disconnect(client);
    }

    if (client->opus_decoder) {
        xz_opus_dec_destroy(client->opus_decoder);
        client->opus_decoder = NULL;
    }

    if (client->audio) {
        xz_audio_destroy(client->audio);
    }

    if (client->ws) {
        xz_ws_destroy(client->ws);
    }

    lisa_mem_free(client);
}

void xz_client_set_callbacks(xz_client_t client, const xz_client_callbacks_t *callbacks)
{
    if (!client) {
        return;
    }

    if (callbacks) {
        memcpy(&client->callbacks, callbacks, sizeof(xz_client_callbacks_t));
    }
}

int xz_client_connect(xz_client_t client)
{
    if (!client) {
        return -1;
    }

    if (client->connected) {
        LISA_LOGW(TAG, "Already connected");
        return 0;
    }

    LISA_LOGI(TAG, "Connecting to %s", client->url);
    return xz_ws_connect(client->ws);
}

int xz_client_disconnect(xz_client_t client)
{
    if (!client) {
        return -1;
    }

    if (client->interacting) {
        xz_client_stop_interaction(client);
    }

    LISA_LOGI(TAG, "Disconnecting");
    return xz_ws_disconnect(client->ws);
}

bool xz_client_is_connected(xz_client_t client)
{
    return client && client->connected;
}

int xz_client_start_interaction(xz_client_t client)
{
    if (!client || !client->connected) {
        LISA_LOGE(TAG, "Not connected");
        return -1;
    }

    if (client->interacting) {
        LISA_LOGW(TAG, "Already interacting");
        return 0;
    }

    /* 发送 Listen 消息 */
    char listen_msg[256];
    if (xz_msg_create_listen(listen_msg, sizeof(listen_msg), client->session_id) > 0) {
        xz_ws_send_text(client->ws, listen_msg);
    }

    /* 开始音频上传 */
    xz_audio_start(client->audio);
    client->interacting = true;

    LISA_LOGI(TAG, "Interaction started");
    return 0;
}

int xz_client_stop_interaction(xz_client_t client)
{
    if (!client || !client->interacting) {
        return 0;
    }

    /* 停止音频上传 */
    xz_audio_stop(client->audio);

    /* 发送 Stop 消息 */
    char stop_msg[256];
    if (xz_msg_create_stop(stop_msg, sizeof(stop_msg), client->session_id) > 0) {
        xz_ws_send_text(client->ws, stop_msg);
    }

    client->interacting = false;
    LISA_LOGI(TAG, "Interaction stopped");
    return 0;
}

int xz_client_write_audio(xz_client_t client, const int16_t *pcm_data, int samples)
{
    if (!client || !client->interacting) {
        /* 静默丢弃 */
        return 0;
    }

    return xz_audio_write(client->audio, pcm_data, samples);
}

int xz_client_send_text(xz_client_t client, const char *text)
{
    if (!client || !client->connected || !text) {
        return -1;
    }

    /* 发送文本消息 (使用 listen 消息格式) */
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return -1;
    }

    cJSON_AddStringToObject(root, "type", "listen");
    cJSON_AddBoolToObject(root, "enable", true);
    cJSON_AddStringToObject(root, "text", text);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!json_str) {
        return -1;
    }

    int ret = xz_ws_send_text(client->ws, json_str);
    free(json_str);

    return ret;
}

xz_opus_dec_t xz_client_get_opus_decoder(xz_client_t client)
{
    return client ? client->opus_decoder : NULL;
}

int xz_client_get_tts_sample_rate(xz_client_t client)
{
    return client ? client->server_sample_rate : 24000;
}

/**
 * @brief 发送 Ping 消息
 */
static int send_ping_message(xz_client_t client)
{
    if (!client || !client->connected) {
        return -1;
    }

    char ping_msg[128];
    int len = xz_msg_create_ping(ping_msg, sizeof(ping_msg));
    if (len <= 0) {
        return -1;
    }

    int ret = xz_ws_send_text(client->ws, ping_msg);
    if (ret == 0) {
        client->last_heartbeat_time = lisa_os_get_tick_ms();
        LISA_LOGD(TAG, "Ping sent");
    }

    return ret;
}

/**
 * @brief 心跳检查 (由定时器调用)
 */
static int heartbeat_check(void *arg)
{
    xz_client_t client = (xz_client_t)arg;
    if (!client || !client->connected) {
        return 0;
    }

    uint32_t now = lisa_os_get_tick_ms();
    if (now - client->last_heartbeat_time >= HEARTBEAT_INTERVAL_MS) {
        send_ping_message(client);
    }

    /* 重新安排下一次检查 */
    evs_handler_post_runnable_delay(heartbeat_check, arg, 5000);
    return 0;
}

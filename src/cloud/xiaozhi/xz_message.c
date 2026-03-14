/**
 * @file xz_message.c
 * @brief 小智云端 JSON 消息处理实现
 */

#define TAG "xz_msg"

#include "xz_message.h"
#include "lisa_log.h"
#include "lisa_time.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * @brief 创建 Hello 消息
 */
int xz_msg_create_hello(char *buf, size_t buf_len)
{
    if (!buf || buf_len == 0) {
        return -1;
    }

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return -1;
    }

    cJSON_AddStringToObject(root, "type", "hello");
    cJSON_AddNumberToObject(root, "version", 1);
    cJSON_AddStringToObject(root, "transport", "websocket");

    cJSON *audio_params = cJSON_CreateObject();
    cJSON_AddStringToObject(audio_params, "format", "opus");
    cJSON_AddNumberToObject(audio_params, "sample_rate", 16000);
    cJSON_AddNumberToObject(audio_params, "channels", 1);
    cJSON_AddNumberToObject(audio_params, "frame_duration", 60);
    cJSON_AddItemToObject(root, "audio_params", audio_params);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!json_str) {
        return -1;
    }

    size_t len = strlen(json_str);
    if (len >= buf_len) {
        free(json_str);
        return -1;
    }

    strcpy(buf, json_str);
    free(json_str);

    LISA_LOGI(TAG, "Created Hello message: %s", buf);
    return len;
}

/**
 * @brief 创建 Listen 消息
 */
int xz_msg_create_listen(char *buf, size_t buf_len, const char *session_id)
{
    if (!buf || buf_len == 0) {
        return -1;
    }

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return -1;
    }

    cJSON_AddStringToObject(root, "type", "listen");
    cJSON_AddStringToObject(root, "state", "start");
    cJSON_AddStringToObject(root, "mode", "manual");
    if (session_id) {
        cJSON_AddStringToObject(root, "session_id", session_id);
    }

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!json_str) {
        return -1;
    }

    size_t len = strlen(json_str);
    if (len >= buf_len) {
        free(json_str);
        return -1;
    }

    strcpy(buf, json_str);
    free(json_str);

    LISA_LOGI(TAG, "Created Listen message: %s", buf);
    return len;
}

/**
 * @brief 创建 Stop 消息
 */
int xz_msg_create_stop(char *buf, size_t buf_len, const char *session_id)
{
    if (!buf || buf_len == 0) {
        return -1;
    }

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return -1;
    }

    cJSON_AddStringToObject(root, "type", "listen");
    cJSON_AddStringToObject(root, "state", "stop");
    cJSON_AddStringToObject(root, "mode", "auto");
    if (session_id) {
        cJSON_AddStringToObject(root, "session_id", session_id);
    }

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!json_str) {
        return -1;
    }

    size_t len = strlen(json_str);
    if (len >= buf_len) {
        free(json_str);
        return -1;
    }

    strcpy(buf, json_str);
    free(json_str);

    LISA_LOGI(TAG, "Created Stop message: %s", buf);
    return len;
}

/**
 * @brief 创建 Ping 消息
 */
int xz_msg_create_ping(char *buf, size_t buf_len)
{
    if (!buf || buf_len == 0) {
        return -1;
    }

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return -1;
    }

    cJSON_AddStringToObject(root, "type", "ping");
    /* 使用时间戳作为 ping ID，便于追踪 */
    uint32_t tick_ms = lisa_os_get_tick_ms();
    cJSON_AddNumberToObject(root, "id", tick_ms);
    cJSON_AddNumberToObject(root, "timestamp", tick_ms);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!json_str) {
        return -1;
    }

    size_t len = strlen(json_str);
    if (len >= buf_len) {
        free(json_str);
        return -1;
    }

    strcpy(buf, json_str);
    free(json_str);

    LISA_LOGI(TAG, "Created Ping message: %s", buf);
    return len;
}

/**
 * @brief 创建 Pong 消息
 */
int xz_msg_create_pong(char *buf, size_t buf_len)
{
    if (!buf || buf_len == 0) {
        return -1;
    }

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return -1;
    }

    cJSON_AddStringToObject(root, "type", "pong");

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!json_str) {
        return -1;
    }

    size_t len = strlen(json_str);
    if (len >= buf_len) {
        free(json_str);
        return -1;
    }

    strcpy(buf, json_str);
    free(json_str);

    LISA_LOGD(TAG, "Created Pong message");
    return len;
}

/**
 * @brief 解析收到的消息
 */
xz_msg_type_e xz_msg_parse(const char *json_str, const xz_msg_callbacks_t *callbacks)
{
    if (!json_str || !callbacks) {
        return XZ_MSG_TYPE_UNKNOWN;
    }

    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        LISA_LOGE(TAG, "Failed to parse JSON: %s", json_str);
        return XZ_MSG_TYPE_UNKNOWN;
    }

    cJSON *type_item = cJSON_GetObjectItem(root, "type");
    if (!type_item || !cJSON_IsString(type_item)) {
        LISA_LOGE(TAG, "Missing 'type' field");
        cJSON_Delete(root);
        return XZ_MSG_TYPE_UNKNOWN;
    }

    const char *type = type_item->valuestring;
    xz_msg_type_e msg_type = XZ_MSG_TYPE_UNKNOWN;

    /* Hello 消息 */
    if (strcmp(type, "hello") == 0) {
        msg_type = XZ_MSG_TYPE_HELLO;
        cJSON *session_item = cJSON_GetObjectItem(root, "session_id");
        cJSON *audio_params_item = cJSON_GetObjectItem(root, "audio_params");
        const char *session_id = session_item && cJSON_IsString(session_item) ?
                                 session_item->valuestring : NULL;

        /* 打印完整的 Hello 响应 */
        char *response_str = cJSON_Print(root);
        LISA_LOGI(TAG, "Received Hello message from server: %s", response_str ? response_str : "null");
        if (response_str) {
            free(response_str);
        }

        /* 提取并打印服务器返回的音频参数 */
        xz_server_audio_params_t audio_params = {0};
        if (audio_params_item && cJSON_IsObject(audio_params_item)) {
            cJSON *fmt_item = cJSON_GetObjectItem(audio_params_item, "format");
            cJSON *sr_item = cJSON_GetObjectItem(audio_params_item, "sample_rate");
            cJSON *ch_item = cJSON_GetObjectItem(audio_params_item, "channels");
            cJSON *dur_item = cJSON_GetObjectItem(audio_params_item, "frame_duration");

            if (fmt_item && cJSON_IsString(fmt_item)) {
                strncpy(audio_params.format, fmt_item->valuestring, sizeof(audio_params.format) - 1);
            }
            if (sr_item && cJSON_IsNumber(sr_item)) {
                audio_params.sample_rate = sr_item->valueint;
            }
            if (ch_item && cJSON_IsNumber(ch_item)) {
                audio_params.channels = ch_item->valueint;
            }
            if (dur_item && cJSON_IsNumber(dur_item)) {
                audio_params.frame_duration = dur_item->valueint;
            }

            LISA_LOGI(TAG, "Server audio params: format=%s, sample_rate=%d, channels=%d, frame_duration=%d",
                      audio_params.format, audio_params.sample_rate, audio_params.channels, audio_params.frame_duration);
        }

        /* 使用默认值（如果没有提供） */
        if (audio_params.sample_rate == 0) {
            audio_params.sample_rate = 24000;  /* 默认 24kHz */
        }
        if (audio_params.channels == 0) {
            audio_params.channels = 1;
        }
        if (audio_params.frame_duration == 0) {
            audio_params.frame_duration = 60;
        }
        if (audio_params.format[0] == '\0') {
            strcpy(audio_params.format, "opus");
        }

        if (callbacks->on_hello) {
            callbacks->on_hello(session_id, &audio_params, callbacks->user_data);
        }
    }
    /* Pong 消息 */
    else if (strcmp(type, "pong") == 0) {
        msg_type = XZ_MSG_TYPE_PONG;
        cJSON *id_item = cJSON_GetObjectItem(root, "id");
        cJSON *ts_item = cJSON_GetObjectItem(root, "timestamp");
        if (id_item && cJSON_IsNumber(id_item)) {
            LISA_LOGI(TAG, "Received Pong message: id=%lld, timestamp=%lld",
                      (long long)cJSON_GetNumberValue(id_item),
                      ts_item && cJSON_IsNumber(ts_item) ? (long long)cJSON_GetNumberValue(ts_item) : 0);
        } else {
            LISA_LOGI(TAG, "Received Pong message");
        }
    }
    /* Ping 消息 - 服务器主动发送，需要回复 Pong */
    else if (strcmp(type, "ping") == 0) {
        msg_type = XZ_MSG_TYPE_PING;
        LISA_LOGI(TAG, "Received Ping message from server, should reply with Pong");

        /* 调用回调，让应用层发送 Pong 响应 */
        if (callbacks->on_ping) {
            callbacks->on_ping(callbacks->user_data);
        }
    }
    /* STT 消息 */
    else if (strcmp(type, "stt") == 0) {
        msg_type = XZ_MSG_TYPE_STT;
        cJSON *text_item = cJSON_GetObjectItem(root, "text");
        cJSON *is_final_item = cJSON_GetObjectItem(root, "is_final");

        if (text_item && cJSON_IsString(text_item)) {
            bool is_final = is_final_item && cJSON_IsBool(is_final_item) ?
                           cJSON_IsTrue(is_final_item) : false;
            LISA_LOGI(TAG, "STT: %s (final=%d)", text_item->valuestring, is_final);
            if (callbacks->on_stt) {
                callbacks->on_stt(text_item->valuestring, is_final, callbacks->user_data);
            }
        }
    }
    /* LLM 消息 */
    else if (strcmp(type, "llm") == 0) {
        msg_type = XZ_MSG_TYPE_LLM;
        cJSON *content_item = cJSON_GetObjectItem(root, "content");
        cJSON *is_end_item = cJSON_GetObjectItem(root, "is_end");

        if (content_item && cJSON_IsString(content_item)) {
            bool is_end = is_end_item && cJSON_IsBool(is_end_item) ?
                         cJSON_IsTrue(is_end_item) : false;
            LISA_LOGI(TAG, "LLM: %s (end=%d)", content_item->valuestring, is_end);
            if (callbacks->on_llm) {
                callbacks->on_llm(content_item->valuestring, is_end, callbacks->user_data);
            }
        }
    }
    /* TTS 消息 */
    else if (strcmp(type, "tts") == 0) {
        msg_type = XZ_MSG_TYPE_TTS;
        cJSON *state_item = cJSON_GetObjectItem(root, "state");

        if (state_item && cJSON_IsString(state_item)) {
            const char *state = state_item->valuestring;
            xz_tts_state_e tts_state;

            if (strcmp(state, "start") == 0) {
                tts_state = XZ_TTS_STATE_START;
                LISA_LOGI(TAG, "TTS: start");
            } else if (strcmp(state, "end") == 0) {
                tts_state = XZ_TTS_STATE_END;
                LISA_LOGI(TAG, "TTS: end");
            } else {
                tts_state = XZ_TTS_STATE_DATA;
                LISA_LOGD(TAG, "TTS: data");
            }

            /* TTS 数据在二进制帧中，这里只处理状态 */
            if (callbacks->on_tts) {
                callbacks->on_tts(tts_state, NULL, 0, callbacks->user_data);
            }
        }
    }
    /* IoT 消息 */
    else if (strcmp(type, "iot") == 0) {
        msg_type = XZ_MSG_TYPE_IOT;
        cJSON *command_item = cJSON_GetObjectItem(root, "command");
        cJSON *param_item = cJSON_GetObjectItem(root, "param");

        const char *command = command_item && cJSON_IsString(command_item) ?
                             command_item->valuestring : NULL;
        const char *param = param_item && cJSON_IsString(param_item) ?
                           param_item->valuestring : NULL;

        LISA_LOGI(TAG, "IoT: command=%s, param=%s",
                  command ? command : "null",
                  param ? param : "null");

        if (callbacks->on_iot) {
            callbacks->on_iot(command, param, callbacks->user_data);
        }
    }
    /* Error 消息 */
    else if (strcmp(type, "error") == 0) {
        msg_type = XZ_MSG_TYPE_ERROR;
        cJSON *code_item = cJSON_GetObjectItem(root, "code");
        cJSON *msg_item = cJSON_GetObjectItem(root, "message");

        int code = code_item && cJSON_IsNumber(code_item) ? code_item->valueint : -1;
        const char *msg = msg_item && cJSON_IsString(msg_item) ? msg_item->valuestring : "Unknown error";

        LISA_LOGE(TAG, "Error: code=%d, message=%s", code, msg);

        if (callbacks->on_error) {
            callbacks->on_error(code, msg, callbacks->user_data);
        }
    }
    /* 未知消息 */
    else {
        LISA_LOGW(TAG, "Unknown message type: %s", type);
        msg_type = XZ_MSG_TYPE_UNKNOWN;
    }

    cJSON_Delete(root);
    return msg_type;
}

/**
 * @brief 获取消息类型字符串
 */
const char *xz_msg_type_to_string(xz_msg_type_e type)
{
    switch (type) {
        case XZ_MSG_TYPE_HELLO:  return "hello";
        case XZ_MSG_TYPE_PING:   return "ping";
        case XZ_MSG_TYPE_PONG:   return "pong";
        case XZ_MSG_TYPE_STT:    return "stt";
        case XZ_MSG_TYPE_TTS:    return "tts";
        case XZ_MSG_TYPE_LLM:    return "llm";
        case XZ_MSG_TYPE_IOT:    return "iot";
        case XZ_MSG_TYPE_ERROR:  return "error";
        default:                 return "unknown";
    }
}

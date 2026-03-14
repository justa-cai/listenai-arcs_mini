/**
 * @file xz_message.h
 * @brief 小智云端 JSON 消息处理
 */

#ifndef __XZ_MESSAGE_H__
#define __XZ_MESSAGE_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 消息类型 */
typedef enum {
    XZ_MSG_TYPE_HELLO = 0,     /**< 握手消息 */
    XZ_MSG_TYPE_PING,          /**< Ping 消息 */
    XZ_MSG_TYPE_PONG,          /**< Pong 消息 */
    XZ_MSG_TYPE_STT,           /**< 语音识别结果 */
    XZ_MSG_TYPE_TTS,           /**< 语音合成 */
    XZ_MSG_TYPE_LLM,           /**< 大语言模型响应 */
    XZ_MSG_TYPE_IOT,           /**< IoT 控制指令 */
    XZ_MSG_TYPE_ERROR,         /**< 错误消息 */
    XZ_MSG_TYPE_UNKNOWN,       /**< 未知类型 */
} xz_msg_type_e;

/** TTS 状态 */
typedef enum {
    XZ_TTS_STATE_START = 0,    /**< TTS 开始 */
    XZ_TTS_STATE_DATA,         /**< TTS 数据 */
    XZ_TTS_STATE_END,          /**< TTS 结束 */
} xz_tts_state_e;

/** 服务器音频参数 */
typedef struct {
    char format[32];      /**< 音频格式 (opus, pcm) */
    int sample_rate;      /**< 采样率 (Hz) */
    int channels;         /**< 声道数 */
    int frame_duration;   /**< 帧时长 (ms) */
} xz_server_audio_params_t;

/** 消息回调 */
typedef struct {
    void (*on_hello)(const char *session_id, const xz_server_audio_params_t *audio_params, void *user);
    void (*on_ping)(void *user);            /**< 服务器 Ping 消息回调，应自动回复 Pong */
    void (*on_stt)(const char *text, bool is_final, void *user);
    void (*on_llm)(const char *content, bool is_end, void *user);
    void (*on_tts)(xz_tts_state_e state, const uint8_t *data, uint32_t len, void *user);
    void (*on_iot)(const char *command, const char *param, void *user);
    void (*on_error)(int code, const char *message, void *user);
    void *user_data;
} xz_msg_callbacks_t;

/**
 * @brief 创建 Hello 消息
 * @param buf 输出缓冲区
 * @param buf_len 缓冲区大小
 * @return 消息长度，-1 表示失败
 */
int xz_msg_create_hello(char *buf, size_t buf_len);

/**
 * @brief 创建 Listen 消息 (开始音频上传)
 * @param buf 输出缓冲区
 * @param buf_len 缓冲区大小
 * @param session_id 会话 ID (从 Hello 响应获取)
 * @return 消息长度，-1 表示失败
 */
int xz_msg_create_listen(char *buf, size_t buf_len, const char *session_id);

/**
 * @brief 创建 Stop 消息 (停止音频上传)
 * @param buf 输出缓冲区
 * @param buf_len 缓冲区大小
 * @param session_id 会话 ID (从 Hello 响应获取)
 * @return 消息长度，-1 表示失败
 */
int xz_msg_create_stop(char *buf, size_t buf_len, const char *session_id);

/**
 * @brief 创建 Ping 消息
 * @param buf 输出缓冲区
 * @param buf_len 缓冲区大小
 * @return 消息长度，-1 表示失败
 */
int xz_msg_create_ping(char *buf, size_t buf_len);

/**
 * @brief 创建 Pong 消息
 * @param buf 输出缓冲区
 * @param buf_len 缓冲区大小
 * @return 消息长度，-1 表示失败
 */
int xz_msg_create_pong(char *buf, size_t buf_len);

/**
 * @brief 解析收到的消息
 * @param json_str JSON 字符串
 * @param callbacks 回调函数
 * @return 消息类型
 */
xz_msg_type_e xz_msg_parse(const char *json_str, const xz_msg_callbacks_t *callbacks);

/**
 * @brief 获取消息类型字符串
 * @param type 消息类型
 * @return 类型字符串
 */
const char *xz_msg_type_to_string(xz_msg_type_e type);

#ifdef __cplusplus
}
#endif

#endif /* __XZ_MESSAGE_H__ */

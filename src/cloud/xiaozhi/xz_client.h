/**
 * @file xz_client.h
 * @brief 小智云端主客户端
 * @note 整合所有模块，提供完整的语音助手功能
 */

#ifndef __XZ_CLIENT_H__
#define __XZ_CLIENT_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 小智客户端句柄 */
typedef struct xz_client_s *xz_client_t;

/** 前向声明 - Opus 解码器 */
typedef struct xz_opus_dec_s *xz_opus_dec_t;

/** 客户端事件类型 */
typedef enum {
    XZ_CLIENT_EVENT_CONNECTED = 0,     /**< 连接成功 */
    XZ_CLIENT_EVENT_DISCONNECTED,      /**< 连接断开 */
    XZ_CLIENT_EVENT_ERROR,             /**< 发生错误 */
    XZ_CLIENT_EVENT_STT_TEXT,          /**< 识别到文本 */
    XZ_CLIENT_EVENT_LLM_CONTENT,       /**< LLM 响应内容 */
    XZ_CLIENT_EVENT_LLM_EMOJI,         /**< LLM 表情更新 */
    XZ_CLIENT_EVENT_TTS_START,         /**< TTS 开始 */
    XZ_CLIENT_EVENT_TTS_DATA,          /**< TTS 音频数据 */
    XZ_CLIENT_EVENT_TTS_END,           /**< TTS 结束 */
    XZ_CLIENT_EVENT_IOT_COMMAND,       /**< IoT 控制指令 */
} xz_client_event_e;

/** 客户端回调 */
typedef struct {
    void (*on_connected)(void *user);
    void (*on_disconnected)(void *user);
    void (*on_error)(int code, const char *message, void *user);
    void (*on_stt_text)(const char *text, bool is_final, void *user);
    void (*on_llm_content)(const char *content, bool is_end, void *user);
    void (*on_llm_emoji)(const char *emoji_name, void *user);
    void (*on_tts_start)(void *user);
    void (*on_tts_text)(const char *text, void *user);    /**< TTS 文本回调 */
    void (*on_tts_data)(const uint8_t *data, uint32_t len, void *user);
    void (*on_tts_end)(void *user);
    void (*on_iot_command)(const char *command, const char *param, void *user);
    void *user_data;
} xz_client_callbacks_t;

/** 客户端配置 */
typedef struct {
    const char *url;            /**< WebSocket URL */
    const char *token;          /**< Authorization token */
    const char *device_id;      /**< 设备 ID */
    const char *client_id;      /**< 客户端 ID */
    uint32_t timeout_ms;        /**< 连接超时 (毫秒) */
} xz_client_config_t;

/**
 * @brief 创建小智客户端
 * @param config 客户端配置
 * @return 客户端句柄，失败返回 NULL
 */
xz_client_t xz_client_create(const xz_client_config_t *config);

/**
 * @brief 销毁小智客户端
 * @param client 客户端句柄
 */
void xz_client_destroy(xz_client_t client);

/**
 * @brief 设置回调函数
 * @param client 客户端句柄
 * @param callbacks 回调函数
 */
void xz_client_set_callbacks(xz_client_t client, const xz_client_callbacks_t *callbacks);

/**
 * @brief 连接到服务器
 * @param client 客户端句柄
 * @return 0 成功, -1 失败
 */
int xz_client_connect(xz_client_t client);

/**
 * @brief 断开连接
 * @param client 客户端句柄
 * @return 0 成功, -1 失败
 */
int xz_client_disconnect(xz_client_t client);

/**
 * @brief 检查连接状态
 * @param client 客户端句柄
 * @return true 已连接, false 未连接
 */
bool xz_client_is_connected(xz_client_t client);

/**
 * @brief 开始语音交互
 * @param client 客户端句柄
 * @return 0 成功, -1 失败
 */
int xz_client_start_interaction(xz_client_t client);

/**
 * @brief 停止语音交互
 * @param client 客户端句柄
 * @return 0 成功, -1 失败
 */
int xz_client_stop_interaction(xz_client_t client);

/**
 * @brief 写入 PCM 音频数据
 * @param client 客户端句柄
 * @param pcm_data PCM 数据 (int16_t)
 * @param samples 采样点数
 * @return 0 成功, -1 失败
 */
int xz_client_write_audio(xz_client_t client, const int16_t *pcm_data, int samples);

/**
 * @brief 发送文本消息
 * @param client 客户端句柄
 * @param text 文本内容
 * @return 0 成功, -1 失败
 */
int xz_client_send_text(xz_client_t client, const char *text);

/**
 * @brief 获取客户端的 Opus 解码器
 * @param client 客户端句柄
 * @return Opus 解码器句柄，可能返回 NULL
 */
xz_opus_dec_t xz_client_get_opus_decoder(xz_client_t client);

/**
 * @brief 获取服务器配置的 TTS 采样率
 * @param client 客户端句柄
 * @return 采样率 (Hz)
 */
int xz_client_get_tts_sample_rate(xz_client_t client);

#ifdef __cplusplus
}
#endif

#endif /* __XZ_CLIENT_H__ */

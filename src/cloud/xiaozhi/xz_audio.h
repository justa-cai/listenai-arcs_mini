/**
 * @file xz_audio.h
 * @brief 小智云端音频发送器
 * @note 负责从音频流获取数据、Opus 编码并发送
 */

#ifndef __XZ_AUDIO_H__
#define __XZ_AUDIO_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 音频发送器句柄 */
typedef struct xz_audio_s *xz_audio_t;

/** 音频配置 */
typedef struct {
    int sample_rate;        /**< 采样率 (Hz): 16000 */
    int channels;           /**< 声道数: 1 */
    int frame_duration_ms;  /**< 帧时长 (ms): 60 */
    int bitrate;            /**< Opus 比特率 (bps): 24000 */
} xz_audio_config_t;

/**
 * @brief 创建音频发送器
 * @param config 音频配置
 * @return 音频发送器句柄，失败返回 NULL
 */
xz_audio_t xz_audio_create(const xz_audio_config_t *config);

/**
 * @brief 销毁音频发送器
 * @param audio 音频发送器句柄
 */
void xz_audio_destroy(xz_audio_t audio);

/**
 * @brief 开始音频上传
 * @param audio 音频发送器句柄
 * @return 0 成功, -1 失败
 */
int xz_audio_start(xz_audio_t audio);

/**
 * @brief 停止音频上传
 * @param audio 音频发送器句柄
 * @return 0 成功, -1 失败
 */
int xz_audio_stop(xz_audio_t audio);

/**
 * @brief 写入 PCM 音频数据
 * @param audio 音频发送器句柄
 * @param pcm_data PCM 数据 (int16_t)
 * @param samples 采样点数
 * @return 0 成功, -1 失败
 */
int xz_audio_write(xz_audio_t audio, const int16_t *pcm_data, int samples);

/**
 * @brief 检查是否正在上传
 * @param audio 音频发送器句柄
 * @return true 正在上传, false 未上传
 */
bool xz_audio_is_sending(xz_audio_t audio);

/**
 * @brief 设置 WebSocket 发送回调
 * @param audio 音频发送器句柄
 * @param send_cb 发送回调函数
 * @param user_data 用户数据
 */
typedef int (*xz_audio_send_cb_t)(const void *data, uint32_t len, void *user_data);
void xz_audio_set_send_callback(xz_audio_t audio, xz_audio_send_cb_t send_cb, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* __XZ_AUDIO_H__ */

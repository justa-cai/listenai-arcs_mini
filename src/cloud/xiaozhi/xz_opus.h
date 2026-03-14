/**
 * @file xz_opus.h
 * @brief 小智云端 Opus 编码器封装
 * @note 基于 arcs-sdk/modules/opusdec/opus_encoder.h
 */

#ifndef __XZ_OPUS_H__
#define __XZ_OPUS_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Opus 编码器句柄 */
typedef struct xz_opus_s *xz_opus_t;

/** 编码配置 */
typedef struct {
    int sample_rate;    /**< 采样率 (Hz): 16000 */
    int channels;       /**< 声道数: 1 (单声道) */
    int frame_size;     /**< 帧大小 (采样数): 960 (60ms @ 16kHz) */
    int bitrate;        /**< 比特率 (bps): 24000 */
    int complexity;     /**< 复杂度: 0-10 (0 = 最低 CPU 占用) */
} xz_opus_config_t;

/**
 * @brief 创建 Opus 编码器
 * @param config 编码器配置
 * @return 编码器句柄，失败返回 NULL
 */
xz_opus_t xz_opus_create(const xz_opus_config_t *config);

/**
 * @brief 销毁 Opus 编码器
 * @param encoder 编码器句柄
 */
void xz_opus_destroy(xz_opus_t encoder);

/**
 * @brief 编码 PCM 数据为 Opus
 * @param encoder 编码器句柄
 * @param pcm_data 输入的 PCM 数据 (int16_t)
 * @param frame_size 帧大小 (采样数)
 * @param opus_data 输出的 Opus 数据缓冲区
 * @param opus_size 输入时为缓冲区大小(字节)，输出时为实际编码长度
 * @return 0 成功, -1 失败
 */
int xz_opus_encode(xz_opus_t encoder,
                   const int16_t *pcm_data,
                   int frame_size,
                   uint8_t *opus_data,
                   int *opus_size);

/**
 * @brief 重置编码器状态
 * @param encoder 编码器句柄
 */
void xz_opus_reset(xz_opus_t encoder);

/**
 * @brief 获取帧大小
 * @param encoder 编码器句柄
 * @return 帧大小 (采样数)
 */
int xz_opus_get_frame_size(xz_opus_t encoder);

/**
 * @brief 获取采样率
 * @param encoder 编码器句柄
 * @return 采样率 (Hz)
 */
int xz_opus_get_sample_rate(xz_opus_t encoder);

#ifdef __cplusplus
}
#endif

#endif /* __XZ_OPUS_H__ */

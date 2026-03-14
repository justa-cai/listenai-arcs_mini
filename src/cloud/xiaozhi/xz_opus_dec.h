/**
 * @file xz_opus_dec.h
 * @brief 小智云端 Opus 解码器封装
 * @note 基于 arcs-sdk/modules/opusdec/opus_wrapper.h
 */

#ifndef __XZ_OPUS_DEC_H__
#define __XZ_OPUS_DEC_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Opus 解码器句柄 */
typedef struct xz_opus_dec_s *xz_opus_dec_t;

/** Opus 解码器配置 */
typedef struct {
    int sample_rate;    /**< 采样率 (Hz): 8000, 12000, 16000, 24000, 48000 */
    int channels;       /**< 声道数: 1 (单声道) 或 2 (立体声) */
    int frame_size;     /**< 帧大小 (采样数): 通常为 sample_rate * 0.06 (60ms) */
} xz_opus_dec_config_t;

/** Opus 解码结果 */
typedef enum {
    XZ_OPUS_DECODE_OK = 0,         /**< 解码成功 */
    XZ_OPUS_DECODE_ERR = -1,       /**< 解码错误 */
    XZ_OPUS_DECODE_ERR_CORRUPT = -2,  /**< 数据损坏 */
    XZ_OPUS_DECODE_ERR_INVALID = -3,  /**< 无效参数 */
} xz_opus_dec_result_e;

/**
 * @brief 创建 Opus 解码器
 * @param config 解码器配置
 * @return 解码器句柄，失败返回 NULL
 */
xz_opus_dec_t xz_opus_dec_create(const xz_opus_dec_config_t *config);

/**
 * @brief 销毁 Opus 解码器
 * @param decoder 解码器句柄
 */
void xz_opus_dec_destroy(xz_opus_dec_t decoder);

/**
 * @brief 解码 Opus 数据
 * @param decoder 解码器句柄
 * @param in_data 输入的 Opus 编码数据
 * @param in_len 输入数据长度 (字节)
 * @param out_data 输出的 PCM 数据缓冲区 (int16_t)
 * @param out_len 输入时为缓冲区大小(采样数)，输出时为实际解码采样数
 * @return 解码结果
 * @note 输出数据为 16 位 PCM 格式
 */
xz_opus_dec_result_e xz_opus_dec_decode(xz_opus_dec_t decoder,
                                         const uint8_t *in_data,
                                         size_t in_len,
                                         int16_t *out_data,
                                         int *out_len);

/**
 * @brief 重置解码器状态
 * @param decoder 解码器句柄
 */
void xz_opus_dec_reset(xz_opus_dec_t decoder);

/**
 * @brief 获取解码器采样率
 * @param decoder 解码器句柄
 * @return 采样率 (Hz)
 */
int xz_opus_dec_get_sample_rate(xz_opus_dec_t decoder);

/**
 * @brief 获取解码器声道数
 * @param decoder 解码器句柄
 * @return 声道数
 */
int xz_opus_dec_get_channels(xz_opus_dec_t decoder);

/**
 * @brief 获取解码器帧大小
 * @param decoder 解码器句柄
 * @return 帧大小 (采样数)
 */
int xz_opus_dec_get_frame_size(xz_opus_dec_t decoder);

/**
 * @brief 计算输出缓冲区大小
 * @param config 解码器配置
 * @param frame_count 帧数量
 * @return 需要的缓冲区大小 (采样数)
 */
int xz_opus_dec_calc_pcm_size(const xz_opus_dec_config_t *config, int frame_count);

#ifdef __cplusplus
}
#endif

#endif /* __XZ_OPUS_DEC_H__ */

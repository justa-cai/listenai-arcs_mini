/**
 * @file opus_wrapper.h
 * @brief Opus 解码器简化封装
 * @note 提供简化的 API 用于 Opus 音频解码
 */

#ifndef __OPUS_WRAPPER_H__
#define __OPUS_WRAPPER_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Opus 解码器句柄 */
typedef struct opus_decoder_s *opus_dec_t;

/** Opus 解码配置 */
typedef struct {
    int sample_rate;    /**< 采样率 (Hz): 8000, 12000, 16000, 24000, 48000 */
    int channels;       /**< 声道数: 1 (单声道) 或 2 (立体声) */
    int frame_size;     /**< 帧大小 (采样数): 通常为 sample_rate/50 (20ms) */
    int application;    /**< 应用类型: 2048 (VOIP), 2049 (音频), 2051 (低延迟) */
} opus_decoder_config_t;

/** Opus 解码结果 */
typedef enum {
    OPUS_DECODE_OK = 0,        /**< 解码成功 */
    OPUS_DECODE_ERR = -1,      /**< 解码错误 */
    OPUS_DECODE_ERR_CORRUPT = -2,  /**< 数据损坏 */
    OPUS_DECODE_ERR_INVALID = -3,  /**< 无效参数 */
} opus_decode_result_t;

/**
 * @brief 创建 Opus 解码器
 * @param config 解码器配置
 * @return 解码器句柄，失败返回 NULL
 */
opus_dec_t opus_dec_create(const opus_decoder_config_t *config);

/**
 * @brief 销毁 Opus 解码器
 * @param decoder 解码器句柄
 */
void opus_dec_destroy(opus_dec_t decoder);

/**
 * @brief 解码 Opus 数据
 * @param decoder 解码器句柄
 * @param in_data 输入的 Opus 编码数据
 * @param in_len 输入数据长度 (字节)
 * @param out_data 输出的 PCM 数据缓冲区
 * @param out_len 输出缓冲区大小 (字节)
 * @return 解码结果
 * @note 输出数据为 16 位 PCM 格式
 */
opus_decode_result_t opus_dec_decode(opus_dec_t decoder,
                                      const uint8_t *in_data,
                                      size_t in_len,
                                      int16_t *out_data,
                                      size_t *out_len);

/**
 * @brief 重置解码器状态
 * @param decoder 解码器句柄
 */
void opus_dec_reset(opus_dec_t decoder);

/**
 * @brief 获取解码器采样率
 * @param decoder 解码器句柄
 * @return 采样率 (Hz)
 */
int opus_dec_get_sample_rate(opus_dec_t decoder);

/**
 * @brief 获取解码器声道数
 * @param decoder 解码器句柄
 * @return 声道数
 */
int opus_dec_get_channels(opus_dec_t decoder);

/**
 * @brief 获取解码器帧大小
 * @param decoder 解码器句柄
 * @return 帧大小 (采样数)
 */
int opus_dec_get_frame_size(opus_dec_t decoder);

/**
 * @brief 计算 PCM 输出缓冲区大小
 * @param config 解码器配置
 * @param frame_count 帧数量
 * @return 需要的缓冲区大小 (字节)
 */
size_t opus_dec_calc_pcm_size(const opus_decoder_config_t *config, int frame_count);

/**
 * @brief 获取 Opus 库版本
 * @return 版本字符串
 */
const char *opus_dec_get_version(void);

#ifdef __cplusplus
}
#endif

#endif /* __OPUS_WRAPPER_H__ */

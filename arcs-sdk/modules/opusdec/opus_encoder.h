/**
 * @file opus_encoder.h
 * @brief Opus 编码器简化封装
 * @note 提供简化的 API 用于 Opus 音频编码
 */

#ifndef __OPUS_ENCODER_H__
#define __OPUS_ENCODER_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Opus 编码器句柄 */
typedef struct opus_encoder_s *opus_enc_t;

/** Opus 编码配置 */
typedef struct {
    int sample_rate;    /**< 采样率 (Hz): 8000, 12000, 16000, 24000, 48000 */
    int channels;       /**< 声道数: 1 (单声道) 或 2 (立体声) */
    int frame_size;     /**< 帧大小 (采样数): 通常为 sample_rate/50 (20ms) */
    int application;    /**< 应用类型: 2048 (VOIP), 2049 (音频), 2051 (低延迟) */
    int bitrate;        /**< 比特率 (bps): 6000-510000 */
    int complexity;     /**< 复杂度: 0-10 (越高质量越好但越慢) */
    bool vbr;           /**< 可变比特率 */
} opus_encoder_config_t;

/** Opus 编码结果 */
typedef enum {
    OPUS_ENCODE_OK = 0,        /**< 编码成功 */
    OPUS_ENCODE_ERR = -1,      /**< 编码错误 */
    OPUS_ENCODE_ERR_INVALID = -3,  /**< 无效参数 */
} opus_encode_result_t;

/**
 * @brief 创建 Opus 编码器
 * @param config 编码器配置
 * @return 编码器句柄，失败返回 NULL
 */
opus_enc_t opus_enc_create(const opus_encoder_config_t *config);

/**
 * @brief 销毁 Opus 编码器
 * @param encoder 编码器句柄
 */
void opus_enc_destroy(opus_enc_t encoder);

/**
 * @brief 编码 PCM 数据为 Opus
 * @param encoder 编码器句柄
 * @param in_data 输入的 PCM 数据 (int16_t)
 * @param in_samples 输入采样点数
 * @param out_data 输出的 Opus 数据缓冲区
 * @param out_len 输入时为缓冲区大小(字节)，输出时为实际编码长度
 * @return 编码结果
 */
opus_encode_result_t opus_enc_encode(opus_enc_t encoder,
                                          const int16_t *in_data,
                                          int in_samples,
                                          uint8_t *out_data,
                                          size_t *out_len);

/**
 * @brief 重置编码器状态
 * @param encoder 编码器句柄
 */
void opus_enc_reset(opus_enc_t encoder);

/**
 * @brief 设置比特率
 * @param encoder 编码器句柄
 * @param bitrate 比特率 (bps)
 * @return 0 成功, -1 失败
 */
int opus_enc_set_bitrate(opus_enc_t encoder, int bitrate);

/**
 * @brief 获取编码器采样率
 * @param encoder 编码器句柄
 * @return 采样率 (Hz)
 */
int opus_enc_get_sample_rate(opus_enc_t encoder);

/**
 * @brief 获取编码器声道数
 * @param encoder 编码器句柄
 * @return 声道数
 */
int opus_enc_get_channels(opus_enc_t encoder);

/**
 * @brief 获取编码器帧大小
 * @param encoder 编码器句柄
 * @return 帧大小 (采样数)
 */
int opus_enc_get_frame_size(opus_enc_t encoder);

/**
 * @brief 计算 Opus 输出缓冲区大小
 * @param config 编码器配置
 * @param frame_count 帧数量
 * @return 需要的缓冲区大小 (字节)
 */
size_t opus_enc_calc_packet_size(const opus_encoder_config_t *config, int frame_count);

/**
 * @brief 获取 Opus 库版本
 * @return 版本字符串
 */
const char *opus_enc_get_version(void);

#ifdef __cplusplus
}
#endif

#endif /* __OPUS_ENCODER_H__ */

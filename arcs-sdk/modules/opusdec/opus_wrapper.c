/**
 * @file opus_wrapper.c
 * @brief Opus 解码器简化封装实现
 */

#include <stdlib.h>
#include <string.h>

#ifndef TAG
#define TAG "opus_wrapper"
#endif

/* 先包含封装头文件以获取类型定义 */
#include "opus_wrapper.h"

/* 再包含 libopus 头文件 */
#include "opus/opus.h"

/* Opus 解码器上下文 */
struct opus_decoder_s {
    OpusDecoder *decoder;      /**< libopus 解码器句柄 */
    int sample_rate;           /**< 采样率 */
    int channels;              /**< 声道数 */
    int frame_size;            /**< 帧大小 */
    opus_decoder_config_t config; /**< 配置 */
};

/* 默认配置 */
#define OPUS_DEFAULT_SAMPLE_RATE   48000
#define OPUS_DEFAULT_CHANNELS      1
#define OPUS_DEFAULT_FRAME_SIZE    960     /* 48kHz / 50 = 960 samples (20ms) */
#define OPUS_DEFAULT_APPLICATION   OPUS_APPLICATION_AUDIO
#define OPUS_MAX_PACKET_SIZE       4000    /**< 最大 Opus 包大小 */

opus_dec_t opus_dec_create(const opus_decoder_config_t *config)
{
    struct opus_decoder_s *dec = NULL;
    int opus_err;

    if (!config) {
        return NULL;
    }

    /* 分配解码器结构 */
    dec = (struct opus_decoder_s *)calloc(1, sizeof(struct opus_decoder_s));
    if (!dec) {
        return NULL;
    }

    /* 保存配置 */
    dec->config = *config;
    dec->sample_rate = (config->sample_rate > 0) ? config->sample_rate : OPUS_DEFAULT_SAMPLE_RATE;
    dec->channels = (config->channels > 0) ? config->channels : OPUS_DEFAULT_CHANNELS;
    dec->frame_size = (config->frame_size > 0) ? config->frame_size : (dec->sample_rate / 50);

    /* 创建 Opus 解码器 */
    dec->decoder = opus_decoder_create(dec->sample_rate, dec->channels, &opus_err);
    if (!dec->decoder || opus_err != OPUS_OK) {
        free(dec);
        return NULL;
    }

    return dec;
}

void opus_dec_destroy(opus_dec_t decoder)
{
    if (decoder) {
        if (decoder->decoder) {
            opus_decoder_destroy(decoder->decoder);
        }
        free(decoder);
    }
}

opus_decode_result_t opus_dec_decode(opus_dec_t decoder,
                                          const uint8_t *in_data,
                                          size_t in_len,
                                          int16_t *out_data,
                                          size_t *out_len)
{
    int decoded_samples;
    int max_samples;

    if (!decoder || !in_data || !out_data || !out_len) {
        return OPUS_DECODE_ERR_INVALID;
    }

    /* 计算最大输出采样数 */
    max_samples = decoder->frame_size * decoder->channels;
    if (*out_len < (size_t)(max_samples * sizeof(int16_t))) {
        return OPUS_DECODE_ERR_INVALID;
    }

    /* 解码 */
    decoded_samples = opus_decode(decoder->decoder,
                                   in_data,
                                   (int32_t)in_len,
                                   out_data,
                                   decoder->frame_size,
                                   0);  /* 不使用 FEC */

    /* 处理解码结果 */
    if (decoded_samples < 0) {
        *out_len = 0;
        switch (decoded_samples) {
            case OPUS_INVALID_PACKET:
                return OPUS_DECODE_ERR_CORRUPT;
            case OPUS_BUFFER_TOO_SMALL:
            case OPUS_BAD_ARG:
                return OPUS_DECODE_ERR_INVALID;
            default:
                return OPUS_DECODE_ERR;
        }
    }

    /* 更新输出长度 */
    *out_len = (size_t)(decoded_samples * decoder->channels * sizeof(int16_t));

    return OPUS_DECODE_OK;
}

void opus_dec_reset(opus_dec_t decoder)
{
    if (decoder && decoder->decoder) {
        opus_decoder_ctl(decoder->decoder, OPUS_RESET_STATE);
    }
}

int opus_dec_get_sample_rate(opus_dec_t decoder)
{
    return decoder ? decoder->sample_rate : 0;
}

int opus_dec_get_channels(opus_dec_t decoder)
{
    return decoder ? decoder->channels : 0;
}

int opus_dec_get_frame_size(opus_dec_t decoder)
{
    return decoder ? decoder->frame_size : 0;
}

size_t opus_dec_calc_pcm_size(const opus_decoder_config_t *config, int frame_count)
{
    int sample_rate = config ? config->sample_rate : OPUS_DEFAULT_SAMPLE_RATE;
    int channels = config ? config->channels : OPUS_DEFAULT_CHANNELS;
    int frame_size = (config && config->frame_size > 0) ? config->frame_size : (sample_rate / 50);

    return (size_t)(frame_size * channels * sizeof(int16_t) * frame_count);
}

const char *opus_dec_get_version(void)
{
    return opus_get_version_string();
}

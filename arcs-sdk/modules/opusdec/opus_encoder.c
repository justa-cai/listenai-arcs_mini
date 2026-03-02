/**
 * @file opus_encoder.c
 * @brief Opus 编码器简化封装实现
 */

#include <stdlib.h>
#include <string.h>

#ifndef TAG
#define TAG "opus_encoder"
#endif

/* 先包含封装头文件以获取类型定义 */
#include "opus_encoder.h"

/* 再包含 libopus 头文件 */
#include "opus/opus.h"

/* Opus 编码器上下文 */
struct opus_encoder_s {
    OpusEncoder *encoder;      /**< libopus 编码器句柄 */
    int sample_rate;           /**< 采样率 */
    int channels;              /**< 声道数 */
    int frame_size;            /**< 帧大小 */
    opus_encoder_config_t config; /**< 配置 */
};

/* 默认配置 */
#define OPUS_DEFAULT_SAMPLE_RATE   16000
#define OPUS_DEFAULT_CHANNELS      1
#define OPUS_DEFAULT_FRAME_SIZE    320     /* 16kHz / 50 = 320 samples (20ms) */
#define OPUS_DEFAULT_APPLICATION   OPUS_APPLICATION_VOIP
#define OPUS_DEFAULT_BITRATE       24000   /* 24 kbps */
#define OPUS_DEFAULT_COMPLEXITY    5
#define OPUS_MAX_PACKET_SIZE       4000    /**< 最大 Opus 包大小 */

opus_enc_t opus_enc_create(const opus_encoder_config_t *config)
{
    struct opus_encoder_s *enc = NULL;
    int opus_err;

    if (!config) {
        return NULL;
    }

    /* 分配编码器结构 */
    enc = (struct opus_encoder_s *)calloc(1, sizeof(struct opus_encoder_s));
    if (!enc) {
        return NULL;
    }

    /* 保存配置 */
    enc->config = *config;
    enc->sample_rate = (config->sample_rate > 0) ? config->sample_rate : OPUS_DEFAULT_SAMPLE_RATE;
    enc->channels = (config->channels > 0) ? config->channels : OPUS_DEFAULT_CHANNELS;
    enc->frame_size = (config->frame_size > 0) ? config->frame_size : (enc->sample_rate / 50);

    /* 创建 Opus 编码器 */
    enc->encoder = opus_encoder_create(
        enc->sample_rate,
        enc->channels,
        (config->application > 0) ? config->application : OPUS_DEFAULT_APPLICATION,
        &opus_err
    );

    if (!enc->encoder || opus_err != OPUS_OK) {
        free(enc);
        return NULL;
    }

    /* 设置比特率 */
    int bitrate = (config->bitrate > 0) ? config->bitrate : OPUS_DEFAULT_BITRATE;
    opus_encoder_ctl(enc->encoder, OPUS_SET_BITRATE(bitrate));

    /* 设置复杂度 */
    int complexity = (config->complexity >= 0 && config->complexity <= 10) ?
                     config->complexity : OPUS_DEFAULT_COMPLEXITY;
    opus_encoder_ctl(enc->encoder, OPUS_SET_COMPLEXITY(complexity));

    /* 设置 VBR */
    int vbr = config->vbr ? 1 : 0;
    opus_encoder_ctl(enc->encoder, OPUS_SET_VBR(vbr));

    return enc;
}

void opus_enc_destroy(opus_enc_t encoder)
{
    if (encoder) {
        if (encoder->encoder) {
            opus_encoder_destroy(encoder->encoder);
        }
        free(encoder);
    }
}

opus_encode_result_t opus_enc_encode(opus_enc_t encoder,
                                          const int16_t *in_data,
                                          int in_samples,
                                          uint8_t *out_data,
                                          size_t *out_len)
{
    int encoded_bytes;

    if (!encoder || !in_data || !out_data || !out_len) {
        return OPUS_ENCODE_ERR_INVALID;
    }

    /* 确保输入采样数与帧大小匹配 */
    if (in_samples != encoder->frame_size) {
        return OPUS_ENCODE_ERR_INVALID;
    }

    /* 编码 */
    encoded_bytes = opus_encode(
        encoder->encoder,
        in_data,
        encoder->frame_size,
        out_data,
        *out_len
    );

    /* 处理编码结果 */
    if (encoded_bytes < 0) {
        *out_len = 0;
        switch (encoded_bytes) {
            case OPUS_BUFFER_TOO_SMALL:
            case OPUS_BAD_ARG:
                return OPUS_ENCODE_ERR_INVALID;
            default:
                return OPUS_ENCODE_ERR;
        }
    }

    /* 更新输出长度 */
    *out_len = (size_t)encoded_bytes;

    return OPUS_ENCODE_OK;
}

void opus_enc_reset(opus_enc_t encoder)
{
    if (encoder && encoder->encoder) {
        opus_encoder_ctl(encoder->encoder, OPUS_RESET_STATE);
    }
}

int opus_enc_set_bitrate(opus_enc_t encoder, int bitrate)
{
    if (!encoder || !encoder->encoder) {
        return -1;
    }

    return opus_encoder_ctl(encoder->encoder, OPUS_SET_BITRATE(bitrate));
}

int opus_enc_get_sample_rate(opus_enc_t encoder)
{
    return encoder ? encoder->sample_rate : 0;
}

int opus_enc_get_channels(opus_enc_t encoder)
{
    return encoder ? encoder->channels : 0;
}

int opus_enc_get_frame_size(opus_enc_t encoder)
{
    return encoder ? encoder->frame_size : 0;
}

size_t opus_enc_calc_packet_size(const opus_encoder_config_t *config, int frame_count)
{
    int sample_rate = config ? config->sample_rate : OPUS_DEFAULT_SAMPLE_RATE;
    int channels = config ? config->channels : OPUS_DEFAULT_CHANNELS;
    int frame_size = (config && config->frame_size > 0) ? config->frame_size : (sample_rate / 50);

    /* Max packet size is 1276 bytes per frame for Opus */
    return (size_t)(1276 * frame_count);
}

const char *opus_enc_get_version(void)
{
    return opus_get_version_string();
}

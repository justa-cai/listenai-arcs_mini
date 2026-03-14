/**
 * @file xz_opus_dec.c
 * @brief 小智云端 Opus 解码器封装实现
 */

#define TAG "xz_opus_dec"

#include "xz_opus_dec.h"
#include "opus_wrapper.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include <string.h>

/** Opus 解码器结构 */
struct xz_opus_dec_s {
    opus_dec_t handle;
    int sample_rate;
    int channels;
    int frame_size;
};

xz_opus_dec_t xz_opus_dec_create(const xz_opus_dec_config_t *config)
{
    if (!config) {
        LISA_LOGE(TAG, "Invalid config");
        return NULL;
    }

    /* 验证采样率 */
    if (config->sample_rate != 8000 &&
        config->sample_rate != 12000 &&
        config->sample_rate != 16000 &&
        config->sample_rate != 24000 &&
        config->sample_rate != 48000) {
        LISA_LOGE(TAG, "Unsupported sample rate: %d", config->sample_rate);
        return NULL;
    }

    /* 验证声道数 */
    if (config->channels != 1 && config->channels != 2) {
        LISA_LOGE(TAG, "Unsupported channels: %d", config->channels);
        return NULL;
    }

    /* 计算帧大小 (默认 60ms) */
    int frame_size = config->frame_size > 0 ? config->frame_size :
                     (config->sample_rate * 60) / 1000;  /* 60ms */

    /* 创建 SDK 解码器 */
    opus_decoder_config_t sdk_config = {
        .sample_rate = config->sample_rate,
        .channels = config->channels,
        .frame_size = frame_size,
    };

    opus_dec_t handle = opus_dec_create(&sdk_config);
    if (!handle) {
        LISA_LOGE(TAG, "Failed to create Opus decoder");
        return NULL;
    }

    /* 分配结构 */
    struct xz_opus_dec_s *decoder = (struct xz_opus_dec_s *)lisa_mem_calloc(1, sizeof(struct xz_opus_dec_s));
    if (!decoder) {
        opus_dec_destroy(handle);
        return NULL;
    }

    decoder->handle = handle;
    decoder->sample_rate = config->sample_rate;
    decoder->channels = config->channels;
    decoder->frame_size = frame_size;

    LISA_LOGI(TAG, "Opus decoder created: %dHz, %dch, %d samples/frame",
              decoder->sample_rate, decoder->channels, decoder->frame_size);

    return decoder;
}

void xz_opus_dec_destroy(xz_opus_dec_t decoder)
{
    if (!decoder) {
        return;
    }

    if (decoder->handle) {
        opus_dec_destroy(decoder->handle);
    }

    lisa_mem_free(decoder);
}

xz_opus_dec_result_e xz_opus_dec_decode(xz_opus_dec_t decoder,
                                         const uint8_t *in_data,
                                         size_t in_len,
                                         int16_t *out_data,
                                         int *out_len)
{
    if (!decoder || !in_data || !out_data || !out_len) {
        return XZ_OPUS_DECODE_ERR_INVALID;
    }

    /* 计算最大输出采样数 */
    int max_samples = decoder->frame_size * decoder->channels;
    if (*out_len < max_samples) {
        LISA_LOGE(TAG, "Output buffer too small: need %d, got %d",
                  max_samples, *out_len);
        return XZ_OPUS_DECODE_ERR_INVALID;
    }

    /* 解码 */
    size_t out_bytes = max_samples * sizeof(int16_t);
    opus_decode_result_t result = opus_dec_decode(decoder->handle,
                                                   in_data,
                                                   in_len,
                                                   out_data,
                                                   &out_bytes);

    if (result != OPUS_DECODE_OK) {
        LISA_LOGE(TAG, "Opus decode failed: %d", result);
        return (result == OPUS_DECODE_ERR_CORRUPT) ? XZ_OPUS_DECODE_ERR_CORRUPT :
               (result == OPUS_DECODE_ERR_INVALID) ? XZ_OPUS_DECODE_ERR_INVALID :
               XZ_OPUS_DECODE_ERR;
    }

    /* 更新输出长度 */
    *out_len = (int)(out_bytes / sizeof(int16_t));

    /* 调试输出 */
    static int decode_count = 0;
    if (decode_count++ < 5) {
        LISA_LOGI(TAG, "Opus decoded: %zu bytes -> %d samples", in_len, *out_len);
    }

    return XZ_OPUS_DECODE_OK;
}

void xz_opus_dec_reset(xz_opus_dec_t decoder)
{
    if (!decoder || !decoder->handle) {
        return;
    }

    opus_dec_reset(decoder->handle);
    LISA_LOGD(TAG, "Opus decoder reset");
}

int xz_opus_dec_get_sample_rate(xz_opus_dec_t decoder)
{
    return decoder ? decoder->sample_rate : 0;
}

int xz_opus_dec_get_channels(xz_opus_dec_t decoder)
{
    return decoder ? decoder->channels : 0;
}

int xz_opus_dec_get_frame_size(xz_opus_dec_t decoder)
{
    return decoder ? decoder->frame_size : 0;
}

int xz_opus_dec_calc_pcm_size(const xz_opus_dec_config_t *config, int frame_count)
{
    if (!config || frame_count <= 0) {
        return 0;
    }

    int frame_size = config->frame_size > 0 ? config->frame_size :
                     (config->sample_rate * 60) / 1000;  /* 默认 60ms */

    return frame_size * config->channels * frame_count;
}

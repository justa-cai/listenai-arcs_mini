/**
 * @file xz_opus.c
 * @brief 小智云端 Opus 编码器封装实现
 */

#define TAG "xz_opus"

#include "xz_opus.h"
#include "opus_encoder.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include <string.h>

/** Opus 编码器结构 */
struct xz_opus_s {
    opus_enc_t handle;
    int sample_rate;
    int channels;
    int frame_size;
    int bitrate;
};

xz_opus_t xz_opus_create(const xz_opus_config_t *config)
{
    if (!config) {
        LISA_LOGE(TAG, "Invalid config");
        return NULL;
    }

    /* 验证配置参数 */
    if (config->sample_rate != 16000) {
        LISA_LOGE(TAG, "Only 16kHz sample rate is supported");
        return NULL;
    }

    if (config->channels != 1) {
        LISA_LOGE(TAG, "Only mono (1 channel) is supported");
        return NULL;
    }

    /* 计算帧大小: 60ms @ 16kHz = 960 采样 */
    int frame_size = config->frame_size > 0 ? config->frame_size : 960;
    int bitrate = config->bitrate > 0 ? config->bitrate : 24000;
    int complexity = config->complexity >= 0 ? config->complexity : 0;

    /* 创建 Opus 编码器 */
    opus_encoder_config_t opus_config = {
        .sample_rate = config->sample_rate,
        .channels = config->channels,
        .frame_size = frame_size,
        .application = 2048,      /* OPUS_APPLICATION_VOIP */
        .bitrate = bitrate,
        .complexity = complexity,
        .vbr = true,              /* 可变比特率 */
    };

    opus_enc_t handle = opus_enc_create(&opus_config);
    if (!handle) {
        LISA_LOGE(TAG, "Failed to create Opus encoder");
        return NULL;
    }

    /* 分配结构 */
    struct xz_opus_s *encoder = (struct xz_opus_s *)lisa_mem_calloc(1, sizeof(struct xz_opus_s));
    if (!encoder) {
        opus_enc_destroy(handle);
        return NULL;
    }

    encoder->handle = handle;
    encoder->sample_rate = config->sample_rate;
    encoder->channels = config->channels;
    encoder->frame_size = frame_size;
    encoder->bitrate = bitrate;

    LISA_LOGI(TAG, "Opus encoder created: %dHz, %dch, %d samples/frame, %d bps",
              encoder->sample_rate, encoder->channels, encoder->frame_size, encoder->bitrate);

    return encoder;
}

void xz_opus_destroy(xz_opus_t encoder)
{
    if (!encoder) {
        return;
    }

    if (encoder->handle) {
        opus_enc_destroy(encoder->handle);
    }

    lisa_mem_free(encoder);
}

int xz_opus_encode(xz_opus_t encoder,
                   const int16_t *pcm_data,
                   int frame_size,
                   uint8_t *opus_data,
                   int *opus_size)
{
    if (!encoder || !pcm_data || !opus_data || !opus_size) {
        return -1;
    }

    if (frame_size != encoder->frame_size) {
        LISA_LOGE(TAG, "Frame size mismatch: expected %d, got %d",
                  encoder->frame_size, frame_size);
        return -1;
    }

    /* 编码 */
    size_t out_len = *opus_size;
    opus_encode_result_t result = opus_enc_encode(encoder->handle,
                                                   pcm_data,
                                                   frame_size,
                                                   opus_data,
                                                   &out_len);

    if (result != OPUS_ENCODE_OK) {
        LISA_LOGE(TAG, "Opus encode failed: %d", result);
        return -1;
    }

    *opus_size = (int)out_len;

    /* 调试输出 */
    static int encode_count = 0;
    if (encode_count++ < 5) {
        LISA_LOGI(TAG, "Opus encoded: %d samples -> %d bytes", frame_size, *opus_size);
    }

    return 0;
}

void xz_opus_reset(xz_opus_t encoder)
{
    if (!encoder || !encoder->handle) {
        return;
    }

    opus_enc_reset(encoder->handle);
    LISA_LOGD(TAG, "Opus encoder reset");
}

int xz_opus_get_frame_size(xz_opus_t encoder)
{
    return encoder ? encoder->frame_size : 0;
}

int xz_opus_get_sample_rate(xz_opus_t encoder)
{
    return encoder ? encoder->sample_rate : 0;
}

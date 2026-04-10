/**
 * @file bt_audio_codec_sbc.c
 * @brief SBC Codec适配器
 * 
 * Copyright (C) ListenAI 2025
 */

#include "bt_audio_codec.h"
#include "sbc.h"
#include <string.h>
#include <stdlib.h>
#include <lisa_log.h>

/* SBC编解码器上下文 */
static struct {
    sbc_t encoder;
    sbc_t decoder;
    bool encoder_initialized;
    bool decoder_initialized;
    bt_audio_codec_config_t config;
} g_sbc_ctx;

/* ========================================================================
 * SBC配置辅助函数
 * ======================================================================== */

static void sbc_config_from_bt_config(sbc_t *sbc, const bt_audio_codec_config_t *config)
{
    /* 采样率 */
    switch (config->format.sample_rate) {
        case 16000:
            sbc->frequency = SBC_FREQ_16000;
            break;
        case 32000:
            sbc->frequency = SBC_FREQ_32000;
            break;
        case 44100:
            sbc->frequency = SBC_FREQ_44100;
            break;
        case 48000:
            sbc->frequency = SBC_FREQ_48000;
            break;
        default:
            sbc->frequency = SBC_FREQ_44100;
            break;
    }
    
    /* 声道模式 */
    if (config->format.channels == 1) {
        sbc->mode = SBC_MODE_MONO;
    } else {
        sbc->mode = SBC_MODE_JOINT_STEREO;
    }
    
    /* 默认配置 */
    sbc->blocks = SBC_BLK_16;
    sbc->subbands = SBC_SB_8;
    sbc->allocation = SBC_AM_LOUDNESS;
    sbc->bitpool = 32;  /* 标准A2DP bitpool */
    sbc->endian = SBC_LE;
}

/* ========================================================================
 * Codec操作接口实现
 * ======================================================================== */

static bt_audio_error_t sbc_codec_init(const bt_audio_codec_config_t *config)
{
    if (!config) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    memset(&g_sbc_ctx, 0, sizeof(g_sbc_ctx));
    memcpy(&g_sbc_ctx.config, config, sizeof(bt_audio_codec_config_t));
    
    /* 初始化解码器 */
    if (sbc_init(&g_sbc_ctx.decoder, 0) < 0) {
        return BT_AUDIO_ERR_CODEC_FAILED;
    }

    g_sbc_ctx.decoder_initialized = true;
    
    /* 初始化编码器*/
    if (sbc_init(&g_sbc_ctx.encoder, 0) < 0) {
        sbc_finish(&g_sbc_ctx.decoder);
        return BT_AUDIO_ERR_CODEC_FAILED;
    }
    sbc_config_from_bt_config(&g_sbc_ctx.encoder, config);
    g_sbc_ctx.encoder_initialized = true;
    
    size_t codesize = sbc_get_codesize(&g_sbc_ctx.encoder);
    size_t frame_length = sbc_get_frame_length(&g_sbc_ctx.encoder);
    LISA_LOGI("SBC_CODEC", "Encoder initialized: %dHz %dch, codesize=%zu, frame_length=%zu",
              config->format.sample_rate, config->format.channels, codesize, frame_length);
    LISA_LOGI("SBC_CODEC", "Config: freq=%d, mode=%d, blocks=%d, subbands=%d, bitpool=%d",
              g_sbc_ctx.encoder.frequency, g_sbc_ctx.encoder.mode,
              g_sbc_ctx.encoder.blocks, g_sbc_ctx.encoder.subbands, g_sbc_ctx.encoder.bitpool);
    
    return BT_AUDIO_OK;
}

static bt_audio_error_t sbc_codec_deinit(void)
{
    if (g_sbc_ctx.decoder_initialized) {
        sbc_finish(&g_sbc_ctx.decoder);
        g_sbc_ctx.decoder_initialized = false;
    }
    
    if (g_sbc_ctx.encoder_initialized) {
        sbc_finish(&g_sbc_ctx.encoder);
        g_sbc_ctx.encoder_initialized = false;
    }
    
    return BT_AUDIO_OK;
}

static bt_audio_error_t sbc_codec_encode(const void *pcm_in, size_t pcm_len,
                                          void *encoded_out, size_t encoded_size,
                                          size_t *encoded_len)
{
    if (!g_sbc_ctx.encoder_initialized) {
        return BT_AUDIO_ERR_NOT_INITIALIZED;
    }
    
    if (!pcm_in || !encoded_out || !encoded_len) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    size_t pcm_frame_size = sbc_get_codesize(&g_sbc_ctx.encoder);
    size_t sbc_frame_size = sbc_get_frame_length(&g_sbc_ctx.encoder);
    
    /* 检查输入是否为一帧 */
    if (pcm_len != pcm_frame_size) {
        LISA_LOGE("SBC_CODEC", "Invalid PCM size: %zu, expected: %zu", pcm_len, pcm_frame_size);
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    /* 检查输出buffer是否足够 */
    if (encoded_size < sbc_frame_size) {
        LISA_LOGE("SBC_CODEC", "Output buffer too small: %zu < %zu", encoded_size, sbc_frame_size);
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    /* 编码一帧 */
    ssize_t written = 0;
    ssize_t result = sbc_encode(&g_sbc_ctx.encoder, pcm_in, pcm_frame_size,
                                encoded_out, encoded_size, &written);
    
    if (result < 0 || written == 0) {
        LISA_LOGE("SBC_CODEC", "Encode failed: result=%zd, written=%zd", result, written);
        LISA_LOGE("SBC_CODEC", "Encoder config: freq=%d, mode=%d, blocks=%d, subbands=%d, bitpool=%d",
                  g_sbc_ctx.encoder.frequency, g_sbc_ctx.encoder.mode, 
                  g_sbc_ctx.encoder.blocks, g_sbc_ctx.encoder.subbands, g_sbc_ctx.encoder.bitpool);
        return BT_AUDIO_ERR_CODEC_FAILED;
    }
    
    *encoded_len = written;
    
    return BT_AUDIO_OK;
}

static bt_audio_error_t sbc_codec_decode(const void *encoded_in, size_t encoded_len,
                                          void *pcm_out, size_t pcm_size,
                                          size_t *pcm_len)
{
    if (!g_sbc_ctx.decoder_initialized) {
        return BT_AUDIO_ERR_NOT_INITIALIZED;
    }
    
    if (!encoded_in || !pcm_out || !pcm_len) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    const uint8_t *input = (const uint8_t *)encoded_in;
    uint8_t *output = (uint8_t *)pcm_out;
    size_t remaining = encoded_len;
    size_t total_pcm_len = 0;
    
    /* 循环解码多个SBC帧 */
    while (remaining > 0 && (pcm_size - total_pcm_len) > 0) {
        /* 检查并跳过到同步头 0x9c */
        while (remaining > 0 && input[0] != 0x9c) {
            input++;
            remaining--;
        }
        
        if (remaining == 0) {
            break;  /* 没有更多数据 */
        }
        
        size_t frame_pcm_len = 0;
        ssize_t consumed = sbc_decode(&g_sbc_ctx.decoder, input, remaining,
                                      output, pcm_size - total_pcm_len, 
                                      &frame_pcm_len);
        
        if (consumed <= 0) {
            /* 解码失败，跳过一些数据继续尝试 */
            if (remaining > 32) {
                input += 32;
                remaining -= 32;
                continue;
            } else {
                break;
            }
        }
        
        /* 解码成功，更新指针和计数 */
        input += consumed;
        remaining -= consumed;
        output += frame_pcm_len;
        total_pcm_len += frame_pcm_len;
    }
    
    *pcm_len = total_pcm_len;
    
    if (total_pcm_len == 0) {
        return BT_AUDIO_ERR_CODEC_FAILED;
    }
    
    return BT_AUDIO_OK;
}

static size_t sbc_codec_get_frame_size(void)
{
    if (!g_sbc_ctx.encoder_initialized) {
        return 0;
    }
    
    return sbc_get_frame_length(&g_sbc_ctx.encoder);
}

static size_t sbc_codec_get_pcm_frame_size(void)
{
    if (!g_sbc_ctx.encoder_initialized) {
        return 0;
    }
    
    return sbc_get_codesize(&g_sbc_ctx.encoder);
}

static uint32_t sbc_codec_get_frame_duration_us(void)
{
    if (!g_sbc_ctx.encoder_initialized) {
        return 0;
    }
    
    return sbc_get_frame_duration(&g_sbc_ctx.encoder);
}

static bt_audio_error_t sbc_codec_reset(void)
{
    if (g_sbc_ctx.decoder_initialized) {
        sbc_reinit(&g_sbc_ctx.decoder, 0);
    }
    
    if (g_sbc_ctx.encoder_initialized) {
        sbc_reinit(&g_sbc_ctx.encoder, 0);
    }
    
    return BT_AUDIO_OK;
}

/* ========================================================================
 * Codec操作接口注册
 * ======================================================================== */

static const bt_audio_codec_ops_t sbc_codec_ops = {
    .name = "SBC",
    .type = BT_CODEC_SBC,
    .init = sbc_codec_init,
    .deinit = sbc_codec_deinit,
    .encode = sbc_codec_encode,
    .decode = sbc_codec_decode,
    .get_frame_size = sbc_codec_get_frame_size,
    .get_pcm_frame_size = sbc_codec_get_pcm_frame_size,
    .get_frame_duration_us = sbc_codec_get_frame_duration_us,
    .reset = sbc_codec_reset,
};

bt_audio_error_t bt_audio_codec_sbc_register(void)
{
    return bt_audio_codec_register(&sbc_codec_ops);
}

/**
 * @file bt_audio_codec_lc3.c
 * @brief LC3 Codec适配器
 * 
 * Copyright (C) ListenAI 2025
 */

#include "bt_audio_codec.h"
#include "codec_lc3.h"
#include <string.h>
#include <stdlib.h>

/* LC3编解码器上下文 */
static struct {
    bool initialized;
    bt_audio_codec_config_t config;
    uint16_t frame_size;
    uint16_t pcm_frame_size;
} g_lc3_ctx;

/* ========================================================================
 * Codec操作接口实现
 * ======================================================================== */

static bt_audio_error_t lc3_codec_init(const bt_audio_codec_config_t *config)
{
    if (!config) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    memset(&g_lc3_ctx, 0, sizeof(g_lc3_ctx));
    memcpy(&g_lc3_ctx.config, config, sizeof(bt_audio_codec_config_t));
    
    /* 计算帧时长(ms) */
    uint8_t frame_ms = config->frame_duration_us / 1000;
    if (frame_ms == 0) {
        frame_ms = 10;  /* 默认10ms */
    }
    
    /* 启动LC3 codec */
    uint8_t result = app_lc3_start(
        config->format.sample_rate,
        config->format.channels,
        frame_ms,
        config->format.bits_per_sample,
        0  /* hrmode */
    );
    
    if (result != 0) {
        return BT_AUDIO_ERR_CODEC_FAILED;
    }
    
    /* 计算帧大小 */
    g_lc3_ctx.pcm_frame_size = (config->format.sample_rate / 1000) * 
                                frame_ms * 
                                config->format.channels * 
                                (config->format.bits_per_sample / 8);
    
    /* LC3编码帧大小基于比特率计算 */
    g_lc3_ctx.frame_size = (config->bitrate * frame_ms) / 8;
    
    g_lc3_ctx.initialized = true;
    
    return BT_AUDIO_OK;
}

static bt_audio_error_t lc3_codec_deinit(void)
{
    if (!g_lc3_ctx.initialized) {
        return BT_AUDIO_OK;
    }
    
    app_lc3_stop();
    g_lc3_ctx.initialized = false;
    
    return BT_AUDIO_OK;
}

static bt_audio_error_t lc3_codec_encode(const void *pcm_in, size_t pcm_len,
                                          void *encoded_out, size_t encoded_size,
                                          size_t *encoded_len)
{
    if (!g_lc3_ctx.initialized) {
        return BT_AUDIO_ERR_NOT_INITIALIZED;
    }
    
    if (!pcm_in || !encoded_out || !encoded_len) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    /* LC3编码需要使用专门的编码接口 */
    /* 注意: 原始的app_lc3接口只提供了解码，这里需要扩展编码支持 */
    /* 暂时返回不支持 */
    return BT_AUDIO_ERR_NOT_SUPPORTED;
}

static bt_audio_error_t lc3_codec_decode(const void *encoded_in, size_t encoded_len,
                                          void *pcm_out, size_t pcm_size,
                                          size_t *pcm_len)
{
    if (!g_lc3_ctx.initialized) {
        return BT_AUDIO_ERR_NOT_INITIALIZED;
    }
    
    if (!encoded_in || !pcm_out || !pcm_len) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    uint16_t out_len = 0;
    uint8_t result = app_lc3_dec(
        encoded_len,
        (uint8_t*)encoded_in,
        &out_len,
        (uint8_t*)pcm_out,
        0  /* bfi: bad frame indicator */
    );
    
    if (result != 0) {
        return BT_AUDIO_ERR_CODEC_FAILED;
    }
    
    *pcm_len = out_len;
    return BT_AUDIO_OK;
}

static size_t lc3_codec_get_frame_size(void)
{
    return g_lc3_ctx.frame_size;
}

static size_t lc3_codec_get_pcm_frame_size(void)
{
    return g_lc3_ctx.pcm_frame_size;
}

static bt_audio_error_t lc3_codec_reset(void)
{
    if (!g_lc3_ctx.initialized) {
        return BT_AUDIO_ERR_NOT_INITIALIZED;
    }
    
    /* 重启codec */
    app_lc3_stop();
    
    uint8_t frame_ms = g_lc3_ctx.config.frame_duration_us / 1000;
    app_lc3_start(
        g_lc3_ctx.config.format.sample_rate,
        g_lc3_ctx.config.format.channels,
        frame_ms,
        g_lc3_ctx.config.format.bits_per_sample,
        0
    );
    
    return BT_AUDIO_OK;
}

/* ========================================================================
 * Codec操作接口注册
 * ======================================================================== */

static const bt_audio_codec_ops_t lc3_codec_ops = {
    .name = "LC3",
    .type = BT_CODEC_LC3,
    .init = lc3_codec_init,
    .deinit = lc3_codec_deinit,
    .encode = lc3_codec_encode,
    .decode = lc3_codec_decode,
    .get_frame_size = lc3_codec_get_frame_size,
    .get_pcm_frame_size = lc3_codec_get_pcm_frame_size,
    .reset = lc3_codec_reset,
};

bt_audio_error_t bt_audio_codec_lc3_register(void)
{
    return bt_audio_codec_register(&lc3_codec_ops);
}

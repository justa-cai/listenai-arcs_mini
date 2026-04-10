/**
 * @file bt_audio_codec_msbc.c
 * @brief mSBC Codec适配器
 * 
 * Copyright (C) ListenAI 2025
 */

#include "bt_audio_codec.h"
#include "msbc.h"
#include <string.h>
#include <stdlib.h>

#define MSBC_FRAME_SIZE 57      /* mSBC编码帧大小 */
#define MSBC_PCM_FRAME_SIZE 240 /* mSBC PCM帧大小 (120个16位采样) */

/* mSBC静音帧 - 用于丢包隐藏 */
static const uint8_t msbc_mute_frame[MSBC_FRAME_SIZE] = {
    0xAD, 0x00, 0x00, 0xC5, 0x00, 0x00, 0x00, 0x00, 0x77, 0x6D, 0xB6, 0xDD, 0xDB, 0x6D, 0xB7, 0x76,
    0xDB, 0x6D, 0xDD, 0xB6, 0xDB, 0x77, 0x6D, 0xB6, 0xDD, 0xDB, 0x6D, 0xB7, 0x76, 0xDB, 0x6D, 0xDD,
    0xB6, 0xDB, 0x77, 0x6D, 0xB6, 0xDD, 0xDB, 0x6D, 0xB7, 0x76, 0xDB, 0x6D, 0xDD, 0xB6, 0xDB, 0x77,
    0x6D, 0xB6, 0xDD, 0xDB, 0x6D, 0xB7, 0x76, 0xDB, 0x6C
};

/* mSBC编解码器上下文 */
static struct {
    msbc_t encoder;
    msbc_t decoder;
    bool encoder_initialized;
    bool decoder_initialized;
    bt_audio_codec_config_t config;
    uint8_t encoder_priv_mem[1024];  /* 编码器私有内存 */
    uint8_t decoder_priv_mem[1024];  /* 解码器私有内存 */
    /* 解码器可能需要缓存不完整的帧数据（从蓝牙接收可能不对齐） */
    uint8_t dec_residue[MSBC_FRAME_SIZE];
    size_t dec_residue_len;
} g_msbc_ctx;

/* ========================================================================
 * Codec操作接口实现
 * ======================================================================== */

static bt_audio_error_t msbc_codec_init(const bt_audio_codec_config_t *config)
{
    if (!config) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    memset(&g_msbc_ctx, 0, sizeof(g_msbc_ctx));
    memcpy(&g_msbc_ctx.config, config, sizeof(bt_audio_codec_config_t));
    
    /* 初始化解码器 */
    if (msbc_init(&g_msbc_ctx.decoder, 0) < 0) {
        return BT_AUDIO_ERR_CODEC_FAILED;
    }
    g_msbc_ctx.decoder_initialized = true;
    
    /* 初始化编码器 */
    if (msbc_init(&g_msbc_ctx.encoder, 0) < 0) {
        msbc_finish(&g_msbc_ctx.decoder);
        return BT_AUDIO_ERR_CODEC_FAILED;
    }
    g_msbc_ctx.encoder_initialized = true;
    
    return BT_AUDIO_OK;
}

static bt_audio_error_t msbc_codec_deinit(void)
{
    if (g_msbc_ctx.decoder_initialized) {
        msbc_finish(&g_msbc_ctx.decoder);
        g_msbc_ctx.decoder_initialized = false;
    }
    
    if (g_msbc_ctx.encoder_initialized) {
        msbc_finish(&g_msbc_ctx.encoder);
        g_msbc_ctx.encoder_initialized = false;
    }
    
    return BT_AUDIO_OK;
}

static bt_audio_error_t msbc_codec_encode(const void *pcm_in, size_t pcm_len,
                                           void *encoded_out, size_t encoded_size,
                                           size_t *encoded_len)
{
    if (!g_msbc_ctx.encoder_initialized) {
        return BT_AUDIO_ERR_NOT_INITIALIZED;
    }
    
    if (!pcm_in || !encoded_out || !encoded_len) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    /* 检查输入是否为一帧 */
    if (pcm_len != MSBC_PCM_FRAME_SIZE) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    /* 检查输出buffer是否足够 */
    if (encoded_size < MSBC_FRAME_SIZE) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    /* 编码一帧 */
    size_t written = 0;
    ssize_t result = msbc_encode(&g_msbc_ctx.encoder, pcm_in, MSBC_PCM_FRAME_SIZE,
                                 encoded_out, encoded_size, &written);
    
    if (result < 0 || written == 0) {
        return BT_AUDIO_ERR_CODEC_FAILED;
    }
    
    *encoded_len = written;
    return BT_AUDIO_OK;
}

static bt_audio_error_t msbc_codec_decode(const void *encoded_in, size_t encoded_len,
                                           void *pcm_out, size_t pcm_size,
                                           size_t *pcm_len)
{
    if (!g_msbc_ctx.decoder_initialized) {
        return BT_AUDIO_ERR_NOT_INITIALIZED;
    }
    
    if (!encoded_in || !pcm_out || !pcm_len) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    const uint8_t *input = (const uint8_t *)encoded_in;
    uint8_t *output = (uint8_t *)pcm_out;
    size_t remaining = encoded_len;
    size_t total_pcm_len = 0;
    
    /* 1. 先处理上次遗留的不完整mSBC帧 */
    if (g_msbc_ctx.dec_residue_len > 0) {
        size_t need = MSBC_FRAME_SIZE - g_msbc_ctx.dec_residue_len;
        size_t copy = (remaining < need) ? remaining : need;
        
        memcpy(g_msbc_ctx.dec_residue + g_msbc_ctx.dec_residue_len, input, copy);
        g_msbc_ctx.dec_residue_len += copy;
        input += copy;
        remaining -= copy;
        
        /* 如果凑够了一个完整帧，进行解码 */
        if (g_msbc_ctx.dec_residue_len == MSBC_FRAME_SIZE) {
            size_t written = 0;  /* 注意：msbc_decode 返回的是采样数 */
            ssize_t consumed = msbc_decode(&g_msbc_ctx.decoder, g_msbc_ctx.dec_residue,
                                          MSBC_FRAME_SIZE, output, MSBC_PCM_FRAME_SIZE / 2, &written);
            
            if (consumed > 0 && written > 0) {
                size_t written_bytes = written * 2;  /* 采样数转字节数 */
                output += written_bytes;
                total_pcm_len += written_bytes;
            } else {
                /* 解码失败，使用静音帧PLC */
                size_t mute_written = 0;
                msbc_decode(&g_msbc_ctx.decoder, msbc_mute_frame, MSBC_FRAME_SIZE,
                           output, MSBC_PCM_FRAME_SIZE / 2, &mute_written);
                if (mute_written > 0) {
                    size_t mute_bytes = mute_written * 2;
                    output += mute_bytes;
                    total_pcm_len += mute_bytes;
                }
            }
            g_msbc_ctx.dec_residue_len = 0;
        }
    }
    
    /* 2. 循环解码所有完整的mSBC帧 */
    while (remaining >= MSBC_FRAME_SIZE && (pcm_size - total_pcm_len) >= MSBC_PCM_FRAME_SIZE) {
        size_t written = 0;  /* 注意：msbc_decode 返回的是采样数，不是字节数 */
        ssize_t consumed = msbc_decode(&g_msbc_ctx.decoder, input, remaining,
                                       output, MSBC_PCM_FRAME_SIZE / 2, &written);  /* 120个采样 */
        
        if (consumed <= 0 || written == 0) {
            /* 解码失败，使用静音帧进行丢包隐藏 (PLC) */
            size_t mute_written = 0;
            ssize_t mute_consumed = msbc_decode(&g_msbc_ctx.decoder, msbc_mute_frame, MSBC_FRAME_SIZE,
                                                output, MSBC_PCM_FRAME_SIZE / 2, &mute_written);
            
            if (mute_consumed > 0 && mute_written > 0) {
                size_t mute_bytes = mute_written * 2;  /* 采样数转字节数 */
                output += mute_bytes;
                total_pcm_len += mute_bytes;
            }
            
            /* 跳过损坏的数据 */
            if (consumed > 0) {
                input += consumed;
                remaining -= consumed;
            } else {
                /* 完全失败，跳过一个帧的大小 */
                size_t skip = (remaining >= MSBC_FRAME_SIZE) ? MSBC_FRAME_SIZE : remaining;
                input += skip;
                remaining -= skip;
            }
            continue;
        }
        
        /* 解码成功，更新指针（written是采样数，需要转为字节数） */
        size_t written_bytes = written * 2;  /* 16位采样转字节 */
        input += consumed;
        output += written_bytes;
        remaining -= consumed;
        total_pcm_len += written_bytes;
    }
    
    /* 3. 保存剩余的不完整mSBC帧 */
    if (remaining > 0 && remaining < MSBC_FRAME_SIZE) {
        memcpy(g_msbc_ctx.dec_residue, input, remaining);
        g_msbc_ctx.dec_residue_len = remaining;
    }
    
    *pcm_len = total_pcm_len;
    return (total_pcm_len > 0) ? BT_AUDIO_OK : BT_AUDIO_ERR_CODEC_FAILED;
}

static size_t msbc_codec_get_frame_size(void)
{
    return MSBC_FRAME_SIZE;
}

static size_t msbc_codec_get_pcm_frame_size(void)
{
    return MSBC_PCM_FRAME_SIZE;
}

static bt_audio_error_t msbc_codec_reset(void)
{
    if (g_msbc_ctx.decoder_initialized) {
        msbc_reinit(&g_msbc_ctx.decoder, 0);
    }
    
    if (g_msbc_ctx.encoder_initialized) {
        msbc_reinit(&g_msbc_ctx.encoder, 0);
    }
    
    /* 清空解码器残留数据 */
    g_msbc_ctx.dec_residue_len = 0;
    
    return BT_AUDIO_OK;
}

/* ========================================================================
 * Codec操作接口注册
 * ======================================================================== */

static const bt_audio_codec_ops_t msbc_codec_ops = {
    .name = "mSBC",
    .type = BT_CODEC_MSBC,
    .init = msbc_codec_init,
    .deinit = msbc_codec_deinit,
    .encode = msbc_codec_encode,
    .decode = msbc_codec_decode,
    .get_frame_size = msbc_codec_get_frame_size,
    .get_pcm_frame_size = msbc_codec_get_pcm_frame_size,
    .reset = msbc_codec_reset,
};

bt_audio_error_t bt_audio_codec_msbc_register(void)
{
    return bt_audio_codec_register(&msbc_codec_ops);
}

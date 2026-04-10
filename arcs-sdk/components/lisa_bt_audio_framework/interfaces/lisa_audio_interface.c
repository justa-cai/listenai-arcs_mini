/**
 * @file lisa_audio_interface.c
 * @brief lisa_audio驱动接口适配器
 * 
 * 职责：
 * ========================================================================
 * 提供本地物理声卡的音频输入输出能力，作为底层音频接口实现。
 * 
 * 本接口与蓝牙角色（Sink/Source）无关，只负责：
 * - 播放输出：将PCM数据输出到本地扬声器
 * - 录音输入：从本地麦克风采集PCM数据
 * 
 * 适用场景：
 * ========================================================================
 * 1. **Sink模式（蓝牙耳机/音箱）**：
 *    - Playback: 蓝牙接收 → framework解码 → playback_write() → 本地扬声器
 *    - Capture: 本地麦克风 → capture_callback() → framework编码 → 蓝牙发送
 * 
 * 2. **Source模式（蓝牙音源）**：
 *    - Capture: 本地麦克风 → capture_callback() → framework编码 → 蓝牙发送
 *    - Playback: 可能用于本地监听或其他用途
 * 
 * 3. **混合场景**：
 *    - 同时使用本地声卡和蓝牙音频
 * 
 * 数据流向：
 * ========================================================================
 * Playback: PCM数据 → playback_write() → lisa_audio驱动 → DAC → 扬声器
 * Capture: 麦克风 → ADC → lisa_audio驱动 → 回调通知 → framework
 * 
 * @note 本接口不处理编解码，只处理PCM音频数据
 * @note 编解码由framework的codec manager处理
 * 
 * Copyright (C) ListenAI 2025
 */

#include "bt_audio_interface.h"
#include "lisa_audio.h"
#include <string.h>
#include <stdlib.h>
#include "lisa_log.h"
#include "FreeRTOS.h"
#include "task.h"

#define TAG "LISA_AUDIO_INTERFACE"

#define LISA_AUDIO_DEVICE_NAME "audio0"

/* lisa_audio上下文 */
static struct {
    lisa_device_t *audio_dev;
    
    /* 播放相关 */
    struct {
        bool is_open;
        bool started;  /* 播放是否已启动 */
        bt_audio_format_t format;
        bt_audio_playback_callback_t callback;
        void *user_data;
    } playback;
    
    /* 录音相关 */
    struct {
        bool is_open;
        bt_audio_format_t format;
        bt_audio_capture_callback_t callback;
        void *user_data;
    } capture;
    
    uint8_t current_volume;
    
} g_lisa_ctx;

/* ========================================================================
 * 辅助函数
 * ======================================================================== */

/* 转换采样率 */
static lisa_audio_rate_t convert_sample_rate(uint32_t rate)
{
    switch (rate) {
        case 8000:
            return LISA_AUDIO_RATE_8K;
        case 16000:
            return LISA_AUDIO_RATE_16K;
        case 24000:
            return LISA_AUDIO_RATE_24K;
        case 32000:
            return LISA_AUDIO_RATE_32K;
        case 48000:
            return LISA_AUDIO_RATE_48K;
        case 96000:
            return LISA_AUDIO_RATE_96K;
        default:
            LISA_LOGW(TAG, "Unsupported sample rate: %u", rate);
            return LISA_AUDIO_RATE_48K;
    }
}

/* 转换声道配置 */
static lisa_audio_channel_t convert_channels(uint8_t channels)
{
    switch (channels) {
        case 1:
            return LISA_AUDIO_CH_LEFT;
        case 2:
            return LISA_AUDIO_CH_STEREO;
        default:
            LISA_LOGW(TAG, "Unsupported channel count: %u", channels);
            return LISA_AUDIO_CH_STEREO;
    }
}

/* 转换采样位深 */
static lisa_audio_bits_t convert_bits(uint8_t bits)
{
    switch (bits) {
        case 16:
            return LISA_AUDIO_BIT_16;
        case 24:
            return LISA_AUDIO_BIT_24;
        case 32:
            return LISA_AUDIO_BIT_32;
        default:
            LISA_LOGW(TAG, "Unsupported bit depth: %u", bits);
            return LISA_AUDIO_BIT_16;
    }
}

/* lisa_audio回调函数 */
static void lisa_audio_event_callback(const lisa_audio_event_t *event, void *user_data)
{
    if (!event) {
        return;
    }

    /* 处理录音数据 */
    if (event->record_buffer && event->record_samples > 0) {
        if (g_lisa_ctx.capture.is_open && g_lisa_ctx.capture.callback) {
            size_t data_size = event->record_samples * 
                              g_lisa_ctx.capture.format.channels *
                              (g_lisa_ctx.capture.format.bits_per_sample / 8);
            g_lisa_ctx.capture.callback(event->record_buffer, data_size, 
                                       g_lisa_ctx.capture.user_data);
        }
    }
}

/* ========================================================================
 * 播放接口实现：输出PCM到本地扬声器
 * ======================================================================== */

static bt_audio_error_t lisa_playback_open(const bt_audio_format_t *format)
{
    if (!format) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    if (g_lisa_ctx.playback.is_open) {
        return BT_AUDIO_ERR_BUSY;
    }
    
    /* 获取音频设备 */
    if (!g_lisa_ctx.audio_dev) {
        g_lisa_ctx.audio_dev = lisa_device_get(LISA_AUDIO_DEVICE_NAME);
        if (!g_lisa_ctx.audio_dev) {
            return BT_AUDIO_ERR_NOT_FOUND;
        }
    }
    
    LISA_LOGI(TAG, "Opening playback: %u Hz, %u ch, %u bits",
               format->sample_rate, format->channels, format->bits_per_sample);

    /* 配置播放参数 */
    lisa_audio_play_config_t play_config = {
        .format = {
            .sample_rate = convert_sample_rate(format->sample_rate),
            .channels = convert_channels(format->channels),
            .sample_bits = convert_bits(format->bits_per_sample),
        },
        .gain = {
            .analog_gain = 0,
            .digital_gain = 0,
        },
        .buffer_count = CONFIG_LISA_BT_AUDIO_HW_BUFFER_COUNT,
        .buffer_samples = format->sample_rate * CONFIG_LISA_BT_AUDIO_WORK_BUFFER_TIME_MS 
                        / 1000 / (format->channels * (format->bits_per_sample / 8)),
    };
    
    int ret = lisa_audio_play_config(g_lisa_ctx.audio_dev, &play_config);
    if (ret != 0) {
        return BT_AUDIO_ERR_INTERFACE_FAILED;
    }
    
    /* 启动播放 */
    ret = lisa_audio_play_start(g_lisa_ctx.audio_dev);
    if (ret != 0) {
        return BT_AUDIO_ERR_INTERFACE_FAILED;
    }
    
    /* 保存配置 */
    memcpy(&g_lisa_ctx.playback.format, format, sizeof(bt_audio_format_t));
    g_lisa_ctx.playback.is_open = true;
    g_lisa_ctx.playback.started = true;
    

    LISA_LOGI(TAG, "Playback opened: %u Hz, %u ch, %u bits",
               format->sample_rate, format->channels, format->bits_per_sample);
    return BT_AUDIO_OK;
}

static int lisa_playback_write(const void *data, size_t size)
{
    if (!g_lisa_ctx.playback.is_open) {
        return -1;
    }
    
    if (!data || size == 0) {
        return 0;
    }
    
    /* 计算采样点数（所有通道总和）*/
    uint32_t samples = size / (g_lisa_ctx.playback.format.bits_per_sample / 8);

    int ret = lisa_audio_play_write(g_lisa_ctx.audio_dev, data, samples);
    if (ret < 0) {
        LISA_LOGE(TAG, "lisa_audio_play_write failed: %d", ret);
        return ret;
    }

    return size;
}

static bt_audio_error_t lisa_playback_close(void)
{
    if (!g_lisa_ctx.playback.is_open) {
        return BT_AUDIO_OK;
    }
    
    /* 停止播放 */
    lisa_audio_play_stop(g_lisa_ctx.audio_dev);
    
    g_lisa_ctx.playback.is_open = false;
    g_lisa_ctx.playback.started = false;
    g_lisa_ctx.playback.callback = NULL;
    g_lisa_ctx.playback.user_data = NULL;
    
    return BT_AUDIO_OK;
}

static bt_audio_error_t lisa_playback_pause(void)
{
    if (!g_lisa_ctx.playback.is_open) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    /* lisa_audio可能不支持暂停，这里暂时返回成功 */
    return BT_AUDIO_OK;
}

static bt_audio_error_t lisa_playback_resume(void)
{
    if (!g_lisa_ctx.playback.is_open) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    return BT_AUDIO_OK;
}

/* ========================================================================
 * 录音接口实现（Sink模式：本地麦克风采集 → 蓝牙发送）
 * ======================================================================== */

static bt_audio_error_t lisa_capture_open(const bt_audio_format_t *format,
                                          bt_audio_capture_callback_t callback,
                                          void *user_data)
{
    if (!format || !callback) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    if (g_lisa_ctx.capture.is_open) {
        return BT_AUDIO_ERR_BUSY;
    }
    
    /* 获取音频设备 */
    if (!g_lisa_ctx.audio_dev) {
        g_lisa_ctx.audio_dev = lisa_device_get(LISA_AUDIO_DEVICE_NAME);
        if (!g_lisa_ctx.audio_dev) {
            return BT_AUDIO_ERR_NOT_FOUND;
        }
    }
    
    /* 注册回调 */
    int ret = lisa_audio_register_callback(g_lisa_ctx.audio_dev, 
                                          lisa_audio_event_callback, 
                                          NULL);
    if (ret != 0) {
        return BT_AUDIO_ERR_INTERFACE_FAILED;
    }
    
    /* 配置录音参数 */
    lisa_audio_record_config_t record_config = {
        .format = {
            .sample_rate = convert_sample_rate(format->sample_rate),
            .channels = convert_channels(format->channels),
            .sample_bits = convert_bits(format->bits_per_sample),
        },
        .gain = {
            .analog_gain = 16,
            .digital_gain = 2,
        },
        .differential_input = true,
        .enable_hpf = true,
    };
    
    ret = lisa_audio_record_config(g_lisa_ctx.audio_dev, &record_config);
    if (ret != 0) {
        lisa_audio_unregister_callback(g_lisa_ctx.audio_dev, lisa_audio_event_callback);
        return BT_AUDIO_ERR_INTERFACE_FAILED;
    }
    
    /* 启动录音 */
    ret = lisa_audio_record_start(g_lisa_ctx.audio_dev);
    if (ret != 0) {
        lisa_audio_unregister_callback(g_lisa_ctx.audio_dev, lisa_audio_event_callback);
        return BT_AUDIO_ERR_INTERFACE_FAILED;
    }
    
    /* 保存配置 */
    memcpy(&g_lisa_ctx.capture.format, format, sizeof(bt_audio_format_t));
    g_lisa_ctx.capture.callback = callback;
    g_lisa_ctx.capture.user_data = user_data;
    g_lisa_ctx.capture.is_open = true;
    
    return BT_AUDIO_OK;
}

static int lisa_capture_read(void *buffer, size_t size)
{
    if (!g_lisa_ctx.capture.is_open) {
        return -1;
    }
    
    /* lisa_audio使用回调方式，不支持主动读取 */
    return BT_AUDIO_ERR_NOT_SUPPORTED;
}

static bt_audio_error_t lisa_capture_close(void)
{
    if (!g_lisa_ctx.capture.is_open) {
        return BT_AUDIO_OK;
    }
    
    /* 停止录音 */
    lisa_audio_record_stop(g_lisa_ctx.audio_dev);
    
    /* 注销回调 */
    lisa_audio_unregister_callback(g_lisa_ctx.audio_dev, lisa_audio_event_callback);
    
    g_lisa_ctx.capture.is_open = false;
    g_lisa_ctx.capture.callback = NULL;
    g_lisa_ctx.capture.user_data = NULL;
    
    return BT_AUDIO_OK;
}

/* ========================================================================
 * 音量控制
 * ======================================================================== */

static bt_audio_error_t lisa_set_volume(uint8_t volume)
{
    if (volume > 100) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    g_lisa_ctx.current_volume = volume;
    
    if (!g_lisa_ctx.audio_dev) {
        return BT_AUDIO_OK;
    }
    
    /* 计算增益 (简化版本: 0-100 映射到 -40dB ~ 0dB) */
    int8_t digital_gain = (int8_t)((volume * 40) / 100) - 40;
    
    if (g_lisa_ctx.playback.is_open) {
        lisa_audio_gain_t gain = {
            .analog_gain = 0,
            .digital_gain = digital_gain,
        };
        lisa_audio_play_set_gain(g_lisa_ctx.audio_dev, &gain);
    }
    
    return BT_AUDIO_OK;
}

static bt_audio_error_t lisa_get_volume(uint8_t *volume)
{
    if (!volume) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    *volume = g_lisa_ctx.current_volume;
    return BT_AUDIO_OK;
}

/* ========================================================================
 * 音频接口操作注册（Sink模式接口）
 * ======================================================================== */

static const bt_audio_interface_ops_t lisa_audio_interface_ops = {
    .name = "lisa_audio",
    .playback_open = lisa_playback_open,      /* 蓝牙音频 → 本地播放 */
    .playback_write = lisa_playback_write,
    .playback_close = lisa_playback_close,
    .playback_pause = lisa_playback_pause,
    .playback_resume = lisa_playback_resume,
    .capture_open = lisa_capture_open,        /* 本地录音 → 蓝牙发送 */
    .capture_read = lisa_capture_read,
    .capture_close = lisa_capture_close,
    .set_volume = lisa_set_volume,
    .get_volume = lisa_get_volume,
};

/**
 * @brief 注册 lisa_audio 适配器（用于Sink模式）
 * 
 * 将此接口注册到蓝牙音频框架，使框架能够使用本地 lisa_audio 声卡
 * 进行音频播放和录音。
 * 
 * 使用场景：
 * - 蓝牙音箱模式：接收手机音频并播放
 * - 蓝牙耳机模式：双向音频通话
 * 
 * @return BT_AUDIO_OK: 成功, 其他: 错误码
 */
bt_audio_error_t bt_audio_interface_lisa_register(void)
{
    memset(&g_lisa_ctx, 0, sizeof(g_lisa_ctx));
    g_lisa_ctx.current_volume = 50;  /* 默认音量50% */
    
    return bt_audio_interface_register(&lisa_audio_interface_ops);
}

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
#include "lisa_audio_playback_resample.h"
#include "core_feature_base.h"
#include <string.h>
#include <stdlib.h>
#include "lisa_log.h"
#include "FreeRTOS.h"
#include "task.h"

#define TAG "LISA_AUDIO_INTERFACE"

#define LISA_AUDIO_PROFILE_LOG_INTERVAL 64U
#define LISA_AUDIO_RESAMPLE_SLOW_US 5000U
#define LISA_AUDIO_PLAYBACK_WRITE_SLOW_US 10000U

#define LISA_AUDIO_DEVICE_NAME "audio0"

extern volatile uint32_t SystemCoreClock;

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
        struct {
            lisa_audio_playback_resample_state_t state;
            int16_t *buffer;
            size_t buffer_capacity_samples;
            uint32_t profile_calls;
            uint32_t profile_slow_calls;
            uint32_t profile_max_cycles;
            uint64_t profile_total_cycles;
        } resample;
        struct {
            uint32_t profile_calls;
            uint32_t profile_slow_calls;
            uint32_t profile_max_total_cycles;
            uint32_t profile_max_driver_cycles;
            uint64_t profile_total_cycles;
            uint64_t profile_driver_cycles;
        } write;
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
        case 44100:
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

static void lisa_playback_resample_reset(void)
{
    free(g_lisa_ctx.playback.resample.buffer);
    memset(&g_lisa_ctx.playback.resample, 0, sizeof(g_lisa_ctx.playback.resample));
}

static bt_audio_error_t lisa_playback_resample_ensure_capacity(size_t input_frames)
{
    size_t required_samples;
    int16_t *buffer;

    if (!g_lisa_ctx.playback.resample.state.enabled) {
        return BT_AUDIO_OK;
    }

    required_samples = input_frames * g_lisa_ctx.playback.resample.state.resampler.channels * 2U;
    if (required_samples == 0) {
        required_samples = g_lisa_ctx.playback.resample.state.resampler.channels * 2U;
    }

    if (required_samples <= g_lisa_ctx.playback.resample.buffer_capacity_samples) {
        return BT_AUDIO_OK;
    }

    buffer = realloc(g_lisa_ctx.playback.resample.buffer, required_samples * sizeof(int16_t));
    if (!buffer) {
        LISA_LOGE(TAG, "Failed to allocate playback resample buffer: %u samples",
                  (unsigned int)required_samples);
        return BT_AUDIO_ERR_NO_MEMORY;
    }

    g_lisa_ctx.playback.resample.buffer = buffer;
    g_lisa_ctx.playback.resample.buffer_capacity_samples = required_samples;
    return BT_AUDIO_OK;
}

static uint32_t lisa_resample_cycles_to_us(uint32_t cycles)
{
    if (SystemCoreClock == 0U) {
        return 0U;
    }

    return (uint32_t)(((uint64_t)cycles * 1000000ULL) / (uint64_t)SystemCoreClock);
}

static void lisa_playback_resample_profile(uint32_t cycles, size_t input_size, size_t output_size)
{
    uint32_t current_us;
    uint32_t average_us;
    uint32_t max_us;

    g_lisa_ctx.playback.resample.profile_calls++;
    g_lisa_ctx.playback.resample.profile_total_cycles += cycles;
    if (cycles > g_lisa_ctx.playback.resample.profile_max_cycles) {
        g_lisa_ctx.playback.resample.profile_max_cycles = cycles;
    }

    current_us = lisa_resample_cycles_to_us(cycles);
    if (current_us >= LISA_AUDIO_RESAMPLE_SLOW_US) {
        g_lisa_ctx.playback.resample.profile_slow_calls++;
    }

    if ((current_us >= LISA_AUDIO_RESAMPLE_SLOW_US) ||
        ((g_lisa_ctx.playback.resample.profile_calls % LISA_AUDIO_PROFILE_LOG_INTERVAL) == 0U)) {
        average_us = lisa_resample_cycles_to_us((uint32_t)(g_lisa_ctx.playback.resample.profile_total_cycles /
                                                            g_lisa_ctx.playback.resample.profile_calls));
        max_us = lisa_resample_cycles_to_us(g_lisa_ctx.playback.resample.profile_max_cycles);

        LISA_LOGD(TAG,
                  "Resample profile: cur=%u cyc/%u us avg=%u us max=%u us slow=%u/%u in=%u out=%u",
                  cycles,
                  current_us,
                  average_us,
                  max_us,
                  g_lisa_ctx.playback.resample.profile_slow_calls,
                  g_lisa_ctx.playback.resample.profile_calls,
                  (unsigned int)input_size,
                  (unsigned int)output_size);
    }
}

static void lisa_playback_write_profile(uint32_t total_cycles,
                                        uint32_t driver_cycles,
                                        uint32_t resample_cycles,
                                        size_t input_size,
                                        size_t output_size)
{
    uint32_t total_us;
    uint32_t driver_us;
    uint32_t resample_us;
    uint32_t avg_total_us;
    uint32_t avg_driver_us;
    uint32_t max_total_us;
    uint32_t max_driver_us;

    g_lisa_ctx.playback.write.profile_calls++;
    g_lisa_ctx.playback.write.profile_total_cycles += total_cycles;
    g_lisa_ctx.playback.write.profile_driver_cycles += driver_cycles;

    if (total_cycles > g_lisa_ctx.playback.write.profile_max_total_cycles) {
        g_lisa_ctx.playback.write.profile_max_total_cycles = total_cycles;
    }
    if (driver_cycles > g_lisa_ctx.playback.write.profile_max_driver_cycles) {
        g_lisa_ctx.playback.write.profile_max_driver_cycles = driver_cycles;
    }

    total_us = lisa_resample_cycles_to_us(total_cycles);
    driver_us = lisa_resample_cycles_to_us(driver_cycles);
    resample_us = lisa_resample_cycles_to_us(resample_cycles);

    if (driver_us >= LISA_AUDIO_PLAYBACK_WRITE_SLOW_US) {
        g_lisa_ctx.playback.write.profile_slow_calls++;
    }

    if ((driver_us >= LISA_AUDIO_PLAYBACK_WRITE_SLOW_US) ||
        ((g_lisa_ctx.playback.write.profile_calls % LISA_AUDIO_PROFILE_LOG_INTERVAL) == 0U)) {
        avg_total_us = lisa_resample_cycles_to_us((uint32_t)(g_lisa_ctx.playback.write.profile_total_cycles /
                                                              g_lisa_ctx.playback.write.profile_calls));
        avg_driver_us = lisa_resample_cycles_to_us((uint32_t)(g_lisa_ctx.playback.write.profile_driver_cycles /
                                                               g_lisa_ctx.playback.write.profile_calls));
        max_total_us = lisa_resample_cycles_to_us(g_lisa_ctx.playback.write.profile_max_total_cycles);
        max_driver_us = lisa_resample_cycles_to_us(g_lisa_ctx.playback.write.profile_max_driver_cycles);

        LISA_LOGD(TAG,
                  "Playback write profile: total=%u us drv=%u us rs=%u us avg_total=%u us avg_drv=%u us max_total=%u us max_drv=%u us slow=%u/%u in=%u out=%u",
                  total_us,
                  driver_us,
                  resample_us,
                  avg_total_us,
                  avg_driver_us,
                  max_total_us,
                  max_driver_us,
                  g_lisa_ctx.playback.write.profile_slow_calls,
                  g_lisa_ctx.playback.write.profile_calls,
                  (unsigned int)input_size,
                  (unsigned int)output_size);
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
    uint32_t hardware_rate;
    bt_audio_error_t err;

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

    err = lisa_audio_playback_resample_prepare(&g_lisa_ctx.playback.resample.state, format);
    if (err != BT_AUDIO_OK) {
        return err;
    }

    hardware_rate = lisa_audio_playback_resample_output_rate(&g_lisa_ctx.playback.resample.state);
    
    if (g_lisa_ctx.playback.resample.state.enabled) {
        LISA_LOGI(TAG, "Opening playback: %u Hz, %u ch, %u bits, resample to %u Hz",
                  format->sample_rate, format->channels, format->bits_per_sample, hardware_rate);
    } else {
        LISA_LOGI(TAG, "Opening playback: %u Hz, %u ch, %u bits",
                  format->sample_rate, format->channels, format->bits_per_sample);
    }

    /* 配置播放参数 */
    lisa_audio_play_config_t play_config = {
        .format = {
            .sample_rate = convert_sample_rate(hardware_rate),
            .channels = convert_channels(format->channels),
            .sample_bits = convert_bits(format->bits_per_sample),
        },
        .gain = {
            .analog_gain = 0,
            .digital_gain = 0,
        },
        .buffer_count = CONFIG_LISA_BT_AUDIO_HW_BUFFER_COUNT,
        .buffer_samples = lisa_audio_playback_resample_buffer_samples(&g_lisa_ctx.playback.resample.state,
                                                                      CONFIG_LISA_BT_AUDIO_WORK_BUFFER_TIME_MS),
    };
    
    int ret = lisa_audio_play_config(g_lisa_ctx.audio_dev, &play_config);
    if (ret != 0) {
        lisa_playback_resample_reset();
        return BT_AUDIO_ERR_INTERFACE_FAILED;
    }
    
    /* 启动播放 */
    ret = lisa_audio_play_start(g_lisa_ctx.audio_dev);
    if (ret != 0) {
        lisa_playback_resample_reset();
        return BT_AUDIO_ERR_INTERFACE_FAILED;
    }
    
    /* 保存配置 */
    memcpy(&g_lisa_ctx.playback.format, format, sizeof(bt_audio_format_t));
    g_lisa_ctx.playback.is_open = true;
    g_lisa_ctx.playback.started = true;

    if (g_lisa_ctx.playback.resample.state.enabled) {
        LISA_LOGI(TAG, "Playback opened: %u Hz, %u ch, %u bits, hw %u Hz",
                  format->sample_rate, format->channels, format->bits_per_sample, hardware_rate);
    } else {
        LISA_LOGI(TAG, "Playback opened: %u Hz, %u ch, %u bits",
                  format->sample_rate, format->channels, format->bits_per_sample);
    }
    return BT_AUDIO_OK;
}

static int lisa_playback_write(const void *data, size_t size)
{
    const void *write_data = data;
    size_t write_size = size;
    uint32_t total_start_cycles;
    uint32_t total_cycles;
    uint32_t resample_cycles = 0U;
    uint32_t driver_cycles;
    uint32_t start_cycles;

    if (!g_lisa_ctx.playback.is_open) {
        return -1;
    }
    
    if (!data || size == 0) {
        return 0;
    }

    total_start_cycles = (uint32_t)__get_rv_cycle();

    if (g_lisa_ctx.playback.resample.state.enabled) {
        size_t frame_bytes = g_lisa_ctx.playback.resample.state.resampler.channels * sizeof(int16_t);
        size_t input_frames;
        bt_audio_error_t err;

        if ((size % frame_bytes) != 0U) {
            LISA_LOGE(TAG, "Playback data is not frame-aligned for 44.1k resample: %u",
                      (unsigned int)size);
            return -BT_AUDIO_ERR_INVALID_PARAM;
        }

        uint32_t elapsed_cycles;

        input_frames = size / frame_bytes;
        err = lisa_playback_resample_ensure_capacity(input_frames);
        if (err != BT_AUDIO_OK) {
            return -(int)err;
        }

        start_cycles = (uint32_t)__get_rv_cycle();
        write_size = lisa_audio_resampler_process(&g_lisa_ctx.playback.resample.state.resampler,
                                                  (const int16_t *)data,
                                                  input_frames,
                                                  g_lisa_ctx.playback.resample.buffer,
                                                  g_lisa_ctx.playback.resample.buffer_capacity_samples);
        elapsed_cycles = (uint32_t)(__get_rv_cycle() - start_cycles);
        resample_cycles = elapsed_cycles;
        lisa_playback_resample_profile(elapsed_cycles, size, write_size);
        write_data = g_lisa_ctx.playback.resample.buffer;

        if (write_size == 0U) {
            return (int)size;
        }
    }
    
    /* 计算采样点数（所有通道总和）*/
    uint32_t samples = write_size / (g_lisa_ctx.playback.format.bits_per_sample / 8);

    start_cycles = (uint32_t)__get_rv_cycle();
    int ret = lisa_audio_play_write(g_lisa_ctx.audio_dev, write_data, samples);
    driver_cycles = (uint32_t)(__get_rv_cycle() - start_cycles);
    total_cycles = (uint32_t)(__get_rv_cycle() - total_start_cycles);
    if (ret < 0) {
        LISA_LOGE(TAG, "lisa_audio_play_write failed: %d", ret);
        return ret;
    }

    lisa_playback_write_profile(total_cycles, driver_cycles, resample_cycles, size, write_size);

    return (int)size;
}

static bt_audio_error_t lisa_playback_close(void)
{
    if (!g_lisa_ctx.playback.is_open) {
        lisa_playback_resample_reset();
        return BT_AUDIO_OK;
    }
    
    /* 停止播放 */
    lisa_audio_play_stop(g_lisa_ctx.audio_dev);
    
    g_lisa_ctx.playback.is_open = false;
    g_lisa_ctx.playback.started = false;
    g_lisa_ctx.playback.callback = NULL;
    g_lisa_ctx.playback.user_data = NULL;
    lisa_playback_resample_reset();
    
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

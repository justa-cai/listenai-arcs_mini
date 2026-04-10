/**
 * @file null_audio_interface.c
 * @brief 空音频接口实现（用于纯蓝牙传输场景）
 * 
 * 职责：
 * ========================================================================
 * 提供空操作的音频接口实现，用于不需要本地音频输入输出的场景。
 * 
 * 适用场景：
 * ========================================================================
 * 1. **纯Source模式**：
 *    - 只通过蓝牙发送音频，不需要本地扬声器播放
 *    - 例如：蓝牙音源设备，仅将音频流发送到耳机
 * 
 * 2. **纯Sink模式**：
 *    - 只通过蓝牙接收音频，不需要本地麦克风采集
 *    - 例如：蓝牙音箱，仅接收手机音频播放
 * 
 * 3. **测试场景**：
 *    - 用于测试framework而不依赖实际硬件
 * 
 * 实现说明：
 * ========================================================================
 * - playback_write(): 丢弃所有写入的数据（假装播放）
 * - capture_read(): 返回0或静音数据（假装录音）
 * - 其他操作都返回成功，但不做实际操作
 * 
 * Copyright (C) ListenAI 2025
 */

#include "bt_audio_interface.h"
#include "lisa_log.h"
#include <string.h>

#define TAG "NULL_AUDIO_IF"

/* 空接口上下文 */
static struct {
    bool playback_open;
    bool capture_open;
    bt_audio_format_t playback_format;
    bt_audio_format_t capture_format;
    uint8_t volume;
} g_null_ctx = {
    .volume = 50,
};

/* ========================================================================
 * 播放接口实现（丢弃所有数据）
 * ======================================================================== */

static bt_audio_error_t null_playback_open(const bt_audio_format_t *format)
{
    if (!format) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    if (g_null_ctx.playback_open) {
        return BT_AUDIO_ERR_BUSY;
    }
    
    LISA_LOGI(TAG, "Opening null playback: %uHz, %uch, %ubits",
              format->sample_rate, format->channels, format->bits_per_sample);
    
    memcpy(&g_null_ctx.playback_format, format, sizeof(bt_audio_format_t));
    g_null_ctx.playback_open = true;
    
    return BT_AUDIO_OK;
}

static int null_playback_write(const void *data, size_t size)
{
    if (!g_null_ctx.playback_open) {
        return -BT_AUDIO_ERR_NOT_INITIALIZED;
    }
    
    /* 假装播放：丢弃所有数据 */
    return (int)size;
}

static bt_audio_error_t null_playback_close(void)
{
    if (!g_null_ctx.playback_open) {
        return BT_AUDIO_OK;
    }
    
    LISA_LOGI(TAG, "Closing null playback");
    g_null_ctx.playback_open = false;
    
    return BT_AUDIO_OK;
}

static bt_audio_error_t null_playback_pause(void)
{
    return BT_AUDIO_OK;
}

static bt_audio_error_t null_playback_resume(void)
{
    return BT_AUDIO_OK;
}

/* ========================================================================
 * 录音接口实现（返回静音或零数据）
 * ======================================================================== */

static bt_audio_error_t null_capture_open(const bt_audio_format_t *format,
                                          bt_audio_capture_callback_t callback,
                                          void *user_data)
{
    if (!format) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    if (g_null_ctx.capture_open) {
        return BT_AUDIO_ERR_BUSY;
    }
    
    LISA_LOGI(TAG, "Opening null capture: %uHz, %uch, %ubits",
              format->sample_rate, format->channels, format->bits_per_sample);
    
    memcpy(&g_null_ctx.capture_format, format, sizeof(bt_audio_format_t));
    g_null_ctx.capture_open = true;
    
    /* 注意：不会主动调用callback，因为没有实际数据产生 */
    (void)callback;
    (void)user_data;
    
    return BT_AUDIO_OK;
}

static int null_capture_read(void *buffer, size_t size)
{
    if (!g_null_ctx.capture_open) {
        return -BT_AUDIO_ERR_NOT_INITIALIZED;
    }
    
    if (!buffer || size == 0) {
        return 0;
    }
    
    /* 返回静音数据（全0） */
    memset(buffer, 0, size);
    return (int)size;
}

static bt_audio_error_t null_capture_close(void)
{
    if (!g_null_ctx.capture_open) {
        return BT_AUDIO_OK;
    }
    
    LISA_LOGI(TAG, "Closing null capture");
    g_null_ctx.capture_open = false;
    
    return BT_AUDIO_OK;
}

/* ========================================================================
 * 音量控制（空操作）
 * ======================================================================== */

static bt_audio_error_t null_set_volume(uint8_t volume)
{
    if (volume > 100) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    g_null_ctx.volume = volume;
    return BT_AUDIO_OK;
}

static bt_audio_error_t null_get_volume(uint8_t *volume)
{
    if (!volume) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }
    
    *volume = g_null_ctx.volume;
    return BT_AUDIO_OK;
}

/* ========================================================================
 * 接口操作表
 * ======================================================================== */

static const bt_audio_interface_ops_t null_audio_interface_ops = {
    .name = "null_audio",
    .playback_open = null_playback_open,
    .playback_write = null_playback_write,
    .playback_close = null_playback_close,
    .playback_pause = null_playback_pause,
    .playback_resume = null_playback_resume,
    .capture_open = null_capture_open,
    .capture_read = null_capture_read,
    .capture_close = null_capture_close,
    .set_volume = null_set_volume,
    .get_volume = null_get_volume,
};

/* ========================================================================
 * 注册接口
 * ======================================================================== */

/**
 * @brief 注册空音频接口
 * 
 * 注册一个不执行实际操作的音频接口。
 * 
 * 使用场景：
 * - 纯蓝牙音频传输，不需要本地播放/录音
 * - 测试和开发，不依赖实际硬件
 * 
 * @return BT_AUDIO_OK: 成功, 其他: 错误码
 */
bt_audio_error_t bt_audio_interface_null_register(void)
{
    LISA_LOGI(TAG, "Registering null audio interface");
    
    memset(&g_null_ctx, 0, sizeof(g_null_ctx));
    g_null_ctx.volume = 50;
    
    return bt_audio_interface_register(&null_audio_interface_ops);
}

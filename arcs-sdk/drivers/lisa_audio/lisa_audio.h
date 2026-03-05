/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lisa_audio.h
 * @brief LISA Audio 设备驱动接口
 *
 * 基于 lisa_device 框架的音频设备驱动，提供统一的音频录音(Record)和播放(Play)接口。
 * 支持音频数据回调、增益控制、相位补偿等功能。
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "lisa_device.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Audio 类型定义
 * ======================================================================== */

/**
 * @brief 音频设备状态
 */
typedef enum {
    LISA_AUDIO_STATUS_IDLE = 0,       /* 空闲状态 */
    LISA_AUDIO_STATUS_RUNNING,        /* 运行中 */
    LISA_AUDIO_STATUS_PAUSED,         /* 已暂停 */
    LISA_AUDIO_STATUS_ERROR,          /* 错误状态 */
} lisa_audio_status_t;

/**
 * @brief 音频采样率
 */
typedef enum {
    LISA_AUDIO_RATE_8K = 8000,        /* 8kHz 采样率 */
    LISA_AUDIO_RATE_16K = 16000,      /* 16kHz 采样率 */
    LISA_AUDIO_RATE_24K = 24000,      /* 24kHz 采样率 */
    LISA_AUDIO_RATE_32K = 32000,      /* 32kHz 采样率 */
    LISA_AUDIO_RATE_48K = 48000,      /* 48kHz 采样率 */
    LISA_AUDIO_RATE_96K = 96000,      /* 96kHz 采样率 */
} lisa_audio_rate_t;

/**
 * @brief 音频通道配置
 */
typedef enum {
    LISA_AUDIO_CH_LEFT = 0x01,        /* 左声道 */
    LISA_AUDIO_CH_RIGHT = 0x02,       /* 右声道 */
    LISA_AUDIO_CH_STEREO = 0x03,      /* 立体声（左+右） */
} lisa_audio_channel_t;

/**
 * @brief 音频采样位深
 */
typedef enum {
    LISA_AUDIO_BIT_16 = 2,            /* 16位采样 (2字节) */
    LISA_AUDIO_BIT_24 = 3,            /* 24位采样 (3字节) */
    LISA_AUDIO_BIT_32 = 4,            /* 32位采样 (4字节) */
} lisa_audio_bits_t;

/**
 * @brief 音频格式配置
 */
typedef struct {
    lisa_audio_rate_t sample_rate;    /* 采样率 */
    lisa_audio_channel_t channels;    /* 通道配置 */
    lisa_audio_bits_t sample_bits;    /* 采样位深 */
} lisa_audio_format_t;

/**
 * @brief 音频增益配置
 */
typedef struct {
    int8_t analog_gain;               /* 模拟增益 (dB) */
    int8_t digital_gain;              /* 数字增益 (dB) */
} lisa_audio_gain_t;

/**
 * @brief 音频事件结构
 *
 * 用于统一回调函数中传递音频数据事件，包含录音数据和回声数据
 */
typedef struct {
    const void *record_buffer;    /* 录音数据缓冲区，无数据时为 NULL */
    const void *echo_buffer;      /* 播放回声数据缓冲区，无数据时为 NULL */
    uint32_t record_samples;      /* 录音采样点数 */
    uint32_t echo_samples;        /* 回声采样点数 */
} lisa_audio_event_t;

/**
 * @brief 相位补偿配置
 *
 * 用于调整录音和播音之间的相位差
 */
typedef struct {
    uint16_t record_skip_samples; /* 录音数据丢弃的采样点数 */
    uint16_t echo_skip_samples;   /* ECHO数据丢弃的采样点数 */
} lisa_audio_phase_compensation_t;

/**
 * @brief 音频数据回调函数类型
 *
 * 统一的音频事件回调，用于接收录音和播放回声数据
 *
 * @param event 音频事件结构，包含数据缓冲区指针
 * @param user_data 用户自定义数据
 *
 * @note 回调函数在中断上下文中执行，应尽量简短快速
 */
typedef void (*lisa_audio_callback_t)(const lisa_audio_event_t *event, void *user_data);

/**
 * @brief 录音配置结构
 */
typedef struct {
    lisa_audio_format_t format;       /* 音频格式（采样率、通道、位深） */
#ifdef CONFIG_LISA_AUDIO_RECORD_INDIVIDUAL_GAIN
    lisa_audio_gain_t gain_l;         /* 左声道增益配置 */
    lisa_audio_gain_t gain_r;         /* 右声道增益配置 */
#else
    lisa_audio_gain_t gain;           /* 增益配置（模拟增益、数字增益） */
#endif
    bool enable_hpf;                  /* 启用高通滤波器 */
    bool differential_input;          /* 启用差分输入 */
} lisa_audio_record_config_t;

/**
 * @brief 播放配置结构
 */
typedef struct {
    lisa_audio_format_t format;       /* 音频格式（采样率、通道、位深） */
    lisa_audio_gain_t gain;           /* 增益配置（模拟增益、数字增益） */
    uint8_t buffer_count;             /* 缓冲区数量 */
    uint16_t buffer_samples;          /* 每个缓冲区采样点数 */
} lisa_audio_play_config_t;

/**
 * @brief IOCTL 命令枚举
 */
typedef enum {
    /* Record 命令 (0x00 ~ 0x1F) */
    LISA_AUDIO_IOCTL_RECORD_START = 0x00,       /* 启动录音 */
    LISA_AUDIO_IOCTL_RECORD_STOP = 0x01,        /* 停止录音 */
    LISA_AUDIO_IOCTL_RECORD_PAUSE = 0x02,       /* 暂停录音 */
    LISA_AUDIO_IOCTL_RECORD_RESUME = 0x03,      /* 恢复录音 */
    LISA_AUDIO_IOCTL_RECORD_RESET = 0x04,       /* 重置录音 */
    LISA_AUDIO_IOCTL_RECORD_SET_GAIN = 0x05,    /* 设置录音增益 */
    LISA_AUDIO_IOCTL_RECORD_GET_STATUS = 0x06,  /* 获取录音状态 */
    LISA_AUDIO_IOCTL_RECORD_GET_CONFIG = 0x07,  /* 获取录音配置 */
    LISA_AUDIO_IOCTL_RECORD_SET_CONFIG = 0x08,  /* 设置录音配置 */

    /* Play 命令 (0x20 ~ 0x3F) */
    LISA_AUDIO_IOCTL_PLAY_START = 0x20,         /* 启动播放 */
    LISA_AUDIO_IOCTL_PLAY_STOP = 0x21,          /* 停止播放 */
    LISA_AUDIO_IOCTL_PLAY_SET_GAIN = 0x22,      /* 设置播放增益 */
    LISA_AUDIO_IOCTL_PLAY_GET_STATUS = 0x23,    /* 获取播放状态 */
    LISA_AUDIO_IOCTL_PLAY_GET_CONFIG = 0x24,    /* 获取播放配置 */
    LISA_AUDIO_IOCTL_PLAY_SET_CONFIG = 0x25,    /* 设置播放配置 */
    LISA_AUDIO_IOCTL_PLAY_GET_BUFFER = 0x26,    /* 获取播放缓冲区 */
    LISA_AUDIO_IOCTL_PLAY_FLUSH = 0x27,         /* 等待播放完成 */

    /* 通用命令 (0x40 ~ 0x5F) */
    LISA_AUDIO_IOCTL_SET_PHASE_COMPENSATION = 0x40, /* 设置相位补偿 */
    LISA_AUDIO_IOCTL_GET_PHASE_COMPENSATION = 0x41, /* 获取相位补偿 */
} lisa_audio_ioctl_cmd_t;

/* ========================================================================
 * Audio 设备 API 结构体
 * ======================================================================== */

/**
 * @brief Audio 设备 API 函数指针结构
 */
typedef struct {
    /* 统一回调 */
    int (*register_callback)(lisa_device_t *dev, lisa_audio_callback_t callback, void *user_data);
    int (*unregister_callback)(lisa_device_t *dev, lisa_audio_callback_t callback);

    /* Record 操作 */
    int (*record_config)(lisa_device_t *dev, const lisa_audio_record_config_t *config);
    int (*record_control)(lisa_device_t *dev, uint32_t cmd, void *arg);

    /* Play 操作 */
    int (*play_config)(lisa_device_t *dev, const lisa_audio_play_config_t *config);
    int (*play_write)(lisa_device_t *dev, const void *buffer, uint32_t samples);
    int (*play_get_buffer)(lisa_device_t *dev, void **buffer, uint32_t timeout_ms);
    int (*play_control)(lisa_device_t *dev, uint32_t cmd, void *arg);

    /* 通用操作 */
    int (*ioctl)(lisa_device_t *dev, uint8_t cmd, void *arg);
} lisa_audio_api_t;

/* ========================================================================
 * Audio 对外接口函数
 * ======================================================================== */

/* ===== 回调管理接口 ===== */

/**
 * @brief 注册统一音频数据回调
 *
 * 注册回调函数以接收音频事件，包括录音数据和播放回声数据。
 * 回调函数会在中断上下文中执行。
 *
 * @param dev Audio设备指针
 * @param callback 回调函数指针
 * @param user_data 用户自定义数据，将传递给回调函数
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 *
 * @note 回调函数在中断上下文中执行，应尽量简短快速
 */
static inline int lisa_audio_register_callback(lisa_device_t *dev,
                                               lisa_audio_callback_t callback,
                                               void *user_data)
{
    if (!dev || !dev->api || !callback) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_audio_api_t *api = (lisa_audio_api_t *)dev->api;
    if (!api->register_callback) {
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }
    return api->register_callback(dev, callback, user_data);
}

/**
 * @brief 注销统一音频数据回调
 *
 * @param dev Audio设备指针
 * @param callback 要注销的回调函数指针
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_audio_unregister_callback(lisa_device_t *dev, lisa_audio_callback_t callback)
{
    if (!dev || !dev->api || !callback) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_audio_api_t *api = (lisa_audio_api_t *)dev->api;
    if (!api->unregister_callback) {
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }
    return api->unregister_callback(dev, callback);
}


/* ===== Record 配置接口 ===== */

/**
 * @brief 配置录音参数
 *
 * 配置录音的音频格式、增益、滤波器等参数。
 *
 * @param dev Audio设备指针
 * @param config 录音配置参数结构体指针，包含：
 *               - format: 音频格式（采样率、通道、位深）
 *               - gain: 增益配置（模拟增益、数字增益）
 *               - enable_hpf: 是否启用高通滤波器
 *               - differential_input: 是否启用差分输入
 *               - buffer_count: 缓冲区数量
 *               - buffer_samples: 每个缓冲区采样点数
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 *
 * @note 配置前请确保设备已初始化
 */
static inline int lisa_audio_record_config(lisa_device_t *dev, const lisa_audio_record_config_t *config)
{
    if (!dev || !dev->api || !config) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_audio_api_t *api = (lisa_audio_api_t *)dev->api;
    if (!api->record_config) {
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }
    return api->record_config(dev, config);
}

/* ===== Record 控制接口 ===== */

/**
 * @brief 启动录音
 *
 * 启动音频录音功能，开始采集音频数据。
 *
 * @param dev Audio设备指针
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 *
 * @note 启动前需先调用 lisa_audio_record_config() 配置录音参数
 */
static inline int lisa_audio_record_start(lisa_device_t *dev)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_audio_api_t *api = (lisa_audio_api_t *)dev->api;
    if (!api->record_control) {
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }
    return api->record_control(dev, LISA_AUDIO_IOCTL_RECORD_START, NULL);
}

/**
 * @brief 停止录音
 *
 * 停止音频录音功能。
 *
 * @param dev Audio设备指针
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_audio_record_stop(lisa_device_t *dev)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_audio_api_t *api = (lisa_audio_api_t *)dev->api;
    if (!api->record_control) {
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }
    return api->record_control(dev, LISA_AUDIO_IOCTL_RECORD_STOP, NULL);
}

/**
 * @brief 暂停录音
 *
 * 暂停音频录音，可通过 lisa_audio_record_resume() 恢复。
 *
 * @param dev Audio设备指针
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_audio_record_pause(lisa_device_t *dev)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_audio_api_t *api = (lisa_audio_api_t *)dev->api;
    if (!api->record_control) {
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }
    return api->record_control(dev, LISA_AUDIO_IOCTL_RECORD_PAUSE, NULL);
}

/**
 * @brief 恢复录音
 *
 * 恢复之前暂停的音频录音。
 *
 * @param dev Audio设备指针
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_audio_record_resume(lisa_device_t *dev)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_audio_api_t *api = (lisa_audio_api_t *)dev->api;
    if (!api->record_control) {
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }
    return api->record_control(dev, LISA_AUDIO_IOCTL_RECORD_RESUME, NULL);
}

/**
 * @brief 设置录音增益
 *
 * 动态调整录音的模拟和数字增益。
 *
 * @param dev Audio设备指针
 * @param gain 增益配置结构体指针，包含模拟增益和数字增益（单位：dB）
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_audio_record_set_gain(lisa_device_t *dev, const lisa_audio_gain_t *gain)
{
    if (!dev || !dev->api || !gain) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_audio_api_t *api = (lisa_audio_api_t *)dev->api;
    if (!api->record_control) {
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }
    return api->record_control(dev, LISA_AUDIO_IOCTL_RECORD_SET_GAIN, (void *)gain);
}

/* ===== Play 配置接口 ===== */

/**
 * @brief 配置播放参数
 *
 * 配置播放的音频格式、增益等参数。
 *
 * @param dev Audio设备指针
 * @param config 播放配置参数结构体指针，包含：
 *               - format: 音频格式（采样率、通道、位深）
 *               - gain: 增益配置（模拟增益、数字增益）
 *               - buffer_count: 缓冲区数量
 *               - buffer_samples: 每个缓冲区采样点数
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 *
 * @note 配置前请确保设备已初始化
 */
static inline int lisa_audio_play_config(lisa_device_t *dev, const lisa_audio_play_config_t *config)
{
    if (!dev || !dev->api || !config) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_audio_api_t *api = (lisa_audio_api_t *)dev->api;
    if (!api->play_config) {
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }
    return api->play_config(dev, config);
}

/* ===== Play 数据传输接口 ===== */

/**
 * @brief 写入播放数据
 *
 * 向播放设备写入音频数据。
 *
 * @param dev Audio设备指针
 * @param buffer 音频数据缓冲区指针
 * @param samples 采样点数（所有通道总和）
 *
 * @return >=0 实际写入的采样点数
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 *
 * @note 写入前需先调用 lisa_audio_play_config() 配置播放参数
 */
static inline int lisa_audio_play_write(lisa_device_t *dev, const void *buffer, uint32_t samples)
{
    if (!dev || !dev->api || !buffer) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_audio_api_t *api = (lisa_audio_api_t *)dev->api;
    if (!api->play_write) {
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }
    return api->play_write(dev, buffer, samples);
}

/**
 * @brief 获取播放空闲缓冲区
 *
 * 获取一个可用的播放缓冲区指针，用于直接写入音频数据。
 *
 * @param dev Audio设备指针
 * @param buffer 输出参数，用于接收缓冲区指针
 * @param timeout_ms 超时时间（毫秒）
 *
 * @return >=0 缓冲区大小（采样点数）
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return LISA_DEVICE_ERR_TIMEOUT 获取超时
 * @return <0 其他错误
 *
 * @note 获取缓冲区后需要调用 lisa_audio_play_write() 提交数据
 */
static inline int lisa_audio_play_get_buffer(lisa_device_t *dev, void **buffer, uint32_t timeout_ms)
{
    if (!dev || !dev->api || !buffer) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_audio_api_t *api = (lisa_audio_api_t *)dev->api;
    if (!api->play_get_buffer) {
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }
    return api->play_get_buffer(dev, buffer, timeout_ms);
}

/* ===== Play 控制接口 ===== */

/**
 * @brief 启动播放
 *
 * 启动音频播放功能，开始输出音频数据。
 *
 * @param dev Audio设备指针
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 *
 * @note 启动前需先调用 lisa_audio_play_config() 配置播放参数
 */
static inline int lisa_audio_play_start(lisa_device_t *dev)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_audio_api_t *api = (lisa_audio_api_t *)dev->api;
    if (!api->play_control) {
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }
    return api->play_control(dev, LISA_AUDIO_IOCTL_PLAY_START, NULL);
}

/**
 * @brief 停止播放
 *
 * 停止音频播放功能。
 *
 * @param dev Audio设备指针
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_audio_play_stop(lisa_device_t *dev)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_audio_api_t *api = (lisa_audio_api_t *)dev->api;
    if (!api->play_control) {
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }
    return api->play_control(dev, LISA_AUDIO_IOCTL_PLAY_STOP, NULL);
}

/**
 * @brief 等待播放完成
 *
 * 阻塞等待，直到所有已写入的音频数据播放完毕。
 *
 * @param dev Audio设备指针
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 *
 * @note 此函数会阻塞直到播放完成
 */
static inline int lisa_audio_play_flush(lisa_device_t *dev)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_audio_api_t *api = (lisa_audio_api_t *)dev->api;
    if (!api->play_control) {
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }
    return api->play_control(dev, LISA_AUDIO_IOCTL_PLAY_FLUSH, NULL);
}

/**
 * @brief 设置播放增益
 *
 * 动态调整播放的模拟和数字增益。
 *
 * @param dev Audio设备指针
 * @param gain 增益配置结构体指针，包含模拟增益和数字增益（单位：dB）
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_audio_play_set_gain(lisa_device_t *dev, const lisa_audio_gain_t *gain)
{
    if (!dev || !dev->api || !gain) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_audio_api_t *api = (lisa_audio_api_t *)dev->api;
    if (!api->play_control) {
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }
    return api->play_control(dev, LISA_AUDIO_IOCTL_PLAY_SET_GAIN, (void *)gain);
}

/* ===== 通用控制接口 ===== */

/**
 * @brief 设置相位补偿
 *
 * 配置录音和播放之间的相位补偿，用于调整数据同步。
 *
 * @param dev Audio设备指针
 * @param compensation 相位补偿配置结构体指针，包含：
 *                     - record_skip_samples: 录音数据丢弃的采样点数
 *                     - echo_skip_samples: ECHO数据丢弃的采样点数
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_audio_set_phase_compensation(lisa_device_t *dev,
                                                    const lisa_audio_phase_compensation_t *compensation)
{
    if (!dev || !dev->api || !compensation) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_audio_api_t *api = (lisa_audio_api_t *)dev->api;
    if (!api->ioctl) {
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }
    return api->ioctl(dev, LISA_AUDIO_IOCTL_SET_PHASE_COMPENSATION, (void *)compensation);
}

/**
 * @brief 获取相位补偿
 *
 * 读取当前的相位补偿配置。
 *
 * @param dev Audio设备指针
 * @param compensation 输出参数，用于接收相位补偿配置
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_audio_get_phase_compensation(lisa_device_t *dev,
                                                    lisa_audio_phase_compensation_t *compensation)
{
    if (!dev || !dev->api || !compensation) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_audio_api_t *api = (lisa_audio_api_t *)dev->api;
    if (!api->ioctl) {
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }
    return api->ioctl(dev, LISA_AUDIO_IOCTL_GET_PHASE_COMPENSATION, (void *)compensation);
}

/**
 * @brief 通用 IOCTL 接口
 *
 * 提供通用的设备控制接口，用于执行各种控制命令。
 *
 * @param dev Audio设备指针
 * @param cmd 控制命令，参见 lisa_audio_ioctl_cmd_t
 * @param arg 命令参数，根据具体命令而定
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_audio_ioctl(lisa_device_t *dev, uint8_t cmd, void *arg)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_audio_api_t *api = (lisa_audio_api_t *)dev->api;
    if (!api->ioctl) {
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }
    return api->ioctl(dev, cmd, arg);
}

#ifdef __cplusplus
}
#endif

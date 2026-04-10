/**
 * @file bt_audio_types.h
 * @brief 蓝牙音频框架公共类型定义
 * 
 * Copyright (C) ListenAI 2025
 */

#ifndef BT_AUDIO_TYPES_H_
#define BT_AUDIO_TYPES_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 错误码定义
 * ======================================================================== */
typedef enum {
    BT_AUDIO_OK = 0,                    /* 成功 */
    BT_AUDIO_ERR_INVALID_PARAM,         /* 无效参数 */
    BT_AUDIO_ERR_NO_MEMORY,             /* 内存不足 */
    BT_AUDIO_ERR_NOT_FOUND,             /* 未找到 */
    BT_AUDIO_ERR_ALREADY_EXISTS,        /* 已存在 */
    BT_AUDIO_ERR_NOT_INITIALIZED,       /* 未初始化 */
    BT_AUDIO_ERR_NOT_SUPPORTED,         /* 不支持 */
    BT_AUDIO_ERR_BUSY,                  /* 忙碌中 */
    BT_AUDIO_ERR_TIMEOUT,               /* 超时 */
    BT_AUDIO_ERR_CODEC_FAILED,          /* 编解码失败 */
    BT_AUDIO_ERR_INTERFACE_FAILED,      /* 音频接口失败 */
    BT_AUDIO_ERR_INVALID_STATE,         /* 无效状态 */
    BT_AUDIO_ERR_DEVICE_OPEN,           /* 设备打开失败 */
} bt_audio_error_t;

/* ========================================================================
 * Codec类型
 * ======================================================================== */
typedef enum {
    BT_CODEC_NONE = 0,
    BT_CODEC_PCM,                       /* PCM透传模式（不编解码） */
    BT_CODEC_SBC,                       /* SubBand Codec (A2DP) */
    BT_CODEC_LC3,                       /* Low Complexity Communication Codec (LE Audio) */
    BT_CODEC_MSBC,                      /* Modified SBC (HFP WBS) */
    BT_CODEC_CVSD,                      /* Continuous Variable Slope Delta (HFP NBS) */
} bt_audio_codec_type_t;

/* ========================================================================
 * 音频格式
 * ======================================================================== */
typedef struct {
    uint32_t sample_rate;               /* 采样率 (Hz): 8000, 16000, 48000... */
    uint8_t channels;                   /* 声道数: 1=单声道, 2=立体声 */
    uint8_t bits_per_sample;            /* 采样位深: 16, 24, 32 */
} bt_audio_format_t;

/* ========================================================================
 * Codec配置
 * ======================================================================== */
typedef struct {
    bt_audio_codec_type_t codec_type;   /* Codec类型 */
    bt_audio_format_t format;           /* 音频格式 */
    uint32_t bitrate;                   /* 比特率 (bps) */
    uint32_t frame_duration_us;         /* 帧时长 (微秒) - 使用 uint32_t 避免溢出 */
    uint32_t frame_size_bytes;          /* 每帧大小 (字节) */
    void *codec_specific_config;        /* Codec特定配置 */
    size_t config_size;                 /* 配置大小 */
} bt_audio_codec_config_t;

/* ========================================================================
 * 音频方向
 * ======================================================================== */
typedef enum {
    BT_AUDIO_DIR_PLAYBACK = 0,          /* 播放方向 (解码) */
    BT_AUDIO_DIR_CAPTURE,               /* 录音方向 (编码) */
} bt_audio_direction_t;

/* ========================================================================
 * 音频事件
 * ======================================================================== */
typedef enum {
    BT_AUDIO_EVENT_STARTED,             /* 会话已启动 */
    BT_AUDIO_EVENT_STOPPED,             /* 会话已停止 */
    BT_AUDIO_EVENT_UNDERRUN,            /* 播放缓冲下溢 */
    BT_AUDIO_EVENT_OVERRUN,             /* 录音缓冲溢出 */
    BT_AUDIO_EVENT_ERROR,               /* 发生错误 */
} bt_audio_event_type_t;

typedef struct {
    bt_audio_event_type_t event;        /* 事件类型 */
    void *data;                         /* 事件数据 */
    size_t data_len;                    /* 数据长度 */
} bt_audio_event_t;

/* ========================================================================
 * 音频缓冲区
 * ======================================================================== */
typedef struct {
    uint8_t *data;                      /* 数据指针 */
    size_t size;                        /* 缓冲区大小 */
    size_t used;                        /* 已使用大小 */
    uint32_t timestamp;                 /* 时间戳 */
} bt_audio_buffer_t;

/* ========================================================================
 * 编码帧信息
 * ======================================================================== */
typedef struct {
    uint16_t frame_size;                /* 帧长度 */
} bt_audio_frame_info_t;

/* ========================================================================
 * 统计信息
 * ======================================================================== */
typedef struct {
    uint32_t total_frames;              /* 总帧数 */
    uint32_t dropped_frames;            /* 丢弃帧数 */
    uint32_t error_frames;              /* 错误帧数 */
    uint32_t bytes_processed;           /* 已处理字节数 */
} bt_audio_stats_t;

#ifdef __cplusplus
}
#endif

#endif /* BT_AUDIO_TYPES_H_ */

/**
 * @file xz_config.h
 * @brief 小智云端编译时配置
 */

#ifndef __XZ_CONFIG_H__
#define __XZ_CONFIG_H__

/**
 * @brief 启用小智云端协议
 * @note 在 prj.conf 或编译选项中定义 XIAOZHI_CLOUD 来启用
 */

#ifdef XIAOZHI_CLOUD

/* 默认服务器配置 */
#ifndef XZ_SERVER_URL
#define XZ_SERVER_URL            "wss://api.tenclass.net/xiaozhi/v1/"
#endif

#ifndef XZ_DEVICE_ID
#define XZ_DEVICE_ID             "arcs_mini"
#endif

#ifndef XZ_CLIENT_ID
#define XZ_CLIENT_ID             "default"
#endif

/* 音频配置 */
#ifndef XZ_AUDIO_SAMPLE_RATE
#define XZ_AUDIO_SAMPLE_RATE     16000    /* 采样率 (Hz) */
#endif

#ifndef XZ_AUDIO_CHANNELS
#define XZ_AUDIO_CHANNELS        1        /* 声道数 */
#endif

#ifndef XZ_AUDIO_FRAME_MS
#define XZ_AUDIO_FRAME_MS        60       /* 帧时长 (ms) */
#endif

#ifndef XZ_AUDIO_BITRATE
#define XZ_AUDIO_BITRATE         24000    /* Opus 比特率 (bps) */
#endif

/* 网络配置 */
#ifndef XZ_TIMEOUT_MS
#define XZ_TIMEOUT_MS            10000    /* 连接超时 (ms) */
#endif

/* 调试配置 */
#ifndef XZ_DEBUG_LOG
#define XZ_DEBUG_LOG             1        /* 启用调试日志 */
#endif

#endif /* XIAOZHI_CLOUD */

#endif /* __XZ_CONFIG_H__ */

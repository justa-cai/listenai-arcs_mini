/**
 * @file bt_audio_codec.h
 * @brief 蓝牙音频Codec抽象接口
 * 
 * Copyright (C) ListenAI 2025
 */

#ifndef BT_AUDIO_CODEC_H_
#define BT_AUDIO_CODEC_H_

#include "bt_audio_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Codec操作接口
 * ======================================================================== */

/**
 * @brief Codec操作接口结构体
 * 
 * 所有codec适配器必须实现此接口
 */
typedef struct bt_audio_codec_ops {
    const char *name;                   /* Codec名称 */
    bt_audio_codec_type_t type;         /* Codec类型 */
    
    /**
     * @brief 初始化codec
     * @param config codec配置
     * @return BT_AUDIO_OK: 成功, 其他: 错误码
     */
    bt_audio_error_t (*init)(const bt_audio_codec_config_t *config);
    
    /**
     * @brief 去初始化codec
     * @return BT_AUDIO_OK: 成功, 其他: 错误码
     */
    bt_audio_error_t (*deinit)(void);
    
    /**
     * @brief 编码PCM音频
     * @param pcm_in PCM输入数据
     * @param pcm_len PCM数据长度
     * @param encoded_out 编码输出缓冲区
     * @param encoded_size 输出缓冲区大小
     * @param encoded_len 实际编码长度(输出)
     * @return BT_AUDIO_OK: 成功, 其他: 错误码
     */
    bt_audio_error_t (*encode)(const void *pcm_in, size_t pcm_len,
                                void *encoded_out, size_t encoded_size,
                                size_t *encoded_len);
    
    /**
     * @brief 解码编码音频为PCM
     * @param encoded_in 编码输入数据
     * @param encoded_len 编码数据长度
     * @param pcm_out PCM输出缓冲区
     * @param pcm_size 输出缓冲区大小
     * @param pcm_len 实际PCM长度(输出)
     * @return BT_AUDIO_OK: 成功, 其他: 错误码
     */
    bt_audio_error_t (*decode)(const void *encoded_in, size_t encoded_len,
                                void *pcm_out, size_t pcm_size,
                                size_t *pcm_len);
    
    /**
     * @brief 获取编码帧大小
     * @return 编码帧字节数
     */
    size_t (*get_frame_size)(void);
    
    /**
     * @brief 获取PCM帧大小
     * @return PCM帧字节数
     */
    size_t (*get_pcm_frame_size)(void);
    
    /**
     * @brief 获取帧时长（微秒）
     * @return 单帧时长（微秒），0表示不支持
     * @note 用于计算音频发送定时器周期
     */
    uint32_t (*get_frame_duration_us)(void);
    
    /**
     * @brief 复位codec状态
     * @return BT_AUDIO_OK: 成功, 其他: 错误码
     */
    bt_audio_error_t (*reset)(void);
    
} bt_audio_codec_ops_t;

/* ========================================================================
 * Codec管理器接口
 * ======================================================================== */

/**
 * @brief 注册codec
 * @param codec_ops codec操作接口
 * @return BT_AUDIO_OK: 成功, 其他: 错误码
 */
bt_audio_error_t bt_audio_codec_register(const bt_audio_codec_ops_t *codec_ops);

/**
 * @brief 注销codec
 * @param codec_type codec类型
 * @return BT_AUDIO_OK: 成功, 其他: 错误码
 */
bt_audio_error_t bt_audio_codec_unregister(bt_audio_codec_type_t codec_type);

/**
 * @brief 获取codec操作接口
 * @param codec_type codec类型
 * @return codec操作接口指针, NULL表示未找到
 */
const bt_audio_codec_ops_t* bt_audio_codec_get(bt_audio_codec_type_t codec_type);

/**
 * @brief 初始化codec管理器
 * @return BT_AUDIO_OK: 成功, 其他: 错误码
 */
bt_audio_error_t bt_audio_codec_manager_init(void);

/**
 * @brief 去初始化codec管理器
 * @return BT_AUDIO_OK: 成功, 其他: 错误码
 */
bt_audio_error_t bt_audio_codec_manager_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* BT_AUDIO_CODEC_H_ */

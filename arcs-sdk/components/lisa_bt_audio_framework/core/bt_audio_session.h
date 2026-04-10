/**
 * @file bt_audio_session.h
 * @brief 蓝牙音频会话管理 - 简洁清晰的API设计
 * 
 * 架构说明：
 * 
 * 【编码模式 - 下行播放】
 * 蓝牙 → playback_write → 队列 → [解码线程] → PCM → [播放线程] → 硬件
 * 
 * 【透传模式 - 下行播放】
 * 蓝牙 → playback_write → PCM → [播放线程] → 硬件
 * 
 * 【编码模式 - 上行录音】
 * 硬件 → [录音回调] → PCM → [编码线程] → 帧队列 → capture_read_frame → 蓝牙
 * 
 * 【透传模式 - 上行录音】
 * 应用 → capture_write_frame → 帧队列 → capture_read_frame → 蓝牙
 * 
 * 设计特性：
 * - 编码和透传模式完全分离（零耦合）
 * - PCM数据和编码帧使用不同接口
 * - 不缓存编码数据，立即解码降低延迟
 * - 使用队列保证帧对齐
 * 
 * Copyright (C) ListenAI 2025
 */

#ifndef BT_AUDIO_SESSION_H_
#define BT_AUDIO_SESSION_H_

#include "bt_audio_types.h"
#include "bt_audio_codec.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 会话句柄
 * ======================================================================== */
typedef void* bt_audio_session_handle_t;

/* ========================================================================
 * 会话事件回调
 * ======================================================================== */

/**
 * @brief 会话事件回调函数
 * @param session 会话句柄
 * @param event 事件
 * @param user_data 用户数据
 */
typedef void (*bt_audio_session_event_callback_t)(bt_audio_session_handle_t session,
                                                   const bt_audio_event_t *event,
                                                   void *user_data);

/**
 * @brief 播放数据回调函数（session 抛出解码后的 PCM 数据）
 * @param pcm_data PCM 数据指针
 * @param size 数据大小（字节）
 * @param user_data 用户数据
 * @return 实际写入的字节数，<0 表示错误
 */
typedef int (*bt_audio_playback_data_callback_t)(const void *pcm_data,
                                                  size_t size,
                                                  void *user_data);

/**
 * @brief 录音数据回调函数（session 需要从外部获取 PCM 数据进行编码）
 * @param pcm_buffer 接收 PCM 数据的缓冲区
 * @param buffer_size 缓冲区大小（字节）
 * @param user_data 用户数据
 * @return 实际读取的字节数，<0 表示错误，0 表示暂无数据
 */
typedef int (*bt_audio_capture_data_callback_t)(void *pcm_buffer,
                                                 size_t buffer_size,
                                                 void *user_data);

/* ========================================================================
 * 会话配置
 * ======================================================================== */

/**
 * @brief 音频会话配置
 */
typedef struct {
    bt_audio_codec_type_t codec_type;   /**< Codec类型 (SBC/mSBC/CVSD等) */
    bt_audio_direction_t direction;     /**< 音频方向 (播放/录音) */
    bool passthrough_mode;              /**< 透传模式标志
                                         *   - true: 不做编解码，数据直通
                                         *   - false: 需要编解码处理
                                         */
    
    /* 回调函数 */
    bt_audio_session_event_callback_t event_callback;       /**< 事件回调函数 */
    bt_audio_playback_data_callback_t playback_data_callback; /**< 播放数据回调（PLAYBACK方向必须）*/
    bt_audio_capture_data_callback_t capture_data_callback;   /**< 录音数据回调（CAPTURE方向可选）*/
    
    void *user_data;                    /**< 用户自定义数据 */
} bt_audio_session_config_t;

/* ========================================================================
 * 会话生命周期管理
 * ======================================================================== */

/**
 * @brief 初始化会话管理器
 * @return BT_AUDIO_OK: 成功, 其他: 错误码
 * @note 必须在使用任何会话接口前调用
 */
bt_audio_error_t bt_audio_session_manager_init(void);

/**
 * @brief 去初始化会话管理器
 * @return BT_AUDIO_OK: 成功, 其他: 错误码
 * @note 会自动销毁所有活动会话
 */
bt_audio_error_t bt_audio_session_manager_deinit(void);

/**
 * @brief 创建音频会话
 * @param config 会话配置
 * @param session 会话句柄(输出)
 * @return BT_AUDIO_OK: 成功, 其他: 错误码
 */
bt_audio_error_t bt_audio_session_create(const bt_audio_session_config_t *config,
                                          bt_audio_session_handle_t *session);

/**
 * @brief 销毁音频会话
 * @param session 会话句柄
 * @return BT_AUDIO_OK: 成功, 其他: 错误码
 * @note 会自动停止会话并释放所有资源
 */
bt_audio_error_t bt_audio_session_destroy(bt_audio_session_handle_t session);

/**
 * @brief 启动音频会话
 * @param session 会话句柄
 * @param codec_config codec配置（包含采样率、通道数等）
 * @return BT_AUDIO_OK: 成功, 其他: 错误码
 * @note 启动后会创建处理线程，开始数据传输
 *       编码模式下，启动后可通过 bt_audio_session_get_codec_params 获取帧参数
 */
bt_audio_error_t bt_audio_session_start(bt_audio_session_handle_t session,
                                         const bt_audio_codec_config_t *codec_config);

/**
 * @brief 停止音频会话
 * @param session 会话句柄
 * @return BT_AUDIO_OK: 成功, 其他: 错误码
 * @note 会停止所有线程并清理缓冲区
 */
bt_audio_error_t bt_audio_session_stop(bt_audio_session_handle_t session);

/**
 * @brief 暂停音频会话（仅播放方向）
 * @param session 会话句柄
 * @return BT_AUDIO_OK: 成功, 其他: 错误码
 */
bt_audio_error_t bt_audio_session_pause(bt_audio_session_handle_t session);

/**
 * @brief 恢复音频会话（仅播放方向）
 * @param session 会话句柄
 * @return BT_AUDIO_OK: 成功, 其他: 错误码
 */
bt_audio_error_t bt_audio_session_resume(bt_audio_session_handle_t session);

/* ========================================================================
 * 数据接口 - 下行播放（蓝牙 → 硬件播放）
 * ======================================================================== */

/**
 * @brief 播放：写入待播放数据
 * @param session 会话句柄
 * @param data 数据指针
 * @param size 数据大小
 * @return BT_AUDIO_OK: 成功, 其他: 错误码
 * 
 * @note 使用场景：
 *       【编码模式】接收SBC/mSBC/CVSD等编码数据
 *         - 数据通过队列传递给解码线程
 *         - 解码线程立即解码后写入PCM缓冲区
 *         - 播放线程从PCM缓冲区读取并播放
 * 
 *       【透传模式】接收PCM数据
 *         - 数据直接写入PCM缓冲区
 *         - 播放线程从PCM缓冲区读取并播放
 */
bt_audio_error_t bt_audio_session_playback_write(bt_audio_session_handle_t session,
                                                  const void *data,
                                                  size_t size);

/* ========================================================================
 * 数据接口 - 上行录音（硬件录音 → 蓝牙）
 * ======================================================================== */

/**
 * @brief 录音：写入PCM数据（仅编码模式）
 * @param session 会话句柄
 * @param pcm_data PCM数据指针
 * @param size 数据大小（可以是任意大小）
 * @return BT_AUDIO_OK: 成功, 其他: 错误码
 * 
 * @note 【仅编码模式使用】
 *       - PCM数据写入ringbuf
 *       - 编码线程从ringbuf读取并编码
 *       - 编码后的完整帧放入队列
 *       - 蓝牙通过 capture_read_frame 读取
 * 
 * @warning 透传模式不应调用此接口，应使用 capture_write_frame
 */
bt_audio_error_t bt_audio_session_capture_write_pcm(bt_audio_session_handle_t session,
                                                     const void *pcm_data,
                                                     size_t size);

/**
 * @brief 录音：写入已编码的完整帧（仅透传模式）
 * @param session 会话句柄
 * @param frame_data 编码帧数据指针（必须是完整帧，帧对齐）
 * @param frame_size 帧大小
 * @return BT_AUDIO_OK: 成功, 其他: 错误码
 * 
 * @note 【仅透传模式使用】
 *       - 应用层提供已编码的完整帧
 *       - 要求数据必须是帧对齐的
 *       - 直接放入队列
 *       - 蓝牙通过 capture_read_frame 读取
 * 
 * @warning 编码模式不应调用此接口，应使用 capture_write_pcm
 */
bt_audio_error_t bt_audio_session_capture_write_frame(bt_audio_session_handle_t session,
                                                       const void *frame_data,
                                                       size_t frame_size);

/**
 * @brief 录音：读取编码后的完整帧（供蓝牙发送）
 * @param session 会话句柄
 * @param buffer 接收缓冲区
 * @param buffer_size 缓冲区大小
 * @param frame_size 实际读取的帧大小（输出）
 * @return BT_AUDIO_OK: 成功, 其他: 错误码
 * 
 * @note 【编码模式和透传模式都使用】
 *       - 从队列读取完整的编码帧
 *       - 总是返回完整帧，保证帧对齐
 *       - 非阻塞，无数据时 frame_size=0
 */
bt_audio_error_t bt_audio_session_capture_read_frame(bt_audio_session_handle_t session,
                                                      void *buffer,
                                                      size_t buffer_size,
                                                      size_t *frame_size);

/* ========================================================================
 * 辅助查询接口
 * ======================================================================== */

/**
 * @brief 获取codec帧参数（仅编码模式）
 * @param session 会话句柄
 * @param frame_duration_us 单帧时长（微秒）（输出）
 * @param frame_size_bytes 单帧大小（字节）（输出）
 * @return BT_AUDIO_OK: 成功, 其他: 错误码
 * 
 * @note 仅用于编码模式，透传模式需手动设置
 *       必须在 bt_audio_session_start 之后调用
 *       如果codec不支持相应接口，返回0
 */
bt_audio_error_t bt_audio_session_get_codec_params(bt_audio_session_handle_t session,
                                                    uint32_t *frame_duration_us,
                                                    size_t *frame_size_bytes);

/**
 * @brief 获取编码帧信息
 * @param session 会话句柄
 * @param frame_info 帧信息(输出)
 * @return BT_AUDIO_OK: 成功, 其他: 错误码
 * 
 * @note 用于获取编码帧的大小信息：
 *       - SBC: 首次编码后返回实际帧大小（动态计算）
 *       - mSBC: 返回固定57字节
 *       - 透传模式: 返回首次写入的帧大小
 */
bt_audio_error_t bt_audio_session_get_frame_info(bt_audio_session_handle_t session,
                                                  bt_audio_frame_info_t *frame_info);

/**
 * @brief 设置音量
 * @param session 会话句柄
 * @param volume 音量 (0-100)
 * @return BT_AUDIO_OK: 成功, 其他: 错误码
 */
bt_audio_error_t bt_audio_session_set_volume(bt_audio_session_handle_t session,
                                              uint8_t volume);

/**
 * @brief 获取统计信息
 * @param session 会话句柄
 * @param stats 统计信息(输出)
 * @return BT_AUDIO_OK: 成功, 其他: 错误码
 * 
 * @note 统计信息包括：
 *       - 处理的总帧数
 *       - 处理的总字节数
 *       - 丢帧数
 *       - 错误帧数
 */
bt_audio_error_t bt_audio_session_get_stats(bt_audio_session_handle_t session,
                                             bt_audio_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif /* BT_AUDIO_SESSION_H_ */

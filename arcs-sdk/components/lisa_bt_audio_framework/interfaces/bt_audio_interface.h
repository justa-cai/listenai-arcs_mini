/**
 * @file bt_audio_interface.h
 * @brief 蓝牙音频接口抽象层
 * 
 * Copyright (C) ListenAI 2025
 */

#ifndef BT_AUDIO_INTERFACE_H_
#define BT_AUDIO_INTERFACE_H_

#include "bt_audio_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 播放数据请求回调
 * @param buffer 数据缓冲区
 * @param size 需要的数据大小
 * @param user_data 用户数据
 * @return 实际写入的数据大小
 */
typedef size_t (*bt_audio_playback_callback_t)(void *buffer, size_t size, void *user_data);

/**
 * @brief 录音数据到达回调
 * @param buffer 数据缓冲区
 * @param size 数据大小
 * @param user_data 用户数据
 */
typedef void (*bt_audio_capture_callback_t)(const void *buffer, size_t size, void *user_data);

/* ========================================================================
 * 音频接口操作
 * ======================================================================== */

/**
 * @brief 音频接口操作结构体
 * 
 * 所有音频接口适配器必须实现此接口
 */
typedef struct bt_audio_interface_ops {
    const char *name;                   /* 接口名称 */
    
    /**
     * @brief 打开播放接口
     * @param format 音频格式
     * @return BT_AUDIO_OK: 成功, 其他: 错误码
     * @note 播放使用 playback_write 主动写入，不需要回调
     */
    bt_audio_error_t (*playback_open)(const bt_audio_format_t *format);
    
    /**
     * @brief 写入播放数据
     * @param data 数据指针
     * @param size 数据大小
     * @return 实际写入的字节数, <0表示错误
     * @note 此函数可能阻塞直到有缓冲空间，或立即返回写入的字节数
     */
    int (*playback_write)(const void *data, size_t size);
    
    /**
     * @brief 关闭播放接口
     * @return BT_AUDIO_OK: 成功, 其他: 错误码
     */
    bt_audio_error_t (*playback_close)(void);
    
    /**
     * @brief 暂停播放
     * @return BT_AUDIO_OK: 成功, 其他: 错误码
     */
    bt_audio_error_t (*playback_pause)(void);
    
    /**
     * @brief 恢复播放
     * @return BT_AUDIO_OK: 成功, 其他: 错误码
     */
    bt_audio_error_t (*playback_resume)(void);
    
    /**
     * @brief 打开录音接口
     * @param format 音频格式
     * @param callback 数据回调
     * @param user_data 用户数据
     * @return BT_AUDIO_OK: 成功, 其他: 错误码
     */
    bt_audio_error_t (*capture_open)(const bt_audio_format_t *format,
                                      bt_audio_capture_callback_t callback,
                                      void *user_data);
    
    /**
     * @brief 读取录音数据
     * @param buffer 数据缓冲区
     * @param size 缓冲区大小
     * @return 实际读取的字节数, <0表示错误
     */
    int (*capture_read)(void *buffer, size_t size);
    
    /**
     * @brief 关闭录音接口
     * @return BT_AUDIO_OK: 成功, 其他: 错误码
     */
    bt_audio_error_t (*capture_close)(void);
    
    /**
     * @brief 设置播放音量
     * @param volume 音量 (0-100)
     * @return BT_AUDIO_OK: 成功, 其他: 错误码
     */
    bt_audio_error_t (*set_volume)(uint8_t volume);
    
    /**
     * @brief 获取当前音量
     * @param volume 音量指针(输出)
     * @return BT_AUDIO_OK: 成功, 其他: 错误码
     */
    bt_audio_error_t (*get_volume)(uint8_t *volume);
    
} bt_audio_interface_ops_t;

/* ========================================================================
 * 音频接口管理器
 * ======================================================================== */

/**
 * @brief 注册音频接口
 * @param interface_ops 音频接口操作
 * @return BT_AUDIO_OK: 成功, 其他: 错误码
 */
bt_audio_error_t bt_audio_interface_register(const bt_audio_interface_ops_t *interface_ops);

/**
 * @brief 注销音频接口
 * @param name 接口名称
 * @return BT_AUDIO_OK: 成功, 其他: 错误码
 */
bt_audio_error_t bt_audio_interface_unregister(const char *name);

/**
 * @brief 获取音频接口
 * @param name 接口名称
 * @return 音频接口操作指针, NULL表示未找到
 */
const bt_audio_interface_ops_t* bt_audio_interface_get(const char *name);

/**
 * @brief 获取默认音频接口
 * @return 默认音频接口操作指针, NULL表示未找到
 */
const bt_audio_interface_ops_t* bt_audio_interface_get_default(void);

/**
 * @brief 设置默认音频接口
 * @param name 接口名称
 * @return BT_AUDIO_OK: 成功, 其他: 错误码
 */
bt_audio_error_t bt_audio_interface_set_default(const char *name);

/**
 * @brief 初始化音频接口管理器
 * @return BT_AUDIO_OK: 成功, 其他: 错误码
 */
bt_audio_error_t bt_audio_interface_manager_init(void);

/**
 * @brief 去初始化音频接口管理器
 * @return BT_AUDIO_OK: 成功, 其他: 错误码
 */
bt_audio_error_t bt_audio_interface_manager_deinit(void);

/* ========================================================================
 * 具体接口实现的注册函数
 * ======================================================================== */

/**
 * @brief 注册 lisa_audio 硬件接口
 * @return BT_AUDIO_OK: 成功, 其他: 错误码
 * @note 用户应用需要主动调用此函数注册接口
 */
bt_audio_error_t bt_audio_interface_lisa_register(void);

#ifdef __cplusplus
}
#endif

#endif /* BT_AUDIO_INTERFACE_H_ */

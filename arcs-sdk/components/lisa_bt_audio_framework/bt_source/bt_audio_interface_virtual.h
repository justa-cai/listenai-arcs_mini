
/**
 * @file bt_audio_interface_virtual.h
 * @brief 蓝牙虚拟音频接口（A2DP/HFP自动，极简阻塞API）
 *
 * - playback: 固定A2DP，capture: 固定HFP
 * - open自动阻塞等待链路ready，open返回后可安全write
 * - 支持编码/透传两种模式
 */

#ifndef BT_AUDIO_INTERFACE_VIRTUAL_H_
#define BT_AUDIO_INTERFACE_VIRTUAL_H_

#include "bt_audio_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    VINTF_PROFILE_PLAYBACK = 0,
    VINTF_PROFILE_CAPTURE = 1
} vintf_profile_type_t;

typedef void (*vintf_open_complete_callback_t)(bt_audio_format_t format);

/**
 * @brief 录音数据到达回调
 * @param buffer 数据缓冲区
 * @param size 数据大小
 * @param user_data 用户数据
 */
typedef void (*bt_audio_capture_callback_t)(const void *buffer, size_t size, void *user_data);

typedef struct {
    vintf_profile_type_t type;
    vintf_open_complete_callback_t open_complete_callback;
    bt_audio_capture_callback_t audio_data_callback;
    void *user_data;
} vintf_open_info_t;

/**
 * @brief 虚拟音频接口初始化
 * @return BT_AUDIO_OK: 成功, 其他: 错误码
 */
bt_audio_error_t vintf_init(void);

/**
 * @brief 虚拟音频接口去初始化
 * @return BT_AUDIO_OK: 成功, 其他: 错误码
 */
bt_audio_error_t vintf_deinit(void);

/**
 * @brief 统一profile open接口，playback固定A2DP，capture固定HFP
 */
bt_audio_error_t vintf_profile_open(vintf_open_info_t *info);

/**
 * @brief 统一profile close接口
 */
bt_audio_error_t vintf_profile_close(void);

/**
 * @brief 写入播放数据
 */
int vintf_playback_write(const void *buffer, size_t size);

/**
 * @brief 设置音频帧参数（为透传模式使用）
 * @param frame_duration_us 单帧时长，单位微秒
 * @param frame_size_bytes 单帧大小，单位字节
 * @return BT_AUDIO_OK: 成功, 其他: 错误码
 * @note 此接口主要为透传模式设计，编码模式会在初始化编码器时自动设置这些参数
 *       建议在vintf_init之后、vintf_profile_open之前调用
 */
bt_audio_error_t bt_vintf_set_frame_params(uint32_t frame_duration_us, size_t frame_size_bytes);

/**
 * @brief 设置音量
 * @param volume 音量值 (0-100)
 * @return BT_AUDIO_OK: 成功, 其他: 错误码
 */
bt_audio_error_t vintf_set_volume(uint8_t volume);

/**
 * @brief 获取音量
 * @param volume 音量输出指针
 * @return BT_AUDIO_OK: 成功, 其他: 错误码
 */
bt_audio_error_t vintf_get_volume(uint8_t *volume);

#ifdef __cplusplus
}
#endif

#endif /* BT_AUDIO_INTERFACE_VIRTUAL_H_ */

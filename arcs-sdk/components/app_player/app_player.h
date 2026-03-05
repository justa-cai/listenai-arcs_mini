/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#ifndef __LISTENAI_APP_PLAYER_H__
#define __LISTENAI_APP_PLAYER_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ==================== 前置声明 ====================
typedef struct app_player_s app_player_t;

/**
 * @brief 播放器错误码
 */
typedef enum {
    APP_PLAYER_OK = 0,                 /**< 成功 */
    APP_PLAYER_ERR_INVALID_PARAM = -1, /**< 无效参数 */
    APP_PLAYER_ERR_NO_MEMORY = -2,     /**< 内存不足 */
    APP_PLAYER_ERR_INVALID_STATE = -3, /**< 状态错误 */
    APP_PLAYER_ERR_NOT_SUPPORTED = -4, /**< 不支持的操作 */
    APP_PLAYER_ERR_TIMEOUT = -5,       /**< 超时 */
    APP_PLAYER_ERR_IO = -6,            /**< IO 错误 */
} app_player_err_t;

/**
 * @brief PA 控制回调函数类型
 * @param onoff  1: 打开PA, 0: 关闭PA
 * @return 0: 成功, 其他值: 失败
 */
typedef int (*app_player_pa_ctrl_cb_t)(int onoff);

// ==================== 音频焦点管理类型（可选功能） ====================

#ifdef CONFIG_APP_PLAYER_AUDIO_FOCUS

/**
 * @brief 播放器焦点状态
 */
typedef enum {
    APP_PLAYER_FOCUS_FOREGROUND = 0,  /**< 前景焦点：正常播放 */
    APP_PLAYER_FOCUS_BACKGROUND,      /**< 背景焦点：降级播放（暂停或降音量） */
    APP_PLAYER_FOCUS_NONE             /**< 无焦点：停止播放 */
} app_player_focus_state_t;

/**
 * @brief 焦点丢失策略
 */
typedef enum {
    APP_PLAYER_FOCUS_LOSS_IGNORE = 0,  /**< 忽略焦点变化，继续播放 */
    APP_PLAYER_FOCUS_LOSS_PAUSE,       /**< 暂停播放（可自动恢复） */
    APP_PLAYER_FOCUS_LOSS_STOP,        /**< 停止播放（不可自动恢复） */
    APP_PLAYER_FOCUS_LOSS_DUCK,        /**< 降低音量（暂不支持） */
} app_player_focus_loss_policy_t;

/**
 * @brief 焦点行为配置
 */
typedef struct {
    app_player_focus_loss_policy_t on_background;  /**< 变为后景时的策略 */
    app_player_focus_loss_policy_t on_focus_lost;  /**< 完全失去焦点时的策略 */
    uint8_t duck_volume_percent;                    /**< 降低音量时的百分比(0-100)（暂不支持） */
} app_player_focus_behavior_t;

/**
 * @brief 焦点变化回调函数
 * @param player 播放器实例
 * @param new_state 新的焦点状态
 * @param by_which 触发焦点变化的播放器句柄
 * @param user_data 用户自定义数据
 * @return true=完全接管处理（app_player不执行任何操作），false=执行默认策略
 *
 * @note 此回调在焦点状态变化时由焦点管理器调用，用于通知应用层焦点变化事件
 * @note 返回值控制行为：
 *       - 返回 false：app_player 根据 behavior 配置自动执行策略（pause/stop/ignore）
 *       - 返回 true：app_player 完全跳过处理，由应用层自行处理
 * @warning 回调函数应快速返回，避免阻塞焦点管理器
 *          耗时操作应提交到任务队列异步执行
 */
typedef bool (*app_player_focus_change_cb_t)(app_player_t *player,
                                              app_player_focus_state_t new_state,
                                              app_player_t *by_which,
                                              void *user_data);

/**
 * @brief 焦点通道配置（静态属性）
 */
typedef struct {
    const char *name;                            /**< 通道名称（需与 app_player_create 的 name 匹配） */
    int priority;                                /**< 优先级（数值越小优先级越高，0 = 最高） */
    const char **capture_names;                  /**< 可抢占的通道名称列表（通过 name 引用其他播放器） */
    int capture_count;                           /**< 抢占列表长度 */
    app_player_focus_behavior_t behavior;        /**< 焦点丢失时的行为配置 */
} app_player_focus_channel_config_t;

#endif // CONFIG_APP_PLAYER_AUDIO_FOCUS

/**
 * @brief app_player 初始化配置结构体
 */
typedef struct {
    app_player_pa_ctrl_cb_t pa_ctrl_callback;  /**< PA 控制回调函数（必填） */

#ifdef CONFIG_APP_PLAYER_AUDIO_FOCUS
    const app_player_focus_channel_config_t *focus_configs; /**< 焦点通道配置数组（可选） */
    int focus_config_count;                                  /**< 焦点通道配置数量 */
#endif
} app_player_config_t;

/**
 * @brief 播放器状态
 */
typedef enum {
    APP_PLAYER_STATE_IDLE = 0,  /**< 空闲状态 */
    APP_PLAYER_STATE_PREPARING, /**< 准备中 */
    APP_PLAYER_STATE_PREPARED,  /**< 准备完成 */
    APP_PLAYER_STATE_PLAYING,   /**< 播放中 */
    APP_PLAYER_STATE_PAUSED,    /**< 已暂停 */
    APP_PLAYER_STATE_STOPPED,   /**< 已停止 */
    APP_PLAYER_STATE_ERROR      /**< 错误状态 */
} app_player_state_t;

/**
 * @brief 播放器事件类型
 */
typedef enum {
    APP_PLAYER_EVENT_ERROR = 0,     /**< 播放错误 */
    APP_PLAYER_EVENT_PREPARED,      /**< 准备完成 */
    APP_PLAYER_EVENT_PLAYING,       /**< 播放中 */
    APP_PLAYER_EVENT_PAUSED,        /**< 已暂停 */
    APP_PLAYER_EVENT_STOPPED,       /**< 已停止 */
    APP_PLAYER_EVENT_COMPLETED,     /**< 播放完成 */
    APP_PLAYER_EVENT_SEEK_COMPLETE, /**< Seek 完成 */
} app_player_event_t;

/**
 * @brief 播放器事件回调函数
 * @param player 播放器实例
 * @param event 事件类型
 * @param user_data 用户自定义数据
 */
typedef void (*app_player_event_cb_t)(app_player_t *player, app_player_event_t event, void *user_data);

/**
 * @brief 高级播放选项
 */
typedef struct {
    const char *url;        /**< 播放 URL（必填） */
    uint32_t throw_time_ms; /**< 开始指定时长内的低能量段（毫秒，0=不跳过） */
} app_player_play_opt_t;

/**
 * @brief   初始化 app_player 模块
 * @param   config 初始化配置参数
 * @return  APP_PLAYER_OK 成功，其他表示错误
 * @note    必须在创建播放器实例前调用此函数
 * @note    config 中的 pa_ctrl_callback 不能为 NULL
 */
int app_player_init(const app_player_config_t *config);

/**
 * @brief   创建播放器实例
 * @param   name 播放器名称
 * @return  播放器实例指针，失败返回 NULL
 */
app_player_t *app_player_create(const char *name);

/**
 * @brief   销毁播放器实例
 * @param   player 播放器实例
 * @return  APP_PLAYER_OK 成功，其他表示错误
 */
int app_player_destroy(app_player_t *player);

/**
 * @brief   注册事件回调
 * @param   player 播放器实例
 * @param   event_cb 事件回调函数
 * @param   user_data 用户数据
 * @return  APP_PLAYER_OK 成功，其他表示错误
 * @note    建议在首次播放前调用
 */
int app_player_register_callback(app_player_t *player, app_player_event_cb_t event_cb, void *user_data);

/**
 * @brief   播放指定 URL
 * @param   player 播放器实例
 * @param   url 播放地址（支持 http://, https://等）
 * @return  APP_PLAYER_OK 成功，其他表示错误
 */
int app_player_play(app_player_t *player, const char *url);

/**
 * @brief   高级播放接口（支持更多选项）
 * @param   player 播放器实例
 * @param   opt 播放选项
 * @return  APP_PLAYER_OK 成功，其他表示错误
 */
int app_player_play_ex(app_player_t *player, const app_player_play_opt_t *opt);

/**
 * @brief   停止播放（异步）
 * @param   player 播放器实例
 * @return  APP_PLAYER_OK 成功，其他表示错误
 * @warning 流式播放模式下不支持此操作
 */
int app_player_stop(app_player_t *player);

/**
 * @brief   停止播放（同步，等待停止完成）
 * @param   player 播放器实例
 * @return  APP_PLAYER_OK 成功，其他表示错误
 * @warning 流式播放模式下不支持此操作
 */
int app_player_stop_sync(app_player_t *player);

/**
 * @brief   暂停播放
 * @param   player 播放器实例
 * @return  APP_PLAYER_OK 成功，其他表示错误
 * @warning 流式播放模式下不支持此操作
 */
int app_player_pause(app_player_t *player);

/**
 * @brief   恢复播放（异步）
 * @param   player 播放器实例
 * @return  APP_PLAYER_OK 成功，其他表示错误
 * @warning 流式播放模式下不支持此操作
 */
int app_player_resume(app_player_t *player);

/**
 * @brief   恢复播放（同步，等待恢复完成）
 * @param   player 播放器实例
 * @return  APP_PLAYER_OK 成功，其他表示错误
 * @warning 流式播放模式下不支持此操作
 */
int app_player_resume_sync(app_player_t *player);

/**
 * @brief   重置播放器到初始状态
 * @param   player 播放器实例
 * @return  APP_PLAYER_OK 成功，其他表示错误
 */
int app_player_reset(app_player_t *player);

/**
 * @brief   跳转到指定位置
 * @param   player 播放器实例
 * @param   seek_ms 跳转位置（毫秒）
 * @return  APP_PLAYER_OK 成功，其他表示错误
 * @warning 流式播放模式下不支持此操作
 */
int app_player_seek(app_player_t *player, uint32_t seek_ms);

/**
 * @brief   获取播放器状态
 * @param   player 播放器实例
 * @return  播放器状态
 */
app_player_state_t app_player_get_state(app_player_t *player);

/**
 * @brief   获取当前播放位置
 * @param   player 播放器实例
 * @param   position 输出参数，当前位置（毫秒）
 * @return  APP_PLAYER_OK 成功，其他表示错误
 */
int app_player_get_position(app_player_t *player, uint32_t *position);

/**
 * @brief   获取总时长
 * @param   player 播放器实例
 * @param   duration 输出参数，总时长（毫秒）
 * @return  APP_PLAYER_OK 成功，其他表示错误
 */
int app_player_get_duration(app_player_t *player, uint32_t *duration);

/**
 * @brief   设置音量
 * @param   player 播放器实例
 * @param   volume 音量值（1-100）
 * @return  APP_PLAYER_OK 成功，其他表示错误
 */
int app_player_set_volume(app_player_t *player, uint8_t volume);

/**
 * @brief   开始流式播放
 * @param   player 播放器实例
 * @param   sample_rate 采样率（Hz，如 8000, 16000, 48000）
 * @param   channels 声道数（当前仅支持 1）
 * @param   bits 位深度（当前仅支持 16）
 * @return  APP_PLAYER_OK 成功，其他表示错误
 * @note    调用此函数后，使用 app_player_write_stream 写入 PCM 数据
 * @note    流式播放适用于实时音频合成、网络音频流等场景
 * @warning 流式播放模式仅支持以下操作流程：
 *          app_player_play_stream -> app_player_write_stream -> app_player_finish_stream
 *          流式播放模式下不支持 seek/pause/stop/resume 等操作
 */
int app_player_play_stream(app_player_t *player, uint32_t sample_rate, uint8_t channels, uint8_t bits);

/**
 * @brief   写入流数据
 * @param   player 播放器实例
 * @param   data 数据指针
 * @param   size 数据大小（字节）
 * @param   timeout_ms 超时时间（毫秒）
 * @return  实际写入的字节数，<0 表示错误
 * @note    必须先调用 app_player_play_stream 开启流式播放
 * @note    支持多次调用以逐块写入数据
 * @warning 仅在流式播放模式下有效，必须先调用 app_player_play_stream
 */
int app_player_write_stream(app_player_t *player, const uint8_t *data, uint32_t size, uint32_t timeout_ms);

/**
 * @brief   结束流式播放
 * @param   player 播放器实例
 * @return  APP_PLAYER_OK 成功，其他表示错误
 * @note    通知播放器所有流数据已写入完毕，等待播放完成
 * @note    调用此函数后将收到 APP_PLAYER_EVENT_COMPLETED 事件
 * @warning 仅在流式播放模式下有效，调用后退出流式播放模式
 */
int app_player_finish_stream(app_player_t *player);

// ==================== 音频焦点管理 API（可选功能） ====================

#ifdef CONFIG_APP_PLAYER_AUDIO_FOCUS

/**
 * @brief   注册播放器焦点变化回调
 * @param   player 播放器实例
 * @param   on_focus_change 焦点状态变化回调函数（可为 NULL 仅使用默认策略）
 * @param   user_data 用户自定义数据（传递给回调）
 * @return  APP_PLAYER_OK 成功，其他表示错误
 *
 * @note    需要在 app_player_init() 时配置过焦点通道（focus_configs）
 * @note    如果未配置焦点通道，该播放器将不参与焦点管理
 * @note    可以在播放器创建后的任意时刻调用，甚至可以动态更换回调
 * @note    焦点变化时，app_player 会根据 behavior 配置自动执行策略，
 *          除非回调返回 true 表示完全接管
 *
 * @par 示例1：仅监听焦点变化，使用默认策略
 * @code
 * bool music_focus_cb(app_player_t *player, app_player_focus_state_t state,
 *                     app_player_t *by_which, void *user_data) {
 *     LOGI("Focus changed to %d by player %p", state, by_which);
 *     return false;  // 让 app_player 执行配置的策略
 * }
 *
 * app_player_register_focus_cb(music, music_focus_cb, NULL);
 * @endcode
 *
 * @par 示例2：完全自定义焦点处理
 * @code
 * bool music_focus_cb(app_player_t *player, app_player_focus_state_t state,
 *                     app_player_t *by_which, void *user_data) {
 *     if (state == APP_PLAYER_FOCUS_BACKGROUND) {
 *         // 自定义：降低音量而不是暂停
 *         app_player_set_volume(player, 30);
 *         return true;  // 阻止 app_player 执行默认策略
 *     }
 *     return false;  // 其他情况使用默认策略
 * }
 * @endcode
 */
int app_player_register_focus_cb(app_player_t *player,
                                  app_player_focus_change_cb_t on_focus_change,
                                  void *user_data);

/**
 * @brief   设置播放器的焦点行为策略（运行时动态修改）
 * @param   player 播放器实例
 * @param   behavior 新的焦点行为配置
 * @return  APP_PLAYER_OK 成功，其他表示错误
 *
 * @note    此函数允许在运行时动态修改播放器的焦点行为策略
 * @note    仅对已注册焦点通道的播放器有效
 * @note    不会影响其他播放器的配置
 * @note    修改后的配置立即生效，影响后续的焦点变化处理
 * @warning 建议在播放器空闲状态下调用，避免影响正在进行的焦点处理
 *
 * @par 使用示例：
 * @code
 * // 修改 MUSIC 播放器的焦点行为
 * app_player_focus_behavior_t new_behavior = {
 *     .on_background = APP_PLAYER_FOCUS_LOSS_IGNORE,  // 后景继续播放
 *     .on_focus_lost = APP_PLAYER_FOCUS_LOSS_STOP,    // 完全失焦停止
 * };
 * app_player_set_focus_behavior(g_music_player, &new_behavior);
 * @endcode
 */
int app_player_set_focus_behavior(app_player_t *player,
                                   const app_player_focus_behavior_t *behavior);

/**
 * @brief   获取播放器当前的焦点行为策略
 * @param   player 播放器实例
 * @param   behavior 输出参数，当前的焦点行为配置
 * @return  APP_PLAYER_OK 成功，其他表示错误
 *
 * @note    可用于保存和恢复焦点行为配置
 *
 * @par 使用示例：
 * @code
 * // 保存原始配置
 * app_player_focus_behavior_t original_behavior;
 * app_player_get_focus_behavior(g_music_player, &original_behavior);
 *
 * // 修改配置进行测试
 * app_player_focus_behavior_t test_behavior = { ... };
 * app_player_set_focus_behavior(g_music_player, &test_behavior);
 *
 * // 恢复原始配置
 * app_player_set_focus_behavior(g_music_player, &original_behavior);
 * @endcode
 */
int app_player_get_focus_behavior(app_player_t *player,
                                   app_player_focus_behavior_t *behavior);

#endif // CONFIG_APP_PLAYER_AUDIO_FOCUS

#ifdef __cplusplus
}
#endif

#endif // __LISTENAI_APP_PLAYER_H__

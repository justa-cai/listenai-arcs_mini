#ifndef __LISTENAI_AUDIOMGR_H__
#define __LISTENAI_AUDIOMGR_H__

#include <stdbool.h>

#define MAX_PLAYER_COUNT  8
#define MAX_CAPTURE_COUNT 8

typedef enum { FOREGROUND = 0, BACKGROUND, FOCUS_NONE } focus_state_e;

/**
 * @brief 焦点通道结构体
 */
typedef struct focus_channel_s {
    int id;                                // 通道ID
    char *name;                            // 通道名称
    int priority;                          // 优先级，数值越小优先级越高
    int capture_ids[MAX_CAPTURE_COUNT];    // 可抢占的通道ID列表
    int capture_count;                     // 抢占列表长度
    focus_state_e state;                   // 当前焦点状态
    void (*on_focus_change)(focus_state_e state, int by_which_id, void *user_data);  // 焦点变化回调
    void *user_data;                       // 回调用户数据
    bool registered;                       // 是否已注册
} focus_channel_t;

/**
 * @brief 焦点管理器结构体
 */
typedef struct listen_audiomgr_s {
    focus_channel_t channels[MAX_PLAYER_COUNT];  // 通道数组
    int channel_count;                           // 已注册通道数量
    int foreground_id;                           // 当前前景通道ID，-1表示无
    int background_id;                           // 当前背景通道ID，-1表示无
} listen_audiomgr_t;

/**
 * @brief 创建焦点管理器
 * @return 焦点管理器句柄
 */
listen_audiomgr_t *listen_audiomgr_create(void);

/**
 * @brief 销毁焦点管理器
 * @param handle 焦点管理器句柄
 */
void listen_audiomgr_destroy(listen_audiomgr_t *handle);

/**
 * @brief 注册焦点通道
 * @param handle 焦点管理器句柄
 * @param id 通道ID
 * @param name 通道名称
 * @param priority 优先级
 * @param capture_ids 可抢占的通道ID列表
 * @param capture_count 抢占列表长度
 * @param on_focus_change 焦点变化回调
 * @return 0 成功，其他失败
 */
int listen_audiomgr_register_channel(listen_audiomgr_t *handle,
                                     int id,
                                     const char *name,
                                     int priority,
                                     int *capture_ids,
                                     int capture_count,
                                     void (*on_focus_change)(focus_state_e, int, void *),
                                     void *user_data);

/**
 * @brief 请求焦点
 * @param handle 焦点管理器句柄
 * @param id 通道ID
 */
void listen_audiomgr_acquire_channel(listen_audiomgr_t *handle, int id);

/**
 * @brief 释放焦点
 * @param handle 焦点管理器句柄
 * @param id 通道ID
 */
void listen_audiomgr_release_channel(listen_audiomgr_t *handle, int id);

/**
 * @brief 获取通道名称（用于日志）
 * @param handle 焦点管理器句柄
 * @param id 通道ID
 * @return 通道名称
 */
const char *listen_audiomgr_get_channel_name(listen_audiomgr_t *handle, int id);

/**
 * @brief 获取焦点状态名称（用于日志）
 * @param state 焦点状态
 * @return 状态名称
 */
const char *listen_audiomgr_get_state_name(focus_state_e state);

#endif

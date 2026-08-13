#pragma once

#include <stdint.h>

#include "lisa_pm.h"

/**
 * @brief `lisa_pm` SoC 移植层操作表
 *
 * 该结构由具体 SoC 的 porting 实现填写，用于向公共层暴露
 * PM 初始化、策略下发、锁管理、睡眠钩子注册以及唤醒原因映射等能力。
 */
typedef struct {
    /** 初始化底层 PM 能力。 */
    int32_t (*init)(void);
    /** 将 `lisa_pm` 系统策略转换并应用到 SoC PM。 */
    int32_t (*apply_policy)(lisa_pm_system_policy_t policy);
    /** 获取系统级 PM 锁。 */
    int32_t (*acquire_lock)(void);
    /** 释放系统级 PM 锁。 */
    int32_t (*release_lock)(void);
    /** 注册睡眠阶段回调。 */
    int32_t (*register_sleep_hooks)(void);
    /** 注销睡眠阶段回调。 */
    int32_t (*unregister_sleep_hooks)(void);
    /** 注册受 `lisa_pm` 管理的 PM 设备。 */
    int32_t (*register_managed_device)(void);
    /** 注销受 `lisa_pm` 管理的 PM 设备。 */
    int32_t (*unregister_managed_device)(void);
    /** 获取当前记录的归一化唤醒原因。 */
    lisa_pm_wakeup_cause_t (*get_wakeup_cause)(void);
    /** 将 SoC 原始唤醒原因转换为 `lisa_pm` 语义。 */
    lisa_pm_wakeup_cause_t (*map_wakeup_cause)(uint32_t cause);
#if CONFIG_LISA_PM_REMOTE_LOCK_CLIENT
    /** 请求远端 AP 持有系统级 PM 锁。 */
    int32_t (*remote_lock_acquire)(void);
    /** 请求远端 AP 释放系统级 PM 锁。 */
    int32_t (*remote_lock_release)(void);
    /** 读取远端 AP remote lock 状态。 */
    int32_t (*remote_lock_get_state)(lisa_pm_remote_lock_state_t *state);
#endif
} lisa_pm_porting_ops_t;

/**
 * @brief 初始化当前 SoC 的 PM 移植实现。
 *
 * @retval 0 初始化成功
 * @retval <0 初始化失败
 */
int32_t lisa_pm_porting_init(void);

/**
 * @brief 应用系统级 PM 策略。
 *
 * @param policy `lisa_pm` 侧定义的目标系统策略
 *
 * @retval 0 设置成功
 * @retval <0 设置失败
 */
int32_t lisa_pm_porting_apply_policy(lisa_pm_system_policy_t policy);

/**
 * @brief 获取系统级 PM 锁。
 *
 * @retval 0 获取成功
 * @retval <0 获取失败
 */
int32_t lisa_pm_porting_acquire_lock(void);

/**
 * @brief 释放系统级 PM 锁。
 *
 * @retval 0 释放成功
 * @retval <0 释放失败
 */
int32_t lisa_pm_porting_release_lock(void);

/**
 * @brief 注册睡眠阶段钩子。
 *
 * @retval 0 注册成功
 * @retval <0 注册失败
 */
int32_t lisa_pm_porting_register_sleep_hooks(void);

/**
 * @brief 注销睡眠阶段钩子。
 *
 * @retval 0 注销成功
 * @retval <0 注销失败
 */
int32_t lisa_pm_porting_unregister_sleep_hooks(void);

/**
 * @brief 注册受 `lisa_pm` 管理的 PM 设备。
 *
 * @retval 0 注册成功
 * @retval <0 注册失败
 */
int32_t lisa_pm_porting_register_managed_device(void);

/**
 * @brief 注销受 `lisa_pm` 管理的 PM 设备。
 *
 * @retval 0 注销成功
 * @retval <0 注销失败
 */
int32_t lisa_pm_porting_unregister_managed_device(void);

/**
 * @brief 获取当前保存的归一化唤醒原因。
 *
 * @return `lisa_pm` 语义下的唤醒原因
 */
lisa_pm_wakeup_cause_t lisa_pm_porting_get_wakeup_cause(void);

/**
 * @brief 将 SoC 原始唤醒原因转换为 `lisa_pm` 统一语义。
 *
 * @param cause SoC PM 层返回的原始唤醒原因
 *
 * @return 归一化后的唤醒原因；无法识别时返回 `LISA_PM_WAKEUP_UNKNOWN`
 */
lisa_pm_wakeup_cause_t lisa_pm_porting_map_wakeup_cause(uint32_t cause);

#if CONFIG_LISA_PM_REMOTE_LOCK_CLIENT
/**
 * @brief 请求远端 AP 持有系统级 PM 锁。
 *
 * @retval 0 成功
 * @retval <0 失败
 */
int32_t lisa_pm_porting_remote_lock_acquire(void);

/**
 * @brief 请求远端 AP 释放系统级 PM 锁。
 *
 * @retval 0 成功
 * @retval <0 失败
 */
int32_t lisa_pm_porting_remote_lock_release(void);

/**
 * @brief 读取远端 AP remote lock 状态。
 *
 * @param state 输出状态快照
 *
 * @retval 0 成功
 * @retval <0 失败
 */
int32_t lisa_pm_porting_remote_lock_get_state(lisa_pm_remote_lock_state_t *state);
#endif

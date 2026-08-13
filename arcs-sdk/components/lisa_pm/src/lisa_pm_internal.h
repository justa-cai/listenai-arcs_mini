#pragma once

#include <stdint.h>

#include "lisa_pm.h"
#include "lisa_device.h"
#include "pm.h"
#include "lisa_pm_porting.h"

/**
 * @brief 扫描并注册通过 `lisa_device` 暴露的 PM 设备。
 *
 * @retval 0 注册成功
 * @retval <0 注册失败
 */
int32_t lisa_pm_register_discovered_devices(void);

/**
 * @brief 复制一份设备 PM 描述并注册到 `lisa_pm` 内部表。
 *
 * @param name 设备名称
 * @param system_ops 设备的 system PM 回调表
 * @param ctx 传递给回调表的上下文指针
 *
 * @retval 0 注册成功
 * @retval <0 注册失败
 */
int32_t lisa_pm_device_register_copy(const char *name,
                                     const lisa_pm_system_ops_t *system_ops,
                                     void *ctx);

/**
 * @brief 注册一个 `lisa_pm_device_t` 设备对象。
 *
 * @param dev 待注册的设备描述对象
 *
 * @retval 0 注册成功
 * @retval <0 注册失败
 */
int32_t lisa_pm_device_register(lisa_pm_device_t *dev);

/**
 * @brief 注销一个已注册的 `lisa_pm_device_t` 设备对象。
 *
 * @param dev 待注销的设备描述对象
 *
 * @retval 0 注销成功
 * @retval <0 注销失败
 */
int32_t lisa_pm_device_unregister(lisa_pm_device_t *dev);

/**
 * @brief 检查当前所有 PM 设备是否允许系统进入睡眠。
 *
 * @retval 0 当前允许进入睡眠
 * @retval <0 存在设备仍处于忙状态
 */
int32_t lisa_pm_devices_check_idle(void);

/**
 * @brief 逐个调用设备的挂起前准备逻辑。
 *
 * @retval 0 全部设备准备成功
 * @retval <0 任一设备准备失败
 */
int32_t lisa_pm_devices_prepare_suspend(void);

/**
 * @brief 逐个调用设备的唤醒后恢复逻辑。
 *
 * @retval 0 全部设备恢复成功
 * @retval <0 任一设备恢复失败
 */
int32_t lisa_pm_devices_resume_restore(void);

/**
 * @brief `lisa_pm` 睡眠钩子的进入回调。
 *
 * @param sleep_time_us 本次预计睡眠时长，单位微秒
 * @param arg SoC PM 层透传的上下文参数
 *
 * @retval 0 回调处理成功
 */
int32_t lisa_pm_framework_hook_enter(uint32_t sleep_time_us, void *arg);

/**
 * @brief `lisa_pm` 睡眠钩子的退出回调。
 *
 * @param sleep_time_us 本次实际睡眠时长，单位微秒
 * @param arg SoC PM 层透传的原始唤醒信息
 *
 * @retval 0 回调处理成功
 */
int32_t lisa_pm_framework_hook_exit(uint32_t sleep_time_us, void *arg);

/**
 * @brief 受管设备空闲检查回调。
 *
 * @param mode 当前 SoC PM 准备进入的睡眠模式
 *
 * @retval 0 当前允许进入睡眠
 * @retval <0 当前不允许进入睡眠
 */
int32_t lisa_pm_framework_device_check_idle(pm_mode_t mode);

/**
 * @brief 受管设备睡眠前回调。
 *
 * @param sleep_time_us 本次预计睡眠时长，单位微秒
 * @param arg SoC PM 层透传的上下文参数
 *
 * @retval 0 回调处理成功
 * @retval <0 回调处理失败
 */
int32_t lisa_pm_framework_device_on_enter(uint32_t sleep_time_us, void *arg);

/**
 * @brief 受管设备唤醒后回调。
 *
 * @param sleep_time_us 本次实际睡眠时长，单位微秒
 * @param arg SoC PM 层透传的原始唤醒信息
 *
 * @retval 0 回调处理成功
 * @retval <0 回调处理失败
 */
int32_t lisa_pm_framework_device_on_wake(uint32_t sleep_time_us, void *arg);

/**
 * @brief 分发应用级睡眠前回调。
 */
void lisa_pm_dispatch_app_before_sleep(void);

/**
 * @brief 分发应用级唤醒后回调。
 *
 * @param cause 本次归一化唤醒原因
 */
void lisa_pm_dispatch_app_after_wake(lisa_pm_wakeup_cause_t cause);

/**
 * @brief 在普通任务上下文中分发应用级唤醒后回调。
 *
 * @param cause 本次归一化唤醒原因
 */
void lisa_pm_dispatch_app_after_wake_in_task(lisa_pm_wakeup_cause_t cause);

/**
 * @brief 更新 `lisa_pm` 当前记录的归一化唤醒原因。
 *
 * @param cause 新的唤醒原因
 */
void lisa_pm_internal_update_wakeup_cause(lisa_pm_wakeup_cause_t cause);

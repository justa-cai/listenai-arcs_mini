/**
 * @file power_manager.h
 * @brief Power management functions for system boot and shutdown
 * @copyright Copyright (C) 2025 ANHUI LISTENAI Co., Ltd. All Rights Reserved.
 */

#ifndef __POWER_MANAGER_H__
#define __POWER_MANAGER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* 电源管理时间配置宏定义 */
#define POWER_BUTTON_HOLD_TIME_MS 3000 // 按键保持时间 (3秒)
#define POWER_SAMPLE_INTERVAL_MS  50   // 按键采样间隔 (50ms)

/**
 * @brief 电源管理配置
 */
typedef struct {
    /**
     * @brief 关机回调
     */
    void (*on_shutdown)(void);
} power_config_t;

/**
 * @brief 初始化电源管理
 *
 * @param config 电源管理配置
 */
void power_init(const power_config_t *config);

/**
 * @brief 等待电源键长按时间达到开机要求
 *
 * @return true 用户持续按压达到所需时间，或 USB 已连接，可以继续执行开机流程
 * @return false 用户未按压或未达到所需时间，应当关机
 */
bool power_wait_settle(void);

/**
 * @brief 检查 USB 是否已连接
 *
 * @return true USB 已连接
 * @return false USB 未连接
 */
bool power_is_usb_plugged(void);

/**
 * @brief 执行系统关机
 */
void power_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* __POWER_MANAGER_H__ */

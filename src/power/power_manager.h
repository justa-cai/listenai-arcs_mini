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
#include "battery/battery.h"

/* 电源管理引脚配置宏定义 */
#define POWER_BUTTON_GPIO_PORT      GPIOB()         // 电源按键GPIO端口
#define POWER_BUTTON_PIN_NUM        4               // 电源按键引脚号 (PB4)
#define POWER_BUTTON_PIN_MASK       (0x01 << POWER_BUTTON_PIN_NUM)  // 电源按键引脚掩码

#define POWER_LATCH_GPIO_PORT       GPIOB()         // 电源锁存GPIO端口  
#define POWER_LATCH_PIN_NUM         3               // 电源锁存引脚号 (PB3)
#define POWER_LATCH_PIN_MASK        (0x01 << POWER_LATCH_PIN_NUM)   // 电源锁存引脚掩码

#define POWER_IOMUX_PAD             CSK_IOMUX_PAD_B // IO复用PAD

/* 电源管理时间配置宏定义 */
#define POWER_BUTTON_HOLD_TIME_MS   3000            // 按键保持时间 (3秒)
#define POWER_SAMPLE_INTERVAL_MS    50              // 按键采样间隔 (50ms)

/**
 * @brief 检查电源按键长按开机
 * 
 * 检查PB4按键是否被长按3秒以启动系统
 * 如果在3秒内释放按键，系统将关机
 * 如果持续按下3秒，系统继续启动
 */
void power_check_boot_button(void);

/**
 * @brief 启动电源关机检测线程
 * 
 * 创建后台线程监控PB4按键长按3秒关机
 */
void power_start_shutdown_monitor(void);

/**
 * @brief 执行系统关机
 * 
 * 关闭LED，调用关机函数，切断电源
 */
void power_shutdown_system(void);

#ifdef __cplusplus
}
#endif

#endif /* __POWER_MANAGER_H__ */

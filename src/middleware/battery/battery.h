/*
 * @file battery.h
 * @brief
 * @version 0.1
 * @date 2025-04-16
 *
 * @copyright Copyright (C) 2025 ANHUI LISTENAI Co., Ltd. All Rights Reserved.
 */

#ifndef __BATTERY_H__
#define __BATTERY_H__

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 电池状态
 */
typedef enum {
    BATTERY_STATUS_NO_BATTERY,  // 无电池
    BATTERY_STATUS_NOT_CONNECT, // 未连接
    BATTERY_STATUS_CHARGING,    // 充电中
    BATTERY_STATUS_CHARGE_DONE, // 充电完成
    BATTERY_STATUS_UNKNOWN,     // 未知状态
} battery_status_t;

void battery_init(void);

/**
 * @brief 获取原始电池电量百分比 (无平滑/去抖)
 *
 * @return 0-100
 */
uint8_t battery_get_pct_raw(void);

/**
 * @brief 获取滤波后的电池电压 (mV)
 *
 * @return 电压值，单位 mV
 */
uint16_t battery_get_voltage_mv(void);

/**
 * @brief 获取 CH32 ADC 原始采样值
 *
 * 当前用于打印 BAT_TEMP_ADC_CHANNEL 的 raw 值。返回 0xFFFF 表示无效。
 *
 * @return CH32 ADC raw value
 */
uint16_t battery_get_temp_adc_raw(void);

/**
 * @brief 获取 USB 插入的稳定状态 (带去抖)
 *
 * @return true 表示已稳定插入
 */
bool battery_usb_plugged_stable_get(void);

/**
 * @brief 获取电池状态
 *
 * @return battery_status_t
 */
battery_status_t battery_get_status(void);

#if defined(__cplusplus)
}
#endif

#endif

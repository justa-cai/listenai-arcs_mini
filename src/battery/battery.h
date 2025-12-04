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

/**
 * @brief 电池状态
 */
typedef enum {
    BATTERY_STATUS_NOT_CONNECT, // 未连接
    BATTERY_STATUS_CHARGING,    // 充电中
    BATTERY_STATUS_CHARGE_DONE, // 充电完成
    BATTERY_STATUS_UNKNOWN,     // 未知状态
} battery_status_t;

typedef enum {
    USB_STATUS_UNPLUG = 0,
    USB_STATUS_PLUG,
} usb_status_t;

/**
 * @brief 电压采样初始化
 *
 */
void battery_adc_sample_init(void);

void usb_plug_detect_gpio_init(void);

usb_status_t get_usb_status(void);

/**
 * @brief 获取电池电量(百分比)
 *
 */
uint8_t get_battery_voltage_percentage(void);

/**
 * @brief 获取电池状态
 *
 * @return battery_status_t
 */
battery_status_t get_battery_status(void);

/**
 * @brief 获取电池类型
 *
 * @return bool
 */
uint16_t get_battery_id(void);

/**
 * @brief 判断语音键是否按下（目前，语音键和电池ID共同复用了一个引脚）
 *
 * @return bool
 */
bool is_voice_key_pressed(void);

#if defined(__cplusplus)
}
#endif

#endif

/**
 * @file lisa_btn.h
 * @brief 通用按键驱动接口
 * @version 2.0
 * @date 2025-12-11
 *
 * @copyright Copyright (C) 2025 ANHUI LISTENAI Co., Ltd. All Rights Reserved.
 */

#ifndef __LISA_BTN_H__
#define __LISA_BTN_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* ===== 按键事件类型 ===== */
typedef enum {
    LISA_BTN_PRESS_DOWN = 0,
    LISA_BTN_PRESS_CLICK,
    LISA_BTN_PRESS_DOUBLE_CLICK,
    LISA_BTN_PRESS_TRIPLE_CLICK,
    LISA_BTN_PRESS_QUADRUPLE_CLICK,
    LISA_BTN_PRESS_QUINTUPLE_CLICK,
    LISA_BTN_PRESS_SEXTUPLE_CLICK,
    LISA_BTN_PRESS_SEPTUPLE_CLICK,
    LISA_BTN_PRESS_REPEAT_CLICK,
    LISA_BTN_PRESS_SHORT_START,
    LISA_BTN_PRESS_SHORT_UP,
    LISA_BTN_PRESS_LONG_START,
    LISA_BTN_PRESS_LONG_UP,
    LISA_BTN_PRESS_LONG_HOLD,
    LISA_BTN_PRESS_LONG_HOLD_UP,
    LISA_BTN_PRESS_MAX,
    LISA_BTN_PRESS_NONE,
} lisa_btn_event_t;

/* ===== 按键类型 ===== */
typedef enum {
    LISA_BTN_TYPE_ADC = 0,      /* ADC按键 */
    LISA_BTN_TYPE_GPIO,         /* GPIO按键 */
} lisa_btn_type_t;

/* ===== 按键事件回调 ===== */
typedef void (*lisa_btn_cb_t)(lisa_btn_event_t evt, uint8_t btn_id, void *user);

/* ===== ADC按键模式 ===== */
typedef enum {
    LISA_BTN_ADC_MODE_ONE_TO_ONE = 0,   /* 一对一：一个ADC通道对应一个按键 */
    LISA_BTN_ADC_MODE_ONE_TO_MANY,      /* 一对多：一个ADC通道对应多个按键（电阻分压） */
} lisa_btn_adc_mode_t;

/* ===== ADC按键电压范围 ===== */
typedef struct {
    uint16_t voltage_min;       /* 电压最小值 (mV) */
    uint16_t voltage_max;       /* 电压最大值 (mV) */
} lisa_btn_adc_range_t;

/* ===== 按键时间配置 ===== */
typedef struct {
    uint16_t short_press_time;  /* 短按时间 (ms) */
    uint16_t long_press_time;   /* 长按时间 (ms) */
    uint16_t long_hold_time;    /* 长按保持时间 (ms) */
    uint16_t scan_period;       /* 扫描周期 (ms) */
} lisa_btn_time_config_t;

/* ===== ADC按键引脚配置 ===== */
typedef struct {
    uint8_t pin_num;            /* 引脚号 (如PB6的6) */
    uint8_t iomux_func;         /* IOMUX复用功能 */
} lisa_btn_adc_pin_t;

/* ===== ADC按键配置 ===== */
typedef struct {
    const char *adc_dev_name;   /* ADC设备名称 (如"adc0") */
    uint16_t ref_voltage;       /* 参考电压 (mV) */
    uint8_t resolution;         /* 分辨率 (bit) */
    
    lisa_btn_adc_mode_t mode;   /* ADC按键模式 */
    
    /* 一对多模式配置 */
    struct {
        uint8_t adc_channel;    /* ADC通道号 (0-5) */
        lisa_btn_adc_pin_t pin; /* 引脚配置 */
        uint8_t button_count;   /* 按键数量 */
        const lisa_btn_adc_range_t *ranges; /* 按键电压范围数组 */
    } one_to_many;
    
    /* 一对一模式配置 */
    struct {
        uint8_t button_count;   /* 按键数量 */
        struct {
            uint8_t adc_channel;    /* ADC通道号 */
            lisa_btn_adc_pin_t pin; /* 引脚配置 */
            lisa_btn_adc_range_t range; /* 电压范围 */
        } *buttons;             /* 按键配置数组 */
    } one_to_one;
    
    lisa_btn_time_config_t time_config; /* 时间配置 */
    lisa_btn_cb_t callback;     /* 事件回调函数 */
    void *user_data;            /* 用户数据 */
} lisa_btn_adc_config_t;

/* ===== GPIO按键配置 ===== */
typedef struct {
    uint8_t pin_num;            /* 引脚号 */
    uint8_t iomux_func;         /* IOMUX复用功能 */
    uint8_t active_level;       /* 有效电平 (0=低电平, 1=高电平) */
    bool pull_enable;           /* 是否使能上下拉 */
    bool pull_up;               /* true=上拉, false=下拉 */
} lisa_btn_gpio_item_t;

typedef struct {
    const char *gpio_dev_name;  /* GPIO设备名称 (默认"gpio0") */
    uint8_t button_count;       /* 按键数量 */
    const lisa_btn_gpio_item_t *buttons; /* GPIO按键配置数组 */
    
    lisa_btn_time_config_t time_config; /* 时间配置 */
    lisa_btn_cb_t callback;     /* 事件回调函数 */
    void *user_data;            /* 用户数据 */
} lisa_btn_gpio_config_t;

/* ===== 接口函数 ===== */

/**
 * @brief 初始化ADC按键驱动
 * @param config ADC按键配置
 * @return 0=成功, 其他=失败
 */
int lisa_btn_adc_init(const lisa_btn_adc_config_t *config);

/**
 * @brief 初始化GPIO按键驱动
 * @param config GPIO按键配置
 * @return 0=成功, 其他=失败
 */
int lisa_btn_gpio_init(const lisa_btn_gpio_config_t *config);

/**
 * @brief 反初始化按键驱动
 */
void lisa_btn_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* __LISA_BTN_H__ */
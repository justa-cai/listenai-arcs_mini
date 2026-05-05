/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "lisa_display.h"
#include <lisa_semaphore.h>
#include <lisa_mutex.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct lisa_display_panel {
    lisa_device_t *bus_dev;      /**< 数据总线设备（用于像素传输）*/
    lisa_device_t *cmd_bus_dev;  /**< 命令总线设备（可选，NULL 表示与数据总线共用）*/
    lisa_device_t *bus_hw_ref;   /**< 物理总线设备引用（用于共享底层互斥锁）*/
    lisa_device_t *rst_gpio;     /**< 复位 GPIO 设备 */
    uint32_t rst_pin;            /**< 复位引脚号 */
    lisa_display_backlight_t backlight;  /**< 背光配置 */
    lisa_display_capabilities_t caps;    /**< 显示能力与状态 */
    lisa_device_t *panel_dev;    /**< Panel 驱动设备 */
    lisa_mutex_t *bus_mutex;     /**< 总线访问互斥锁（共享总线时多个 Panel 共享同一锁）*/
    bool bus_mutex_owner;        /**< 是否为锁的创建者（用于判断是否需要释放）*/

    /* --- 私有数据 --- */
    void *priv_data;
    /* 来自 attach 的面板初始化参数副本（如果有） */
    void *init_params;
    size_t init_params_len;

    void *rotate_ctx;            /**< 旋转上下文指针（由 panel_rotate_init 绑定）*/
} lisa_display_panel_t;

typedef struct {
    uint16_t panel_w;
    uint16_t panel_h;
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    uint16_t x_offset;
    uint16_t y_offset;
} lisa_display_panel_mem_area_t;

typedef uint8_t lisa_mem_coord_t[4];

/**
 * @brief Panel 驱动接口 API 结构体
 *
 * 每个屏幕芯片驱动（第2层）都必须实现该接口。
 */
typedef struct lisa_display_panel_driver {
    int (*init)(lisa_display_panel_t *panel);

    int (*get_capabilities)(lisa_display_panel_t *panel, lisa_display_capabilities_t *caps);

    int (*write)(lisa_display_panel_t *panel, uint16_t x, uint16_t y,
                 const lisa_display_buffer_desc_t *desc, const void *buf);

    int (*blanking_on)(lisa_display_panel_t *panel);
    int (*blanking_off)(lisa_display_panel_t *panel);

    int (*set_brightness)(lisa_display_panel_t *panel, uint8_t brightness);

    int (*set_orientation)(lisa_display_panel_t *panel, lisa_display_orientation_t orientation);

} lisa_display_panel_driver_t;

/**
 * @brief 发送命令和关联的数据
 *
 * @param panel Panel 实例
 * @param cmd 要发送的命令字节
 * @param cmd_bits 命令位数
 * @param data 指向数据的指针
 * @param len 数据长度
 * @return 0 成功, <0 失败
 */
int panel_write_cmd_data(lisa_display_panel_t *panel, int cmd, uint8_t cmd_bits, const void *data, size_t len);

/**
 * @brief 绘制位图
 *
 * @param panel Panel 实例
 * @param cmd 命令
 * @param cmd_bits 命令位数
 * @param x x 坐标
 * @param y y 坐标
 * @param w 宽度
 * @param h 高度
 * @param pixels 位图数据
 * @return 0 成功, <0 失败
 */
int panel_draw_pixels(lisa_display_panel_t *panel, uint32_t cmd, uint16_t cmd_bits, uint16_t x, uint16_t y, uint16_t w, uint16_t h, const void *pixels);

/**
 * @brief 设置背光亮度
 *
 * @param backlight 背光实例
 * @param brightness 亮度值 (0-100)
 * @return 0 成功, <0 失败
 */
int panel_set_backlight_brightness(lisa_display_backlight_t *backlight, uint8_t brightness);

/**
 * @brief 初始化旋转上下文
 *
 * @param panel Panel 实例
 * @return 0 成功, <0 失败
 */
int panel_rotate_init(lisa_display_panel_t *panel);

/**
 * @brief 设置复位引脚电平
 *
 * @param panel Panel 实例
 * @param level 电平值 (0 或 1)
 */
void panel_reset_pin_set(lisa_display_panel_t *panel, uint8_t level);

/**
 * @brief 设置显存区域坐标
 *
 * @param panel Panel 实例
 * @param area 区域参数
 * @param x 输出 x 坐标数组
 * @param y 输出 y 坐标数组
 */
void panel_set_mem_area(lisa_display_panel_t *panel, lisa_display_panel_mem_area_t *area, lisa_mem_coord_t x, lisa_mem_coord_t y);

#ifdef __cplusplus
}
#endif

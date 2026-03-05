/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief LISA Touch 中断模式示例
 *
 * 本示例演示如何使用 LISA Touch 驱动的中断模式：
 * 1. 初始化Touch设备
 * 2. 配置I2C和GPIO总线
 * 3. 设置中断模式和回调函数
 * 4. 在GPIO中断触发时自动读取触摸数据并通过回调返回
 */

#include <stdbool.h>
#include <stdio.h>
#include "lisa_device.h"
#include "lisa_touch.h"
#include "lisa_gpio.h"
#include "IOMuxManager.h"
#include "board.h"
#include "pinmux.h"

#include "FreeRTOS.h"
#include "task.h"

#define TOUCH_DEVICE     "touch_cst328"
#define I2C_DEVICE       "i2c0"
#define GPIO_DEVICE      "gpioa"

/* Touch I2C 引脚定义 */
#define LISA_TOUCH_I2C_SDA_PORT  CSK_IOMUX_PAD_A
#define LISA_TOUCH_I2C_SDA_PIN   22
#define LISA_TOUCH_I2C_SDA_FUNC  8

#define LISA_TOUCH_I2C_SCL_PORT  CSK_IOMUX_PAD_A
#define LISA_TOUCH_I2C_SCL_PIN   23
#define LISA_TOUCH_I2C_SCL_FUNC  8

/* Touch GPIO 引脚定义 */
#define LISA_TOUCH_I2C_RST_PORT  CSK_IOMUX_PAD_A
#define LISA_TOUCH_I2C_RST_PIN   25
#define LISA_TOUCH_I2C_RST_FUNC  0

#define LISA_TOUCH_I2C_INT_PORT  CSK_IOMUX_PAD_A
#define LISA_TOUCH_I2C_INT_PIN   24
#define LISA_TOUCH_I2C_INT_FUNC  0

/*
    为满足不同板型示例场景，重定向 I2C0 和 GPIOA 的 pinmux 配置
    
    注意：此重定向会覆盖 boards/arcs_evb/pinmux.c 中的默认实现（使用 weak 符号机制）
    - lisa_i2c0_pinmux(): 仅配置示例所需的 I2C 引脚（PA22=SDA, PA23=SCL）
    - lisa_gpioa_pinmux(): 仅配置示例所需的 GPIO 引脚（PA25=RST, PA24=INT）
    其他默认引脚配置（如 LCD_RST_PIN, PA_EN_PIN 等）在此示例中不会被配置
*/
#ifdef CONFIG_BOARD_ARCS_EVB
void lisa_i2c0_pinmux()
{
    IOMuxManager_PinConfigure(LISA_TOUCH_I2C_SDA_PORT, LISA_TOUCH_I2C_SDA_PIN, LISA_TOUCH_I2C_SDA_FUNC);
    IOMuxManager_PinConfigure(LISA_TOUCH_I2C_SCL_PORT, LISA_TOUCH_I2C_SCL_PIN, LISA_TOUCH_I2C_SCL_FUNC);
}

void lisa_gpioa_pinmux()
{
    IOMuxManager_PinConfigure(LISA_TOUCH_I2C_RST_PORT, LISA_TOUCH_I2C_RST_PIN, LISA_TOUCH_I2C_RST_FUNC);
    IOMuxManager_PinConfigure(LISA_TOUCH_I2C_INT_PORT, LISA_TOUCH_I2C_INT_PIN, LISA_TOUCH_I2C_INT_FUNC);
}
#endif

/* 触摸活动标志：用于主循环判断是否有触摸事件（volatile 保证多任务访问的正确性） */
static volatile bool touch_active = false;

/* 触摸事件回调函数（在任务上下文中调用，由驱动内部任务触发） */
static void touch_event_callback(const lisa_touch_event_t *event, void *user_data)
{
    (void)user_data;

    /* 此回调在驱动内部的任务上下文中调用，可以安全使用 printf */
    switch (event->type) {
        case LISA_TOUCH_EVENT_PRESS:
            /* 按下事件：输出坐标 */
            if (event->point_count > 0) {
                printf("[Callback] Touch: x=%4d, y=%4d, type=PRESS\n",
                       event->points[0].x, event->points[0].y);
            }
            touch_active = true;  /* 标记有触摸活动 */
            break;
            
        case LISA_TOUCH_EVENT_RELEASE:
            /* 释放事件 */
            printf("[Callback] Touch: type=RELEASE\n");
            touch_active = false;  /* 清除触摸活动标志 */
            break;
            
        case LISA_TOUCH_EVENT_NONE:
            /* 无事件，不处理 */
            break;
            
        default:
            break;
    }
}

int main(int argc, char **argv)
{
    printf("=== LISA Touch Interrupt Mode Example ===\n");

    /* I2C0 和 GPIOA 引脚复用配置已由驱动初始化时自动完成（调用示例中重写的 lisa_i2c0_pinmux, lisa_gpioa_pinmux） */
    printf("I2C pins configured (SDA: PA%d, SCL: PA%d)\n", LISA_TOUCH_I2C_SDA_PIN, LISA_TOUCH_I2C_SCL_PIN);
    printf("GPIO pins configured (RST: PA%d, INT: PA%d)\n", LISA_TOUCH_I2C_RST_PIN, LISA_TOUCH_I2C_INT_PIN);

    /* 获取设备 */
    lisa_device_t *touch_dev = lisa_device_get(TOUCH_DEVICE);
    if (!lisa_device_ready(touch_dev)) {
        printf("Error: %s device not ready\n", TOUCH_DEVICE);
        return -1;
    }
    printf("%s device ready\n", TOUCH_DEVICE);

    /* 获取I2C设备 */
    lisa_device_t *i2c_dev = lisa_device_get(I2C_DEVICE);
    if (!lisa_device_ready(i2c_dev)) {
        printf("Error: %s device not ready\n", I2C_DEVICE);
        return -1;
    }
    printf("%s device ready\n", I2C_DEVICE);

    lisa_device_t *gpio_dev = lisa_device_get(GPIO_DEVICE);
    if (!lisa_device_ready(gpio_dev)) {
        printf("Error: %s device not ready\n", GPIO_DEVICE);
        return -1;
    }
    printf("%s device ready\n", GPIO_DEVICE);

    /* 配置Touch总线 */
    lisa_touch_bus_config_t bus_config = {
        .bus_type = LISA_TOUCH_BUS_I2C,
        .config = {
            .i2c = {
                .i2c_dev = i2c_dev,
                .int_gpio = gpio_dev,
                .int_pin = LISA_TOUCH_I2C_INT_PIN,
                .rst_gpio = gpio_dev,
                .rst_pin = LISA_TOUCH_I2C_RST_PIN,
            }
        }
    };

    int ret = lisa_touch_attach_bus(touch_dev, &bus_config);
    if (ret != 0) {
        printf("Error: Touch bus attach failed (code: %d)\n", ret);
        return -1;
    }
    printf("Touch bus attached successfully\n");

    /* 设置触摸事件回调函数 */
    ret = lisa_touch_set_callback(touch_dev, touch_event_callback, NULL);
    if (ret != 0) {
        printf("Error: Touch callback set failed (code: %d)\n", ret);
        return -1;
    }
    printf("Touch callback set successfully\n");

    /* 设置中断模式 */
    ret = lisa_touch_set_int_mode(touch_dev, LISA_TOUCH_INT_MODE_INTERRUPT);
    if (ret != 0) {
        printf("Error: Touch interrupt mode set failed (code: %d)\n", ret);
        return -1;
    }
    printf("Touch interrupt mode enabled\n");

    /* 启用Touch设备 */
    ret = lisa_touch_enable(touch_dev);
    if (ret != 0) {
        printf("Error: Touch enable failed (code: %d)\n", ret);
        return -1;
    }
    printf("Touch device enabled\n");

    /* 获取设备能力 */
    lisa_touch_capabilities_t caps;
    ret = lisa_touch_get_capabilities(touch_dev, &caps);
    if (ret == 0) {
        printf("Touch capabilities: max_x=%d, max_y=%d, max_points=%d\n",
               caps.max_x, caps.max_y, caps.max_points);
    }

    printf("\n=== Interrupt Mode Active ===\n");
    printf("Touch events will be reported via callback when GPIO interrupt triggers\n");
    printf("Press the touch screen to see coordinates in callback\n");
    printf("(Callback is called from driver's internal task, not from ISR)\n\n");

    /* 主循环：保持程序运行，触摸事件通过回调自动处理 */
    /* 驱动内部会创建一个任务来处理 I2C 读取，回调在任务上下文中调用 */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));  /* 每秒检查一次 */
        /* 仅在无触摸活动时输出等待消息，避免与触摸事件消息交错 */
        if (!touch_active) {
            printf("[Main Loop] System running, waiting for touch events...\n");
        }
    }

    return 0;
}


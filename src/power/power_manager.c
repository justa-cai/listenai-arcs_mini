/**
 * @file power_manager.c
 * @brief Power management functions for system boot and shutdown
 * @copyright Copyright (C) 2025 ANHUI LISTENAI Co., Ltd. All Rights Reserved.
 */

#include "power_manager.h"
#include "lisa_thread.h"
#include "lisa_log.h"

#include "FreeRTOS.h"
#include "task.h"
#include "IOMuxManager.h"
#include "Driver_GPIO.h"
#include "battery/battery.h"
#include <stdio.h>

#define TAG "power_mgr"

/* External functions */
extern void app_led_off(void);
extern void shunt_down(void);

/**
 * @brief 初始化电源管理GPIO
 */
static void power_gpio_init(void)
{
    // 初始化GPIOB
    GPIO_Initialize(POWER_BUTTON_GPIO_PORT, NULL, NULL);
    
    // 配置电源按键引脚 (PB4) 为输入
    IOMuxManager_PinConfigure(POWER_IOMUX_PAD, POWER_BUTTON_PIN_NUM, CSK_IOMUX_FUNC_DEFAULT);
    GPIO_SetDir(POWER_BUTTON_GPIO_PORT, POWER_BUTTON_PIN_MASK, 0);
    GPIO_Control(POWER_BUTTON_GPIO_PORT, CSK_GPIO_MODE_PULL_UP | CSK_GPIO_DEBOUNCE_DISABLE, POWER_BUTTON_PIN_MASK);
    
    // 配置电源锁存引脚 (PB3) 为输出并设置为高电平
    IOMuxManager_PinConfigure(POWER_IOMUX_PAD, POWER_LATCH_PIN_NUM, CSK_IOMUX_FUNC_DEFAULT);
    GPIO_SetDir(POWER_LATCH_GPIO_PORT, POWER_LATCH_PIN_MASK, 1);
    GPIO_PinWrite(POWER_LATCH_GPIO_PORT, POWER_LATCH_PIN_MASK, 1);
    
    printf("Power GPIO initialized - Button: PB%d, Latch: PB%d\n", 
           POWER_BUTTON_PIN_NUM, POWER_LATCH_PIN_NUM);
}

/**
 * @brief 读取电源按键状态
 * @return 1: 按键释放, 0: 按键按下
 */
static int power_button_read(void)
{
    return GPIO_PinRead(POWER_BUTTON_GPIO_PORT, POWER_BUTTON_PIN_MASK) ? 1 : 0;
}

/* Power shutdown detection task */
static void power_shutdown_task(void *arg)
{
    (void)arg;
    
    printf("Power shutdown detection task started\n");
    
    // 等待系统完全启动
    lisa_thread_mdelay(5000);
    
    // 等待开机按键松开，防止开机按下后未松开直接关机
    printf("Waiting for power button to be released after boot\n");
    while (power_button_read() == 0) {
        printf("Power button still pressed, waiting for release...\n");
        lisa_thread_mdelay(500);  // 每500ms检查一次
    }
    printf("Power button released, shutdown monitor ready\n");
    
    while (1) {
        int button_state = power_button_read();
        usb_status_t usb_status = get_usb_status();
        
        if ((button_state == 0) && (usb_status == USB_STATUS_UNPLUG)) { // 按键按下
            printf("Power button pressed, checking for long press (3s)\n");
            
            // 等待3秒，检查按键是否持续按下
            uint32_t count = 0;
            const uint32_t max_count = POWER_BUTTON_HOLD_TIME_MS / POWER_SAMPLE_INTERVAL_MS;
            
            while (count < max_count) {
                lisa_thread_mdelay(POWER_SAMPLE_INTERVAL_MS);
                
                if (power_button_read() != 0) {
                    // 按键释放，不到3秒 - 忽略短按
                    printf("Power button released early (%u ms), ignoring short press\n", 
                           count * POWER_SAMPLE_INTERVAL_MS);
                    break;  // 跳出内层循环，继续监控
                }
                
                count++;
                
                // 每秒打印一次进度
                if ((count * POWER_SAMPLE_INTERVAL_MS) % 1000 == 0) {
                    printf("Power button held for %u ms (need 3000ms for shutdown)\n", 
                           count * POWER_SAMPLE_INTERVAL_MS);
                }
            }
            
            // 检查是否达到3秒
            if (count >= max_count) {
                printf("Power button held for 3s, shutting down\n");
                power_shutdown_system();
                return;
            }
        }
        
        lisa_thread_mdelay(POWER_SAMPLE_INTERVAL_MS);
    }
}

/**
 * @brief 检查开机按键长按
 * 简化版本：必须按住3秒才能开机，否则关机
 */
static void check_power_button_long_press(void)
{
    printf("Starting power button boot check (3s hold required)\n");
    
    // 立即关闭LED，避免开机时闪烁
    extern void app_led_off(void);
    app_led_off();
    
    // 初始化GPIO
    power_gpio_init();
    
    // 稍等GPIO稳定
    lisa_thread_mdelay(50);
    
    int button_state = power_button_read();
    printf("Initial button state: %d (0=pressed, 1=released)\n", button_state);
    
    // 检测usb状态
    usb_status_t usb_status = get_usb_status();
    printf("usb status: %d (0=USB_STATUS_UNPLUG, 1=USB_STATUS_PLUG)\n", usb_status);
    
    // 如果检测到充电状态，直接开机
    if (usb_status == USB_STATUS_PLUG) {
        printf("usb detected, boot allowed without button press\n");
        // 3秒检测成功后启动LED
        printf("Enabling LED after charging detection\n");
        extern void app_led_init(void);
        app_led_init();
        return;
    }
    
    // 如果按键没有按下且没有充电，直接关机
    if (button_state != 0) {
        printf("Power button not pressed and not charging, shutting down\n");
        app_led_off();
        shunt_down();
        return;
    }
    
    printf("Power button pressed, checking 3s hold requirement\n");
    
    // 检查是否持续按住3秒
    uint32_t count = 0;
    const uint32_t max_count = POWER_BUTTON_HOLD_TIME_MS / POWER_SAMPLE_INTERVAL_MS;
    
    while (count < max_count) {
        lisa_thread_mdelay(POWER_SAMPLE_INTERVAL_MS);
        
        if (power_button_read() != 0) {
            // 按键释放，不到3秒就关机
            printf("Power button released early (%u ms), shutting down\n", 
                   count * POWER_SAMPLE_INTERVAL_MS);
            app_led_off();
            shunt_down();
            return;
        }
        
        count++;
        
        // 每秒打印一次进度
        if ((count * POWER_SAMPLE_INTERVAL_MS) % 1000 == 0) {
            printf("Power button held for %u ms (need %u ms)\n", 
                   count * POWER_SAMPLE_INTERVAL_MS, POWER_BUTTON_HOLD_TIME_MS);
        }
    }
    
    printf("Power button held for 3s, boot allowed\n");
    
    // 3秒检测成功后启动LED
    printf("Enabling LED after successful 3s check\n");
    extern void app_led_init(void);
    app_led_init();
}

void power_check_boot_button(void)
{
    printf("Starting power button boot check...\n");
    check_power_button_long_press();
    printf("Power button check completed, continuing boot...\n");
}

void power_start_shutdown_monitor(void)
{
    printf("Starting power shutdown monitor\n");
    
    // 创建电源关机检测线程
    lisa_thread_attr_t power_attr = {
        .name = "POWER_SHUTDOWN",
        .stack_size = 4096,
        .priority = 5
    };
    lisa_thread_create(&power_attr, power_shutdown_task, NULL);
    
    printf("Power shutdown monitor thread created\n");
}

void power_shutdown_system(void)
{
    printf("Executing system shutdown...\n");
    
    // 关闭LED
    app_led_off();
    
    // 调用关机函数
    shunt_down();
    
    printf("System shutdown completed\n");
}

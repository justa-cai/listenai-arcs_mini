/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief BLE 外设示例
 *
 * 本示例演示如何使用 LISA Bluetooth 组件实现 BLE 外设功能：
 * 1. 自定义广播数据
 * 2. 自定义扫描响应数据
 * 3. 自定义 GAP 配置
 */

#define LOG_TAG "sample"
#include <lisa_log.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arcs_ap_base.h"
#include "spiflash.h"
#include "arcs_flash_if.h"
#include "nvs.h"
#include "lisa_bluetooth.h"
#include "bt_app_if.h"
#include "hogpd_msg.h"
#include "hogpd.h"
#include "IOMuxManager.h"
#include "Driver_GPIO.h"
#include "pinmux.h"
#include "lcd_logo.h"

#include "FreeRTOS.h"
#include "task.h"

#if CFG_NVS
#define NVDS_FLASH_ADDRESS_OFFSET   (0xFF8000) //The last 32KB of 16B flash
#define NVDS_FLASH_SIZE      (0x8000) //32KB

struct nvs_fs arcs_nvs_fs;
FLASH_DEV arcs_flash_dev  = {
    .base_addr = CMN_FLASHC_BASE,
    .d_width = 4,
    .sclk_div = 0xFF, //divider is 1
    .run_mod = RUN_WITHOUT_INT,
    .timeout = 0x180000
};

static int arcs_nvs_init(void)
{
    struct flash_pages_info info;

    flash_if_init(&arcs_flash_dev, 0, 0);

    //init nvs module
    arcs_nvs_fs.offset = NVDS_FLASH_ADDRESS_OFFSET;
    arcs_nvs_fs.flash_device = &arcs_flash_dev;
    flash_get_page_info_by_offs(&arcs_flash_dev, arcs_nvs_fs.offset, &info);
    arcs_nvs_fs.sector_size = info.size;
    arcs_nvs_fs.sector_count = NVDS_FLASH_SIZE/info.size;

    nvds_init(&arcs_nvs_fs);

    return 0;
}
#endif

static const uint8_t user_adv_data[] = {
    // Flags
    0x02, 0x01, 0x06,
    // Complete List of 16-bit Service UUIDs: HID (0x1812)
    0x03, 0x03, 0x12, 0x18,
    // Complete Local Name: "LISA_BLE"
    0x09, 0x09, 'L', 'I', 'S', 'A', '_', 'B', 'L', 'E',
};

const uint8_t* lisa_bt_get_adv_data(uint8_t *len)
{
    *len = sizeof(user_adv_data);
    return user_adv_data;
}

static const uint8_t user_scan_rsp_data[] = {
    // Manufacturer Specific Data
    0x05, 0xFF, 0xAB, 0x0A, 0x01, 0x02,
};

const uint8_t* lisa_bt_get_scan_rsp_data(uint8_t *len)
{
    *len = sizeof(user_scan_rsp_data);
    return user_scan_rsp_data;
}

void lisa_bt_gap_config(ble_gap_cfg_t *cfg)
{
    // Set custom MAC address
    uint8_t mac[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    memcpy(cfg->addr.addr, mac, 6);
    
    // Set custom device name
    const char *name = "LISA_CUSTOM";
    cfg->name_len = strlen(name);
    if (cfg->name_len > sizeof(cfg->name)) {
        cfg->name_len = sizeof(cfg->name);
    }
    memcpy(cfg->name, name, cfg->name_len);
}

// HOGPD 回调结构（在 custom_service.c 中定义）
extern const hogpd_cb_t ble_hogpd_cb;

static void *gpio_handler = NULL;
static volatile bool button_pressed = false;
static bool hogpd_enabled = false;
static uint8_t last_connected_state = 0;

// GPIO 中断回调
static void gpio_button_callback(uint32_t event, void *workspace)
{
    if (event & (1 << POWER_KEY_PIN)) {
        LISA_LOGI(LOG_TAG, "Button pressed!");
        button_pressed = true;
        // 禁用中断防止抖动
        GPIO_Control(gpio_handler, CSK_GPIO_INTR_DISABLE, (1 << POWER_KEY_PIN));
    }
}

// 初始化 GPIO
static void gpio_button_init(void)
{
    // 配置引脚复用
    lisa_gpiob_pinmux();

    // 获取 GPIO 句柄
    gpio_handler = GPIOB();

    // 初始化 GPIO
    GPIO_Initialize(gpio_handler, gpio_button_callback, NULL);

    // 设置为输入
    GPIO_SetDir(gpio_handler, (1 << POWER_KEY_PIN), CSK_GPIO_DIR_INPUT);

    // 配置下降沿触发中断（按键按下）
    GPIO_Control(gpio_handler,
        CSK_GPIO_DEBOUNCE_DISABLE |
        CSK_GPIO_SET_INTR_NEGATIVE_EDGE |
        CSK_GPIO_INTR_ENABLE, (1 << POWER_KEY_PIN));

    LISA_LOGI(LOG_TAG, "GPIO button initialized (PB%d)", POWER_KEY_PIN);
}

// 发送"下一首"媒体按键
static void send_media_next_track(void)
{
    // 媒体控制 HID 报告格式：
    // Report ID: 0x03, Report Index: HIDS_MEDIA_INDEX (2)
    // Scan Next Track: Usage ID 0x00B5 (小端序: 0xB5, 0x00)

    // 发送"按下"报告
    uint8_t media_report_press[] = {0xB5, 0x00};
    int ret = ble_hogpd_report_upd(0, HIDS_MEDIA_INDEX, sizeof(media_report_press), media_report_press);
    if (ret == 0) {
        LISA_LOGI(LOG_TAG, "Media press sent: 0x%02X%02X", media_report_press[0], media_report_press[1]);
    } else {
        LISA_LOGE(LOG_TAG, "Failed to send media press: %d", ret);
        return;
    }

    // 短暂延迟后发送"抬起"报告
    vTaskDelay(pdMS_TO_TICKS(50));

    uint8_t media_report_release[] = {0x00, 0x00};
    ret = ble_hogpd_report_upd(0, HIDS_MEDIA_INDEX, sizeof(media_report_release), media_report_release);
    if (ret == 0) {
        LISA_LOGI(LOG_TAG, "Media release sent");
    } else {
        LISA_LOGE(LOG_TAG, "Failed to send media release: %d", ret);
    }
}

int main(int argc, char **argv)
{
    LISA_LOGI(LOG_TAG, "=== BLE Peripheral with HID Keyboard ===");

    arcs_nvs_init();

    // 射频校准
    ls_rf_probe();
    ls_rf_cali_proc();

    // 初始化 GPIO
    gpio_button_init();

    // BLE 初始化
    lisa_bluetooth_init();

    LISA_LOGI(LOG_TAG, "BLE Stack Initialized");

    // 初始化 LCD 并显示 Logo
    lcd_show_logo();

    // Start Advertising
    app_ble_adv_start(0, BLE_ADV_GEN);

    LISA_LOGI(LOG_TAG, "Press POWER_KEY (PB4) to send 'Next Track' media key");

    while (1) {
        // 检查连接状态
        uint8_t connected = app_ble_connected_state(0);

        if (connected && !hogpd_enabled && connected != last_connected_state) {
            // 连接建立，启用 HOGPD 服务和连接参数更新
            LISA_LOGI(LOG_TAG, "BLE connected, enabling HOGPD service");
            ble_hogpd_enable(0);
            ble_gap_set_con_param_dis(0);  // 启用连接参数更新，解决连接超时问题
            hogpd_enabled = true;
        }

        // 检测断开连接并重新启动广播
        if (!connected && hogpd_enabled && connected != last_connected_state) {
            // 断开连接了，重新启动广播
            LISA_LOGI(LOG_TAG, "BLE disconnected, restarting advertising");
            app_ble_adv_start(0, BLE_ADV_GEN);
            hogpd_enabled = false;
        }

        last_connected_state = connected;

        if (button_pressed) {
            button_pressed = false;

            // 检查是否已连接
            if (hogpd_enabled) {
                // 发送媒体按键
                send_media_next_track();
            } else {
                LISA_LOGW(LOG_TAG, "BLE not connected, ignoring button");
            }

            // 短暂延迟后重新启用中断
            vTaskDelay(pdMS_TO_TICKS(200));
            GPIO_Control(gpio_handler, CSK_GPIO_INTR_ENABLE, (1 << POWER_KEY_PIN));
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }

    return 0;
}

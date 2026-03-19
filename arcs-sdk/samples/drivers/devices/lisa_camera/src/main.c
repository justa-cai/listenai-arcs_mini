/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief LISA Camera 摄像头驱动示例
 *
 * 演示内容：
 * 1. 初始化摄像头设备并配置总线接口（DVP）
 * 2. 配置摄像头参数（分辨率、像素格式等）
 * 3. 捕获图像帧并通过串口发送
 *
 * 注意：本示例需要实际连接摄像头硬件才能正常运行
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "lisa_device.h"
#include "lisa_camera.h"
#include "lisa_thread.h"

#include "lisa_gpio.h"
#include "lisa_uart.h"
#include "IOMuxManager.h"
#include "pinmux.h"

#define TAG "sample_camera"
#include <lisa_log.h>

/* 摄像头设备名称 */
#define CAMERA_DEVICE    "camera"

/* DVP设备名称 */
#define DVP_DEVICE       "dvp0"

/* DMA 通道 */
#define DMA_CHANNEL      2

/* 串口发送帧数限制 */
#define MAX_SEND_FRAMES  20

/* ========================================================================
 * 串口配置
 * ======================================================================== */

static lisa_device_t *uart_dev = NULL;
static volatile bool uart_tx_done = true;  /* 发送完成标志 */

/* UART 发送缓冲区 (放在 PSRAM) */
static uint8_t uart_buf[640 * 480 * 2] __attribute__((section(".psram.data"))) = {0};

/**
 * @brief UART 事件回调函数
 */
static void uart_event_callback(lisa_uart_event_t event, void *user_data)
{
    if (event == LISA_UART_EVENT_TX_DONE) {
        uart_tx_done = true;
    }
}

static int serial_init(void)
{
    /* 获取 UART 设备 */
    uart_dev = lisa_device_get("uart1");
    if (!lisa_device_ready(uart_dev)) {
        LOGE("UART1 device not ready");
        return -1;
    }

    /* 配置 UART: 3Mbps, 8N1, DMA 模式 */
    lisa_uart_config_t uart_cfg = {
        .baudrate = 3000000,
        .data_bits = LISA_UART_DATA_BITS_8,
        .stop_bits = LISA_UART_STOP_BITS_1,
        .parity = LISA_UART_PARITY_NONE,
        .flow_ctrl = LISA_UART_FLOW_CONTROL_NONE,
        .transfer_mode = LISA_UART_TRANSFER_MODE_DMA,
        .dma_tx_channel = 0xFF,  /* 自动分配 */
        .dma_rx_channel = 0xFF,
    };

    int ret = lisa_uart_configure(uart_dev, &uart_cfg);
    if (ret != LISA_DEVICE_OK) {
        LOGE("UART configure failed: %d", ret);
        return ret;
    }

    /* 设置事件回调 */
    lisa_uart_set_callback(uart_dev, uart_event_callback, NULL);

    return 0;
}

/**
 * @brief 检查上一次发送是否完成
 */
static bool serial_tx_ready(void)
{
    return uart_tx_done;
}

/**
 * @brief 异步发送数据
 */
static void serial_send(const uint8_t *data, uint32_t len)
{
    LOGI("Send frame: %02x %02x %02x ... %02x %02x %02x, len=%u",
         data[0], data[1], data[2],
         data[len - 3], data[len - 2], data[len - 1], len);

    /* 标记为发送中 */
    uart_tx_done = false;

    /* 异步发送 */
    int ret = lisa_uart_write_async(uart_dev, data, len);
    if (ret < 0) {
        LOGE("UART send failed: %d", ret);
        uart_tx_done = true;  /* 发送失败，恢复标志 */
    }
}

/* ========================================================================
 * DVP 引脚配置
 * ======================================================================== */

#ifdef CONFIG_BOARD_ARCS_EVB

#define CAM_PWDN_PIN    7

#define CAM_HSYNC_PIN   10
#define CAM_VSYNC_PIN   11
#define CAM_PCLK_PIN    12
#define CAM_MCLK_PIN    26
#define CAM_D0_PIN      13
#define CAM_D1_PIN      14
#define CAM_D2_PIN      15
#define CAM_D3_PIN      16
#define CAM_D4_PIN      17
#define CAM_D5_PIN      18
#define CAM_D6_PIN      19
#define CAM_D7_PIN      20

void lisa_gpiob_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, CAM_PWDN_PIN, CSK_IOMUX_FUNC_DEFAULT);
}

void lisa_uart1_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 21, CSK_IOMUX_FUNC_ALTER3);
}

void lisa_i2c0_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 22, 8);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 23, 8);
}

void lisa_dvp_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_HSYNC_PIN, CSK_IOMUX_FUNC_ALTER16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_VSYNC_PIN, CSK_IOMUX_FUNC_ALTER16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_PCLK_PIN, CSK_IOMUX_FUNC_ALTER16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D0_PIN, CSK_IOMUX_FUNC_ALTER16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D1_PIN, CSK_IOMUX_FUNC_ALTER16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D2_PIN, CSK_IOMUX_FUNC_ALTER16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D3_PIN, CSK_IOMUX_FUNC_ALTER16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D4_PIN, CSK_IOMUX_FUNC_ALTER16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D5_PIN, CSK_IOMUX_FUNC_ALTER16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D6_PIN, CSK_IOMUX_FUNC_ALTER16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D7_PIN, CSK_IOMUX_FUNC_ALTER16);
}

#elif CONFIG_BOARD_ARCS_MINI
/* ARCS_MINI: DVP/I2C0/GPIOA/GPIOB pinmux 均已在 pinmux.c 中配置，
 * 引脚宏（CAM_*_PIN、CAMERA_RST_PIN 等）来自 pinmux.h */
#endif

int main(int argc, char **argv)
{
    int ret;

    LOGI("=== LISA Camera Driver Sample ===");

    /* 初始化串口 */
    if (serial_init() != 0) {
        LOGE("Serial init failed");
        return -1;
    }
    LOGI("Serial initialized (3Mbps)");

    /* 获取摄像头设备 */
    lisa_device_t *camera_dev = lisa_device_get(CAMERA_DEVICE);
    if (!lisa_device_ready(camera_dev)) {
        LOGE("Error: %s device not ready", CAMERA_DEVICE);
        return -1;
    }
    LOGI("%s device ready", CAMERA_DEVICE);


    lisa_device_t *i2c_dev = lisa_device_get("i2c0");
    if (!lisa_device_ready(i2c_dev)) {
        LOGE("Error: %s device not ready", "i2c0");
        return -1;
    }

    /* 配置摄像头参数 */
    lisa_camera_config_t config = {
        .hw_config = {
            .mclk_pad = CSK_IOMUX_PAD_A,
            .mclk_pin = CAM_MCLK_PIN,
#ifdef CONFIG_BOARD_ARCS_EVB
            .pwdn_gpio_dev = lisa_device_get("gpiob"),
            .pwdn_pin = CAM_PWDN_PIN,
            .pwdn_delay_us = 0,
#endif
            .xclk_delay_us = 0,
            .i2c_dev = i2c_dev,
        },
        .xclk_freq_hz = 18000000,
        .fb_count = 3,
        .enable_hmirror = false,
        .enable_vflip = false,
        .enable_colorbar = false,
    };

    LOGI("Setting up camera...");
    ret = lisa_camera_setup(camera_dev, &config);
    if (ret != LISA_DEVICE_OK) {
        LOGE("Failed to setup camera: %d", ret);
        return -1;
    }

    lisa_camera_capabilities_t caps;
    ret = lisa_camera_get_capabilities(camera_dev, &caps);
    if (ret != LISA_DEVICE_OK) {
        LOGE("Failed to get capabilities: %d", ret);
        return -1;
    }
    LOGI("Camera capabilities: max_width=%u, max_height=%u, supported_formats=0x%08X",
         caps.max_width, caps.max_height, caps.supported_formats);

    /* 配置 DVP 总线接口 */
    lisa_camera_bus_config_t bus_config = {
        .dma_channel = DMA_CHANNEL,
        .bus_type = LISA_CAMERA_BUS_DVP,
        .config.dvp = {
            .dvp_dev        = lisa_device_get("dvp0"),
            .dvp_freq       = config.xclk_freq_hz,
            .data_align     = 1,
            .line_offset    = 0,
            .pixel_offset   = 0,
            .pclk_polarity  = 0,
            .vsync_polarity = 1,
            .hsync_polarity = 1,
        }
    };
    lisa_camera_get_framesize(camera_dev, &bus_config.width, &bus_config.height);
    bus_config.pixel_format = lisa_camera_get_pixformat(camera_dev);

    ret = lisa_camera_attach_bus(camera_dev, &bus_config);
    if (ret != LISA_DEVICE_OK) {
        LOGE("Failed to attach bus: %d", ret);
        return -1;
    }

    /* 启动摄像头 */
    LOGI("Starting camera...");
    ret = lisa_camera_start(camera_dev);
    if (ret != LISA_DEVICE_OK) {
        LOGE("Failed to start camera: %d", ret);
        return -1;
    }

    /* 主循环：捕获图像并通过串口发送 */
    int send_cnt = 0;
    while (1) {
        lisa_camera_fb_t *fb = NULL;

        /* 捕获一帧图像 */
        ret = lisa_camera_capture(camera_dev, &fb);
        if (ret != LISA_DEVICE_OK || fb == NULL) {
            LOGE("Capture failed: %d", ret);
            lisa_thread_mdelay(100);
            continue;
        }

        LOGI("Captured frame: %ux%u, len=%u, ts=%u",
                fb->width, fb->height, fb->len, fb->timestamp);

        /* 通过串口发送图像数据 (限制发送帧数) */
        if (send_cnt < MAX_SEND_FRAMES && serial_tx_ready()) {
            /* 复制到发送缓冲区 */
            uint32_t copy_len = (fb->len < sizeof(uart_buf)) ? fb->len : sizeof(uart_buf);
            memcpy(uart_buf, fb->buf, copy_len);

            /* 异步发送数据 */
            serial_send(uart_buf, copy_len);
            send_cnt++;

            LOGI("Frame %d sending...", send_cnt);
        }

        /* 立即释放帧缓冲区，不等待发送完成 */
        lisa_camera_release_fb(camera_dev, fb);
    }

    /* 停止摄像头 (不会执行到这里) */
    lisa_camera_stop(camera_dev);

    return 0;
}

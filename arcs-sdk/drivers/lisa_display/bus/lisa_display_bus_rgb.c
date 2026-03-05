/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lisa_display_bus_rgb.c
 * @brief RGB 并行总线驱动实现（支持独立命令通道）
 *
 * 架构说明：
 * - 数据通道：RGB 并行接口，用于高速像素传输
 * - 命令通道：可选的 3-Wire SPI 或 I2C，用于屏幕配置
 * - 对外API保持不变，内部自动路由命令到独立通道
 */

#include "lisa_device.h"
#include "lisa_display.h"
#include "lisa_display_bus.h"
#include "lisa_rgb.h"
#include <string.h>

#define LOG_TAG "rgb_bus"
#include "lisa_log.h"


/* ========================================================================
 * RGB 总线私有数据
 * ======================================================================== */

typedef struct {
    lisa_device_t *rgb_dev;       // RGB 控制器设备
    /* 注意：命令通道由 Panel 层的 cmd_bus_dev 管理，RGB 总线不需要保存 */
} rgb_bus_priv_t;

/* ========================================================================
 * RGB 总线 API 实现
 * ======================================================================== */

/**
 * @brief 附加并初始化 RGB 总线
 */
static int rgb_bus_attach(lisa_device_t *bus_dev, lisa_display_bus_type_t bus_type,
                          const lisa_display_bus_config_u *bus_config)
{
    LOGI("bus_type:%d", bus_type);
    if (bus_type != LISA_DISPLAY_BUS_RGB) {
        LOGE("bus type unmatch.");
        return LISA_DEVICE_ERR_INVALID;
    }

    // 获取实际的 RGB 控制器设备（而不是总线设备）
    lisa_device_t *rgb_dev = bus_config->rgb.rgb_dev;
    if (!rgb_dev) {
        LOGE("RGB device not specified in bus_config");
        return LISA_DEVICE_ERR_INVALID;
    }

    // 保存 RGB 设备引用到私有数据
    rgb_bus_priv_t *priv = (rgb_bus_priv_t *)bus_dev->priv_data;
    priv->rgb_dev = rgb_dev;

    if (lisa_rgb_setup(rgb_dev, &bus_config->rgb) != LISA_DEVICE_OK) {
        LOGE("lisa_rgb_setup failed.");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    LISA_LOGI(LOG_TAG, "RGB bus attached");

    // 初始化 RGB 控制器
    return lisa_rgb_start(rgb_dev);
}

/**
 * @brief 传输命令和数据
 *
 * 实现说明：
 * - RGB 并行接口本身不支持命令传输
 * - 命令传输由 Panel 层自动路由到独立的 cmd_bus_dev (软件 SPI 等)
 * - 此函数不应该被调用，如果被调用则说明配置有问题
 */
static int rgb_bus_trans_cmd_data(lisa_device_t *bus_dev, uint32_t cmd, uint8_t cmd_bits,
                                   const void *data, size_t len)
{
    return LISA_DEVICE_OK;
}

/**
 * @brief 发送像素数据
 */
static int rgb_bus_write_pixels(lisa_device_t *bus_dev, const void *pixels, size_t len)
{
    rgb_bus_priv_t *priv = (rgb_bus_priv_t *)bus_dev->priv_data;

    return lisa_rgb_update_framebuffer(priv->rgb_dev, pixels, len);
}

/**
 * @brief 传输控制（RGB 模式无 CS 信号，空实现）
 */
static void rgb_bus_transfer_control(lisa_device_t *bus_dev, bool enable)
{
    // RGB 模式无 CS 信号控制，空实现
}

/**
 * @brief 等待传输完成
 */
static int rgb_bus_wait_for_completion(lisa_device_t *bus_dev, int32_t timeout_ms)
{

    return LISA_DEVICE_OK;
}

/* ========================================================================
 * RGB 总线设备注册
 * ======================================================================== */

static const lisa_display_bus_api_t rgb_bus_api = {
    .attach = rgb_bus_attach,
    .trans_cmd_data = rgb_bus_trans_cmd_data,
    .write_pixels = rgb_bus_write_pixels,
    .transfer_control = rgb_bus_transfer_control,
    .wait_for_completion = rgb_bus_wait_for_completion,
};

static rgb_bus_priv_t rgb_bus_priv;

static int rgb_bus_device_init(void)
{
    memset(&rgb_bus_priv, 0, sizeof(rgb_bus_priv));
    return LISA_DEVICE_OK;
}

LISA_DEVICE_REGISTER(panel_bus_rgb, &rgb_bus_api, &rgb_bus_priv, NULL,
                     rgb_bus_device_init, LISA_DEVICE_PRIORITY_HIGH);

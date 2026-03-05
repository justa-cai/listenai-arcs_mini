/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "lisa_display.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Display 总线驱动接口 API 结构体
 *
 * 这是对显示数据总线的抽象，每个具体的数据总线驱动（第3层）
 * 都必须实现该接口，供上层 Panel 驱动（第2层）调用。
 *
 * 架构说明：
 * - SPI/QSPI 模式：命令和数据共用同一总线，trans_cmd_data 和 write_pixels 都可用
 * - RGB 模式：仅数据传输，trans_cmd_data 通过独立的命令通道实现（见 bus 私有数据）
 */
typedef struct {
    /**
     * @brief 附加并初始化总线
     *
     * @param bus_dev 总线设备实例
     * @param bus_type 总线类型
     * @param bus_config 总线配置信息
     * @return 0 成功, <0 失败
     */
    int (*attach)(lisa_device_t *bus_dev,lisa_display_bus_type_t bus_type, const lisa_display_bus_config_u *bus_config);

    /**
     * @brief 传输命令和数据
     *
     * 实现说明：
     * - SPI/QSPI 模式：直接通过数据总线发送
     * - RGB 模式：内部路由到独立的命令通道（SPI/I2C）
     *
     * @param bus_dev 总线设备实例
     * @param cmd 要发送的命令
     * @param cmd_bits 命令位宽
     * @param data 指向数据的指针
     * @param len 数据长度
     * @return 0 成功, <0 失败
     */
    int (*trans_cmd_data)(lisa_device_t *bus_dev, uint32_t cmd, uint8_t cmd_bits, const void *data, size_t len);

    /**
     * @brief 发送像素数据（用于刷新屏幕）
     *
     * 这个接口可以为特定总线进行优化，例如使用DMA。
     *
     * @param bus_dev 总线设备实例
     * @param pixels 指向像素数据缓冲区的指针
     * @param len 缓冲区大小（字节）
     * @return 0 成功, <0 失败
     */
    int (*write_pixels)(lisa_device_t *bus_dev, const void *pixels, size_t len);

    /**
     * @brief 控制传输周期，CS 信号管理
     *
     * 在进行批量数据传输（如分块旋转传输）时调用，
     * enable=true 时 CS 信号保持低电平用于连续传输，
     * enable=false 时 CS 信号拉高结束当前传输周期。
     * 这可以减少 CS 信号操作次数，提高传输效率。
     *
     * @param bus_dev 总线设备实例
     * @param enable true: 启用连续传输模式（CS 保持低电平）, false: 结束连续传输（CS 拉高）
     */
    void (*transfer_control)(lisa_device_t *bus_dev, bool enable);

    /**
     * @brief 等待异步传输完成
     *
     * 等待由 write_pixels_async 启动的异步 DMA 传输完成。
     *
     * @param bus_dev 总线设备实例
     * @param timeout_ms 超时时间（毫秒），0 表示无限等待
     * @return 0 传输成功完成, <0 失败或超时
     */
    int (*wait_for_completion)(lisa_device_t *bus_dev, int32_t timeout_ms);

} lisa_display_bus_api_t;

/**
 * @brief Display 命令总线 API 结构体
 *
 * 专门用于命令总线的接口,仅负责发送配置命令和读取屏幕参数,
 * 不负责像素数据传输。
 *
 * 适用场景:
 * - RGB 并行接口 + 独立 SPI/I2C 命令通道
 * - 需要分离命令和数据传输的显示架构
 */
typedef struct {
    /**
     * @brief 配置命令总线
     *
     * 初始化命令总线的 GPIO、时序等参数。
     *
     * @param cmd_bus_type 命令总线类型
     * @param cmd_bus_config 命令总线配置
     * @return 0 成功, <0 失败
     */
    int (*configure)(lisa_display_cmd_bus_type_t cmd_bus_type,
                     const lisa_display_cmd_bus_config_u *cmd_bus_config);

    /**
     * @brief 发送命令和参数数据
     *
     * 用于发送屏幕配置命令,例如设置显示方向、亮度等。
     *
     * @param cmd 命令字节
     * @param cmd_bits 命令位宽(通常为 8)
     * @param data 参数数据缓冲区
     * @param len 参数数据长度
     * @return 0 成功, <0 失败
     */
    int (*write_cmd)(uint32_t cmd, uint8_t cmd_bits, const void *data, size_t len);

    /**
     * @brief 读取屏幕配置参数
     *
     * 用于读取屏幕寄存器配置,例如读取显示 ID、状态等。
     *
     * @param cmd 命令字节
     * @param cmd_bits 命令位宽(通常为 8)
     * @param data 接收数据缓冲区
     * @param len 要读取的数据长度
     * @return 0 成功, <0 失败
     */
    int (*read_cmd)(uint32_t cmd, uint8_t cmd_bits, void *data, size_t len);

} lisa_display_cmd_bus_api_t;

#ifdef __cplusplus
}
#endif
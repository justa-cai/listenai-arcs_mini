/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lisa_uart.h
 * @brief LISA UART 设备驱动接口
 */

#pragma once

#include "lisa_device.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * UART 类型定义
 * ======================================================================== */

/**
 * @brief UART 波特率枚举
 */
typedef enum {
    LISA_UART_BAUDRATE_1200 = 1200,
    LISA_UART_BAUDRATE_2400 = 2400,
    LISA_UART_BAUDRATE_4800 = 4800,
    LISA_UART_BAUDRATE_9600 = 9600,
    LISA_UART_BAUDRATE_19200 = 19200,
    LISA_UART_BAUDRATE_38400 = 38400,
    LISA_UART_BAUDRATE_57600 = 57600,
    LISA_UART_BAUDRATE_115200 = 115200,
    LISA_UART_BAUDRATE_230400 = 230400,
    LISA_UART_BAUDRATE_460800 = 460800,
    LISA_UART_BAUDRATE_921600 = 921600,
} lisa_uart_baudrate_t;

/**
 * @brief UART 数据位
 */
typedef enum {
    LISA_UART_DATA_BITS_5 = 5, /* 5 数据位 */
    LISA_UART_DATA_BITS_6 = 6, /* 6 数据位 */
    LISA_UART_DATA_BITS_7 = 7, /* 7 数据位 */
    LISA_UART_DATA_BITS_8 = 8, /* 8 数据位 */
} lisa_uart_data_bits_t;

/**
 * @brief UART 停止位
 */
typedef enum {
    LISA_UART_STOP_BITS_1 = 0,   /* 1 停止位 */
    LISA_UART_STOP_BITS_1_5 = 1, /* 1.5 停止位 */
    LISA_UART_STOP_BITS_2 = 2,   /* 2 停止位 */
} lisa_uart_stop_bits_t;

/**
 * @brief UART 校验位
 */
typedef enum {
    LISA_UART_PARITY_NONE = 0, /* 无校验 */
    LISA_UART_PARITY_ODD = 1,  /* 奇校验 */
    LISA_UART_PARITY_EVEN = 2, /* 偶校验 */
} lisa_uart_parity_t;

/**
 * @brief UART 流控制
 */
typedef enum {
    LISA_UART_FLOW_CONTROL_NONE = 0,     /* 无流控 */
    LISA_UART_FLOW_CONTROL_RTS_CTS = 1,  /* RTS/CTS 硬件流控 */
    LISA_UART_FLOW_CONTROL_XON_XOFF = 2, /* XON/XOFF 软件流控 */
} lisa_uart_flow_control_t;

/**
 * @brief UART 传输模式
 */
typedef enum {
    LISA_UART_TRANSFER_MODE_INTERRUPT = 0, /* 中断模式 */
    LISA_UART_TRANSFER_MODE_DMA = 1,       /* DMA 模式 */
} lisa_uart_transfer_mode_t;

/**
 * @brief UART 循环接收缓冲区配置
 */
typedef struct {
    uint32_t buffer_count;    /* 循环缓冲区数量 (2=Ping-Pong, 3/4/...=多缓冲, 0=禁用) */
    uint32_t buffer_size;     /* 单个缓冲区大小 (字节) */
} lisa_uart_rx_buf_config_t;

/**
 * @brief UART 配置结构体
 */
typedef struct {
    uint32_t baudrate;                       /* 波特率 */
    lisa_uart_data_bits_t data_bits;         /* 数据位 */
    lisa_uart_stop_bits_t stop_bits;         /* 停止位 */
    lisa_uart_parity_t parity;               /* 校验位 */
    lisa_uart_flow_control_t flow_ctrl;      /* 流控制 */
    lisa_uart_transfer_mode_t transfer_mode; /* 传输模式（中断/DMA） */
    lisa_uart_rx_buf_config_t rx_buf_config; /* 循环接收缓冲区配置 */
    uint8_t dma_tx_channel;                  /* DMA TX 通道 (0-3, 或 0xFF 自动分配) */
    uint8_t dma_rx_channel;                  /* DMA RX 通道 (0-3, 或 0xFF 自动分配) */
} lisa_uart_config_t;

/**
 * @brief UART 事件类型
 */
typedef enum {
    LISA_UART_EVENT_RX_READY = 0x01,    /* 接收数据就绪 */
    LISA_UART_EVENT_TX_DONE = 0x02,     /* 发送完成 */
    LISA_UART_EVENT_ERROR = 0x04,       /* 错误事件 */
    LISA_UART_EVENT_BREAK = 0x08,       /* 检测到 BREAK 信号 */
    LISA_UART_EVENT_OVERRUN = 0x10,     /* 接收溢出 */
    LISA_UART_EVENT_PARITY_ERROR = 0x20,/* 校验错误 */
    LISA_UART_EVENT_FRAME_ERROR = 0x40, /* 帧错误 */
    LISA_UART_EVENT_RX_TIMEOUT = 0x80,  /* 接收超时（空闲中断） */
} lisa_uart_event_t;


/**
 * @brief UART 事件回调函数类型
 *
 * @param event 事件类型
 * @param user_data 用户数据指针
 *
 * @note 如果需要访问设备，可以通过 user_data 传入设备指针
 */
typedef void (*lisa_uart_callback_t)(lisa_uart_event_t event, void *user_data);

/* ========================================================================
 * UART 设备 API 结构体
 * ======================================================================== */

typedef struct {
    int (*configure)(lisa_device_t *dev, const lisa_uart_config_t *config);
    int (*get_config)(lisa_device_t *dev, lisa_uart_config_t *config);
    int (*write_sync)(lisa_device_t *dev, const uint8_t *buf, uint32_t len, uint32_t timeout_ms);
    int (*read_sync)(lisa_device_t *dev, uint8_t *buf, uint32_t len, uint32_t timeout_ms);
    int (*rx_enable)(lisa_device_t *dev);
    int (*rx_disable)(lisa_device_t *dev);
    int (*poll_in)(lisa_device_t *dev, uint8_t *byte);
    void (*poll_out)(lisa_device_t *dev, uint8_t byte);
#ifdef CONFIG_LISA_UART_ASYNC_API
    int (*write_async)(lisa_device_t *dev, const uint8_t *buf, uint32_t len);
    int (*set_callback)(lisa_device_t *dev, lisa_uart_callback_t callback, void *user_data);
    int (*write_abort)(lisa_device_t *dev);
    uint32_t (*get_tx_count)(lisa_device_t *dev);
#endif
} lisa_uart_api_t;

/* ========================================================================
 * UART 对外接口函数
 * ======================================================================== */

/* ===== 配置接口 ===== */

/**
 * @brief 配置UART
 *
 * 配置UART的工作参数。
 * 设备初始化后不会自动配置，必须显式调用此函数进行配置。
 *
 * @param dev UART设备指针
 * @param config 配置参数结构体指针，包含：
 *               - baudrate: 波特率
 *               - data_bits: 数据位（5-8）
 *               - stop_bits: 停止位（1, 1.5, 2）
 *               - parity: 校验位（无/奇/偶）
 *               - flow_ctrl: 流控制（无/硬件/软件）
 *               - transfer_mode: 传输模式（中断/DMA）
 *               - rx_buf_config: 循环接收缓冲区配置（可选）
 *               - dma_tx_channel: DMA TX通道号（0-3，或0xFF自动分配，仅DMA模式有效）
 *               - dma_rx_channel: DMA RX通道号（0-3，或0xFF自动分配，仅DMA模式有效）
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return LISA_DEVICE_ERR_NO_MEM 内存分配失败
 * @return <0 其他错误
 *
 * @note 配置前请确保设备已初始化
 * @note 此函数会创建必要的同步信号量和循环缓冲区
 * @note 可以多次调用以重新配置设备，旧的配置会被覆盖
 * @note DMA模式下，如果dma_tx_channel和dma_rx_channel设置为0xFF，驱动将自动分配可用通道
 * @note 系统共有4个DMA通道（0-3），请根据实际需求合理分配
 */
static inline int lisa_uart_configure(lisa_device_t *dev, const lisa_uart_config_t *config)
{
    if (!dev || !dev->api || !config) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_uart_api_t *api = (lisa_uart_api_t *)dev->api;
    return api->configure ? api->configure(dev, config) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 获取UART当前配置
 *
 * 读取UART的当前配置信息。
 *
 * @param dev UART设备指针
 * @param config 输出参数，用于接收配置信息
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_uart_get_config(lisa_device_t *dev, lisa_uart_config_t *config)
{
    if (!dev || !dev->api || !config) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_uart_api_t *api = (lisa_uart_api_t *)dev->api;
    return api->get_config ? api->get_config(dev, config) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/* ===== 数据传输接口 ===== */

/**
 * @brief 使能UART接收
 *
 * 启动UART接收功能。
 *
 * @param dev UART设备指针
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_READY 设备未配置
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 *
 * @note 调用此函数前必须先调用 lisa_uart_configure() 配置设备
 */
static inline int lisa_uart_rx_enable(lisa_device_t *dev)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_uart_api_t *api = (lisa_uart_api_t *)dev->api;
    return api->rx_enable ? api->rx_enable(dev) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 禁用UART接收
 *
 * 停止UART接收功能。
 *
 * @param dev UART设备指针
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_READY 设备未配置
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 *
 * @note 调用此函数前必须先调用 lisa_uart_configure() 配置设备
 */
static inline int lisa_uart_rx_disable(lisa_device_t *dev)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_uart_api_t *api = (lisa_uart_api_t *)dev->api;
    return api->rx_disable ? api->rx_disable(dev) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief UART同步写数据
 *
 * 向UART发送数据（阻塞）。函数会等待数据发送完成后才返回。
 * 内部通过异步发送+信号量的方式实现，避免轮询，降低CPU占用。
 *
 * @param dev UART设备指针
 * @param buf 数据缓冲区指针
 * @param len 数据长度
 * @param timeout_ms 超时时间（毫秒）
 *
 * @return >=0 实际发送的字节数
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_READY 设备未配置
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return LISA_DEVICE_ERR_BUSY 设备忙
 * @return LISA_DEVICE_ERR_TIMEOUT 发送超时
 * @return <0 其他错误
 *
 * @note 调用此函数前必须先调用 lisa_uart_configure() 配置设备
 * @note 此函数会阻塞直到发送完成或超时
 * @note 不应在中断上下文中调用此函数
 */
static inline int lisa_uart_write_sync(lisa_device_t *dev, const uint8_t *buf, uint32_t len, uint32_t timeout_ms)
{
    if (!dev || !dev->api || !buf || len == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_uart_api_t *api = (lisa_uart_api_t *)dev->api;
    return api->write_sync ? api->write_sync(dev, buf, len, timeout_ms) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief UART同步读数据
 *
 * 从UART接收数据（阻塞）。函数在以下情况下返回：
 * 1. 接收到指定长度的数据 -> 返回实际接收长度
 * 2. 检测到空闲中断（不定长数据接收完成）-> 返回实际接收长度
 * 3. 缓冲区溢出，驱动已停止接收 -> 返回 LISA_DEVICE_ERR_OVERFLOW
 *
 * 内部通过循环缓冲区+信号量的方式实现，避免轮询，降低CPU占用。
 *
 * @param dev UART设备指针
 * @param buf 数据缓冲区指针，用于存储接收的数据
 * @param len 期望接收的数据长度
 *
 * @return >=0 实际接收的字节数
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_READY 设备未配置或接收未使能
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return LISA_DEVICE_ERR_OVERFLOW 缓冲区溢出，需调用 rx_disable + rx_enable 恢复
 * @return <0 其他错误
 *
 * @note 调用此函数前必须先调用 lisa_uart_configure() 配置设备
 * @note 需要先调用 rx_enable 启动接收，否则返回 LISA_DEVICE_ERR_NOT_READY
 * @note 此函数会阻塞直到满足上述三种返回条件之一
 * @note 不应在中断上下文中调用此函数
 * @note 如果返回 LISA_DEVICE_ERR_OVERFLOW，需要外部调用 rx_disable + rx_enable 恢复接收
 */
static inline int lisa_uart_read_sync(lisa_device_t *dev, uint8_t *buf, uint32_t len)
{
    if (!dev || !dev->api || !buf || len == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_uart_api_t *api = (lisa_uart_api_t *)dev->api;
    return api->read_sync ? api->read_sync(dev, buf, len, 0xFFFFFFFFU) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/* ===== 轮询接口 ===== */

/**
 * @brief 轮询接收单个字节（非阻塞）
 *
 * 接收一个字节，没有收到会立即返回
 *
 * @param dev UART设备指针
 * @param byte 输出参数，接收到的字节
 *
 * @return 0 成功接收到数据
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_READY 设备未配置
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return LISA_DEVICE_ERR_TIMEOUT 无数据可读
 * @return <0 其他错误
 *
 * @note 调用此函数前必须先调用 lisa_uart_configure() 配置设备
 */
static inline int lisa_uart_poll_in(lisa_device_t *dev, uint8_t *byte)
{
    if (!dev || !dev->api || !byte) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_uart_api_t *api = (lisa_uart_api_t *)dev->api;
    return api->poll_in ? api->poll_in(dev, byte) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 发送单个字节（阻塞）
 *
 * 发送一个字节，等待直到字节发送完成
 *
 * @param dev UART设备指针
 * @param byte 要发送的字节
 *
 * @note 调用此函数前必须先调用 lisa_uart_configure() 配置设备
 * @note 此函数会阻塞等待直到字节发送完成
 * @note 调用者需要确保 dev 和 dev->api 有效
 */
static inline void lisa_uart_poll_out(lisa_device_t *dev, uint8_t byte)
{
    if (!dev || !dev->api) {
        return;
    }
    lisa_uart_api_t *api = (lisa_uart_api_t *)dev->api;
    if (api->poll_out) {
        api->poll_out(dev, byte);
    }
}

/* ===== 异步接口 ===== */

#ifdef CONFIG_LISA_UART_ASYNC_API
/**
 * @brief UART异步写数据
 *
 * 向UART发送数据（非阻塞）。函数立即返回，不等待发送完成。
 * 需要配合事件回调或轮询状态来确认发送完成。
 *
 * @param dev UART设备指针
 * @param buf 数据缓冲区指针
 * @param len 数据长度
 *
 * @return >=0 实际发送的字节数
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_READY 设备未配置
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return LISA_DEVICE_ERR_BUSY 设备忙
 * @return <0 其他错误
 *
 * @note 调用此函数前必须先调用 lisa_uart_configure() 配置设备
 */
static inline int lisa_uart_write_async(lisa_device_t *dev, const uint8_t *buf, uint32_t len)
{
    if (!dev || !dev->api || !buf || len == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_uart_api_t *api = (lisa_uart_api_t *)dev->api;
    return api->write_async ? api->write_async(dev, buf, len) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 设置UART事件回调函数
 *
 * 注册UART事件回调函数，用于异步通知。
 *
 * @param dev UART设备指针
 * @param callback 回调函数指针
 * @param user_data 用户数据指针，将在回调时传递
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_uart_set_callback(lisa_device_t *dev, lisa_uart_callback_t callback, void *user_data)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_uart_api_t *api = (lisa_uart_api_t *)dev->api;
    return api->set_callback ? api->set_callback(dev, callback, user_data) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 终止UART异步写操作
 *
 * 终止正在进行的异步写操作。
 *
 * @param dev UART设备指针
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_READY 设备未配置
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 *
 * @note 调用此函数前必须先调用 lisa_uart_configure() 配置设备
 */
static inline int lisa_uart_write_abort(lisa_device_t *dev)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_uart_api_t *api = (lisa_uart_api_t *)dev->api;
    return api->write_abort ? api->write_abort(dev) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 获取已发送的字节数
 *
 * 在 异步 模式下，可用于查询当前传输进度。
 *
 * @param dev UART设备指针
 *
 * @return 已发送的字节数（如果设备未配置则返回0）
 *
 * @note 调用此函数前必须先调用 lisa_uart_configure() 配置设备
 */
static inline uint32_t lisa_uart_get_tx_count(lisa_device_t *dev)
{
    if (!dev || !dev->api) {
        return 0;
    }
    lisa_uart_api_t *api = (lisa_uart_api_t *)dev->api;
    return api->get_tx_count ? api->get_tx_count(dev) : 0;
}
#endif

/* ========================================================================
 * 便捷宏定义
 * ======================================================================== */

/**
 * @brief 默认UART配置（115200, 8N1, 无流控, 中断模式）
 */
#define LISA_UART_CONFIG_DEFAULT()                                                                                     \
    {                                                                                                                  \
        .baudrate = LISA_UART_BAUDRATE_115200,                                                                         \
        .data_bits = LISA_UART_DATA_BITS_8,                                                                            \
        .stop_bits = LISA_UART_STOP_BITS_1,                                                                            \
        .parity = LISA_UART_PARITY_NONE,                                                                               \
        .flow_ctrl = LISA_UART_FLOW_CONTROL_NONE,                                                                      \
        .transfer_mode = LISA_UART_TRANSFER_MODE_INTERRUPT,                                                            \
        .dma_tx_channel = 0xFF,                                                                                        \
        .dma_rx_channel = 0xFF,                                                                                        \
    }

/**
 * @brief 低速UART配置（9600, 8N1, 无流控, 中断模式）
 */
#define LISA_UART_CONFIG_LOW_SPEED()                                                                                   \
    {                                                                                                                  \
        .baudrate = LISA_UART_BAUDRATE_9600,                                                                           \
        .data_bits = LISA_UART_DATA_BITS_8,                                                                            \
        .stop_bits = LISA_UART_STOP_BITS_1,                                                                            \
        .parity = LISA_UART_PARITY_NONE,                                                                               \
        .flow_ctrl = LISA_UART_FLOW_CONTROL_NONE,                                                                      \
        .transfer_mode = LISA_UART_TRANSFER_MODE_INTERRUPT,                                                            \
        .dma_tx_channel = 0xFF,                                                                                        \
        .dma_rx_channel = 0xFF,                                                                                        \
    }

/**
 * @brief 高速UART配置（921600, 8N1, 无流控, 中断模式）
 */
#define LISA_UART_CONFIG_HIGH_SPEED()                                                                                  \
    {                                                                                                                  \
        .baudrate = LISA_UART_BAUDRATE_921600,                                                                         \
        .data_bits = LISA_UART_DATA_BITS_8,                                                                            \
        .stop_bits = LISA_UART_STOP_BITS_1,                                                                            \
        .parity = LISA_UART_PARITY_NONE,                                                                               \
        .flow_ctrl = LISA_UART_FLOW_CONTROL_NONE,                                                                      \
        .transfer_mode = LISA_UART_TRANSFER_MODE_INTERRUPT,                                                            \
        .dma_tx_channel = 0xFF,                                                                                        \
        .dma_rx_channel = 0xFF,                                                                                        \
    }

/**
 * @brief 带硬件流控的UART配置（115200, 8N1, RTS/CTS, 中断模式）
 */
#define LISA_UART_CONFIG_FLOW_CONTROL()                                                                                \
    {                                                                                                                  \
        .baudrate = LISA_UART_BAUDRATE_115200,                                                                         \
        .data_bits = LISA_UART_DATA_BITS_8,                                                                            \
        .stop_bits = LISA_UART_STOP_BITS_1,                                                                            \
        .parity = LISA_UART_PARITY_NONE,                                                                               \
        .flow_ctrl = LISA_UART_FLOW_CONTROL_RTS_CTS,                                                                   \
        .transfer_mode = LISA_UART_TRANSFER_MODE_INTERRUPT,                                                            \
        .dma_tx_channel = 0xFF,                                                                                        \
        .dma_rx_channel = 0xFF,                                                                                        \
    }

/**
 * @brief DMA模式UART配置（115200, 8N1, 无流控, DMA模式, 自动分配DMA通道）
 */
#define LISA_UART_CONFIG_DMA()                                                                                         \
    {                                                                                                                  \
        .baudrate = LISA_UART_BAUDRATE_115200,                                                                         \
        .data_bits = LISA_UART_DATA_BITS_8,                                                                            \
        .stop_bits = LISA_UART_STOP_BITS_1,                                                                            \
        .parity = LISA_UART_PARITY_NONE,                                                                               \
        .flow_ctrl = LISA_UART_FLOW_CONTROL_NONE,                                                                      \
        .transfer_mode = LISA_UART_TRANSFER_MODE_DMA,                                                                  \
        .dma_tx_channel = 0xFF,                                                                                        \
        .dma_rx_channel = 0xFF,                                                                                        \
    }

#ifdef __cplusplus
}
#endif

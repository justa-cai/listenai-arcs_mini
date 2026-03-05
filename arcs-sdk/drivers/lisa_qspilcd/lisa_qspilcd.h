/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lisa_qspilcd.h
 * @brief LISA QSPI LCD 设备驱动接口
 */

#pragma once

#include "lisa_device.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * QSPI LCD 配置数据结构
 * ======================================================================== */

/**
 * @brief QSPI LCD 控制码字段定义（与 HAL Driver_QSPI_LCD 保持一致）
 */
typedef uint32_t lisa_qspilcd_control_t;

/*----- QSPI DMA mode -----*/
#define LISA_QSPILCD_DMA_TX                  (0x0U)
#define LISA_QSPILCD_DMA_RX                  (0x1U)
#define LISA_QSPILCD_DMA_BOTH                (0x2U)

/*----- SPI Control Codes: Mode -----*/
#define LISA_QSPILCD_MODE_Pos                0
#define LISA_QSPILCD_MODE_Msk                (0xFUL << LISA_QSPILCD_MODE_Pos)
#define LISA_QSPILCD_MODE_UNSET              (0x00UL << LISA_QSPILCD_MODE_Pos)
#define LISA_QSPILCD_MODE_MASTER             (0x01UL << LISA_QSPILCD_MODE_Pos)
#define LISA_QSPILCD_MODE_SLAVE              (0x02UL << LISA_QSPILCD_MODE_Pos)

/*----- SPI Control Codes: TX I/O -----*/
#define LISA_QSPILCD_TXIO_Pos                4
#define LISA_QSPILCD_TXIO_Msk                (3UL << LISA_QSPILCD_TXIO_Pos)
#define LISA_QSPILCD_TXIO_UNSET              (0x00UL << LISA_QSPILCD_TXIO_Pos)
#define LISA_QSPILCD_TXIO_DMA                (0x01UL << LISA_QSPILCD_TXIO_Pos)
#define LISA_QSPILCD_TXIO_PIO                (0x02UL << LISA_QSPILCD_TXIO_Pos)
#define LISA_QSPILCD_TXIO_BOTH               (LISA_QSPILCD_TXIO_DMA | LISA_QSPILCD_TXIO_PIO)
#define LISA_QSPILCD_TXIO_AUTO               LISA_QSPILCD_TXIO_BOTH

/*----- SPI Control Codes: RX I/O -----*/
#define LISA_QSPILCD_RXIO_Pos                6
#define LISA_QSPILCD_RXIO_Msk                (3UL << LISA_QSPILCD_RXIO_Pos)
#define LISA_QSPILCD_RXIO_UNSET              (0x00UL << LISA_QSPILCD_RXIO_Pos)
#define LISA_QSPILCD_RXIO_DMA                (0x01UL << LISA_QSPILCD_RXIO_Pos)
#define LISA_QSPILCD_RXIO_PIO                (0x02UL << LISA_QSPILCD_RXIO_Pos)
#define LISA_QSPILCD_RXIO_BOTH               (LISA_QSPILCD_RXIO_DMA | LISA_QSPILCD_RXIO_PIO)
#define LISA_QSPILCD_RXIO_AUTO               LISA_QSPILCD_RXIO_BOTH

/*----- SPI Control Codes: Mode Parameters: Frame Format -----*/
#define LISA_QSPILCD_FRAME_FORMAT_Pos        8
#define LISA_QSPILCD_FRAME_FORMAT_Msk        (0xFUL << LISA_QSPILCD_FRAME_FORMAT_Pos)
#define LISA_QSPILCD_FRM_FMT_UNSET           (0UL << LISA_QSPILCD_FRAME_FORMAT_Pos)
#define LISA_QSPILCD_CPOL0_CPHA0             (1UL << LISA_QSPILCD_FRAME_FORMAT_Pos)
#define LISA_QSPILCD_CPOL0_CPHA1             (2UL << LISA_QSPILCD_FRAME_FORMAT_Pos)
#define LISA_QSPILCD_CPOL1_CPHA0             (3UL << LISA_QSPILCD_FRAME_FORMAT_Pos)
#define LISA_QSPILCD_CPOL1_CPHA1             (4UL << LISA_QSPILCD_FRAME_FORMAT_Pos)

/*----- SPI Control Codes: Mode Parameters: Data Bits -----*/
#define LISA_QSPILCD_DATA_BITS_Pos           12
#define LISA_QSPILCD_DATA_BITS_Msk           (0x3FUL << LISA_QSPILCD_DATA_BITS_Pos)
#define LISA_QSPILCD_DATA_BITS_UNSET         (0UL << LISA_QSPILCD_DATA_BITS_Pos)
#define LISA_QSPILCD_DATA_BITS(n)            (((n) & 0x3F) << LISA_QSPILCD_DATA_BITS_Pos)

/*----- SPI Control Codes: Mode Parameters: Bit Order -----*/
#define LISA_QSPILCD_BIT_ORDER_Pos           18
#define LISA_QSPILCD_BIT_ORDER_Msk           (3UL << LISA_QSPILCD_BIT_ORDER_Pos)
#define LISA_QSPILCD_BIT_ORDER_UNSET         (0UL << LISA_QSPILCD_BIT_ORDER_Pos)
#define LISA_QSPILCD_MSB_LSB                 (1UL << LISA_QSPILCD_BIT_ORDER_Pos)
#define LISA_QSPILCD_LSB_MSB                 (2UL << LISA_QSPILCD_BIT_ORDER_Pos)

/*----- SPI Control Codes: Transfer Controls -----*/
#define LISA_QSPILCD_CMD_PHASE_Pos           20
#define LISA_QSPILCD_CMD_PHASE_Msk           (0x1UL << LISA_QSPILCD_CMD_PHASE_Pos)
#define LISA_QSPILCD_CMD_PHASE_DISABLE       (0x0UL << LISA_QSPILCD_CMD_PHASE_Pos)
#define LISA_QSPILCD_CMD_PHASE_ENABLE        (0x1UL << LISA_QSPILCD_CMD_PHASE_Pos)

#define LISA_QSPILCD_ADDR_PHASE_Pos          21
#define LISA_QSPILCD_ADDR_PHASE_Msk          (0x1UL << LISA_QSPILCD_ADDR_PHASE_Pos)
#define LISA_QSPILCD_ADDR_PHASE_DISABLE      (0x0UL << LISA_QSPILCD_ADDR_PHASE_Pos)
#define LISA_QSPILCD_ADDR_PHASE_ENABLE       (0x1UL << LISA_QSPILCD_ADDR_PHASE_Pos)
#define LISA_QSPILCD_ADDR_PHASE_FMT_Pos      22
#define LISA_QSPILCD_ADDR_PHASE_FMT_Msk      (0x3UL << LISA_QSPILCD_ADDR_PHASE_FMT_Pos)
#define LISA_QSPILCD_ADDR_PHASE_1BYTES       (0x0UL << LISA_QSPILCD_ADDR_PHASE_FMT_Pos)
#define LISA_QSPILCD_ADDR_PHASE_2BYTES       (0x1UL << LISA_QSPILCD_ADDR_PHASE_FMT_Pos)
#define LISA_QSPILCD_ADDR_PHASE_3BYTES       (0x2UL << LISA_QSPILCD_ADDR_PHASE_FMT_Pos)
#define LISA_QSPILCD_ADDR_PHASE_4BYTES       (0x3UL << LISA_QSPILCD_ADDR_PHASE_FMT_Pos)

/*----- SPI Control Codes: Exclusive Controls -----*/
#define LISA_QSPILCD_EXCL_OP_Pos             24
#define LISA_QSPILCD_EXCL_OP_Msk             (0xFUL << LISA_QSPILCD_EXCL_OP_Pos)
#define LISA_QSPILCD_EXCL_OP_UNSET           (0UL << LISA_QSPILCD_EXCL_OP_Pos)
#define LISA_QSPILCD_SET_BUS_SPEED           (1UL << LISA_QSPILCD_EXCL_OP_Pos)
#define LISA_QSPILCD_GET_BUS_SPEED           (2UL << LISA_QSPILCD_EXCL_OP_Pos)
#define LISA_QSPILCD_ABORT_TRANSFER          (3UL << LISA_QSPILCD_EXCL_OP_Pos)
#define LISA_QSPILCD_RESET_FIFO              (4UL << LISA_QSPILCD_EXCL_OP_Pos)
#define LISA_QSPILCD_SET_ADV_ATTR            (5UL << LISA_QSPILCD_EXCL_OP_Pos)

/*----- SPI Control Codes: DMA enable/disable/size -----*/
#define LISA_QSPILCD_DMA_Pos                 28
#define LISA_QSPILCD_DMA_Msk                 (0x3UL << LISA_QSPILCD_DMA_Pos)
#define LISA_QSPILCD_DMA_UNSET               (0x0UL << LISA_QSPILCD_DMA_Pos)
#define LISA_QSPILCD_DMA_ENABLE              (0x1UL << LISA_QSPILCD_DMA_Pos)
#define LISA_QSPILCD_DMA_DISABLE             (0x2UL << LISA_QSPILCD_DMA_Pos)
#define LISA_QSPILCD_DMA_SIZE                (0x3UL << LISA_QSPILCD_DMA_Pos)

/**
 * @brief QSPI LCD 数据线数量
 */
typedef enum {
    LISA_QSPILCD_LANE_SINGLE = 0,
    LISA_QSPILCD_LANE_DUAL   = 1,
    LISA_QSPILCD_LANE_QUAD   = 2,
} lisa_qspilcd_lane_num_t;

/**
 * @brief QSPI LCD 硬件配置
 */
typedef struct lisa_qspilcd_config {
    uint8_t qspi_tx_dma_ch;     /**< QSPI TX DMA 通道号 */
    uint32_t qspi_sck_freq;     /**< QSPI 时钟频率（Hz），0 表示使用默认值 */
    bool cs_active_high;        /**< 片选为高电平有效，默认低电平有效 */
} lisa_qspilcd_config_t;

/**
 * @brief QSPI LCD 传输描述
 */
typedef struct {
    const void *buf;                   /**< 传输数据缓冲区 */
    uint32_t size_bytes;               /**< 数据大小（字节） */
    lisa_qspilcd_lane_num_t lane;      /**< 数据线模式 */
    uint8_t data_bits;                 /**< 数据位宽 */
    bool use_dma;                      /**< true 使用 DMA，false 使用 PIO */
} lisa_qspilcd_xfer_t;

/* ========================================================================
 * QSPI LCD 设备 API
 * ======================================================================== */

typedef struct {
    int (*transfer)(lisa_device_t *dev, const lisa_qspilcd_xfer_t *xfer);
    int (*wait_done)(lisa_device_t *dev, uint32_t timeout_ms);
    int (*cs_configure)(lisa_device_t *dev, lisa_device_t *gpio_dev, uint32_t cs_pin);
    int (*set_lane)(lisa_device_t *dev, lisa_qspilcd_lane_num_t lane);
    int (*set_data_bits)(lisa_device_t *dev, uint8_t data_bits);
    int (*control)(lisa_device_t *dev, uint32_t control, uint32_t arg);
    void (*cs_control)(lisa_device_t *dev, bool level);
} lisa_qspilcd_api_t;

/* ========================================================================
 * QSPI LCD 对外辅助函数
 * ======================================================================== */

#define LISA_QSPILCD0_NAME "qspilcd0"

/**
 * @brief 执行 QSPI LCD 数据传输
 * @param dev 设备实例
 * @param xfer 传输描述结构体指针，包含缓冲区、大小、数据线模式等参数
 * @return 0 = 成功，负数 = 错误码
 *
 * @note PIO 模式下函数同步返回，DMA 模式下需调用 lisa_qspilcd_wait_done() 等待完成
 *
 * @code
 * lisa_qspilcd_xfer_t xfer = {
 *     .buf = data,
 *     .size_bytes = sizeof(data),
 *     .lane = LISA_QSPILCD_LANE_QUAD,
 *     .data_bits = 8,
 *     .use_dma = true,
 * };
 * lisa_qspilcd_transfer(qspi, &xfer);
 * @endcode
 */
static inline int lisa_qspilcd_transfer(lisa_device_t *dev, const lisa_qspilcd_xfer_t *xfer)
{
    if (!dev || !dev->api || !xfer || !xfer->buf || xfer->size_bytes == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_qspilcd_api_t *api = (lisa_qspilcd_api_t *)dev->api;
    return api->transfer ? api->transfer(dev, xfer) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 等待 QSPI LCD 传输完成
 * @param dev 设备实例
 * @param timeout_ms 超时时间（毫秒），0 表示不等待立即返回
 * @return 0 = 成功，LISA_DEVICE_ERR_TIMEOUT = 超时，其他负数 = 错误码
 *
 * @note 仅在 DMA 模式下需要调用此函数，PIO 模式下 transfer 返回即表示完成
 */
static inline int lisa_qspilcd_wait_done(lisa_device_t *dev, uint32_t timeout_ms)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_qspilcd_api_t *api = (lisa_qspilcd_api_t *)dev->api;
    return api->wait_done ? api->wait_done(dev, timeout_ms) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 设置 QSPI LCD 数据线模式
 * @param dev 设备实例
 * @param lane 数据线模式：
 *             - LISA_QSPILCD_LANE_SINGLE: 单线模式（IO0）
 *             - LISA_QSPILCD_LANE_DUAL: 双线模式（IO0, IO1）
 *             - LISA_QSPILCD_LANE_QUAD: 四线模式（IO0-IO3）
 * @return 0 = 成功，负数 = 错误码
 *
 * @note 切换数据线模式应在总线空闲时进行
 */
static inline int lisa_qspilcd_set_lane(lisa_device_t *dev, lisa_qspilcd_lane_num_t lane)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_qspilcd_api_t *api = (lisa_qspilcd_api_t *)dev->api;
    return api->set_lane ? api->set_lane(dev, lane) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 设置 QSPI LCD 数据位宽
 * @param dev 设备实例
 * @param data_bits 数据位宽（1~32），常用值为 8、16、32
 * @return 0 = 成功，负数 = 错误码
 *
 * @note 切换数据位宽应在总线空闲时进行
 */
static inline int lisa_qspilcd_set_data_bits(lisa_device_t *dev, uint8_t data_bits)
{
    if (!dev || !dev->api || data_bits == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_qspilcd_api_t *api = (lisa_qspilcd_api_t *)dev->api;
    return api->set_data_bits ? api->set_data_bits(dev, data_bits) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 执行 QSPI LCD 设备控制操作
 * @param dev 设备实例
 * @param control 控制码，使用 lisa_qspilcd_control_t 枚举值
 * @param arg 控制参数，具体含义取决于控制码
 * @return 0 = 成功，负数 = 错误码
 *
 * @note 常用控制码示例：
 * - LISA_QSPILCD_CONTROL_SET_BUS_SPEED: 设置总线速度，arg = 频率(Hz)
 * - LISA_QSPILCD_CONTROL_DMA_ENABLE: 启用 DMA 传输
 * - LISA_QSPILCD_CONTROL_SET_CLOCK_MODE: 设置时钟模式，arg = 0/1/2/3
 * - LISA_QSPILCD_CONTROL_SET_BIT_ORDER: 设置位序，arg = 0(MSB->LSB) 或 1(LSB->MSB)
 */
static inline int lisa_qspilcd_control(lisa_device_t *dev, lisa_qspilcd_control_t control, uint32_t arg)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_qspilcd_api_t *api = (lisa_qspilcd_api_t *)dev->api;
    return api->control ? api->control(dev, (uint32_t)control, arg) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 控制 QSPI LCD 片选（CS）引脚电平
 * @param dev 设备实例
 * @param level 电平状态：
 *              - true: 拉高 CS（结束传输）
 *              - false: 拉低 CS（开始传输）
 *
 * @note 使用前需先调用 lisa_qspilcd_cs_configure() 配置 CS 引脚
 *
 * @code
 * // 手动 CS 控制示例
 * lisa_qspilcd_cs_control(qspi, false);  // 拉低 CS，开始传输
 * lisa_qspilcd_transfer(qspi, &xfer);
 * lisa_qspilcd_wait_done(qspi, 1000);
 * lisa_qspilcd_cs_control(qspi, true);   // 拉高 CS，结束传输
 * @endcode
 */
static inline void lisa_qspilcd_cs_control(lisa_device_t *dev, bool level)
{
    if (!dev || !dev->api) {
        return;
    }
    lisa_qspilcd_api_t *api = (lisa_qspilcd_api_t *)dev->api;
    if (api->cs_control) {
        api->cs_control(dev, level);
    }
}

/**
 * @brief 配置 QSPI LCD 片选（CS）引脚
 * @param qspilcd_dev QSPI LCD 设备实例
 * @param gpio_dev GPIO 设备实例，用于控制 CS 引脚
 * @param cs_pin CS 引脚编号
 * @return 0 = 成功，负数 = 错误码
 *
 * @note 配置后可使用 lisa_qspilcd_cs_control() 手动控制 CS 电平，
 *       适用于需要在多次传输之间保持 CS 低电平的场景
 *
 * @code
 * lisa_device_t *gpio = lisa_device_get("gpiob");
 * lisa_qspilcd_cs_configure(qspi, gpio, 19);  // 配置 PB19 为 CS 引脚
 * @endcode
 */
static inline int lisa_qspilcd_cs_configure(lisa_device_t *qspilcd_dev, lisa_device_t *gpio_dev, uint32_t cs_pin)
{
    if (!qspilcd_dev || !qspilcd_dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_qspilcd_api_t *api = (lisa_qspilcd_api_t *)qspilcd_dev->api;
    return api->cs_configure ? api->cs_configure(qspilcd_dev, gpio_dev, cs_pin) : LISA_DEVICE_ERR_NOT_SUPPORT;
}


#ifdef __cplusplus
}
#endif

/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lisa_spi_arcs.c
 * @brief LISA SPI ARCS 平台适配层
 *
 * 此文件实现 ARCS 芯片平台的 SPI 硬件适配
 */

#include "lisa_spi.h"
#include "Driver_SPI.h"
#include "ClockManager.h"
#include "dma.h"
#include <lisa_mutex.h>
#include <string.h>
#include "board.h"

#define LOG_TAG "lisa_spi_arcs"
#include <lisa_log.h>

/* 配置锁宏 - 用于保护配置和设备级控制操作 */
#define CONFIG_LOCK(priv)                                                                                              \
    do {                                                                                                               \
        if (priv->config_mutex) {                                                                                      \
            lisa_mutex_lock(priv->config_mutex, LISA_OS_WAIT_FOREVER);                                                 \
        }                                                                                                              \
    } while (0)

#define CONFIG_UNLOCK(priv)                                                                                            \
    do {                                                                                                               \
        if (priv->config_mutex) {                                                                                      \
            lisa_mutex_unlock(priv->config_mutex);                                                                     \
        }                                                                                                              \
    } while (0)

/* 传输锁宏 - 用于保护传输操作 */
#define TRANSFER_LOCK(priv)                                                                                            \
    do {                                                                                                               \
        if (priv->transfer_mutex) {                                                                                    \
            lisa_mutex_lock(priv->transfer_mutex, LISA_OS_WAIT_FOREVER);                                               \
        }                                                                                                              \
    } while (0)

#define TRANSFER_UNLOCK(priv)                                                                                          \
    do {                                                                                                               \
        if (priv->transfer_mutex) {                                                                                    \
            lisa_mutex_unlock(priv->transfer_mutex);                                                                   \
        }                                                                                                              \
    } while (0)

typedef struct {
    void *hal_handler;                      /* HAL SPI 句柄 (SPI0/SPI1)*/
    lisa_spi_config_t current_config;       /* 当前配置 */
    lisa_mutex_t *config_mutex;             /* 配置互斥锁 */
    lisa_mutex_t *transfer_mutex;           /* 传输互斥锁 */
    uint32_t rx_buf_addr;                   /* 用于DMA接收的缓存地址 */
    uint32_t rx_size;                       /* 用于DMA接收的缓存大小 */
    lisa_spi_transfer_callback_t callback;  /* 传输完成回调函数 */
    void *user_data;                        /* 用户自定义数据指针 */
} lisa_spi_priv_t;

/* ===== SPI 设备静态实例 ===== */
#ifdef CONFIG_LISA_SPI0
static lisa_spi_priv_t spi0_priv;
#endif
#ifdef CONFIG_LISA_SPI1
static lisa_spi_priv_t spi1_priv;
#endif
#ifdef CONFIG_LISA_SPI2
static lisa_spi_priv_t spi2_priv;
#endif

/**
 * @brief HAL SPI 事件回调函数
 */
static void spi_hal_event_callback(uint32_t event, uint32_t usr_param)
{
    lisa_spi_priv_t *priv = (lisa_spi_priv_t *)usr_param;

    if (event & CSK_SPI_EVENT_TRANSFER_COMPLETE) {
#if CONFIG_DCACHE_ENABLE
    if (priv->current_config.rx_transfer_mode == LISA_SPI_DMA_TRANSFER) {
        // DMA模式需要刷cache
        if (priv->rx_buf_addr && priv->rx_size) {
            HAL_InvalidateDCache_by_Addr((unsigned long)priv->rx_buf_addr, priv->rx_size);
            priv->rx_buf_addr = 0;
            priv->rx_size = 0;
        }
    }
#endif

        if (priv->callback) {
            priv->callback(priv->user_data);
        }
    }
}

/* ===== ARCS平台SPI实现函数 ===== */
static int arcs_spi_configure(lisa_device_t *dev, const lisa_spi_config_t *config)
{
    if (!lisa_device_is_initialized(dev) || !config) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_spi_priv_t *priv = (lisa_spi_priv_t *)dev->priv_data;
    uint32_t control = 0;
    int32_t ret;

    /* 验证 DMA 通道 (仅当使用 DMA 传输模式时) */
    if (config->tx_transfer_mode == LISA_SPI_DMA_TRANSFER) {
        if (config->tx_dma_channel > 3) {
            LISA_LOGE(LOG_TAG, "Invalid DMA TX channel: %d (valid range: 0-3)", config->tx_dma_channel);
            return LISA_DEVICE_ERR_INVALID;
        }
    }

    if (config->rx_transfer_mode == LISA_SPI_DMA_TRANSFER) {
        if (config->rx_dma_channel > 3) {
            LISA_LOGE(LOG_TAG, "Invalid DMA RX channel: %d (valid range: 0-3)", config->rx_dma_channel);
            return LISA_DEVICE_ERR_INVALID;
        }
    }

    CONFIG_LOCK(priv);

    /* 检查是否需要释放旧的DMA通道 */
    bool old_tx_is_dma = (priv->current_config.tx_transfer_mode == LISA_SPI_DMA_TRANSFER);
    bool old_rx_is_dma = (priv->current_config.rx_transfer_mode == LISA_SPI_DMA_TRANSFER);
    bool new_tx_is_dma = (config->tx_transfer_mode == LISA_SPI_DMA_TRANSFER);
    bool new_rx_is_dma = (config->rx_transfer_mode == LISA_SPI_DMA_TRANSFER);

    /* 如果从DMA模式切换到中断模式，释放DMA通道 */
    if (old_tx_is_dma && !new_tx_is_dma) {
        dma_channel_unreserve(priv->current_config.tx_dma_channel);
    }
    
    if (old_rx_is_dma && !new_rx_is_dma) {
        dma_channel_unreserve(priv->current_config.rx_dma_channel);
    }

    /* 配置SPI基本参数 */
    if (config->master_mode) {
        control |= CSK_SPI_MODE_MASTER;
    } else {
        control |= CSK_SPI_MODE_SLAVE;
    }

    if (config->mode == LISA_SPI_MODE_0) {
        control |= CSK_SPI_CPOL0_CPHA0;
    } else if (config->mode == LISA_SPI_MODE_1) {
        control |= CSK_SPI_CPOL0_CPHA1;
    } else if (config->mode == LISA_SPI_MODE_2) {
        control |= CSK_SPI_CPOL1_CPHA0;
    } else if (config->mode == LISA_SPI_MODE_3) {
        control |= CSK_SPI_CPOL1_CPHA1;
    }

    if (config->bit_order == LISA_SPI_BIT_ORDER_MSB_FIRST) {
        control |= CSK_SPI_MSB_LSB;
    } else {
        control |= CSK_SPI_LSB_MSB;
    }

    control |= CSK_SPI_DATA_BITS(config->data_bits);

    /* 配置传输模式 */
    if (config->tx_transfer_mode == LISA_SPI_DMA_TRANSFER) {
        control |= CSK_SPI_TXIO_DMA;
    } else {
        control |= CSK_SPI_TXIO_PIO;
    }

    if (config->rx_transfer_mode == LISA_SPI_DMA_TRANSFER) {
        control |= CSK_SPI_RXIO_DMA;
    } else {
        control |= CSK_SPI_RXIO_PIO;
    }

    /* 执行SPI控制配置 */
    if (priv->current_config.frequency == config->frequency) {
        ret = SPI_Control(priv->hal_handler, control, 0);
    }
    else {
        ret = SPI_Control(priv->hal_handler, control, config->frequency);
    }
    if (ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "SPI_Control failed: %d", ret);
        ret = LISA_DEVICE_ERR_NOT_SUPPORT;
        goto exit;
    }

    /* 如果新配置使用DMA模式，配置DMA通道 */
    if (new_tx_is_dma || new_rx_is_dma) {
        SPI_ADV_ATTR spi_attr = {0};

        if (new_tx_is_dma) {
            spi_attr.flags |= SPI_ATTR_TX_DMACH_RSVD | SPI_ATTR_TX_NSYNCA | SPI_ATTR_TX_DMA_BSIZE;
            spi_attr.tx_dmach_rsvd = config->tx_dma_channel;
            spi_attr.tx_nsynca = 1;
            spi_attr.tx_dma_bsize = DMA_WIDTH_HALFWORD;
        }

        if (new_rx_is_dma) {
            spi_attr.flags |= SPI_ATTR_RX_NSYNCA | SPI_ATTR_RX_DMACH_RSVD;
            spi_attr.rx_dmach_rsvd = config->rx_dma_channel;
            spi_attr.rx_nsynca = 1;
        }

        ret = SPI_Control(priv->hal_handler, CSK_SPI_SET_ADV_ATTR, (uint32_t)&spi_attr);
        if (ret != CSK_DRIVER_OK) {
            LISA_LOGE(LOG_TAG, "SPI_Control set ADV_ATTR failed: %d", ret);
            ret = LISA_DEVICE_ERR_NOT_SUPPORT;
            goto exit;
        }
    }

    /* 保存新配置 */
    memcpy(&priv->current_config, config, sizeof(lisa_spi_config_t));

    ret = LISA_DEVICE_OK;

exit:
    CONFIG_UNLOCK(priv);
    return ret;
}

static int arcs_spi_get_config(lisa_device_t *dev, lisa_spi_config_t *config)
{
    if (!lisa_device_is_initialized(dev) || !config) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_spi_priv_t *priv = (lisa_spi_priv_t *)dev->priv_data;

    CONFIG_LOCK(priv);
    memcpy(config, &priv->current_config, sizeof(lisa_spi_config_t));
    CONFIG_UNLOCK(priv);

    return LISA_DEVICE_OK;
}

static int arcs_spi_register_callback(lisa_device_t *dev, lisa_spi_transfer_callback_t callback, void *user_data)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_spi_priv_t *priv = (lisa_spi_priv_t *)dev->priv_data;

    CONFIG_LOCK(priv);
    priv->callback = callback;
    priv->user_data = user_data;
    CONFIG_UNLOCK(priv);

    return LISA_DEVICE_OK;
}

static int arcs_spi_transfer(lisa_device_t *dev, const lisa_spi_transfer_t *xfer)
{
    if (!lisa_device_is_initialized(dev) || !xfer || (!xfer->tx_buf || !xfer->rx_buf) || xfer->len == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_spi_priv_t *priv = (lisa_spi_priv_t *)dev->priv_data;
    int ret;

    TRANSFER_LOCK(priv);

#if CONFIG_DCACHE_ENABLE
    if (priv->current_config.tx_transfer_mode == LISA_SPI_DMA_TRANSFER) {
        // DMA模式需要刷cache
        HAL_FlushDCache_by_Addr((unsigned long)xfer->tx_buf, xfer->len);
        HAL_FlushDCache_by_Addr((unsigned long)xfer->rx_buf, xfer->len);
    }
#endif

    ret = SPI_Transfer(priv->hal_handler, xfer->tx_buf, xfer->rx_buf, xfer->len);
    if (ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "SPI_Transfer failed: %d", ret);
        ret = LISA_DEVICE_ERR_NOT_SUPPORT;
        goto exit;
    }

    priv->rx_buf_addr = (uint32_t)xfer->rx_buf;
    priv->rx_size = xfer->len;

    ret = LISA_DEVICE_OK;
    TRANSFER_UNLOCK(priv);
exit:
    return ret;
}

static int arcs_spi_write(lisa_device_t *dev, const uint8_t *buf, uint32_t len)
{
    if (!lisa_device_is_initialized(dev) || !buf || len == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_spi_priv_t *priv = (lisa_spi_priv_t *)dev->priv_data;
    int ret;

    TRANSFER_LOCK(priv);
#if CONFIG_DCACHE_ENABLE
    if (priv->current_config.tx_transfer_mode == LISA_SPI_DMA_TRANSFER) {
        // DMA模式需要刷cache
        HAL_FlushDCache_by_Addr((unsigned long)buf, len);
    }
#endif

    ret = SPI_Send(priv->hal_handler, buf, len);
    if (ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "SPI_Send failed: %d", ret);
        ret = LISA_DEVICE_ERR_NOT_SUPPORT;
        goto exit;
    }

    ret = LISA_DEVICE_OK;

exit:
    TRANSFER_UNLOCK(priv);
    return ret;
}

static int arcs_spi_read(lisa_device_t *dev, uint8_t *buf, uint32_t len)
{
    if (!lisa_device_is_initialized(dev) || !buf || len == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_spi_priv_t *priv = (lisa_spi_priv_t *)dev->priv_data;
    int ret;

    TRANSFER_LOCK(priv);
#if CONFIG_DCACHE_ENABLE
    if (priv->current_config.rx_transfer_mode == LISA_SPI_DMA_TRANSFER) {
        // DMA模式需要刷cache
        HAL_FlushDCache_by_Addr((unsigned long)buf, len);
    }
#endif

    if (priv->current_config.flags == LISA_SPI_FLAG_SOFTWARE_CS && (!priv->current_config.master_mode)) {
        ret = SPI_Receive_NEnd(priv->hal_handler, buf, len);
    } else {
        ret = SPI_Receive(priv->hal_handler, buf, len);
    }
    if (ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "SPI_Receive failed: %d", ret);
        ret = LISA_DEVICE_ERR_NOT_SUPPORT;
        goto exit;
    }

    priv->rx_buf_addr = (uint32_t)buf;
    priv->rx_size = len;

    ret = LISA_DEVICE_OK;

exit:
    TRANSFER_UNLOCK(priv);
    return ret;
}

/* ===== 设备初始化函数 ===== */
#ifdef CONFIG_LISA_SPI0
static int arcs_spi0_init(void)
{
    int ret;
    
    memset(&spi0_priv, 0, sizeof(spi0_priv));

    spi0_priv.hal_handler = SPI0();
    if (!spi0_priv.hal_handler) {
        LISA_LOGE(LOG_TAG, "Failed to get SPI0 handler");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    ret = SPI_Initialize(spi0_priv.hal_handler, spi_hal_event_callback, (uint32_t)&spi0_priv);
    if (ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "SPI_Initialize failed: %d", ret);
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }

    ret = SPI_PowerControl(spi0_priv.hal_handler, CSK_POWER_FULL);
    if (ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "SPI_PowerControl failed: %d", ret);
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }

    HAL_CRM_SetSpi0ClkSrc(CRM_IpSrcPeriClk);

    spi0_priv.config_mutex = lisa_mutex_create();
    if (!spi0_priv.config_mutex) {
        LISA_LOGE(LOG_TAG, "Failed to create config_mutex for SPI0");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    spi0_priv.transfer_mutex = lisa_mutex_create();
    if (!spi0_priv.transfer_mutex) {
        LISA_LOGE(LOG_TAG, "Failed to create transfer_mutex for SPI0");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    lisa_spi0_pinmux();

    return LISA_DEVICE_OK;
}
#endif

#ifdef CONFIG_LISA_SPI1
static int arcs_spi1_init(void)
{
    
    int ret;
    memset(&spi1_priv, 0, sizeof(spi1_priv));

    spi1_priv.hal_handler = SPI1();
    if (!spi1_priv.hal_handler) {
        LISA_LOGE(LOG_TAG, "Failed to get SPI1 handler");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    ret = SPI_Initialize(spi1_priv.hal_handler, spi_hal_event_callback, (uint32_t)&spi1_priv);
    if (ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "SPI_Initialize failed: %d", ret);
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }

    ret = SPI_PowerControl(spi1_priv.hal_handler, CSK_POWER_FULL);
    if (ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "SPI_PowerControl failed: %d", ret);
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }

    HAL_CRM_SetSpi1ClkSrc(CRM_IpSrcPeriClk);
    spi1_priv.config_mutex = lisa_mutex_create();
    if (!spi1_priv.config_mutex) {
        LISA_LOGE(LOG_TAG, "Failed to create config_mutex for SPI1");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    spi1_priv.transfer_mutex = lisa_mutex_create();
    if (!spi1_priv.transfer_mutex) {
        LISA_LOGE(LOG_TAG, "Failed to create transfer_mutex for SPI1");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    lisa_spi1_pinmux();

    return LISA_DEVICE_OK;
}
#endif

#ifdef CONFIG_LISA_SPI2
static int arcs_spi2_init(void)
{
    
    int ret;
    memset(&spi2_priv, 0, sizeof(spi2_priv));

    spi2_priv.hal_handler = SPI2();
    if (!spi2_priv.hal_handler) {
        LISA_LOGE(LOG_TAG, "Failed to get SPI2 handler");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    ret = SPI_Initialize(spi2_priv.hal_handler, spi_hal_event_callback, (uint32_t)&spi2_priv);
    if (ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "SPI_Initialize failed: %d", ret);
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }

    ret = SPI_PowerControl(spi2_priv.hal_handler, CSK_POWER_FULL);
    if (ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "SPI_PowerControl failed: %d", ret);
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }

    HAL_CRM_SetSpi2ClkSrc(CRM_IpSrcPeriClk);
    spi2_priv.config_mutex = lisa_mutex_create();
    if (!spi2_priv.config_mutex) {
        LISA_LOGE(LOG_TAG, "Failed to create config_mutex for SPI2");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    spi2_priv.transfer_mutex = lisa_mutex_create();
    if (!spi2_priv.transfer_mutex) {
        LISA_LOGE(LOG_TAG, "Failed to create transfer_mutex for SPI2");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    lisa_spi2_pinmux();

    return LISA_DEVICE_OK;
}
#endif

 /* ===== ARCS SPI API 实例 ===== */
 static const lisa_spi_api_t arcs_spi_api = {
    .configure = arcs_spi_configure,
    .get_config = arcs_spi_get_config,
    .transfer = arcs_spi_transfer,
    .write = arcs_spi_write,
    .read = arcs_spi_read,
    .register_callback = arcs_spi_register_callback,
 };

 /* ===== 设备注册 ===== */
#ifdef CONFIG_LISA_SPI0
LISA_DEVICE_REGISTER(spi0, &arcs_spi_api, &spi0_priv, NULL, arcs_spi0_init, LISA_DEVICE_PRIORITY_NORMAL);
#endif
#ifdef CONFIG_LISA_SPI1
LISA_DEVICE_REGISTER(spi1, &arcs_spi_api, &spi1_priv, NULL, arcs_spi1_init, LISA_DEVICE_PRIORITY_NORMAL);
#endif
#ifdef CONFIG_LISA_SPI2
LISA_DEVICE_REGISTER(spi2, &arcs_spi_api, &spi2_priv, NULL, arcs_spi2_init, LISA_DEVICE_PRIORITY_NORMAL);
#endif

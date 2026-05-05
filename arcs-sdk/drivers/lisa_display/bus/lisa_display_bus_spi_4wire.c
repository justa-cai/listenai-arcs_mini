/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdbool.h>
#include "lisa_device.h"
#include "lisa_display_bus.h"
#include "lisa_spi.h"
#include "lisa_gpio.h"
#include "lisa_semaphore.h"

#define LOG_TAG "bus.spi4"
#include <lisa_log.h>

typedef struct {
    lisa_device_t *spi_dev;
    lisa_semaphore_t *tx_complete_sem;
    bool callback_registered;
} bus_spi4_shared_t;

typedef struct {
    bus_spi4_shared_t *shared;
    lisa_device_t *dc_gpio;
    uint32_t dc_pin;
    lisa_device_t *cs_gpio;
    uint32_t cs_pin;
} bus_spi4_priv_t;

/* ========================================================================
 * 静态多实例数组
 * ======================================================================== */
#ifdef CONFIG_LISA_DUAL_DISPLAY
#define LISA_DISPLAY_BUS_SPI_4WIRE_MAX_INSTANCES 2
#else
#define LISA_DISPLAY_BUS_SPI_4WIRE_MAX_INSTANCES 1
#endif
static bus_spi4_priv_t display_bus_spi4_priv[LISA_DISPLAY_BUS_SPI_4WIRE_MAX_INSTANCES];
static bus_spi4_shared_t display_bus_spi4_shared[LISA_DISPLAY_BUS_SPI_4WIRE_MAX_INSTANCES];

/* ========================================================================
 * 内部辅助函数
 * ======================================================================== */

static inline void set_dc_pin(bus_spi4_priv_t *priv, int level)
{
    lisa_gpio_write_pin(priv->dc_gpio, priv->dc_pin, level);
}

static bus_spi4_shared_t *find_shared_spi_ctx(lisa_device_t *spi_dev)
{
    int free_idx = -1;

    for (int i = 0; i < LISA_DISPLAY_BUS_SPI_4WIRE_MAX_INSTANCES; i++) {
        if (display_bus_spi4_shared[i].spi_dev == spi_dev) {
            return &display_bus_spi4_shared[i];
        }
        if (!display_bus_spi4_shared[i].spi_dev && free_idx < 0) {
            free_idx = i;
        }
    }

    if (free_idx < 0) {
        return NULL;
    }

    display_bus_spi4_shared[free_idx].spi_dev = spi_dev;
    display_bus_spi4_shared[free_idx].tx_complete_sem = lisa_semaphore_create(1);
    if (!display_bus_spi4_shared[free_idx].tx_complete_sem) {
        display_bus_spi4_shared[free_idx].spi_dev = NULL;
        return NULL;
    }

    return &display_bus_spi4_shared[free_idx];
}

static void spi_transfer_callback(void *user_data)
{
    bus_spi4_shared_t *shared = (bus_spi4_shared_t *)user_data;
    if (shared && shared->tx_complete_sem) {
        lisa_semaphore_give(shared->tx_complete_sem);
    }
}

/* ========================================================================
 * Bus API 实现
 * ======================================================================== */
static int bus_spi4_attach(lisa_device_t *bus_dev, lisa_display_bus_type_t bus_type, const lisa_display_bus_config_u *bus_config)
{
    if (!bus_dev->priv_data) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    if (bus_type != LISA_DISPLAY_BUS_SPI_4WIRE) {
        LISA_LOGE(LOG_TAG, "Bus type mismatch: %d", bus_type);
        return LISA_DEVICE_ERR_INVALID;
    }

    bus_spi4_priv_t *priv = (bus_spi4_priv_t *)bus_dev->priv_data;
    const lisa_display_bus_spi_4wire_config_t *spi_config = &bus_config->spi_4wire;

    priv->dc_gpio = spi_config->dc_gpio;
    priv->dc_pin  = spi_config->dc_pin;
    priv->cs_gpio = spi_config->cs_gpio;
    priv->cs_pin  = spi_config->cs_pin;
    priv->shared = find_shared_spi_ctx(spi_config->spi_dev);
    if (!priv->shared) {
        LISA_LOGE(LOG_TAG, "Failed to allocate shared SPI context");
        return LISA_DEVICE_ERR_NO_MEM;
    }

    lisa_gpio_configure(priv->cs_gpio, priv->cs_pin, LISA_GPIO_CONFIG_OUTPUT_HIGH);
    lisa_gpio_configure(priv->dc_gpio, priv->dc_pin, LISA_GPIO_CONFIG_OUTPUT_HIGH);

    /* 初始配置为中断模式 */
    lisa_spi_config_t spi_cfg = {
        .frequency        = spi_config->spi_freq ? spi_config->spi_freq : 50*1000*1000,
        .bit_order        = LISA_SPI_BIT_ORDER_MSB_FIRST,
        .flags            = LISA_SPI_FLAG_SOFTWARE_CS,
        .tx_transfer_mode = LISA_SPI_INTERRUPT_TRANSFER,
        .rx_transfer_mode = LISA_SPI_INTERRUPT_TRANSFER,
        .tx_dma_channel   = CONFIG_LISA_DISPLAY_SPI_4WIRE_DMA_CH,
        .mode             = LISA_SPI_MODE_3,
        .master_mode      = true,
        .data_bits        = 8,
    };
    int ret = lisa_spi_configure(priv->shared->spi_dev, &spi_cfg);
    if (ret) {
        LISA_LOGE(LOG_TAG, "Failed to configure SPI: %d", ret);
        return ret;
    }
    if (!priv->shared->callback_registered) {
        lisa_spi_register_callback(priv->shared->spi_dev, spi_transfer_callback, priv->shared);
        priv->shared->callback_registered = true;
    }


    return LISA_DEVICE_OK;
}

static int bus_spi4_trans_cmd_data(lisa_device_t *bus_dev, uint32_t cmd, uint8_t cmd_bits, const void *data, size_t len)
{
    bus_spi4_priv_t *priv = (bus_spi4_priv_t *)bus_dev->priv_data;

    if (cmd_bits > 0) {
        set_dc_pin(priv, 0); // Command mode
        lisa_spi_write(priv->shared->spi_dev, (uint8_t *)&cmd, cmd_bits / 8);
        
        if (lisa_semaphore_take(priv->shared->tx_complete_sem, 100) != LISA_OK) {
            LISA_LOGE(LOG_TAG, "SPI cmd transfer timeout");
            return LISA_DEVICE_ERR_TIMEOUT;
        }
    }
    if (data && len > 0) {
        set_dc_pin(priv, 1); // Data mode
        lisa_spi_write(priv->shared->spi_dev, data, len);
        
        if (lisa_semaphore_take(priv->shared->tx_complete_sem, 100) != LISA_OK) {
            LISA_LOGE(LOG_TAG, "SPI data transfer timeout");
            return LISA_DEVICE_ERR_TIMEOUT;
        }
    }

    return LISA_DEVICE_OK;
}

static void bus_spi4_transfer_control(lisa_device_t *bus_dev, bool enable)
{
    if (!bus_dev || !bus_dev->priv_data) {
        return;
    }
    bus_spi4_priv_t *priv = (bus_spi4_priv_t *)bus_dev->priv_data;

    if (enable) {
        lisa_gpio_write_pin(priv->cs_gpio, priv->cs_pin, 0);
    }
    else {
        lisa_gpio_write_pin(priv->cs_gpio, priv->cs_pin, 1);
    }
}

static int bus_spi4_write_pixels(lisa_device_t *bus_dev, const void *pixels, size_t len)
{
    bus_spi4_priv_t *priv = (bus_spi4_priv_t *)bus_dev->priv_data;

    /* 切换到DMA模式用于像素传输 */
    lisa_spi_config_t spi_config;
    lisa_spi_get_config(priv->shared->spi_dev, &spi_config);

    spi_config.tx_transfer_mode = LISA_SPI_DMA_TRANSFER;
    spi_config.data_bits = 16;
    int ret = lisa_spi_configure(priv->shared->spi_dev, &spi_config);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "%s: spi configure failed.", __func__);
        return ret;
    }

    set_dc_pin(priv, 1); // Data mode
    ret = lisa_spi_write(priv->shared->spi_dev, pixels, len/2);
    return ret;
}

static int bus_spi4_wait_for_completion(lisa_device_t *bus_dev, int32_t timeout_ms)
{
    bus_spi4_priv_t *priv = (bus_spi4_priv_t *)bus_dev->priv_data;
    if (lisa_semaphore_take(priv->shared->tx_complete_sem, timeout_ms) != LISA_OK) {
        LISA_LOGE(LOG_TAG, "%s: semaphore take failed.", __func__);
        return LISA_DEVICE_ERR_TIMEOUT;
    }

    /* 切换回中断模式用于命令传输 */
    lisa_spi_config_t spi_config;
    lisa_spi_get_config(priv->shared->spi_dev, &spi_config);

    spi_config.tx_transfer_mode = LISA_SPI_INTERRUPT_TRANSFER;
    spi_config.data_bits = 8;
    int ret = lisa_spi_configure(priv->shared->spi_dev, &spi_config);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "%s: spi configure failed.", __func__);
        return ret;
    }

    return LISA_DEVICE_OK;
}

/* ========================================================================
 * Bus API 结构体（所有实例共享）
 * ======================================================================== */

static const lisa_display_bus_api_t display_bus_spi4_api = {
    .attach              = bus_spi4_attach,
    .write_pixels        = bus_spi4_write_pixels,
    .trans_cmd_data      = bus_spi4_trans_cmd_data,
    .transfer_control    = bus_spi4_transfer_control,
    .wait_for_completion = bus_spi4_wait_for_completion,
};

/* ========================================================================
 * 实例初始化函数
 * ======================================================================== */

/**
 * @brief 通用实例初始化函数
 * @param idx 实例索引 (0-based)
 * @return LISA_DEVICE_OK 成功, LISA_DEVICE_ERR_INVALID 索引无效
 */
static int bus_spi4_init_instance(int idx)
{
    if (idx < 0 || idx >= LISA_DISPLAY_BUS_SPI_4WIRE_MAX_INSTANCES) {
        LISA_LOGE(LOG_TAG, "Invalid instance index: %d (max: %d)", idx, LISA_DISPLAY_BUS_SPI_4WIRE_MAX_INSTANCES);
        return LISA_DEVICE_ERR_INVALID;
    }

    /* 清零私有数据 */
    memset(&display_bus_spi4_priv[idx], 0, sizeof(bus_spi4_priv_t));
    
    LISA_LOGI(LOG_TAG, "SPI 4-wire bus instance %d initialized", idx);
    return LISA_DEVICE_OK;
}

/**
 * @brief 宏：生成实例初始化函数
 * @param n 实例编号
 *
 * 使用宏可以避免为每个实例手写重复的初始化函数，
 * 提高代码可维护性和扩展性。
 */
#define DEFINE_BUS_SPI4_INIT_FUNC(n) \
    static int bus_spi4_init_##n(void) \
    { \
        return bus_spi4_init_instance(n); \
    }

/* 为实例 0 生成初始化函数 */
DEFINE_BUS_SPI4_INIT_FUNC(0)

/* 如果启用双显示，为实例 1 生成初始化函数 */
#if LISA_DISPLAY_BUS_SPI_4WIRE_MAX_INSTANCES > 1
DEFINE_BUS_SPI4_INIT_FUNC(1)
#endif

/* ========================================================================
 * 静态设备注册（每个实例一个设备）
 * ======================================================================== */

LISA_DEVICE_REGISTER(panel_bus_spi_4wire_0, &display_bus_spi4_api, &display_bus_spi4_priv[0], NULL, bus_spi4_init_0, LISA_DEVICE_LEVEL_NORMAL, LISA_DEVICE_PRIORITY_HIGH);

#if LISA_DISPLAY_BUS_SPI_4WIRE_MAX_INSTANCES > 1
LISA_DEVICE_REGISTER(panel_bus_spi_4wire_1, &display_bus_spi4_api, &display_bus_spi4_priv[1], NULL, bus_spi4_init_1, LISA_DEVICE_LEVEL_NORMAL, LISA_DEVICE_PRIORITY_HIGH);
#endif

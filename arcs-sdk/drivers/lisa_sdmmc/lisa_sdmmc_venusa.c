/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lisa_sdmmc_venusa.c
 * @brief LISA SDMMC Venusa device adapter
 */

#define LOG_TAG "lisa_sdmmc_venusa"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "lisa_mutex.h"
#include "lisa_sdmmc.h"
#include "port/venusa/sdmmc_init.h"

#include <lisa_log.h>

#if CONFIG_LISA_PM
#include "lisa_pm.h"
#endif

#ifndef CONFIG_LISA_SDMMC_ACCESS_BUFFER_ALIGN_SIZE
#define CONFIG_LISA_SDMMC_ACCESS_BUFFER_ALIGN_SIZE 64
#endif

#ifndef CONFIG_LISA_SDMMC_ACCESS_WRAP_BUFFER_SIZE
#define CONFIG_LISA_SDMMC_ACCESS_WRAP_BUFFER_SIZE (4 * 1024)
#endif

typedef struct {
    lisa_mutex_t *mutex;
    bool initialized;
    volatile bool xfer_in_flight;
} venusa_sdmmc_priv_t;

static venusa_sdmmc_priv_t sdmmc0_priv;

__attribute__((section(".psram.data"), aligned(CONFIG_LISA_SDMMC_ACCESS_BUFFER_ALIGN_SIZE)))
static uint8_t s_read_wrap_buffer[CONFIG_LISA_SDMMC_ACCESS_WRAP_BUFFER_SIZE];

__attribute__((section(".psram.data"), aligned(CONFIG_LISA_SDMMC_ACCESS_BUFFER_ALIGN_SIZE)))
static uint8_t s_write_wrap_buffer[CONFIG_LISA_SDMMC_ACCESS_WRAP_BUFFER_SIZE];

#define DEVICE_LOCK(priv)                                           \
    do {                                                            \
        if ((priv)->mutex != NULL) {                                \
            lisa_mutex_lock((priv)->mutex, LISA_OS_WAIT_FOREVER);   \
        }                                                           \
    } while (0)

#define DEVICE_UNLOCK(priv)                    \
    do {                                       \
        if ((priv)->mutex != NULL) {           \
            lisa_mutex_unlock((priv)->mutex);  \
        }                                      \
    } while (0)

static int venusa_sdmmc_check_ready(lisa_device_t *dev)
{
    if ((dev == NULL) || (dev->priv_data == NULL)) {
        return LISA_DEVICE_ERR_INVALID;
    }

    venusa_sdmmc_priv_t *priv = (venusa_sdmmc_priv_t *)dev->priv_data;
    if (!priv->initialized) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    return LISA_DEVICE_OK;
}

static bool venusa_sdmmc_is_aligned(const void *buffer)
{
    return (((uintptr_t)buffer % CONFIG_LISA_SDMMC_ACCESS_BUFFER_ALIGN_SIZE) == 0U);
}

static int venusa_sdmmc_probe(lisa_device_t *dev)
{
    if ((dev == NULL) || (dev->priv_data == NULL)) {
        return LISA_DEVICE_ERR_INVALID;
    }

    venusa_sdmmc_priv_t *priv = (venusa_sdmmc_priv_t *)dev->priv_data;
    if (priv->initialized) {
        return LISA_DEVICE_OK;
    }

    DEVICE_LOCK(priv);

    if (priv->initialized) {
        DEVICE_UNLOCK(priv);
        return LISA_DEVICE_OK;
    }

    int ret = sdmmc_hard_init();
    if (ret != 0) {
        DEVICE_UNLOCK(priv);
        return LISA_DEVICE_ERR_IO;
    }

    priv->initialized = true;
    DEVICE_UNLOCK(priv);

    LISA_LOGI(LOG_TAG, "SD/MMC card probed successfully");
    return LISA_DEVICE_OK;
}

static int venusa_sdmmc_status(lisa_device_t *dev)
{
    if ((dev == NULL) || (dev->priv_data == NULL)) {
        return LISA_DEVICE_ERR_INVALID;
    }

    venusa_sdmmc_priv_t *priv = (venusa_sdmmc_priv_t *)dev->priv_data;
    if (!priv->initialized) {
        return LISA_SDMMC_STATUS_UNINIT;
    }

    if (!sdmmc_card_exists()) {
        priv->initialized = false;
        return LISA_SDMMC_STATUS_NO_MEDIA;
    }

    return LISA_SDMMC_STATUS_OK;
}

static int venusa_sdmmc_read(lisa_device_t *dev, uint8_t *buff, uint32_t sector, uint32_t count)
{
    if ((buff == NULL) || (count == 0U)) {
        return LISA_DEVICE_ERR_INVALID;
    }

    int ret = venusa_sdmmc_check_ready(dev);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    venusa_sdmmc_priv_t *priv = (venusa_sdmmc_priv_t *)dev->priv_data;
    const uint32_t sectors_per_wrap = CONFIG_LISA_SDMMC_ACCESS_WRAP_BUFFER_SIZE / SDMMC_SECTOR_SIZE;

    DEVICE_LOCK(priv);
    priv->xfer_in_flight = true;

    if (!venusa_sdmmc_is_aligned(buff)) {
        uint32_t remaining = count;
        uint32_t current_sector = sector;
        uint8_t *dst = buff;

        ret = 0;
        while (remaining > 0U) {
            uint32_t n = (remaining > sectors_per_wrap) ? sectors_per_wrap : remaining;
            uint32_t bytes = n * SDMMC_SECTOR_SIZE;

            sdmmc_cache_invalidate(s_read_wrap_buffer, bytes);
            ret = sdmmc_read_sectors(current_sector, n, s_read_wrap_buffer);
            if (ret != 0) {
                break;
            }

            sdmmc_cache_invalidate(s_read_wrap_buffer, bytes);
            memcpy(dst, s_read_wrap_buffer, bytes);
            dst += bytes;
            current_sector += n;
            remaining -= n;
        }
    } else {
        uint32_t bytes = count * SDMMC_SECTOR_SIZE;

        sdmmc_cache_invalidate(buff, bytes);
        ret = sdmmc_read_sectors(sector, count, buff);
        if (ret == 0) {
            sdmmc_cache_invalidate(buff, bytes);
        }
    }

    priv->xfer_in_flight = false;
    DEVICE_UNLOCK(priv);
    return (ret == 0) ? LISA_DEVICE_OK : LISA_DEVICE_ERR_IO;
}

static int venusa_sdmmc_write(lisa_device_t *dev, const uint8_t *buff, uint32_t sector, uint32_t count)
{
    if ((buff == NULL) || (count == 0U)) {
        return LISA_DEVICE_ERR_INVALID;
    }

    int ret = venusa_sdmmc_check_ready(dev);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    venusa_sdmmc_priv_t *priv = (venusa_sdmmc_priv_t *)dev->priv_data;
    const uint32_t sectors_per_wrap = CONFIG_LISA_SDMMC_ACCESS_WRAP_BUFFER_SIZE / SDMMC_SECTOR_SIZE;

    DEVICE_LOCK(priv);
    priv->xfer_in_flight = true;

    if (!venusa_sdmmc_is_aligned(buff)) {
        uint32_t remaining = count;
        uint32_t current_sector = sector;
        const uint8_t *src = buff;

        ret = 0;
        while (remaining > 0U) {
            uint32_t n = (remaining > sectors_per_wrap) ? sectors_per_wrap : remaining;
            uint32_t bytes = n * SDMMC_SECTOR_SIZE;

            memcpy(s_write_wrap_buffer, src, bytes);
            sdmmc_cache_flush(s_write_wrap_buffer, bytes);

            ret = sdmmc_write_sectors(current_sector, n, s_write_wrap_buffer);
            if (ret != 0) {
                break;
            }

            src += bytes;
            current_sector += n;
            remaining -= n;
        }
    } else {
        sdmmc_cache_flush(buff, count * SDMMC_SECTOR_SIZE);
        ret = sdmmc_write_sectors(sector, count, buff);
    }

    priv->xfer_in_flight = false;
    DEVICE_UNLOCK(priv);
    return (ret == 0) ? LISA_DEVICE_OK : LISA_DEVICE_ERR_IO;
}

static int venusa_sdmmc_ioctl(lisa_device_t *dev, uint8_t cmd, void *buff)
{
    if (cmd == LISA_SDMMC_IOCTL_CTRL_SYNC) {
        return LISA_DEVICE_OK;
    }

    int ret = venusa_sdmmc_check_ready(dev);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    if (buff == NULL) {
        return LISA_DEVICE_ERR_INVALID;
    }

    venusa_sdmmc_priv_t *priv = (venusa_sdmmc_priv_t *)dev->priv_data;

    DEVICE_LOCK(priv);

    switch (cmd) {
    case LISA_SDMMC_IOCTL_GET_SECTOR_COUNT:
        ret = sdmmc_get_sector_count((uint32_t *)buff);
        break;

    case LISA_SDMMC_IOCTL_GET_SECTOR_SIZE:
        *(uint32_t *)buff = SDMMC_SECTOR_SIZE;
        ret = 0;
        break;

    case LISA_SDMMC_IOCTL_GET_ERASE_BLOCK_SZ:
        ret = sdmmc_get_erase_block_size((uint32_t *)buff);
        break;

    default:
        ret = LISA_DEVICE_ERR_INVALID;
        break;
    }

    DEVICE_UNLOCK(priv);

    if (ret == LISA_DEVICE_ERR_INVALID) {
        return LISA_DEVICE_ERR_INVALID;
    }
    return (ret == 0) ? LISA_DEVICE_OK : LISA_DEVICE_ERR_IO;
}

static const lisa_sdmmc_api_t venusa_sdmmc_api = {
    .probe = venusa_sdmmc_probe,
    .status = venusa_sdmmc_status,
    .read = venusa_sdmmc_read,
    .write = venusa_sdmmc_write,
    .ioctl = venusa_sdmmc_ioctl,
};

static int venusa_sdmmc0_init(void)
{
    memset(&sdmmc0_priv, 0, sizeof(sdmmc0_priv));

    sdmmc0_priv.mutex = lisa_mutex_create();
    if (sdmmc0_priv.mutex == NULL) {
        LISA_LOGE(LOG_TAG, "Create SDMMC mutex failed");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    int ret = sdmmc_platform_init();
    if (ret != 0) {
        LISA_LOGE(LOG_TAG, "SDMMC platform init failed: %d", ret);
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    LISA_LOGD(LOG_TAG, "SDMMC device registered");
    return LISA_DEVICE_OK;
}

/**
 * @brief 停止并释放 SDMMC0 设备的全部软硬件资源，恢复芯片上电初始状态
 *
 * 由 lisa_device_destroy() 调用。释放顺序与 venusa_sdmmc0_init 申请相反：
 *   1) sdmmc_platform_reset() 复位平台电源/时钟（唤醒后 reinit 会重做 platform_init）；
 *   2) 释放 OS 资源 mutex；
 *   3) memset 整个 priv，回到 _init 之前的零初值（initialized / xfer_in_flight
 *      随之清零，强制唤醒后业务侧重新 probe 探卡）。
 *
 * 约定：调用方需保证此时无扇区读写在途、无并发业务在使用本设备。
 */
static int venusa_sdmmc0_deinit(void)
{
    venusa_sdmmc_priv_t *priv = &sdmmc0_priv;

    sdmmc_platform_reset();

    if (priv->mutex) {
        lisa_mutex_delete(priv->mutex);
    }

    memset(&sdmmc0_priv, 0, sizeof(sdmmc0_priv));
    return LISA_DEVICE_OK;
}

#if CONFIG_LISA_PM
/* ===== System PM 回调 =====
 *
 * 与 lisa_audio 一致，本驱动采用“睡前 destroy / 唤醒后 reinit”模型：应用在睡眠前
 * 调 lisa_device_destroy(sdmmc0) 释放全部软硬件资源（platform_reset + mutex），唤醒后
 * 经 lisa_device_reinit(sdmmc0) 重建（重做 platform_init），probe 由业务侧唤醒后按需
 * 重新触发。因此 prepare_suspend / resume_restore 不再需要，仅保留 check_idle：只读
 * priv->xfer_in_flight，不取 mutex、不访问 HAL。
 */
static int32_t venusa_sdmmc_pm_check_idle(void *ctx)
{
    venusa_sdmmc_priv_t *priv = (venusa_sdmmc_priv_t *)ctx;
    if (priv == NULL) {
        return 1;
    }
    return priv->xfer_in_flight ? 0 : 1;
}

static const lisa_pm_system_ops_t venusa_sdmmc_pm_ops = {
    .check_idle      = venusa_sdmmc_pm_check_idle,
    .prepare_suspend = NULL,
    .resume_restore  = NULL,
};
#endif

LISA_DEVICE_REGISTER_DEINIT(sdmmc0, &venusa_sdmmc_api, &sdmmc0_priv, NULL, venusa_sdmmc0_init,
                            venusa_sdmmc0_deinit, LISA_DEVICE_LEVEL_NORMAL, CONFIG_LISA_SDMMC_INIT_PRIORITY);
#if CONFIG_LISA_PM
LISA_DEVICE_PM_ATTACH(sdmmc0, &venusa_sdmmc_pm_ops, NULL, &sdmmc0_priv);
#endif

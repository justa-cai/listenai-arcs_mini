/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lisa_sdmmc_arcs.c
 * @brief LISA SDMMC ARCS 平台适配层
 *
 * 此文件实现 ARCS 芯片平台的 SDMMC 硬件适配
 */

#define LOG_TAG "lisa_sdmmc_arcs"

#include <stddef.h>
#include <string.h>
#include <errno.h>

#include "lisa_mutex.h"
#include "board.h"

#include "lisa_sdmmc.h"
#include "lib_sdc.h"
#include "drv_sdc.h"
#include "arcs_ap.h"
#include "cache.h"
#include "port/arcs/sdmmc_init.h"

#include <lisa_log.h>

/* ========================================================================
 * DISK 常量定义
 * ======================================================================== */

#define DISK_SECTOR_SIZE 512
#define SD_PORT SD_0

#ifndef CONFIG_LISA_SDMMC_ACCESS_BUFFER_ALIGN_SIZE
#define CONFIG_LISA_SDMMC_ACCESS_BUFFER_ALIGN_SIZE 64
#endif

#ifndef CONFIG_LISA_SDMMC_ACCESS_WRAP_BUFFER_SIZE
#define CONFIG_LISA_SDMMC_ACCESS_WRAP_BUFFER_SIZE (4 * 1024)
#endif

/* ========================================================================
 * DISK 私有数据结构定义
 * ======================================================================== */

typedef struct {
    lisa_mutex_t *mutex;    /* 互斥锁 */
    bool initialized;       /* 初始化标志 */
} lisa_sdmmc_priv_t;

/* ===== DISK 设备静态实例 ===== */
static lisa_sdmmc_priv_t sdmmc0_priv;

/* ===== 对齐缓冲区（用于非对齐 DMA 操作） ===== */
__attribute__((section(".psram.data"), aligned(CONFIG_LISA_SDMMC_ACCESS_BUFFER_ALIGN_SIZE)))
static uint8_t read_wrap_buffer[CONFIG_LISA_SDMMC_ACCESS_WRAP_BUFFER_SIZE];

__attribute__((section(".psram.data"), aligned(CONFIG_LISA_SDMMC_ACCESS_BUFFER_ALIGN_SIZE)))
static uint8_t write_wrap_buffer[CONFIG_LISA_SDMMC_ACCESS_WRAP_BUFFER_SIZE];

/* ===== 辅助宏定义 ===== */
#define DEVICE_LOCK(priv)                                                      \
    do {                                                                       \
        if (priv->mutex) {                                                     \
            lisa_mutex_lock(priv->mutex, LISA_OS_WAIT_FOREVER);                \
        }                                                                      \
    } while (0)

#define DEVICE_UNLOCK(priv)                                                    \
    do {                                                                       \
        if (priv->mutex) {                                                     \
            lisa_mutex_unlock(priv->mutex);                                    \
        }                                                                      \
    } while (0)

/* ===== 内部辅助函数 ===== */

static inline int check_device_initialized(lisa_device_t *dev)
{
    if (!dev || !dev->priv_data) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_sdmmc_priv_t *priv = (lisa_sdmmc_priv_t *)dev->priv_data;
    if (!priv->initialized) {
        LISA_LOGE(LOG_TAG, "Disk device not initialized");
        return LISA_DEVICE_ERR_NOT_READY;
    }

    return LISA_DEVICE_OK;
}

/* ===== API 实现函数 ===== */

/**
 * @brief 探测 SD/MMC 卡设备
 *
 * 执行纯探测操作：卡检测、总线配置等。
 * 电源和时钟已在设备注册阶段完成初始化。
 *
 * @note 使用双重检查锁定模式 (DCLP) 确保线程安全的单次探测
 */
static int arcs_sdmmc_probe(lisa_device_t *dev)
{
    if (!dev || !dev->priv_data) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_sdmmc_priv_t *priv = (lisa_sdmmc_priv_t *)dev->priv_data;

    /* 第一次检查（无锁，快速路径） */
    if (priv->initialized) {
        return LISA_DEVICE_OK;
    }

    DEVICE_LOCK(priv);

    /* 第二次检查（有锁，避免竞态） */
    if (priv->initialized) {
        DEVICE_UNLOCK(priv);
        return LISA_DEVICE_OK;
    }

    /* 探测 SD/MMC 卡并配置 */
    int ret = sdmmc_hard_init();
    if (ret != 0) {
        LISA_LOGE(LOG_TAG, "SD/MMC card detection failed: %d", ret);
        DEVICE_UNLOCK(priv);
        return LISA_DEVICE_ERR_IO;
    }

    priv->initialized = true;

    DEVICE_UNLOCK(priv);

    LISA_LOGI(LOG_TAG, "SD/MMC card probed successfully");
    return LISA_DEVICE_OK;
}

/**
 * @brief 获取磁盘状态
 */
static int arcs_sdmmc_status(lisa_device_t *dev)
{
    if (!dev || !dev->priv_data) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_sdmmc_priv_t *priv = (lisa_sdmmc_priv_t *)dev->priv_data;

    if (!priv->initialized) {
        return LISA_SDMMC_STATUS_UNINIT;
    }

    return LISA_SDMMC_STATUS_OK;
}

/**
 * @brief 从磁盘读取扇区数据
 */
static int arcs_sdmmc_read(lisa_device_t *dev, uint8_t *buff, uint32_t sector, uint32_t count)
{
    int ret;
    uint32_t sectors_per_wrap = CONFIG_LISA_SDMMC_ACCESS_WRAP_BUFFER_SIZE / DISK_SECTOR_SIZE;

    if (!buff || count == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }

    ret = check_device_initialized(dev);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    lisa_sdmmc_priv_t *priv = (lisa_sdmmc_priv_t *)dev->priv_data;

    DEVICE_LOCK(priv);

    /* 检查缓冲区是否对齐 */
    if ((uint32_t)buff % CONFIG_LISA_SDMMC_ACCESS_BUFFER_ALIGN_SIZE != 0) {
        /* 非对齐缓冲区，使用 wrap buffer 分块读取 */
        uint32_t remaining = count;
        uint32_t current_sector = sector;
        uint8_t *dst_ptr = buff;

        while (remaining > 0) {
            uint32_t sectors_to_read = (remaining > sectors_per_wrap) ? sectors_per_wrap : remaining;

            ret = gm_sdc_api_sdcard_sector_read(SD_PORT, current_sector, sectors_to_read, read_wrap_buffer);
            if (ret != 0) {
                LISA_LOGE(LOG_TAG, "Disk read failed at sector %u, count %u: %d", current_sector, sectors_to_read, ret);
                DEVICE_UNLOCK(priv);
                return LISA_DEVICE_ERR_IO;
            }

            memcpy(dst_ptr, read_wrap_buffer, sectors_to_read * DISK_SECTOR_SIZE);
            dst_ptr += sectors_to_read * DISK_SECTOR_SIZE;
            current_sector += sectors_to_read;
            remaining -= sectors_to_read;
        }
    } else {
        /* 对齐缓冲区，直接读取 */
        ret = gm_sdc_api_sdcard_sector_read(SD_PORT, sector, count, buff);
        if (ret != 0) {
            LISA_LOGE(LOG_TAG, "Disk read failed at sector %u, count %u: %d", sector, count, ret);
            DEVICE_UNLOCK(priv);
            return LISA_DEVICE_ERR_IO;
        }
        /* DMA 读取后 invalidate cache，确保 CPU 读取到最新数据 */
#if CONFIG_DCACHE_ENABLE
        dcache_invalidate_range((uint32_t)buff, (uint32_t)buff + count * DISK_SECTOR_SIZE);
#endif
    }

    DEVICE_UNLOCK(priv);
    return LISA_DEVICE_OK;
}

/**
 * @brief 向磁盘写入扇区数据
 */
static int arcs_sdmmc_write(lisa_device_t *dev, const uint8_t *buff, uint32_t sector, uint32_t count)
{
    int ret;
    uint32_t sectors_per_wrap = CONFIG_LISA_SDMMC_ACCESS_WRAP_BUFFER_SIZE / DISK_SECTOR_SIZE;

    if (!buff || count == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }

    ret = check_device_initialized(dev);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    lisa_sdmmc_priv_t *priv = (lisa_sdmmc_priv_t *)dev->priv_data;

    DEVICE_LOCK(priv);

    /* 检查缓冲区是否对齐 */
    if ((uint32_t)buff % CONFIG_LISA_SDMMC_ACCESS_BUFFER_ALIGN_SIZE != 0) {
        /* 非对齐缓冲区，使用 wrap buffer 分块写入 */
        uint32_t remaining = count;
        uint32_t current_sector = sector;
        const uint8_t *src_ptr = buff;

        while (remaining > 0) {
            uint32_t sectors_to_write = (remaining > sectors_per_wrap) ? sectors_per_wrap : remaining;

            memcpy(write_wrap_buffer, src_ptr, sectors_to_write * DISK_SECTOR_SIZE);

            ret = gm_sdc_api_sdcard_sector_write(SD_PORT, current_sector, sectors_to_write, write_wrap_buffer);
            if (ret != 0) {
                LISA_LOGE(LOG_TAG, "Disk write failed at sector %u, count %u: %d", current_sector, sectors_to_write, ret);
                DEVICE_UNLOCK(priv);
                return LISA_DEVICE_ERR_IO;
            }

            src_ptr += sectors_to_write * DISK_SECTOR_SIZE;
            current_sector += sectors_to_write;
            remaining -= sectors_to_write;
        }
    } else {
        /* 对齐缓冲区，直接写入 */
        /* DMA 写入前 flush cache，确保内存数据是最新的 */
#if CONFIG_DCACHE_ENABLE
        dcache_flush_range((uint32_t)buff, (uint32_t)buff + count * DISK_SECTOR_SIZE);
#endif
        ret = gm_sdc_api_sdcard_sector_write(SD_PORT, sector, count, (void *)buff);
        if (ret != 0) {
            LISA_LOGE(LOG_TAG, "Disk write failed at sector %u, count %u: %d", sector, count, ret);
            DEVICE_UNLOCK(priv);
            return LISA_DEVICE_ERR_IO;
        }
    }

    DEVICE_UNLOCK(priv);
    return LISA_DEVICE_OK;
}

/**
 * @brief 磁盘控制命令
 */
static int arcs_sdmmc_ioctl(lisa_device_t *dev, uint8_t cmd, void *buff)
{
    uint32_t blk_len, blk_num, erase_size;
    uint32_t err;
    int result = LISA_DEVICE_OK;

    /* SYNC 命令：SD/MMC 是同步写入的，直接返回成功 */
    if (cmd == LISA_SDMMC_IOCTL_CTRL_SYNC) {
        return LISA_DEVICE_OK;
    }

    int ret = check_device_initialized(dev);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    lisa_sdmmc_priv_t *priv = (lisa_sdmmc_priv_t *)dev->priv_data;

    DEVICE_LOCK(priv);

    /* 获取 SD 卡信息 */
    err = lib_sdc_get_card_info(SD_PORT, &blk_len, &blk_num, &erase_size);
    if (err != 0) {
        LISA_LOGE(LOG_TAG, "Failed to get card info: %u", err);
        DEVICE_UNLOCK(priv);
        return LISA_DEVICE_ERR_IO;
    }

    switch (cmd) {
    case LISA_SDMMC_IOCTL_GET_SECTOR_COUNT:
        if (!buff) {
            result = LISA_DEVICE_ERR_INVALID;
        } else {
            *(uint32_t *)buff = blk_num;
        }
        break;

    case LISA_SDMMC_IOCTL_GET_SECTOR_SIZE:
        if (!buff) {
            result = LISA_DEVICE_ERR_INVALID;
        } else {
            *(uint32_t *)buff = blk_len;
        }
        break;

    case LISA_SDMMC_IOCTL_GET_ERASE_BLOCK_SZ:
        if (!buff) {
            result = LISA_DEVICE_ERR_INVALID;
        } else {
            *(uint32_t *)buff = erase_size;
        }
        break;

    default:
        result = LISA_DEVICE_ERR_INVALID;
        break;
    }

    DEVICE_UNLOCK(priv);
    return result;
}

/* ===== API 实例 ===== */
static const lisa_sdmmc_api_t arcs_sdmmc_api = {
    .probe = arcs_sdmmc_probe,
    .status = arcs_sdmmc_status,
    .read = arcs_sdmmc_read,
    .write = arcs_sdmmc_write,
    .ioctl = arcs_sdmmc_ioctl,
};

/* ===== 设备初始化函数 ===== */
static int arcs_sdmmc0_init(void)
{
    /* 初始化私有数据 */
    memset(&sdmmc0_priv, 0, sizeof(sdmmc0_priv));

    /* 创建互斥锁 */
    sdmmc0_priv.mutex = lisa_mutex_create();
    if (!sdmmc0_priv.mutex) {
        LISA_LOGE(LOG_TAG, "Failed to create mutex");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 平台电源和时钟初始化 */
    int ret = sdmmc_platform_init();
    if (ret != 0) {
        LISA_LOGE(LOG_TAG, "SDMMC platform init failed: %d", ret);
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    sdmmc0_priv.initialized = false;

    lisa_sdio_pinmux();

    LISA_LOGD(LOG_TAG, "SDMMC device registered (platform initialized, call lisa_sdmmc_probe to detect card)");

    return LISA_DEVICE_OK;
}

/* ===== 设备注册 ===== */
LISA_DEVICE_REGISTER(sdmmc0,                       /* 设备名称 */
                     &arcs_sdmmc_api,               /* API 指针 */
                     &sdmmc0_priv,                  /* 私有数据指针 */
                     NULL,                         /* 用户数据 */
                     arcs_sdmmc0_init,              /* 初始化函数 */
                     CONFIG_LISA_SDMMC_INIT_PRIORITY); /* 优先级 */

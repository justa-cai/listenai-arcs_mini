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

#if CONFIG_LISA_PM
#include "lisa_pm.h"
#endif

/* ========================================================================
 * DISK 常量定义
 * ======================================================================== */

#define DISK_SECTOR_SIZE 512
#define SD_PORT SD_0

#ifndef CONFIG_LISA_SDMMC_ACCESS_BUFFER_ALIGN_SIZE
#define CONFIG_LISA_SDMMC_ACCESS_BUFFER_ALIGN_SIZE 32
#endif

#ifndef CONFIG_LISA_SDMMC_ACCESS_WRAP_BUFFER_SIZE
#define CONFIG_LISA_SDMMC_ACCESS_WRAP_BUFFER_SIZE (4 * 1024)
#endif

/* ========================================================================
 * DISK 私有数据结构定义
 * ======================================================================== */

typedef struct {
    lisa_mutex_t *mutex;          /* 互斥锁 */
    bool initialized;             /* 初始化标志 */
    volatile bool xfer_in_flight; /* 扇区读写在途标志（system PM check_idle 用） */
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
    priv->xfer_in_flight = true;

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
                uint32_t reset_flag = 6; /* SDHCI_SOFTRST_CMD | SDHCI_SOFTRST_DAT */
                gm_sdc_api_action(SD_PORT, GM_SDC_ACTION_SOFT_RESET, &reset_flag, NULL);
                ret = gm_sdc_api_sdcard_sector_read(SD_PORT, current_sector, sectors_to_read, read_wrap_buffer);
                if (ret != 0) {
                    LISA_LOGE(LOG_TAG, "Disk read failed at sector %u, count %u: %d", current_sector, sectors_to_read, ret);
                    DEVICE_UNLOCK(priv);
                    return LISA_DEVICE_ERR_IO;
                }
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
            uint32_t reset_flag = 6; /* SDHCI_SOFTRST_CMD | SDHCI_SOFTRST_DAT */
            gm_sdc_api_action(SD_PORT, GM_SDC_ACTION_SOFT_RESET, &reset_flag, NULL);
            ret = gm_sdc_api_sdcard_sector_read(SD_PORT, sector, count, buff);
            if (ret != 0) {
                LISA_LOGE(LOG_TAG, "Disk read failed at sector %u, count %u: %d", sector, count, ret);
                DEVICE_UNLOCK(priv);
                return LISA_DEVICE_ERR_IO;
            }
        }
    }

    priv->xfer_in_flight = false;
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
    priv->xfer_in_flight = true;

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
                priv->xfer_in_flight = false;
                DEVICE_UNLOCK(priv);
                return LISA_DEVICE_ERR_IO;
            }

            src_ptr += sectors_to_write * DISK_SECTOR_SIZE;
            current_sector += sectors_to_write;
            remaining -= sectors_to_write;
        }
    } else {
        ret = gm_sdc_api_sdcard_sector_write(SD_PORT, sector, count, (void *)buff);
        if (ret != 0) {
            LISA_LOGE(LOG_TAG, "Disk write failed at sector %u, count %u: %d", sector, count, ret);
            priv->xfer_in_flight = false;
            DEVICE_UNLOCK(priv);
            return LISA_DEVICE_ERR_IO;
        }
    }

    priv->xfer_in_flight = false;
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

/* OS 资源初始化：仅在首次 _init_fn 调用一次，不参与 resume 路径 */
static int arcs_sdmmc_init_resources(lisa_sdmmc_priv_t *priv)
{
    priv->mutex = lisa_mutex_create();
    if (!priv->mutex) {
        LISA_LOGE(LOG_TAG, "Failed to create mutex");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }
    return LISA_DEVICE_OK;
}

/* HAL/硬件初始化：幂等，由 _init_fn 调用（唤醒后经 reinit 重新走本路径）。
 * 不分配资源、不创建 OS 对象。仅做平台电源/时钟与 pinmux，
 * 探卡（sdmmc_hard_init）继续走懒加载，由 arcs_sdmmc_probe 触发。 */
static int arcs_sdmmc_init_hw(lisa_sdmmc_priv_t *priv)
{
    int ret = sdmmc_platform_init();
    if (ret != 0) {
        LISA_LOGE(LOG_TAG, "SDMMC platform init failed: %d", ret);
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 平台刚（重新）上电，旧的探卡状态作废，等下次 probe 再探。 */
    priv->initialized = false;
    priv->xfer_in_flight = false;

    lisa_sdio_pinmux();

    return LISA_DEVICE_OK;
}

static int arcs_sdmmc0_init(void)
{
    /* 初始化私有数据 */
    memset(&sdmmc0_priv, 0, sizeof(sdmmc0_priv));

    int ret = arcs_sdmmc_init_resources(&sdmmc0_priv);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    ret = arcs_sdmmc_init_hw(&sdmmc0_priv);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    LISA_LOGD(LOG_TAG, "SDMMC device registered (platform initialized, call lisa_sdmmc_probe to detect card)");

    return LISA_DEVICE_OK;
}

/* ===== 设备反初始化函数 ===== */

/**
 * @brief 停止并释放 SDMMC0 设备的全部软硬件资源，恢复芯片上电初始状态
 *
 * 由 lisa_device_destroy() 调用。释放顺序与 arcs_sdmmc0_init 申请相反：
 *   1) HAL 无 *_Uninitialize / PowerControl 显式接口，无需显式下电（平台侧
 *      sdmmc_platform_init 不可逆，唤醒后 reinit 会幂等重做）；
 *   2) 释放 OS 资源 mutex；
 *   3) memset 整个 priv，回到 _init 之前的零初值（initialized / xfer_in_flight
 *      随之清零，强制唤醒后业务侧重新 probe 探卡）。
 *
 * wrap buffer 为 .psram.data 静态段，无需 free。
 * 约定：调用方需保证此时无扇区读写在途、无并发业务在使用本设备。
 */
static int arcs_sdmmc0_deinit(void)
{
    lisa_sdmmc_priv_t *priv = &sdmmc0_priv;

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
 * 调 lisa_device_destroy(sdmmc0) 释放全部软硬件资源（mutex），唤醒后在 PM after_wake
 * 回调中调 lisa_device_reinit(sdmmc0) 重建到 _init 后的状态（幂等重做平台电源/时钟与
 * pinmux），probe 由业务侧唤醒后按需重新触发。因此 prepare_suspend / resume_restore
 * 不再需要（原先它们只做 initialized 清零 / 平台重做，已被 destroy/reinit 覆盖，且二者
 * 运行于 PM 临界区无法做重活）。
 *
 * 仅保留 check_idle：只读 priv->xfer_in_flight（arcs_sdmmc_read / arcs_sdmmc_write
 * 入口置 1、所有返回路径清 0，覆盖整个扇区 DMA 区间），不取 mutex、不访问 HAL。
 */
static int32_t arcs_sdmmc_pm_check_idle(void *ctx)
{
    lisa_sdmmc_priv_t *priv = (lisa_sdmmc_priv_t *)ctx;
    if (priv == NULL) {
        return 1; /* 上下文异常时允许睡眠，不阻塞整机 */
    }
    return priv->xfer_in_flight ? 0 : 1;
}

static const lisa_pm_system_ops_t arcs_sdmmc_pm_ops = {
    .check_idle      = arcs_sdmmc_pm_check_idle,
    .prepare_suspend = NULL,
    .resume_restore  = NULL,
};
#endif /* CONFIG_LISA_PM */


/* ===== 设备注册 ===== */
LISA_DEVICE_REGISTER_DEINIT(sdmmc0, &arcs_sdmmc_api, &sdmmc0_priv, NULL, arcs_sdmmc0_init,
                            arcs_sdmmc0_deinit, LISA_DEVICE_LEVEL_NORMAL, CONFIG_LISA_SDMMC_INIT_PRIORITY);
#if CONFIG_LISA_PM
LISA_DEVICE_PM_ATTACH(sdmmc0, &arcs_sdmmc_pm_ops, NULL, &sdmmc0_priv);
#endif

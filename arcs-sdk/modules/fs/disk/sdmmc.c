/*
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file sdmmc.c
 * @brief SD/MMC disk 适配层 - 基于 lisa_sdmmc 驱动接口
 *
 * 本文件将 lisa_sdmmc 驱动接口适配到 FS 层的 disk_operations 接口，
 * 使得文件系统可以通过统一的 disk_access API 访问 SD/MMC 存储设备。
 */

#include <string.h>
#include <errno.h>
#include "fs_env/fs_env.h"
#include "disk/disk.h"
#include "disk/iocache.h"
#include "lisa_device.h"
#include "lisa_sdmmc.h"

/* lisa_sdmmc 设备指针 */
static lisa_device_t *lisa_sdmmc_dev = NULL;

/* FS 层 disk_info 结构体 */
static struct disk_info sdmmc_disk_raw;
static struct disk_info sdmmc_disk;

/**
 * @brief 获取 lisa_sdmmc 设备
 *
 * 延迟获取设备，确保设备已注册
 */
static lisa_device_t *get_lisa_sdmmc_dev(void)
{
    if (lisa_sdmmc_dev == NULL) {
        lisa_sdmmc_dev = lisa_device_get("sdmmc0");
    }
    return lisa_sdmmc_dev;
}

static int disk_sdmmc_access_init(struct disk_info *disk)
{
    ARG_UNUSED(disk);
    // Don't do any initialization in this function.
    // There are issues if sdmmc initialized called by file system.

    return 0;
}

static int disk_sdmmc_access_status(struct disk_info *disk)
{
    ARG_UNUSED(disk);

    lisa_device_t *dev = get_lisa_sdmmc_dev();

    if (dev == NULL) {
        return DISK_STATUS_NOMEDIA;
    }

    int status = lisa_sdmmc_status(dev);

    /* 转换 lisa_sdmmc 状态到 FS 层状态 */
    switch (status) {
    case LISA_SDMMC_STATUS_OK:
        return DISK_STATUS_OK;
    case LISA_SDMMC_STATUS_UNINIT:
        return DISK_STATUS_UNINIT;
    case LISA_SDMMC_STATUS_NO_MEDIA:
        return DISK_STATUS_NOMEDIA;
    case LISA_SDMMC_STATUS_BUSY:
    default:
        return DISK_STATUS_OK;
    }
}

static int disk_sdmmc_access_read(struct disk_info *disk, uint8_t *buff,
                                  uint32_t sector, uint32_t count)
{
    lisa_device_t *dev = get_lisa_sdmmc_dev();

    if (dev == NULL) {
        return -EIO;
    }

    /* RAW 模式不偏移扇区，普通模式从配置的起始扇区开始 */
    if (disk != &sdmmc_disk_raw) {
        sector += CONFIG_DISK_SDMMC_START_SECTOR_NUMBER;
    }

    int ret = lisa_sdmmc_read(dev, buff, sector, count);
    if (ret != 0) {
        return -EIO;
    }

    return 0;
}

static int disk_sdmmc_access_write(struct disk_info *disk, const uint8_t *buff,
                                   uint32_t sector, uint32_t count)
{
    lisa_device_t *dev = get_lisa_sdmmc_dev();

    if (dev == NULL) {
        return -EIO;
    }

    /* RAW 模式不偏移扇区，普通模式从配置的起始扇区开始 */
    if (disk != &sdmmc_disk_raw) {
        sector += CONFIG_DISK_SDMMC_START_SECTOR_NUMBER;
    }

    int ret = lisa_sdmmc_write(dev, buff, sector, count);
    if (ret != 0) {
        return -EIO;
    }

    return 0;
}

static int disk_sdmmc_access_ioctl(struct disk_info *disk, uint8_t cmd, void *buff)
{
    lisa_device_t *dev = get_lisa_sdmmc_dev();
    uint32_t value;
    int ret;

    /* SYNC 命令不需要设备 */
    if (cmd == DISK_IOCTL_CTRL_SYNC) {
        if (dev != NULL) {
            lisa_sdmmc_sync(dev);
        }
        return 0;
    }

    if (dev == NULL) {
        return -EIO;
    }

    switch (cmd) {
    case DISK_IOCTL_GET_SECTOR_COUNT:
        ret = lisa_sdmmc_ioctl(dev, LISA_SDMMC_IOCTL_GET_SECTOR_COUNT, &value);
        if (ret != 0) {
            return -EIO;
        }
        /* 普通模式需要减去起始扇区偏移 */
        if (disk != &sdmmc_disk_raw) {
            value -= CONFIG_DISK_SDMMC_START_SECTOR_NUMBER;
        }
        *(uint32_t *)buff = value;
        return 0;

    case DISK_IOCTL_GET_SECTOR_SIZE:
        ret = lisa_sdmmc_ioctl(dev, LISA_SDMMC_IOCTL_GET_SECTOR_SIZE, &value);
        if (ret != 0) {
            return -EIO;
        }
        *(uint32_t *)buff = value;
        return 0;

    case DISK_IOCTL_GET_ERASE_BLOCK_SZ:
        ret = lisa_sdmmc_ioctl(dev, LISA_SDMMC_IOCTL_GET_ERASE_BLOCK_SZ, &value);
        if (ret != 0) {
            return -EIO;
        }
        *(uint32_t *)buff = value;
        return 0;

    default:
        break;
    }

    return -EINVAL;
}

static const struct disk_operations sdmmc_disk_ops = {
    .init = disk_sdmmc_access_init,
    .status = disk_sdmmc_access_status,
    .read = disk_sdmmc_access_read,
    .write = disk_sdmmc_access_write,
    .ioctl = disk_sdmmc_access_ioctl,
};

static struct disk_info sdmmc_disk = {
    .name = CONFIG_DISK_SDMMC_VOLUME_NAME,
    .ops = &sdmmc_disk_ops,
};

static const struct disk_operations sdmmc_disk_raw_ops = {
    .init = disk_sdmmc_access_init,
    .status = disk_sdmmc_access_status,
    .read = disk_sdmmc_access_read,
    .write = disk_sdmmc_access_write,
    .ioctl = disk_sdmmc_access_ioctl,
};

static struct disk_info sdmmc_disk_raw = {
    .name = "SDRAW",
    .ops = &sdmmc_disk_raw_ops,
};

int disk_sdmmc_init(const void *dev)
{
    int r;
    ARG_UNUSED(dev);

    r = disk_access_register(&sdmmc_disk);
    if (r) {
        return r;
    }

    r = disk_access_register(&sdmmc_disk_raw);
    if (r) {
        return r;
    }

#if CONFIG_DISK_SDMMC_IOCACHE_ENABLE
    struct io_cache_config cache_config;
    io_cache_handle_t cache_handle;
    cache_config.entry_count = CONFIG_DISK_SDMMC_IOCACHE_ENTRY_COUNT;
    cache_config.max_block_size = CONFIG_DISK_SDMMC_IOCACHE_MAX_BLOCK_SIZE;
    cache_config.sector_size = CONFIG_DISK_SDMMC_IOCACHE_SECTOR_SIZE;

    cache_handle = io_cache_create(&sdmmc_disk, &cache_config);
    io_cache_enable(cache_handle, &sdmmc_disk);
#endif
    return 0;
}

/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define TAG "app-msc"

#include <stdint.h>

#include "disk/disk_access.h"
#include "lisa_log.h"
#include "usbd_core.h"
#include "usbd_msc.h"

#define APP_USB_MSC_BLOCK_SIZE     512U
#define APP_USB_MSC_FALLBACK_BLOCKS 0x1000U

/* 板型无 SD 卡（CONFIG_DISK_DRIVER_SDMMC 关闭）时，USB-MSC 退化为空盘：
 * 容量 0、读写拒绝，避免把 flash/KV 分区暴露给主机。 */
#ifdef CONFIG_DISK_DRIVER_SDMMC
#define APP_USB_MSC_DISK_NAME CONFIG_DISK_SDMMC_VOLUME_NAME
#endif

void usbd_msc_get_cap(uint8_t busid, uint8_t lun,
                      uint32_t *block_num, uint32_t *block_size)
{
    uint32_t sector_count = 0U;
    uint32_t sector_size = APP_USB_MSC_BLOCK_SIZE;

    (void)busid;
    (void)lun;

#ifdef APP_USB_MSC_DISK_NAME
    const char *disk = APP_USB_MSC_DISK_NAME;

    if (disk_access_ioctl(disk, DISK_IOCTL_GET_SECTOR_COUNT, &sector_count) != 0 ||
        disk_access_ioctl(disk, DISK_IOCTL_GET_SECTOR_SIZE, &sector_size) != 0 ||
        sector_size == 0U) {
        sector_count = APP_USB_MSC_FALLBACK_BLOCKS;
        sector_size = APP_USB_MSC_BLOCK_SIZE;
    }
#endif

    *block_num = sector_count;
    *block_size = sector_size;
    LISA_LOGI(TAG, "capacity: %lu blocks, %lu bytes",
              (unsigned long)sector_count, (unsigned long)sector_size);
}

int usbd_msc_sector_read(uint8_t busid, uint8_t lun, uint32_t sector,
                         uint8_t *buffer, uint32_t length)
{
#ifdef APP_USB_MSC_DISK_NAME
    const char *disk = APP_USB_MSC_DISK_NAME;

    (void)lun;

    if (buffer == NULL || length == 0U ||
        (length % APP_USB_MSC_BLOCK_SIZE) != 0U ||
        usbd_msc_get_popup(busid) ||
        disk_access_status(disk) != DISK_STATUS_OK) {
        return -1;
    }

    return disk_access_read(disk, buffer, sector,
                            length / APP_USB_MSC_BLOCK_SIZE);
#else
    (void)busid;
    (void)lun;
    (void)sector;
    (void)buffer;
    (void)length;
    return -1;
#endif
}

int usbd_msc_sector_write(uint8_t busid, uint8_t lun, uint32_t sector,
                          uint8_t *buffer, uint32_t length)
{
#ifdef APP_USB_MSC_DISK_NAME
    const char *disk = APP_USB_MSC_DISK_NAME;

    (void)lun;

    if (buffer == NULL || length == 0U ||
        (length % APP_USB_MSC_BLOCK_SIZE) != 0U ||
        usbd_msc_get_popup(busid) ||
        disk_access_status(disk) != DISK_STATUS_OK) {
        return -1;
    }

    return disk_access_write(disk, buffer, sector,
                             length / APP_USB_MSC_BLOCK_SIZE);
#else
    (void)busid;
    (void)lun;
    (void)sector;
    (void)buffer;
    (void)length;
    return -1;
#endif
}

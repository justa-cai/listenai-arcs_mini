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

void usbd_msc_get_cap(uint8_t busid, uint8_t lun,
                      uint32_t *block_num, uint32_t *block_size)
{
    const char *disk = CONFIG_DISK_SDMMC_VOLUME_NAME;
    uint32_t sector_count = 0U;
    uint32_t sector_size = 0U;

    (void)busid;
    (void)lun;

    if (disk_access_ioctl(disk, DISK_IOCTL_GET_SECTOR_COUNT, &sector_count) != 0 ||
        disk_access_ioctl(disk, DISK_IOCTL_GET_SECTOR_SIZE, &sector_size) != 0 ||
        sector_size == 0U) {
        sector_count = APP_USB_MSC_FALLBACK_BLOCKS;
        sector_size = APP_USB_MSC_BLOCK_SIZE;
    }

    *block_num = sector_count;
    *block_size = sector_size;
    LISA_LOGI(TAG, "capacity: %lu blocks, %lu bytes",
              (unsigned long)sector_count, (unsigned long)sector_size);
}

int usbd_msc_sector_read(uint8_t busid, uint8_t lun, uint32_t sector,
                         uint8_t *buffer, uint32_t length)
{
    const char *disk = CONFIG_DISK_SDMMC_VOLUME_NAME;

    (void)lun;

    if (buffer == NULL || length == 0U ||
        (length % APP_USB_MSC_BLOCK_SIZE) != 0U ||
        usbd_msc_get_popup(busid) ||
        disk_access_status(disk) != DISK_STATUS_OK) {
        return -1;
    }

    return disk_access_read(disk, buffer, sector,
                            length / APP_USB_MSC_BLOCK_SIZE);
}

int usbd_msc_sector_write(uint8_t busid, uint8_t lun, uint32_t sector,
                          uint8_t *buffer, uint32_t length)
{
    const char *disk = CONFIG_DISK_SDMMC_VOLUME_NAME;

    (void)lun;

    if (buffer == NULL || length == 0U ||
        (length % APP_USB_MSC_BLOCK_SIZE) != 0U ||
        usbd_msc_get_popup(busid) ||
        disk_access_status(disk) != DISK_STATUS_OK) {
        return -1;
    }

    return disk_access_write(disk, buffer, sector,
                             length / APP_USB_MSC_BLOCK_SIZE);
}

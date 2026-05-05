/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "lsfs.h"
#include "disk/disk_access.h"
#include "lisa_sdmmc.h"
#include "log_print.h"
#if CONFIG_LVFS
#include "lvfs.h"
#endif

// #define FLASHDISK_DEVICE "NAND:"
#define DISK_DEVICE      "SD:"
#define DISK_MOUNT_POINT "/" DISK_DEVICE

static struct lsfs_mount_t flash_lsfs_mnt = {
    .type = LSFS_FATFS,
    .mnt_point = DISK_MOUNT_POINT,
    .fs_data = NULL,
};


static void user_fs_change_to_root_folder(void)
{
    int ret;
    ret = lvfs_chdrive(DISK_MOUNT_POINT);
    if (ret != 0) {
        CLOG("Failed to change drive to %s, error: %d\n", DISK_MOUNT_POINT, ret);
    }
    
    ret = lvfs_chdir(DISK_MOUNT_POINT);
    if (ret != 0) {
        CLOG("Failed to change directory to /SD:/, error: %d\n", ret);
    } else {
        CLOG("Changed to directory /SD:/\n");
    }
}

int user_fs_init(void){
    int ret;

    lisa_sdmmc_probe(lisa_device_get("sdmmc0"));
    disk_init(NULL);
#if CONFIG_LVFS
    lvfs_init();
#endif
    lsfs_init();

    ret = lsfs_mkfs(flash_lsfs_mnt.type, &flash_lsfs_mnt.mnt_point[1], NULL, 0);
    if (ret != 0) {
        CLOG("Failed to format filesystem: %d", ret);
        return ret;
    }

    ret = lsfs_mount(&flash_lsfs_mnt);
    if (ret != 0) {
        CLOG("Failed to mount filesystem after mkfs: %d", ret);
        return ret;
    }

    user_fs_change_to_root_folder();

    return 0;
}

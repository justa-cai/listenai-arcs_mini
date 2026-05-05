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

#define DISK_DEVICE      "SD:"
#define DISK_MOUNT_POINT "/" DISK_DEVICE

static struct lsfs_mount_t flash_lsfs_mnt = {
    .type = LSFS_FATFS,
    .mnt_point = DISK_MOUNT_POINT,
    .fs_data = NULL,
};

static int fs_mount(struct lsfs_mount_t *mp)
{
    int ret;

    ret = lsfs_mount(mp);
    if (ret != 0)
    {
        CLOG("No file system, try reformatting, mount ret: %d", ret);
        ret = lsfs_mkfs(mp->type, &mp->mnt_point[1], NULL, 0);
        if (ret == 0)
        {
            CLOG("mkfs success, retry mount");
            ret = lsfs_mount(mp);
        }
        else
        {
            CLOG("mkfs failed, ret: %d", ret);
        }
    }

    if (ret != 0)
    {
        CLOG("Failed to mount filesystem: %d", ret);
        return ret;
    }
    CLOG("%s mounted successfully", mp->mnt_point);

    return 0;
}

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
    lisa_sdmmc_probe(lisa_device_get("sdmmc0"));
    disk_init(NULL);
#if CONFIG_LVFS
    lvfs_init();
#endif
    lsfs_init();

    if(0 != fs_mount(&flash_lsfs_mnt)){
        return -1;
    }

    user_fs_change_to_root_folder();

    return 0;
}
#include <string.h>
#include "lsfs.h"
#include "disk/disk_access.h"
#include "lisa_sdmmc.h"
#include "log_print.h"
#include "fs.h"
#if CONFIG_LVFS_POSIX_API
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

static int fs_mount(struct lsfs_mount_t *mp)
{
    int ret;

    ret = lsfs_mount(mp);
    if (ret != 0)
    {
        CLOG("Failed to mount filesystem: %d", ret);
        return ret;
    }
    CLOG("%s mounted successfully", mp->mnt_point);
}

static int fs_unmount(struct lsfs_mount_t *mp)
{
    int ret;

    ret = lsfs_unmount(mp);
    if (ret != 0)
    {
        CLOG("%s unmount failed!\n", mp->mnt_point);
    }
    CLOG("%s unmount success!\n", mp->mnt_point);
}

static int list_dir(const char *path)
{
    struct lsfs_dir_t dir;
    struct lsfs_dirent entry;
    int ret;
    int nfile = 0, ndir = 0;
    /* 初始化目录对象 */
    lsfs_dir_t_init(&dir);

    ret = lsfs_opendir(&dir, path);
    CLOG("%s opendir: %d", path, ret);
    if (ret == 0)
    {

        while (1)
        {
            ret = lsfs_readdir(&dir, &entry);
            if (ret != 0 || entry.name[0] == 0)
                break; /* Error or end of dir */

            if (entry.type == LSFS_DIR_ENTRY_DIR)
            {
                /* Directory */
                CLOG("   <DIR>   %s", entry.name);
                ndir++;
            }
            else
            {
                /* File */
                CLOG("%10lu %s", entry.size, entry.name);
                nfile++;
            }
        }
        lsfs_closedir(&dir);
        CLOG("%d dirs, %d files.", ndir, nfile);
    }
    else
    {
        CLOG("Failed to open \"%s\". (%u)", path, ret);
    }
    return ret;
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
#if CONFIG_LVFS_POSIX_API
    lvfs_init();
#endif
    lsfs_init();

    if(0 != fs_mount(&flash_lsfs_mnt)){
        CLOG("fs_mount fail!!");
        return -1;
    }
    
    user_fs_change_to_root_folder();
    
    return 0;
}

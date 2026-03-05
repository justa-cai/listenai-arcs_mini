#include <string.h>
#include "lsfs.h"
#include "disk/disk_access.h"
#include "arcs_ap.h"
#include "log_print.h"
#include "fs.h"
#if CONFIG_LVFS_POSIX_API
#include "lvfs.h"
#endif

// #define FLASHDISK_DEVICE "NAND:"
#define DISK_DEVICE      "NAND:"
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
    if (ret != 0) {
        CLOG("Failed to mount filesystem: %d", ret);
        return ret;
    }
    CLOG("%s mounted successfully", mp->mnt_point);
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

int user_fs_init(void)
{
    int ret = 0;
    disk_init(NULL);
#if CONFIG_LVFS_POSIX_API
    lvfs_init();
#endif
    lsfs_init();

    if (0 != fs_mount(&flash_lsfs_mnt)) {
        ret = lsfs_mkfs(flash_lsfs_mnt.type, &flash_lsfs_mnt.mnt_point[1], NULL, 0);
        if (ret != 0) {
            CLOG("Failed to create filesystem: %d", ret);
            return ret;
        }
        if (0 != fs_mount(&flash_lsfs_mnt)) {
            CLOG("Failed to mount filesystem: %d", ret);
            return ret;
        }
    }

    return 0;
}

#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include "lsfs.h"
#include "lvfs.h"
#include "disk/disk_access.h"
#include "lisa_log.h"

#define RAMDISK_DEVICE      "RAM:"
#define RAMDISK_MOUNT_POINT "/"RAMDISK_DEVICE
#define RAMDISK_FILE RAMDISK_MOUNT_POINT"/ram.txt"

static struct lsfs_mount_t ram_lsfs_mnt = {
    .type = LSFS_FATFS,
    .mnt_point = RAMDISK_MOUNT_POINT,
    .fs_data = NULL,
};

static int fs_mount(struct lsfs_mount_t* mp)
{
    int ret;

    ret = lsfs_mount(mp);
    if(ret != 0){
        LOGD("No file system reformatting");
        ret = lsfs_mkfs(mp->type,&mp->mnt_point[1],NULL,0);
        if(ret == 0){
            LOGD("mkfs success");
            ret = lsfs_mount(mp);
        }
        else{
            LOGE("mkfs failed,fr:%d", ret);
        }
    }

    if (ret != 0)
    {
        LOGE("Failed to mount filesystem: %d", ret);
        return ret;
    }
    LOGI("%s mounted successfully", mp->mnt_point);

}

static int fs_unmount(struct lsfs_mount_t* mp)
{
    int ret;

    ret = lsfs_unmount(mp);
    if (ret != 0)
    {
        LOGE("%s unmount failed!\n", mp->mnt_point);
        return ret;
    }
    LOGI("%s unmount success!\n", mp->mnt_point);
    return 0;
}

extern int test_posix_operations(void);
int main(int argc, char **argv)
{
    LOGI("LVFS sample\n");
    
    /*初始化disk设备*/
    disk_init(NULL);

    /*初始化文件系统 */
    lvfs_init();
    lsfs_init();

    fs_mount(&ram_lsfs_mnt);
    test_posix_operations();
#if CONFIG_LVFS_POSIX_API_ALIAS
    extern int test_posix_alias_operations(void);
    test_posix_alias_operations();
#endif
    fs_unmount(&ram_lsfs_mnt);



    return 0;
}

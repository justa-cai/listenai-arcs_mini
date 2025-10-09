#include <string.h>
#include "lsfs.h"
#include "disk/disk_access.h"
#include "lisa_log.h"

#define RAMDISK_DEVICE      "RAM:"
#define RAMDISK_MOUNT_POINT "/"RAMDISK_DEVICE
#define RAMDISK_FILE RAMDISK_MOUNT_POINT "/ram.txt"

#define FLASHDISK_DEVICE      "NAND:"
#define FLASHDISK_MOUNT_POINT "/"FLASHDISK_DEVICE
#define FLASHDISK_FILE FLASHDISK_MOUNT_POINT "/flash.txt"

#define SDMMC_DEVICE      "SD:"
#define SDMMC_MOUNT_POINT "/"SDMMC_DEVICE
#define SDMMC_FILE SDMMC_MOUNT_POINT "/sdmmc.txt"

static struct lsfs_mount_t ram_lsfs_mnt = {
    .type = LSFS_FATFS,
    .mnt_point = RAMDISK_MOUNT_POINT,
    .fs_data = NULL,
};

static struct lsfs_mount_t flash_lsfs_mnt = {
    .type = LSFS_FATFS,
    .mnt_point = FLASHDISK_MOUNT_POINT,
    .fs_data = NULL,
};

static struct lsfs_mount_t sdmmc_lsfs_mnt = {
    .type = LSFS_FATFS,
    .mnt_point = SDMMC_MOUNT_POINT,
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

static int list_dir(const char *path)
{
    struct lsfs_dir_t dir;
    struct lsfs_dirent entry;
    int ret;
    int nfile = 0, ndir = 0;
    /* 初始化目录对象 */
    lsfs_dir_t_init(&dir);

    ret = lsfs_opendir(&dir, path);
    LOGI("%s opendir: %d", path, ret);
    if(ret == 0){

        while(1){
            ret = lsfs_readdir(&dir, &entry);
            if (ret != 0 || entry.name[0] == 0)
                break; /* Error or end of dir */
            
            if(entry.type == LSFS_DIR_ENTRY_DIR){
                /* Directory */
                LOGI("   <DIR>   %s", entry.name);
                ndir++;
            }
            else{
                /* File */
                LOGI("%10lu %s", entry.size, entry.name);
                nfile++;                
            }
        }
        lsfs_closedir(&dir);
        LOGI("%d dirs, %d files.", ndir, nfile);
    }
    else
    {
        LOGE("Failed to open \"%s\". (%u)", path, ret);
    }
    return ret;
}

int fs_write_read(const char *fullpath)
{
    struct lsfs_file_t file;
    int ret;
    char buffer[64];

    lsfs_file_t_init(&file);
    ret = lsfs_open(&file, fullpath, LSFS_O_CREATE | LSFS_O_TRUNC | LSFS_O_WRITE);
    if (ret != 0)
    {
        LOGE("Failed to open file: %d", ret);
        return ret;
    }

    ret = lsfs_write(&file, fullpath, strlen(fullpath));
    if (ret != strlen(fullpath))
    {
        LOGE("Failed to write to file: %d", ret);
        return ret;
    }
    LOGI("Write to file: %s", fullpath);

    lsfs_close(&file);

    lsfs_file_t_init(&file);
    ret = lsfs_open(&file, fullpath, LSFS_O_READ);
    if (ret != 0)
    {
        LOGE("Failed to open file for reading: %d", ret);
        return ret;
    }

    ret = lsfs_read(&file, buffer, sizeof(buffer) - 1);
    if (ret <= 0)
    {
        LOGE("Failed to read from file: %d", ret);
        return ret;
    }

    buffer[ret] = '\0';
    LOGI("Read from file: %s", buffer);

    lsfs_close(&file);

    return 0;
}

int main(int argc, char **argv)
{
    LOGI("LSFS sample\n");
 #if (CONFIG_DISK_DRIVER_SDMMC)
    extern int sdmmc_hard_init(void);
    if(0 != sdmmc_hard_init()){
        LOGE("sdmmc hard init failed");
        return -1;
    }
#endif

    disk_init(NULL);
    lsfs_init();


    fs_mount(&ram_lsfs_mnt);
    fs_write_read(RAMDISK_FILE);
    list_dir(RAMDISK_MOUNT_POINT "/");
    fs_unmount(&ram_lsfs_mnt);

#if (CONFIG_DISK_DRIVER_FLASH)
    fs_mount(&flash_lsfs_mnt);
    fs_write_read(FLASHDISK_FILE);
    list_dir(FLASHDISK_MOUNT_POINT "/");
    fs_unmount(&flash_lsfs_mnt);
#endif

#if (CONFIG_DISK_DRIVER_SDMMC)
    fs_mount(&sdmmc_lsfs_mnt);
    fs_write_read(SDMMC_FILE);
    list_dir(SDMMC_MOUNT_POINT "/");
    fs_unmount(&sdmmc_lsfs_mnt);
#endif

    return 0;
}

#include <string.h>
#include "ff.h"
#include "disk/disk_access.h"
#include "lisa_log.h"

#define RAMDISK_MOUNT_POINT "RAM:"
#define RAMDISK_FILE RAMDISK_MOUNT_POINT "/ram.txt"

#define FLASHDISK_MOUNT_POINT "NAND:"
#define FLASHDISK_FILE FLASHDISK_MOUNT_POINT "/flash.txt"

#define SDMMC_MOUNT_POINT "SD:"
#define SDMMC_FILE SDMMC_MOUNT_POINT "/sdmmc.txt"

static FATFS s_fs;

static int fs_mount(const char *mountpoint)
{

    FRESULT fr;

    // 挂载文件系统
    fr = f_mount(&s_fs, mountpoint, 1);

    if (fr == FR_NO_FILESYSTEM)
    {
        static uint8_t work[FF_MAX_SS];
        MKFS_PARM mkfs_opt = {
            .fmt = FM_ANY | FM_SFD, /* Any suitable FAT */
            .n_fat = 1,             /* One FAT fs table */
            .align = 0,             /* Get sector size via diskio query */
            .n_root = 512,
            .au_size = 0 /* Auto calculate cluster size */
        };

        LOGD("No file system reformatting");
        fr = f_mkfs(mountpoint, &mkfs_opt, work, sizeof(work));
        if (fr == FR_OK)
        {
            LOGD("mkfs success");
            fr = f_mount(&s_fs, mountpoint, 1);
        }
        else
        {
            LOGE("mkfs failed,fr:%d", fr);
        }
    }
    if (fr != FR_OK)
    {
        LOGE("Failed to mount filesystem: %d", fr);
        return fr;
    }

    LOGI("%s mounted successfully", mountpoint);
}

static int fs_unmount(const char *mountpoint)
{
    FRESULT fr;

    fr = f_mount(NULL, mountpoint, 0);
    if (fr != FR_OK)
    {
        LOGE("%s unmount failed!\n", mountpoint);
    }
    LOGI("%s unmount success!\n", mountpoint);
}

static FRESULT list_dir(const char *path)
{
    FRESULT res;
    DIR dir;
    FILINFO fno;
    int nfile, ndir;

    res = f_opendir(&dir, path); /* Open the directory */
    LOGI("%s opendir: %d", path, res);
    if (res == FR_OK)
    {
        nfile = ndir = 0;
        for (;;)
        {
            res = f_readdir(&dir, &fno); /* Read a directory item */
            if (res != FR_OK || fno.fname[0] == 0)
                break; /* Error or end of dir */
            if (fno.fattrib & AM_DIR)
            { /* Directory */
                LOGI("   <DIR>   %s", fno.fname);
                ndir++;
            }
            else
            { /* File */
                LOGI("%10llu %s", fno.fsize, fno.fname);
                nfile++;
            }
        }
        f_closedir(&dir);
        LOGI("%d dirs, %d files.", ndir, nfile);
    }
    else
    {
        LOGE("Failed to open \"%s\". (%u)", path, res);
    }
    return res;
}

int fatfs_write_read(const char *fullpath)
{
    FIL fil;
    FRESULT fr;
    UINT bw;
    UINT br;
    char buffer[64];

    // 创建并打开一个新文件
    fr = f_open(&fil, fullpath, FA_WRITE | FA_CREATE_ALWAYS);
    if (fr != FR_OK)
    {
        LOGE("Failed to open file: %d", fr);
        return fr;
    }

    fr = f_write(&fil, fullpath, strlen(fullpath), &bw);
    if (fr != FR_OK || bw != strlen(fullpath))
    {
        LOGE("Failed to write to file: %d", fr);
        return fr;
    }
    LOGI("Write to file: %s", fullpath);

    // 关闭文件
    f_close(&fil);

    // 读取文件
    fr = f_open(&fil, fullpath, FA_READ);
    if (fr != FR_OK)
    {
        LOGE("Failed to open file for reading: %d", fr);
        return fr;
    }

    fr = f_read(&fil, buffer, sizeof(buffer) - 1, &br);
    if (fr != FR_OK)
    {
        LOGE("Failed to read from file: %d", fr);
        return fr;
    }

    buffer[br] = '\0';
    LOGI("Read from file: %s", buffer);

    f_close(&fil);

    return 0;
}

int main(int argc, char **argv)
{
    LOGI("FATFS sample\n");

 #if (CONFIG_DISK_DRIVER_SDMMC)
    extern int sdmmc_hard_init(void);
    if(0 != sdmmc_hard_init()){
        LOGE("sdmmc hard init failed");
        return -1;
    }
#endif
    disk_init(NULL);

    fs_mount(RAMDISK_MOUNT_POINT);
    fatfs_write_read(RAMDISK_FILE);
    list_dir(RAMDISK_MOUNT_POINT "/");
    fs_unmount(RAMDISK_MOUNT_POINT);

#if (CONFIG_DISK_DRIVER_FLASH)
    fs_mount(FLASHDISK_MOUNT_POINT);
    fatfs_write_read(FLASHDISK_FILE);
    list_dir(FLASHDISK_MOUNT_POINT "/");
    fs_unmount(FLASHDISK_MOUNT_POINT);
#endif

#if (CONFIG_DISK_DRIVER_SDMMC)
    fs_mount(SDMMC_MOUNT_POINT);
    fatfs_write_read(SDMMC_FILE);
    list_dir(SDMMC_MOUNT_POINT "/");
    fs_unmount(SDMMC_MOUNT_POINT);
#endif

    return 0;
}

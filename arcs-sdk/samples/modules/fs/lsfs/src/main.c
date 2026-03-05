#define LOG_TAG "sample"
#include <lisa_log.h>
#include <string.h>
#include "lsfs.h"
#include "disk/disk_access.h"
#include "lisa_sdmmc.h"

#define SDMMC_DEVICE      "SD:"
#define SDMMC_MOUNT_POINT "/"SDMMC_DEVICE
#define TEST_FILE         SDMMC_MOUNT_POINT"/test.txt"

static struct lsfs_mount_t sdmmc_mnt = {
    .type = LSFS_FATFS,
    .mnt_point = SDMMC_MOUNT_POINT,
    .fs_data = NULL,
};

int main(int argc, char **argv)
{
    int ret;
    struct lsfs_file_t file;
    struct lsfs_dir_t dir;
    struct lsfs_dirent entry;
    char read_buffer[64];
    const char *write_data = "Hello, LSFS!";
    ssize_t bytes;

    LOGI("=== LSFS Sample ===\n");

    /* 1. 初始化 SD 卡和磁盘子系统 */
    lisa_sdmmc_probe(lisa_device_get("sdmmc0"));
    disk_init(NULL);
    lsfs_init();

    /* 2. 挂载文件系统 */
    ret = lsfs_mount(&sdmmc_mnt);
    if (ret != 0) {
        LOGI("Mount failed, formatting...");
        ret = lsfs_mkfs(LSFS_FATFS, SDMMC_DEVICE, NULL, 0);
        if (ret == 0) {
            ret = lsfs_mount(&sdmmc_mnt);
        }
    }

    if (ret != 0) {
        LOGE("Failed to mount filesystem: %d", ret);
        return ret;
    }
    LOGI("Mounted %s successfully\n", SDMMC_MOUNT_POINT);

    /* 3. 写入文件 */
    lsfs_file_t_init(&file);
    ret = lsfs_open(&file, TEST_FILE, LSFS_O_CREATE | LSFS_O_TRUNC | LSFS_O_WRITE);
    if (ret == 0) {
        bytes = lsfs_write(&file, write_data, strlen(write_data));
        LOGI("Write %d bytes to %s", (int)bytes, TEST_FILE);
        lsfs_close(&file);
    }

    /* 4. 读取文件 */
    lsfs_file_t_init(&file);
    ret = lsfs_open(&file, TEST_FILE, LSFS_O_READ);
    if (ret == 0) {
        bytes = lsfs_read(&file, read_buffer, sizeof(read_buffer) - 1);
        if (bytes > 0) {
            read_buffer[bytes] = '\0';
            LOGI("Read %d bytes: %s", (int)bytes, read_buffer);
        }
        lsfs_close(&file);
    }

    /* 5. 遍历目录 */
    LOGI("Listing directory: %s/", SDMMC_MOUNT_POINT);
    lsfs_dir_t_init(&dir);
    ret = lsfs_opendir(&dir, SDMMC_MOUNT_POINT"/");
    if (ret == 0) {
        while (1) {
            ret = lsfs_readdir(&dir, &entry);
            if (ret != 0 || entry.name[0] == 0)
                break;

            if (entry.type == LSFS_DIR_ENTRY_DIR) {
                LOGI("  <DIR>  %s", entry.name);
            } else {
                LOGI("  %10lu  %s", entry.size, entry.name);
            }
        }
        lsfs_closedir(&dir);
    }

    /* 6. 卸载文件系统 */
    lsfs_unmount(&sdmmc_mnt);
    LOGI("Unmounted %s", SDMMC_MOUNT_POINT);

    return 0;
}

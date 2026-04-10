#define LOG_TAG "sample"
#include <lisa_log.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include "lsfs.h"
#include "lvfs.h"
#include "disk/disk_access.h"
#include "lisa_sdmmc.h"


#define SDMMC_DEVICE      "SD:"
#define SDMMC_MOUNT_POINT "/"SDMMC_DEVICE
#define TEST_FILE_FD      SDMMC_MOUNT_POINT"/test_fd.txt"
#define TEST_FILE_STREAM  SDMMC_MOUNT_POINT"/test_stream.txt"

static struct lsfs_mount_t sdmmc_mnt = {
    .type = LSFS_FATFS,
    .mnt_point = SDMMC_MOUNT_POINT,
    .fs_data = NULL,
};

int main(int argc, char **argv)
{
    int ret;
    const char *test_data = "Hello, POSIX!";

    LOGI("=== LVFS POSIX Sample ===\n");

    /* 1. 初始化 SD 卡和磁盘子系统 */
    lisa_sdmmc_probe(lisa_device_get("sdmmc0"));
    disk_init(NULL);

    /* 2. 初始化 LVFS 和 LSFS */
    lvfs_init();
    lsfs_init();

    /* 3. 挂载文件系统 */
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

    /* ===== 4. 文件描述符接口演示 ===== */
    LOGI("--- File Descriptor API Demo ---");

    int fd;
    char read_buffer[64];
    ssize_t bytes;

    /* 打开并写入文件 */
    fd = open(TEST_FILE_FD, O_CREAT | O_RDWR | O_TRUNC, 0666);
    if (fd >= 0) {
        bytes = write(fd, test_data, strlen(test_data));
        LOGI("write() wrote %d bytes", (int)bytes);

        /* 定位到文件开头并读取 */
        lseek(fd, 0, SEEK_SET);
        bytes = read(fd, read_buffer, sizeof(read_buffer) - 1);
        if (bytes > 0) {
            read_buffer[bytes] = '\0';
            LOGI("read() read %d bytes: %s", (int)bytes, read_buffer);
        }

        /* 同步到磁盘 */
        fsync(fd);
        LOGI("fsync() completed");

        /* 截断文件 */
        ftruncate(fd, 5);
        LOGI("ftruncate() to 5 bytes");

        close(fd);
        LOGI("close() completed\n");
    }

#if CONFIG_LVFS_POSIX_API_ALIAS
    /* ===== 5. 标准 I/O 流接口演示 ===== */
    LOGI("--- Standard I/O Stream API Demo ---");

    FILE *fp;

    /* 打开并写入文件流 */
    fp = fopen(TEST_FILE_STREAM, "w+");
    if (fp != NULL) {
        size_t items = fwrite(test_data, 1, strlen(test_data), fp);
        LOGI("fwrite() wrote %d items", (int)items);

        /* 刷新缓冲区 */
        fflush(fp);
        LOGI("fflush() completed");

        /* 定位到文件开头并读取 */
        fseek(fp, 0, SEEK_SET);
        items = fread(read_buffer, 1, sizeof(read_buffer) - 1, fp);
        if (items > 0) {
            read_buffer[items] = '\0';
            LOGI("fread() read %d items: %s", (int)items, read_buffer);
        }

        fclose(fp);
        LOGI("fclose() completed\n");
    }
#endif

    /* 6. 卸载文件系统 */
    lsfs_unmount(&sdmmc_mnt);
    LOGI("Unmounted %s", SDMMC_MOUNT_POINT);

    return 0;
}

#include <stdbool.h>
#include <string.h>

#include "IOMuxManager.h"
#include "disk/disk_access.h"
#include "lisa_device.h"
#include "lisa_sdmmc.h"
#include "lsfs.h"
#include "log_print.h"

#define FLASHDISK_DEVICE      "NAND:"
#define FLASHDISK_MOUNT_POINT "/" FLASHDISK_DEVICE
#define FLASH_BENCH_ROOT      FLASHDISK_MOUNT_POINT "/adb_bench"

#define SDMMC_DEVICE          "SD:"
#define SDMMC_MOUNT_POINT     "/" SDMMC_DEVICE
#define SD_BENCH_ROOT         SDMMC_MOUNT_POINT "/adb_bench"

#define SDMMC_CLK_PAD         CSK_IOMUX_PAD_A
#define SDMMC_CLK_PIN         6
#define SDMMC_CMD_PAD         CSK_IOMUX_PAD_A
#define SDMMC_CMD_PIN         7
#define SDMMC_DAT0_PAD        CSK_IOMUX_PAD_A
#define SDMMC_DAT0_PIN        5
#define SDMMC_DAT1_PAD        CSK_IOMUX_PAD_A
#define SDMMC_DAT1_PIN        4
#define SDMMC_DAT2_PAD        CSK_IOMUX_PAD_A
#define SDMMC_DAT2_PIN        9
#define SDMMC_DAT3_PAD        CSK_IOMUX_PAD_A
#define SDMMC_DAT3_PIN        8
#define SDMMC_FUNC            CSK_IOMUX_FUNC_ALTER15

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

#ifdef CONFIG_BOARD_ARCS_EVB
void lisa_sdio_pinmux(void)
{
    IOMuxManager_PinConfigure(SDMMC_CLK_PAD, SDMMC_CLK_PIN, SDMMC_FUNC);
    IOMuxManager_PinConfigure(SDMMC_CMD_PAD, SDMMC_CMD_PIN, SDMMC_FUNC);
    IOMuxManager_PinConfigure(SDMMC_DAT0_PAD, SDMMC_DAT0_PIN, SDMMC_FUNC);
    IOMuxManager_PinConfigure(SDMMC_DAT1_PAD, SDMMC_DAT1_PIN, SDMMC_FUNC);
    IOMuxManager_PinConfigure(SDMMC_DAT2_PAD, SDMMC_DAT2_PIN, SDMMC_FUNC);
    IOMuxManager_PinConfigure(SDMMC_DAT3_PAD, SDMMC_DAT3_PIN, SDMMC_FUNC);
}
#endif

static int fs_mount(struct lsfs_mount_t *mp)
{
    int ret = lsfs_mount(mp);

    if (ret != 0) {
        LOGW("mount %s failed ret=%d, try mkfs", mp->mnt_point, ret);
        ret = lsfs_mkfs(mp->type, &mp->mnt_point[1], NULL, 0);
        if (ret == 0) {
            ret = lsfs_mount(mp);
        }
    }

    if (ret != 0) {
        LOGE("mount %s failed ret=%d", mp->mnt_point, ret);
        return ret;
    }

    LOGI("%s mounted", mp->mnt_point);
    return 0;
}

static int ensure_dir(const char *path)
{
    struct lsfs_dirent entry = {0};
    int ret = lsfs_stat(path, &entry);

    if (ret == 0) {
        if (entry.type == LSFS_DIR_ENTRY_DIR) {
            return 0;
        }

        LOGE("path exists but is not a directory: %s", path);
        return -1;
    }

    ret = lsfs_mkdir(path);
    if (ret != 0) {
        LOGE("mkdir %s failed ret=%d", path, ret);
        return ret;
    }

    return 0;
}

static bool prepare_flash_target(void)
{
    if (fs_mount(&flash_lsfs_mnt) != 0) {
        LOGE("adb benchmark: flash target mount failed");
        return false;
    }

    if (ensure_dir(FLASH_BENCH_ROOT) != 0) {
        LOGE("adb benchmark: flash target root create failed");
        return false;
    }

    LOGI("adb benchmark: flash ready at /NAND:/adb_bench/");
    return true;
}

static bool prepare_sd_target(void)
{
    lisa_device_t *sdmmc = lisa_device_get("sdmmc0");
    int ret;

    if (sdmmc == NULL) {
        LOGW("adb benchmark: sd not ready (device lookup failed)");
        return false;
    }

    ret = lisa_sdmmc_probe(sdmmc);
    if (ret != LISA_DEVICE_OK) {
        LOGW("adb benchmark: sd not ready (probe ret=%d)", ret);
        return false;
    }

    if (lisa_sdmmc_status(sdmmc) != LISA_SDMMC_STATUS_OK) {
        LOGW("adb benchmark: sd not ready (status=%d)", lisa_sdmmc_status(sdmmc));
        return false;
    }

    if (fs_mount(&sdmmc_lsfs_mnt) != 0) {
        LOGW("adb benchmark: sd not ready (mount failed)");
        return false;
    }

    if (ensure_dir(SD_BENCH_ROOT) != 0) {
        LOGW("adb benchmark: sd not ready (root create failed)");
        return false;
    }

    LOGI("adb benchmark: sd ready at /SD:/adb_bench/");
    return true;
}

int adb_benchmark_fs_init(void)
{
    bool flash_ready;

    disk_init(NULL);
    lsfs_init();

    flash_ready = prepare_flash_target();
    (void)prepare_sd_target();

    return flash_ready ? 0 : -1;
}

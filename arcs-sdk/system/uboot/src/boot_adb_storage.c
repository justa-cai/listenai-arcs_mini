#include "boot_adb_storage.h"

#include <stddef.h>
#include <string.h>

boot_adb_storage_result_t boot_adb_storage_prepare(const boot_adb_storage_ops_t *ops)
{
    boot_adb_storage_path_state_t state;

    if (ops == NULL || ops->default_root == NULL || ops->sdmmc_init == NULL ||
        ops->disk_init == NULL || ops->lsfs_init == NULL || ops->mount_sd == NULL ||
        ops->root_state == NULL || ops->mkdir == NULL) {
        return BOOT_ADB_STORAGE_RESULT_INVALID_OPS;
    }

    if (strcmp(ops->default_root, BOOT_ADB_STORAGE_DEFAULT_ROOT) != 0) {
        return BOOT_ADB_STORAGE_RESULT_INVALID_ROOT;
    }

    if (ops->sdmmc_init() != 0) {
        return BOOT_ADB_STORAGE_RESULT_SDMMC_INIT_FAILED;
    }

    if (ops->disk_init(NULL) != 0) {
        return BOOT_ADB_STORAGE_RESULT_DISK_INIT_FAILED;
    }

    if (ops->lsfs_init() != 0) {
        return BOOT_ADB_STORAGE_RESULT_LSFS_INIT_FAILED;
    }

    if (ops->mount_sd() != 0) {
        return BOOT_ADB_STORAGE_RESULT_MOUNT_FAILED;
    }

    state = ops->root_state(ops->default_root);
    if (state == BOOT_ADB_STORAGE_PATH_DIR) {
        return BOOT_ADB_STORAGE_RESULT_READY;
    }

    if (state == BOOT_ADB_STORAGE_PATH_MISSING) {
        if (ops->mkdir(ops->default_root) != 0) {
            return BOOT_ADB_STORAGE_RESULT_ROOT_CREATE_FAILED;
        }
        state = ops->root_state(ops->default_root);
    }

    return state == BOOT_ADB_STORAGE_PATH_DIR ? BOOT_ADB_STORAGE_RESULT_READY
                                              : BOOT_ADB_STORAGE_RESULT_ROOT_VERIFY_FAILED;
}

const char *boot_adb_storage_result_string(boot_adb_storage_result_t result)
{
    switch (result) {
    case BOOT_ADB_STORAGE_RESULT_READY:
        return "ready";
    case BOOT_ADB_STORAGE_RESULT_INVALID_OPS:
        return "invalid-ops";
    case BOOT_ADB_STORAGE_RESULT_INVALID_ROOT:
        return "root-config";
    case BOOT_ADB_STORAGE_RESULT_SDMMC_INIT_FAILED:
        return "sdmmc-init";
    case BOOT_ADB_STORAGE_RESULT_DISK_INIT_FAILED:
        return "disk-init";
    case BOOT_ADB_STORAGE_RESULT_LSFS_INIT_FAILED:
        return "lsfs-init";
    case BOOT_ADB_STORAGE_RESULT_MOUNT_FAILED:
        return "mount";
    case BOOT_ADB_STORAGE_RESULT_ROOT_CREATE_FAILED:
        return "root-create";
    case BOOT_ADB_STORAGE_RESULT_ROOT_VERIFY_FAILED:
        return "root-verify";
    default:
        return "unknown";
    }
}

#ifndef BOOT_ADB_STORAGE_UNIT_TEST

#if CONFIG_ADB_SYNC && defined(CONFIG_BOOT_ADB_SDMMC_FS)
#include "lisa_device.h"
#include "lisa_sdmmc.h"
#include "lsfs.h"

extern int disk_init(const void *dev);
extern int lsfs_init(void);
extern int boot_watchdog_feed(void);

static struct lsfs_mount_t boot_adb_storage_sd_mount = {
    .type = LSFS_FATFS,
    .mnt_point = BOOT_ADB_STORAGE_MOUNT_POINT,
    .fs_data = NULL,
};

static int boot_adb_storage_sdmmc_init_wrapper(void)
{
    lisa_device_t *sdmmc = lisa_device_get("sdmmc0");
    lisa_device_stats_t stats = {0};
    int ret;

    printf("boot adb: sdmmc lookup start\n");
    if (sdmmc == NULL) {
        printf("boot adb: sdmmc device lookup failed\n");
        return -1;
    }

    printf("boot adb: sdmmc lookup ok dev=%p state=%d\n",
           sdmmc,
           (int)lisa_device_get_state(sdmmc));
    printf("boot adb: lisa_sdmmc_probe start\n");
    boot_watchdog_feed();
    ret = lisa_sdmmc_probe(sdmmc);
    lisa_device_get_stats(sdmmc, &stats);
    printf("boot adb: lisa_sdmmc_probe done ret=%d state=%d init_ret=%d refs=%u\n",
           ret,
           (int)lisa_device_get_state(sdmmc),
           stats.init_result,
           stats.ref_count);

    if (ret != 0) {
        return ret;
    }

    return 0;
}

static int boot_adb_storage_disk_init_wrapper(const void *dev)
{
    return disk_init(dev);
}

static int boot_adb_storage_lsfs_init_wrapper(void)
{
    return lsfs_init();
}

static int boot_adb_storage_mount_sd_wrapper(void)
{
    int ret;

    boot_watchdog_feed();
    ret = lsfs_mount(&boot_adb_storage_sd_mount);

    if (ret == 0) {
        return 0;
    }

    printf("boot adb: mount failed ret=%d, try mkfs dev=%s\n",
           ret,
           &boot_adb_storage_sd_mount.mnt_point[1]);

    boot_watchdog_feed();
    ret = lsfs_mkfs(boot_adb_storage_sd_mount.type,
                    &boot_adb_storage_sd_mount.mnt_point[1],
                    NULL,
                    0);
    if (ret != 0) {
        printf("boot adb: mkfs failed ret=%d dev=%s\n",
               ret,
               &boot_adb_storage_sd_mount.mnt_point[1]);
        return ret;
    }

    printf("boot adb: mkfs ok dev=%s, retry mount\n",
           &boot_adb_storage_sd_mount.mnt_point[1]);
    boot_watchdog_feed();
    ret = lsfs_mount(&boot_adb_storage_sd_mount);
    return ret;
}

static boot_adb_storage_path_state_t boot_adb_storage_root_state_wrapper(const char *path)
{
    struct lsfs_dirent entry = {0};

    if (path == NULL) {
        return BOOT_ADB_STORAGE_PATH_ERROR;
    }

    if (lsfs_stat(path, &entry) != 0) {
        return BOOT_ADB_STORAGE_PATH_MISSING;
    }

    return entry.type == LSFS_DIR_ENTRY_DIR ? BOOT_ADB_STORAGE_PATH_DIR
                                            : BOOT_ADB_STORAGE_PATH_OTHER;
}

static int boot_adb_storage_mkdir_wrapper(const char *path)
{
    return lsfs_mkdir(path);
}
#endif

boot_adb_storage_result_t boot_adb_storage_prepare_default(void)
{
#if CONFIG_ADB_SYNC && defined(CONFIG_BOOT_ADB_SDMMC_FS)
    static const boot_adb_storage_ops_t ops = {
        .default_root = BOOT_ADB_STORAGE_DEFAULT_ROOT,
        .sdmmc_init = boot_adb_storage_sdmmc_init_wrapper,
        .disk_init = boot_adb_storage_disk_init_wrapper,
        .lsfs_init = boot_adb_storage_lsfs_init_wrapper,
        .mount_sd = boot_adb_storage_mount_sd_wrapper,
        .root_state = boot_adb_storage_root_state_wrapper,
        .mkdir = boot_adb_storage_mkdir_wrapper,
    };

    return boot_adb_storage_prepare(&ops);
#else
    return BOOT_ADB_STORAGE_RESULT_INVALID_OPS;
#endif
}

#endif

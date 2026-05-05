#ifndef __BOOT_ADB_STORAGE_H__
#define __BOOT_ADB_STORAGE_H__

#include <stdint.h>

#define BOOT_ADB_STORAGE_MOUNT_POINT "/SD:"

#ifndef CONFIG_BOOT_ADB_SDMMC_FS_ROOT
#define CONFIG_BOOT_ADB_SDMMC_FS_ROOT "/SD:/adb/"
#endif

#define BOOT_ADB_STORAGE_DEFAULT_ROOT CONFIG_BOOT_ADB_SDMMC_FS_ROOT

typedef enum {
    BOOT_ADB_STORAGE_PATH_ERROR = -1,
    BOOT_ADB_STORAGE_PATH_MISSING = 0,
    BOOT_ADB_STORAGE_PATH_DIR = 1,
    BOOT_ADB_STORAGE_PATH_OTHER = 2,
} boot_adb_storage_path_state_t;

typedef enum {
    BOOT_ADB_STORAGE_RESULT_READY = 0,
    BOOT_ADB_STORAGE_RESULT_INVALID_OPS = -1,
    BOOT_ADB_STORAGE_RESULT_INVALID_ROOT = -2,
    BOOT_ADB_STORAGE_RESULT_SDMMC_INIT_FAILED = -3,
    BOOT_ADB_STORAGE_RESULT_DISK_INIT_FAILED = -4,
    BOOT_ADB_STORAGE_RESULT_LSFS_INIT_FAILED = -5,
    BOOT_ADB_STORAGE_RESULT_MOUNT_FAILED = -6,
    BOOT_ADB_STORAGE_RESULT_ROOT_CREATE_FAILED = -7,
    BOOT_ADB_STORAGE_RESULT_ROOT_VERIFY_FAILED = -8,
} boot_adb_storage_result_t;

typedef struct {
    const char *default_root;
    int (*sdmmc_init)(void);
    int (*disk_init)(const void *dev);
    int (*lsfs_init)(void);
    int (*mount_sd)(void);
    boot_adb_storage_path_state_t (*root_state)(const char *path);
    int (*mkdir)(const char *path);
} boot_adb_storage_ops_t;

boot_adb_storage_result_t boot_adb_storage_prepare(const boot_adb_storage_ops_t *ops);
const char *boot_adb_storage_result_string(boot_adb_storage_result_t result);

#ifndef BOOT_ADB_STORAGE_UNIT_TEST
boot_adb_storage_result_t boot_adb_storage_prepare_default(void);
#endif

#endif /* __BOOT_ADB_STORAGE_H__ */

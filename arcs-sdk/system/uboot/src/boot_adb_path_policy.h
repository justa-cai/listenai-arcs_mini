#ifndef __BOOT_ADB_PATH_POLICY_H__
#define __BOOT_ADB_PATH_POLICY_H__

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

static inline bool boot_adb_path_policy_fs_enabled(void)
{
#if defined(CONFIG_BOOT_ADB_SDMMC_FS)
    return true;
#else
    return false;
#endif
}

static inline bool boot_adb_path_policy_is_raw_path(const char *path)
{
    static const char raw_prefix[] = "/RAW/";

    return path != NULL && strncmp(path, raw_prefix, sizeof(raw_prefix) - 1U) == 0;
}

static inline bool boot_adb_path_policy_is_allowed(const char *path, bool fs_enabled)
{
    if (path == NULL || path[0] == '\0') {
        return false;
    }

    if (path[0] != '/') {
        return fs_enabled;
    }

    if (boot_adb_path_policy_is_raw_path(path)) {
        return true;
    }

    return fs_enabled;
}

static inline bool boot_adb_path_policy_should_prefix_default_root(const char *path, bool fs_enabled)
{
    return path != NULL && path[0] != '/' && fs_enabled;
}

#endif

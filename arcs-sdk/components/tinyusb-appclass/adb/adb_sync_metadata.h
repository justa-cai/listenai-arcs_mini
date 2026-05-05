#ifndef __ADB_SYNC_METADATA_H__
#define __ADB_SYNC_METADATA_H__

#include <stdint.h>

#ifndef CONFIG_ADB_PUSH_PULL_DEFAULT_ROOT
#define CONFIG_ADB_PUSH_PULL_DEFAULT_ROOT "/RAM:/adb/"
#endif

#define ADB_SYNC_METADATA_ROOT CONFIG_ADB_PUSH_PULL_DEFAULT_ROOT
#define ADB_SYNC_METADATA_SIDECAR_PATH CONFIG_ADB_PUSH_PULL_DEFAULT_ROOT ".adb_sync_metadata"

enum adb_sync_metadata_status {
    ADB_SYNC_METADATA_OK = 0,
    ADB_SYNC_METADATA_NOT_FOUND = 1,
    ADB_SYNC_METADATA_BYPASS = 2,
    ADB_SYNC_METADATA_INVALID_PATH = -1,
    ADB_SYNC_METADATA_IO_ERROR = -2,
    ADB_SYNC_METADATA_MALFORMED_RECORD = -3,
    ADB_SYNC_METADATA_NO_MEMORY = -4,
};

int adb_sync_metadata_get_timestamp(const char *path, uint32_t *timestamp);
int adb_sync_metadata_record_timestamp(const char *path, uint32_t timestamp);
int adb_sync_metadata_build_full_path(const char *path, char **full_path);
const char *adb_sync_metadata_status_string(int status);

#endif

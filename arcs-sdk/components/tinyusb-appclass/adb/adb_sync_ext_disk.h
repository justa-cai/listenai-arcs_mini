#ifndef __ADB_SYNC_EXT_DISK_H__
#define __ADB_SYNC_EXT_DISK_H__

#include "stdint.h"

#ifndef ADB_SYNC_EXT_MOUNT_POINT
#define ADB_SYNC_EXT_MOUNT_POINT "/RAW/"
#endif

struct adb_sync_ext_disk_ctx {
    const char *name;
    uint32_t curr_sec;
    uint32_t sec_size;
    uint32_t buf_idx;
    uint8_t *buf;
};

struct adb_sync_ext_disk_ctx *adb_sync_ext_disk_ctx_init(const char *name, uint32_t start_addr);
void adb_sync_ext_disk_ctx_free(struct adb_sync_ext_disk_ctx *ctx);
int adb_sync_ext_disk_write(struct adb_sync_ext_disk_ctx *ctx, const uint8_t *data, uint32_t len);
bool adb_sync_is_ext_disk_access(const char *path);
int adb_sync_ext_disk_get_info_by_path(char *path, char **name, uint32_t *addr, uint32_t *size);
int adb_sync_ext_disk_read(struct adb_sync_ext_disk_ctx *ctx, uint8_t *data, uint32_t len);
#endif

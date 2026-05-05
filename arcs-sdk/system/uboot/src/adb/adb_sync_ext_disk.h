#ifndef __ADB_SYNC_EXT_DISK_H__
#define __ADB_SYNC_EXT_DISK_H__

#include <stdbool.h>
#include <stdint.h>

#ifndef ADB_SYNC_EXT_MOUNT_POINT
#define ADB_SYNC_EXT_MOUNT_POINT "/RAW/"
#endif

enum {
    ADB_SYNC_EXT_ERR_NONE = 0,
    ADB_SYNC_EXT_ERR_UNKNOWN,
    ADB_SYNC_EXT_ERR_NOMEM,
    ADB_SYNC_EXT_ERR_INVALID_PARAM,
    ADB_SYNC_EXT_ERR_INVALID_PATH,
    ADB_SYNC_EXT_ERR_INVALID_ADDR,
    ADB_SYNC_EXT_ERR_INVALID_SIZE,
    ADB_SYNC_EXT_ERR_DISK_ACCESS_DENIED,

    ADB_SYNC_EXT_ERR_DISK_INIT_FAILED,
    ADB_SYNC_EXT_ERR_DISK_WRITE,
    ADB_SYNC_EXT_ERR_DISK_READ,
};

struct adb_sync_ext_disk_ctx {
    const char *name;
    uint64_t start_addr;
    uint64_t transferred_size;
    uint64_t erased_size;
    uint64_t curr_sec;
    uint32_t sec_size;
    uint32_t buf_idx;
    uint32_t buf_size;
    uint32_t flash_erase_size;
    uint32_t direct_flash_begin_tick;
    uint32_t direct_flash_erase_ticks;
    uint32_t direct_flash_write_ticks;
    bool direct_flash;
    bool direct_flash_session_active;
    bool buf_from_large_chunk;
    bool buf_from_psram;
    uint8_t *buf;
};

struct adb_sync_raw_flash_stats {
    uint64_t bytes;
    uint32_t total_ms;
    uint32_t erase_ms;
    uint32_t write_ms;
};

int adb_sync_ext_disk_ctx_init(const char *name,
                               uint64_t start_addr,
                               uint64_t size,
                               bool write,
                               struct adb_sync_ext_disk_ctx **out_ctx);
void adb_sync_ext_disk_ctx_free(struct adb_sync_ext_disk_ctx *ctx);
int adb_sync_ext_disk_write(struct adb_sync_ext_disk_ctx *ctx, const uint8_t *data, uint32_t len);
bool adb_sync_is_ext_disk_access(const char *path);
int adb_sync_ext_disk_get_info_by_path(char *path, char **name, uint64_t *addr, uint64_t *size);
int adb_sync_ext_disk_read(struct adb_sync_ext_disk_ctx *ctx, uint8_t *data, uint32_t len);
void adb_sync_ext_disk_write_start(const char *name, uint64_t addr, uint64_t size);
void adb_sync_ext_disk_write_end(const char *name, uint64_t addr, uint64_t size);
void adb_sync_raw_flash_stats_get(struct adb_sync_raw_flash_stats *stats);

#endif

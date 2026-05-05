#ifndef __ADB_SYNC_EXT_DISK_H__
#define __ADB_SYNC_EXT_DISK_H__

#include <stdbool.h>
#include <stdint.h>

#ifndef ADB_SYNC_EXT_MOUNT_POINT
#define ADB_SYNC_EXT_MOUNT_POINT "/RAW/"
#endif

typedef struct {
    const void *buff;
    uint32_t sector_count;
} sdc_sector_buffer_t;

struct adb_sync_ext_disk_ctx {
    const char *name;
    uint64_t start_addr;
    uint64_t total_size;
    uint64_t transferred_size;
    uint64_t erased_size;
    uint64_t curr_sec;
    uint32_t sec_size;
    uint32_t buf_idx;
    uint32_t buf_size;
    uint32_t direct_flash_begin_tick;
    uint32_t direct_flash_erase_ticks;
    uint32_t direct_flash_write_ticks;
    bool is_write;
    bool write_started;
    bool direct_flash;
    bool direct_flash_session_active;
    bool buf_from_psram;
    bool buf_is_shared;
    uint8_t *buf;
    uint8_t *align_buf;
    uint32_t align_buf_size;
};

struct adb_sync_raw_flash_stats {
    uint64_t bytes;
    uint32_t total_ms;
    uint32_t erase_ms;
    uint32_t write_ms;
};

struct adb_sync_ext_disk_ctx *adb_sync_ext_disk_ctx_init(const char *name, uint64_t start_addr, uint64_t size,
                                                         bool write);
void adb_sync_ext_disk_set_align_buf(struct adb_sync_ext_disk_ctx *ctx, uint8_t *buf, uint32_t size);
void adb_sync_ext_disk_ctx_free(struct adb_sync_ext_disk_ctx *ctx);
int adb_sync_ext_disk_write(struct adb_sync_ext_disk_ctx *ctx, const uint8_t *data, uint32_t len);
int adb_sync_ext_disk_writev(struct adb_sync_ext_disk_ctx *ctx, const void *buffers, uint32_t buffer_count);
bool adb_sync_is_ext_disk_access(const char *path);
int adb_sync_ext_disk_get_info_by_path(char *path, char **name, uint64_t *addr, uint64_t *size);
int adb_sync_ext_disk_read(struct adb_sync_ext_disk_ctx *ctx, uint8_t *data, uint32_t len);
void adb_sync_raw_flash_stats_get(struct adb_sync_raw_flash_stats *stats);

#endif

#ifndef __ADB_SYNC_EXT_DISK_H__
#define __ADB_SYNC_EXT_DISK_H__

#include <stdbool.h>
#include <stdint.h>

#ifndef ADB_SYNC_EXT_MOUNT_POINT
#define ADB_SYNC_EXT_MOUNT_POINT "/RAW/"
#endif

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

/* Policy injection: callers (e.g. boot/recovery) install a vtable to gate
 * which (name, addr, size, write) tuples ADB sync may touch and to receive
 * write lifecycle hooks. Pass NULL ops, or leave individual callbacks NULL,
 * to fall back to defaults: can_access -> true, write_start -> 0,
 * write_done -> no-op.
 *
 * Lifetime / threading contract:
 *   - The pointer is stored as-is (no deep copy). The pointee MUST outlive
 *     every ADB sync transfer that may consult the policy — typically a
 *     `static const` object with program lifetime.
 *   - Intended to be called once from a single-threaded init path before
 *     ADB sync starts serving (see boot_adb_runtime_start() for the boot
 *     side). There is no internal locking; do not race set_policy() with
 *     in-flight transfers, and do not swap or NULL-out ops while a
 *     transfer is mid-flight. */
struct adb_sync_ext_disk_policy_ops {
    bool (*can_access)(const char *name, uint64_t addr, uint64_t size, bool write);
    int (*write_start)(const char *name, uint64_t addr, uint64_t size);
    void (*write_done)(const char *name, uint64_t addr, uint64_t size);
};

void adb_sync_ext_disk_set_policy(const struct adb_sync_ext_disk_policy_ops *ops);

/*
 * 长操作协作 hooks（与磁盘访问策略 policy_ops 正交）。
 *
 *   - policy_ops 决定哪些 (name, addr, size, write) 元组可写、写入前后
 *     做什么 lifecycle —— 是"授权与策略"层面的注入；
 *   - runtime_ops 是组件在长块 erase/write 过程中给 caller 的协作通知，
 *     caller 可借此喂自家看门狗、检查取消、上报进度等 —— 是"运行时横切
 *     关注点"。
 *
 * 组件本身不假设 caller 拿这些 hook 做什么。字段单独 NULL 即等价于该
 * hook 不感兴趣；整个 ops 指针 NULL 也安全（fall back 到全 NULL）。
 *
 * 与 set_policy 同款 lifetime 约定：单线程 init 路径一次性设置，不要在
 * in-flight transfer 中替换。
 */
struct adb_sync_ext_disk_runtime_ops {
    /* 长块 erase/write 每完成一个单位（一次 erase sector / 一次 chunk
     * write）触发一次。典型用途：喂自家 wdt。 */
    void (*yield)(void);
};

void adb_sync_ext_disk_set_runtime_ops(const struct adb_sync_ext_disk_runtime_ops *ops);

struct adb_sync_ext_disk_ctx *adb_sync_ext_disk_ctx_init(const char *name, uint64_t start_addr, uint64_t size,
                                                         bool write);
void adb_sync_ext_disk_set_align_buf(struct adb_sync_ext_disk_ctx *ctx, uint8_t *buf, uint32_t size);
void adb_sync_ext_disk_ctx_free(struct adb_sync_ext_disk_ctx *ctx);
int adb_sync_ext_disk_write(struct adb_sync_ext_disk_ctx *ctx, const uint8_t *data, uint32_t len);
bool adb_sync_is_ext_disk_access(const char *path);
int adb_sync_ext_disk_get_info_by_path(char *path, char **name, uint64_t *addr, uint64_t *size);
int adb_sync_ext_disk_read(struct adb_sync_ext_disk_ctx *ctx, uint8_t *data, uint32_t len);
void adb_sync_raw_flash_stats_get(struct adb_sync_raw_flash_stats *stats);

#endif

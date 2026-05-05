#include "disk/disk_access.h"
#include "adb_utils.h"
#include "adb_sync_ext_disk.h"
#include "boot_flash.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "boot_mem_chunk.h"

#if defined(CONFIG_BOOT_ADB_SDMMC_RAW)
#include "lisa_device.h"
#include "lisa_sdmmc.h"
#endif

#define BOOT_FLASH_BASE_ADDR          0x30000000ULL
#define BOOT_ADB_RAW_FLASH_ERASE_SIZE (64U * 1024U)
#define BOOT_ADB_RAW_FLASH_STAGE_SIZE (64U * 1024U)

static struct adb_sync_raw_flash_stats g_adb_sync_raw_flash_stats;

static bool adb_sync_ext_disk_is_sdmmc_raw(const char *name)
{
    return name != NULL && strcmp(name, "SDRAW") == 0;
}

__attribute__((weak)) bool disk_device_can_access(const char *name, uint64_t addr)
{
    return true;
}

__attribute__((weak)) bool adb_sync_ext_disk_sdmmc_raw_enabled(void)
{
#if defined(CONFIG_BOOT_ADB_SDMMC_RAW)
    return true;
#else
    return false;
#endif
}

__attribute__((weak)) int adb_sync_ext_disk_prepare_sdmmc_raw(void)
{
#if defined(CONFIG_BOOT_ADB_SDMMC_RAW)
    lisa_device_t *sdmmc = lisa_device_get("sdmmc0");

    if (sdmmc == NULL) {
        return -1;
    }

    return lisa_sdmmc_probe(sdmmc);
#else
    return -1;
#endif
}

__attribute__((weak)) void disk_device_write_done(const char *name, uint64_t addr, uint64_t size)
{
    return;
}

__attribute__((weak)) void disk_device_write_start(const char *name, uint64_t addr, uint64_t size)
{
    return;
}

void adb_sync_ext_disk_write_end(const char *name, uint64_t addr, uint64_t size)
{
    disk_device_write_done(name, addr, size);
}

void adb_sync_ext_disk_write_start(const char *name, uint64_t addr, uint64_t size)
{
    disk_device_write_start(name, addr, size);
}

#ifdef CONFIG_MODULE_FREERTOS
static uint32_t adb_sync_ext_disk_ticks_now(void)
{
    return (uint32_t)xTaskGetTickCount();
}
#else
static uint32_t adb_sync_ext_disk_ticks_now(void)
{
    return 0;
}
#endif

static bool adb_sync_ext_disk_is_raw_flash(const char *name)
{
    return (strcmp(name, "NAND") == 0) || (strcmp(name, "FLASH") == 0);
}

static bool adb_sync_ext_disk_can_direct_flash(const char *name, uint64_t start_addr, uint64_t size, bool write)
{
    if (!write || !adb_sync_ext_disk_is_raw_flash(name) || size == 0) {
        return false;
    }

    return ((start_addr % BOOT_ADB_RAW_FLASH_ERASE_SIZE) == 0) &&
           ((size % BOOT_ADB_RAW_FLASH_ERASE_SIZE) == 0);
}

static uint8_t *adb_sync_ext_disk_direct_flash_ptr(uint64_t flash_addr)
{
    return (uint8_t *)(uintptr_t)(BOOT_FLASH_BASE_ADDR + flash_addr);
}

static bool adb_sync_ext_disk_direct_flash_region_is_blank(uint64_t flash_addr, uint32_t len)
{
    const uint8_t *p = adb_sync_ext_disk_direct_flash_ptr(flash_addr);

    while (len > 0) {
        if (*p != 0xFF) {
            return false;
        }
        p++;
        len--;
    }

    return true;
}

static int adb_sync_ext_disk_direct_flash_ensure_erased(struct adb_sync_ext_disk_ctx *ctx,
                                                        uint64_t flash_addr,
                                                        uint32_t len)
{
    uint64_t write_offset;
    uint64_t required_size;
    uint64_t erase_size;
    uint32_t begin_tick;
    int r;

    if (!ctx->direct_flash) {
        return 0;
    }

    if (flash_addr < ctx->start_addr) {
        return ADB_SYNC_EXT_ERR_DISK_WRITE;
    }

    write_offset = flash_addr - ctx->start_addr;
    required_size = write_offset + len;
    if (ctx->erased_size >= required_size) {
        return 0;
    }

    required_size = ((required_size + ctx->flash_erase_size - 1) / ctx->flash_erase_size) * ctx->flash_erase_size;
    erase_size = required_size - ctx->erased_size;
    if (erase_size == 0) {
        return 0;
    }

    if (adb_sync_ext_disk_direct_flash_region_is_blank(ctx->start_addr + ctx->erased_size, (uint32_t)erase_size)) {
        ctx->erased_size += erase_size;
        return 0;
    }

    begin_tick = adb_sync_ext_disk_ticks_now();
    r = boot_flash_erase(adb_sync_ext_disk_direct_flash_ptr(ctx->start_addr + ctx->erased_size),
                         (uint32_t)erase_size);
    if (r) {
        ADB_LOGE("adb sync direct flash erase failed, addr:0x%08x size:0x%08x err:%d\n",
                 (uint32_t)(ctx->start_addr + ctx->erased_size), (uint32_t)erase_size, r);
        return ADB_SYNC_EXT_ERR_DISK_WRITE;
    }

    ctx->erased_size += erase_size;
    ctx->direct_flash_erase_ticks += adb_sync_ext_disk_ticks_now() - begin_tick;
    return 0;
}

static int adb_sync_ext_disk_direct_flash_program(struct adb_sync_ext_disk_ctx *ctx,
                                                  const uint8_t *data,
                                                  uint32_t len)
{
    uint64_t flash_addr;
    uint32_t begin_tick;
    int r;

    if (!ctx->direct_flash || data == NULL || len == 0) {
        return 0;
    }

    flash_addr = ctx->start_addr + ctx->transferred_size;
    r = adb_sync_ext_disk_direct_flash_ensure_erased(ctx, flash_addr, len);
    if (r) {
        return r;
    }

    begin_tick = adb_sync_ext_disk_ticks_now();
    r = boot_flash_write(adb_sync_ext_disk_direct_flash_ptr(flash_addr), (uint8_t *)data, len);
    if (r) {
        ADB_LOGE("adb sync direct flash write failed, addr:0x%08x size:0x%08x err:%d\n",
                 (uint32_t)flash_addr, len, r);
        return ADB_SYNC_EXT_ERR_DISK_WRITE;
    }

    ctx->direct_flash_write_ticks += adb_sync_ext_disk_ticks_now() - begin_tick;
    ctx->transferred_size += len;
    ctx->curr_sec = (ctx->start_addr + ctx->transferred_size) / ctx->sec_size;
    return 0;
}

int adb_sync_ext_disk_ctx_init(const char *name,
                               uint64_t start_addr,
                               uint64_t size,
                               bool write,
                               struct adb_sync_ext_disk_ctx **out_ctx)
{
    struct adb_sync_ext_disk_ctx *ctx;
    int r;

    if (name == NULL || out_ctx == NULL) {
        ADB_LOGE("adb sync ext disk init error, name is null\n");
        return ADB_SYNC_EXT_ERR_INVALID_PATH;
    }
    if (size == 0) {
        ADB_LOGE("adb sync ext disk init error, size is zero\n");
        return ADB_SYNC_EXT_ERR_INVALID_SIZE;
    }

    if (adb_sync_ext_disk_is_sdmmc_raw(name)) {
        if (!adb_sync_ext_disk_sdmmc_raw_enabled()) {
            ADB_LOGE("adb sync ext disk sdmmc raw disabled, name:%s\n", name);
            return ADB_SYNC_EXT_ERR_DISK_ACCESS_DENIED;
        }

        r = adb_sync_ext_disk_prepare_sdmmc_raw();
        if (r != 0) {
            ADB_LOGE("adb sync ext disk sdmmc raw prepare failed, name:%s, err:%d\n", name, r);
            return ADB_SYNC_EXT_ERR_DISK_ACCESS_DENIED;
        }
    }

    if (!disk_device_can_access(name, start_addr)) {
        ADB_LOGE("disk device can not access, name:%s, addr:0x%llx\n", name, (unsigned long long)start_addr);
        return ADB_SYNC_EXT_ERR_DISK_ACCESS_DENIED;
    }

    ctx = ADB_MALLOC(sizeof(struct adb_sync_ext_disk_ctx));
    if (ctx == NULL) {
        ADB_LOGE("adb sync ext disk context malloc error");
        return ADB_SYNC_EXT_ERR_NOMEM;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->name = name;
    ctx->start_addr = start_addr;
    ctx->flash_erase_size = BOOT_ADB_RAW_FLASH_ERASE_SIZE;
    ctx->direct_flash = adb_sync_ext_disk_can_direct_flash(name, start_addr, size, write);

    if (ctx->direct_flash) {
        ctx->sec_size = 512;
        ctx->buf_size = BOOT_ADB_RAW_FLASH_STAGE_SIZE;
        ctx->buf = inram_malloc(32, ctx->buf_size);
        if (ctx->buf == NULL) {
            ctx->buf = boot_mem_large_chunk_get((uint32_t)-1);
            if (ctx->buf != NULL) {
                ctx->buf_from_large_chunk = true;
                ctx->buf_size = (MEM_CHUNK_SIZE / ctx->flash_erase_size) * ctx->flash_erase_size;
            }
        }
        if (ctx->buf == NULL) {
            ctx->buf = psram_malloc_align(32, ctx->buf_size);
            if (ctx->buf != NULL) {
                ctx->buf_from_psram = true;
            }
        }
    } else {
        r = disk_access_init(ctx->name);
        if (r) {
            ADB_LOGE("disk access init error, disk name:%s\n", ctx->name);
            ADB_FREE(ctx);
            return ADB_SYNC_EXT_ERR_DISK_INIT_FAILED;
        }

        r = disk_access_ioctl(ctx->name, DISK_IOCTL_GET_SECTOR_SIZE, &ctx->sec_size);
        if (r) {
            ADB_LOGE("get disk sector size error, disk name:%s\n", ctx->name);
            ADB_FREE(ctx);
            return ADB_SYNC_EXT_ERR_DISK_INIT_FAILED;
        }

        ctx->buf_size = MEM_CHUNK_SIZE;
        ctx->buf = boot_mem_large_chunk_get((uint32_t)-1);
        if (ctx->buf != NULL) {
            ctx->buf_from_large_chunk = true;
        }
    }

    if (ctx->buf == NULL) {
        ADB_LOGE("adb sync ext disk buffer malloc error");
        ADB_FREE(ctx);
        return ADB_SYNC_EXT_ERR_NOMEM;
    }

    if (ctx->buf_size == 0) {
        ADB_LOGE("adb sync ext disk invalid buffer size");
        if (ctx->buf_from_large_chunk) {
            boot_mem_large_chunk_put(ctx->buf);
        } else if (ctx->buf_from_psram) {
            psram_free(ctx->buf);
        } else {
            inram_free(ctx->buf);
        }
        ADB_FREE(ctx);
        return ADB_SYNC_EXT_ERR_NOMEM;
    }

    if (start_addr % ctx->sec_size != 0) {
        ADB_LOGE("start_addr is not align with sec_size, start_addr:0x%llx, sec_size:%d\n",
                 (unsigned long long)start_addr, ctx->sec_size);
        if (ctx->buf_from_large_chunk) {
            boot_mem_large_chunk_put(ctx->buf);
        } else if (ctx->buf_from_psram) {
            psram_free(ctx->buf);
        } else {
            inram_free(ctx->buf);
        }
        ADB_FREE(ctx);
        return ADB_SYNC_EXT_ERR_INVALID_ADDR;
    }

    memset(ctx->buf, 0, ctx->buf_size);
    ctx->curr_sec = start_addr / ctx->sec_size;
    ctx->direct_flash_begin_tick = adb_sync_ext_disk_ticks_now();

    if (ctx->direct_flash) {
        boot_flash_session_begin();
        ctx->direct_flash_session_active = true;
    }

    *out_ctx = ctx;

    ADB_LOGI("adb sync ext disk init, name:%s, sec_size:%d, start_sec:%u, start addr:%llu, direct_flash:%d\n",
             name, ctx->sec_size, (uint32_t)ctx->curr_sec, start_addr, ctx->direct_flash);

    return 0;
}

static int adb_sync_ext_disk_flush(struct adb_sync_ext_disk_ctx *ctx, bool force)
{
    uint32_t flush_len;
    uint32_t sector_count;
    int r;

    if (ctx == NULL || ctx->buf_idx == 0) {
        return 0;
    }

    if (ctx->direct_flash) {
        flush_len = force ? ctx->buf_idx : (ctx->buf_idx / ctx->flash_erase_size) * ctx->flash_erase_size;
        if (flush_len == 0) {
            return 0;
        }

        r = adb_sync_ext_disk_direct_flash_program(ctx, ctx->buf, flush_len);
        if (r) {
            return r;
        }

        ctx->buf_idx -= flush_len;
        if (ctx->buf_idx != 0) {
            memmove(ctx->buf, ctx->buf + flush_len, ctx->buf_idx);
        }
        return 0;
    }

    flush_len = (ctx->buf_idx / ctx->sec_size) * ctx->sec_size;
    if (flush_len == 0) {
        memset(ctx->buf + ctx->buf_idx, 0, ctx->sec_size - ctx->buf_idx);
        flush_len = ctx->sec_size;
    }

    sector_count = flush_len / ctx->sec_size;
    r = disk_access_write(ctx->name, ctx->buf, ctx->curr_sec, sector_count);
    if (r) {
        return ADB_SYNC_EXT_ERR_DISK_WRITE;
    }

    ctx->curr_sec += sector_count;
    ctx->transferred_size += flush_len;
    ctx->buf_idx -= flush_len;
    if (ctx->buf_idx != 0) {
        memmove(ctx->buf, ctx->buf + flush_len, ctx->buf_idx);
    }

    return 0;
}

void adb_sync_ext_disk_ctx_free(struct adb_sync_ext_disk_ctx *ctx)
{
    if (ctx == NULL) {
        return;
    }

    (void)adb_sync_ext_disk_flush(ctx, true);

    if (ctx->direct_flash_session_active) {
        boot_flash_session_end();
        ctx->direct_flash_session_active = false;
    }

    if (ctx->direct_flash) {
#ifdef CONFIG_MODULE_FREERTOS
        uint32_t total_ticks = adb_sync_ext_disk_ticks_now() - ctx->direct_flash_begin_tick;
        g_adb_sync_raw_flash_stats.bytes = ctx->transferred_size;
        g_adb_sync_raw_flash_stats.total_ms = (uint32_t)pdTICKS_TO_MS(total_ticks);
        g_adb_sync_raw_flash_stats.erase_ms = (uint32_t)pdTICKS_TO_MS(ctx->direct_flash_erase_ticks);
        g_adb_sync_raw_flash_stats.write_ms = (uint32_t)pdTICKS_TO_MS(ctx->direct_flash_write_ticks);

        ADB_LOGI("adb raw flash stats, total:%u ms, erase:%u ms, write:%u ms, bytes:%llu\n",
                 (unsigned)pdTICKS_TO_MS(total_ticks),
                 (unsigned)pdTICKS_TO_MS(ctx->direct_flash_erase_ticks),
                 (unsigned)pdTICKS_TO_MS(ctx->direct_flash_write_ticks),
                 ctx->transferred_size);
#else
        g_adb_sync_raw_flash_stats.bytes = ctx->transferred_size;
        g_adb_sync_raw_flash_stats.total_ms = 0;
        g_adb_sync_raw_flash_stats.erase_ms = 0;
        g_adb_sync_raw_flash_stats.write_ms = 0;
        ADB_LOGI("adb raw flash stats, bytes:%llu\n", ctx->transferred_size);
#endif
    }

    if (ctx->buf_from_large_chunk) {
        boot_mem_large_chunk_put(ctx->buf);
    } else if (ctx->buf_from_psram) {
        psram_free(ctx->buf);
    } else {
        inram_free(ctx->buf);
    }

    ADB_FREE(ctx);
}

void adb_sync_raw_flash_stats_get(struct adb_sync_raw_flash_stats *stats)
{
    if (stats == NULL) {
        return;
    }

    *stats = g_adb_sync_raw_flash_stats;
}

int adb_sync_ext_disk_write(struct adb_sync_ext_disk_ctx *ctx, const uint8_t *data, uint32_t len)
{
    extern int boot_watchdog_feed(void);

    if (ctx == NULL || data == NULL || len == 0) {
        return ADB_SYNC_EXT_ERR_INVALID_PARAM;
    }

    boot_watchdog_feed();

    while (len > 0) {
        uint32_t copy_len;
        uint32_t remaining;
        int r;

        if (ctx->direct_flash && ctx->buf_idx == 0 && len >= ctx->flash_erase_size) {
            uint32_t direct_len = (len / ctx->flash_erase_size) * ctx->flash_erase_size;

            r = adb_sync_ext_disk_direct_flash_program(ctx, data, direct_len);
            if (r) {
                return r;
            }

            data += direct_len;
            len -= direct_len;
            continue;
        }

        if (ctx->buf_idx == ctx->buf_size) {
            r = adb_sync_ext_disk_flush(ctx, false);
            if (r) {
                return r;
            }
        }

        remaining = ctx->buf_size - ctx->buf_idx;
        copy_len = len < remaining ? len : remaining;
        memcpy(ctx->buf + ctx->buf_idx, data, copy_len);
        ctx->buf_idx += copy_len;
        data += copy_len;
        len -= copy_len;

        if (ctx->direct_flash) {
            if (ctx->buf_idx == ctx->buf_size) {
                r = adb_sync_ext_disk_flush(ctx, false);
                if (r) {
                    return r;
                }
            }
        } else if (ctx->buf_idx >= ctx->sec_size) {
            r = adb_sync_ext_disk_flush(ctx, false);
            if (r) {
                return r;
            }
        }
    }

    return 0;
}

int adb_sync_ext_disk_read(struct adb_sync_ext_disk_ctx *ctx, uint8_t *data, uint32_t len)
{
    int r;
    int cnt;

    if (ctx == NULL || data == NULL || len == 0) {
        ADB_LOGE("adb sync ext disk read error, ctx:%p, data:%p, len:%d\n", ctx, data, len);
        return ADB_SYNC_EXT_ERR_INVALID_PARAM;
    }

    cnt = (len / ctx->sec_size) + ((len % ctx->sec_size) ? 1 : 0);
    r = disk_access_read(ctx->name, data, ctx->curr_sec, cnt);
    if (r) {
        return ADB_SYNC_EXT_ERR_DISK_READ;
    }

    ctx->curr_sec += cnt;
    return len;
}

bool adb_sync_is_ext_disk_access(const char *path)
{
    return strncmp(path, ADB_SYNC_EXT_MOUNT_POINT, strlen(ADB_SYNC_EXT_MOUNT_POINT)) == 0;
}

int adb_sync_ext_disk_get_info_by_path(char *path, char **name, uint64_t *addr, uint64_t *size)
{
    char *p = strstr(path, ADB_SYNC_EXT_MOUNT_POINT);
    char *saveptr;
    char *token;

    if (p != path) {
        return -1;
    }

    p += strlen(ADB_SYNC_EXT_MOUNT_POINT);
    *size = 0;
    *addr = 0;

    token = strtok_r(p, "/", &saveptr);
    if (token == NULL) {
        return -1;
    }
    *name = token;

    token = strtok_r(NULL, "/", &saveptr);
    if (token == NULL) {
        return -1;
    }

    *addr = strtoull(token, NULL, 16);
    ADB_LOGI("ext disk init, addr: %llu\n", *addr);

    token = strtok_r(NULL, "/", &saveptr);
    if (token == NULL) {
        return -1;
    }

    *size = strtoull(token, NULL, 16);
    ADB_LOGI("ext disk init, size: %llu\n", *size);

    return 0;
}

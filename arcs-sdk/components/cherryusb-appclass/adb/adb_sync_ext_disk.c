#include "disk/disk_access.h"
#include "adb_utils.h"
#include "adb_sync_ext_disk.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef CONFIG_MODULE_FREERTOS
#include "FreeRTOS.h"
#include "task.h"
#endif

#ifdef CONFIG_MODULE_FREERTOS
static uint32_t adb_sync_ext_disk_ticks_now(void)
{
    return (uint32_t)xTaskGetTickCount();
}

static uint32_t adb_sync_ext_disk_ticks_to_ms(uint32_t ticks)
{
    return (uint32_t)pdTICKS_TO_MS(ticks);
}
#else
static uint32_t adb_sync_ext_disk_ticks_now(void)
{
    return 0;
}

static uint32_t adb_sync_ext_disk_ticks_to_ms(uint32_t ticks)
{
    (void)ticks;
    return 0;
}
#endif

#ifdef CONFIG_BOOT_ADB
#include "boot_flash.h"
#include "cache.h"
#include "lib_sdc.h"
#if defined(CONFIG_BOOT_ADB_SDMMC_RAW)
#include "lisa_device.h"
#include "lisa_sdmmc.h"
#endif
#if defined(CONFIG_PSRAM_HEAP) && CONFIG_PSRAM_HEAP
#include "esp_heap_caps.h"
#endif

#define BOOT_FLASH_BASE_ADDR          0x30000000ULL
#define BOOT_FLASH_SECTOR_SIZE        512U
#define BOOT_FLASH_ERASE_SIZE         (64U * 1024U)
#define BOOT_FLASH_STAGE_SIZE         BOOT_FLASH_ERASE_SIZE
#define ADB_SYNC_EXT_DISK_SD_PORT     SD_0

#ifndef CONFIG_LISA_SDMMC_ACCESS_BUFFER_ALIGN_SIZE
#define CONFIG_LISA_SDMMC_ACCESS_BUFFER_ALIGN_SIZE 64U
#endif

static uint8_t *boot_flash_stage_buf;
static bool boot_flash_stage_buf_from_psram;
static struct adb_sync_raw_flash_stats g_adb_sync_raw_flash_stats;

#if defined(CONFIG_PSRAM_HEAP) && CONFIG_PSRAM_HEAP
static bool adb_sync_ext_disk_boot_flash_can_alloc_internal(uint32_t capacity, uint32_t alignment)
{
    size_t required_internal_block = (size_t)capacity + (size_t)alignment;
    size_t largest_internal_block = heap_caps_get_largest_free_block(
        MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL);

    return largest_internal_block >= required_internal_block;
}
#endif

static uint8_t *adb_sync_ext_disk_boot_flash_stage_buf_get(bool *from_psram)
{
    if (boot_flash_stage_buf == NULL) {
#if defined(CONFIG_PSRAM_HEAP) && CONFIG_PSRAM_HEAP
        if (adb_sync_ext_disk_boot_flash_can_alloc_internal(BOOT_FLASH_STAGE_SIZE, 32U)) {
            boot_flash_stage_buf = inram_malloc(32, BOOT_FLASH_STAGE_SIZE);
        }
#else
        boot_flash_stage_buf = inram_malloc(32, BOOT_FLASH_STAGE_SIZE);
#endif
#if defined(CONFIG_PSRAM_HEAP) && CONFIG_PSRAM_HEAP
        if (boot_flash_stage_buf == NULL) {
            boot_flash_stage_buf = psram_malloc_align(32, BOOT_FLASH_STAGE_SIZE);
            if (boot_flash_stage_buf != NULL) {
                boot_flash_stage_buf_from_psram = true;
            }
        }
#endif
    }

    if (from_psram != NULL) {
        *from_psram = boot_flash_stage_buf_from_psram;
    }

    return boot_flash_stage_buf;
}

static void adb_sync_ext_disk_boot_flash_stage_buf_put(void)
{
    if (boot_flash_stage_buf == NULL) {
        return;
    }

#if defined(CONFIG_PSRAM_HEAP) && CONFIG_PSRAM_HEAP
    if (boot_flash_stage_buf_from_psram) {
        psram_free(boot_flash_stage_buf);
    } else
#endif
    {
        inram_free(boot_flash_stage_buf);
    }

    boot_flash_stage_buf = NULL;
    boot_flash_stage_buf_from_psram = false;
}

#endif

/* Policy ops registered by the caller (NULL = no caller has registered yet,
 * see adb_sync_ext_disk.h for default semantics). */
static const struct adb_sync_ext_disk_policy_ops *g_policy_ops;

void adb_sync_ext_disk_set_policy(const struct adb_sync_ext_disk_policy_ops *ops)
{
    g_policy_ops = ops;
}

/* Runtime hooks 与 policy 正交，单独 setter。 */
static const struct adb_sync_ext_disk_runtime_ops *g_runtime_ops;

void adb_sync_ext_disk_set_runtime_ops(const struct adb_sync_ext_disk_runtime_ops *ops)
{
    g_runtime_ops = ops;
}

/* 长块 erase/write 中的协作 yield 点：组件不反向依赖任何 wdt 实现，
 * 由 caller 通过 runtime_ops->yield 注入；未注册即 no-op。 */
static void adb_sync_ext_disk_yield(void)
{
    if (g_runtime_ops != NULL && g_runtime_ops->yield != NULL) {
        g_runtime_ops->yield();
    }
}

static bool adb_sync_ext_disk_policy_can_access(const char *name, uint64_t addr, uint64_t size, bool write)
{
    if (g_policy_ops != NULL && g_policy_ops->can_access != NULL) {
        return g_policy_ops->can_access(name, addr, size, write);
    }
    return true;
}

static int adb_sync_ext_disk_policy_write_start(const char *name, uint64_t addr, uint64_t size)
{
    if (g_policy_ops != NULL && g_policy_ops->write_start != NULL) {
        return g_policy_ops->write_start(name, addr, size);
    }
    return 0;
}

static void adb_sync_ext_disk_policy_write_done(const char *name, uint64_t addr, uint64_t size)
{
    if (g_policy_ops != NULL && g_policy_ops->write_done != NULL) {
        g_policy_ops->write_done(name, addr, size);
    }
}

static bool adb_sync_ext_disk_is_sdmmc_raw(const char *name)
{
    return name != NULL && strcmp(name, "SDRAW") == 0;
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

static uint64_t adb_sync_ext_disk_final_size(const struct adb_sync_ext_disk_ctx *ctx)
{
    if (ctx == NULL) {
        return 0;
    }

    if (ctx->total_size != 0U) {
        return ctx->total_size;
    }

    return ctx->transferred_size + ctx->buf_idx;
}

static void adb_sync_ext_disk_finish_write(struct adb_sync_ext_disk_ctx *ctx)
{
    uint64_t final_size;

    if (ctx != NULL && ctx->write_started) {
        final_size = adb_sync_ext_disk_final_size(ctx);
        ctx->total_size = final_size;
        adb_sync_ext_disk_policy_write_done(ctx->name, ctx->start_addr, final_size);
        ctx->write_started = false;
    }
}

#ifdef CONFIG_BOOT_ADB
static bool adb_sync_ext_disk_is_boot_raw_flash(const char *name)
{
    return strcmp(name, "NAND") == 0 || strcmp(name, "FLASH") == 0;
}

static bool adb_sync_ext_disk_use_boot_flash_fast_path(const char *name, uint64_t addr, uint64_t size, bool write)
{
    (void)size;

    if (!write || !adb_sync_ext_disk_is_boot_raw_flash(name)) {
        return false;
    }

    return (addr % BOOT_FLASH_ERASE_SIZE) == 0;
}

static bool adb_sync_ext_disk_use_boot_flash_read_path(const char *name, bool write)
{
    return !write && adb_sync_ext_disk_is_boot_raw_flash(name);
}

static uint8_t *adb_sync_ext_disk_boot_flash_ptr(uint64_t flash_addr)
{
    return (uint8_t *)(uintptr_t)(BOOT_FLASH_BASE_ADDR + flash_addr);
}

static int adb_sync_ext_disk_boot_flash_ensure_erased(struct adb_sync_ext_disk_ctx *ctx, uint64_t flash_addr,
                                                      uint32_t len)
{
    uint64_t required_size;
    uint32_t erase_len;
    uint32_t begin_tick;
    int r;

    if (ctx == NULL || !ctx->direct_flash) {
        return 0;
    }

    required_size = (flash_addr - ctx->start_addr) + len;
    if (ctx->erased_size >= required_size) {
        return 0;
    }

    erase_len = (uint32_t)(required_size - ctx->erased_size);
    begin_tick = adb_sync_ext_disk_ticks_now();
    adb_sync_ext_disk_yield();
    r = boot_flash_erase(adb_sync_ext_disk_boot_flash_ptr(ctx->start_addr + ctx->erased_size), erase_len);
    adb_sync_ext_disk_yield();
    if (r) {
        ADB_LOGE("boot raw flash erase failed, addr:0x%08x size:0x%08x err:%d\n",
                 (uint32_t)(ctx->start_addr + ctx->erased_size), erase_len, r);
        return r;
    }

    ctx->erased_size += erase_len;
    ctx->direct_flash_erase_ticks += adb_sync_ext_disk_ticks_now() - begin_tick;
    return 0;
}

static int adb_sync_ext_disk_boot_flash_program(struct adb_sync_ext_disk_ctx *ctx, const uint8_t *data, uint32_t len)
{
    uint64_t flash_addr;
    uint32_t begin_tick;
    int r;

    if (ctx == NULL || data == NULL || len == 0U) {
        return 0;
    }

    flash_addr = ctx->start_addr + ctx->transferred_size;
    r = adb_sync_ext_disk_boot_flash_ensure_erased(ctx, flash_addr, len);
    if (r) {
        return r;
    }

    begin_tick = adb_sync_ext_disk_ticks_now();
    adb_sync_ext_disk_yield();
    r = boot_flash_write(adb_sync_ext_disk_boot_flash_ptr(flash_addr), (uint8_t *)data, len);
    adb_sync_ext_disk_yield();
    if (r) {
        ADB_LOGE("boot raw flash write failed, addr:0x%08x size:0x%08x err:%d\n",
                 (uint32_t)flash_addr, len, r);
        return r;
    }

    ctx->direct_flash_write_ticks += adb_sync_ext_disk_ticks_now() - begin_tick;
    ctx->transferred_size += len;
    ctx->curr_sec = (ctx->start_addr + ctx->transferred_size) / ctx->sec_size;
    return 0;
}

static int adb_sync_ext_disk_boot_flash_read(struct adb_sync_ext_disk_ctx *ctx, uint8_t *data, uint32_t len)
{
    uint8_t *flash_ptr;

    if (ctx == NULL || data == NULL || len == 0U) {
        return -1;
    }

    flash_ptr = adb_sync_ext_disk_boot_flash_ptr(ctx->start_addr + ctx->transferred_size);
    HAL_InvalidateDCache_by_Addr((uint32_t *)flash_ptr, len);
    memcpy(data, flash_ptr, len);

    ctx->transferred_size += len;
    ctx->curr_sec = (ctx->start_addr + ctx->transferred_size) / ctx->sec_size;
    return (int)len;
}

static int adb_sync_ext_disk_boot_flash_flush(struct adb_sync_ext_disk_ctx *ctx, bool force)
{
    uint32_t flush_len;

    if (ctx == NULL || !ctx->direct_flash || ctx->buf_idx == 0) {
        return 0;
    }

    flush_len = force ? ctx->buf_idx : (ctx->buf_idx / BOOT_FLASH_ERASE_SIZE) * BOOT_FLASH_ERASE_SIZE;
    if (flush_len == 0) {
        return 0;
    }

    if (adb_sync_ext_disk_boot_flash_program(ctx, ctx->buf, flush_len) != 0) {
        return -1;
    }

    ctx->buf_idx -= flush_len;
    if (ctx->buf_idx != 0U) {
        memmove(ctx->buf, ctx->buf + flush_len, ctx->buf_idx);
    }
    return 0;
}
#endif

static int adb_sync_ext_disk_parse_hex_u64(char *token, uint64_t *value)
{
    char *end = NULL;
    unsigned long long parsed;

    if (token == NULL || value == NULL || token[0] == '\0') {
        return -1;
    }

    parsed = strtoull(token, &end, 16);
    if (end == token || *end != '\0') {
        return -1;
    }

    *value = (uint64_t)parsed;
    return 0;
}

struct adb_sync_ext_disk_ctx * __attribute__((section(".psram.text")))
adb_sync_ext_disk_ctx_init(const char *name, uint64_t start_addr, uint64_t size,
                           bool write)
{
    int r;
    struct adb_sync_ext_disk_ctx *ctx = NULL;

    if (name == NULL || (!write && size == 0U)) {
        return NULL;
    }

    if (adb_sync_ext_disk_is_sdmmc_raw(name)) {
        if (!adb_sync_ext_disk_sdmmc_raw_enabled()) {
            ADB_LOGE("adb sync ext disk sdmmc raw disabled, name:%s\n", name);
            return NULL;
        }

        r = adb_sync_ext_disk_prepare_sdmmc_raw();
        if (r != 0) {
            ADB_LOGE("adb sync ext disk sdmmc raw prepare failed, name:%s, err:%d\n", name, r);
            return NULL;
        }
    }

    if (!adb_sync_ext_disk_policy_can_access(name, start_addr, size, write)) {
        ADB_LOGE("adb sync ext disk policy deny, name:%s, addr:0x%llx, size:0x%llx, write:%d\n", name,
                 (unsigned long long)start_addr, (unsigned long long)size, write);
        return NULL;
    }

    ctx = ADB_MALLOC(sizeof(struct adb_sync_ext_disk_ctx));
    if (ctx == NULL) {
        ADB_LOGE("adb sync ext disk context malloc error");
        return NULL;
    }

    memset(ctx, 0, sizeof(struct adb_sync_ext_disk_ctx));
    ctx->name = name;
    ctx->start_addr = start_addr;
    ctx->total_size = size;
    ctx->is_write = write;
#ifdef CONFIG_BOOT_ADB
    ctx->direct_flash = adb_sync_ext_disk_use_boot_flash_read_path(name, write) ||
                        adb_sync_ext_disk_use_boot_flash_fast_path(name, start_addr, size, write);
    if (ctx->direct_flash) {
        ctx->sec_size = BOOT_FLASH_SECTOR_SIZE;
    } else
#endif
    {
        r = disk_access_ioctl(ctx->name, DISK_IOCTL_GET_SECTOR_SIZE, &ctx->sec_size);
        if (r) {
            ADB_LOGE("get disk sector size error, disk name:%s\n", ctx->name);
            goto failed;
        }
    }

    if (ctx->sec_size == 0 || (start_addr % ctx->sec_size) != 0) {
        ADB_LOGE("start_addr is not align with sec_size, start_addr:0x%llx, sec_size:%u\n",
                 (unsigned long long)start_addr, ctx->sec_size);
        goto failed;
    }

#ifdef CONFIG_BOOT_ADB
    if (ctx->direct_flash) {
        if (write) {
            ctx->buf_size = BOOT_FLASH_STAGE_SIZE;
            ctx->buf = adb_sync_ext_disk_boot_flash_stage_buf_get(&ctx->buf_from_psram);
            ctx->buf_is_shared = (ctx->buf != NULL);
        } else {
            ctx->buf_size = ctx->sec_size;
            ctx->buf = ADB_MALLOC(ctx->buf_size);
        }
    } else
#endif
    {
        ctx->buf_size = ctx->sec_size;
        ctx->buf = ADB_MALLOC(ctx->buf_size);
    }
    if (ctx->buf == NULL) {
        ADB_LOGE("adb sync ext disk buffer malloc error");
        goto failed;
    }

    memset(ctx->buf, 0, ctx->buf_size);
    ctx->curr_sec = start_addr / ctx->sec_size;
    ctx->direct_flash_begin_tick = adb_sync_ext_disk_ticks_now();

    if (write) {
        r = adb_sync_ext_disk_policy_write_start(ctx->name, ctx->start_addr, ctx->total_size);
        if (r) {
            ADB_LOGE("adb sync ext disk write start failed, name:%s, error:%d\n", ctx->name, r);
            goto failed;
        }
        ctx->write_started = true;
    }

#ifdef CONFIG_BOOT_ADB
    if (ctx->direct_flash && write) {
        adb_sync_ext_disk_yield();
        boot_flash_session_begin();
        ctx->direct_flash_session_active = true;
        adb_sync_ext_disk_yield();
    }
#endif

    ADB_LOGI("adb sync ext disk init, name:%s, sec_size:%u, start_addr:0x%llx, size:0x%llx, write:%d\n", name,
             ctx->sec_size, (unsigned long long)start_addr, (unsigned long long)size, write);

    return ctx;

failed:
    adb_sync_ext_disk_finish_write(ctx);
    if (ctx != NULL) {
#ifdef CONFIG_BOOT_ADB
        if (ctx->buf_is_shared) {
            adb_sync_ext_disk_boot_flash_stage_buf_put();
            ctx->buf = NULL;
        } else
#endif
        if (ctx->buf_from_psram) {
            psram_free(ctx->buf);
        } else {
            ADB_FREE(ctx->buf);
        }
        ADB_FREE(ctx);
    }
    return NULL;
}

static int adb_sync_ext_disk_flush(struct adb_sync_ext_disk_ctx *ctx)
{
    int r;

    if (ctx->buf_idx == 0) {
        return 0;
    }

#ifdef CONFIG_BOOT_ADB
    if (ctx->direct_flash) {
        return adb_sync_ext_disk_boot_flash_flush(ctx, true);
    }
#endif

    adb_sync_ext_disk_yield();
    r = disk_access_write(ctx->name, ctx->buf, ctx->curr_sec, 1);
    adb_sync_ext_disk_yield();
    if (r) {
        return r;
    }

    ctx->curr_sec += 1;
    ctx->buf_idx = 0;

    return 0;
}

void __attribute__((section(".psram.text")))
adb_sync_ext_disk_ctx_free(struct adb_sync_ext_disk_ctx *ctx)
{
    if (ctx == NULL) {
        return;
    }

    if (ctx->is_write) {
        int r = adb_sync_ext_disk_flush(ctx);
        if (r) {
            ADB_LOGE("adb sync ext disk flush failed, name:%s, error:%d\n", ctx->name, r);
        }
        adb_sync_ext_disk_finish_write(ctx);
    }

#ifdef CONFIG_BOOT_ADB
    if (ctx->direct_flash) {
        if (ctx->direct_flash_session_active) {
            boot_flash_session_end();
            ctx->direct_flash_session_active = false;
        }

        uint32_t total_ticks = adb_sync_ext_disk_ticks_now() - ctx->direct_flash_begin_tick;

        g_adb_sync_raw_flash_stats.bytes = ctx->transferred_size;
        g_adb_sync_raw_flash_stats.total_ms = adb_sync_ext_disk_ticks_to_ms(total_ticks);
        g_adb_sync_raw_flash_stats.erase_ms = adb_sync_ext_disk_ticks_to_ms(ctx->direct_flash_erase_ticks);
        g_adb_sync_raw_flash_stats.write_ms = adb_sync_ext_disk_ticks_to_ms(ctx->direct_flash_write_ticks);
    }
#endif

#ifdef CONFIG_BOOT_ADB
    if (ctx->buf_is_shared) {
        adb_sync_ext_disk_boot_flash_stage_buf_put();
        ctx->buf = NULL;
    } else if (ctx->buf_from_psram) {
        psram_free(ctx->buf);
    } else
#endif
    {
        ADB_FREE(ctx->buf);
    }
    ADB_FREE(ctx);
}

void adb_sync_ext_disk_set_align_buf(struct adb_sync_ext_disk_ctx *ctx, uint8_t *buf, uint32_t size)
{
    if (ctx == NULL) {
        return;
    }

    ctx->align_buf = buf;
    ctx->align_buf_size = size;
}

static int adb_sync_ext_disk_write_align_start_sector(struct adb_sync_ext_disk_ctx *ctx, const uint8_t *data,
                                                      uint32_t len)
{
    int r;
    uint32_t sector_count;
    uint32_t tail_len;
    uint32_t write_len = 0U;
    const uint8_t *write_ptr = data;
#if defined(CONFIG_BOOT_ADB) && defined(CONFIG_BOOT_ADB_BACKEND_CHERRYUSB)
    uint8_t *shadow_buf = NULL;
#endif

    sector_count = len / ctx->sec_size;
    if (sector_count != 0) {
        write_len = sector_count * ctx->sec_size;
#if defined(CONFIG_BOOT_ADB) && defined(CONFIG_BOOT_ADB_BACKEND_CHERRYUSB)
        if (((uintptr_t)data % CONFIG_LISA_SDMMC_ACCESS_BUFFER_ALIGN_SIZE) != 0U) {
            shadow_buf = ctx->align_buf;
            if (shadow_buf != NULL && ctx->align_buf_size >= write_len) {
                memcpy(shadow_buf, data, write_len);
                write_ptr = shadow_buf;
            } else {
                shadow_buf = adb_boot_try_inram_malloc(CONFIG_LISA_SDMMC_ACCESS_BUFFER_ALIGN_SIZE, write_len);
                if (shadow_buf != NULL) {
                    memcpy(shadow_buf, data, write_len);
                    write_ptr = shadow_buf;
                }
            }
        }
#endif
        adb_sync_ext_disk_yield();
        r = disk_access_write(ctx->name, write_ptr, ctx->curr_sec, sector_count);
        adb_sync_ext_disk_yield();
#if defined(CONFIG_BOOT_ADB) && defined(CONFIG_BOOT_ADB_BACKEND_CHERRYUSB)
        if (shadow_buf != NULL && shadow_buf != ctx->align_buf) {
            inram_free(shadow_buf);
        }
#endif
        if (r) {
            return r;
        }
        ctx->curr_sec += sector_count;
        data += write_len;
    }

    tail_len = len % ctx->sec_size;
    if (tail_len != 0) {
        memcpy(ctx->buf, data, tail_len);
        ctx->buf_idx = tail_len;
    }

    return 0;
}

int adb_sync_ext_disk_write(struct adb_sync_ext_disk_ctx *ctx, const uint8_t *data, uint32_t len)
{
    int r = 0;
    uint32_t cpy_len;
    uint64_t pending_size;

    if (ctx == NULL || data == NULL || len == 0 || !ctx->is_write) {
        return -1;
    }

    pending_size = ctx->direct_flash ? ctx->buf_idx : 0U;
    if (ctx->total_size != 0U && (ctx->transferred_size + pending_size + len) > ctx->total_size) {
        ADB_LOGE("adb sync ext disk write overflow, transferred:0x%llx, len:0x%x, size:0x%llx\n",
                 (unsigned long long)ctx->transferred_size, len, (unsigned long long)ctx->total_size);
        return -1;
    }

#ifdef CONFIG_BOOT_ADB
    if (ctx->direct_flash) {
        adb_sync_ext_disk_yield();
        while (len != 0U) {
            if (ctx->buf_idx == 0U && len >= BOOT_FLASH_ERASE_SIZE) {
                uint32_t direct_len = (len / BOOT_FLASH_ERASE_SIZE) * BOOT_FLASH_ERASE_SIZE;

                r = adb_sync_ext_disk_boot_flash_program(ctx, data, direct_len);
                if (r) {
                    return r;
                }

                data += direct_len;
                len -= direct_len;
                continue;
            }

            cpy_len = len;
            if ((ctx->buf_idx + cpy_len) > ctx->buf_size) {
                cpy_len = ctx->buf_size - ctx->buf_idx;
            }

            memcpy(ctx->buf + ctx->buf_idx, data, cpy_len);
            ctx->buf_idx += cpy_len;
            data += cpy_len;
            len -= cpy_len;

            if (ctx->buf_idx == ctx->buf_size) {
                r = adb_sync_ext_disk_boot_flash_flush(ctx, false);
                if (r) {
                    return r;
                }
            }
        }

        return 0;
    }
#endif

    if (ctx->buf_idx == 0) {
        r = adb_sync_ext_disk_write_align_start_sector(ctx, data, len);
    } else {
        if (ctx->buf_idx == ctx->sec_size) {
            r = adb_sync_ext_disk_flush(ctx);
            if (r) {
                return r;
            }
        }

        cpy_len = len <= (ctx->sec_size - ctx->buf_idx) ? len : (ctx->sec_size - ctx->buf_idx);
        memcpy(ctx->buf + ctx->buf_idx, data, cpy_len);
        ctx->buf_idx += cpy_len;

        if (ctx->buf_idx == ctx->sec_size) {
            r = adb_sync_ext_disk_flush(ctx);
            if (r) {
                return r;
            }
        }

        r = adb_sync_ext_disk_write_align_start_sector(ctx, data + cpy_len, len - cpy_len);
    }

    if (r == 0) {
        ctx->transferred_size += len;
    }

    return r;
}

void adb_sync_raw_flash_stats_get(struct adb_sync_raw_flash_stats *stats)
{
    if (stats == NULL) {
        return;
    }

#ifdef CONFIG_BOOT_ADB
    *stats = g_adb_sync_raw_flash_stats;
#else
    memset(stats, 0, sizeof(*stats));
#endif
}

int __attribute__((section(".psram.text")))
adb_sync_ext_disk_read(struct adb_sync_ext_disk_ctx *ctx, uint8_t *data, uint32_t len)
{
    int r;
    uint32_t sector_count;

    if (ctx == NULL || data == NULL || len == 0 || ctx->is_write) {
        return -1;
    }

    if ((ctx->transferred_size + len) > ctx->total_size) {
        ADB_LOGE("adb sync ext disk read overflow, transferred:0x%llx, len:0x%x, size:0x%llx\n",
                 (unsigned long long)ctx->transferred_size, len, (unsigned long long)ctx->total_size);
        return -1;
    }

#ifdef CONFIG_BOOT_ADB
    if (ctx->direct_flash) {
        return adb_sync_ext_disk_boot_flash_read(ctx, data, len);
    }

    if (adb_sync_ext_disk_is_sdmmc_raw(ctx->name)) {
        sector_count = (len / ctx->sec_size) + ((len % ctx->sec_size) ? 1u : 0u);
        adb_sync_ext_disk_yield();
        r = (int)gm_sdc_api_sdcard_sector_read(ADB_SYNC_EXT_DISK_SD_PORT,
                                               (u32)ctx->curr_sec,
                                               sector_count,
                                               data);
        adb_sync_ext_disk_yield();
        if (r != 0) {
            return r;
        }

        ctx->curr_sec += sector_count;
        ctx->transferred_size += len;
        return (int)len;
    }
#endif

    sector_count = (len / ctx->sec_size) + ((len % ctx->sec_size) ? 1u : 0u);
    adb_sync_ext_disk_yield();
    r = disk_access_read(ctx->name, data, ctx->curr_sec, sector_count);
    adb_sync_ext_disk_yield();
    if (r) {
        return r;
    }

    ctx->curr_sec += sector_count;
    ctx->transferred_size += len;

    return (int)len;
}

bool adb_sync_is_ext_disk_access(const char *path)
{
    return strncmp(path, ADB_SYNC_EXT_MOUNT_POINT, strlen(ADB_SYNC_EXT_MOUNT_POINT)) == 0;
}

static char *adb_sync_ext_disk_take_segment(char **cursor)
{
    char *token;
    char *slash;

    if (cursor == NULL || *cursor == NULL || **cursor == '\0') {
        return NULL;
    }

    token = *cursor;
    slash = strchr(token, '/');
    if (slash != NULL) {
        *slash = '\0';
        *cursor = slash + 1;
    } else {
        *cursor = token + strlen(token);
    }

    return token;
}

int __attribute__((section(".psram.text")))
adb_sync_ext_disk_get_info_by_path(char *path, char **name, uint64_t *addr, uint64_t *size)
{
    char *cursor;
    char *token;

    if (path == NULL || name == NULL || addr == NULL || size == NULL ||
        strstr(path, ADB_SYNC_EXT_MOUNT_POINT) != path) {
        return -1;
    }

    cursor = path + strlen(ADB_SYNC_EXT_MOUNT_POINT);
    *addr = 0;
    *size = 0;

    token = adb_sync_ext_disk_take_segment(&cursor);
    if (token == NULL || token[0] == '\0') {
        return -1;
    }
    *name = token;

    token = adb_sync_ext_disk_take_segment(&cursor);
    if (adb_sync_ext_disk_parse_hex_u64(token, addr) != 0) {
        return -1;
    }

    token = adb_sync_ext_disk_take_segment(&cursor);
    if (token == NULL) {
        return 0;
    }

    if (adb_sync_ext_disk_parse_hex_u64(token, size) != 0 || *size == 0U) {
        return -1;
    }

    if (cursor != NULL && *cursor != '\0') {
        return -1;
    }

    return 0;
}

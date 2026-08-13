#include "unity.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "adb_sync_ext_disk.h"

#define TEST_FLASH_BASE_ADDR 0x30000000u
#define TEST_FLASH_STORAGE_SIZE (256u * 1024u)

static uint8_t g_boot_flash_storage[TEST_FLASH_STORAGE_SIZE];
static uint32_t g_boot_flash_erase_calls;
static uint32_t g_boot_flash_write_calls;
static uint32_t g_watchdog_feed_calls;
static uint32_t g_boot_flash_session_begin_calls;
static uint32_t g_boot_flash_session_end_calls;
static uint32_t g_dcache_invalidate_calls;
static uint8_t *g_boot_flash_view;

static void ensure_flash_view_mapped(void)
{
    void *mapped;

    if (g_boot_flash_view != NULL) {
        return;
    }

    mapped = mmap((void *)(uintptr_t)TEST_FLASH_BASE_ADDR, TEST_FLASH_STORAGE_SIZE,
                  PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    TEST_ASSERT_TRUE(mapped != MAP_FAILED);
    TEST_ASSERT_EQUAL_PTR((void *)(uintptr_t)TEST_FLASH_BASE_ADDR, mapped);
    g_boot_flash_view = (uint8_t *)mapped;
}

static void flash_view_sync(uint32_t offset, uint32_t size)
{
    TEST_ASSERT_NOT_NULL(g_boot_flash_view);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(sizeof(g_boot_flash_storage), offset + size);
    memcpy(g_boot_flash_view + offset, g_boot_flash_storage + offset, size);
}

void *exram_malloc(size_t align, size_t size)
{
    (void)align;
    return calloc(1, size);
}

void exram_free(void *ptr)
{
    free(ptr);
}

void *inram_malloc(size_t align, size_t size)
{
    (void)align;
    return calloc(1, size);
}

void inram_free(void *ptr)
{
    free(ptr);
}

void *psram_malloc_align(size_t align, size_t size)
{
    (void)align;
    return calloc(1, size);
}

void psram_free(void *ptr)
{
    free(ptr);
}

int printk(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    (void)vfprintf(stderr, fmt, args);
    va_end(args);
    return 0;
}

int boot_flash_erase(uint8_t *addr, uint32_t size)
{
    uintptr_t flash_addr = (uintptr_t)addr - TEST_FLASH_BASE_ADDR;

    if ((flash_addr + size) > sizeof(g_boot_flash_storage)) {
        return -1;
    }

    g_boot_flash_erase_calls++;
    memset(g_boot_flash_storage + flash_addr, 0xff, size);
    return 0;
}

int boot_flash_write(uint8_t *addr, uint8_t *data, uint32_t size)
{
    uintptr_t flash_addr = (uintptr_t)addr - TEST_FLASH_BASE_ADDR;

    if ((flash_addr + size) > sizeof(g_boot_flash_storage)) {
        return -1;
    }

    g_boot_flash_write_calls++;
    memcpy(g_boot_flash_storage + flash_addr, data, size);
    return 0;
}

static void test_runtime_yield(void)
{
    g_watchdog_feed_calls++;
}

static const struct adb_sync_ext_disk_runtime_ops test_runtime_ops = {
    .yield = test_runtime_yield,
};

void HAL_InvalidateDCache_by_Addr(uint32_t *addr, uint32_t dsize)
{
    uintptr_t flash_addr = (uintptr_t)addr - TEST_FLASH_BASE_ADDR;

    TEST_ASSERT_LESS_OR_EQUAL_UINT32(sizeof(g_boot_flash_storage), (uint32_t)flash_addr + dsize);
    g_dcache_invalidate_calls++;
    flash_view_sync((uint32_t)flash_addr, dsize);
}

void boot_flash_session_begin(void)
{
    g_boot_flash_session_begin_calls++;
}

void boot_flash_session_end(void)
{
    g_boot_flash_session_end_calls++;
}

int disk_access_ioctl(const char *name, uint8_t cmd, void *buff)
{
    (void)name;
    (void)cmd;
    (void)buff;
    return -1;
}

int disk_access_read(const char *name, uint8_t *data, uint64_t start_sector, uint32_t num_sector)
{
    (void)name;
    (void)data;
    (void)start_sector;
    (void)num_sector;
    return -1;
}

int disk_access_write(const char *name, const uint8_t *data, uint64_t start_sector, uint32_t num_sector)
{
    (void)name;
    (void)data;
    (void)start_sector;
    (void)num_sector;
    return -1;
}

void setUp(void)
{
    ensure_flash_view_mapped();
    memset(g_boot_flash_storage, 0xff, sizeof(g_boot_flash_storage));
    memset(g_boot_flash_view, 0xff, TEST_FLASH_STORAGE_SIZE);
    g_boot_flash_erase_calls = 0u;
    g_boot_flash_write_calls = 0u;
    g_watchdog_feed_calls = 0u;
    g_boot_flash_session_begin_calls = 0u;
    g_boot_flash_session_end_calls = 0u;
    g_dcache_invalidate_calls = 0u;
    adb_sync_ext_disk_set_runtime_ops(&test_runtime_ops);
}

void tearDown(void)
{
}

void test_raw_flash_fast_path_feeds_watchdog_on_large_write(void)
{
    struct adb_sync_ext_disk_ctx *ctx;
    uint8_t payload[128 * 1024];
    size_t i;

    for (i = 0; i < sizeof(payload); ++i) {
        payload[i] = (uint8_t)i;
    }

    ctx = adb_sync_ext_disk_ctx_init("FLASH", 0x10000ULL, sizeof(payload), true);
    TEST_ASSERT_NOT_NULL(ctx);

    TEST_ASSERT_EQUAL_INT(0, adb_sync_ext_disk_write(ctx, payload, sizeof(payload)));
    adb_sync_ext_disk_ctx_free(ctx);

    TEST_ASSERT_TRUE(g_boot_flash_erase_calls > 0u);
    TEST_ASSERT_TRUE(g_boot_flash_write_calls > 0u);
    TEST_ASSERT_TRUE(g_watchdog_feed_calls > 0u);
    TEST_ASSERT_EQUAL_UINT32(1u, g_boot_flash_session_begin_calls);
    TEST_ASSERT_EQUAL_UINT32(1u, g_boot_flash_session_end_calls);
    TEST_ASSERT_EQUAL_MEMORY(payload, g_boot_flash_storage + 0x10000u, sizeof(payload));
}

void test_raw_flash_stream_flushes_on_each_erase_block(void)
{
    struct adb_sync_ext_disk_ctx *ctx;
    uint8_t payload[128 * 1024];
    size_t offset;

    for (offset = 0; offset < sizeof(payload); ++offset) {
        payload[offset] = (uint8_t)(offset >> 2);
    }

    ctx = adb_sync_ext_disk_ctx_init("FLASH", 0x10000ULL, sizeof(payload), true);
    TEST_ASSERT_NOT_NULL(ctx);

    for (offset = 0; offset < sizeof(payload); offset += 32 * 1024) {
        TEST_ASSERT_EQUAL_INT(0, adb_sync_ext_disk_write(ctx, payload + offset, 32 * 1024));
    }
    adb_sync_ext_disk_ctx_free(ctx);

    TEST_ASSERT_EQUAL_UINT32(2u, g_boot_flash_erase_calls);
    TEST_ASSERT_EQUAL_UINT32(2u, g_boot_flash_write_calls);
    TEST_ASSERT_EQUAL_MEMORY(payload, g_boot_flash_storage + 0x10000u, sizeof(payload));
}

void test_raw_flash_read_invalidates_cache_before_copy(void)
{
    struct adb_sync_ext_disk_ctx *ctx;
    uint8_t expected[4096];
    uint8_t actual[4096];

    memset(expected, 0x3cu, sizeof(expected));
    memcpy(g_boot_flash_storage + 0x10000u, expected, sizeof(expected));
    memset(g_boot_flash_view + 0x10000u, 0xa5, sizeof(expected));

    ctx = adb_sync_ext_disk_ctx_init("FLASH", 0x10000ULL, sizeof(expected), false);
    TEST_ASSERT_NOT_NULL(ctx);

    TEST_ASSERT_EQUAL_INT((int)sizeof(expected), adb_sync_ext_disk_read(ctx, actual, sizeof(actual)));

    TEST_ASSERT_EQUAL_UINT32(1u, g_dcache_invalidate_calls);
    TEST_ASSERT_EQUAL_MEMORY(expected, actual, sizeof(expected));
    adb_sync_ext_disk_ctx_free(ctx);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_raw_flash_fast_path_feeds_watchdog_on_large_write);
    RUN_TEST(test_raw_flash_stream_flushes_on_each_erase_block);
    RUN_TEST(test_raw_flash_read_invalidates_cache_before_copy);
    return UNITY_END();
}

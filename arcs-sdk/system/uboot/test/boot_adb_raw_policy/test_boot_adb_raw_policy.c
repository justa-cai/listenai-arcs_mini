#include "unity.h"

#ifdef RUN_TEST
#undef RUN_TEST
#endif
#define RUN_TEST(TestFunc, TestLineNum) UnityDefaultTestRun(TestFunc, #TestFunc, TestLineNum)

#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "boot_mem_chunk.h"
#include "disk/disk_access.h"
#include "adb_sync_ext_disk.h"

#ifdef TEST_ASSERT_EQUAL_INT
#undef TEST_ASSERT_EQUAL_INT
#endif
#define TEST_ASSERT_EQUAL_INT(expected, actual) TEST_ASSERT_EQUAL_UINT32((uint32_t)(expected), (uint32_t)(actual))

static int (*const g_parse_path)(char *, char **, uint64_t *, uint64_t *) =
    adb_sync_ext_disk_get_info_by_path;

static uint32_t g_sector_size;
static uint8_t g_disk_storage[4096];
static uint64_t g_last_read_sector;
static uint64_t g_last_write_sector;
static uint32_t g_last_write_count;
static uint32_t g_access_calls;
static uint32_t g_ioctl_calls;
static uint32_t g_read_calls;
static uint32_t g_write_calls;
static uint32_t g_write_start_calls;
static uint32_t g_write_done_calls;
static uint32_t g_sdmmc_init_calls;
static bool g_upgrade_mode;
static bool g_handshake_required;
static bool g_handshake_ok;
static bool g_sdmmc_raw_enabled;
static char g_call_order[16];
static size_t g_call_order_len;
static char g_last_device[16];
static uint64_t g_last_access_addr;
static uint64_t g_last_write_start_addr;
static uint64_t g_last_write_start_size;
static uint64_t g_last_write_done_addr;
static uint64_t g_last_write_done_size;

static struct adb_sync_ext_disk_ctx *ctx_init_or_null(const char *name, uint64_t start_addr, uint64_t size, bool write)
{
    struct adb_sync_ext_disk_ctx *ctx = NULL;

    if (adb_sync_ext_disk_ctx_init(name, start_addr, size, write, &ctx) != 0) {
        return NULL;
    }

    return ctx;
}

static void record_call(char step)
{
    g_call_order[g_call_order_len++] = step;
    g_call_order[g_call_order_len] = '\0';
}

static bool is_known_device(const char *name)
{
    return strcmp(name, "FLASH") == 0 || strcmp(name, "NAND") == 0 || strcmp(name, "SDRAW") == 0;
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

int boot_watchdog_feed(void)
{
    return 0;
}

int boot_flash_write(uint8_t *addr, uint8_t *data, uint32_t size)
{
    (void)addr;
    (void)data;
    (void)size;
    return 0;
}

int boot_flash_erase(uint8_t *addr, uint32_t size)
{
    (void)addr;
    (void)size;
    return 0;
}

void boot_flash_lock_clear(void)
{
}

void boot_flash_lock_boot(void)
{
}

void boot_flash_lock_resume(void)
{
}

void boot_flash_session_begin(void)
{
}

void boot_flash_session_end(void)
{
}

int printk(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    (void)vfprintf(stderr, fmt, args);
    va_end(args);
    return 0;
}

int disk_access_init(const char *name)
{
    (void)name;
    return 0;
}

int disk_access_ioctl(const char *name, uint8_t cmd, void *buff)
{
    g_ioctl_calls++;
    if (!is_known_device(name) || cmd != DISK_IOCTL_GET_SECTOR_SIZE) {
        return -1;
    }

    *(uint32_t *)buff = g_sector_size;
    return 0;
}

int disk_access_read(const char *name, uint8_t *data, uint64_t start_sector, uint32_t num_sector)
{
    size_t len = (size_t)(num_sector * g_sector_size);
    size_t offset;

    if (!is_known_device(name) || len > sizeof(g_disk_storage)) {
        return -1;
    }

    offset = (size_t)((start_sector * g_sector_size) % sizeof(g_disk_storage));
    if ((offset + len) > sizeof(g_disk_storage)) {
        offset = 0;
    }

    g_read_calls++;
    g_last_read_sector = start_sector;
    record_call('r');
    memcpy(data, g_disk_storage + offset, len);
    return 0;
}

int disk_access_write(const char *name, const uint8_t *data, uint64_t start_sector, uint32_t num_sector)
{
    size_t len = (size_t)(num_sector * g_sector_size);
    size_t offset;

    if (!is_known_device(name) || len > sizeof(g_disk_storage)) {
        return -1;
    }

    offset = (size_t)((start_sector * g_sector_size) % sizeof(g_disk_storage));
    if ((offset + len) > sizeof(g_disk_storage)) {
        offset = 0;
    }

    g_write_calls++;
    g_last_write_sector = start_sector;
    g_last_write_count = num_sector;
    record_call('w');
    memcpy(g_disk_storage + offset, data, len);
    return 0;
}

int adb_sync_ext_disk_prepare_sdmmc_raw(void)
{
    g_sdmmc_init_calls++;
    return 0;
}

bool boot_handshake_is_need(void)
{
    return g_handshake_required;
}

bool boot_handshake_is_ok(void)
{
    return g_handshake_ok;
}

uint8_t boot_upgrade_mode_get(void)
{
    return g_upgrade_mode ? 1u : 0u;
}

bool adb_sync_ext_disk_sdmmc_raw_enabled(void)
{
    return g_sdmmc_raw_enabled;
}

bool disk_device_can_access(const char *name, uint64_t addr)
{
    g_access_calls++;
    g_last_access_addr = addr;
    snprintf(g_last_device, sizeof(g_last_device), "%s", name);
    record_call('a');

    if (!is_known_device(name)) {
        return false;
    }

    if ((strcmp(name, "FLASH") == 0 || strcmp(name, "NAND") == 0) && addr < 0x30000ULL && !g_upgrade_mode) {
        return false;
    }

    return true;
}

void disk_device_write_start(const char *name, uint64_t addr, uint64_t size)
{
    g_write_start_calls++;
    snprintf(g_last_device, sizeof(g_last_device), "%s", name);
    g_last_write_start_addr = addr;
    g_last_write_start_size = size;
    record_call('s');
}

void disk_device_write_done(const char *name, uint64_t addr, uint64_t size)
{
    g_write_done_calls++;
    snprintf(g_last_device, sizeof(g_last_device), "%s", name);
    g_last_write_done_addr = addr;
    g_last_write_done_size = size;
    record_call('d');
}

void setUp(void)
{
    g_sector_size = 512;
    memset(g_disk_storage, 0, sizeof(g_disk_storage));
    g_last_read_sector = 0;
    g_last_write_sector = 0;
    g_last_write_count = 0;
    g_access_calls = 0;
    g_ioctl_calls = 0;
    g_read_calls = 0;
    g_write_calls = 0;
    g_write_start_calls = 0;
    g_write_done_calls = 0;
    g_sdmmc_init_calls = 0;
    g_upgrade_mode = false;
    g_handshake_required = false;
    g_handshake_ok = true;
    g_sdmmc_raw_enabled = false;
    memset(g_call_order, 0, sizeof(g_call_order));
    g_call_order_len = 0;
    memset(g_last_device, 0, sizeof(g_last_device));
    g_last_access_addr = 0;
    g_last_write_start_addr = 0;
    g_last_write_start_size = 0;
    g_last_write_done_addr = 0;
    g_last_write_done_size = 0;
}

void tearDown(void)
{
}

void test_parser_accepts_64_bit_raw_addresses(void)
{
    char path[] = "/RAW/FLASH/100000000/200";
    char *name = NULL;
    uint64_t addr = 0;
    uint64_t size = 0;

    TEST_ASSERT_EQUAL_INT(0, g_parse_path(path, &name, &addr, &size));
    TEST_ASSERT_TRUE(strcmp(name, "FLASH") == 0);
    TEST_ASSERT_TRUE(addr == 0x100000000ULL);
    TEST_ASSERT_TRUE(size == 0x200ULL);
}

void test_write_denied_before_upgrade_does_not_touch_disk_io(void)
{
    struct adb_sync_ext_disk_ctx *ctx = ctx_init_or_null("FLASH", 0x1000ULL, 0x200ULL, true);

    TEST_ASSERT_TRUE(ctx == NULL);
    TEST_ASSERT_EQUAL_UINT32(1u, g_access_calls);
    TEST_ASSERT_EQUAL_UINT32(0u, g_write_start_calls);
    TEST_ASSERT_EQUAL_UINT32(0u, g_write_calls);
    TEST_ASSERT_EQUAL_UINT32(0u, g_write_done_calls);
    TEST_ASSERT_TRUE(strcmp(g_call_order, "a") == 0);
}

void test_write_after_upgrade_brackets_disk_io_with_policy_callbacks(void)
{
    struct adb_sync_ext_disk_ctx *ctx;
    uint8_t data[512];

    memset(data, 0x5a, sizeof(data));
    g_upgrade_mode = true;

    ctx = ctx_init_or_null("FLASH", 0x1000ULL, sizeof(data), true);
    TEST_ASSERT_TRUE(ctx != NULL);
    adb_sync_ext_disk_write_start("FLASH", 0x1000ULL, sizeof(data));
    TEST_ASSERT_EQUAL_INT(0, adb_sync_ext_disk_write(ctx, data, sizeof(data)));
    adb_sync_ext_disk_ctx_free(ctx);
    adb_sync_ext_disk_write_end("FLASH", 0x1000ULL, sizeof(data));

    TEST_ASSERT_EQUAL_UINT32(1u, g_access_calls);
    TEST_ASSERT_EQUAL_UINT32(1u, g_write_start_calls);
    TEST_ASSERT_EQUAL_UINT32(1u, g_write_calls);
    TEST_ASSERT_EQUAL_UINT32(1u, g_write_done_calls);
    TEST_ASSERT_TRUE(strcmp(g_call_order, "aswd") == 0);
    TEST_ASSERT_TRUE(g_last_write_start_addr == 0x1000ULL);
    TEST_ASSERT_TRUE(g_last_write_start_size == sizeof(data));
    TEST_ASSERT_TRUE(g_last_write_done_addr == 0x1000ULL);
    TEST_ASSERT_TRUE(g_last_write_done_size == sizeof(data));
}

void test_sdraw_lazy_init_and_large_offsets_flow_through_policy(void)
{
    struct adb_sync_ext_disk_ctx *ctx;
    uint8_t data[512];
    const uint64_t start_addr = 0x100000000ULL;

    g_sdmmc_raw_enabled = true;

    ctx = ctx_init_or_null("SDRAW", start_addr, sizeof(data), false);
    TEST_ASSERT_TRUE(ctx != NULL);
    TEST_ASSERT_EQUAL_INT((int)sizeof(data), adb_sync_ext_disk_read(ctx, data, sizeof(data)));
    adb_sync_ext_disk_ctx_free(ctx);

    TEST_ASSERT_EQUAL_UINT32(1u, g_sdmmc_init_calls);
    TEST_ASSERT_EQUAL_UINT32(1u, g_access_calls);
    TEST_ASSERT_EQUAL_UINT32(1u, g_read_calls);
    TEST_ASSERT_TRUE(g_last_access_addr == start_addr);
    TEST_ASSERT_TRUE(g_last_read_sector == (start_addr / g_sector_size));
}

void test_sdraw_access_is_denied_when_raw_sdmmc_is_disabled(void)
{
    struct adb_sync_ext_disk_ctx *ctx;

    ctx = ctx_init_or_null("SDRAW", 0x100000000ULL, 0x200ULL, false);

    TEST_ASSERT_TRUE(ctx == NULL);
    TEST_ASSERT_EQUAL_UINT32(0u, g_access_calls);
    TEST_ASSERT_EQUAL_UINT32(0u, g_sdmmc_init_calls);
    TEST_ASSERT_TRUE(strcmp(g_call_order, "") == 0);
}

void test_invalid_device_name_and_zero_size_are_rejected(void)
{
    struct adb_sync_ext_disk_ctx *ctx;

    ctx = ctx_init_or_null("BOGUS", 0x0ULL, 0x200ULL, false);
    TEST_ASSERT_TRUE(ctx == NULL);
    TEST_ASSERT_EQUAL_UINT32(1u, g_access_calls);
    TEST_ASSERT_EQUAL_UINT32(0u, g_ioctl_calls);

    setUp();
    ctx = ctx_init_or_null("FLASH", 0x30000ULL, 0x0ULL, false);
    TEST_ASSERT_TRUE(ctx == NULL);
    TEST_ASSERT_EQUAL_UINT32(0u, g_access_calls);
    TEST_ASSERT_EQUAL_UINT32(0u, g_ioctl_calls);
}

int main(void)
{
    UnityBegin("system/uboot/test/boot_adb_raw_policy/test_boot_adb_raw_policy.c");

    RUN_TEST(test_parser_accepts_64_bit_raw_addresses, __LINE__);
    RUN_TEST(test_write_denied_before_upgrade_does_not_touch_disk_io, __LINE__);
    RUN_TEST(test_write_after_upgrade_brackets_disk_io_with_policy_callbacks, __LINE__);
    RUN_TEST(test_sdraw_access_is_denied_when_raw_sdmmc_is_disabled, __LINE__);
    RUN_TEST(test_sdraw_lazy_init_and_large_offsets_flow_through_policy, __LINE__);
    RUN_TEST(test_invalid_device_name_and_zero_size_are_rejected, __LINE__);

    return UnityEnd();
}

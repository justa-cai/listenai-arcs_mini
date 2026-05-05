#include "unity.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "adb_sync_ext_disk.h"
#include "adb_sync_protocol.h"
#include "disk/disk_access.h"

static uint32_t g_sector_size;
static uint8_t g_disk_storage[4096];
static uint32_t g_write_calls;
static uint64_t g_last_write_sector;
static uint32_t g_last_write_count;
static uint8_t g_boot_flash_storage[256 * 1024];
static uint32_t g_boot_flash_erase_calls;
static uint32_t g_boot_flash_write_calls;
static uint32_t g_watchdog_feed_calls;
static uint32_t g_boot_flash_session_begin_calls;
static uint32_t g_boot_flash_session_end_calls;
static uint64_t g_last_policy_start_addr;
static uint64_t g_last_policy_start_size;
static uint64_t g_last_policy_done_addr;
static uint64_t g_last_policy_done_size;
static uint32_t g_policy_start_calls;
static uint32_t g_policy_done_calls;

void *exram_malloc(size_t align, size_t size)
{
    (void)align;
    return calloc(1, size);
}

void exram_free(void *ptr)
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
    uintptr_t flash_addr = (uintptr_t)addr - 0x30000000u;
    uintptr_t erase_start = flash_addr & ~(uintptr_t)(4096u - 1u);
    uintptr_t erase_end = (flash_addr + size + 4095u) & ~(uintptr_t)(4096u - 1u);

    if (erase_end > sizeof(g_boot_flash_storage)) {
        return -1;
    }

    g_boot_flash_erase_calls++;
    memset(g_boot_flash_storage + erase_start, 0xff, erase_end - erase_start);
    return 0;
}

int boot_flash_write(uint8_t *addr, uint8_t *data, uint32_t size)
{
    uintptr_t flash_addr = (uintptr_t)addr - 0x30000000u;

    if ((flash_addr + size) > sizeof(g_boot_flash_storage)) {
        return -1;
    }

    g_boot_flash_write_calls++;
    memcpy(g_boot_flash_storage + flash_addr, data, size);
    return 0;
}

int boot_watchdog_feed(void)
{
    g_watchdog_feed_calls++;
    return 0;
}

void boot_flash_session_begin(void)
{
    g_boot_flash_session_begin_calls++;
}

void boot_flash_session_end(void)
{
    g_boot_flash_session_end_calls++;
}

int disk_access_init(const char *name)
{
    (void)name;
    return 0;
}

int disk_access_ioctl(const char *name, uint8_t cmd, void *buff)
{
    if (name == NULL || strcmp(name, "FLASH") != 0 || cmd != DISK_IOCTL_GET_SECTOR_SIZE || buff == NULL) {
        return -1;
    }

    *(uint32_t *)buff = g_sector_size;
    return 0;
}

int disk_access_read(const char *name, uint8_t *data, uint64_t start_sector, uint32_t num_sector)
{
    size_t offset = (size_t)(start_sector * g_sector_size);
    size_t len = (size_t)(num_sector * g_sector_size);

    if (name == NULL || strcmp(name, "FLASH") != 0 || data == NULL || (offset + len) > sizeof(g_disk_storage)) {
        return -1;
    }

    memcpy(data, g_disk_storage + offset, len);
    return 0;
}

int disk_access_write(const char *name, const uint8_t *data, uint64_t start_sector, uint32_t num_sector)
{
    size_t offset = (size_t)(start_sector * g_sector_size);
    size_t len = (size_t)(num_sector * g_sector_size);

    if (name == NULL || strcmp(name, "FLASH") != 0 || data == NULL || (offset + len) > sizeof(g_disk_storage)) {
        return -1;
    }

    g_write_calls++;
    g_last_write_sector = start_sector;
    g_last_write_count = num_sector;
    memcpy(g_disk_storage + offset, data, len);
    return 0;
}

bool adb_sync_ext_disk_policy_can_access(const char *name, uint64_t addr, uint64_t size, bool write)
{
    (void)addr;
    (void)size;
    (void)write;
    return name != NULL && strcmp(name, "FLASH") == 0;
}

int adb_sync_ext_disk_policy_write_start(const char *name, uint64_t addr, uint64_t size)
{
    if (name == NULL || strcmp(name, "FLASH") != 0) {
        return -1;
    }

    g_policy_start_calls++;
    g_last_policy_start_addr = addr;
    g_last_policy_start_size = size;
    return 0;
}

void adb_sync_ext_disk_policy_write_done(const char *name, uint64_t addr, uint64_t size)
{
    if (name == NULL || strcmp(name, "FLASH") != 0) {
        return;
    }

    g_policy_done_calls++;
    g_last_policy_done_addr = addr;
    g_last_policy_done_size = size;
}

static void fill_pattern(uint8_t *data, size_t len, uint8_t seed)
{
    size_t i;

    for (i = 0; i < len; ++i) {
        data[i] = (uint8_t)(seed + i);
    }
}

void setUp(void)
{
    g_sector_size = 512u;
    memset(g_disk_storage, 0, sizeof(g_disk_storage));
    memset(g_boot_flash_storage, 0xff, sizeof(g_boot_flash_storage));
    g_write_calls = 0u;
    g_last_write_sector = 0u;
    g_last_write_count = 0u;
    g_boot_flash_erase_calls = 0u;
    g_boot_flash_write_calls = 0u;
    g_watchdog_feed_calls = 0u;
    g_boot_flash_session_begin_calls = 0u;
    g_boot_flash_session_end_calls = 0u;
    g_last_policy_start_addr = 0u;
    g_last_policy_start_size = 0u;
    g_last_policy_done_addr = 0u;
    g_last_policy_done_size = 0u;
    g_policy_start_calls = 0u;
    g_policy_done_calls = 0u;
}

void tearDown(void)
{
}

void test_parser_accepts_raw_path_without_size_for_streaming_push(void)
{
    char path[] = "/RAW/FLASH/0x830000";
    char *name = NULL;
    uint64_t addr = 0u;
    uint64_t size = 123u;

    TEST_ASSERT_EQUAL_INT(0, adb_sync_ext_disk_get_info_by_path(path, &name, &addr, &size));
    TEST_ASSERT_TRUE(strcmp(name, "FLASH") == 0);
    TEST_ASSERT_TRUE(addr == 0x830000ULL);
    TEST_ASSERT_TRUE(size == 0u);
}

void test_streaming_write_without_size_reports_actual_bytes_on_done(void)
{
    struct adb_sync_ext_disk_ctx *ctx;
    uint8_t first[300];
    uint8_t second[300];

    fill_pattern(first, sizeof(first), 0x10u);
    fill_pattern(second, sizeof(second), 0x80u);

    ctx = adb_sync_ext_disk_ctx_init("FLASH", 0x200ULL, 0u, true);
    TEST_ASSERT_TRUE(ctx != NULL);
    TEST_ASSERT_EQUAL_INT(0, adb_sync_ext_disk_write(ctx, first, sizeof(first)));
    TEST_ASSERT_EQUAL_INT(0, adb_sync_ext_disk_write(ctx, second, sizeof(second)));
    adb_sync_ext_disk_ctx_free(ctx);

    TEST_ASSERT_EQUAL_UINT32(1u, g_policy_start_calls);
    TEST_ASSERT_EQUAL_UINT32(1u, g_policy_done_calls);
    TEST_ASSERT_TRUE(g_last_policy_start_addr == 0x200ULL);
    TEST_ASSERT_TRUE(g_last_policy_start_size == 0u);
    TEST_ASSERT_TRUE(g_last_policy_done_addr == 0x200ULL);
    TEST_ASSERT_TRUE(g_last_policy_done_size == 600ULL);
    TEST_ASSERT_EQUAL_UINT32(2u, g_write_calls);
    TEST_ASSERT_TRUE(memcmp(g_disk_storage + 0x200u, first, sizeof(first)) == 0);
    TEST_ASSERT_TRUE(memcmp(g_disk_storage + 0x200u + sizeof(first), second, sizeof(second)) == 0);
}

void test_known_size_multichunk_write_accepts_exact_total_size(void)
{
    struct adb_sync_ext_disk_ctx *ctx;
    uint8_t first[300];
    uint8_t second[300];

    fill_pattern(first, sizeof(first), 0x21u);
    fill_pattern(second, sizeof(second), 0x61u);

    ctx = adb_sync_ext_disk_ctx_init("FLASH", 0x200ULL, 600u, true);
    TEST_ASSERT_TRUE(ctx != NULL);
    TEST_ASSERT_EQUAL_INT(0, adb_sync_ext_disk_write(ctx, first, sizeof(first)));
    TEST_ASSERT_EQUAL_INT(0, adb_sync_ext_disk_write(ctx, second, sizeof(second)));
    adb_sync_ext_disk_ctx_free(ctx);

    TEST_ASSERT_EQUAL_UINT32(2u, g_write_calls);
    TEST_ASSERT_TRUE(g_last_policy_done_size == 600ULL);
}

void test_aligned_raw_flash_stream_write_uses_boot_flash_path(void)
{
    struct adb_sync_ext_disk_ctx *ctx;
    uint8_t data[70000];

    fill_pattern(data, sizeof(data), 0x33u);

    ctx = adb_sync_ext_disk_ctx_init("FLASH", 0x10000ULL, 0u, true);
    TEST_ASSERT_TRUE(ctx != NULL);
    TEST_ASSERT_EQUAL_INT(0, adb_sync_ext_disk_write(ctx, data, sizeof(data)));
    adb_sync_ext_disk_ctx_free(ctx);

    TEST_ASSERT_EQUAL_UINT32(0u, g_write_calls);
    TEST_ASSERT_TRUE(g_boot_flash_erase_calls >= 1u);
    TEST_ASSERT_TRUE(g_boot_flash_write_calls >= 1u);
    TEST_ASSERT_TRUE(g_watchdog_feed_calls > 0u);
    TEST_ASSERT_EQUAL_UINT32(1u, g_boot_flash_session_begin_calls);
    TEST_ASSERT_EQUAL_UINT32(1u, g_boot_flash_session_end_calls);
    TEST_ASSERT_TRUE(memcmp(g_boot_flash_storage + 0x10000u, data, sizeof(data)) == 0);
    TEST_ASSERT_TRUE(g_last_policy_done_size == sizeof(data));
}

void test_sync_recv_chunk_is_capped_to_protocol_limit(void)
{
    TEST_ASSERT_EQUAL_UINT32(4096u, adb_sync_data_chunk_limit(4096u));
    TEST_ASSERT_EQUAL_UINT32(65536u, adb_sync_data_chunk_limit(65536u));
    TEST_ASSERT_EQUAL_UINT32(65536u, adb_sync_data_chunk_limit(65537u));
    TEST_ASSERT_EQUAL_UINT32(65536u, adb_sync_data_chunk_limit(188304u));
}

void test_zero_size_read_context_stays_rejected(void)
{
    TEST_ASSERT_TRUE(adb_sync_ext_disk_ctx_init("FLASH", 0x0ULL, 0u, false) == NULL);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_parser_accepts_raw_path_without_size_for_streaming_push);
    RUN_TEST(test_streaming_write_without_size_reports_actual_bytes_on_done);
    RUN_TEST(test_known_size_multichunk_write_accepts_exact_total_size);
    RUN_TEST(test_aligned_raw_flash_stream_write_uses_boot_flash_path);
    RUN_TEST(test_sync_recv_chunk_is_capped_to_protocol_limit);
    RUN_TEST(test_zero_size_read_context_stays_rejected);

    return UNITY_END();
}

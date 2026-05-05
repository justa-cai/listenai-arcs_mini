#include "unity.h"

#ifdef RUN_TEST
#undef RUN_TEST
#endif
#define RUN_TEST(TestFunc, TestLineNum) UnityDefaultTestRun(TestFunc, #TestFunc, TestLineNum)

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "adb_sync_ext_disk.h"
#include "boot_mem_chunk.h"
#include "disk/disk_access.h"

#ifdef TEST_ASSERT_EQUAL_INT
#undef TEST_ASSERT_EQUAL_INT
#endif
#define TEST_ASSERT_EQUAL_INT(expected, actual) TEST_ASSERT_EQUAL_UINT32((uint32_t)(expected), (uint32_t)(actual))

#ifdef TEST_ASSERT_NOT_NULL
#undef TEST_ASSERT_NOT_NULL
#endif
#define TEST_ASSERT_NOT_NULL(ptr) TEST_ASSERT_TRUE((ptr) != NULL)

#ifdef TEST_ASSERT_EQUAL_HEX32
#undef TEST_ASSERT_EQUAL_HEX32
#endif
#define TEST_ASSERT_EQUAL_HEX32(expected, actual) TEST_ASSERT_EQUAL_UINT32((uint32_t)(expected), (uint32_t)(actual))
#define TEST_FLASH_BASE_ADDR 0x30000000UL
#define TEST_FLASH_MAP_SIZE  (8U * 1024U * 1024U)

#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
#define MAP_ANONYMOUS MAP_ANON
#endif

static uint32_t g_sector_size;
static uint32_t g_disk_write_calls;
static uint64_t g_last_disk_write_sector;
static uint32_t g_last_disk_write_count;
static uint32_t g_boot_flash_erase_calls;
static uint32_t g_boot_flash_write_calls;
static uint32_t g_boot_flash_session_begin_calls;
static uint32_t g_boot_flash_session_end_calls;
static uint32_t g_last_boot_flash_erase_size;
static uint32_t g_last_boot_flash_write_size;
static uintptr_t g_last_boot_flash_erase_addr;
static uintptr_t g_last_boot_flash_write_addr;
static uint32_t g_watchdog_feed_calls;
static uint8_t *g_flash_map;

static void ensure_flash_map_ready(void)
{
    void *ptr;

    if (g_flash_map != NULL) {
        memset(g_flash_map, 0xFF, TEST_FLASH_MAP_SIZE);
        return;
    }

    ptr = mmap((void *)TEST_FLASH_BASE_ADDR,
               TEST_FLASH_MAP_SIZE,
               PROT_READ | PROT_WRITE,
               MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
               -1,
               0);
    if (ptr == MAP_FAILED) {
        perror("mmap flash map");
        abort();
    }

    g_flash_map = ptr;
    memset(g_flash_map, 0xFF, TEST_FLASH_MAP_SIZE);
}

void *inram_malloc(size_t align, size_t size)
{
    (void)align;
    return calloc(1, size);
}

void *inram_realloc(void *ptr, size_t size)
{
    return realloc(ptr, size);
}

void *inram_calloc(size_t align, size_t num, size_t size)
{
    (void)align;
    return calloc(num, size);
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

int disk_access_init(const char *name)
{
    (void)name;
    return 0;
}

int disk_access_ioctl(const char *name, uint8_t cmd, void *buff)
{
    (void)name;
    if (cmd != DISK_IOCTL_GET_SECTOR_SIZE) {
        return -1;
    }

    *(uint32_t *)buff = g_sector_size;
    return 0;
}

int disk_access_read(const char *name, uint8_t *data, uint64_t start_sector, uint32_t num_sector)
{
    (void)name;
    (void)data;
    (void)start_sector;
    (void)num_sector;
    return 0;
}

int disk_access_write(const char *name, const uint8_t *data, uint64_t start_sector, uint32_t num_sector)
{
    (void)name;
    (void)data;
    g_disk_write_calls++;
    g_last_disk_write_sector = start_sector;
    g_last_disk_write_count = num_sector;
    return 0;
}

bool disk_device_can_access(const char *name, uint64_t addr)
{
    (void)name;
    (void)addr;
    return true;
}

void disk_device_write_start(const char *name, uint64_t addr, uint64_t size)
{
    (void)name;
    (void)addr;
    (void)size;
}

void disk_device_write_done(const char *name, uint64_t addr, uint64_t size)
{
    (void)name;
    (void)addr;
    (void)size;
}

int boot_flash_write(uint8_t *addr, uint8_t *data, uint32_t size)
{
    memcpy(addr, data, size);
    (void)data;
    g_boot_flash_write_calls++;
    g_last_boot_flash_write_addr = (uintptr_t)addr;
    g_last_boot_flash_write_size = size;
    return 0;
}

int boot_flash_erase(uint8_t *addr, uint32_t size)
{
    memset(addr, 0xFF, size);
    g_boot_flash_erase_calls++;
    g_last_boot_flash_erase_addr = (uintptr_t)addr;
    g_last_boot_flash_erase_size = size;
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

int boot_watchdog_feed(void)
{
    g_watchdog_feed_calls++;
    return 0;
}

void setUp(void)
{
    ensure_flash_map_ready();
    g_sector_size = 512;
    g_disk_write_calls = 0;
    g_last_disk_write_sector = 0;
    g_last_disk_write_count = 0;
    g_boot_flash_erase_calls = 0;
    g_boot_flash_write_calls = 0;
    g_boot_flash_session_begin_calls = 0;
    g_boot_flash_session_end_calls = 0;
    g_last_boot_flash_erase_size = 0;
    g_last_boot_flash_write_size = 0;
    g_last_boot_flash_erase_addr = 0;
    g_last_boot_flash_write_addr = 0;
    g_watchdog_feed_calls = 0;
}

void tearDown(void)
{
}

void test_aligned_raw_nand_write_uses_boot_flash_fast_path(void)
{
    struct adb_sync_ext_disk_ctx *ctx = NULL;
    uint8_t *payload = calloc(1, 128 * 1024);

    TEST_ASSERT_NOT_NULL(payload);
    TEST_ASSERT_EQUAL_INT(0, adb_sync_ext_disk_ctx_init("NAND", 0x500000, 0x20000, true, &ctx));
    TEST_ASSERT_NOT_NULL(ctx);

    TEST_ASSERT_EQUAL_INT(0, adb_sync_ext_disk_write(ctx, payload, 128 * 1024));
    adb_sync_ext_disk_ctx_free(ctx);

    TEST_ASSERT_EQUAL_INT(0, g_disk_write_calls);
    TEST_ASSERT_EQUAL_INT(0, g_boot_flash_erase_calls);
    TEST_ASSERT_EQUAL_INT(1, g_boot_flash_write_calls);
    TEST_ASSERT_EQUAL_INT(1, g_boot_flash_session_begin_calls);
    TEST_ASSERT_EQUAL_INT(1, g_boot_flash_session_end_calls);
    TEST_ASSERT_EQUAL_HEX32(0x30000000u + 0x500000u, (uint32_t)g_last_boot_flash_write_addr);
    TEST_ASSERT_EQUAL_INT(128 * 1024, g_last_boot_flash_write_size);
    TEST_ASSERT_TRUE(g_watchdog_feed_calls > 0);

    free(payload);
}

void test_unaligned_raw_nand_write_falls_back_to_disk_access(void)
{
    struct adb_sync_ext_disk_ctx *ctx = NULL;
    uint8_t payload[1024] = {0};

    TEST_ASSERT_EQUAL_INT(0, adb_sync_ext_disk_ctx_init("NAND", 0x500200, 0x1000, true, &ctx));
    TEST_ASSERT_NOT_NULL(ctx);

    TEST_ASSERT_EQUAL_INT(0, adb_sync_ext_disk_write(ctx, payload, sizeof(payload)));
    adb_sync_ext_disk_ctx_free(ctx);

    TEST_ASSERT_EQUAL_INT(0, g_boot_flash_erase_calls);
    TEST_ASSERT_EQUAL_INT(0, g_boot_flash_write_calls);
    TEST_ASSERT_EQUAL_INT(1, g_disk_write_calls);
    TEST_ASSERT_EQUAL_INT(0x500200 / 512, g_last_disk_write_sector);
    TEST_ASSERT_EQUAL_INT(2, g_last_disk_write_count);
}

int main(void)
{
    UnityBegin("system/uboot/test/boot_adb_raw_flash_fastpath/test_boot_adb_raw_flash_fastpath.c");
    RUN_TEST(test_aligned_raw_nand_write_uses_boot_flash_fast_path, __LINE__);
    RUN_TEST(test_unaligned_raw_nand_write_falls_back_to_disk_access, __LINE__);
    return UnityEnd();
}

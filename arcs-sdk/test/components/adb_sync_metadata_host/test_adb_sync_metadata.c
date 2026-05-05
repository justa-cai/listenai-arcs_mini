#include "unity.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "adb_sync_metadata.h"
#include "lsfs.h"

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))
#define TEST_METADATA_PATH "/SD:/adb/.adb_sync_metadata"

typedef struct {
    uint8_t data[1024];
    size_t size;
    bool exists;
    bool fail_stat;
    bool fail_open_read;
    bool fail_open_write;
    bool fail_write;
} test_sidecar_t;

static test_sidecar_t g_sidecar;
static char g_log_buffer[1024];

void *exram_malloc(int align, size_t size)
{
    (void)align;
    return malloc(size);
}

void exram_free(void *ptr)
{
    free(ptr);
}

int printk(const char *fmt, ...)
{
    va_list args;
    int written;

    va_start(args, fmt);
    written = vsnprintf(g_log_buffer, sizeof(g_log_buffer), fmt, args);
    va_end(args);
    return written;
}

static bool test_path_matches(const char *path)
{
    return path != NULL && strcmp(path, TEST_METADATA_PATH) == 0;
}

int lsfs_stat(const char *path, struct lsfs_dirent *entry)
{
    if (!test_path_matches(path) || g_sidecar.fail_stat || !g_sidecar.exists || entry == NULL) {
        return -1;
    }

    memset(entry, 0, sizeof(*entry));
    entry->size = (uint32_t)g_sidecar.size;
    return 0;
}

int lsfs_open(struct lsfs_file_t *fp, const char *file_name, lsfs_mode_t flags)
{
    if (fp == NULL || !test_path_matches(file_name)) {
        return -1;
    }
    if ((flags & LSFS_O_WRITE) != 0u && g_sidecar.fail_open_write) {
        return -1;
    }
    if ((flags & LSFS_O_READ) != 0u && (g_sidecar.fail_open_read || !g_sidecar.exists)) {
        return -1;
    }

    lsfs_file_t_init(fp);
    strncpy(fp->path, file_name, sizeof(fp->path) - 1u);
    fp->flags = flags;
    fp->open = 1u;

    if ((flags & LSFS_O_TRUNC) != 0u) {
        g_sidecar.exists = true;
        g_sidecar.size = 0u;
    }

    return 0;
}

int lsfs_close(struct lsfs_file_t *fp)
{
    if (fp == NULL || fp->open == 0u) {
        return -1;
    }

    fp->open = 0u;
    return 0;
}

ssize_t lsfs_read(struct lsfs_file_t *fp, void *ptr, size_t size)
{
    size_t remaining;
    size_t read_len;

    if (fp == NULL || ptr == NULL || fp->open == 0u || !g_sidecar.exists) {
        return -1;
    }

    remaining = g_sidecar.size - fp->pos;
    read_len = size < remaining ? size : remaining;
    memcpy(ptr, g_sidecar.data + fp->pos, read_len);
    fp->pos += read_len;
    return (ssize_t)read_len;
}

ssize_t lsfs_write(struct lsfs_file_t *fp, const void *ptr, size_t size)
{
    if (fp == NULL || ptr == NULL || fp->open == 0u || (fp->flags & LSFS_O_WRITE) == 0u || g_sidecar.fail_write) {
        return -1;
    }
    if ((fp->pos + size) > sizeof(g_sidecar.data)) {
        return -1;
    }

    memcpy(g_sidecar.data + fp->pos, ptr, size);
    fp->pos += size;
    g_sidecar.size = fp->pos > g_sidecar.size ? fp->pos : g_sidecar.size;
    g_sidecar.exists = true;
    return (ssize_t)size;
}

static void seed_sidecar(const char *text)
{
    memset(&g_sidecar, 0, sizeof(g_sidecar));
    if (text != NULL) {
        size_t len = strlen(text);
        memcpy(g_sidecar.data, text, len);
        g_sidecar.size = len;
        g_sidecar.exists = true;
    }
}

static void assert_sidecar_equals(const char *expected)
{
    size_t len = strlen(expected);

    TEST_ASSERT_TRUE(g_sidecar.exists);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)len, (uint32_t)g_sidecar.size);
    TEST_ASSERT_TRUE(memcmp(expected, g_sidecar.data, len) == 0);
}

void setUp(void)
{
    memset(&g_sidecar, 0, sizeof(g_sidecar));
    memset(g_log_buffer, 0, sizeof(g_log_buffer));
}

void tearDown(void)
{
}

void test_build_full_path_roots_relative_paths_and_preserves_absolute_paths(void)
{
    char *full_path = NULL;

    TEST_ASSERT_EQUAL_INT(ADB_SYNC_METADATA_OK,
                          adb_sync_metadata_build_full_path("bench/file.txt", &full_path));
    TEST_ASSERT_TRUE(strcmp(full_path, "/SD:/adb/bench/file.txt") == 0);
    exram_free(full_path);

    full_path = NULL;
    TEST_ASSERT_EQUAL_INT(ADB_SYNC_METADATA_OK,
                          adb_sync_metadata_build_full_path("/SD:/adb/bench/file.txt", &full_path));
    TEST_ASSERT_TRUE(strcmp(full_path, "/SD:/adb/bench/file.txt") == 0);
    exram_free(full_path);
}

void test_build_full_path_rejects_escaping_relative_paths(void)
{
    char *full_path = NULL;

    TEST_ASSERT_EQUAL_INT(ADB_SYNC_METADATA_INVALID_PATH,
                          adb_sync_metadata_build_full_path("bench/../escape.txt", &full_path));
    TEST_ASSERT_TRUE(full_path == NULL);
}

void test_get_timestamp_returns_not_found_when_sidecar_is_missing(void)
{
    uint32_t timestamp = 99u;

    TEST_ASSERT_EQUAL_INT(ADB_SYNC_METADATA_NOT_FOUND,
                          adb_sync_metadata_get_timestamp("bench/file.txt", &timestamp));
    TEST_ASSERT_EQUAL_UINT32(99u, timestamp);
}

void test_record_timestamp_persists_and_overwrites_relative_file_entries(void)
{
    uint32_t timestamp = 0u;

    TEST_ASSERT_EQUAL_INT(ADB_SYNC_METADATA_OK,
                          adb_sync_metadata_record_timestamp("bench/file.txt", 111u));
    assert_sidecar_equals("111\tbench/file.txt\n");

    TEST_ASSERT_EQUAL_INT(ADB_SYNC_METADATA_OK,
                          adb_sync_metadata_get_timestamp("bench/file.txt", &timestamp));
    TEST_ASSERT_EQUAL_UINT32(111u, timestamp);

    TEST_ASSERT_EQUAL_INT(ADB_SYNC_METADATA_OK,
                          adb_sync_metadata_record_timestamp("bench/file.txt", 222u));
    assert_sidecar_equals("222\tbench/file.txt\n");

    TEST_ASSERT_EQUAL_INT(ADB_SYNC_METADATA_OK,
                          adb_sync_metadata_get_timestamp("bench/file.txt", &timestamp));
    TEST_ASSERT_EQUAL_UINT32(222u, timestamp);
}

void test_record_timestamp_appends_new_relative_entries_without_disturbing_others(void)
{
    uint32_t timestamp = 0u;

    seed_sidecar("111\tbench/first.txt\n");

    TEST_ASSERT_EQUAL_INT(ADB_SYNC_METADATA_OK,
                          adb_sync_metadata_record_timestamp("bench/second.txt", 333u));
    assert_sidecar_equals("111\tbench/first.txt\n333\tbench/second.txt\n");

    TEST_ASSERT_EQUAL_INT(ADB_SYNC_METADATA_OK,
                          adb_sync_metadata_get_timestamp("bench/second.txt", &timestamp));
    TEST_ASSERT_EQUAL_UINT32(333u, timestamp);
}

void test_record_timestamp_replaces_duplicate_entries_without_crashing(void)
{
    uint32_t timestamp = 0u;

    seed_sidecar("111\tbench/file.txt\n222\tbench/file.txt\n333\tbench/other.txt\n");

    TEST_ASSERT_EQUAL_INT(ADB_SYNC_METADATA_OK,
                          adb_sync_metadata_record_timestamp("bench/file.txt", 444u));
    assert_sidecar_equals("444\tbench/file.txt\n333\tbench/other.txt\n");

    TEST_ASSERT_EQUAL_INT(ADB_SYNC_METADATA_OK,
                          adb_sync_metadata_get_timestamp("bench/file.txt", &timestamp));
    TEST_ASSERT_EQUAL_UINT32(444u, timestamp);
}

void test_absolute_and_raw_paths_bypass_metadata_sidecar(void)
{
    uint32_t timestamp = 77u;

    TEST_ASSERT_EQUAL_INT(ADB_SYNC_METADATA_BYPASS,
                          adb_sync_metadata_record_timestamp("/SD:/adb/bench/file.txt", 111u));
    TEST_ASSERT_FALSE(g_sidecar.exists);

    TEST_ASSERT_EQUAL_INT(ADB_SYNC_METADATA_BYPASS,
                          adb_sync_metadata_record_timestamp("/RAW/FLASH/0/1000", 111u));
    TEST_ASSERT_FALSE(g_sidecar.exists);

    TEST_ASSERT_EQUAL_INT(ADB_SYNC_METADATA_BYPASS,
                          adb_sync_metadata_get_timestamp("/SD:/adb/bench/file.txt", &timestamp));
    TEST_ASSERT_EQUAL_UINT32(77u, timestamp);
}

void test_invalid_relative_paths_are_rejected(void)
{
    TEST_ASSERT_EQUAL_INT(ADB_SYNC_METADATA_INVALID_PATH,
                          adb_sync_metadata_record_timestamp("", 1u));
    TEST_ASSERT_EQUAL_INT(ADB_SYNC_METADATA_INVALID_PATH,
                          adb_sync_metadata_record_timestamp("../escape.txt", 1u));
    TEST_ASSERT_EQUAL_INT(ADB_SYNC_METADATA_INVALID_PATH,
                          adb_sync_metadata_record_timestamp("bench/../escape.txt", 1u));
    TEST_ASSERT_EQUAL_INT(ADB_SYNC_METADATA_INVALID_PATH,
                          adb_sync_metadata_record_timestamp("bench//file.txt", 1u));
    TEST_ASSERT_FALSE(g_sidecar.exists);
}

void test_malformed_sidecar_records_fail_closed(void)
{
    uint32_t timestamp = 0u;

    seed_sidecar("broken-line-without-tab\n");
    TEST_ASSERT_EQUAL_INT(ADB_SYNC_METADATA_MALFORMED_RECORD,
                          adb_sync_metadata_get_timestamp("bench/file.txt", &timestamp));

    seed_sidecar("111\t../escape.txt\n");
    TEST_ASSERT_EQUAL_INT(ADB_SYNC_METADATA_MALFORMED_RECORD,
                          adb_sync_metadata_record_timestamp("bench/file.txt", 2u));
}

void test_write_failures_surface_as_io_errors(void)
{
    seed_sidecar("111\tbench/file.txt\n");
    g_sidecar.fail_write = true;

    TEST_ASSERT_EQUAL_INT(ADB_SYNC_METADATA_IO_ERROR,
                          adb_sync_metadata_record_timestamp("bench/file.txt", 444u));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_build_full_path_roots_relative_paths_and_preserves_absolute_paths);
    RUN_TEST(test_build_full_path_rejects_escaping_relative_paths);
    RUN_TEST(test_get_timestamp_returns_not_found_when_sidecar_is_missing);
    RUN_TEST(test_record_timestamp_persists_and_overwrites_relative_file_entries);
    RUN_TEST(test_record_timestamp_appends_new_relative_entries_without_disturbing_others);
    RUN_TEST(test_record_timestamp_replaces_duplicate_entries_without_crashing);
    RUN_TEST(test_absolute_and_raw_paths_bypass_metadata_sidecar);
    RUN_TEST(test_invalid_relative_paths_are_rejected);
    RUN_TEST(test_malformed_sidecar_records_fail_closed);
    RUN_TEST(test_write_failures_surface_as_io_errors);

    return UNITY_END();
}

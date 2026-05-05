#include "unity.h"
#include "adb_sync_stage.h"
#include "esp_heap_caps.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

struct flush_capture {
    uint8_t data[2048];
    uint32_t lengths[4];
    uint32_t offsets[4];
    uint32_t calls;
    uint32_t total;
    uintptr_t ptrs[4];
    int fail_on_call;
    int fail_code;
};

struct alloc_counters {
    uint32_t inram_allocs;
    uint32_t inram_frees;
    uint32_t exram_allocs;
    uint32_t exram_frees;
    uint32_t psram_allocs;
    uint32_t psram_frees;
    size_t largest_internal_block;
    bool force_inram_fail;
};

static struct alloc_counters g_alloc_counters;

static void *test_aligned_alloc(size_t align, size_t size)
{
    size_t alloc_size = size;
    void *ptr;

    if (align != 0u) {
        size_t rem = alloc_size % align;

        if (rem != 0u) {
            alloc_size += align - rem;
        }
    }

    ptr = aligned_alloc(align, alloc_size);
    if (ptr == NULL) {
        return NULL;
    }

    memset(ptr, 0, alloc_size);
    return ptr;
}

void *inram_malloc(size_t align, size_t size)
{
    if (g_alloc_counters.force_inram_fail) {
        return NULL;
    }

    g_alloc_counters.inram_allocs++;
    return test_aligned_alloc(align, size);
}

void inram_free(void *ptr)
{
    g_alloc_counters.inram_frees++;
    free(ptr);
}

void *exram_malloc(size_t align, size_t size)
{
    g_alloc_counters.exram_allocs++;
    return test_aligned_alloc(align, size);
}

void exram_free(void *ptr)
{
    g_alloc_counters.exram_frees++;
    free(ptr);
}

void *psram_malloc_align(size_t align, size_t size)
{
    g_alloc_counters.psram_allocs++;
    return test_aligned_alloc(align, size);
}

void psram_free(void *ptr)
{
    g_alloc_counters.psram_frees++;
    free(ptr);
}

int printk(const char *fmt, ...)
{
    (void)fmt;
    return 0;
}

size_t heap_caps_get_largest_free_block(uint32_t caps)
{
    if (caps == (MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL)) {
        return g_alloc_counters.largest_internal_block;
    }

    return 0u;
}

static int capture_flush(void *user_data, const uint8_t *data, uint32_t len)
{
    struct flush_capture *capture = (struct flush_capture *)user_data;

    if ((int)capture->calls == capture->fail_on_call) {
        return capture->fail_code;
    }

    if ((capture->total + len) > sizeof(capture->data)) {
        return -99;
    }

    capture->ptrs[capture->calls] = (uintptr_t)data;
    capture->lengths[capture->calls] = len;
    capture->offsets[capture->calls] = capture->total;
    memcpy(capture->data + capture->total, data, len);
    capture->total += len;
    capture->calls++;
    return 0;
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
    memset(&g_alloc_counters, 0, sizeof(g_alloc_counters));
    g_alloc_counters.largest_internal_block = (size_t)-1;
}

void tearDown(void)
{
}

void test_stage_flushes_aligned_full_chunk_and_tail(void)
{
    struct adb_sync_stage stage;
    struct flush_capture capture = { .fail_on_call = -1, .fail_code = -1 };
    uint8_t first[700 + 1];
    uint8_t second[500 + 1];
    uint8_t expected[1200];
    const uint8_t *first_data = first + 1;
    const uint8_t *second_data = second + 1;

    fill_pattern(first, sizeof(first), 0x10u);
    fill_pattern(second, sizeof(second), 0x80u);
    memcpy(expected, first_data, 700u);
    memcpy(expected + 700u, second_data, 500u);

    TEST_ASSERT_EQUAL_INT(0, adb_sync_stage_init(&stage, 1024u, 64u));
    TEST_ASSERT_EQUAL_INT(0, adb_sync_stage_write(&stage, first_data, 700u, capture_flush, &capture));
    TEST_ASSERT_EQUAL_INT(0, adb_sync_stage_write(&stage, second_data, 500u, capture_flush, &capture));
    TEST_ASSERT_EQUAL_INT(0, adb_sync_stage_finish(&stage, capture_flush, &capture));

    TEST_ASSERT_EQUAL_UINT32(2u, capture.calls);
    TEST_ASSERT_EQUAL_UINT32(1024u, capture.lengths[0]);
    TEST_ASSERT_EQUAL_UINT32(176u, capture.lengths[1]);
    TEST_ASSERT_EQUAL_UINT32(1200u, capture.total);
    TEST_ASSERT_TRUE((capture.ptrs[0] % 64u) == 0u);
    TEST_ASSERT_TRUE((capture.ptrs[1] % 64u) == 0u);
    TEST_ASSERT_EQUAL_MEMORY(expected, capture.data, sizeof(expected));

    adb_sync_stage_deinit(&stage);
}

void test_stage_propagates_flush_error(void)
{
    struct adb_sync_stage stage;
    struct flush_capture capture = { .fail_on_call = 0, .fail_code = -5 };
    uint8_t data[1024 + 3];

    fill_pattern(data, sizeof(data), 0x20u);

    TEST_ASSERT_EQUAL_INT(0, adb_sync_stage_init(&stage, 1024u, 64u));
    TEST_ASSERT_EQUAL_INT(-5, adb_sync_stage_write(&stage, data + 3, 1024u, capture_flush, &capture));

    adb_sync_stage_deinit(&stage);
}

void test_stage_boot_prefers_inram_and_frees_with_inram_free(void)
{
    struct adb_sync_stage stage;

    TEST_ASSERT_EQUAL_INT(0, adb_sync_stage_init(&stage, 1024u, 64u));
    TEST_ASSERT_EQUAL_UINT32(1u, g_alloc_counters.inram_allocs);
    TEST_ASSERT_EQUAL_UINT32(0u, g_alloc_counters.psram_allocs);

    adb_sync_stage_deinit(&stage);

    TEST_ASSERT_EQUAL_UINT32(1u, g_alloc_counters.inram_frees);
    TEST_ASSERT_EQUAL_UINT32(0u, g_alloc_counters.exram_frees);
    TEST_ASSERT_EQUAL_UINT32(0u, g_alloc_counters.psram_frees);
}

void test_stage_boot_falls_back_to_psram_and_frees_with_psram_free(void)
{
    struct adb_sync_stage stage;

    g_alloc_counters.force_inram_fail = true;

    TEST_ASSERT_EQUAL_INT(0, adb_sync_stage_init(&stage, 1024u, 64u));
    TEST_ASSERT_EQUAL_UINT32(0u, g_alloc_counters.inram_allocs);
    TEST_ASSERT_EQUAL_UINT32(1u, g_alloc_counters.psram_allocs);

    adb_sync_stage_deinit(&stage);

    TEST_ASSERT_EQUAL_UINT32(0u, g_alloc_counters.inram_frees);
    TEST_ASSERT_EQUAL_UINT32(0u, g_alloc_counters.exram_frees);
    TEST_ASSERT_EQUAL_UINT32(1u, g_alloc_counters.psram_frees);
}

void test_stage_boot_skips_inram_when_largest_internal_block_is_too_small(void)
{
    struct adb_sync_stage stage;

    g_alloc_counters.largest_internal_block = 1024u;

    TEST_ASSERT_EQUAL_INT(0, adb_sync_stage_init(&stage, 2048u, 64u));
    TEST_ASSERT_EQUAL_UINT32(0u, g_alloc_counters.inram_allocs);
    TEST_ASSERT_EQUAL_UINT32(1u, g_alloc_counters.psram_allocs);

    adb_sync_stage_deinit(&stage);

    TEST_ASSERT_EQUAL_UINT32(0u, g_alloc_counters.inram_frees);
    TEST_ASSERT_EQUAL_UINT32(1u, g_alloc_counters.psram_frees);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_stage_flushes_aligned_full_chunk_and_tail);
    RUN_TEST(test_stage_propagates_flush_error);
    RUN_TEST(test_stage_boot_prefers_inram_and_frees_with_inram_free);
    RUN_TEST(test_stage_boot_falls_back_to_psram_and_frees_with_psram_free);
    RUN_TEST(test_stage_boot_skips_inram_when_largest_internal_block_is_too_small);

    return UNITY_END();
}

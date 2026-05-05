#include "unity.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct adb_sync_stage {
    uint8_t *buf;
    uint32_t capacity;
    uint32_t size;
};

typedef int (*adb_sync_stage_flush_fn)(void *user_data, const uint8_t *data, uint32_t len);

int adb_sync_stage_init(struct adb_sync_stage *stage, uint32_t capacity, uint32_t alignment);
void adb_sync_stage_deinit(struct adb_sync_stage *stage);
int adb_sync_stage_write(struct adb_sync_stage *stage, const uint8_t *data, uint32_t len,
                         adb_sync_stage_flush_fn flush, void *user_data);
int adb_sync_stage_finish(struct adb_sync_stage *stage, adb_sync_stage_flush_fn flush, void *user_data);

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

void *exram_malloc(size_t align, size_t size)
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

void exram_free(void *ptr)
{
    free(ptr);
}

int printk(const char *fmt, ...)
{
    (void)fmt;
    return 0;
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

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_stage_flushes_aligned_full_chunk_and_tail);
    RUN_TEST(test_stage_propagates_flush_error);

    return UNITY_END();
}

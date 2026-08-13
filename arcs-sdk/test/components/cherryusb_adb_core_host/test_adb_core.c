#include "unity.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "adb.h"

static size_t g_largest_internal_block;
static unsigned int g_inram_alloc_calls;
static unsigned int g_psram_alloc_calls;
static unsigned int g_exram_alloc_calls;
static bool g_inram_malloc_force_success;

#include "../../../components/cherryusb-appclass/adb/adb.c"

void *inram_malloc(size_t align, size_t size)
{
    size_t required_size = size + align;

    g_inram_alloc_calls++;
    if (g_inram_malloc_force_success) {
        return calloc(1u, size);
    }
    if (g_largest_internal_block < required_size) {
        return NULL;
    }
    return calloc(1u, size);
}

void inram_free(void *ptr)
{
    free(ptr);
}

void *psram_malloc_align(size_t align, size_t size)
{
    (void)align;
    g_psram_alloc_calls++;
    return calloc(1u, size);
}

void psram_free(void *ptr)
{
    free(ptr);
}

void *exram_malloc(size_t align, size_t size)
{
    (void)align;
    g_exram_alloc_calls++;
    return calloc(1u, size);
}

void exram_free(void *ptr)
{
    free(ptr);
}

size_t heap_caps_get_largest_free_block(uint32_t caps)
{
    (void)caps;
    return g_largest_internal_block;
}

bool adb_dev_send(uint8_t *buf, uint32_t len)
{
    (void)buf;
    (void)len;
    return true;
}

void adb_dev_notify_packet_free(void)
{
}

void adb_dev_recv_cb_set(adb_dev_recv_cb_t cb)
{
    (void)cb;
}

uint32_t adb_service_open(const uint8_t *name, const uint8_t *args, uint32_t remote_id)
{
    (void)name;
    (void)args;
    (void)remote_id;
    return 1u;
}

int adb_service_write(uint32_t local_id, uint32_t remote_id, adb_packet_t *packet)
{
    (void)local_id;
    (void)remote_id;
    adb_packet_free(packet);
    return 0;
}

void adb_service_close(uint32_t local_id, uint32_t remote_id)
{
    (void)local_id;
    (void)remote_id;
}

void adb_service_close_all(void)
{
}

void adb_service_note_close_sent(uint32_t local_id, uint32_t remote_id)
{
    (void)local_id;
    (void)remote_id;
}

SemaphoreHandle_t xSemaphoreCreateMutex(void)
{
    return (SemaphoreHandle_t)0x1;
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t semaphore, uint32_t ticks_to_wait)
{
    (void)semaphore;
    (void)ticks_to_wait;
    return pdTRUE;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t semaphore)
{
    (void)semaphore;
    return pdTRUE;
}

int printk(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    (void)vfprintf(stderr, fmt, args);
    va_end(args);
    return 0;
}

void setUp(void)
{
    g_largest_internal_block = 0u;
    g_inram_alloc_calls = 0u;
    g_psram_alloc_calls = 0u;
    g_exram_alloc_calls = 0u;
    g_inram_malloc_force_success = false;
}

void tearDown(void)
{
}

void test_adb_packet_alloc_prefers_inram_when_available(void)
{
    adb_packet_t *packet;

    g_largest_internal_block = sizeof(adb_packet_t) + 65536u + ADB_PACKET_ALIGN;
    packet = adb_packet_alloc(65536u);

    TEST_ASSERT_NOT_NULL(packet);
    TEST_ASSERT_EQUAL_UINT(ADB_PACKET_HEADER_SIZE, sizeof(adb_packet_t));
    TEST_ASSERT_EQUAL_UINT(ADB_PACKET_HEADER_SIZE, (unsigned int)((uintptr_t)packet->data - (uintptr_t)packet));
    TEST_ASSERT_EQUAL_UINT(1u, g_inram_alloc_calls);
    TEST_ASSERT_EQUAL_UINT(0u, g_psram_alloc_calls);
    TEST_ASSERT_EQUAL_UINT(0u, g_exram_alloc_calls);

    adb_packet_free(packet);
}

void test_adb_packet_alloc_returns_null_when_inram_unavailable(void)
{
    adb_packet_t *packet;

    g_largest_internal_block = 0u;
    packet = adb_packet_alloc(65536u);

    TEST_ASSERT_NULL(packet);
    TEST_ASSERT_EQUAL_UINT(0u, g_inram_alloc_calls);
    TEST_ASSERT_EQUAL_UINT(0u, g_psram_alloc_calls);
    TEST_ASSERT_EQUAL_UINT(0u, g_exram_alloc_calls);
}

void test_adb_packet_alloc_rejects_fragmented_inram_before_alloc(void)
{
    adb_packet_t *packet;

    g_largest_internal_block = sizeof(adb_packet_t) + 65536u;
    g_inram_malloc_force_success = true;
    packet = adb_packet_alloc(65536u);

    TEST_ASSERT_NULL(packet);
    TEST_ASSERT_EQUAL_UINT(0u, g_inram_alloc_calls);
    TEST_ASSERT_EQUAL_UINT(0u, g_psram_alloc_calls);
    TEST_ASSERT_EQUAL_UINT(0u, g_exram_alloc_calls);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_adb_packet_alloc_prefers_inram_when_available);
    RUN_TEST(test_adb_packet_alloc_returns_null_when_inram_unavailable);
    RUN_TEST(test_adb_packet_alloc_rejects_fragmented_inram_before_alloc);
    return UNITY_END();
}

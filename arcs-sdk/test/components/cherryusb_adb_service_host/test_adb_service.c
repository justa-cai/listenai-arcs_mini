#include "unity.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "adb.h"
#include "adb_device.h"
#include "adb_services.h"

static adb_dev_recv_cb_t g_recv_cb;
static uint32_t g_service_open_count;
static uint32_t g_service_close_count;

struct sent_message {
    struct message msg;
};

static struct sent_message g_sent_messages[8];
static uint32_t g_sent_message_count;

static int test_sync_open(struct adb_service *service, const uint8_t *args);
static int test_sync_close(struct adb_service *service);
static int test_sync_write(struct adb_service *service, adb_packet_t *packet);

#include "../../../components/cherryusb-appclass/adb/adb_services.c"
#undef LOG_TAG
#include "../../../components/cherryusb-appclass/adb/adb.c"

static const struct adb_service_handle g_test_sync_handle = {
    .name = (uint8_t *)"sync",
    .open = test_sync_open,
    .close = test_sync_close,
    .write = test_sync_write,
};

static adb_packet_t *test_packet_alloc_with_payload(uint32_t command,
                                                    uint32_t arg0,
                                                    uint32_t arg1,
                                                    const char *payload)
{
    size_t payload_len = payload == NULL ? 0u : strlen(payload) + 1u;
    adb_packet_t *packet = adb_packet_alloc((uint32_t)payload_len);
    const uint8_t *payload_bytes = (const uint8_t *)payload;
    uint32_t checksum = 0u;

    TEST_ASSERT_NOT_NULL(packet);

    packet->msg.command = command;
    packet->msg.arg0 = arg0;
    packet->msg.arg1 = arg1;
    packet->msg.data_length = (uint32_t)payload_len;
    packet->msg.magic = command ^ 0xffffffffu;

    if (payload_len != 0u) {
        memcpy(packet->data, payload, payload_len);
        for (size_t i = 0; i < payload_len; ++i) {
            checksum += payload_bytes[i];
        }
    }

    packet->msg.data_check = checksum;

    return packet;
}

static void test_deliver_packet(adb_packet_t *packet)
{
    TEST_ASSERT_NOT_NULL(g_recv_cb);
    g_recv_cb(packet);
}

void *inram_malloc(size_t align, size_t size)
{
    (void)align;
    return calloc(1u, size);
}

void inram_free(void *ptr)
{
    free(ptr);
}

void *psram_malloc_align(size_t align, size_t size)
{
    (void)align;
    return calloc(1u, size);
}

void psram_free(void *ptr)
{
    free(ptr);
}

void *exram_malloc(size_t align, size_t size)
{
    (void)align;
    return calloc(1u, size);
}

void exram_free(void *ptr)
{
    free(ptr);
}

size_t heap_caps_get_largest_free_block(uint32_t caps)
{
    (void)caps;
    return 1024u * 1024u;
}

bool adb_dev_send(uint8_t *buf, uint32_t len)
{
    TEST_ASSERT_EQUAL_UINT(sizeof(struct message), len);
    TEST_ASSERT_TRUE(g_sent_message_count < (sizeof(g_sent_messages) / sizeof(g_sent_messages[0])));

    memcpy(&g_sent_messages[g_sent_message_count].msg, buf, len);
    g_sent_message_count++;
    return true;
}

void adb_dev_notify_packet_free(void)
{
}

void adb_dev_recv_cb_set(adb_dev_recv_cb_t cb)
{
    g_recv_cb = cb;
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

static int test_sync_open(struct adb_service *service, const uint8_t *args)
{
    (void)args;
    g_service_open_count++;
    service->data = service;
    return 0;
}

static int test_sync_close(struct adb_service *service)
{
    g_service_close_count++;
    service->data = NULL;
    return 0;
}

static int test_sync_write(struct adb_service *service, adb_packet_t *packet)
{
    (void)service;
    adb_packet_free(packet);
    return 0;
}

void setUp(void)
{
    memset(adb_services, 0, sizeof(adb_services));
    memset(adb_services_handle, 0, sizeof(adb_services_handle));
    g_recv_cb = NULL;
    g_service_open_count = 0u;
    g_service_close_count = 0u;
    g_sent_message_count = 0u;
    adb_msg_send_lock = NULL;
    adb_remote_max_payload = MAX_PAYLOAD;

    adb_init();
    TEST_ASSERT_EQUAL_INT(0, adb_service_hd_register(&g_test_sync_handle));
}

void tearDown(void)
{
    adb_reset();
}

void test_remote_close_after_local_close_does_not_emit_duplicate_clse(void)
{
    adb_packet_t *open_packet;
    adb_packet_t *close_packet;
    uint32_t local_id;

    open_packet = test_packet_alloc_with_payload(A_OPEN, 1u, 0u, "sync:");
    test_deliver_packet(open_packet);

    TEST_ASSERT_EQUAL_UINT32(1u, g_service_open_count);
    TEST_ASSERT_EQUAL_UINT32(1u, g_sent_message_count);
    TEST_ASSERT_EQUAL_UINT32(A_OKAY, g_sent_messages[0].msg.command);

    local_id = g_sent_messages[0].msg.arg0;
    TEST_ASSERT_NOT_EQUAL(0u, local_id);
    TEST_ASSERT_EQUAL_UINT32(1u, g_sent_messages[0].msg.arg1);

    adb_close(local_id, 1u);

    TEST_ASSERT_EQUAL_UINT32(2u, g_sent_message_count);
    TEST_ASSERT_EQUAL_UINT32(A_CLSE, g_sent_messages[1].msg.command);
    TEST_ASSERT_EQUAL_UINT32(local_id, g_sent_messages[1].msg.arg0);
    TEST_ASSERT_EQUAL_UINT32(1u, g_sent_messages[1].msg.arg1);

    close_packet = test_packet_alloc_with_payload(A_CLSE, 1u, local_id, NULL);
    test_deliver_packet(close_packet);

    TEST_ASSERT_EQUAL_UINT32(1u, g_service_close_count);
    TEST_ASSERT_EQUAL_UINT32(2u, g_sent_message_count);
}

void test_immediate_reopen_after_local_close_receives_okay(void)
{
    adb_packet_t *first_open;
    adb_packet_t *first_close;
    adb_packet_t *second_open;
    uint32_t first_local_id;

    first_open = test_packet_alloc_with_payload(A_OPEN, 1u, 0u, "sync:");
    test_deliver_packet(first_open);

    TEST_ASSERT_EQUAL_UINT32(A_OKAY, g_sent_messages[0].msg.command);
    first_local_id = g_sent_messages[0].msg.arg0;

    adb_close(first_local_id, 1u);
    TEST_ASSERT_EQUAL_UINT32(A_CLSE, g_sent_messages[1].msg.command);

    first_close = test_packet_alloc_with_payload(A_CLSE, 1u, first_local_id, NULL);
    test_deliver_packet(first_close);

    second_open = test_packet_alloc_with_payload(A_OPEN, 2u, 0u, "sync:");
    test_deliver_packet(second_open);

    TEST_ASSERT_EQUAL_UINT32(3u, g_sent_message_count);
    TEST_ASSERT_EQUAL_UINT32(A_OKAY, g_sent_messages[2].msg.command);
    TEST_ASSERT_NOT_EQUAL(first_local_id, g_sent_messages[2].msg.arg0);
    TEST_ASSERT_EQUAL_UINT32(2u, g_sent_messages[2].msg.arg1);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_remote_close_after_local_close_does_not_emit_duplicate_clse);
    RUN_TEST(test_immediate_reopen_after_local_close_receives_okay);
    return UNITY_END();
}

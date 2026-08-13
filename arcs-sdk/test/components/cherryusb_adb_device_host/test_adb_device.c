#include "unity.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "task.h"
#include "adb.h"
#include "adb_device.h"

struct usb_read_request {
    uint8_t ep;
    uint8_t *buf;
    uint32_t len;
};

struct usb_read_capture {
    struct usb_read_request requests[8];
    uint32_t count;
};

struct usb_write_request {
    uint8_t ep;
    uint32_t len;
};

struct usb_write_capture {
    struct usb_write_request requests[32];
    uint32_t count;
};

static struct usb_read_capture g_usb_read_capture;
static struct usb_write_capture g_usb_write_capture;
static TaskHandle_t g_task_handle = (TaskHandle_t)0x1234;
static uint32_t g_task_notify_count;
static int g_semaphore_tokens;
static pthread_mutex_t g_semaphore_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_semaphore_cond = PTHREAD_COND_INITIALIZER;
static adb_packet_t *g_dispatched_packet;
static uint32_t g_auto_complete_write_delay_us;

static void fill_pattern(uint8_t *data, uint32_t len, uint8_t seed)
{
    uint32_t i;

    for (i = 0; i < len; ++i) {
        data[i] = (uint8_t)(seed + i);
    }
}

static void assert_pattern(const uint8_t *data, uint32_t len, uint8_t seed)
{
    uint32_t i;

    for (i = 0; i < len; ++i) {
        TEST_ASSERT_EQUAL_UINT8((uint8_t)(seed + i), data[i]);
    }
}

static uint64_t monotonic_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((uint64_t)ts.tv_sec * 1000u) + ((uint64_t)ts.tv_nsec / 1000000u);
}

static void capture_packet(adb_packet_t *packet)
{
    g_dispatched_packet = packet;
}

static struct message make_message(uint32_t command, uint32_t data_length)
{
    struct message msg = {0};

    msg.command = command;
    msg.arg0 = 1u;
    msg.arg1 = 2u;
    msg.data_length = data_length;
    msg.magic = command ^ 0xffffffffu;
    return msg;
}

static void adb_ep_in_cb(uint8_t busid, uint8_t ep, uint32_t nbytes);

static void *delayed_write_complete(void *arg)
{
    uint32_t delay_us = *(uint32_t *)arg;

    free(arg);
    struct timespec delay = {
        .tv_sec = delay_us / 1000000u,
        .tv_nsec = (long)(delay_us % 1000000u) * 1000l,
    };

    nanosleep(&delay, NULL);
    adb_ep_in_cb(0u, 0x81u, 0u);
    return NULL;
}

#include "../../../components/cherryusb-appclass/adb/adb_device.c"

void *exram_malloc(size_t align, size_t size)
{
    (void)align;
    return calloc(1u, size);
}

void exram_free(void *ptr)
{
    free(ptr);
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

size_t heap_caps_get_largest_free_block(uint32_t caps)
{
    (void)caps;
    return 1024u * 1024u;
}

int printk(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    (void)vfprintf(stderr, fmt, args);
    va_end(args);
    return 0;
}

adb_packet_t *adb_packet_alloc(uint32_t payload_len)
{
    return (adb_packet_t *)calloc(1u, sizeof(adb_packet_t) + payload_len);
}

void adb_packet_free(adb_packet_t *packet)
{
    free(packet);
}

BaseType_t xTaskCreate(TaskFunction_t task_func, const char *name,
                       uint16_t stack_depth, void *params,
                       UBaseType_t priority, TaskHandle_t *task_handle)
{
    (void)task_func;
    (void)name;
    (void)stack_depth;
    (void)params;
    (void)priority;

    if (task_handle != NULL) {
        *task_handle = g_task_handle;
    }

    return pdPASS;
}

void vTaskDelete(TaskHandle_t task_handle)
{
    (void)task_handle;
}

void vTaskNotifyGiveFromISR(TaskHandle_t task_handle, BaseType_t *woken)
{
    (void)task_handle;
    if (woken != NULL) {
        *woken = pdFALSE;
    }
    g_task_notify_count++;
}

void xTaskNotifyGive(TaskHandle_t task_handle)
{
    (void)task_handle;
    g_task_notify_count++;
}

void xTaskNotifyStateClear(TaskHandle_t task_handle)
{
    (void)task_handle;
    g_task_notify_count = 0u;
}

uint32_t ulTaskNotifyTake(BaseType_t clear_count_on_exit, uint32_t ticks_to_wait)
{
    uint32_t notified = g_task_notify_count;

    (void)ticks_to_wait;
    if (clear_count_on_exit != pdFALSE) {
        g_task_notify_count = 0u;
    } else if (g_task_notify_count > 0u) {
        g_task_notify_count--;
    }

    return notified;
}

SemaphoreHandle_t xSemaphoreCreateBinary(void)
{
    pthread_mutex_lock(&g_semaphore_mutex);
    g_semaphore_tokens = 0;
    pthread_mutex_unlock(&g_semaphore_mutex);
    return &g_semaphore_tokens;
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t semaphore, uint32_t ticks_to_wait)
{
    int ret = pdTRUE;
    struct timespec deadline;

    if (semaphore == NULL) {
        return pdFALSE;
    }

    pthread_mutex_lock(&g_semaphore_mutex);
    if (ticks_to_wait == 0u) {
        if (g_semaphore_tokens <= 0) {
            ret = pdFALSE;
            goto done;
        }
    } else {
        clock_gettime(CLOCK_REALTIME, &deadline);
        deadline.tv_sec += ticks_to_wait / 1000u;
        deadline.tv_nsec += (long)(ticks_to_wait % 1000u) * 1000000l;
        if (deadline.tv_nsec >= 1000000000l) {
            deadline.tv_sec += 1;
            deadline.tv_nsec -= 1000000000l;
        }

        while (g_semaphore_tokens <= 0) {
            if (pthread_cond_timedwait(&g_semaphore_cond, &g_semaphore_mutex, &deadline) != 0) {
                ret = pdFALSE;
                goto done;
            }
        }
    }

    g_semaphore_tokens--;

done:
    pthread_mutex_unlock(&g_semaphore_mutex);
    return ret;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t semaphore)
{
    if (semaphore == NULL) {
        return pdFALSE;
    }

    pthread_mutex_lock(&g_semaphore_mutex);
    g_semaphore_tokens++;
    pthread_cond_signal(&g_semaphore_cond);
    pthread_mutex_unlock(&g_semaphore_mutex);
    return pdTRUE;
}

BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t semaphore, BaseType_t *woken)
{
    BaseType_t ret = xSemaphoreGive(semaphore);

    if (woken != NULL) {
        *woken = pdFALSE;
    }

    return ret;
}

void vSemaphoreDelete(SemaphoreHandle_t semaphore)
{
    (void)semaphore;
    g_semaphore_tokens = 0;
}

int usbd_ep_start_read(uint8_t busid, uint8_t ep, uint8_t *data, uint32_t data_len)
{
    struct usb_read_request *request;
    const uint32_t request_capacity =
        (uint32_t)(sizeof(g_usb_read_capture.requests) / sizeof(g_usb_read_capture.requests[0]));

    (void)busid;
    TEST_ASSERT_TRUE(g_usb_read_capture.count < request_capacity);

    request = &g_usb_read_capture.requests[g_usb_read_capture.count++];
    request->ep = ep;
    request->buf = data;
    request->len = data_len;
    return 0;
}

int usbd_ep_start_write(uint8_t busid, uint8_t ep, const uint8_t *data, uint32_t data_len)
{
    struct usb_write_request *request;
    const uint32_t request_capacity =
        (uint32_t)(sizeof(g_usb_write_capture.requests) / sizeof(g_usb_write_capture.requests[0]));
    pthread_t thread;
    uint32_t *delay_us;

    (void)busid;
    (void)data;
    TEST_ASSERT_TRUE(g_usb_write_capture.count < request_capacity);

    request = &g_usb_write_capture.requests[g_usb_write_capture.count++];
    request->ep = ep;
    request->len = data_len;

    if (g_auto_complete_write_delay_us == 0u) {
        return 0;
    }

    delay_us = (uint32_t *)malloc(sizeof(*delay_us));
    TEST_ASSERT_NOT_NULL(delay_us);
    *delay_us = g_auto_complete_write_delay_us;
    TEST_ASSERT_EQUAL_INT(0, pthread_create(&thread, NULL, delayed_write_complete, delay_us));
    TEST_ASSERT_EQUAL_INT(0, pthread_detach(thread));
    return 0;
}

void usbd_add_endpoint(uint8_t busid, struct usbd_endpoint *ep)
{
    (void)busid;
    (void)ep;
}

static void init_configured_device(void)
{
    struct usbd_interface intf = {0};

    adb_dev_init_intf(0u, &intf, 0x81u, 0x01u);
    TEST_ASSERT_NOT_NULL(intf.notify_handler);
    intf.notify_handler(0u, USBD_EVENT_CONFIGURED, NULL);
    TEST_ASSERT_EQUAL_UINT32(1u, g_usb_read_capture.count);
    TEST_ASSERT_EQUAL_UINT32(ADB_MESSAGE_SIZE, g_usb_read_capture.requests[0].len);
}

void setUp(void)
{
    memset(&g_usb_read_capture, 0, sizeof(g_usb_read_capture));
    memset(&g_usb_write_capture, 0, sizeof(g_usb_write_capture));
    g_task_notify_count = 0u;
    g_dispatched_packet = NULL;
    g_auto_complete_write_delay_us = 0u;

    if (s_tx_sem != NULL) {
        adb_dev_deinit();
    }
}

void tearDown(void)
{
    if (g_dispatched_packet != NULL) {
        adb_packet_free(g_dispatched_packet);
        g_dispatched_packet = NULL;
    }

    if (s_tx_sem != NULL) {
        adb_dev_deinit();
    }
}

void test_boot_rx_64k_payload_arms_single_data_read(void)
{
    const struct message msg = make_message(A_WRTE, MAX_PAYLOAD);

    init_configured_device();

    memcpy(s_rx_msg_buf, &msg, sizeof(msg));
    s_rx_nbytes = ADB_MESSAGE_SIZE;

    adb_dev_handle_msg_done();

    TEST_ASSERT_EQUAL_UINT32(2u, g_usb_read_capture.count);
    TEST_ASSERT_EQUAL_UINT32(MAX_PAYLOAD, g_usb_read_capture.requests[1].len);
    TEST_ASSERT_EQUAL_PTR(s_rx_packet->data, g_usb_read_capture.requests[1].buf);
}

void test_boot_rx_dispatches_full_64k_payload_after_single_completion(void)
{
    const struct message msg = make_message(A_WRTE, MAX_PAYLOAD);

    init_configured_device();
    adb_dev_recv_cb_set(capture_packet);

    memcpy(s_rx_msg_buf, &msg, sizeof(msg));
    s_rx_nbytes = ADB_MESSAGE_SIZE;
    adb_dev_handle_msg_done();

    fill_pattern(s_rx_packet->data, msg.data_length, 0x20u);
    s_rx_nbytes = msg.data_length;
    adb_dev_handle_data_done();

    TEST_ASSERT_NOT_NULL(g_dispatched_packet);
    TEST_ASSERT_EQUAL_UINT32(msg.data_length, g_dispatched_packet->msg.data_length);
    assert_pattern(g_dispatched_packet->data, msg.data_length, 0x20u);
}

void test_tx_send_waits_for_in_completion_before_returning(void)
{
    uint8_t payload[8] = {0};
    uint64_t begin_ms;
    uint64_t elapsed_ms;

    init_configured_device();
    g_auto_complete_write_delay_us = 30000u;

    begin_ms = monotonic_ms();
    TEST_ASSERT_TRUE(adb_dev_send(payload, sizeof(payload)));
    elapsed_ms = monotonic_ms() - begin_ms;

    TEST_ASSERT_TRUE(elapsed_ms >= 20u);
    TEST_ASSERT_EQUAL_UINT32(1u, g_usb_write_capture.count);
    TEST_ASSERT_EQUAL_UINT32(sizeof(payload), g_usb_write_capture.requests[0].len);
    TEST_ASSERT_EQUAL_INT(1, g_semaphore_tokens);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_boot_rx_64k_payload_arms_single_data_read);
    RUN_TEST(test_boot_rx_dispatches_full_64k_payload_after_single_completion);
    RUN_TEST(test_tx_send_waits_for_in_completion_before_returning);
    return UNITY_END();
}

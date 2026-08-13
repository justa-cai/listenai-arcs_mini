/*
 * Copyright (c) 2024, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * ADB device HAL - CherryUSB raw vendor-endpoint transport.
 */

#define LOG_TAG "adb.dev"

#include "adb_utils.h"
#include "adb_device.h"

#include "usbd_core.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#if defined(CONFIG_BOOT_ADB) && defined(CONFIG_BOOT_ADB_BACKEND_CHERRYUSB)
#include "soc/chip.h"
#endif

#include <string.h>

enum adb_rx_state {
    ADB_RX_STATE_MSG = 0,
    ADB_RX_STATE_DATA,
    ADB_RX_STATE_DROP,
    ADB_RX_STATE_WAIT_PACKET,
};

static adb_dev_recv_cb_t s_recv_cb = NULL;
static SemaphoreHandle_t s_tx_sem = NULL;
static TaskHandle_t s_rx_dispatch_task = NULL;
static uint32_t s_rx_nbytes = 0;
static uint32_t s_rx_payload_offset = 0;
static uint32_t s_rx_drop_remaining = 0;
static enum adb_rx_state s_rx_state = ADB_RX_STATE_MSG;
static volatile bool s_configured = false;
static volatile bool s_configure_pending = false;
static volatile bool s_reset_pending = false;
static volatile bool s_soft_reconnect_pending = false;
static struct message s_rx_header;
static adb_packet_t *s_rx_packet = NULL;

static uint8_t s_busid;
static struct usbd_endpoint s_ep_out;
static struct usbd_endpoint s_ep_in;

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX static uint8_t
    s_rx_msg_buf[USB_ALIGN_UP(ADB_MESSAGE_SIZE, CONFIG_USB_ALIGN_SIZE)];
USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX static uint8_t
    s_rx_data_buf[USB_ALIGN_UP(ADB_MAX_DROP_XFER_BUFSIZE, CONFIG_USB_ALIGN_SIZE)];
USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX static uint8_t
    s_tx_buf[USB_ALIGN_UP(ADB_EP_IN_BUFSIZE, CONFIG_USB_ALIGN_SIZE)];

#if defined(CONFIG_BOOT_ADB) && defined(CONFIG_BOOT_ADB_BACKEND_CHERRYUSB)
#define ADB_RX_DISPATCH_TASK_STACK_DEPTH 1024U
static StackType_t *s_rx_dispatch_stack = NULL;
static StaticTask_t *s_rx_dispatch_tcb = NULL;
#endif

static bool adb_dev_msg_validate(const struct message *msg)
{
    if (msg == NULL) {
        return false;
    }

    if (msg->magic != (msg->command ^ 0xffffffffU)) {
        ADB_LOGE("adb msg magic check failed, cmd:%lx, calc magic:%lx\n",
                 (unsigned long)msg->command,
                 (unsigned long)(msg->command ^ 0xffffffffU));
        return false;
    }

    if (msg->data_length > MAX_PAYLOAD) {
        ADB_LOGE("adb packet data length check failed, len:%lu\n",
                 (unsigned long)msg->data_length);
        return false;
    }

    return true;
}

static void adb_dev_dispatch_packet(adb_packet_t *packet)
{
    if (packet == NULL) {
        return;
    }

    if (s_recv_cb != NULL) {
        s_recv_cb(packet);
        return;
    }

    adb_packet_free(packet);
}

static void adb_dev_notify_dispatch_task(void)
{
    if (s_rx_dispatch_task == NULL) {
        return;
    }

    if (xPortIsInsideInterrupt()) {
        BaseType_t woken = pdFALSE;

        vTaskNotifyGiveFromISR(s_rx_dispatch_task, &woken);
        portYIELD_FROM_ISR(woken);
        return;
    }

    xTaskNotifyGive(s_rx_dispatch_task);
}

#if defined(CONFIG_BOOT_ADB) && defined(CONFIG_BOOT_ADB_BACKEND_CHERRYUSB)
static void adb_dev_soft_reconnect(void)
{
#ifdef USB_ARCS_POWER_SOFTCONN
    s_configured = false;
    s_configure_pending = false;
    IP_USBC->POWER &= ~USB_ARCS_POWER_SOFTCONN;
    vTaskDelay(pdMS_TO_TICKS(20));
    IP_USBC->POWER |= USB_ARCS_POWER_SOFTCONN;
#endif
}
#endif

static bool adb_dev_wait_tx_sem(void)
{
    if (xSemaphoreTake(s_tx_sem, pdMS_TO_TICKS(500)) == pdTRUE) {
        return true;
    }

    ADB_LOGW("ADB TX timeout\n");
    return false;
}

static void adb_dev_reset_rx_state(void)
{
    s_rx_nbytes = 0U;
    s_rx_payload_offset = 0U;
    s_rx_drop_remaining = 0U;
    s_rx_state = ADB_RX_STATE_MSG;

    if (s_rx_packet != NULL) {
        adb_packet_free(s_rx_packet);
        s_rx_packet = NULL;
    }
}

static void adb_dev_start_read(uint8_t *buf, uint32_t len)
{
    if (!s_configured || s_ep_out.ep_addr == 0U || buf == NULL || len == 0U) {
        return;
    }

    s_rx_nbytes = 0U;
    usbd_ep_start_read(s_busid, s_ep_out.ep_addr, buf, len);
}

static void adb_dev_arm_msg_read(void)
{
    s_rx_state = ADB_RX_STATE_MSG;
    s_rx_payload_offset = 0U;
    memset(s_rx_msg_buf, 0, sizeof(s_rx_msg_buf));
    adb_dev_start_read(s_rx_msg_buf, ADB_MESSAGE_SIZE);
}

static void adb_dev_arm_data_read(uint32_t remaining)
{
    uint32_t read_len = remaining;
    uint8_t *read_buf;

    if (s_rx_packet == NULL) {
        adb_dev_arm_msg_read();
        return;
    }

    if (read_len > ADB_MAX_DATA_XFER_BUFSIZE) {
        read_len = ADB_MAX_DATA_XFER_BUFSIZE;
    }

    read_buf = s_rx_packet->data + s_rx_payload_offset;
    s_rx_state = ADB_RX_STATE_DATA;
    adb_dev_start_read(read_buf, read_len);
}

static void adb_dev_arm_drop_read(uint32_t remaining)
{
    uint32_t read_len = remaining;

    if (read_len > ADB_MAX_DROP_XFER_BUFSIZE) {
        read_len = ADB_MAX_DROP_XFER_BUFSIZE;
    }

    s_rx_state = ADB_RX_STATE_DROP;
    adb_dev_start_read(s_rx_data_buf, read_len);
}

static void adb_dev_arm_wait_packet(void)
{
    s_rx_state = ADB_RX_STATE_WAIT_PACKET;
}

static void adb_dev_try_resume_wait_packet(void)
{
    adb_packet_t *packet;

    if (s_rx_state != ADB_RX_STATE_WAIT_PACKET) {
        return;
    }

    packet = adb_packet_alloc(s_rx_header.data_length);
    if (packet == NULL) {
        return;
    }

    memcpy(&packet->msg, &s_rx_header, sizeof(s_rx_header));
    s_rx_packet = packet;
    s_rx_payload_offset = 0U;

    if (packet->msg.data_length == 0U) {
        s_rx_packet = NULL;
        adb_dev_arm_msg_read();
        adb_dev_dispatch_packet(packet);
        return;
    }

    adb_dev_arm_data_read(packet->msg.data_length);
}

static void adb_dev_handle_msg_done(void)
{
    adb_packet_t *packet;

    if (s_rx_nbytes != ADB_MESSAGE_SIZE) {
        if (s_rx_nbytes != 0U) {
            ADB_LOGW("unexpected adb header size:%lu expected:%lu\n",
                     (unsigned long)s_rx_nbytes,
                     (unsigned long)ADB_MESSAGE_SIZE);
        }
        adb_dev_arm_msg_read();
        return;
    }

    memcpy(&s_rx_header, s_rx_msg_buf, sizeof(s_rx_header));
    if (!adb_dev_msg_validate(&s_rx_header)) {
        adb_dev_arm_msg_read();
        return;
    }


    packet = adb_packet_alloc(s_rx_header.data_length);
    if (packet == NULL) {
        ADB_LOGD("adb packet alloc failed, wait packet free, payload:%lu\n",
                 (unsigned long)s_rx_header.data_length);
        adb_dev_arm_wait_packet();
        if (s_rx_dispatch_task != NULL) {
            xTaskNotifyGive(s_rx_dispatch_task);
        }
        return;
    }

    memcpy(&packet->msg, &s_rx_header, sizeof(s_rx_header));
    s_rx_packet = packet;
    s_rx_payload_offset = 0U;

    if (packet->msg.data_length == 0U) {
        s_rx_packet = NULL;
        adb_dev_arm_msg_read();
        adb_dev_dispatch_packet(packet);
        return;
    }

    adb_dev_arm_data_read(packet->msg.data_length);
}

static void adb_dev_handle_data_done(void)
{
    uint32_t remaining;

    if (s_rx_packet == NULL) {
        adb_dev_arm_msg_read();
        return;
    }


    remaining = s_rx_packet->msg.data_length - s_rx_payload_offset;
    if (s_rx_nbytes > remaining) {
        ADB_LOGE("adb payload overflow, recv:%lu remaining:%lu\n",
                 (unsigned long)s_rx_nbytes,
                 (unsigned long)remaining);
        adb_packet_free(s_rx_packet);
        s_rx_packet = NULL;
        adb_dev_arm_msg_read();
        return;
    }

    s_rx_payload_offset += s_rx_nbytes;

    if (s_rx_payload_offset < s_rx_packet->msg.data_length) {
        adb_dev_arm_data_read(s_rx_packet->msg.data_length - s_rx_payload_offset);
        return;
    }

    adb_packet_t *packet = s_rx_packet;
    s_rx_packet = NULL;
    adb_dev_arm_msg_read();
    adb_dev_dispatch_packet(packet);
}

static void adb_dev_handle_drop_done(void)
{
    if (s_rx_nbytes > s_rx_drop_remaining) {
        s_rx_drop_remaining = 0U;
    } else {
        s_rx_drop_remaining -= s_rx_nbytes;
    }

    if (s_rx_drop_remaining == 0U) {
        adb_dev_arm_msg_read();
        return;
    }

    adb_dev_arm_drop_read(s_rx_drop_remaining);
}

static void adb_ep_out_cb(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)busid;
    (void)ep;

    if (!s_configured || s_reset_pending || s_configure_pending) {
        return;
    }

    s_rx_nbytes = nbytes;
    if (s_rx_dispatch_task != NULL) {
        BaseType_t woken = pdFALSE;

        vTaskNotifyGiveFromISR(s_rx_dispatch_task, &woken);
        portYIELD_FROM_ISR(woken);
    }
}

static void adb_rx_dispatch(void *arg)
{
    (void)arg;

    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (s_reset_pending) {
            s_reset_pending = false;
            adb_reset();
            adb_dev_reset_rx_state();
        }

#if defined(CONFIG_BOOT_ADB) && defined(CONFIG_BOOT_ADB_BACKEND_CHERRYUSB)
        if (s_soft_reconnect_pending && s_configured) {
            s_soft_reconnect_pending = false;
            adb_dev_soft_reconnect();
            continue;
        }
#endif

        if (s_configure_pending) {
            s_configure_pending = false;
            adb_dev_arm_msg_read();
            continue;
        }

        if (!s_configured) {
            continue;
        }

        switch (s_rx_state) {
        case ADB_RX_STATE_MSG:
            adb_dev_handle_msg_done();
            break;
        case ADB_RX_STATE_DATA:
            adb_dev_handle_data_done();
            break;
        case ADB_RX_STATE_DROP:
            adb_dev_handle_drop_done();
            break;
        case ADB_RX_STATE_WAIT_PACKET:
            adb_dev_try_resume_wait_packet();
            break;
        default:
            adb_dev_arm_msg_read();
            break;
        }
    }
}

static void adb_ep_in_cb(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)busid;
    (void)ep;

    if (nbytes == 0U) {
        ADB_LOGD("ZLP completed\n");
    }


    if (s_tx_sem != NULL) {
        BaseType_t woken = pdFALSE;

        xSemaphoreGiveFromISR(s_tx_sem, &woken);
        portYIELD_FROM_ISR(woken);
    }
}

static void adb_notify_handler(uint8_t busid, uint8_t event, void *arg)
{
    (void)arg;

    if (event == USBD_EVENT_CONFIGURED) {
        s_busid = busid;
        s_configured = true;
        s_configure_pending = true;
        adb_dev_notify_dispatch_task();
    } else if (event == USBD_EVENT_RESET) {
        if (s_configured) {
            s_soft_reconnect_pending = true;
        }
        s_configured = false;
        s_configure_pending = false;
        adb_dev_reset(0);
    }
}

struct usbd_interface *adb_dev_init_intf(uint8_t busid,
                                         struct usbd_interface *intf,
                                         uint8_t in_ep, uint8_t out_ep)
{
    s_busid = busid;

    intf->class_interface_handler = NULL;
    intf->class_endpoint_handler = NULL;
    intf->vendor_handler = NULL;
    intf->notify_handler = adb_notify_handler;

    s_ep_out.ep_addr = out_ep;
    s_ep_out.ep_cb = adb_ep_out_cb;
    s_ep_in.ep_addr = in_ep;
    s_ep_in.ep_cb = adb_ep_in_cb;

    usbd_add_endpoint(busid, &s_ep_out);
    usbd_add_endpoint(busid, &s_ep_in);

    if (s_tx_sem == NULL) {
        s_tx_sem = xSemaphoreCreateBinary();
        if (s_tx_sem == NULL) {
            ADB_LOGE("Failed to create ADB TX semaphore\n");
        } else {
            xSemaphoreGive(s_tx_sem);
        }
    }

    if (s_rx_dispatch_task == NULL) {
#if defined(CONFIG_BOOT_ADB) && defined(CONFIG_BOOT_ADB_BACKEND_CHERRYUSB)
        if (s_rx_dispatch_stack == NULL) {
            s_rx_dispatch_stack = inram_calloc(32, ADB_RX_DISPATCH_TASK_STACK_DEPTH, sizeof(StackType_t));
        }
        if (s_rx_dispatch_tcb == NULL) {
            s_rx_dispatch_tcb = inram_calloc(sizeof(void *), 1, sizeof(StaticTask_t));
        }
        if (s_rx_dispatch_stack != NULL && s_rx_dispatch_tcb != NULL) {
            s_rx_dispatch_task = xTaskCreateStatic(adb_rx_dispatch, "adb_rx_d",
                                                   ADB_RX_DISPATCH_TASK_STACK_DEPTH, NULL,
                                                   CONFIG_ADB_TASK_PRIORITY,
                                                   s_rx_dispatch_stack, s_rx_dispatch_tcb);
        }
        if (s_rx_dispatch_task == NULL) {
#else
        if (xTaskCreate(adb_rx_dispatch, "adb_rx_d", 1024, NULL,
                        CONFIG_ADB_TASK_PRIORITY, &s_rx_dispatch_task) != pdPASS) {
#endif
            ADB_LOGE("Failed to create ADB RX dispatch task\n");
        }
    }

    adb_dev_reset_rx_state();
    s_configured = false;
    s_configure_pending = false;
    s_soft_reconnect_pending = false;
    return intf;
}

void adb_dev_init(void)
{
    if (s_tx_sem != NULL) {
        return;
    }

    s_tx_sem = xSemaphoreCreateBinary();
    if (s_tx_sem == NULL) {
        ADB_LOGE("Failed to create ADB TX semaphore\n");
        return;
    }

    xSemaphoreGive(s_tx_sem);
}

bool adb_dev_deinit(void)
{
    s_recv_cb = NULL;
    adb_dev_reset_rx_state();
    s_configured = false;
    s_configure_pending = false;
    s_reset_pending = false;
    s_soft_reconnect_pending = false;

    if (s_rx_dispatch_task != NULL) {
        vTaskDelete(s_rx_dispatch_task);
        s_rx_dispatch_task = NULL;
    }

#if defined(CONFIG_BOOT_ADB) && defined(CONFIG_BOOT_ADB_BACKEND_CHERRYUSB)
    if (s_rx_dispatch_tcb != NULL) {
        inram_free(s_rx_dispatch_tcb);
        s_rx_dispatch_tcb = NULL;
    }
    if (s_rx_dispatch_stack != NULL) {
        inram_free(s_rx_dispatch_stack);
        s_rx_dispatch_stack = NULL;
    }
#endif

    if (s_tx_sem != NULL) {
        vSemaphoreDelete(s_tx_sem);
        s_tx_sem = NULL;
    }

    return true;
}

void adb_dev_reset(uint8_t rhport)
{
    (void)rhport;

    if (s_tx_sem != NULL) {
        if (xPortIsInsideInterrupt()) {
            BaseType_t woken = pdFALSE;

            xSemaphoreGiveFromISR(s_tx_sem, &woken);
            portYIELD_FROM_ISR(woken);
        } else {
            xSemaphoreGive(s_tx_sem);
        }
    }

    /* Defer service teardown to task context; close handlers are not ISR-safe. */
    s_reset_pending = true;
    adb_dev_notify_dispatch_task();
}

void adb_dev_recv_cb_set(adb_dev_recv_cb_t cb)
{
    s_recv_cb = cb;
}

void adb_dev_notify_packet_free(void)
{
    if (s_rx_state != ADB_RX_STATE_WAIT_PACKET || s_rx_dispatch_task == NULL) {
        return;
    }

    xTaskNotifyGive(s_rx_dispatch_task);
}


bool adb_dev_send(uint8_t *buf, uint32_t len)
{
    int ret;

    if (!adb_dev_wait_tx_sem()) {
        return false;
    }

    if (len > 0U && buf != NULL) {
        if (len > sizeof(s_tx_buf)) {
            xSemaphoreGive(s_tx_sem);
            ADB_LOGE("ADB TX len %lu exceeds buffer %u\n",
                     (unsigned long)len,
                     (unsigned)sizeof(s_tx_buf));
            return false;
        }

        memcpy(s_tx_buf, buf, len);
    }

    ret = usbd_ep_start_write(s_busid, s_ep_in.ep_addr, s_tx_buf, len);
    if (ret < 0) {
        xSemaphoreGive(s_tx_sem);
        ADB_LOGE("usbd_ep_start_write failed: %d\n", ret);
        return false;
    }

    return true;
}

/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define TAG "usb_audio"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "app_usb_cherry.h"
#include "lisa_log.h"
#include "sysheap.h"
#include "usbd_core.h"
#include "usbd_audio.h"

#define APP_USB_AUDIO_QUEUE_LENGTH 100U
#define APP_USB_AUDIO_TASK_STACK   1024U

struct app_usb_audio_data {
    uint8_t *data;
    uint32_t size;
};

static QueueHandle_t audio_queue;
static TaskHandle_t audio_task_handle;
static volatile bool host_stream_open;
static volatile bool is_recording;
static volatile bool audio_ep_busy;
static bool mute[APP_USB_AUDIO_CHANNELS + 1U];
static int volume_db[APP_USB_AUDIO_CHANNELS + 1U];
static uint32_t sample_rate = APP_USB_AUDIO_SAMPLE_RATE;

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX
static uint8_t audio_packet[APP_USB_AUDIO_PACKET_BYTES];

static void app_usb_audio_release(struct app_usb_audio_data *audio_data)
{
    if (audio_data->data != NULL) {
        psram_free(audio_data->data);
        audio_data->data = NULL;
        audio_data->size = 0U;
    }
}

static void app_usb_audio_discard_queue(void)
{
    struct app_usb_audio_data audio_data = { 0 };

    while (audio_queue != NULL &&
           xQueueReceive(audio_queue, &audio_data, 0) == pdPASS) {
        app_usb_audio_release(&audio_data);
    }
}

void app_usb_audio_start_recording(void)
{
    is_recording = true;
}

void app_usb_audio_stop_recording(void)
{
    is_recording = false;
}

bool app_usb_audio_is_recording(void)
{
    return is_recording;
}

int app_usb_audio_write(void *data, uint32_t sample, uint32_t channel, uint8_t bit)
{
    struct app_usb_audio_data audio_data;
    const uint16_t *input;
    uint16_t *output;

    if (audio_queue == NULL || data == NULL || sample == 0U ||
        channel < APP_USB_AUDIO_CHANNELS || bit != APP_USB_AUDIO_SAMPLE_BITS) {
        return -1;
    }

    audio_data.size = sample * APP_USB_AUDIO_CHANNELS * sizeof(uint16_t);
    audio_data.data = psram_malloc(audio_data.size);
    if (audio_data.data == NULL) {
        return -1;
    }

    input = data;
    output = (uint16_t *)audio_data.data;
    for (uint32_t i = 0; i < sample; i++) {
        for (uint32_t ch = 0; ch < APP_USB_AUDIO_CHANNELS; ch++) {
            output[i * APP_USB_AUDIO_CHANNELS + ch] = input[i * channel + ch];
        }
    }

    if (xQueueSend(audio_queue, &audio_data, 0) != pdPASS) {
        app_usb_audio_release(&audio_data);
        LISA_LOGE(TAG, "audio queue send failed");
        return -1;
    }

    return 0;
}

static void app_usb_audio_task(void *arg)
{
    struct app_usb_audio_data current = { 0 };
    uint32_t offset = 0U;

    (void)arg;

    while (1) {
        if (!host_stream_open || !is_recording) {
            app_usb_audio_release(&current);
            app_usb_audio_discard_queue();
            vTaskDelay(pdMS_TO_TICKS(2));
            continue;
        }

        if (current.data == NULL) {
            if (xQueueReceive(audio_queue, &current, pdMS_TO_TICKS(10)) != pdPASS) {
                continue;
            }
            offset = 0U;
        }

        if (audio_ep_busy) {
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(2));
            continue;
        }

        uint32_t remaining = current.size - offset;
        uint32_t packet_bytes = remaining;
        if (packet_bytes > sizeof(audio_packet)) {
            packet_bytes = sizeof(audio_packet);
        }

        memset(audio_packet, 0, sizeof(audio_packet));
        memcpy(audio_packet, current.data + offset, packet_bytes);

        audio_ep_busy = true;
        if (usbd_ep_start_write(APP_USB_BUS_ID, APP_USB_AUDIO_IN_EP,
                                audio_packet, sizeof(audio_packet)) < 0) {
            audio_ep_busy = false;
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }

        offset += packet_bytes;
        if (offset == current.size) {
            app_usb_audio_release(&current);
            offset = 0U;
        }
    }
}

static void app_usb_audio_in_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    BaseType_t yield = pdFALSE;

    (void)busid;
    (void)ep;
    (void)nbytes;

    audio_ep_busy = false;
    if (audio_task_handle != NULL) {
        vTaskNotifyGiveFromISR(audio_task_handle, &yield);
        portYIELD_FROM_ISR(yield);
    }
}

static struct usbd_endpoint audio_in_endpoint = {
    .ep_cb = app_usb_audio_in_callback,
    .ep_addr = APP_USB_AUDIO_IN_EP,
};

void app_usb_cherry_audio_register_endpoint(uint8_t busid)
{
    usbd_add_endpoint(busid, &audio_in_endpoint);
}

void app_usb_cherry_audio_on_disconnect(void)
{
    host_stream_open = false;
    is_recording = false;
    audio_ep_busy = false;
}

int app_usb_audio_run(void)
{
    audio_queue = xQueueCreate(APP_USB_AUDIO_QUEUE_LENGTH,
                               sizeof(struct app_usb_audio_data));
    if (audio_queue == NULL) {
        LISA_LOGE(TAG, "failed to create audio queue");
        return -1;
    }

    if (xTaskCreate(app_usb_audio_task, "usb_audio", APP_USB_AUDIO_TASK_STACK,
                    NULL, configMAX_PRIORITIES - 1,
                    &audio_task_handle) != pdPASS) {
        vQueueDelete(audio_queue);
        audio_queue = NULL;
        LISA_LOGE(TAG, "failed to create audio task");
        return -1;
    }

    return 0;
}

void usbd_audio_open(uint8_t busid, uint8_t intf)
{
    (void)busid;

    if (intf == APP_USB_AUDIO_STREAM_ITF) {
        audio_ep_busy = false;
        host_stream_open = true;
        app_usb_audio_start_recording();
    }
}

void usbd_audio_close(uint8_t busid, uint8_t intf)
{
    (void)busid;

    if (intf == APP_USB_AUDIO_STREAM_ITF) {
        host_stream_open = false;
        audio_ep_busy = false;
        app_usb_audio_stop_recording();
    }
}

void usbd_audio_set_volume(uint8_t busid, uint8_t ep, uint8_t channel, int value_db)
{
    (void)busid;

    if (ep == APP_USB_AUDIO_IN_EP && channel <= APP_USB_AUDIO_CHANNELS) {
        volume_db[channel] = value_db;
    }
}

int usbd_audio_get_volume(uint8_t busid, uint8_t ep, uint8_t channel)
{
    (void)busid;

    if (ep == APP_USB_AUDIO_IN_EP && channel <= APP_USB_AUDIO_CHANNELS) {
        return volume_db[channel];
    }
    return 0;
}

void usbd_audio_set_mute(uint8_t busid, uint8_t ep, uint8_t channel, bool value)
{
    (void)busid;

    if (ep == APP_USB_AUDIO_IN_EP && channel <= APP_USB_AUDIO_CHANNELS) {
        mute[channel] = value;
    }
}

bool usbd_audio_get_mute(uint8_t busid, uint8_t ep, uint8_t channel)
{
    (void)busid;

    if (ep == APP_USB_AUDIO_IN_EP && channel <= APP_USB_AUDIO_CHANNELS) {
        return mute[channel];
    }
    return false;
}

void usbd_audio_set_sampling_freq(uint8_t busid, uint8_t ep, uint32_t value)
{
    (void)busid;

    if (ep == APP_USB_AUDIO_IN_EP && value == APP_USB_AUDIO_SAMPLE_RATE) {
        sample_rate = value;
    }
}

uint32_t usbd_audio_get_sampling_freq(uint8_t busid, uint8_t ep)
{
    (void)busid;

    return ep == APP_USB_AUDIO_IN_EP ? sample_rate : 0U;
}

/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "uac_device.h"

#include <stdbool.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "sysheap.h"
#include "uac_descriptors.h"
#include "usb_device_config.h"
#undef MIN
#include "ring_buffer.h"
#include "usbd_audio.h"
#include "usbd_core.h"

#define TAG "uac"
#include "lisa_log.h"

static uac_stream_t *capture_stream;
static uac_stream_t *playback_stream;
static volatile bool mic_opened;
static volatile bool mic_ep_busy;
static volatile bool spk_opened;
static bool mic_capture_started;
static bool spk_play_started;
static bool mic_mute;
static bool spk_mute;
static int mic_volume_db = UAC_VOLUME_DEFAULT_DB;
static int spk_volume_db = UAC_VOLUME_DEFAULT_DB;
static volatile uint32_t mic_sample_rate = UAC_MIC_SAMPLE_RATE;
static volatile uint32_t spk_sample_rate = UAC_SPK_SAMPLE_RATE;
static struct ring_buf spk_ring;
static uint8_t *spk_ring_mem;
static TaskHandle_t mic_task_handle;

static uac_stream_t *stream_from_ep(uint8_t ep)
{
    return (ep == UAC_MIC_IN_EP) ? capture_stream : playback_stream;
}

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX static uint8_t mic_packet[UAC_MIC_PACKET_BYTES];
USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX static uint8_t spk_usb_packet[UAC_SPK_PACKET_BYTES];
static uint8_t spk_play_packet[UAC_SPK_PLAY_BYTES];
static uint8_t spk_silence_packet[UAC_SPK_PLAY_BYTES];

static void reset_stream_state_from_isr(void)
{
    if (spk_ring_mem != NULL) {
        UBaseType_t saved = taskENTER_CRITICAL_FROM_ISR();
        ring_buf_reset(&spk_ring);
        taskEXIT_CRITICAL_FROM_ISR(saved);
    }
}

static void push_speaker_packet_from_isr(const uint8_t *data, uint32_t len)
{
    UBaseType_t saved = taskENTER_CRITICAL_FROM_ISR();

    if (len > ring_buf_space_get(&spk_ring)) {
        uint8_t drop_buf[32];
        uint32_t drop = len - ring_buf_space_get(&spk_ring);

        while (drop > 0U) {
            uint32_t chunk = (drop > sizeof(drop_buf)) ? sizeof(drop_buf) : drop;
            uint32_t got = ring_buf_get(&spk_ring, drop_buf, chunk);
            if (got == 0U) {
                break;
            }
            drop -= got;
        }
    }

    (void)ring_buf_put(&spk_ring, data, len);
    taskEXIT_CRITICAL_FROM_ISR(saved);
}

static uint32_t pop_speaker_packet(uint8_t *data, uint32_t len)
{
    uint32_t copied;

    taskENTER_CRITICAL();
    copied = ring_buf_get(&spk_ring, data, len);
    taskEXIT_CRITICAL();

    return copied;
}

void uac_stream_on_disconnect(void)
{
    mic_opened = false;
    spk_opened = false;
    mic_ep_busy = false;
    reset_stream_state_from_isr();
}

static void notify_mic_task_from_isr(void)
{
    if (mic_task_handle != NULL) {
        BaseType_t yield = pdFALSE;
        vTaskNotifyGiveFromISR(mic_task_handle, &yield);
        portYIELD_FROM_ISR(yield);
    }
}

void usbd_audio_open(uint8_t busid, uint8_t intf)
{
    (void)busid;
    if (intf == UAC_ITF_MIC) {
        mic_ep_busy = false;
        mic_opened = true;
        notify_mic_task_from_isr();
        LISA_LOGI(TAG, "UAC mic opened");
    } else if (intf == UAC_ITF_SPK) {
        reset_stream_state_from_isr();
        spk_opened = true;
        (void)usbd_ep_start_read(USB_DEVICE_BUS_ID, UAC_SPK_OUT_EP, spk_usb_packet, sizeof(spk_usb_packet));
        LISA_LOGI(TAG, "UAC speaker opened");
    }
}

void usbd_audio_close(uint8_t busid, uint8_t intf)
{
    (void)busid;
    if (intf == UAC_ITF_MIC) {
        mic_opened = false;
        mic_ep_busy = false;
        LISA_LOGI(TAG, "UAC mic closed");
    } else if (intf == UAC_ITF_SPK) {
        spk_opened = false;
        reset_stream_state_from_isr();
        LISA_LOGI(TAG, "UAC speaker closed");
    }
}

void usbd_audio_set_volume(uint8_t busid, uint8_t ep, uint8_t ch, int volume_db)
{
    (void)busid;
    uac_stream_t *stream = stream_from_ep(ep);

    if (ep == UAC_MIC_IN_EP) {
        mic_volume_db = volume_db;
    } else if (ep == UAC_SPK_OUT_EP) {
        spk_volume_db = volume_db;
    } else {
        return;
    }

    if (stream != NULL && stream->ops != NULL && stream->ops->set_volume != NULL) {
        stream->ops->set_volume(stream, ch, volume_db);
    }
    LISA_LOGI(TAG, "volume ep 0x%02x ch%u %d dB", ep, ch, volume_db);
}

int usbd_audio_get_volume(uint8_t busid, uint8_t ep, uint8_t ch)
{
    (void)busid;
    uac_stream_t *stream = stream_from_ep(ep);

    if (ep != UAC_MIC_IN_EP && ep != UAC_SPK_OUT_EP) {
        return 0;
    }

    if (stream != NULL && stream->ops != NULL && stream->ops->get_volume != NULL) {
        return stream->ops->get_volume(stream, ch);
    }

    return (ep == UAC_MIC_IN_EP) ? mic_volume_db : spk_volume_db;
}

void usbd_audio_set_mute(uint8_t busid, uint8_t ep, uint8_t ch, bool mute)
{
    (void)busid;
    uac_stream_t *stream = stream_from_ep(ep);

    if (ep == UAC_MIC_IN_EP) {
        mic_mute = mute;
    } else if (ep == UAC_SPK_OUT_EP) {
        spk_mute = mute;
    } else {
        return;
    }

    if (stream != NULL && stream->ops != NULL && stream->ops->set_mute != NULL) {
        stream->ops->set_mute(stream, ch, mute);
    }
    LISA_LOGI(TAG, "mute ep 0x%02x ch%u %s", ep, ch, mute ? "on" : "off");
}

bool usbd_audio_get_mute(uint8_t busid, uint8_t ep, uint8_t ch)
{
    (void)busid;
    uac_stream_t *stream = stream_from_ep(ep);

    if (ep != UAC_MIC_IN_EP && ep != UAC_SPK_OUT_EP) {
        return false;
    }

    if (stream != NULL && stream->ops != NULL && stream->ops->get_mute != NULL) {
        return stream->ops->get_mute(stream, ch);
    }

    return (ep == UAC_MIC_IN_EP) ? mic_mute : spk_mute;
}

void usbd_audio_set_sampling_freq(uint8_t busid, uint8_t ep, uint32_t sampling_freq)
{
    (void)busid;
    uac_stream_t *stream = stream_from_ep(ep);

    if (ep == UAC_MIC_IN_EP) {
        mic_sample_rate = sampling_freq;
    } else if (ep == UAC_SPK_OUT_EP) {
        spk_sample_rate = sampling_freq;
    } else {
        return;
    }

    if (stream != NULL && stream->ops != NULL && stream->ops->set_sampling_freq != NULL) {
        stream->ops->set_sampling_freq(stream, sampling_freq);
    }
}

uint32_t usbd_audio_get_sampling_freq(uint8_t busid, uint8_t ep)
{
    (void)busid;
    uac_stream_t *stream = stream_from_ep(ep);

    if (ep != UAC_MIC_IN_EP && ep != UAC_SPK_OUT_EP) {
        return 0;
    }

    if (stream != NULL && stream->ops != NULL && stream->ops->get_sampling_freq != NULL) {
        return stream->ops->get_sampling_freq(stream);
    }

    return (ep == UAC_MIC_IN_EP) ? mic_sample_rate : spk_sample_rate;
}

static void usbd_audio_in_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)busid;
    (void)ep;
    (void)nbytes;
    mic_ep_busy = false;
    notify_mic_task_from_isr();
}

static void usbd_audio_out_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)ep;
    if (spk_opened && nbytes > 0) {
        push_speaker_packet_from_isr(spk_usb_packet, nbytes);
    }
    if (spk_opened) {
        (void)usbd_ep_start_read(busid, UAC_SPK_OUT_EP, spk_usb_packet, sizeof(spk_usb_packet));
    }
}

static struct usbd_endpoint audio_in_ep = { .ep_cb = usbd_audio_in_callback, .ep_addr = UAC_MIC_IN_EP };
static struct usbd_endpoint audio_out_ep = { .ep_cb = usbd_audio_out_callback, .ep_addr = UAC_SPK_OUT_EP };

void uac_stream_register_endpoints(uint8_t busid)
{
    usbd_add_endpoint(busid, &audio_in_ep);
    usbd_add_endpoint(busid, &audio_out_ep);
}

static void mic_task(void *arg)
{
    (void)arg;
    while (1) {
        if (!mic_opened) {
            if (mic_capture_started) {
                if (capture_stream != NULL && capture_stream->ops != NULL && capture_stream->ops->stop != NULL) {
                    capture_stream->ops->stop(capture_stream);
                }
                mic_capture_started = false;
            }
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10));
            continue;
        }

        if (!mic_capture_started) {
            if (capture_stream != NULL && capture_stream->ops != NULL && capture_stream->ops->start != NULL) {
                int ret = capture_stream->ops->start(capture_stream);
                if (ret != 0) {
                    LISA_LOGE(TAG, "capture start failed: %d", ret);
                    vTaskDelay(pdMS_TO_TICKS(10));
                    continue;
                }
            }
            mic_capture_started = true;
        }

        if (mic_ep_busy) {
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(2));
            continue;
        }

        uint32_t copied = 0;
        if (!mic_mute && capture_stream != NULL && capture_stream->ops != NULL && capture_stream->ops->read != NULL) {
            copied = capture_stream->ops->read(capture_stream, mic_packet, sizeof(mic_packet));
        }
        if (copied < sizeof(mic_packet)) {
            memset(&mic_packet[copied], 0, sizeof(mic_packet) - copied);
        }

        mic_ep_busy = true;
        int ret = usbd_ep_start_write(USB_DEVICE_BUS_ID, UAC_MIC_IN_EP, mic_packet, sizeof(mic_packet));
        if (ret < 0) {
            mic_ep_busy = false;
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
}

static void speaker_task(void *arg)
{
    (void)arg;

    while (1) {
        if (!spk_opened) {
            if (spk_play_started) {
                if (playback_stream != NULL && playback_stream->ops != NULL && playback_stream->ops->stop != NULL) {
                    playback_stream->ops->stop(playback_stream);
                }
                spk_play_started = false;
            }
            vTaskDelay(pdMS_TO_TICKS(2));
            continue;
        }

        if (!spk_play_started) {
            if (playback_stream != NULL && playback_stream->ops != NULL && playback_stream->ops->write != NULL) {
                for (uint32_t i = 0; i < 3U; i++) {
                    (void)playback_stream->ops->write(playback_stream, spk_silence_packet, sizeof(spk_silence_packet));
                }
            }
            if (playback_stream != NULL && playback_stream->ops != NULL && playback_stream->ops->start != NULL) {
                int ret = playback_stream->ops->start(playback_stream);
                if (ret != 0) {
                    LISA_LOGE(TAG, "speaker start failed: %d", ret);
                    vTaskDelay(pdMS_TO_TICKS(10));
                    continue;
                }
            }
            spk_play_started = true;
        }

        uint32_t copied = pop_speaker_packet(spk_play_packet, sizeof(spk_play_packet));
        if (copied < sizeof(spk_play_packet)) {
            memset(&spk_play_packet[copied], 0, sizeof(spk_play_packet) - copied);
        }

        if (playback_stream == NULL || playback_stream->ops == NULL || playback_stream->ops->write == NULL) {
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }

        const uint8_t *play_data = spk_mute ? spk_silence_packet : spk_play_packet;
        uint32_t written = playback_stream->ops->write(playback_stream, play_data, sizeof(spk_play_packet));
        if (written == 0U) {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
}

int uac_stream_init(const uac_device_config_t *config)
{
    capture_stream = config->capture;
    playback_stream = config->playback;

    spk_ring_mem = (uint8_t *)exram_malloc(4, UAC_SPK_RING_BYTES);
    if (spk_ring_mem == NULL) {
        LISA_LOGE(TAG, "failed to allocate speaker ring: %u bytes", (unsigned int)UAC_SPK_RING_BYTES);
        return -1;
    }
    ring_buf_init(&spk_ring, UAC_SPK_RING_BYTES, spk_ring_mem);

    if (xTaskCreate(mic_task, "uac_mic", UAC_TASK_STACK, NULL,
                    UAC_MIC_TASK_PRIORITY, &mic_task_handle) != pdPASS) {
        LISA_LOGE(TAG, "failed to create mic task");
        return -1;
    }

    if (xTaskCreate(speaker_task, "uac_spk", UAC_TASK_STACK, NULL,
                    UAC_SPK_TASK_PRIORITY, NULL) != pdPASS) {
        LISA_LOGE(TAG, "failed to create speaker task");
        return -1;
    }

    return 0;
}

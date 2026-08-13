/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "uac_device.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "ring_buffer.h"
#include "semphr.h"
#include "sysheap.h"
#include "task.h"
#include "tusb.h"
#include "usb_descriptors.h"

#if defined(CONFIG_SOC_VENUSA)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#include "ClockManager.h"
#pragma GCC diagnostic pop
#endif

#define TAG "uac"
#include "lisa_log.h"

#define ITF_NUM_AUDIO_CONTROL 0U
#define ITF_NUM_MIC_STREAMING 1U
#define ITF_NUM_SPK_STREAMING 2U

static TaskHandle_t mic_usb_task_handle;
static TaskHandle_t spk_usb_task_handle;
static TaskHandle_t speaker_task_handle;
static SemaphoreHandle_t tinyusb_audio_mutex;
static uac_stream_t *capture_stream;
static uac_stream_t *playback_stream;
static volatile bool mic_streaming;
static volatile bool spk_streaming;
static volatile bool spk_started;
static volatile bool spk_feedback_started;
static int mic_volume_db;
static int spk_volume_db;
static uint32_t mic_sample_rate = UAC_MIC_SAMPLE_RATE;
static uint32_t spk_sample_rate = UAC_SPK_SAMPLE_RATE;
static uint8_t spk_read_buf[UAC_SPK_PACKET_BYTES * 2U];
static uint8_t spk_play_buf[UAC_SPK_PLAY_BYTES];
static uint8_t spk_silence_buf[UAC_SPK_PLAY_BYTES];
static uint8_t mic_packet[UAC_MIC_WRITE_BYTES];
static struct ring_buf spk_ring;
static uint8_t *spk_ring_mem;

static void board_usb_device_init(void)
{
#if defined(CONFIG_SOC_VENUSA)
    __HAL_CRM_USB_CLK_ENABLE();
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBC_CFG_IDDIG = 0x1;
    IP_CMN_SYS->REG_USB_CTRL1.bit.UTMI_DATABUS16_8 = 0x1;
#endif
}

static void usb_device_task(void *param)
{
    (void)param;

    tusb_rhport_init_t dev_init = {
        .role = TUSB_ROLE_DEVICE,
        .speed = TUSB_SPEED_AUTO,
    };

    board_usb_device_init();
    tud_disconnect();
    tusb_init(BOARD_TUD_RHPORT, &dev_init);
    tud_connect();

    while (1) {
        tud_task();
    }
}

typedef enum {
    STREAM_KIND_MIC,
    STREAM_KIND_SPK,
} stream_kind_t;

static stream_kind_t request_stream_kind(const audio_control_request_t *request)
{
    switch (request->bEntityID) {
    case UAC_SPK_CLK_ID:
    case UAC_SPK_IT_ID:
    case UAC_SPK_FU_ID:
    case UAC_SPK_OT_ID:
        return STREAM_KIND_SPK;
    default:
        return STREAM_KIND_MIC;
    }
}

static uac_stream_t *stream_from_kind(stream_kind_t kind)
{
    return (kind == STREAM_KIND_MIC) ? capture_stream : playback_stream;
}

static uint32_t stream_get_sampling_freq(stream_kind_t kind)
{
    uac_stream_t *stream = stream_from_kind(kind);
    if (stream != NULL && stream->ops != NULL && stream->ops->get_sampling_freq != NULL) {
        return stream->ops->get_sampling_freq(stream);
    }
    return (kind == STREAM_KIND_MIC) ? mic_sample_rate : spk_sample_rate;
}

#if CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP
static uint32_t speaker_feedback_value(void)
{
    return (uint32_t)(((uint64_t)stream_get_sampling_freq(STREAM_KIND_SPK) << 16) / 1000U);
}
#endif

static void reset_speaker_ring(void)
{
    if (spk_ring_mem == NULL) {
        return;
    }

    taskENTER_CRITICAL();
    ring_buf_reset(&spk_ring);
    taskEXIT_CRITICAL();
}

static void push_speaker_data(const uint8_t *data, uint32_t len)
{
    if (spk_ring_mem == NULL || data == NULL || len == 0U) {
        return;
    }

    taskENTER_CRITICAL();
    while (len > ring_buf_space_get(&spk_ring)) {
        uint8_t drop[UAC_SPK_PACKET_BYTES];
        uint32_t need_drop = len - ring_buf_space_get(&spk_ring);
        uint32_t chunk = (need_drop > sizeof(drop)) ? sizeof(drop) : need_drop;
        if (ring_buf_get(&spk_ring, drop, chunk) == 0U) {
            break;
        }
    }
    (void)ring_buf_put(&spk_ring, data, len);
    taskEXIT_CRITICAL();

    if (speaker_task_handle != NULL) {
        xTaskNotifyGive(speaker_task_handle);
    }
}

static uint32_t pop_speaker_data(uint8_t *data, uint32_t len)
{
    if (spk_ring_mem == NULL || data == NULL || len == 0U) {
        return 0;
    }

    taskENTER_CRITICAL();
    uint32_t copied = ring_buf_get(&spk_ring, data, len);
    taskEXIT_CRITICAL();
    return copied;
}

static void tinyusb_audio_lock(void)
{
    if (tinyusb_audio_mutex != NULL) {
        (void)xSemaphoreTake(tinyusb_audio_mutex, portMAX_DELAY);
    }
}

static void tinyusb_audio_unlock(void)
{
    if (tinyusb_audio_mutex != NULL) {
        (void)xSemaphoreGive(tinyusb_audio_mutex);
    }
}

static bool stream_get_mute(stream_kind_t kind, uint8_t channel)
{
    uac_stream_t *stream = stream_from_kind(kind);
    if (stream != NULL && stream->ops != NULL && stream->ops->get_mute != NULL) {
        return stream->ops->get_mute(stream, channel);
    }
    return false;
}

static void stream_set_mute(stream_kind_t kind, uint8_t channel, bool mute)
{
    uac_stream_t *stream = stream_from_kind(kind);
    if (stream != NULL && stream->ops != NULL && stream->ops->set_mute != NULL) {
        stream->ops->set_mute(stream, channel, mute);
    }
}

static int stream_get_volume_db(stream_kind_t kind, uint8_t channel)
{
    uac_stream_t *stream = stream_from_kind(kind);
    if (stream != NULL && stream->ops != NULL && stream->ops->get_volume != NULL) {
        return stream->ops->get_volume(stream, channel);
    }
    return (kind == STREAM_KIND_MIC) ? mic_volume_db : spk_volume_db;
}

static void stream_set_volume_db(stream_kind_t kind, uint8_t channel, int volume_db)
{
    uac_stream_t *stream = stream_from_kind(kind);
    if (kind == STREAM_KIND_MIC) {
        mic_volume_db = volume_db;
    } else {
        spk_volume_db = volume_db;
    }

    if (stream != NULL && stream->ops != NULL && stream->ops->set_volume != NULL) {
        stream->ops->set_volume(stream, channel, volume_db);
    }
}

static bool clock_get_request(uint8_t rhport, const audio_control_request_t *request)
{
    uint32_t rate = stream_get_sampling_freq(request_stream_kind(request));

    if (request->bControlSelector == AUDIO_CS_CTRL_SAM_FREQ) {
        if (request->bRequest == AUDIO_CS_REQ_CUR) {
            audio_control_cur_4_t cur = { .bCur = (int32_t)rate };
            return tud_audio_buffer_and_schedule_control_xfer(rhport, (const tusb_control_request_t *)request,
                                                              &cur, sizeof(cur));
        }
        if (request->bRequest == AUDIO_CS_REQ_RANGE) {
            audio_control_range_4_n_t(1) range = {
                .wNumSubRanges = tu_htole16(1),
                .subrange = {{ .bMin = (int32_t)rate, .bMax = (int32_t)rate, .bRes = 0 }},
            };
            return tud_audio_buffer_and_schedule_control_xfer(rhport, (const tusb_control_request_t *)request,
                                                              &range, sizeof(range));
        }
    } else if (request->bControlSelector == AUDIO_CS_CTRL_CLK_VALID && request->bRequest == AUDIO_CS_REQ_CUR) {
        audio_control_cur_1_t valid = { .bCur = 1 };
        return tud_audio_buffer_and_schedule_control_xfer(rhport, (const tusb_control_request_t *)request,
                                                          &valid, sizeof(valid));
    }

    return false;
}

static bool feature_get_request(uint8_t rhport, const audio_control_request_t *request)
{
    stream_kind_t kind = request_stream_kind(request);
    uint8_t ch = request->bChannelNumber;
    uint8_t max_ch = (kind == STREAM_KIND_MIC) ? UAC_MIC_CHANNELS : UAC_SPK_CHANNELS;

    if (ch > max_ch) {
        return false;
    }

    if (request->bControlSelector == AUDIO_FU_CTRL_MUTE && request->bRequest == AUDIO_CS_REQ_CUR) {
        audio_control_cur_1_t cur = { .bCur = (int8_t)stream_get_mute(kind, ch) };
        return tud_audio_buffer_and_schedule_control_xfer(rhport, (const tusb_control_request_t *)request,
                                                          &cur, sizeof(cur));
    }

    if (request->bControlSelector == AUDIO_FU_CTRL_VOLUME) {
        if (request->bRequest == AUDIO_CS_REQ_CUR) {
            audio_control_cur_2_t cur = { .bCur = tu_htole16((int16_t)(stream_get_volume_db(kind, ch) * 256)) };
            return tud_audio_buffer_and_schedule_control_xfer(rhport, (const tusb_control_request_t *)request,
                                                              &cur, sizeof(cur));
        }
        if (request->bRequest == AUDIO_CS_REQ_RANGE) {
            audio_control_range_2_n_t(1) range = {
                .wNumSubRanges = tu_htole16(1),
                .subrange = {{ .bMin = tu_htole16(UAC_VOLUME_MIN_DB_256),
                               .bMax = tu_htole16(UAC_VOLUME_MAX_DB_256),
                               .bRes = tu_htole16(UAC_VOLUME_RES_DB_256) }},
            };
            return tud_audio_buffer_and_schedule_control_xfer(rhport, (const tusb_control_request_t *)request,
                                                              &range, sizeof(range));
        }
    }

    return false;
}

static bool terminal_get_request(uint8_t rhport, const audio_control_request_t *request)
{
    if (request->bControlSelector != AUDIO_TE_CTRL_CONNECTOR || request->bRequest != AUDIO_CS_REQ_CUR) {
        return false;
    }

    stream_kind_t kind = request_stream_kind(request);
    audio_desc_channel_cluster_t cluster = {
        .bNrChannels = (uint8_t)((kind == STREAM_KIND_MIC) ? UAC_MIC_CHANNELS : UAC_SPK_CHANNELS),
        .bmChannelConfig = tu_htole32((kind == STREAM_KIND_MIC) ? UAC_CH_ENABLE(UAC_MIC_CHANNELS) : UAC_CH_ENABLE(UAC_SPK_CHANNELS)),
        .iChannelNames = 0,
    };

    return tud_audio_buffer_and_schedule_control_xfer(rhport, (const tusb_control_request_t *)request,
                                                      &cluster, sizeof(cluster));
}

bool tud_audio_get_req_entity_cb(uint8_t rhport, const tusb_control_request_t *p_request)
{
    const audio_control_request_t *request = (const audio_control_request_t *)p_request;

    if (request->bEntityID == UAC_MIC_CLK_ID || request->bEntityID == UAC_SPK_CLK_ID) {
        return clock_get_request(rhport, request);
    }
    if (request->bEntityID == UAC_MIC_FU_ID || request->bEntityID == UAC_SPK_FU_ID) {
        return feature_get_request(rhport, request);
    }
    if (request->bEntityID == UAC_MIC_IT_ID || request->bEntityID == UAC_MIC_OT_ID ||
        request->bEntityID == UAC_SPK_IT_ID || request->bEntityID == UAC_SPK_OT_ID) {
        return terminal_get_request(rhport, request);
    }

    return false;
}

static bool clock_set_request(const audio_control_request_t *request, const uint8_t *buf)
{
    if (request->bRequest != AUDIO_CS_REQ_CUR || request->bControlSelector != AUDIO_CS_CTRL_SAM_FREQ ||
        request->wLength != sizeof(audio_control_cur_4_t)) {
        return false;
    }

    stream_kind_t kind = request_stream_kind(request);
    uint32_t rate = (uint32_t)((const audio_control_cur_4_t *)buf)->bCur;
    uac_stream_t *stream = stream_from_kind(kind);
    uint32_t current_rate = stream_get_sampling_freq(kind);

    if (rate != current_rate) {
        LISA_LOGW(TAG, "%s fixed sample rate %u Hz, reject %u Hz",
                  (kind == STREAM_KIND_MIC) ? "mic" : "speaker",
                  (unsigned int)current_rate, (unsigned int)rate);
        return false;
    }

    if (kind == STREAM_KIND_MIC) {
        mic_sample_rate = rate;
    } else {
        spk_sample_rate = rate;
    }

    if (stream != NULL && stream->ops != NULL && stream->ops->set_sampling_freq != NULL) {
        stream->ops->set_sampling_freq(stream, rate);
    }
    return true;
}

static bool feature_set_request(const audio_control_request_t *request, const uint8_t *buf)
{
    stream_kind_t kind = request_stream_kind(request);
    uint8_t ch = request->bChannelNumber;
    uint8_t max_ch = (kind == STREAM_KIND_MIC) ? UAC_MIC_CHANNELS : UAC_SPK_CHANNELS;

    if (ch > max_ch || request->bRequest != AUDIO_CS_REQ_CUR) {
        return false;
    }

    if (request->bControlSelector == AUDIO_FU_CTRL_MUTE && request->wLength == sizeof(audio_control_cur_1_t)) {
        stream_set_mute(kind, ch, ((const audio_control_cur_1_t *)buf)->bCur != 0);
        return true;
    }
    if (request->bControlSelector == AUDIO_FU_CTRL_VOLUME && request->wLength == sizeof(audio_control_cur_2_t)) {
        int16_t raw_volume = (int16_t)tu_le16toh(((const audio_control_cur_2_t *)buf)->bCur);
        stream_set_volume_db(kind, ch, raw_volume / 256);
        return true;
    }

    return false;
}

bool tud_audio_set_req_entity_cb(uint8_t rhport, const tusb_control_request_t *p_request, uint8_t *buf)
{
    (void)rhport;
    const audio_control_request_t *request = (const audio_control_request_t *)p_request;

    if (request->bEntityID == UAC_MIC_CLK_ID || request->bEntityID == UAC_SPK_CLK_ID) {
        return clock_set_request(request, buf);
    }
    if (request->bEntityID == UAC_MIC_FU_ID || request->bEntityID == UAC_SPK_FU_ID) {
        return feature_set_request(request, buf);
    }

    return false;
}

bool tud_audio_set_req_ep_cb(uint8_t rhport, const tusb_control_request_t *request, uint8_t *buf)
{
    (void)rhport;
    (void)request;
    (void)buf;
    return false;
}

bool tud_audio_set_req_itf_cb(uint8_t rhport, const tusb_control_request_t *request, uint8_t *buf)
{
    (void)rhport;
    (void)request;
    (void)buf;
    return false;
}

bool tud_audio_get_req_ep_cb(uint8_t rhport, const tusb_control_request_t *request)
{
    (void)rhport;
    (void)request;
    return false;
}

bool tud_audio_get_req_itf_cb(uint8_t rhport, const tusb_control_request_t *request)
{
    (void)rhport;
    (void)request;
    return false;
}

static void set_streaming_state(uint8_t itf, bool opened)
{
    if (itf == ITF_NUM_MIC_STREAMING) {
        mic_streaming = opened;
        if (capture_stream != NULL && capture_stream->ops != NULL) {
            if (opened && capture_stream->ops->start != NULL) {
                (void)capture_stream->ops->start(capture_stream);
            } else if (!opened && capture_stream->ops->stop != NULL) {
                capture_stream->ops->stop(capture_stream);
            }
        }
        if (mic_usb_task_handle != NULL) {
            xTaskNotifyGive(mic_usb_task_handle);
        }
        LISA_LOGI(TAG, "mic %s", opened ? "opened" : "closed");
    } else if (itf == ITF_NUM_SPK_STREAMING) {
        spk_streaming = opened;
        if (!opened) {
            spk_feedback_started = false;
            reset_speaker_ring();
            if (speaker_task_handle != NULL) {
                xTaskNotifyGive(speaker_task_handle);
            }
        }
        if (spk_usb_task_handle != NULL) {
            xTaskNotifyGive(spk_usb_task_handle);
        }
        LISA_LOGI(TAG, "speaker %s", opened ? "opened" : "closed");
    }
}

bool tud_audio_set_itf_cb(uint8_t rhport, const tusb_control_request_t *request)
{
    (void)rhport;
    uint8_t itf = tu_u16_low(tu_le16toh(request->wIndex));
    uint8_t alt = tu_u16_low(tu_le16toh(request->wValue));

    set_streaming_state(itf, alt != 0U);

    if (mic_usb_task_handle != NULL) {
        xTaskNotifyGive(mic_usb_task_handle);
    }
    if (spk_usb_task_handle != NULL) {
        xTaskNotifyGive(spk_usb_task_handle);
    }
    return true;
}

bool tud_audio_set_itf_close_EP_cb(uint8_t rhport, const tusb_control_request_t *request)
{
    return tud_audio_set_itf_cb(rhport, request);
}

bool tud_audio_rx_done_post_read_cb(uint8_t rhport, uint16_t n_bytes_received, uint8_t func_id,
                                    uint8_t ep_out, uint8_t cur_alt_setting)
{
    (void)rhport;
    (void)n_bytes_received;
    (void)func_id;
    (void)ep_out;
    (void)cur_alt_setting;

    if (spk_usb_task_handle != NULL) {
        xTaskNotifyGive(spk_usb_task_handle);
    }
    return true;
}

bool tud_audio_tx_done_pre_load_cb(uint8_t rhport, uint8_t func_id, uint8_t ep_in, uint8_t cur_alt_setting)
{
    (void)rhport;
    (void)func_id;
    (void)ep_in;
    (void)cur_alt_setting;
    if (mic_usb_task_handle != NULL) {
        xTaskNotifyGive(mic_usb_task_handle);
    }
    return true;
}

bool tud_audio_tx_done_post_load_cb(uint8_t rhport, uint16_t n_bytes_copied, uint8_t func_id,
                                    uint8_t ep_in, uint8_t cur_alt_setting)
{
    (void)rhport;
    (void)n_bytes_copied;
    (void)func_id;
    (void)ep_in;
    (void)cur_alt_setting;
    return true;
}

#if CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP
void tud_audio_feedback_params_cb(uint8_t func_id, uint8_t alt_itf, audio_feedback_params_t *feedback_param)
{
    (void)alt_itf;
    if (func_id == UAC_SPK_FUNC_ID) {
        feedback_param->method = AUDIO_FEEDBACK_METHOD_DISABLED;
        feedback_param->sample_freq = stream_get_sampling_freq(STREAM_KIND_SPK);
    }
}
#endif

void tud_mount_cb(void)
{
    LISA_LOGI(TAG, "USB mounted");
}

void tud_umount_cb(void)
{
    set_streaming_state(ITF_NUM_MIC_STREAMING, false);
    set_streaming_state(ITF_NUM_SPK_STREAMING, false);
    LISA_LOGI(TAG, "USB unmounted");
}

void tud_suspend_cb(bool remote_wakeup_en)
{
    (void)remote_wakeup_en;
    LISA_LOGI(TAG, "USB suspended");
}

void tud_resume_cb(void)
{
    LISA_LOGI(TAG, "USB resumed");
}

static bool uac_device_config_valid(const uac_device_config_t *config)
{
    return config != NULL &&
           config->capture != NULL &&
           config->capture->dir == UAC_STREAM_CAPTURE &&
           config->capture->ops != NULL &&
           config->capture->ops->read != NULL &&
           config->playback != NULL &&
           config->playback->dir == UAC_STREAM_PLAYBACK &&
           config->playback->ops != NULL &&
           config->playback->ops->write != NULL;
}

static void mic_usb_task(void *param)
{
    (void)param;
    const TickType_t mic_period = pdMS_TO_TICKS(CONFIG_UAC_MIC_PERIOD_MS);
    TickType_t next_mic_tick = xTaskGetTickCount();

    while (1) {
        TickType_t now = xTaskGetTickCount();

        if (mic_streaming && (int32_t)(now - next_mic_tick) >= 0) {
            uint32_t bytes = capture_stream->ops->read(capture_stream, mic_packet, sizeof(mic_packet));
            if (bytes < sizeof(mic_packet)) {
                memset(&mic_packet[bytes], 0, sizeof(mic_packet) - bytes);
                bytes = sizeof(mic_packet);
            }
            tinyusb_audio_lock();
            (void)tud_audio_n_write(UAC_MIC_FUNC_ID, mic_packet, (uint16_t)bytes);
            tinyusb_audio_unlock();
            next_mic_tick += mic_period;
        } else if (!mic_streaming) {
            next_mic_tick = now + mic_period;
        }

        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(CONFIG_UAC_MIC_PERIOD_MS));
    }
}

static void speaker_usb_task(void *param)
{
    (void)param;

    while (1) {
        if (spk_streaming) {
#if CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP
            if (!spk_feedback_started) {
                tinyusb_audio_lock();
                spk_feedback_started = tud_audio_n_fb_set(UAC_SPK_FUNC_ID, speaker_feedback_value());
                tinyusb_audio_unlock();
            }
#endif
            while (spk_streaming) {
                tinyusb_audio_lock();
                uint16_t avail = tud_audio_n_available(UAC_SPK_FUNC_ID);
                if (avail == 0U) {
                    tinyusb_audio_unlock();
                    break;
                }
                uint16_t chunk = (avail > sizeof(spk_read_buf)) ? sizeof(spk_read_buf) : avail;
                uint16_t got = tud_audio_n_read(UAC_SPK_FUNC_ID, spk_read_buf, chunk);
                tinyusb_audio_unlock();

                if (got == 0U) {
                    break;
                }
                push_speaker_data(spk_read_buf, got);
            }
        }

        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(CONFIG_UAC_SPK_READ_MS));
    }
}

static void speaker_task(void *param)
{
    (void)param;

    while (1) {
        if (!spk_streaming) {
            if (spk_started && playback_stream != NULL && playback_stream->ops != NULL &&
                playback_stream->ops->stop != NULL) {
                playback_stream->ops->stop(playback_stream);
            }
            spk_started = false;
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10));
            continue;
        }

        if (!spk_started) {
            if (playback_stream != NULL && playback_stream->ops != NULL &&
                playback_stream->ops->write != NULL) {
                for (uint32_t i = 0; i < 3U; i++) {
                    (void)playback_stream->ops->write(playback_stream,
                                                      spk_silence_buf,
                                                      sizeof(spk_silence_buf));
                }
            }
            if (playback_stream != NULL && playback_stream->ops != NULL &&
                playback_stream->ops->start != NULL) {
                int ret = playback_stream->ops->start(playback_stream);
                if (ret != 0) {
                    LISA_LOGE(TAG, "speaker start failed: %d", ret);
                    vTaskDelay(pdMS_TO_TICKS(10));
                    continue;
                }
            }
            spk_started = true;
        }

        uint32_t copied = pop_speaker_data(spk_play_buf, sizeof(spk_play_buf));
        if (copied < sizeof(spk_play_buf)) {
            memset(&spk_play_buf[copied], 0, sizeof(spk_play_buf) - copied);
        }

        if (playback_stream == NULL || playback_stream->ops == NULL ||
            playback_stream->ops->write == NULL) {
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }

        const uint8_t *play_data = stream_get_mute(STREAM_KIND_SPK, 0) ? spk_silence_buf : spk_play_buf;
        uint32_t written = playback_stream->ops->write(playback_stream, play_data, sizeof(spk_play_buf));
        if (written == 0U) {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
}

int uac_device_start(const uac_device_config_t *config)
{
    if (!uac_device_config_valid(config)) {
        LISA_LOGE(TAG, "invalid UAC stream config");
        return -1;
    }

    capture_stream = config->capture;
    playback_stream = config->playback;
    mic_volume_db = 0;
    spk_volume_db = 0;

    spk_ring_mem = (uint8_t *)exram_malloc(4, UAC_SPK_RING_BYTES);
    if (spk_ring_mem == NULL) {
        LISA_LOGE(TAG, "failed to allocate speaker ring: %u bytes", (unsigned int)UAC_SPK_RING_BYTES);
        return -1;
    }
    ring_buf_init(&spk_ring, UAC_SPK_RING_BYTES, spk_ring_mem);

    tinyusb_audio_mutex = xSemaphoreCreateMutex();
    if (tinyusb_audio_mutex == NULL) {
        LISA_LOGE(TAG, "failed to create TinyUSB audio mutex");
        return -1;
    }

    if (xTaskCreate(mic_usb_task, "uac_mic_usb", CONFIG_UAC_TASK_STACK,
                    NULL, configMAX_PRIORITIES - 2, &mic_usb_task_handle) != pdPASS) {
        LISA_LOGE(TAG, "failed to create UAC mic USB task");
        return -1;
    }

    if (xTaskCreate(speaker_usb_task, "uac_spk_usb", CONFIG_UAC_TASK_STACK,
                    NULL, configMAX_PRIORITIES - 2, &spk_usb_task_handle) != pdPASS) {
        LISA_LOGE(TAG, "failed to create UAC speaker USB task");
        return -1;
    }

    if (xTaskCreate(speaker_task, "uac_spk", CONFIG_UAC_TASK_STACK,
                    NULL, configMAX_PRIORITIES - 3, &speaker_task_handle) != pdPASS) {
        LISA_LOGE(TAG, "failed to create UAC speaker task");
        return -1;
    }

    if (xTaskCreate(usb_device_task, "tinyusb", CONFIG_UAC_TASK_STACK,
                    NULL, configMAX_PRIORITIES - 1, NULL) != pdPASS) {
        LISA_LOGE(TAG, "failed to create TinyUSB task");
        return -1;
    }

    LISA_LOGI(TAG, "TinyUSB UAC2 ready: mic %uHz/%ubit/%uch, speaker %uHz/%ubit/%uch",
              (unsigned int)UAC_MIC_SAMPLE_RATE, (unsigned int)UAC_MIC_SAMPLE_BITS,
              (unsigned int)UAC_MIC_CHANNELS, (unsigned int)UAC_SPK_SAMPLE_RATE,
              (unsigned int)UAC_SPK_SAMPLE_BITS, (unsigned int)UAC_SPK_CHANNELS);
    return 0;
}

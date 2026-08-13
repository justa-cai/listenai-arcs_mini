/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mic.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "ring_buffer.h"
#include "sysheap.h"
#include "uac_descriptors.h"

#if defined(CONFIG_SOC_VENUSA)
#include "IOMuxManager.h"
#endif

#define TAG "audio_mic"
#include "lisa_log.h"

#if defined(CONFIG_UAC_RECORD_HPF) && (CONFIG_UAC_RECORD_HPF == 1)
#define AUDIO_RECORD_HPF true
#else
#define AUDIO_RECORD_HPF false
#endif

#if defined(CONFIG_UAC_RECORD_DIFFERENTIAL_INPUT) && (CONFIG_UAC_RECORD_DIFFERENTIAL_INPUT == 1)
#define AUDIO_RECORD_DIFFERENTIAL_INPUT true
#else
#define AUDIO_RECORD_DIFFERENTIAL_INPUT false
#endif

#define LISA_MIC_MAX_CHANNELS 2U

typedef struct {
    lisa_device_t *audio_dev;
    struct ring_buf ring;
    uint8_t *ring_mem;
    uint32_t sample_rate;
    uint8_t uac_channels;
    uint8_t hw_channels;
    uint8_t sample_bits;
    bool mute;
    bool started;
    int volume_db;
} mic_priv_t;

static mic_priv_t mic_priv;

static lisa_audio_rate_t sample_rate_to_lisa(uint32_t sample_rate)
{
    switch (sample_rate) {
    case 8000:  return LISA_AUDIO_RATE_8K;
    case 16000: return LISA_AUDIO_RATE_16K;
    case 24000: return LISA_AUDIO_RATE_24K;
    case 32000: return LISA_AUDIO_RATE_32K;
    case 48000: return LISA_AUDIO_RATE_48K;
    case 96000: return LISA_AUDIO_RATE_96K;
    default:    return (lisa_audio_rate_t)sample_rate;
    }
}

static lisa_audio_bits_t sample_bits_to_lisa(uint32_t sample_bits)
{
    switch (sample_bits) {
    case 16: return LISA_AUDIO_BIT_16;
    case 24: return LISA_AUDIO_BIT_24;
    case 32: return LISA_AUDIO_BIT_32;
    default: return LISA_AUDIO_BIT_16;
    }
}

static lisa_audio_channel_t channels_to_lisa(uint32_t channels)
{
    return (channels >= 2U) ? LISA_AUDIO_CH_STEREO : LISA_AUDIO_CH_LEFT;
}

static int mic_start(uac_stream_t *stream)
{
    mic_priv_t *priv = (mic_priv_t *)stream->priv;
    if (priv->audio_dev == NULL) {
        return -1;
    }
    if (priv->started) {
        return 0;
    }

    taskENTER_CRITICAL();
    ring_buf_reset(&priv->ring);
    taskEXIT_CRITICAL();

    int ret = lisa_audio_record_start(priv->audio_dev);
    if (ret == LISA_DEVICE_OK) {
        priv->started = true;
    }
    return ret;
}

static void mic_stop(uac_stream_t *stream)
{
    mic_priv_t *priv = (mic_priv_t *)stream->priv;
    if (priv->audio_dev != NULL && priv->started) {
        (void)lisa_audio_record_stop(priv->audio_dev);
        priv->started = false;
    }
}

static uint32_t mic_read(uac_stream_t *stream, uint8_t *buffer, uint32_t bytes)
{
    mic_priv_t *priv = (mic_priv_t *)stream->priv;
    uint32_t copied;

    if (priv == NULL || priv->mute || buffer == NULL) {
        return 0;
    }

    taskENTER_CRITICAL();
    copied = ring_buf_get(&priv->ring, buffer, bytes);
    taskEXIT_CRITICAL();

    return copied;
}

static void mic_set_mute(uac_stream_t *stream, uint8_t channel, bool mute)
{
    mic_priv_t *priv = (mic_priv_t *)stream->priv;
    priv->mute = mute;
    LISA_LOGI(TAG, "ch%u mute %s", channel, mute ? "on" : "off");
}

static bool mic_get_mute(uac_stream_t *stream, uint8_t channel)
{
    (void)channel;
    mic_priv_t *priv = (mic_priv_t *)stream->priv;
    return priv->mute;
}

static void mic_set_volume(uac_stream_t *stream, uint8_t channel, int volume_db)
{
    mic_priv_t *priv = (mic_priv_t *)stream->priv;
    priv->volume_db = volume_db;
    LISA_LOGI(TAG, "ch%u volume %d dB", channel, volume_db);
}

static int mic_get_volume(uac_stream_t *stream, uint8_t channel)
{
    (void)channel;
    mic_priv_t *priv = (mic_priv_t *)stream->priv;
    return priv->volume_db;
}

static void mic_set_sampling_freq(uac_stream_t *stream, uint32_t sampling_freq)
{
    mic_priv_t *priv = (mic_priv_t *)stream->priv;
    priv->sample_rate = sampling_freq;
}

static uint32_t mic_get_sampling_freq(uac_stream_t *stream)
{
    mic_priv_t *priv = (mic_priv_t *)stream->priv;
    return priv->sample_rate;
}

static const uac_stream_ops_t mic_ops = {
    .start = mic_start,
    .stop = mic_stop,
    .read = mic_read,
    .set_mute = mic_set_mute,
    .get_mute = mic_get_mute,
    .set_volume = mic_set_volume,
    .get_volume = mic_get_volume,
    .set_sampling_freq = mic_set_sampling_freq,
    .get_sampling_freq = mic_get_sampling_freq,
};

static uac_stream_t mic_uac_stream = {
    .name = "mic",
    .dir = UAC_STREAM_CAPTURE,
    .ops = &mic_ops,
    .priv = &mic_priv,
};

static void mic_audio_callback(const lisa_audio_event_t *event, void *user_data);

static void mic_configure_board_pins(void)
{
#if defined(CONFIG_SOC_VENUSA)
    /* Match soc/venusa/hal/demo/audio_in_out/adc_in.c MIC pin setup. */
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_C, 0, CSK_IOMUX_FUNC_DEFAULT); /* MIC1_P */
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_C, 1, CSK_IOMUX_FUNC_DEFAULT); /* MIC1_N */
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_C, 2, CSK_IOMUX_FUNC_DEFAULT); /* MIC0_P */
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_C, 3, CSK_IOMUX_FUNC_DEFAULT); /* MIC0_N */
#endif
}

int mic_init(lisa_device_t *audio_dev)
{
    if (audio_dev == NULL) {
        return -1;
    }

    mic_configure_board_pins();

    mic_priv.audio_dev = audio_dev;
    mic_priv.sample_rate = UAC_MIC_SAMPLE_RATE;
    mic_priv.uac_channels = UAC_MIC_CHANNELS;
    mic_priv.hw_channels = (UAC_MIC_CHANNELS > LISA_MIC_MAX_CHANNELS) ? LISA_MIC_MAX_CHANNELS : UAC_MIC_CHANNELS;
    mic_priv.sample_bits = UAC_MIC_SAMPLE_BITS;
    mic_priv.mute = false;
    mic_priv.started = false;
    mic_priv.volume_db = 0;

    mic_priv.ring_mem = (uint8_t *)exram_malloc(4, UAC_MIC_RING_BYTES);
    if (mic_priv.ring_mem == NULL) {
        LISA_LOGE(TAG, "failed to allocate mic ring: %u bytes", (unsigned int)UAC_MIC_RING_BYTES);
        return -1;
    }
    ring_buf_init(&mic_priv.ring, UAC_MIC_RING_BYTES, mic_priv.ring_mem);

    lisa_audio_record_config_t record_config = {
        .format = {
            .sample_rate = sample_rate_to_lisa(mic_priv.sample_rate),
            .channels = channels_to_lisa(mic_priv.hw_channels),
            .sample_bits = sample_bits_to_lisa(mic_priv.sample_bits),
        },
        .gain = {
            .analog_gain = CONFIG_UAC_RECORD_ANALOG_GAIN,
            .digital_gain = CONFIG_UAC_RECORD_DIGITAL_GAIN,
        },
        .enable_hpf = AUDIO_RECORD_HPF,
        .differential_input = AUDIO_RECORD_DIFFERENTIAL_INPUT,
    };

    int ret = lisa_audio_record_config(audio_dev, &record_config);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(TAG, "lisa_audio_record_config failed: %d", ret);
        return ret;
    }

    ret = lisa_audio_register_callback(audio_dev, mic_audio_callback, NULL);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(TAG, "lisa_audio_register_callback failed: %d", ret);
        return ret;
    }

    return 0;
}

static void mic_on_audio_event(const lisa_audio_event_t *event)
{
    if (event == NULL || event->record_buffer == NULL || event->record_samples == 0U) {
        return;
    }

    const uint32_t sample_bytes = mic_priv.sample_bits / 8U;
    const uint8_t *src = (const uint8_t *)event->record_buffer;
    uint32_t src_samples = event->record_samples;
    uint32_t hw_channels = mic_priv.hw_channels;
    uint32_t uac_channels = mic_priv.uac_channels;

    if (sample_bytes == 0U || hw_channels == 0U || uac_channels == 0U) {
        return;
    }

    uint8_t frame[UAC_MIC_CHANNELS * UAC_MIC_FRAME_BYTES];
    uint32_t frames = src_samples / hw_channels;

    for (uint32_t frame_idx = 0; frame_idx < frames; frame_idx++) {
        const uint8_t *src_frame = src + frame_idx * hw_channels * sample_bytes;
        for (uint32_t ch = 0; ch < uac_channels; ch++) {
            uint8_t *dst = frame + ch * sample_bytes;
            if (ch < hw_channels) {
                memcpy(dst, src_frame + ch * sample_bytes, sample_bytes);
            } else {
                memset(dst, 0, sample_bytes);
            }
        }
        taskENTER_CRITICAL();
        if (sizeof(frame) > ring_buf_space_get(&mic_priv.ring)) {
            uint8_t drop[sizeof(frame)];
            (void)ring_buf_get(&mic_priv.ring, drop, sizeof(frame));
        }
        (void)ring_buf_put(&mic_priv.ring, frame, sizeof(frame));
        taskEXIT_CRITICAL();
    }
}

static void mic_audio_callback(const lisa_audio_event_t *event, void *user_data)
{
    (void)user_data;
    mic_on_audio_event(event);
}

uac_stream_t *mic_stream(void)
{
    return &mic_uac_stream;
}

/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "speaker.h"

#include <stdbool.h>
#include <stdint.h>

#include "lisa_audio.h"
#include "usb_descriptors.h"

#define TAG "uac_spk"
#include "lisa_log.h"

#define AUDIO_DEVICE_NAME "audio0"
#define LISA_PLAY_DIGITAL_GAIN_MIN_DB (-113)
#define LISA_PLAY_DIGITAL_GAIN_MAX_DB 30

typedef struct {
    lisa_device_t *audio_dev;
    uint32_t sample_rate;
    uint8_t channels;
    uint8_t sample_bits;
    bool mute;
    int volume_db;
    int base_analog_gain_db;
    int base_digital_gain_db;
} speaker_priv_t;

static speaker_priv_t speaker_priv;

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
    return (channels == 2U) ? LISA_AUDIO_CH_STEREO : LISA_AUDIO_CH_LEFT;
}

static int clamp_int(int value, int min, int max)
{
    if (value < min) {
        return min;
    }
    if (value > max) {
        return max;
    }
    return value;
}

static int speaker_apply_gain(speaker_priv_t *priv)
{
    if (priv->audio_dev == NULL) {
        return -1;
    }

    int host_attenuation_db = priv->volume_db;
    if (host_attenuation_db <= CONFIG_UAC_PLAY_MUTE_DB) {
        host_attenuation_db = LISA_PLAY_DIGITAL_GAIN_MIN_DB - priv->base_digital_gain_db;
    } else if (host_attenuation_db < 0) {
        host_attenuation_db /= CONFIG_UAC_PLAY_VOLUME_DIVISOR;
    }

    int digital_gain = clamp_int(priv->base_digital_gain_db + host_attenuation_db,
                                 LISA_PLAY_DIGITAL_GAIN_MIN_DB,
                                 LISA_PLAY_DIGITAL_GAIN_MAX_DB);
    lisa_audio_gain_t gain = {
        .analog_gain = (int8_t)priv->base_analog_gain_db,
        .digital_gain = (int8_t)digital_gain,
    };

    return lisa_audio_play_set_gain(priv->audio_dev, &gain);
}

static int speaker_start(uac_stream_t *stream)
{
    speaker_priv_t *priv = (speaker_priv_t *)stream->priv;
    return priv->audio_dev ? lisa_audio_play_start(priv->audio_dev) : -1;
}

static void speaker_stop(uac_stream_t *stream)
{
    speaker_priv_t *priv = (speaker_priv_t *)stream->priv;
    if (priv->audio_dev != NULL) {
        (void)lisa_audio_play_stop(priv->audio_dev);
    }
}

static uint32_t speaker_write(uac_stream_t *stream, const uint8_t *data, uint32_t bytes)
{
    speaker_priv_t *priv = (speaker_priv_t *)stream->priv;
    if (priv->audio_dev == NULL || data == NULL) {
        return 0;
    }

    if (priv->mute) {
        return bytes;
    }

    uint32_t sample_bytes = priv->sample_bits / 8U;
    if (sample_bytes == 0U) {
        return 0;
    }

    uint32_t samples = bytes / sample_bytes;
    int ret = lisa_audio_play_write(priv->audio_dev, data, samples);
    return (ret > 0) ? (uint32_t)ret * sample_bytes : 0U;
}

static void speaker_set_mute(uac_stream_t *stream, uint8_t channel, bool mute)
{
    speaker_priv_t *priv = (speaker_priv_t *)stream->priv;
    priv->mute = mute;
    LISA_LOGI(TAG, "ch%u mute %s", channel, mute ? "on" : "off");
}

static bool speaker_get_mute(uac_stream_t *stream, uint8_t channel)
{
    (void)channel;
    speaker_priv_t *priv = (speaker_priv_t *)stream->priv;
    return priv->mute;
}

static void speaker_set_volume(uac_stream_t *stream, uint8_t channel, int volume_db)
{
    speaker_priv_t *priv = (speaker_priv_t *)stream->priv;
    priv->volume_db = volume_db;

    int ret = speaker_apply_gain(priv);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(TAG, "set play gain failed: %d", ret);
    }

    int effective_volume_db = volume_db;
    if (effective_volume_db <= CONFIG_UAC_PLAY_MUTE_DB) {
        effective_volume_db = LISA_PLAY_DIGITAL_GAIN_MIN_DB - priv->base_digital_gain_db;
    } else if (effective_volume_db < 0) {
        effective_volume_db /= CONFIG_UAC_PLAY_VOLUME_DIVISOR;
    }

    LISA_LOGI(TAG, "ch%u host volume %d dB, play digital gain %d dB",
              channel, volume_db,
              clamp_int(priv->base_digital_gain_db + effective_volume_db,
                        LISA_PLAY_DIGITAL_GAIN_MIN_DB,
                        LISA_PLAY_DIGITAL_GAIN_MAX_DB));
}

static int speaker_get_volume(uac_stream_t *stream, uint8_t channel)
{
    (void)channel;
    speaker_priv_t *priv = (speaker_priv_t *)stream->priv;
    return priv->volume_db;
}

static void speaker_set_sampling_freq(uac_stream_t *stream, uint32_t sampling_freq)
{
    speaker_priv_t *priv = (speaker_priv_t *)stream->priv;
    priv->sample_rate = sampling_freq;
    LISA_LOGI(TAG, "sampling frequency %u Hz", (unsigned int)sampling_freq);
}

static uint32_t speaker_get_sampling_freq(uac_stream_t *stream)
{
    speaker_priv_t *priv = (speaker_priv_t *)stream->priv;
    return priv->sample_rate;
}

static const uac_stream_ops_t speaker_ops = {
    .start = speaker_start,
    .stop = speaker_stop,
    .write = speaker_write,
    .set_mute = speaker_set_mute,
    .get_mute = speaker_get_mute,
    .set_volume = speaker_set_volume,
    .get_volume = speaker_get_volume,
    .set_sampling_freq = speaker_set_sampling_freq,
    .get_sampling_freq = speaker_get_sampling_freq,
};

static uac_stream_t speaker_uac_stream = {
    .name = "speaker",
    .dir = UAC_STREAM_PLAYBACK,
    .ops = &speaker_ops,
    .priv = &speaker_priv,
};

int speaker_init(void)
{
    speaker_priv.audio_dev = lisa_device_get(AUDIO_DEVICE_NAME);
    if (speaker_priv.audio_dev == NULL) {
        LISA_LOGE(TAG, "failed to get %s", AUDIO_DEVICE_NAME);
        return -1;
    }

    speaker_priv.sample_rate = UAC_SPK_SAMPLE_RATE;
    speaker_priv.channels = UAC_SPK_CHANNELS;
    speaker_priv.sample_bits = UAC_SPK_SAMPLE_BITS;
    speaker_priv.mute = false;
    speaker_priv.volume_db = 0;
    speaker_priv.base_analog_gain_db = CONFIG_UAC_PLAY_ANALOG_GAIN;
    speaker_priv.base_digital_gain_db = CONFIG_UAC_PLAY_DIGITAL_GAIN;

    uint32_t play_buffer_samples = (speaker_priv.sample_rate * CONFIG_UAC_PLAY_BUFFER_MS *
                                    speaker_priv.channels) / 1000U;
    if (play_buffer_samples == 0U) {
        play_buffer_samples = speaker_priv.channels;
    }

    lisa_audio_play_config_t play_config = {
        .format = {
            .sample_rate = sample_rate_to_lisa(speaker_priv.sample_rate),
            .channels = channels_to_lisa(speaker_priv.channels),
            .sample_bits = sample_bits_to_lisa(speaker_priv.sample_bits),
        },
        .gain = {
            .analog_gain = CONFIG_UAC_PLAY_ANALOG_GAIN,
            .digital_gain = CONFIG_UAC_PLAY_DIGITAL_GAIN,
        },
        .buffer_count = CONFIG_UAC_PLAY_BUFFER_COUNT,
        .buffer_samples = (uint16_t)play_buffer_samples,
    };

    int ret = lisa_audio_play_config(speaker_priv.audio_dev, &play_config);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(TAG, "lisa_audio_play_config failed: %d", ret);
        return ret;
    }

    return 0;
}

uac_stream_t *speaker_stream(void)
{
    return &speaker_uac_stream;
}

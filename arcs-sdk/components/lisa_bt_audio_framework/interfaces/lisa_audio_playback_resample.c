#include "lisa_audio_playback_resample.h"

#include <string.h>

#define LISA_AUDIO_RESAMPLE_44K1_INPUT_RATE 44100U
#define LISA_AUDIO_RESAMPLE_44K1_OUTPUT_RATE 48000U

typedef bool (*lisa_playback_resample_match_fn_t)(const bt_audio_format_t *format);
typedef bt_audio_error_t (*lisa_playback_resample_enable_fn_t)(lisa_audio_playback_resample_state_t *resample,
                                                               const bt_audio_format_t *format);

typedef struct {
    lisa_playback_resample_match_fn_t match;
    lisa_playback_resample_enable_fn_t enable;
} lisa_playback_resample_strategy_t;

static bool lisa_playback_resample_match_44k1_to_48k(const bt_audio_format_t *format)
{
    return format && (format->sample_rate == LISA_AUDIO_RESAMPLE_44K1_INPUT_RATE);
}

static bt_audio_error_t lisa_playback_resample_enable_44k1_to_48k(lisa_audio_playback_resample_state_t *resample,
                                                                  const bt_audio_format_t *format)
{
    int ret;

    if (!resample || !format) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }

    if (format->bits_per_sample != 16) {
        return BT_AUDIO_ERR_NOT_SUPPORTED;
    }

    ret = lisa_audio_resampler_init_44k1_to_48k(&resample->resampler, format->channels);
    if (ret != 0) {
        return BT_AUDIO_ERR_NOT_SUPPORTED;
    }

    resample->enabled = true;
    resample->bits_per_sample = format->bits_per_sample;
    resample->input_rate = LISA_AUDIO_RESAMPLE_44K1_INPUT_RATE;
    resample->output_rate = LISA_AUDIO_RESAMPLE_44K1_OUTPUT_RATE;

    return BT_AUDIO_OK;
}

static const lisa_playback_resample_strategy_t g_lisa_playback_resample_strategies[] = {
    {
        .match = lisa_playback_resample_match_44k1_to_48k,
        .enable = lisa_playback_resample_enable_44k1_to_48k,
    },
};

static bt_audio_error_t lisa_playback_resample_select_strategy(lisa_audio_playback_resample_state_t *resample,
                                                               const bt_audio_format_t *format)
{
    size_t i;

    for (i = 0; i < (sizeof(g_lisa_playback_resample_strategies) /
                     sizeof(g_lisa_playback_resample_strategies[0])); ++i) {
        const lisa_playback_resample_strategy_t *strategy = &g_lisa_playback_resample_strategies[i];

        if (strategy->match && strategy->match(format)) {
            return strategy->enable ? strategy->enable(resample, format) : BT_AUDIO_OK;
        }
    }

    return BT_AUDIO_OK;
}

bt_audio_error_t lisa_audio_playback_resample_prepare(lisa_audio_playback_resample_state_t *resample,
                                                      const bt_audio_format_t *format)
{
    if (!resample || !format) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }

    memset(resample, 0, sizeof(*resample));
    resample->input_rate = format->sample_rate;
    resample->output_rate = format->sample_rate;

    return lisa_playback_resample_select_strategy(resample, format);
}

uint32_t lisa_audio_playback_resample_output_rate(const lisa_audio_playback_resample_state_t *resample)
{
    if (!resample) {
        return 0U;
    }

    return (resample->output_rate != 0U) ? resample->output_rate : resample->input_rate;
}

uint16_t lisa_audio_playback_resample_buffer_samples(const lisa_audio_playback_resample_state_t *resample,
                                                     uint32_t work_buffer_time_ms)
{
    size_t buffer_samples;

    if (!resample || resample->output_rate == 0U) {
        return 0U;
    }

    if (!resample->enabled) {
        return (uint16_t)(((size_t)resample->output_rate * work_buffer_time_ms) / 1000U);
    }

    buffer_samples = ((((size_t)resample->input_rate * work_buffer_time_ms) + 999U) / 1000U) *
                     resample->output_rate;
    buffer_samples = (buffer_samples + resample->input_rate - 1U) / resample->input_rate;

    return (uint16_t)buffer_samples;
}

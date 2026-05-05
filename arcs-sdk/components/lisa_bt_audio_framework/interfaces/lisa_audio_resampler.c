#include "lisa_audio_resampler.h"

#include <string.h>

#define LISA_AUDIO_RESAMPLE_STEP_NUM 147U
#define LISA_AUDIO_RESAMPLE_SCALE    160U

static int16_t lisa_audio_resampler_interp(int16_t left, int16_t right, uint16_t phase_num)
{
    int32_t delta = (int32_t)right - (int32_t)left;
    int32_t value = (int32_t)left + ((delta * (int32_t)phase_num + 80) / 160);
    return (int16_t)value;
}

int lisa_audio_resampler_init_44k1_to_48k(lisa_audio_resampler_t *resampler, uint8_t channels)
{
    if (!resampler || channels == 0 || channels > LISA_AUDIO_RESAMPLER_MAX_CHANNELS) {
        return -1;
    }

    memset(resampler, 0, sizeof(*resampler));
    resampler->channels = channels;
    return 0;
}

void lisa_audio_resampler_reset(lisa_audio_resampler_t *resampler)
{
    if (!resampler) {
        return;
    }

    memset(resampler, 0, sizeof(*resampler));
}

size_t lisa_audio_resampler_process(lisa_audio_resampler_t *resampler,
                                    const int16_t *input,
                                    size_t input_frames,
                                    int16_t *output,
                                    size_t output_capacity_samples)
{
    size_t output_samples = 0;
    size_t frame_index;
    uint8_t channels;

    if (!resampler || !input || !output || input_frames == 0 || resampler->channels == 0) {
        return 0;
    }

    channels = resampler->channels;

    for (frame_index = 0; frame_index < input_frames; ++frame_index) {
        const int16_t *curr_frame = input + (frame_index * channels);

        if (!resampler->has_prev_frame) {
            memcpy(resampler->prev_frame, curr_frame, channels * sizeof(int16_t));
            resampler->has_prev_frame = true;
            continue;
        }

        while (resampler->phase_num < LISA_AUDIO_RESAMPLE_SCALE) {
            uint8_t channel;

            if ((output_samples + channels) > output_capacity_samples) {
                return output_samples * sizeof(int16_t);
            }

            for (channel = 0; channel < channels; ++channel) {
                output[output_samples++] = lisa_audio_resampler_interp(resampler->prev_frame[channel],
                                                                       curr_frame[channel],
                                                                       resampler->phase_num);
            }

            resampler->phase_num += LISA_AUDIO_RESAMPLE_STEP_NUM;
        }

        resampler->phase_num -= LISA_AUDIO_RESAMPLE_SCALE;
        memcpy(resampler->prev_frame, curr_frame, channels * sizeof(int16_t));
    }

    return output_samples * sizeof(int16_t);
}

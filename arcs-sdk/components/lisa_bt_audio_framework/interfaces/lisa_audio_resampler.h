#ifndef LISA_AUDIO_RESAMPLER_H_
#define LISA_AUDIO_RESAMPLER_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LISA_AUDIO_RESAMPLER_MAX_CHANNELS 2U

typedef struct {
    bool has_prev_frame;
    uint8_t channels;
    uint16_t phase_num;
    int16_t prev_frame[LISA_AUDIO_RESAMPLER_MAX_CHANNELS];
} lisa_audio_resampler_t;

int lisa_audio_resampler_init_44k1_to_48k(lisa_audio_resampler_t *resampler, uint8_t channels);
void lisa_audio_resampler_reset(lisa_audio_resampler_t *resampler);
size_t lisa_audio_resampler_process(lisa_audio_resampler_t *resampler,
                                    const int16_t *input,
                                    size_t input_frames,
                                    int16_t *output,
                                    size_t output_capacity_samples);

#ifdef __cplusplus
}
#endif

#endif

#ifndef LISA_AUDIO_PLAYBACK_RESAMPLE_H_
#define LISA_AUDIO_PLAYBACK_RESAMPLE_H_

#include "bt_audio_interface.h"
#include "lisa_audio_resampler.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool enabled;
    uint8_t bits_per_sample;
    uint32_t input_rate;
    uint32_t output_rate;
    lisa_audio_resampler_t resampler;
} lisa_audio_playback_resample_state_t;

bt_audio_error_t lisa_audio_playback_resample_prepare(lisa_audio_playback_resample_state_t *resample,
                                                      const bt_audio_format_t *format);
uint32_t lisa_audio_playback_resample_output_rate(const lisa_audio_playback_resample_state_t *resample);
uint16_t lisa_audio_playback_resample_buffer_samples(const lisa_audio_playback_resample_state_t *resample,
                                                     uint32_t work_buffer_time_ms);

#ifdef __cplusplus
}
#endif

#endif

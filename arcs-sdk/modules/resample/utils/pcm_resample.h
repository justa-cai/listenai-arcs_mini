#ifndef _AUDIO_PLAYER_UTILS_PCM_RESAMPLE_H_
#define _AUDIO_PLAYER_UTILS_PCM_RESAMPLE_H_

#include <stdint.h>
#include "speex/speex_resampler.h"

typedef int (*resample_process_entry)(SpeexResamplerState *st, const spx_int16_t *in,
		spx_uint32_t *in_len, spx_int16_t *out, spx_uint32_t *out_len);

typedef struct ResampleContext {
	uint8_t channels;
	uint8_t quality;
	uint8_t sample_bytes;
	uint8_t *priv_data;
	uint32_t priv_data_len;
	uint32_t priv_data_capacity;
	uint32_t in_rate;
	uint32_t out_rate;

	SpeexResamplerState *handle;
	resample_process_entry resample_proc;

	int8_t (*init)(struct ResampleContext *const ctx, uint8_t channels, uint32_t in_rate,
			uint32_t out_rate, uint8_t quality);
	int8_t (*uninit)(struct ResampleContext *const ctx);
	int (*process)(const struct ResampleContext *const ctx, const int16_t *in,
			uint32_t *in_len, /* input sample count per channel */
			int16_t *out, uint32_t *out_len /* output sample count per channel */);
	int (*process2)(struct ResampleContext *const ctx, const int16_t *in,
			uint32_t in_len, /* input bytes */
			int16_t *out, uint32_t *out_len /* input capacity of out, output bytes of result */);

} ResampleContext;

#endif
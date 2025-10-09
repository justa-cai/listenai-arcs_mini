#include "pcm_resample.h"
#include <string.h>
#include <stdlib.h>
#include <lisa_mem.h>

static inline int speex_resampler_process_int2(SpeexResamplerState *st, const spx_int16_t *in,
		spx_uint32_t *in_len, spx_int16_t *out, spx_uint32_t *out_len)
{
	return speex_resampler_process_int(st, 0, in, in_len, out, out_len);
}

static int8_t resample_init(struct ResampleContext *const ctx, uint8_t channels, uint32_t in_rate,
		uint32_t out_rate, uint8_t quality)
{
	if (channels != 1 && channels != 2) return -1;
	int err = RESAMPLER_ERR_SUCCESS;
	SpeexResamplerState *st = speex_resampler_init(channels, in_rate, out_rate, quality, &err);
	if (!st) return err;
	ctx->handle = st;
	ctx->channels = channels;
	ctx->sample_bytes = channels * sizeof(int16_t);
	ctx->in_rate = in_rate;
	ctx->out_rate = out_rate;
	ctx->quality = quality;
	ctx->resample_proc = (channels == 2) ? speex_resampler_process_interleaved_int
										 : speex_resampler_process_int2;
	return RESAMPLER_ERR_SUCCESS;
}

static int8_t resample_uninit(struct ResampleContext *const ctx)
{
	speex_resampler_destroy(ctx->handle);
	ctx->handle = NULL;
	if (ctx->priv_data) {
		lisa_mem_free(ctx->priv_data);
		ctx->priv_data = NULL;
	}
	return 0;
}

static int resample_process(const struct ResampleContext *const ctx, const int16_t *in,
		uint32_t *in_len, int16_t *out, uint32_t *out_len)
{
	return ctx->resample_proc(ctx->handle, in, in_len, out, out_len);
}

static int resample_process2(struct ResampleContext *const ctx, const int16_t *in, uint32_t in_len,
		int16_t *out, uint32_t *out_len)
{
	const int16_t *in_ptr = NULL;
	uint32_t in_data_len = 0;

	if (ctx->priv_data_len > 0) {
		if (ctx->priv_data_len + in_len > ctx->priv_data_capacity) {
			ctx->priv_data_capacity = ctx->priv_data_len + in_len;
			ctx->priv_data = (uint8_t *)lisa_mem_realloc(ctx->priv_data, ctx->priv_data_capacity);
		}
		memcpy(ctx->priv_data + ctx->priv_data_len, (uint8_t *)in, in_len);
		ctx->priv_data_len += in_len;
		in_ptr = (int16_t *)ctx->priv_data;
		in_data_len = ctx->priv_data_len;
	} else {
		in_ptr = in;
		in_data_len = in_len;
	}
	const uint32_t in_len2 = in_data_len / ctx->sample_bytes; /* sample count per channel */
	uint32_t in_len3 = in_len2;
	uint32_t left_bytes = 0;
	int ret = ctx->resample_proc(ctx->handle, in_ptr, &in_len3, out, out_len);
	if (ret == RESAMPLER_ERR_SUCCESS) {
		left_bytes = in_data_len - in_len3 * ctx->sample_bytes;
		if (left_bytes > 0) {
			if (!ctx->priv_data) {
				ctx->priv_data = (uint8_t *)lisa_mem_alloc(left_bytes);
			}
			memmove(ctx->priv_data, in_ptr + in_data_len - left_bytes, left_bytes);
		}
		*out_len *= ctx->sample_bytes;
	} else {
		*out_len = 0;
	}
	ctx->priv_data_len = left_bytes;
	return ret;
}

/* should not use this constant directly, just copy */
const ResampleContext resample_context_templet = {
		.handle = NULL,
		.resample_proc = NULL,
		.priv_data = NULL,
		.priv_data_len = 0,
		.priv_data_capacity = 0,
		.init = resample_init,
		.uninit = resample_uninit,
		.process = resample_process,
		.process2 = resample_process2,
};

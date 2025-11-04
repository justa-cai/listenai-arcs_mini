/**
 ****************************************************************************************
 *
 * @file msbc_primitives.h
 *
 * @brief msbc primitives
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */


#ifndef __MSBC_PRIMITIVES_H
#define __MSBC_PRIMITIVES_H

#define SCALE_OUT_BITS 15
#define SBC_X_BUFFER_SIZE 328

#ifdef __GNUC__
#define SBC_ALWAYS_INLINE inline __attribute__((always_inline))
#else
#define SBC_ALWAYS_INLINE __inline
#endif

struct msbc_encoder_state {
	int position;
	/* Number of consecutive blocks handled by the encoder */
	uint8_t increment;
	int16_t SBC_ALIGNED X/*[2]*/[SBC_X_BUFFER_SIZE];
	/* Polyphase analysis filter for 8 subbands configuration,
	 * it handles "increment" blocks at once */
	void (*msbc_analyze_8s)(struct msbc_encoder_state *state,
			int16_t *x, int32_t *out, int out_stride);
	/* Process input data (deinterleave, endian conversion, reordering),
	 * depending on the number of subbands and input data byte order */
	int (*msbc_enc_process_input_8s_le)(int position,
			const uint8_t *pcm, int16_t X/*[2]*/[SBC_X_BUFFER_SIZE],
			int nsamples, int nchannels);
	/* Scale factors calculation */
	void (*msbc_calc_scalefactors)(int32_t sb_sample_f[16]/*[2]*/[8],
			uint32_t scale_factor/*[2]*/[8],
			int blocks, int channels, int subbands);
	const char *implementation_info;
};

/*
 * Initialize pointers to the functions which are the basic "building bricks"
 * of SBC codec. Best implementation is selected based on target CPU
 * capabilities.
 */
void msbc_init_primitives(struct msbc_encoder_state *encoder_state);

#endif

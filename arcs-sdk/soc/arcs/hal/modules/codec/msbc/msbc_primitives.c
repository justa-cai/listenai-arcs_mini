/**
 ****************************************************************************************
 *
 * @file msbc_primitives.c
 *
 * @brief msbc primitives
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */


//#include <limits.h>
#include <string.h>
//#if defined(__IAR_SYSTEMS_ICC__) || defined(DOXYGEN)
//   #include <intrinsics.h>
//#endif
#include "msbc_typedef.h"
#include "msbc.h"
#include "msbc_math.h"
#include "msbc_tables.h"

#include "msbc_primitives.h"


/*
 * A reference C code of analysis filter with SIMD-friendly tables
 * reordering and code layout. This code can be used to develop platform
 * specific SIMD optimizations. Also it may be used as some kind of test
 * for compiler autovectorization capabilities (who knows, if the compiler
 * is very good at this stuff, hand optimized assembly may be not strictly
 * needed for some platform).
 *
 * Note: It is also possible to make a simple variant of analysis filter,
 * which needs only a single constants table without taking care about
 * even/odd cases. This simple variant of filter can be implemented without
 * input data permutation. The only thing that would be lost is the
 * possibility to use pairwise SIMD multiplications. But for some simple
 * CPU cores without SIMD extensions it can be useful. If anybody is
 * interested in implementing such variant of a filter, sourcecode from
 * bluez versions 4.26/4.27 can be used as a reference and the history of
 * the changes in git repository done around that time may be worth checking.
 */


static SBC_ALWAYS_INLINE void msbc_analyze_eight_simd(const int16_t *in, int32_t *out,
							const FIXED_T *consts)
{
	FIXED_A t1[8];
	FIXED_T t2[8];
	int i, hop;

	/* rounding coefficient */
	t1[0] = t1[1] = t1[2] = t1[3] = t1[4] = t1[5] = t1[6] = t1[7] =
		(FIXED_A) 1 << (SBC_PROTO_FIXED8_SCALE-1);

	/* low pass polyphase filter */
	for (hop = 0; hop < 80; hop += 16) {
		t1[0] += (FIXED_A) in[hop] * consts[hop];
		t1[0] += (FIXED_A) in[hop + 1] * consts[hop + 1];
		t1[1] += (FIXED_A) in[hop + 2] * consts[hop + 2];
		t1[1] += (FIXED_A) in[hop + 3] * consts[hop + 3];
		t1[2] += (FIXED_A) in[hop + 4] * consts[hop + 4];
		t1[2] += (FIXED_A) in[hop + 5] * consts[hop + 5];
		t1[3] += (FIXED_A) in[hop + 6] * consts[hop + 6];
		t1[3] += (FIXED_A) in[hop + 7] * consts[hop + 7];
		t1[4] += (FIXED_A) in[hop + 8] * consts[hop + 8];
		t1[4] += (FIXED_A) in[hop + 9] * consts[hop + 9];
		t1[5] += (FIXED_A) in[hop + 10] * consts[hop + 10];
		t1[5] += (FIXED_A) in[hop + 11] * consts[hop + 11];
		t1[6] += (FIXED_A) in[hop + 12] * consts[hop + 12];
		t1[6] += (FIXED_A) in[hop + 13] * consts[hop + 13];
		t1[7] += (FIXED_A) in[hop + 14] * consts[hop + 14];
		t1[7] += (FIXED_A) in[hop + 15] * consts[hop + 15];
	}

	/* scaling */
	t2[0] = t1[0] >> SBC_PROTO_FIXED8_SCALE;
	t2[1] = t1[1] >> SBC_PROTO_FIXED8_SCALE;
	t2[2] = t1[2] >> SBC_PROTO_FIXED8_SCALE;
	t2[3] = t1[3] >> SBC_PROTO_FIXED8_SCALE;
	t2[4] = t1[4] >> SBC_PROTO_FIXED8_SCALE;
	t2[5] = t1[5] >> SBC_PROTO_FIXED8_SCALE;
	t2[6] = t1[6] >> SBC_PROTO_FIXED8_SCALE;
	t2[7] = t1[7] >> SBC_PROTO_FIXED8_SCALE;


	/* do the cos transform */
	t1[0] = t1[1] = t1[2] = t1[3] = t1[4] = t1[5] = t1[6] = t1[7] = 0;

	for (i = 0; i < 4; i++) {
		t1[0] += (FIXED_A) t2[i * 2 + 0] * consts[80 + i * 16 + 0];
		t1[0] += (FIXED_A) t2[i * 2 + 1] * consts[80 + i * 16 + 1];
		t1[1] += (FIXED_A) t2[i * 2 + 0] * consts[80 + i * 16 + 2];
		t1[1] += (FIXED_A) t2[i * 2 + 1] * consts[80 + i * 16 + 3];
		t1[2] += (FIXED_A) t2[i * 2 + 0] * consts[80 + i * 16 + 4];
		t1[2] += (FIXED_A) t2[i * 2 + 1] * consts[80 + i * 16 + 5];
		t1[3] += (FIXED_A) t2[i * 2 + 0] * consts[80 + i * 16 + 6];
		t1[3] += (FIXED_A) t2[i * 2 + 1] * consts[80 + i * 16 + 7];
		t1[4] += (FIXED_A) t2[i * 2 + 0] * consts[80 + i * 16 + 8];
		t1[4] += (FIXED_A) t2[i * 2 + 1] * consts[80 + i * 16 + 9];
		t1[5] += (FIXED_A) t2[i * 2 + 0] * consts[80 + i * 16 + 10];
		t1[5] += (FIXED_A) t2[i * 2 + 1] * consts[80 + i * 16 + 11];
		t1[6] += (FIXED_A) t2[i * 2 + 0] * consts[80 + i * 16 + 12];
		t1[6] += (FIXED_A) t2[i * 2 + 1] * consts[80 + i * 16 + 13];
		t1[7] += (FIXED_A) t2[i * 2 + 0] * consts[80 + i * 16 + 14];
		t1[7] += (FIXED_A) t2[i * 2 + 1] * consts[80 + i * 16 + 15];
	}

	for (i = 0; i < 8; i++)
		out[i] = t1[i] >>
			(SBC_COS_TABLE_FIXED8_SCALE - SCALE_OUT_BITS);
}

static SBC_ALWAYS_INLINE void msbc_analyze_1b_8s_simd_even(struct msbc_encoder_state *state,
		int16_t *x, int32_t *out, int out_stride);

static SBC_ALWAYS_INLINE void msbc_analyze_1b_8s_simd_odd(struct msbc_encoder_state *state,
		int16_t *x, int32_t *out, int out_stride)
{
	msbc_analyze_eight_simd(x, out, analysis_consts_fixed8_simd_odd);
	state->msbc_analyze_8s = msbc_analyze_1b_8s_simd_even;
}

static SBC_ALWAYS_INLINE void msbc_analyze_1b_8s_simd_even(struct msbc_encoder_state *state,
		int16_t *x, int32_t *out, int out_stride)
{
	msbc_analyze_eight_simd(x, out, analysis_consts_fixed8_simd_even);
	state->msbc_analyze_8s = msbc_analyze_1b_8s_simd_odd;
}

static SBC_ALWAYS_INLINE int16_t unaligned16_le(const uint8_t *ptr)
{
	return (int16_t) (ptr[0] | (ptr[1] << 8));
}

/*
 * Internal helper functions for input data processing. In order to get
 * optimal performance, it is important to have "nsamples", "nchannels"
 * and "big_endian" arguments used with this SBC_ALWAYS_INLINE function as compile
 * time constants.
 */

#ifdef __IAR_SYSTEMS_ICC__
#pragma optimize=none
#endif //__IAR_SYSTEMS_ICC__
static SBC_ALWAYS_INLINE int msbc_encoder_process_input_s8_internal(
	int position,
	const uint8_t *pcm, int16_t X/*[2]*/[SBC_X_BUFFER_SIZE],
	int nsamples,/* int nchannels,*/ int big_endian)
{
//    System_printf("Position %d\n", position);
//    /* SysMin will only print to the console when you call flush or exit */
//    System_flush();
	/* handle X buffer wraparound */
	if (position < nsamples) {
			memcpy(&X/*[0]*/[SBC_X_BUFFER_SIZE - 72], &X/*[0]*/[position],
							72 * sizeof(int16_t));
		position = SBC_X_BUFFER_SIZE - 72;
	}

	#define PCM(i) (unaligned16_le(pcm + (i) * 2))

	if (position % 16 == 8) {
		position -= 8;
		nsamples -= 8;
			int16_t *x = &X/*[0]*/[position];
			x[0]  = PCM(0 + (15-8)); // * nchannels);
			x[2]  = PCM(0 + (14-8)); // * nchannels);
			x[3]  = PCM(0 + (8-8)); // * nchannels);
			x[4]  = PCM(0 + (13-8)); // * nchannels);
			x[5]  = PCM(0 + (9-8)); // * nchannels);
			x[6]  = PCM(0 + (12-8)); // * nchannels);
			x[7]  = PCM(0 + (10-8)); // * nchannels);
			x[8]  = PCM(0 + (11-8)); // * nchannels);

		pcm += 16; // * nchannels;
	}

	/* copy/permutate audio samples */
	while (nsamples >= 16) {
		position -= 16;
			int16_t *x = &X/*[0]*/[position];
			x[0]  = PCM(0 + 15); // * nchannels);
			x[1]  = PCM(0 + 7); // * nchannels);
			x[2]  = PCM(0 + 14); // * nchannels);
			x[3]  = PCM(0 + 8); // * nchannels);
			x[4]  = PCM(0 + 13); // * nchannels);
			x[5]  = PCM(0 + 9); // * nchannels);
			x[6]  = PCM(0 + 12); // * nchannels);
			x[7]  = PCM(0 + 10); // * nchannels);
			x[8]  = PCM(0 + 11); // * nchannels);
			x[9]  = PCM(0 + 3); // * nchannels);
			x[10] = PCM(0 + 6); // * nchannels);
			x[11] = PCM(0 + 0); // * nchannels);
			x[12] = PCM(0 + 5); // * nchannels);
			x[13] = PCM(0 + 1); // * nchannels);
			x[14] = PCM(0 + 4); // * nchannels);
			x[15] = PCM(0 + 2); // * nchannels);
		pcm += 32; // * nchannels;
		nsamples -= 16;
	}

	if (nsamples == 8) {
		position -= 8;
			int16_t *x = &X/*[0]*/[position];
			x[-7] = PCM(0 + 7); // * nchannels);
			x[1]  = PCM(0 + 3); // * nchannels);
			x[2]  = PCM(0 + 6); // * nchannels);
			x[3]  = PCM(0 + 0); // * nchannels);
			x[4]  = PCM(0 + 5); // * nchannels);
			x[5]  = PCM(0 + 1); // * nchannels);
			x[6]  = PCM(0 + 4); // * nchannels);
			x[7]  = PCM(0 + 2); // * nchannels);
	}
	#undef PCM

	return position;
}

/*
 * Input data processing functions. The data is endian converted if needed,
 * channels are deintrleaved and audio samples are reordered for use in
 * SIMD-friendly analysis filter function. The results are put into "X"
 * array, getting appended to the previous data (or it is better to say
 * prepended, as the buffer is filled from top to bottom). Old data is
 * discarded when neededed, but availability of (10 * nrof_subbands)
 * contiguous samples is always guaranteed for the input to the analysis
 * filter. This is achieved by copying a sufficient part of old data
 * to the top of the buffer on buffer wraparound.
 */

static int msbc_enc_process_input_8s_le(int position,
		const uint8_t *pcm, int16_t X/*[2]*/[SBC_X_BUFFER_SIZE],
		int nsamples, int nchannels)
{
		return msbc_encoder_process_input_s8_internal(
			position, pcm, X, nsamples,/* 1,*/ 0);
}

static int sbc_enc_process_input_8s_be(int position,
		const uint8_t *pcm, int16_t X/*[2]*/[SBC_X_BUFFER_SIZE],
		int nsamples, int nchannels)
{
		return sbc_encoder_process_input_s8_internal(
			position, pcm, X, nsamples,/* 1,*/ 1);
}
/* Supplementary function to count the number of leading zeros */

static SBC_ALWAYS_INLINE int msbc_clz(uint32_t x)
{
#ifdef __GNUC__
	return __builtin_clz(x);
#elif defined __TI_COMPILER_VERSION || defined __TI_COMPILER_VERSION__
	return __clz(x);
#elif defined __IAR_SYSTEMS_ICC__
    return __CLZ(x);
#else
	/* TODO: this should be replaced with something better if good
	 * performance is wanted when using compilers other than gcc */
	int cnt = 0;
	while (x) {
		cnt++;
		x >>= 1;
	}
	return 32 - cnt;
#endif
}

static void msbc_calc_scalefactors(
	int32_t sb_sample_f[16]/*[2]*/[8],
	uint32_t scale_factor/*[2]*/[8],
	int blocks, int channels, int subbands)
{
	int /*ch,*/ sb, blk;
//	for (ch = 0; ch < channels; ch++) {
		for (sb = 0; sb < subbands; sb++) {
			uint32_t x = 1 << SCALE_OUT_BITS;
			for (blk = 0; blk < blocks; blk++) {
				int32_t tmp = fabs(sb_sample_f[blk]/*[ch]*/[sb]);
				if (tmp != 0)
					x |= tmp - 1;
			}
			scale_factor/*[ch]*/[sb] = (31 - SCALE_OUT_BITS) -
				msbc_clz(x);
		}
//	}
}


/*
 * Detect CPU features and setup function pointers
 */
void msbc_init_primitives(struct msbc_encoder_state *state)
{
	/* Default implementation for analyze functions */
		state->msbc_analyze_8s = msbc_analyze_1b_8s_simd_odd;

	/* Default implementation for input reordering / deinterleaving */
	state->msbc_enc_process_input_8s_le = msbc_enc_process_input_8s_le;

	/* Default implementation for scale factors calculation */
	state->msbc_calc_scalefactors = msbc_calc_scalefactors;

	state->implementation_info = "Listenai";

	/* X86/AMD64 optimizations */
#ifdef SBC_BUILD_WITH_MMX_SUPPORT
	msbc_init_primitives_mmx(state);
#endif

	/* ARM optimizations */
#ifdef SBC_BUILD_WITH_ARMV6_SUPPORT
	msbc_init_primitives_armv6(state);
#endif
#ifdef SBC_BUILD_WITH_IWMMXT_SUPPORT
	msbc_init_primitives_iwmmxt(state);
#endif
#ifdef SBC_BUILD_WITH_NEON_SUPPORT
	msbc_init_primitives_neon(state);

	if (state->increment == 1) {
		state->msbc_analyze_8s = msbc_analyze_1b_8s_simd_odd;
		state->msbc_enc_process_input_4s_le = msbc_enc_process_input_4s_le;
		state->msbc_enc_process_input_4s_be = msbc_enc_process_input_4s_be;
		state->msbc_enc_process_input_8s_le = msbc_enc_process_input_8s_le;
		state->msbc_enc_process_input_8s_be = msbc_enc_process_input_8s_be;
	}
#endif
}

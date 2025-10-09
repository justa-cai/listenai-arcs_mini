/*
 * luna_transform_math.c
 *
 *  Created on: Sep 10, 2017
 *      Author: dwwang
 */

#include "luna_sim/luna.h"
#include "luna_sim/luna_transform_math.h"
#include "luna_sim/fft/luna_fft.h"
#include "luna_sim/common/common.h"
#include "luna_sim/common/check.h"

#define MAX_TRANSFORM_SIZE				(512)

int32_t LUNA_API_SIM(luna_cfft_i32o32)(const int32_t *src, int32_t *dst, int32_t *fft_rshift, e_fft_points points)
{
	
	{
		const int N = points;
		int i = 0;
		complex cpx_input[MAX_TRANSFORM_SIZE];
		complex cpx_output[MAX_TRANSFORM_SIZE];

		memset(cpx_input, 0, sizeof(cpx_input));
		for (i = 0; i < N; i++)
		{
			cpx_input[i].real = src[2 * i + 0];
			cpx_input[i].imag = src[2 * i + 1];;
		}

		*fft_rshift	= luna_cfft(cpx_input, cpx_output, N, 0, 0, 0);

		for (i = 0; i < N; i++)
		{
			dst[2 * i + 0] = luna_saturate_q63_to_q31(cpx_output[i].real);
			dst[2 * i + 1] = luna_saturate_q63_to_q31(cpx_output[i].imag);
		}

		return 0;
	}
}

int32_t LUNA_API_SIM(luna_cifft_i32o32)(const int32_t *src, int32_t *dst, int32_t *fft_rshift, e_fft_points points)
{
	
	{
		const int N = points;
		int i = 0;
		complex cpx_input[MAX_TRANSFORM_SIZE];
		complex cpx_output[MAX_TRANSFORM_SIZE];

		memset(cpx_input, 0, sizeof(cpx_input));
		for (i = 0; i < N; i++)
		{
			cpx_input[i].real = src[2 * i + 0];
			cpx_input[i].imag = src[2 * i + 1];
		}
		*fft_rshift	= luna_cfft(cpx_input, cpx_output, N, 1, *fft_rshift, 0);
		for (i = 0; i < N; i++)
		{
			dst[2 * i + 0] = luna_saturate_q63_to_q31(cpx_output[i].real);
			dst[2 * i + 1] = luna_saturate_q63_to_q31(cpx_output[i].imag);
		}
		return 0;
	}
}

int32_t LUNA_API_SIM(luna_rfft_i32o32)(const int32_t *src, int32_t *dst, int32_t *fft_rshift, e_fft_points points)
{
	
	complex cpx_input[MAX_TRANSFORM_SIZE];
	complex cpx_output[MAX_TRANSFORM_SIZE];
	const int N = points;
	int i = 0;
	uint32_t out_points = N/2;

	memset(cpx_input, 0, sizeof(cpx_input));
	for (i = 0; i < N; i++)
	{
		cpx_input[i].real = src[i];
		cpx_input[i].imag = 0.f;
	}

	*fft_rshift	= luna_cfft(cpx_input, cpx_output, N, 0, 0, 0);

	for (i = 0; i < out_points; i++)
	{
		dst[2 * i + 0] = luna_saturate_q63_to_q31(cpx_output[i].real);
		dst[2 * i + 1] = luna_saturate_q63_to_q31(cpx_output[i].imag);
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_rifft_i32o32)(const int32_t *src, int32_t *dst, int32_t *fft_rshift, e_fft_points points)
{
	
	const int N = points;
	int i = 0;
	complex cpx_input[MAX_TRANSFORM_SIZE];
	complex cpx_output[MAX_TRANSFORM_SIZE];

	memset(cpx_input, 0, sizeof(cpx_input));
	for (i = 0; i < N/2; i++)
	{
		cpx_input[i].real = src[2 * i + 0];
		cpx_input[i].imag = src[2 * i + 1];
	}

	cpx_input[N/2].real = 0;
	cpx_input[N/2].imag = 0;

	for (i = N/2+1; i < N; i++)
	{
		cpx_input[i].real = src[2 * (N - i) + 0];
		cpx_input[i].imag = (long long)(src[2 * (N - i) + 1]);
	}

	*fft_rshift	= luna_cfft(cpx_input, cpx_output, N, 1, *fft_rshift, 1);

	for (i = 0; i < N; i++)
	{
		dst[i] = luna_saturate_q63_to_q31(cpx_output[i].real);
	}

	return 0;
}
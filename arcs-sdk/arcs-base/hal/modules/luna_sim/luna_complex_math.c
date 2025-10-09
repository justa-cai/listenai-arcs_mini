/*
 * luna_complex_math.c
 *
 *  Created on: Sep 10, 2017
 *      Author: dwwang
 */

#include "luna_sim/luna.h"
#include "luna_sim/luna_complex_math.h"
#include "luna_sim/common/common.h"
#include "luna_sim/common/check.h"

int32_t LUNA_API_SIM(luna_clx_mul_i32i32o8)(const int32_t *src1, const int32_t *src2, int8_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_CLX(src1, src2, dst, size * 2, size * 2, size * 2, shift);
	{
		uint32_t i;
		q63_t sum1[2], sum2[2];
		q63_t f_dst;

		for (i = 0; i < size; i++) 
		{
			q63_t a, b, c, d, ac, ad, bc, bd;
			sum1[0] = sum1[1] = sum2[0] = sum2[1] = 0;
			a = *(src1 + 2 * i);
			b = *(src1 + 2 * i + 1);
			c = *(src2 + 2 * i);
			d = *(src2 + 2 * i + 1);
			ac = a * c;
			ad = a * d;
			bc = b * c;
			bd = b * d;

			luna_q63_add_new(sum1, ad);
			luna_q63_add_new(sum1, bc);
			f_dst = luna_q63_shift(sum1, shift);
			*(dst + 2 * i + 1) = luna_saturation_int64_to_int8(f_dst);

			luna_q63_add_new(sum2, ac);
			luna_q63_add_new(sum2, -1 * bd);
			f_dst = luna_q63_shift(sum2, shift);
			*(dst + 2 * i) = luna_saturation_int64_to_int8(f_dst);
		}
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_clx_mul_i32i32o32)(const int32_t *src1, const int32_t *src2, int32_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_CLX(src1, src2, dst, size * 2, size * 2, size * 2, shift);
	{
		uint32_t i;
		q63_t sum1[2], sum2[2];
		q63_t f_dst;

		for (i = 0; i < size; i++)
		{
			q63_t a, b, c, d, ac, ad, bc, bd;
			sum1[0] = sum1[1] = sum2[0] = sum2[1] = 0;
			a = *(src1 + 2 * i);
			b = *(src1 + 2 * i + 1);
			c = *(src2 + 2 * i);
			d = *(src2 + 2 * i + 1);
			ac = a * c;
			ad = a * d;
			bc = b * c;
			bd = b * d;

			luna_q63_add_new(sum1, ad);
			luna_q63_add_new(sum1, bc);
			f_dst = luna_q63_shift(sum1, shift);
			*(dst + 2 * i + 1) = luna_saturation_int64_to_int32(f_dst);

			luna_q63_add_new(sum2, ac);
			luna_q63_add_new(sum2, -1 * bd);
			f_dst = luna_q63_shift(sum2, shift);
			*(dst + 2 * i) = luna_saturation_int64_to_int32(f_dst);
		}
	}

	return 0;
}

/*
 * C=A*conj(B)
 */
int32_t LUNA_API_SIM(luna_clx_conj_mul_i32i32o32)(const int32_t *src1, const int32_t *src2, int32_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_CLX(src1, src2, dst, size * 2, size * 2, size * 2, shift);
	{
		uint32_t i;
		q63_t sum1[2], sum2[2];
		q63_t f_dst;

		for (i = 0; i < size; i++)
		{
			q63_t a, b, c, d, ac, ad, bc, bd;
			sum1[0] = sum1[1] = sum2[0] = sum2[1] = 0;
			a = *(src1 + 2 * i);
			b = *(src1 + 2 * i + 1);
			c = *(src2 + 2 * i);
			d = (int64_t)0 - (int64_t)(*(src2 + 2 * i + 1));
			ac = a * c;
			ad = a * d;
			bc = b * c;
			bd = b * d;

			luna_q63_add_new(sum1, ad);
			luna_q63_add_new(sum1, bc);
			f_dst = luna_q63_shift(sum1, shift);
			*(dst + 2 * i + 1) = luna_saturation_int64_to_int32(f_dst);

			luna_q63_add_new(sum2, ac);
			luna_q63_add_new(sum2, -1 * bd);
			f_dst = luna_q63_shift(sum2, shift);
			*(dst + 2 * i) = luna_saturation_int64_to_int32(f_dst);
		}
	}

	return 0;
}

#if (defined(WIN32))
#else
__attribute__((optimize("O0")))
#endif
int32_t LUNA_API_SIM(luna_clx_mul_real_i32i32o32)(const int32_t *src1, const int32_t *src2, int32_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_CLX(src1, src2, dst, size * 2, size, size * 2, shift);
	{
		uint32_t i = 0;
		q63_t real = 0;
		q63_t img = 0;
		q63_t real_dst = 0;
		q63_t img_dst = 0;
		q63_t scale = 0;

		for (i = 0; i < size; i++)
		{

			real = *(src1 + 2 * i);
			img = *(src1 + 2 * i + 1);
			scale = *(src2 + i);
			real_dst = real * scale;
			img_dst = img * scale;

			real_dst = shfit_floor_x05_int64(real_dst, shift);
			img_dst = shfit_floor_x05_int64(img_dst, shift);

			*(dst + 2 * i + 0) = luna_saturate_q63_to_q31(real_dst);
			*(dst + 2 * i + 1) = luna_saturate_q63_to_q31(img_dst);
		}
	}
	return 0;
}

int32_t LUNA_API_SIM(luna_clx_mul_out_real_i32i32o32)(const int32_t *src1, const int32_t *src2, int32_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_CLX(src1, src2, dst, size * 2, size, size * 2, shift);
	{
		uint32_t i;
		q63_t sum1[2], sum2[2];
		q63_t f_dst;

		for (i = 0; i < size; i++)
		{
			q63_t a, b, c, d, ac, ad, bc, bd;
			sum1[0] = sum1[1] = sum2[0] = sum2[1] = 0;
			a = *(src1 + 2 * i);
			b = *(src1 + 2 * i + 1);
			c = *(src2 + 2 * i);
			d = *(src2 + 2 * i + 1);
			ac = a * c;
			ad = a * d;
			bc = b * c;
			bd = b * d;

			luna_q63_add_new(sum2, ac);
			luna_q63_add_new(sum2, -1 * bd);
			f_dst = luna_q63_shift(sum2, shift);
			*(dst + i) = luna_saturation_int64_to_int32(f_dst);
		}
	}

	return 0;
}

#if (defined(WIN32))
#else
__attribute__((optimize("O0")))
#endif
int32_t LUNA_API_SIM(luna_conjugate_i8o8)(const int8_t *src, int8_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_CLX(src, src, dst, size * 2, size*2, size * 2, shift);
	{
		uint32_t i = 0;
		q63_t real_dst = 0;
		q63_t img_dst = 0;

		for (i = 0; i < size; i++)
		{
			real_dst = (q63_t)src[i * 2 + 0];
			img_dst = 0 - (q63_t)src[i * 2 + 1];
			real_dst = shfit_floor_x05_int64(real_dst, shift);
			img_dst = shfit_floor_x05_int64(img_dst, shift);
			*(dst + 2 * i + 0) = luna_saturate_q63_to_q7(real_dst);
			*(dst + 2 * i + 1) = luna_saturate_q63_to_q7(img_dst);
		}
	}
	return 0;
}

#if (defined(WIN32))
#else
__attribute__((optimize("O0")))
#endif
int32_t LUNA_API_SIM(luna_conjugate_i32o32)(const int32_t *src, int32_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_CLX(src, src, dst, size * 2, size * 2, size * 2, shift);
	{
		uint32_t i = 0;
		q63_t real_dst = 0;
		q63_t img_dst = 0;

		for (i = 0; i < size; i++)
		{
			real_dst = (q63_t)src[i * 2 + 0];
			img_dst = 0 - (q63_t)src[i * 2 + 1];
			real_dst = shfit_floor_x05_int64(real_dst, shift);
			img_dst = shfit_floor_x05_int64(img_dst, shift);
			*(dst + 2 * i + 0) = luna_saturate_q63_to_q31(real_dst);
			*(dst + 2 * i + 1) = luna_saturate_q63_to_q31(img_dst);
		}
	}
	return 0;
}

int32_t LUNA_API_SIM(luna_power_spectrum_i32o32)(const int32_t *src, int32_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_CLX(src, src, dst, size * 2, size * 2, size, shift);
	{
		uint32_t i = 0;
		q63_t real = 0;
		q63_t img = 0;
		q63_t sum[2];
		q63_t f_dst;

		for (i = 0; i < size; i++)
		{
			sum[0] = 0;
			sum[1] = 0;
			real = (q63_t)(src[i * 2 + 0]);
			img = (q63_t)(src[i * 2 + 1]);
			luna_q63_add_new(sum, real * real);
			luna_q63_add_new(sum, img * img);
			f_dst = luna_q63_shift(sum, shift);
			*(dst + i) = luna_saturation_int64_to_int32(f_dst);
		}
	}
	return 0;
}

int32_t LUNA_API_SIM(luna_power_spectrum_i32o64)(const int32_t *src, int64_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_CLX(src, src, dst, size * 2, size * 2, size, shift);
	{
		uint32_t i = 0;
		q63_t real = 0;
		q63_t img = 0;
		q63_t sum[2];
		q63_t f_dst;

		for (i = 0; i < size; i++)
		{
			sum[0] = 0;
			sum[1] = 0;
			real = (q63_t)(src[i * 2 + 0]);
			img = (q63_t)(src[i * 2 + 1]);
			luna_q63_add_new(sum, real * real);
			luna_q63_add_new(sum, img * img);
			f_dst = luna_q63_shift(sum, shift);
			*(dst + i) = (int64_t)(f_dst);
		}
	}
	return 0;
}

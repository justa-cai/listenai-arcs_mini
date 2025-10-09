/*
 *luna_basic_math.c
 *
 *  Created on: Sep 10, 2017
 *      Author: dwwang
 */
#include <math.h>
#include "luna_sim/luna.h"
#include "luna_sim/luna_basic_math.h"
#include "luna_sim/common/common.h"
#include "luna_sim/common/check.h"

//////////////////////////
int32_t LUNA_API_SIM(luna_add_i8i8o8)(const int8_t *src1, const int8_t *src2, int8_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_2SRC1DST(src1, src2, dst, size, size, size, shift);
	{
		uint32_t i = 0;
		int64_t d, d1, d2;
		for (i = 0; i < size; i++) {
			d1 = (int64_t)*(src1 + i);
			d2 = (int64_t)*(src2 + i);
			d = d1 + d2;
			d = shfit_floor_x05_int64(d, shift);
			*(dst + i) = luna_saturate_q63_to_q7(d);
		}
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_add_i8i8o32)(const int8_t* src1, const int8_t* src2, int32_t* dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_2SRC1DST(src1, src2, dst, size, size, size, shift);
	{
		uint32_t i = 0;
		int64_t d, d1, d2;
		for (i = 0; i < size; i++) {
			d1 = (int64_t)*(src1 + i);
			d2 = (int64_t)*(src2 + i);
			d = d1 + d2;
			d = shfit_floor_x05_int64(d, shift);
			*(dst + i) = luna_saturate_q63_to_q31(d);
		}
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_add_i32i32o8)(const int32_t *src1, const int32_t *src2, int8_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_2SRC1DST(src1, src2, dst, size, size, size, shift);
	{
		uint32_t i = 0;
		int64_t d, d1, d2;
		for (i = 0; i < size; i++) {
			d1 = (int64_t)*(src1 + i);
			d2 = (int64_t)*(src2 + i);
			d = d1 + d2;
			d = shfit_floor_x05_int64(d, shift);
			*(dst + i) = luna_saturate_q63_to_q7(d);
		}
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_add_i32i32o32)(const int32_t *src1, const int32_t *src2, int32_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_2SRC1DST(src1, src2, dst, size, size, size, shift);
	{
		uint32_t i = 0;
		int64_t d, d1, d2;
		for (i = 0; i < size; i++) {
			d1 = (int64_t)*(src1 + i);
			d2 = (int64_t)*(src2 + i);
			d = d1 + d2;
			d = shfit_floor_x05_int64(d, shift);
			*(dst + i) = luna_saturate_q63_to_q31(d);
		}
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_sub_i8i8o8)(const int8_t *src1, const int8_t *src2, int8_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_2SRC1DST(src1, src2, dst, size, size, size, shift);
	{
		uint32_t i = 0;
		int64_t d, d1, d2;
		for (i = 0; i < size; i++) {
			d1 = (int64_t)*(src1 + i);
			d2 = (int64_t)*(src2 + i);
			d = d1 - d2;
			d = shfit_floor_x05_int64(d, shift);
			*(dst + i) = luna_saturate_q63_to_q7(d);
		}
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_sub_i8i8o32)(const int8_t* src1, const int8_t* src2, int32_t* dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_2SRC1DST(src1, src2, dst, size, size, size, shift);
	{
		uint32_t i = 0;
		int64_t d, d1, d2;
		for (i = 0; i < size; i++) {
			d1 = (int64_t)*(src1 + i);
			d2 = (int64_t)*(src2 + i);
			d = d1 - d2;
			d = shfit_floor_x05_int64(d, shift);
			*(dst + i) = luna_saturate_q63_to_q31(d);
		}
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_sub_i32i32o8)(const int32_t *src1, const int32_t *src2, int8_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_2SRC1DST(src1, src2, dst, size, size, size, shift);
	{
		uint32_t i = 0;
		int64_t d, d1, d2;
		for (i = 0; i < size; i++) {
			d1 = (int64_t)*(src1 + i);
			d2 = (int64_t)*(src2 + i);
			d = d1 - d2;
			d = shfit_floor_x05_int64(d, shift);
			*(dst + i) = luna_saturate_q63_to_q7(d);
		}
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_sub_i32i32o32)(const int32_t *src1, const int32_t *src2, int32_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_2SRC1DST(src1, src2, dst, size, size, size, shift);
	{
		uint32_t i = 0;
		int64_t d, d1, d2;
		for (i = 0; i < size; i++) {
			d1 = (int64_t)*(src1 + i);
			d2 = (int64_t)*(src2 + i);
			d = d1 - d2;
			d = shfit_floor_x05_int64(d, shift);
			*(dst + i) = luna_saturate_q63_to_q31(d);
		}
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_mul_i8i8o8)(const int8_t *src1, const int8_t *src2, int8_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_2SRC1DST(src1, src2, dst, size, size, size, shift);
	{
		uint32_t i = 0;
		int64_t d, d1, d2;
		for (i = 0; i < size; i++) 
		{
			d1 = (int64_t)*(src1 + i);
			d2 = (int64_t)*(src2 + i);
			d = d1 * d2;
			d = shfit_floor_x05_int64(d, shift);
			*(dst + i) = luna_saturate_q63_to_q7(d);
		}
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_mul_i8i8o32)(const int8_t *src1, const int8_t *src2, int32_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_2SRC1DST(src1, src2, dst, size, size, size, shift);

	{
		uint32_t i = 0;
		int64_t d, d1, d2;
		for (i = 0; i < size; i++) 
		{
			d1 = (int64_t)*(src1 + i);
			d2 = (int64_t)*(src2 + i);
			d = d1 * d2;
			d = shfit_floor_x05_int64(d, shift);
			*(dst + i) = luna_saturate_q63_to_q31(d);
		}
	}

	return 0;
}


int32_t LUNA_API_SIM(luna_mul_i32i32o8)(const int32_t *src1, const int32_t *src2, int8_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_2SRC1DST(src1, src2, dst, size, size, size, shift);

	{
		uint32_t i = 0;
		int64_t d, d1, d2;
		for (i = 0; i < size; i++) 
		{
			d1 = (int64_t)*(src1 + i);
			d2 = (int64_t)*(src2 + i);
			d = d1 * d2;
			d = shfit_floor_x05_int64(d, shift);
			*(dst + i) = luna_saturate_q63_to_q7(d);
		}
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_mul_i32i32o32)(const int32_t *src1, const int32_t *src2, int32_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_2SRC1DST(src1, src2, dst, size, size, size, shift);
	{
		uint32_t i = 0;
		int64_t d, d1, d2;
		for (i = 0; i < size; i++) 
		{
			d1 = (int64_t)*(src1 + i);
			d2 = (int64_t)*(src2 + i);
			d = d1 * d2;
			d = shfit_floor_x05_int64(d, shift);
			*(dst + i) = luna_saturate_q63_to_q31(d);
		}
	}

	return 0;
}


int32_t LUNA_API_SIM(luna_scale_i8i8o8)(const int8_t *src1, const int8_t scalar, int8_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_1SRC1DST(src1, dst, size, size, shift);
	{
		uint32_t i = 0;
		int64_t d, d1, d2;
		for (i = 0; i < size; i++)
		{
			d1 = (int64_t)*(src1 + i);
			d2 = (int64_t)scalar;
			d = d1 * d2;
			d = shfit_floor_x05_int64(d, shift);
			*(dst + i) = luna_saturate_q63_to_q7(d);
		}
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_scale_i8i8o32)(const int8_t *src1, const int8_t scalar, int32_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_1SRC1DST(src1, dst, size, size, shift);
	{
		uint32_t i = 0;
		int64_t d, d1, d2;
		for (i = 0; i < size; i++)
		{
			d1 = (int64_t)*(src1 + i);
			d2 = (int64_t)scalar;
			d = d1 * d2;
			d = shfit_floor_x05_int64(d, shift);
			*(dst + i) = luna_saturate_q63_to_q31(d);
		}
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_scale_i32i32o8)(const int32_t *src1, const int32_t scalar, int8_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_1SRC1DST(src1, dst, size, size, shift);
	{
		uint32_t i = 0;
		int64_t d, d1, d2;
		for (i = 0; i < size; i++)
		{
			d1 = (int64_t)*(src1 + i);
			d2 = (int64_t)scalar;
			d = d1 * d2;
			d = shfit_floor_x05_int64(d, shift);
			*(dst + i) = luna_saturate_q63_to_q7(d);
		}
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_scale_i32i32o32)(const int32_t *src1, const int32_t scalar, int32_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_1SRC1DST(src1, dst, size, size, shift);
	{
		uint32_t i = 0;
		int64_t d, d1, d2;
		for (i = 0; i < size; i++)
		{
			d1 = (int64_t)*(src1 + i);
			d2 = (int64_t)scalar;
			d = d1 * d2;
			d = shfit_floor_x05_int64(d, shift);
			*(dst + i) = luna_saturate_q63_to_q31(d);
		}
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_dot_prod_i8i8o8)(const int8_t *src1, const int8_t *src2, int8_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_2SRC1DST(src1, src2, dst, size, size, 1, shift);
	{
		uint32_t i = 0;
		int64_t d = 0, d1, d2;
		for (i = 0; i < size; i++)
		{
			d1 = (int64_t)*(src1 + i);
			d2 = (int64_t)*(src2 + i);
			d += d1 * d2;
		}
		d = shfit_floor_x05_int64(d, shift);
		*dst = luna_saturate_q63_to_q7(d);
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_dot_prod_i8i8o32)(const int8_t *src1, const int8_t *src2, int32_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_2SRC1DST(src1, src2, dst, size, size, 1, shift);
	{
		uint32_t i = 0;
		int64_t d = 0, d1, d2;
		for (i = 0; i < size; i++)
		{
			d1 = (int64_t)*(src1 + i);
			d2 = (int64_t)*(src2 + i);
			d += d1 * d2;
		}
		d = shfit_floor_x05_int64(d, shift);
		*dst = luna_saturate_q63_to_q31(d);
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_dot_prod_i32i32o8)(const int32_t *src1, const int32_t *src2, int8_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_DOT_32(src1, src2, dst, size, shift);
	{
		uint32_t i = 0;
		q63_t d = 0, d1, d2;
		q63_t sum[2] = { 0,0 };
		q63_t f_dst;
		for (i = 0; i < size; i++)
		{
			d1 = (q63_t)*(src1 + i);
			d2 = (q63_t)*(src2 + i);
			d = d1 * d2;
			luna_q63_add_new_v2(sum, d);
		}
		f_dst = luna_q63_shift_v2(sum, shift);
		*dst = luna_saturation_int64_to_int8(f_dst);
	}
#if 0
	{
		uint32_t i = 0;
		int32_t half_size = size >> 1;
		int64_t d = 0, d1, d2;
		int64_t f_dst;
		int64_t sum[2][2] = { {0,0}, {0, 0} };

		for (i = 0; i < half_size; i++)
		{
			d1 = (int64_t)*(src1 + 2 * i);
			d2 = (int64_t)*(src2 + 2 * i);
			d = d1 * d2;
			luna_q63_add_new_v2(sum[0], d);

			d1 = (int64_t)*(src1 + 2 * i + 1);
			d2 = (int64_t)*(src2 + 2 * i + 1);
			d = d1 * d2;
			luna_q63_add_new_v2(sum[1], d);
		}

		if (size - (2 * half_size))
		{
			d1 = (int64_t)*(src1 + size - 1);
			d2 = (int64_t)*(src2 + size - 1);
			d = d1 * d2;
			luna_q63_add_new_v2(sum[0], d);
		}

		luna_add_72bit_new(sum[0], sum[1]);
		f_dst = luna_q63_shift_v2(sum[0], shift);

		*dst = luna_saturation_int64_to_int8(f_dst);
	}
#endif
	return 0;
}

int32_t LUNA_API_SIM(luna_dot_prod_i32i32o32)(const int32_t *src1, const int32_t *src2, int32_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_DOT_32(src1, src2, dst, size, shift);
	{
		uint32_t i = 0;
		q63_t d = 0, d1, d2;
		q63_t sum[2] = { 0,0 };
		q63_t f_dst;
		for (i = 0; i < size; i++)
		{
			d1 = (q63_t)*(src1 + i);
			d2 = (q63_t)*(src2 + i);
			d = d1 * d2;
			luna_q63_add_new_v2(sum, d);
		}
		f_dst = luna_q63_shift_v2(sum, shift);
		*dst = luna_saturation_int64_to_int32(f_dst);
	}
#if 0
	{
		uint32_t i = 0;
		int32_t half_size = size >> 1;
		int64_t d = 0, d1, d2;
		int64_t f_dst;
		int64_t sum[2][2] = { {0,0}, {0, 0} };

		for (i = 0; i < half_size; i++)
		{
			d1 = (int64_t)*(src1 + 2 * i);
			d2 = (int64_t)*(src2 + 2 * i);
			d = d1 * d2;
			luna_q63_add_new_v2(sum[0], d);

			d1 = (int64_t)*(src1 + 2 * i + 1);
			d2 = (int64_t)*(src2 + 2 * i + 1);
			d = d1 * d2;
			luna_q63_add_new_v2(sum[1], d);
		}

		if (size - (2 * half_size))
		{
			d1 = (int64_t)*(src1 + size - 1);
			d2 = (int64_t)*(src2 + size - 1);
			d = d1 * d2;
			luna_q63_add_new_v2(sum[0], d);
		}

		luna_add_72bit_new(sum[0], sum[1]);
		f_dst = luna_q63_shift_v2(sum[0], shift);

		*dst = luna_saturation_int64_to_int32(f_dst);
	}
#endif
	return 0;
}

int32_t LUNA_API_SIM(luna_dot_prod_i32i32o64)(const int32_t *src1, const int32_t *src2, int64_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_DOT_32(src1, src2, dst, size, shift);
	{
		uint32_t i = 0;
		q63_t d = 0, d1, d2;
		q63_t sum[2] = { 0,0 };

		for (i = 0; i < size; i++)
		{
			d1 = (q63_t)*(src1 + i);
			d2 = (q63_t)*(src2 + i);
			d = d1 * d2;
			luna_q63_add_new_v2(sum, d);
		}
		*dst = (q63_t)luna_q63_shift_v2(sum, shift);
	}
#if 0
	{
		uint32_t i = 0;
		int32_t half_size = size >> 1;
		int64_t d = 0, d1, d2;
		int64_t sum[2][2] = { {0,0}, {0, 0} };

		for (i = 0; i < half_size; i++)
		{
			d1 = (int64_t)*(src1 + 2 * i);
			d2 = (int64_t)*(src2 + 2 * i);
			d = d1 * d2;
			luna_q63_add_new_v2(sum[0], d);

			d1 = (int64_t)*(src1 + 2 * i + 1);
			d2 = (int64_t)*(src2 + 2 * i + 1);
			d = d1 * d2;
			luna_q63_add_new_v2(sum[1], d);
		}

		if (size - (2 * half_size))
		{
			d1 = (int64_t)*(src1 + size - 1);
			d2 = (int64_t)*(src2 + size - 1);
			d = d1 * d2;
			luna_q63_add_new_v2(sum[0], d);
		}

		luna_add_72bit_new(sum[0], sum[1]);

		*dst = (int64_t)luna_q63_shift_v2(sum[0], shift);
	}
#endif
	return 0;
}

int32_t LUNA_API_SIM(luna_vector_sum_i8o8)(const int8_t *src, int8_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_1SRC1DST(src, dst, size, 1, shift);
	{
		uint32_t i = 0;
		int64_t d = 0, d1;
		for (i = 0; i < size; i++)
		{
			d1 = (int64_t)*(src + i);
			d += d1;
		}
		d = shfit_floor_x05_int64(d, shift);
		*dst = luna_saturate_q63_to_q7(d);
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_vector_sum_i8o32)(const int8_t *src, int32_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_1SRC1DST(src, dst, size, 1, shift);
	{
		uint32_t i = 0;
		int64_t d = 0, d1;
		for (i = 0; i < size; i++)
		{
			d1 = (int64_t)*(src + i);
			d += d1;
		}
		d = shfit_floor_x05_int64(d, shift);
		*dst = luna_saturate_q63_to_q31(d);
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_vector_sum_i32o8)(const int32_t *src, int8_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_1SRC1DST(src, dst, size, 1, shift);
	{
		uint32_t i = 0;
		int64_t d = 0, d1;
		for (i = 0; i < size; i++)
		{
			d1 = (int64_t)*(src + i);
			d += d1;
		}
		d = shfit_floor_x05_int64(d, shift);
		*dst = luna_saturate_q63_to_q7(d);
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_vector_sum_i32o32)(const int32_t *src, int32_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_1SRC1DST(src, dst, size, 1, shift);
	{
		uint32_t i = 0;
		int64_t d = 0, d1;
		for (i = 0; i < size; i++)
		{
			d1 = (int64_t)*(src + i);
			d += d1;
		}
		d = shfit_floor_x05_int64(d, shift);
		*dst = luna_saturate_q63_to_q31(d);
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_vector_sum_i32o64)(const int32_t *src, int64_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_1SRC1DST(src, dst, size, 1, shift);
	{
		uint32_t i = 0;
		int64_t d = 0, d1;
		for (i = 0; i < size; i++)
		{
			d1 = (int64_t)*(src + i);
			d += d1;
		}
		*dst = shfit_floor_x05_int64(d, shift);
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_offset_i8i8o8)(const  int8_t *src, const int8_t offset, int8_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_1SRC1DST(src, dst, size, size, shift);
	{
		int64_t d;
		uint32_t i;
		for (i = 0; i < size; i++)
		{
			d = *(src + i);
			d = d + offset;
			d = shfit_floor_x05_int64(d, shift);
			*(dst + i) = luna_saturate_q63_to_q7(d);
		}
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_offset_i8i8o32)(const  int8_t *src, const int8_t offset, int32_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_1SRC1DST(src, dst, size, size, shift);
	{
		int64_t d;
		uint32_t i;
		for (i = 0; i < size; i++)
		{
			d = *(src + i);
			d = d + offset;
			d = shfit_floor_x05_int64(d, shift);
			*(dst + i) = luna_saturate_q63_to_q31(d);
		}
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_offset_i32i32o8)(const  int32_t *src, const int32_t offset, int8_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_1SRC1DST(src, dst, size, size, shift);
	{
		int64_t d;
		uint32_t i;
		for (i = 0; i < size; i++)
		{
			d = *(src + i);
			d = d + offset;
			d = shfit_floor_x05_int64(d, shift);
			*(dst + i) = luna_saturate_q63_to_q7(d);
		}
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_offset_i32i32o32)(const  int32_t *src, const int32_t offset, int32_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_1SRC1DST(src, dst, size, size, shift);
	{
		int64_t d;
		uint32_t i;
		for (i = 0; i < size; i++)
		{
			d = *(src + i);
			d = d + offset;
			d = shfit_floor_x05_int64(d, shift);
			*(dst + i) = luna_saturate_q63_to_q31(d);
		}
	}

	return 0;
}

/**
 * cmp_mode: 0:> 1:>= 2:< 3:<= 4:==
 */

#define LUNA_CALC_CMP_VV(src1, src2, dst, size, cmp_mode) { \
	int32_t i; \
	switch (cmp_mode) \
	{ \
	case LUNA_CMP_GREATER_THAN: \
	{ \
		for (i = 0; i < size; i++) \
		{ \
			if (*(src1 + i) > *(src2 + i)) \
			{ \
				*(dst + i) = 1; \
			} \
			else \
			{ \
				*(dst + i) = 0; \
			} \
		} \
	} \
		break; \
	case LUNA_CMP_GREATER_OR_EQUAL: \
	{ \
		for (i = 0; i < size; i++) \
		{ \
			if (*(src1 + i) >= *(src2 + i)) \
			{ \
				*(dst + i) = 1; \
			} \
			else \
			{ \
				*(dst + i) = 0; \
			} \
		} \
	} \
		break; \
	case LUNA_CMP_LESS_THAN: \
	{ \
		for (i = 0; i < size; i++) \
		{ \
			if (*(src1 + i) < *(src2 + i)) \
			{ \
				*(dst + i) = 1; \
			} \
			else \
			{ \
				*(dst + i) = 0; \
			} \
		} \
	} \
		break; \
	case LUNA_CMP_LESS_OR_EQUAL: \
	{ \
		for (i = 0; i < size; i++) \
		{ \
			if (*(src1 + i) <= *(src2 + i)) \
			{ \
				*(dst + i) = 1; \
			} \
			else \
			{ \
				*(dst + i) = 0; \
			} \
		} \
	} \
		break; \
	case LUNA_CMP_EQUAL: \
	{ \
		for (i = 0; i < size; i++) \
		{ \
			if (*(src1 + i) == *(src2 + i)) \
			{ \
				*(dst + i) = 1; \
			} \
			else \
			{ \
				*(dst + i) = 0; \
			} \
		} \
	} \
		break; \
	} \
}

#define LUNA_CALC_CMP_VS(src1, scalar, dst, size, cmp_mode) { \
	int32_t i; \
	switch (cmp_mode) \
	{ \
	case LUNA_CMP_GREATER_THAN: \
	{ \
		for (i = 0; i < size; i++) \
		{ \
			if (*(src1 + i) > scalar) \
			{ \
				*(dst + i) = 1; \
			} \
			else \
			{ \
				*(dst + i) = 0; \
			} \
		} \
	} \
		break; \
	case LUNA_CMP_GREATER_OR_EQUAL: \
	{ \
		for (i = 0; i < size; i++) \
		{ \
			if (*(src1 + i) >= scalar) \
			{ \
				*(dst + i) = 1; \
			} \
			else \
			{ \
				*(dst + i) = 0; \
			} \
		} \
	} \
		break; \
	case LUNA_CMP_LESS_THAN: \
	{ \
		for (i = 0; i < size; i++) \
		{ \
			if (*(src1 + i) < scalar) \
			{ \
				*(dst + i) = 1; \
			} \
			else \
			{ \
				*(dst + i) = 0; \
			} \
		} \
	} \
		break; \
	case LUNA_CMP_LESS_OR_EQUAL: \
	{ \
		for (i = 0; i < size; i++) \
		{ \
			if (*(src1 + i) <= scalar) \
			{ \
				*(dst + i) = 1; \
			} \
			else \
			{ \
				*(dst + i) = 0; \
			} \
		} \
	} \
		break; \
	case LUNA_CMP_EQUAL: \
	{ \
		for (i = 0; i < size; i++) \
		{ \
			if (*(src1 + i) == scalar) \
			{ \
				*(dst + i) = 1; \
			} \
			else \
			{ \
				*(dst + i) = 0; \
			} \
		} \
	} \
		break; \
	} \
}

int32_t LUNA_API_SIM(luna_cmp_vv_i8i8o8)(const int8_t *src1, const int8_t *src2, int8_t *dst, uint32_t size, uint32_t cmp_mode)
{
	LUNA_CHECK_PARAM_VEC_CMP_VV(src1, src2, dst, size, cmp_mode);

	LUNA_CALC_CMP_VV(src1, src2, dst, size, cmp_mode);

	return 0;
}

int32_t LUNA_API_SIM(luna_cmp_vv_i32i32o32)(const int32_t *src1, const int32_t *src2, int32_t *dst, uint32_t size, uint32_t cmp_mode)
{
	LUNA_CHECK_PARAM_VEC_CMP_VV(src1, src2, dst, size, cmp_mode);

	LUNA_CALC_CMP_VV(src1, src2, dst, size, cmp_mode);

	return 0;
}

int32_t LUNA_API_SIM(luna_cmp_vs_i8i8o8)(const int8_t *src1, const int8_t scalar, int8_t *dst, uint32_t size, uint32_t cmp_mode)
{
	LUNA_CHECK_PARAM_VEC_CMP_VS(src1, dst, size, cmp_mode);

	LUNA_CALC_CMP_VS(src1, scalar, dst, size, cmp_mode);

	return 0;
}

int32_t LUNA_API_SIM(luna_cmp_vs_i32i32o32)(const int32_t *src1, const int32_t scalar, int32_t *dst, uint32_t size, uint32_t cmp_mode)
{
	LUNA_CHECK_PARAM_VEC_CMP_VS(src1, dst, size, cmp_mode);

	LUNA_CALC_CMP_VS(src1, scalar, dst, size, cmp_mode);

	return 0;
}


int32_t LUNA_API_SIM(luna_max_i8o32)(const int8_t *src1, int32_t* dst, uint32_t size)
{
	LUNA_CHECK_PARAM_VEC_1SRC1DST(src1, dst, size, 2, 0);
	{
		int32_t i, idx;
		int32_t max_value = *src1;
		idx = 0;
		for (i = 0; i < size; i++) 
		{
			if (max_value < *(src1 + i))
			{
				max_value = *(src1 + i);
				idx = i;
			}
		}
		*(dst) = max_value;
		*(dst + 1) = idx;
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_max_i32o32)(const int32_t *src1, int32_t* dst, uint32_t size)
{
	LUNA_CHECK_PARAM_VEC_1SRC1DST(src1, dst, size, 2, 0);
	{
		int32_t i, idx;
		int32_t max_value = *src1;
		idx = 0;
		for (i = 0; i < size; i++) 
		{
			if (max_value < *(src1 + i))
			{
				max_value = *(src1 + i);
				idx = i;
			}
		}
		*(dst) = max_value;
		*(dst + 1) = idx;
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_min_i8o32)(const int8_t *src1, int32_t *dst, uint32_t size)
{
	LUNA_CHECK_PARAM_VEC_1SRC1DST(src1, dst, size, 2, 0);
	{
		int32_t i, idx;
		int32_t min_value = *src1;
		idx = 0;
		for (i = 0; i < size; i++)
		{
			if (min_value > *(src1 + i))
			{
				min_value = *(src1 + i);
				idx = i;
			}
		}
		*(dst) = min_value;
		*(dst + 1) = idx;
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_min_i32o32)(const int32_t *src1, int32_t* dst, uint32_t size)
{
	LUNA_CHECK_PARAM_VEC_1SRC1DST(src1, dst, size, 2, 0);
	{
		int32_t i, idx;
		int32_t min_value = *src1;
		idx = 0;
		for (i = 0; i < size; i++) 
		{
			if (min_value > *(src1 + i))
			{
				min_value = *(src1 + i);
				idx = i;
			}
		}
		*(dst) = min_value;
		*(dst + 1) = idx;
	}

	return 0;
}

// shift = q_out - (q_src1 - q_src2);
#if (defined(WIN32))
#else
__attribute__((optimize("O0")))
#endif
int32_t LUNA_API_SIM(luna_div_i32i32o32)(const int32_t *src1, const int32_t *src2, int32_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_DIV_32(src1, q_src1, src2, q_src2, dst, q_out, size);

	{
		const uint32_t iter = 3;
		const int32_t lut_fracwid = 5;
		const int32_t init_fracwid = 29;
		const int32_t recp_flag = 0;
		int32_t i, j;
		double msb_loc_a, a_scale_factor;
		double divisor, dividend;
		double divisor_abs, a_scale, a_fl, a_rcp;
		double q;

		for (i = 0; i < size; i++)
		{
			dividend = (double)*(src1 + i);
			divisor  = (double)*(src2 + i);
			divisor_abs = (double)divisor;
			if (divisor_abs < 0.0f)
				divisor_abs = -divisor_abs;
			divisor_abs = (float)log2(divisor_abs);
			msb_loc_a = ceil(divisor_abs);
			a_scale_factor = 31 - msb_loc_a;
			a_scale = divisor * pow(2, a_scale_factor);
			a_fl = a_scale / pow(2, 31);
			a_rcp = 1.0 / (floor(a_fl * pow(2, lut_fracwid)) * 1.0 / pow(2, lut_fracwid));
			a_rcp = floor(a_rcp * pow(2, init_fracwid) + 0.5) * 1.0 / pow(2, init_fracwid);
			for (j = 0; j < iter; j++)
			{
				a_rcp = a_rcp * floor((2 - a_fl * a_rcp) * pow(2, 30) + 0.5) / pow(2, 30);
    			a_rcp = floor(a_rcp * pow(2, 29) + 0.5) / pow(2, 29);
			}

			if (0 == recp_flag)
			{
				q = (double)a_rcp * dividend * pow(2, (a_scale_factor - 31));
				if (shift & (1<<6)){
					q = floor(q * pow(2, (shift & (~(1<<6)) )));
				} else {
					q = floor(q * pow(2, shift) + 0.5);
				}
			}
			else
			{
				q = a_rcp;
			}
			*(dst + i) = luna_convert_double_to_q31(q);
		}
	}
	return 0;
}

#if 0 //not support in Mars
int32_t LUNA_API_SIM(luna_mul_i8i4o8)(const int8_t *src1, const int8_t *src2, int8_t *dst, uint32_t length, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_2SRC1DST(src1, src2, dst, length, length/2, length, shift);

	{
		uint32_t i = 0;
		int8_t tmp_buf[256*1024];
		int64_t d, d1, d2;
		src_q3_to_int8((int8_t *)src2, tmp_buf, length);
		for (i = 0; i < length; i++)
		{
			d1 = (int64_t)*(src1 + i);
			d2 = (int64_t)*(tmp_buf + i);
			d = d1 * d2;
			d = shfit_floor_x05_int64(d, shift);
			*(dst + i) = luna_saturate_q63_to_q7(d);
		}
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_mul_i8i4o32)(const int8_t *src1, const int8_t *src2, int32_t *dst, uint32_t length, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_2SRC1DST(src1, src2, dst, length, length/2, length, shift);

	{
		uint32_t i = 0;
		int8_t tmp_buf[256*1024];
		int64_t d, d1, d2;
		src_q3_to_int8((int8_t *)src2, tmp_buf, length);
		for (i = 0; i < length; i++)
		{
			d1 = (int64_t)*(src1 + i);
			d2 = (int64_t)*(tmp_buf + i);
			d = d1 * d2;
			d = shfit_floor_x05_int64(d, shift);
			*(dst + i) = luna_saturate_q63_to_q31(d);
		}
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_mul_i32i8o32)(const int32_t *src1, const int8_t *src2, int32_t *dst, uint32_t length, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_2SRC1DST(src1, src2, dst, length, length, length, shift);

	{
		uint32_t i = 0;
		int64_t d, d1, d2;
		for (i = 0; i < length; i++)
		{
			d1 = (int64_t)*(src1 + i);
			d2 = (int64_t)*(src2 + i);
			d = d1 * d2;
			d = shfit_floor_x05_int64(d, shift);
			*(dst + i) = luna_saturate_q63_to_q31(d);
		}
	}

	return 0;
}

/**
 * Z = a*X + b*Y
 **/
int32_t LUNA_API_SIM(luna_scale_add_i8i8o32)(const int8_t *src1, const int8_t scale_a, const int8_t *src2, const int8_t scale_b, int8_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_2SRC1DST(src1, src2, dst, size, size, size, shift);
	{
		uint32_t i = 0;
		int64_t d, d1, d2;
		for (i = 0; i < size; i++) {
			d1 = (int64_t)(*(src1 + i)) * (int64_t)(scale_a);
			d2 = (int64_t)(*(src2 + i)) * (int64_t)(scale_b);
			d = d1 + d2;
			d = shfit_floor_x05_int64(d, shift);
			*(dst + i) = luna_saturate_q63_to_q7(d);
		}
	}

	return 0;
}

/**
 * Z = a*X + b*Y
 **/
int32_t LUNA_API_SIM(luna_scale_add_i32i32o32)(const int32_t *src1, const int32_t scale_a, const int32_t *src2, const int32_t scale_b, int32_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_2SRC1DST(src1, src2, dst, size, size, size, shift);
	{
		uint32_t i = 0;
		int64_t d = 0, d1, d2;
		int64_t sum[2] = { 0,0 };
		for (i = 0; i < size; i++)
		{
			sum[0] = 0;
			sum[1] = 0;
			d1 = (int64_t)(*(src1 + i)) * (int64_t)(scale_a);
			d2 = (int64_t)(*(src2 + i)) * (int64_t)(scale_b);
			luna_q63_add_new(sum, d1);
			luna_q63_add_new(sum, d2);
			d = luna_q63_shift(sum, shift);
			*(dst + i) = luna_saturation_int64_to_int32(d);
		}
	}
	return 0;
}

/**
 * (M, N)*(1, N)=(M, N)
 */
int32_t LUNA_API_SIM(luna_multi_vec_mul_i8i8o8)(const int8_t *src1, const int8_t *src2, int8_t *dst, uint32_t src2_size, uint32_t multi_times, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_2SRC1DST(src1, src2, dst, multi_times*src2_size, src2_size, multi_times*src2_size, shift);
	{
		int i, j;
		int64_t d, d1, d2;
		for (i = 0; i < multi_times; i++)
		{
			for (j = 0; j < src2_size; j++)
			{
				d1 = (int64_t)*(src1 + i * src2_size + j);
				d2 = (int64_t)*(src2 + j);
				d = d1 * d2;
				d = shfit_floor_x05_int64(d, shift);
				*(dst + i * src2_size + j) = luna_saturate_q63_to_q7(d);
			}
		}
	}
	return 0;
}

int32_t LUNA_API_SIM(luna_multi_vec_mul_i8i8o32)(const int8_t *src1, const int8_t *src2, int32_t *dst, uint32_t src2_size, uint32_t multi_times, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_2SRC1DST(src1, src2, dst, multi_times*src2_size, src2_size, multi_times*src2_size, shift);
	{
		int i, j;
		int64_t d, d1, d2;
		for (i = 0; i < multi_times; i++)
		{
			for (j = 0; j < src2_size; j++)
			{
				d1 = (int64_t)*(src1 + i * src2_size + j);
				d2 = (int64_t)*(src2 + j);
				d = d1 * d2;
				d = shfit_floor_x05_int64(d, shift);
				*(dst + i * src2_size + j) = luna_saturate_q63_to_q31(d);
			}
		}
	}
	return 0;
}

int32_t LUNA_API_SIM(luna_multi_vec_mul_i32i32o32)(const int32_t *src1, const int32_t *src2, int32_t *dst, uint32_t src2_size, uint32_t multi_times, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_2SRC1DST(src1, src2, dst, multi_times*src2_size, src2_size, multi_times*src2_size, shift);
	{
		int i, j;
		int64_t d, d1, d2;
		for (i = 0; i < multi_times; i++)
		{
			for (j = 0; j < src2_size; j++)
			{
				d1 = (int64_t)*(src1 + i * src2_size + j);
				d2 = (int64_t)*(src2 + j);
				d = d1 * d2;
				d = shfit_floor_x05_int64(d, shift);
				*(dst + i * src2_size + j) = luna_saturate_q63_to_q31(d);
			}
		}
	}
	return 0;
}

/**
 * N1 + N2 + ... + Nm
 */
int32_t LUNA_API_SIM(luna_multi_vec_add_i32o32)(const int32_t *src, int32_t *dst, uint32_t dst_size, uint32_t multi_times, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_1SRC1DST(src, dst, multi_times*dst_size, dst_size, shift);

	{
		int i, j;
		int64_t d, d1, d2;

		int64_t vec_tmp_buf[32 * 1024];	//one row max size is 32KB
		memset(vec_tmp_buf, 0, sizeof(vec_tmp_buf));
		for (i = 0; i < multi_times; i++)
		{
			for (j = 0; j < dst_size; j++) {
				d1 = (int64_t)*(src + i * dst_size + j);
				d2 = *(vec_tmp_buf + j);
				*(vec_tmp_buf + j) = d1 + d2;
			}
		}

		for (i = 0; i < dst_size; i++)
		{
			d = shfit_floor_x05_int64(vec_tmp_buf[i], shift);
			*(dst + i) = luna_saturate_q63_to_q31(d);
		}
	}
	return 0;
}
#endif

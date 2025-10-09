
#ifndef __COMMON_H__
#define __COMMON_H__

#include "luna_sim/luna_math_types.h"

#define LUNA_INT8_MAX (signed char)0x7f
#define LUNA_INT8_MIN (signed char)0x80
#define LUNA_INT16_MAX (signed short)0x7fff
#define LUNA_INT16_MIN (signed short)0x8000
#define LUNA_INT32_MAX (signed int)0x7fffffff
#define LUNA_INT32_MIN (signed int)0x80000000

#define LUNA_DATA_INLEGAL 0xffff

const static q63_t max_num = 0x7fffffffffffffff;
const static q63_t min_num = 0x8000000000000000;

uint16_t luna_convert_float_to_u16(float x);
q7_t luna_convert_float_to_q7(float x, int32_t Q);
q15_t luna_convert_float_to_q15(float x, int32_t Q);
q31_t luna_convert_float_to_q31(float x, int32_t Q);
q63_t luna_convert_float_to_q63(float x, int32_t Q);

uint16_t luna_convert_double_to_u16(double x);
q7_t  luna_convert_double_to_q7(double x);
q15_t luna_convert_double_to_q15(double x);
q31_t luna_convert_double_to_q31(double x);
q63_t luna_convert_double_to_q63(double x);
q31_t luna_convert_double_to_q31_Q(double x, int32_t Q);

void luna_convert_float_to_q7_vector(float *in, q7_t *out, int32_t size, int32_t Q);
void luna_convert_float_to_q15_vector(float *in, q15_t *out, int32_t size, int32_t Q);
void luna_convert_float_to_q31_vector(float *in, q31_t *out, int32_t size, int32_t Q);
void luna_convert_q7_to_float_vector(q7_t *in, float *out, int32_t size);
void luna_convert_q15_to_float_vector(q15_t *in, float *out, int32_t size);
void luna_convert_q31_to_float_vector(q31_t *in, float *out, int32_t size);

int8_t luna_saturation_int32_to_int8(int32_t in);
int8_t luna_saturation_int64_to_int8(int64_t in);
int16_t luna_saturation_int32_to_int16(int32_t in);
int16_t luna_saturation_int64_to_int16(int64_t in);
int32_t luna_saturation_int64_to_int32(int64_t in);
q7_t luna_saturate_q15_to_q7(q15_t src);
q7_t luna_saturate_q31_to_q7(q31_t src);
q7_t luna_saturate_q63_to_q7(q63_t src);
q15_t luna_saturate_q31_to_q15(q31_t src);
q15_t luna_saturate_q63_to_q15(q63_t src);
q31_t luna_saturate_q63_to_q31(q63_t src);

int32_t luna_q63_add_new(q63_t* sum, q63_t b);
int32_t luna_q63_add_new_v2(q63_t* sum, q63_t b);
void luna_add_72bit_new(int64_t* sum1, int64_t* sum2);
q63_t luna_q63_shift(q63_t *sum, uint32_t shift);
q63_t luna_q63_shift_v2(q63_t *sum, uint32_t shift);

int64_t shfit_floor_x05_int64(int64_t x, int32_t shift);
int32_t shfit_floor_x05_int32(int32_t x, int32_t shift);

int32_t src_q3_to_int8(const int8_t* src_in, int8_t* src_out, uint32_t length);

#endif /* __COMMON_H__ */


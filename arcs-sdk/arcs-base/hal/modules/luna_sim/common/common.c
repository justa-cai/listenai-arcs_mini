#include <stdio.h>
#include <stdint.h>
#include "common.h"

void luna_convert_float_to_q7_vector(float *in, q7_t *out, int32_t size, int32_t Q)
{
	int32_t i;
	for (i = 0; i < size; i++)
	{
		out[i] = luna_convert_float_to_q7(in[i], Q);
	}
}

void luna_convert_float_to_q15_vector(float *in, q15_t *out, int32_t size, int32_t Q)
{
	int32_t i;
	for (i = 0; i < size; i++)
	{
		out[i] = luna_convert_float_to_q15(in[i], Q);
	}
}

void luna_convert_float_to_q31_vector(float *in, q31_t *out, int32_t size, int32_t Q)
{
	int32_t i;
	for (i = 0; i < size; i++)
	{
		out[i] = luna_convert_float_to_q31(in[i], Q);
	}
}

void luna_convert_q7_to_float_vector(q7_t *in, float *out, int32_t size)
{
	int32_t i;
	for (i = 0; i < size; i++)
	{
		out[i] = (float)(in[i]);
	}
}

void luna_convert_q15_to_float_vector(q15_t *in, float *out, int32_t size)
{
	int32_t i;
	for (i = 0; i < size; i++)
	{
		out[i] = (float)(in[i]);
	}
}

void luna_convert_q31_to_float_vector(q31_t *in, float *out, int32_t size)
{
	int32_t i;
	for (i = 0; i < size; i++)
	{
		out[i] = (float)(in[i]);
	}
}

// floating-point to Q(destination type)
int8_t luna_saturation_int32_to_int8(int32_t in)
{
	return (int8_t)((in > INT8_MAX)? (INT8_MAX) : ((in < INT8_MIN)? INT8_MIN : in));
}
int8_t luna_saturation_int64_to_int8(int64_t in)
{
	return (int8_t)((in > INT8_MAX)? (INT8_MAX) : ((in < INT8_MIN)? INT8_MIN : in));
}

int16_t luna_saturation_int32_to_int16(int32_t in)
{
	return (int16_t)((in > INT16_MAX)? (INT16_MAX) : ((in < INT16_MIN)? INT16_MIN : in));
}
int16_t luna_saturation_int64_to_int16(int64_t in)
{
	return (int16_t)((in > INT16_MAX)? (INT16_MAX) : ((in < INT16_MIN)? INT16_MIN : in));
}

int32_t luna_saturation_int64_to_int32(int64_t in)
{
	return (int32_t)((in > INT32_MAX) ? (INT32_MAX) : ((in < INT32_MIN) ? INT32_MIN : in));
}

//#include "basic_define.h"

#define SATURATE_TO_Q7_COMMON   \
q7_t ret;                         \
if (src > LUNA_INT8_MAX)             \
	ret = LUNA_INT8_MAX;            \
else if (src < LUNA_INT8_MIN)        \
	ret = LUNA_INT8_MIN;             \
else                           \
	ret = (q7_t)src;             \
                                \
return ret;

#define SATURATE_TO_Q15_COMMON   \
q15_t ret;                        \
if (src > LUNA_INT16_MAX)            \
	ret = LUNA_INT16_MAX;           \
else if (src < LUNA_INT16_MIN)      \
	ret = LUNA_INT16_MIN;           \
else                           \
	ret = (q15_t)src;            \
                               \
return ret;


q7_t luna_saturate_q15_to_q7(q15_t src)
{
	SATURATE_TO_Q7_COMMON
}

q7_t luna_saturate_q31_to_q7(q31_t src)
{
	SATURATE_TO_Q7_COMMON
}

q7_t luna_saturate_q63_to_q7(q63_t src)
{
	q7_t ret;
	q63_t int8_max = 0x7f;
	q63_t in8_min = 0xffffffffffffff80;
	if (src > int8_max)
		ret = int8_max;
	else if (src < in8_min)
		ret = in8_min;
	else
		ret = (q7_t)src;

	return ret;
}

q15_t luna_saturate_q31_to_q15(q31_t src)
{
	SATURATE_TO_Q15_COMMON
}


q15_t luna_saturate_q63_to_q15(q63_t src)
{
	q15_t ret;
	q63_t int16_max = 0x7fff;
	q63_t int16_min = 0xffffffffffff8000;
	if (src > int16_max)
	{
		ret = LUNA_INT16_MAX;
	}
	else if (src < int16_min)
	{
		ret = LUNA_INT16_MIN;
	}
	else
	{
		ret = (q15_t)src;
	}
	return ret;
}


q31_t luna_saturate_q63_to_q31(q63_t src)
{
	q31_t ret;
	q63_t int32_max = 0x7fffffff;
	q63_t int32_min = 0xffffffff80000000;
	if (src > int32_max)
	{
		ret = LUNA_INT32_MAX;
	}
	else if (src < int32_min)
	{
		ret = LUNA_INT32_MIN;
	}
	else
	{
		ret = (q31_t)src;
	}
	return ret;
}


uint16_t luna_convert_float_to_u16(float x)
{
	q31_t q31;
	uint16_t u16;
	x += (x < 0.0f ? -0.5f : 0.5f);
	q31 = (q31_t)x;
	if (q31 > 0xffff)
	{
		u16 = 0xffff;
	}
	else if (q31 < 0)
	{
		u16 = 0;
	}
	else
	{
		u16 = (uint16_t)q31;
	}

	return u16;
}


// floating-point to Q(destination type)
q7_t luna_convert_float_to_q7(float x, int32_t Q)
{
	q31_t q31;
	q7_t q7;
	x *= (1 << Q);
	x += (x < 0.0f ? -0.5f : 0.5f);
	q31 = (q31_t)x;
	if (q31 > INT8_MAX)
	{
		q7 = INT8_MAX;
	}
	else if (q31 < INT8_MIN)
	{
		q7 = INT8_MIN;
	}
	else
	{
		q7 = (q7_t)q31;
	}

	return q7;
}

q15_t luna_convert_float_to_q15(float x, int32_t Q)
{
	q31_t q31;
	q15_t q15;
	x *= (1 << Q);
	x += (x < 0.0f ? -0.5f : 0.5f);
	q31 = (q31_t)x;
	if (q31 > INT16_MAX)
	{
		q15 = INT16_MAX;
	}
	else if (q31 < INT16_MIN)
	{
		q15 = INT16_MIN;
	}
	else
	{
		q15 = (q15_t)q31;
	}

	return q15;
}

q31_t luna_convert_float_to_q31(float x, int32_t Q)
{
	int64_t q63;
	q31_t q31;
	q63_t int32_max = 0x7fffffff;
	q63_t int32_min = 0xffffffff80000000;
	x *= (1 << Q);
	x += (x < 0.0f ? -0.5f : 0.5f);
	q63 = (int64_t)x;

	if (q63 > int32_max)
	{
		q31 = int32_max;
	}
	else if (q63 < int32_min)
	{
		q31 = int32_min;
	}
	else
	{
		q31 = (q31_t)q63;
	}

	return q31;
}

q63_t luna_convert_float_to_q63(float x, int32_t Q)
{
	int64_t q63;
	//x *= (1 << Q);
	//x += (x < 0.0f ? -0.5f : 0.5f);
	q63 = (int64_t)x;

	return q63;
}

q7_t luna_convert_double_to_q7(double x)
{
	int64_t q63;
	q7_t q7;
	q63_t int8_max = 0x7f;
	q63_t int8_min = 0xffffffffffffff80;

	q63 = (int64_t)x;

	if (q63 > int8_max)
	{
		q7 = int8_max;
	}
	else if (q63 < int8_min)
	{
		q7 = int8_min;
	}
	else
	{
		q7 = (q7_t)q63;
	}

	return q7;
}


q15_t luna_convert_double_to_q15(double x)
{
	int64_t q63;
	q15_t q15;
	q63_t int16_max = 0x7fff;
	q63_t int16_min = 0xffffffffffff8000;

	q63 = (int64_t)x;

	if (q63 > int16_max)
	{
		q15 = int16_max;
	}
	else if (q63 < int16_min)
	{
		q15 = int16_min;
	}
	else
	{
		q15 = (q15_t)q63;
	}

	return q15;
}

q31_t luna_convert_double_to_q31(double x)
{
	int64_t q63;
	q31_t q31;
	q63_t int32_max = 0x7fffffff;
	q63_t int32_min = 0xffffffff80000000;

	q63 = (int64_t)x;
	if (q63 > int32_max)
	{
		q31 = int32_max;
	}
	else if (q63 < int32_min)
	{
		q31 = int32_min;
	}
	else
	{
		q31 = (q31_t)q63;
	}

	return q31;
}

q31_t luna_convert_double_to_q31_Q(double x, int32_t Q)
{
	int64_t q63;
	q31_t q31;
	q63_t int32_max = 0x7fffffff;
	q63_t int32_min = 0xffffffff80000000;
	x *= (1 << Q);
	x += (x < 0.0f ? -0.5f : 0.5f);
	q63 = (int64_t)x;

	if (q63 > int32_max)
	{
		q31 = int32_max;
	}
	else if (q63 < int32_min)
	{
		q31 = int32_min;
	}
	else
	{
		q31 = (q31_t)q63;
	}

	return q31;
}

int32_t luna_q63_add_new(q63_t* sum, q63_t b)
{
	q7_t  carry =0 ;
	q63_t b_ext_u_low63 = 0,b_ext_s_hig64 = 0;
	q63_t long_add_low64 = 0;
	q63_t long_add_hig64 = 0;
			
	if (0 == b)
		return 0;
		
	if((b&0x8000000000000000)==0) {
		b_ext_s_hig64=0;
		b_ext_u_low63=b&0x7fffffffffffffff;
	}
	else {
		b_ext_s_hig64=0xffffffffffffffff;
		b_ext_u_low63=b&0x7fffffffffffffff;
	}
			
	long_add_low64 = b_ext_u_low63 + sum[0];
	long_add_hig64 = b_ext_s_hig64 + sum[1];
	carry = (long_add_low64>>63)&0x1;
	long_add_hig64 += carry;
	
	if((long_add_hig64>>8)<-1 ||  (long_add_hig64>>8)>0) {
		sum[0] = long_add_low64 & 0x7fffffffffffffff;
		sum[1] = long_add_hig64 ;
return LUNA_DATA_INLEGAL;
	}
	else {
		sum[0] = long_add_low64 & 0x7fffffffffffffff;
		sum[1] = long_add_hig64 ;
	}
	return 0;
}

int32_t luna_q63_add_new_v2(q63_t* sum, q63_t b)
{
	int64_t high = sum[1];
	uint64_t low = (uint64_t)sum[0];
	uint64_t value = (uint64_t)b;
	int64_t  b_ext_sign_high = 0;

	if((value & 0x8000000000000000) == 0)
	{
		b_ext_sign_high = 0;
	}
	else
	{
		b_ext_sign_high = 0xffffffffffffffff;
	}

	// Perform the addition on the low part
    uint64_t tempLow = low + value;

	uint64_t carry = (tempLow < low) ? 1 : 0;

    // Adjust the high part (only the low 8 bits of high are used) based on overflow/underflow
    int64_t signedHigh = (int64_t)(b_ext_sign_high + sum[1] + carry); // Use only the low 8 bits of high

    // Saturate the result if necessary
    if (signedHigh > 0x7F) {
        sum[1] = 0x7F;
        sum[0] = 0xFFFFFFFFFFFFFFFF;
    } else if (signedHigh < -0x80) {
        sum[1] = (uint64_t)-0x80;
        sum[0] = 0;
    } else {
        sum[1] = signedHigh;
        sum[0] = tempLow;
    }
	return 0;
}

void luna_add_72bit_new(int64_t* sum1, int64_t* sum2)
{
	uint64_t aLow = (uint64_t)sum1[0];
	uint64_t aHigh = (uint64_t)sum1[1];
	uint64_t bLow = (uint64_t)sum2[0];
	uint64_t bHigh = (uint64_t)sum2[1];

    uint64_t tempLow = aLow + bLow;
    
    uint64_t carry = (tempLow < aLow) ? 1 : 0;
    
    int64_t tempHigh = aHigh + bHigh + carry;
    
    uint64_t saturationMaskHigh = 0xFFFFFFFFFFFFFFFF;
    tempHigh &= saturationMaskHigh;

	// Saturate the result if necessary
    if (tempHigh > 0x7F) {
        sum1[1] = 0x7F;
        sum1[0] = 0xFFFFFFFFFFFFFFFF;
    } else if (tempHigh < -0x80) {
        sum1[1] = (uint64_t)-0x80;
        sum1[0] = 0;
    } else
	{
        sum1[1] = tempHigh;
        sum1[0] = tempLow;
    }
}

q63_t luna_q63_shift(q63_t *sum, uint32_t shift)
{
	q63_t q63_out = 0;
	q7_t sat_flg = 0;
	q7_t shift_trunc = 0;
	q7_t trunc_flg = 0;

	if (shift&(1<<6)) {
		trunc_flg = 1;
		shift = shift&0x3f;
	}
	if(shift==0 || trunc_flg){
		shift_trunc=0;
	}
	else {
		shift_trunc=(sum[0]>>(shift-1))&0x1;
	}
	
	sat_flg = ((sum[1]>>shift)==0) | ((sum[1]>>shift)==-1);
	
	if(sat_flg!=1){
		if(sum[1]<0)
			q63_out = 0x8000000000000000;
		else
			q63_out = 0x7fffffffffffffff;
	}
	else {
		q63_out = (sum[0] & 0x7fffffffffffffff) >> shift;
		q63_out = q63_out | ((sum[1] & (~(0xfffffffffffffffe<<shift)))<<(63-shift));
		if(q63_out!=0x7fffffffffffffff)
			q63_out+=shift_trunc;
	}
	return q63_out;
}

q63_t luna_q63_shift_v2(q63_t *sum, uint32_t shift)
{
	int64_t high = sum[1];
	uint64_t low = (uint64_t)sum[0];
	int64_t result = 0;
	int8_t shift_trunc = 0;
	int8_t trunc_flg = 0;

	if (shift&(1<<6)) {
		trunc_flg = 1;
		shift = shift&0x3f;
	}
    if (shift == 0 || trunc_flg)
	{
		shift_trunc = 0;
	}
	else
	{
		shift_trunc = (low >> (shift - 1)) & 0x1;
	}

	uint64_t highToLow = (0 == shift) ? 0 : ((uint64_t)high << (64 - shift)); 
    uint64_t shiftedLow = ((uint64_t)low >> shift);
    int64_t shiftedHigh = high >> shift;

	result = (int64_t)shiftedLow;
	if (shiftedHigh < -1)
	{
        result = 0x8000000000000000;
    }
	else if (shiftedHigh > 0)
	{
		result = 0x7fffffffffffffff;
	}
	else if (shiftedHigh == -1)
	{
		if (result >= 0 && (int64_t)highToLow >= 0)
		{
			result = 0x8000000000000000;
		}
		else
		{
			result = ((int64_t)shiftedLow | highToLow) + shift_trunc;
		}
	}
	else
	{
        if (result < 0 || (int64_t)highToLow < 0)
		{
            result = 0x7fffffffffffffff;
        }
		else
		{
			result = ((int64_t)shiftedLow | highToLow);
			if (0x7fffffffffffffff != result)
			{
				result += shift_trunc;
			}
		}
    }

	return result;
}

/**
 * -0.5-->0(floor(x+0.5))
 **/
int64_t shfit_floor_x05_int64(int64_t x, int32_t shift)
{
	int64_t val = x;
	int8_t trunc_flg = 0;

	if (shift&(1<<6)) {
		trunc_flg = 1;
		shift = shift&0x3f;
	}

	if (shift >= 64) {
		if (trunc_flg) {
			return (val>=0)?0:-1;
		} else {
			return 0;
		}
	}
	if (shift > 0) {
		if (trunc_flg) {
			val = val >> (shift);
		} else {
			val = val >> (shift - 1);
			val = (val & 0x1) + (val >> 1);
		}
	}

	return val;
}


/**
 * -0.5-->0(floor(x+0.5))
 **/
int32_t shfit_floor_x05_int32(int32_t x, int32_t shift)
{
	int32_t val = x;
	int8_t trunc_flg = 0;

	if (shift&(1UL<<6)) {
		trunc_flg = 1;
		shift = shift&0x3f;
	}
	if (shift >= 32) {
		if (trunc_flg) {
			return (val>=0)?0:-1;
		} else {
			return 0;
		}
	}
	if (shift > 0) {
		if (trunc_flg) {
			val = val >> (shift);
		} else {
			val = val >> (shift - 1);
			val = (val & 0x1) + (val >> 1);
		}
	}

	return val;
}

/**
 * src_q3_to_int8
 * Convert 4 bits of weight data to 8 bits
 **/
int32_t src_q3_to_int8(const int8_t* src_in, int8_t* src_out, uint32_t length)
{
	if (NULL == src_in || NULL == src_out)
	{
		return -1;
	}

	int32_t i;
	int32_t data_4bit_len = (length + 1) / 2;

	for (i = 0; i < data_4bit_len; i++)
	{
		if ((src_in[i] & 0x0F) >= 0x08)//negative value
		{
			src_out[2 * i] = src_in[i] | 0xF0;
		}
		else
		{
			src_out[2 * i] = src_in[i] & 0x0F;
		}

		if ((2 * i + 1) < length)
		{
			if (((src_in[i] >> 4) & 0x0F) >= 0x08)
			{
				src_out[2 * i + 1] = (src_in[i] >> 4) | 0xF0;
			}
			else
			{
				src_out[2 * i + 1] = (src_in[i] >> 4) & 0x0F;
			}
		}

	}

	return 0;
}

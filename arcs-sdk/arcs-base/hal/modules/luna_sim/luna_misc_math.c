/*
 * luna_misc_math.c
 *
 *  Created on: Sep 10, 2017
 *      Author: dwwang
 */
 
#include "luna_sim/luna.h"
#include "luna_sim/luna_misc_math.h"
#include "luna_sim/common/common.h"
#include "luna_sim/common/check.h"


static int32_t shift_pure(int64_t v, int32_t s)
{
    if (s >= 63 || s <= -63) {
		return 0;
	}
	if (s > 0) {
		v = v << s;
	} else {
        v = v >> (-s);
    }
    return v;
}

static int32_t shift_rasym(int64_t v,int32_t s)
{
    if (s >= 63 || s <= -63) {
		return 0;
	}
    if (s >= 0) {
		v = v << s;
	} else {
        s = (-s);
        v = v >> (s - 1);
		v = (v & 0x1) + (v >> 1);
    }
    return v;
}

static int32_t shift_rasyms(int64_t v, int32_t s)
{
	if (s >= 64 || s <= -64) {
		return 0;
	}
    if (s >= 0) {
		v = v << s;
	} else {
        s = (-s);
        v = v >> (s - 1);
		v = (v & 0x1) + (v >> 1);
    }
	if (v > (int32_t)0x7fffffff) {
		v = (int32_t)0x7fffffff;
	} else if (v < (int32_t)0x80000000) {
		v = (int32_t)0x80000000;
	}
	return v;
}


int32_t LUNA_API_SIM(luna_memcpy_i8o8)(int8_t* dst, int8_t* src, uint32_t size)
{
	LUNA_CHECK_PARAM_VEC_1SRC1DST((char*)src, (char*)dst, size, size, 0);

	int i;
	int8_t *p_src = (int8_t *)src;
	int8_t *p_dst = (int8_t *)dst;
	for (i = 0; i < size; i++)
	{
		p_dst[i] = p_src[i];
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_psrammemcpy_i8o8)(int8_t* dst, int8_t* src, uint32_t size)
{
	LUNA_CHECK_PARAM_VEC_1SRC1DST((char*)src, (char*)dst, size, size, 0);

	int i;
	int8_t *p_src = (int8_t *)src;
	int8_t *p_dst = (int8_t *)dst;
	for (i = 0; i < size; i++)
	{
		p_dst[i] = p_src[i];
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_memset_i8o8)(int8_t *dst, int8_t value, uint32_t size)
{
	LUNA_CHECK_PARAM_VEC_1SRC1DST((char*)dst, (char*)dst, size, size, 0);

	int i;
	int8_t *p_dst = (int8_t *)dst;
	for (i = 0; i < size; i++)
	{
		p_dst[i] = value;
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_memset_i16o16)(int16_t *dst, int16_t value, uint32_t size)
{
	LUNA_CHECK_PARAM_VEC_1SRC1DST((char*)dst, (char*)dst, size, size, 0);
	int i;
	int16_t *p_dst = (int16_t *)dst;
	for (i = 0; i < size; i++)
	{
		p_dst[i] = value;
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_memset_i32o32)(int32_t *dst, int32_t value, uint32_t size)
{
	LUNA_CHECK_PARAM_VEC_1SRC1DST((char*)dst, (char*)dst, size, size, 0);

	int i;
	int32_t *p_dst = (int32_t *)dst;
	for (i = 0; i < size; i++)
	{
		p_dst[i] = value;
	}

	return 0;
}


int32_t LUNA_API_SIM(luna_relu_i8o8)(const int8_t *src, int8_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_1SRC1DST(src, dst, size, size, shift);

	{
		int32_t a;
		uint32_t i;
		int8_t *p_src = (int8_t *)src;
		int8_t *p_dst = (int8_t *)dst;
		for (i=0; i<size; i++) {
			a = (int32_t)*(p_src + i);
			a = shfit_floor_x05_int32(a, shift);
			if (a >= 0) {
				*(p_dst + i) = luna_saturate_q31_to_q7(a);
			}
			else {
				*(p_dst + i) = 0;
			}
		}
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_relu_i8o32)(const int8_t *src, int32_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_1SRC1DST(src, dst, size, size, shift);
	{
		int32_t a;
		uint32_t i;
		int8_t *p_src = (int8_t *)src;
		int32_t *p_dst = (int32_t *)dst;
		for (i=0; i<size; i++) {
			a = (int32_t)*(p_src + i);
			a = shfit_floor_x05_int32(a, shift);
			if (a >= 0) {
				*(p_dst + i) = a;
			}
			else {
				*(p_dst + i) = 0;
			}
		}
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_relu_i32o8)(const int32_t *src, int8_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_1SRC1DST(src, dst, size, size, shift);
	{
		int32_t a;
		uint32_t i;
		int32_t *p_src = (int32_t *)src;
		int8_t *p_dst = (int8_t *)dst;
		for (i=0; i<size; i++) {
			a = *(p_src + i);
			a = shfit_floor_x05_int32(a, shift);
			if (a >= 0) {
				*(p_dst + i) = luna_saturate_q31_to_q7(a);
			}
			else {
				*(p_dst + i) = 0;
			}
		}
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_relu_i32o32)(const int32_t *src, int32_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_1SRC1DST(src, dst, size, size, shift);
	{
		int32_t a;
		uint32_t i;
		int32_t *p_src = (int32_t *)src;
		int32_t *p_dst = (int32_t *)dst;
		for (i=0; i<size; i++) {
			a = *(p_src + i);
			a = shfit_floor_x05_int32(a, shift);
			if (a >= 0) {
				*(p_dst + i) = a;
			}
			else {
				*(p_dst + i) = 0;
			}
		}
	}

	return 0;
}


int32_t LUNA_API_SIM(luna_prelu_i8o8)(const int8_t *src, uint32_t slope, int8_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_1SRC1DST(src, dst, size, size, slope + shift);
	{
		int32_t a;
		uint32_t i;
		int8_t *p_src = (int8_t *)src;
		int8_t *p_dst = (int8_t *)dst;
		for (i = 0; i < size; i++) {
			a = (int32_t)*(p_src + i);
			if (a >= 0) {
				a = shfit_floor_x05_int32(a, 0+shift);
				*(p_dst + i) = luna_saturate_q31_to_q7(a);
			}
			else {
				a = shfit_floor_x05_int32(a, slope+shift);
				*(p_dst + i) = luna_saturate_q31_to_q7(a);
			}
		}
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_prelu_i8o32)(const int8_t *src, uint32_t slope, int32_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_1SRC1DST(src, dst, size, size, slope + shift);
	{
		int32_t a;
		uint32_t i;
		int8_t *p_src = (int8_t *)src;
		int32_t *p_dst = (int32_t *)dst;
		for (i = 0; i < size; i++) {
			a = (int32_t)*(p_src + i);
			if (a >= 0) {
				a = shfit_floor_x05_int32(a, 0+shift);
				*(p_dst + i) = a;
			}
			else {
				a = shfit_floor_x05_int32(a, slope+shift);
				*(p_dst + i) = a;
			}
		}
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_prelu_i32o8)(const int32_t *src, uint32_t slope, int8_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_1SRC1DST(src, dst, size, size, slope + shift);
	{
		int32_t a;
		uint32_t i;
		int32_t *p_src = (int32_t *)src;
		int8_t *p_dst = (int8_t *)dst;
		for (i = 0; i < size; i++) {
			a = *(p_src + i);
			if (a >= 0) {
				a = shfit_floor_x05_int32(a, 0+shift);
				*(p_dst + i) = luna_saturate_q31_to_q7(a);
			}
			else {
				a = shfit_floor_x05_int32(a, slope+shift);
				*(p_dst + i) = luna_saturate_q31_to_q7(a);
			}
		}
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_prelu_i32o32)(const int32_t *src, uint32_t slope, int32_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_1SRC1DST(src, dst, size, size, slope + shift);
	{
		int32_t a;
		uint32_t i;
		int32_t *p_src = (int32_t *)src;
		int32_t *p_dst = (int32_t *)dst;
		for (i = 0; i < size; i++) {
			a = *(p_src + i);
			if (a >= 0) {
				a = shfit_floor_x05_int32(a, 0+shift);
				*(p_dst + i) = a;
			}
			else {
				a = shfit_floor_x05_int32(a, slope+shift);
				*(p_dst + i) = a;
			}
		}
	} 

	return 0;
}

int32_t LUNA_API_SIM(luna_relux_i8o8)(const int8_t *src, int8_t x, int8_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_1SRC1DST(src, dst, size, size, shift);

	{
		int32_t a;
		uint32_t i;
		int8_t *p_src = (int8_t *)src;
		int8_t *p_dst = (int8_t *)dst;
		for (i=0; i<size; i++) {
			a = (int32_t)*(p_src + i);
			a = shfit_floor_x05_int32(a, shift);
			if (a >= 0) {
				a = luna_saturate_q31_to_q7(a);
				if (a > x){
					a = x;
				}
				*(p_dst + i) = a;
			}else {
				*(p_dst + i) = 0;
			}			
		}
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_relux_i32o8)(const int32_t *src, const int8_t x, int8_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_CHECK_PARAM_VEC_1SRC1DST(src, dst, size, size, shift);

	{
		int32_t a;
		uint32_t i;
		int32_t *p_src = (int32_t *)src;
		int8_t *p_dst = (int8_t *)dst;
		for (i=0; i<size; i++) {
			a = (int32_t)*(p_src + i);
			a = shfit_floor_x05_int32(a, shift);
			if (a >= 0) {
				a = luna_saturate_q31_to_q7(a);
				if (a > x){
					a = x;
				}
				*(p_dst + i) = a;
			}else {
				*(p_dst + i) = 0;
			}			
		}
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_sigmoid_i32o32)(const int32_t *src, int32_t *dst, uint32_t size)
{
	LUNA_CHECK_PARAM_VEC_1SRC1DST(src, dst, size, size, 0);

	static const uint32_t bands[] = {0, 63656107, 111395083, 153816608, 194953701, 236865036, 281253298, 329433241, 384154475, 447714743, 515399149, 589016819, 679940425, 759830874, 862986200, 965402453, 2147483648};
	static const uint32_t slopes[] = {529475578, 482862538, 424212188, 361565531, 298613704, 238039992, 181912633, 132002182, 89144402, 56020914, 34019888, 18928477, 9459126, 5291645, 2204341, 177654};
	static const uint32_t bias0s[] = {1073741824, 1095849221, 1144526551, 1216321063, 1307759740, 1414659140, 1532274042, 1654777693, 1777444109, 1887935281, 1972419725, 2038648643, 2086619912, 2110212774, 2130063364, 2144640936};
	static const uint32_t bias1s[] = {1073741824, 1051634427, 1002957097, 931162585, 839723908, 732824508, 615209606, 492705955, 370039539, 259548367, 175063923, 108835005, 60863736, 37270874, 17420284, 2842712};

	{
		uint32_t i, j;
		uint32_t sign;
		int64_t absx, slope, bias;
		uint32_t shift = 0;
		int64_t x;

		for (i = 0; i < size; i++) {
			x = ((int32_t*)src)[i];
			if (x < 0) {
				sign = 1;
				absx = -x;
			} else {
				sign = 0;
				absx = x;
			}

			for (j = 1; j < 17; ++j) {
				if (absx <= bands[j]) {
					slope = slopes[j - 1];
					if (1 == sign){
						bias = bias1s[j - 1];
					} else {
						bias = bias0s[j - 1];
					}
					break;
				} else {
					slope = 0;
					bias = 0;
				}
			}

			x = ((slope*x)>>27) + bias;
			x = shfit_floor_x05_int64(x, shift);
			((int32_t*)dst)[i] = luna_saturate_q63_to_q31(x);
		}
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_tanh_i32o32)(const int32_t *src, int32_t *dst, uint32_t size)
{
	LUNA_CHECK_PARAM_VEC_1SRC1DST(src, dst, size, size, 0);

	static const uint32_t bands[] = {0, 33584191, 58182438, 80361293, 102120620, 124483371, 148392654, 174972063, 206183541, 244722657, 281591256, 312045360, 358241226, 403095772, 471425623, 530372454, 2147483648};
	static const uint32_t slopes[] = {2114623134, 1910645334, 1662969581, 1396521636, 1131979061, 881027392, 652221876, 451080022, 281798391, 159724746, 98692663, 60001039, 27632752, 13732196, 2832019, 172634};
	static const uint32_t biass[] = {0, 51039676, 158405367, 317937954, 519217264, 751968259, 1004938252, 1267155516, 1527203771, 1749783808, 1877830237, 1967785133, 2054179492, 2095926998, 2134212723, 2144721503};

	{
		uint32_t i, j;
		uint32_t sign;
		int64_t absx, slope, bias;
		uint32_t shift = 0;
		int64_t x;

		for (i = 0; i < size; i++) {
			x = ((int32_t*)src)[i];
			if (x < 0) {
				sign = 1;
				if (-(1ULL<<31) == x) {
					absx = 1ULL<<31;
				} else {
					absx = -x;
				}
			} else {
				sign = 0;
				absx = x;
			}

			for (j = 1; j < 17; ++j) {
				if (absx <= bands[j]) {
					slope = slopes[j - 1];
					bias = biass[j - 1];
					break;
				} else {
					slope = 0;
					bias = 0;
				}
			}

			if (1==sign) {
				x = ((-slope*absx)>>27) - bias;
			} else {
				x = ((slope*absx)>>27) + bias;
			}
			
			x = shfit_floor_x05_int64(x, shift);
			((int32_t*)dst)[i] = luna_saturate_q63_to_q31(x);
		}
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_sigmoid_i32o8)(const int32_t *src, int8_t *dst, uint32_t size)
{
	LUNA_CHECK_PARAM_VEC_1SRC1DST(src, dst, size, size, 0);

	static const uint32_t bands[] = {0, 63656107, 111395083, 153816608, 194953701, 236865036, 281253298, 329433241, 384154475, 447714743, 515399149, 589016819, 679940425, 759830874, 862986200, 965402453, 2147483648};
	static const uint32_t slopes[] = {529475578, 482862538, 424212188, 361565531, 298613704, 238039992, 181912633, 132002182, 89144402, 56020914, 34019888, 18928477, 9459126, 5291645, 2204341, 177654};
	static const uint32_t bias0s[] = {1073741824, 1095849221, 1144526551, 1216321063, 1307759740, 1414659140, 1532274042, 1654777693, 1777444109, 1887935281, 1972419725, 2038648643, 2086619912, 2110212774, 2130063364, 2144640936};
	static const uint32_t bias1s[] = {1073741824, 1051634427, 1002957097, 931162585, 839723908, 732824508, 615209606, 492705955, 370039539, 259548367, 175063923, 108835005, 60863736, 37270874, 17420284, 2842712};

	{
		uint32_t i, j;
		uint32_t sign;
		int64_t absx, slope, bias;
		uint32_t shift = 0;
		int64_t x;

		for (i = 0; i < size; i++) {
			x = ((int32_t*)src)[i];
			if (x < 0) {
				sign = 1;
				absx = -x;
			} else {
				sign = 0;
				absx = x;
			}

			for (j = 1; j < 17; ++j) {
				if (absx <= bands[j]) {
					slope = slopes[j - 1];
					if (1 == sign){
						bias = bias1s[j - 1];
					} else {
						bias = bias0s[j - 1];
					}
					break;
				} else {
					slope = 0;
					bias = 0;
				}
			}

			x = ((slope*x)>>27) + bias;
			x = shfit_floor_x05_int64(x, shift+24);
			((int8_t*)dst)[i] = luna_saturate_q63_to_q7(x);
		}
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_tanh_i32o8)(const int32_t *src, int8_t *dst, uint32_t size)
{
	LUNA_CHECK_PARAM_VEC_1SRC1DST(src, dst, size, size, 0);

	static const uint32_t bands[] = {0, 33584191, 58182438, 80361293, 102120620, 124483371, 148392654, 174972063, 206183541, 244722657, 281591256, 312045360, 358241226, 403095772, 471425623, 530372454, 2147483648};
	static const uint32_t slopes[] = {2114623134, 1910645334, 1662969581, 1396521636, 1131979061, 881027392, 652221876, 451080022, 281798391, 159724746, 98692663, 60001039, 27632752, 13732196, 2832019, 172634};
	static const uint32_t biass[] = {0, 51039676, 158405367, 317937954, 519217264, 751968259, 1004938252, 1267155516, 1527203771, 1749783808, 1877830237, 1967785133, 2054179492, 2095926998, 2134212723, 2144721503};

	{
		uint32_t i, j;
		uint32_t sign;
		int64_t absx, slope, bias;
		uint32_t shift = 0;
		int64_t x;

		for (i = 0; i < size; i++) {
			x = ((int32_t*)src)[i];
			if (x < 0) {
				sign = 1;
				if (-(1ULL<<31) == x) {
					absx = 1ULL<<31;
				} else {
					absx = -x;
				}
			} else {
				sign = 0;
				absx = x;
			}

			for (j = 1; j < 17; ++j) {
				if (absx <= bands[j]) {
					slope = slopes[j - 1];
					bias = biass[j - 1];
					break;
				} else {
					slope = 0;
					bias = 0;
				}
			}

			if (1==sign) {
				x = ((-slope*absx)>>27) - bias;
			} else {
				x = ((slope*absx)>>27) + bias;
			}
			
			x = shfit_floor_x05_int64(x, shift+24);
			((int8_t*)dst)[i] = luna_saturate_q63_to_q7(x);
		}
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_exp_i32o32)(const int32_t *src, int32_t *dst, uint32_t size)
{
	int32_t X = 0;
	int32_t Y = 0;
	int32_t E = 0;
	int32_t E_SUM = 0;
	const static int32_t p23[5] = { 57364 ,446161 ,2008107 , 5813551, 8388575 };

	for (int i = 0; i < size; i++)
	{
        X = ((int32_t*)src)[i];
		X = shift_rasyms((int64_t)X * (int64_t)774541002,-31);

		E = X >> 23;
		E = E + 1;

		X = X & 0x7fffff;
		X = X - 0x800000;

		Y = p23[0];

		for (int j = 1; j < 5; j++)
		{
			Y = shift_rasym((int64_t)Y * (int64_t)X,-23) + p23[j];
		}
		((int32_t*)dst)[i] = shift_pure(Y, E);
	}

  return 0;
}


int32_t LUNA_API_SIM(luna_softmax_i32o32)(const int32_t *src, int32_t *dst, uint32_t size)
{
	int32_t X = 0;
	int32_t Y = 0;
	int32_t E = 0;
	int32_t E_MAX = 0x80000000;
	int64_t E_SUM = 0;
	uint32_t A = 0x800000, B = 0, C = 0;
	const static int32_t p23[5] = { 57364 ,446161 ,2008107 , 5813551, 8388575 };

	for (int i = 0; i < size; i++){
		if (src[i] > E_MAX) {
			E_MAX = src[i];
		}
	}
	for (int i = 0; i < size; i++) {
		dst[i] = luna_saturate_q63_to_q31((int64_t)(src[i]) - (int64_t)(E_MAX));
	}
	
	for (int i = 0; i < size; i++){
        X = ((int32_t*)dst)[i];
		X = shift_rasyms((int64_t)X * (int64_t)774541002,-31);

		E = X >> 23;
		E = E + 1;

		X = X & 0x7fffff;
		X = X - 0x800000;

		Y = p23[0];

		for (int j = 1; j < 5; j++)
		{
			Y = shift_rasym((int64_t)Y * (int64_t)X,-23) + p23[j];
		}
		((int32_t*)dst)[i] = shift_pure(Y, E);
	}
	for (int i = 0; i < size; i++) {
		E_SUM += dst[i];
	}
	B = luna_saturate_q63_to_q31(E_SUM);
	for (int i = 1; i <= 30; i++) {
		if(A>=B) {
			C = C+1; 
			C = C*2;
			A = A-B;
			A = A*2;
		} else {
			C = C*2;
			A = A*2;
		}
	}
	for (int i = 0; i < size; i++) {
		dst[i] = shfit_floor_x05_int64((int64_t)(dst[i]) * (int64_t)(C), 38);
	}
  	return 0;
}

static int32_t ln_q15q15(uint32_t w32Param, int16_t cParamQ)
{
	static const uint16_t g_s16SimpleLnTable[512] ={
		0x0000,0x0080,0x0100,0x017f,0x01fe,0x027d,0x02fc,0x037a,0x03f8,0x0476,0x04f4,0x0571,0x05ee,0x066b,0x06e8,0x0764,
		0x07e1,0x085d,0x08d8,0x0954,0x09cf,0x0a4a,0x0ac5,0x0b40,0x0bba,0x0c34,0x0cae,0x0d28,0x0da1,0x0e1b,0x0e94,0x0f0d,
		0x0f85,0x0ffd,0x1076,0x10ee,0x1165,0x11dd,0x1254,0x12cb,0x1342,0x13b8,0x142f,0x14a5,0x151b,0x1591,0x1606,0x167c,
		0x16f1,0x1766,0x17da,0x184f,0x18c3,0x1937,0x19ab,0x1a1f,0x1a92,0x1b06,0x1b79,0x1bec,0x1c5e,0x1cd1,0x1d43,0x1db5,
		0x1e27,0x1e99,0x1f0a,0x1f7b,0x1fed,0x205d,0x20ce,0x213f,0x21af,0x221f,0x228f,0x22ff,0x236e,0x23de,0x244d,0x24bc,
		0x252b,0x2599,0x2608,0x2676,0x26e4,0x2752,0x27c0,0x282d,0x289a,0x2907,0x2974,0x29e1,0x2a4e,0x2aba,0x2b26,0x2b93,
		0x2bfe,0x2c6a,0x2cd6,0x2d41,0x2dac,0x2e17,0x2e82,0x2eed,0x2f57,0x2fc1,0x302c,0x3095,0x30ff,0x3169,0x31d2,0x323c,
		0x32a5,0x330e,0x3376,0x33df,0x3447,0x34b0,0x3518,0x3580,0x35e8,0x364f,0x36b7,0x371e,0x3785,0x37ec,0x3853,0x38b9,
		0x3920,0x3986,0x39ec,0x3a52,0x3ab8,0x3b1e,0x3b83,0x3be9,0x3c4e,0x3cb3,0x3d18,0x3d7d,0x3de1,0x3e46,0x3eaa,0x3f0e,
		0x3f72,0x3fd6,0x403a,0x409d,0x4101,0x4164,0x41c7,0x422a,0x428d,0x42ef,0x4352,0x43b4,0x4416,0x4478,0x44da,0x453c,
		0x459d,0x45ff,0x4660,0x46c1,0x4722,0x4783,0x47e4,0x4845,0x48a5,0x4905,0x4966,0x49c6,0x4a25,0x4a85,0x4ae5,0x4b44,
		0x4ba4,0x4c03,0x4c62,0x4cc1,0x4d1f,0x4d7e,0x4ddd,0x4e3b,0x4e99,0x4ef7,0x4f55,0x4fb3,0x5011,0x506e,0x50cc,0x5129,
		0x5186,0x51e3,0x5240,0x529d,0x52f9,0x5356,0x53b2,0x540f,0x546b,0x54c7,0x5523,0x557e,0x55da,0x5635,0x5691,0x56ec,
		0x5747,0x57a2,0x57fd,0x5857,0x58b2,0x590d,0x5967,0x59c1,0x5a1b,0x5a75,0x5acf,0x5b29,0x5b82,0x5bdc,0x5c35,0x5c8e,
		0x5ce7,0x5d40,0x5d99,0x5df2,0x5e4b,0x5ea3,0x5efb,0x5f54,0x5fac,0x6004,0x605c,0x60b4,0x610b,0x6163,0x61ba,0x6212,
		0x6269,0x62c0,0x6317,0x636e,0x63c4,0x641b,0x6472,0x64c8,0x651e,0x6574,0x65cb,0x6620,0x6676,0x66cc,0x6722,0x6777,
		0x67cd,0x6822,0x6877,0x68cc,0x6921,0x6976,0x69cb,0x6a1f,0x6a74,0x6ac8,0x6b1c,0x6b71,0x6bc5,0x6c19,0x6c6c,0x6cc0,
		0x6d14,0x6d67,0x6dbb,0x6e0e,0x6e61,0x6eb4,0x6f08,0x6f5a,0x6fad,0x7000,0x7052,0x70a5,0x70f7,0x714a,0x719c,0x71ee,
		0x7240,0x7292,0x72e4,0x7335,0x7387,0x73d8,0x742a,0x747b,0x74cc,0x751d,0x756e,0x75bf,0x7610,0x7660,0x76b1,0x7701,
		0x7752,0x77a2,0x77f2,0x7842,0x7892,0x78e2,0x7932,0x7981,0x79d1,0x7a21,0x7a70,0x7abf,0x7b0e,0x7b5e,0x7bad,0x7bfb,
		0x7c4a,0x7c99,0x7ce8,0x7d36,0x7d85,0x7dd3,0x7e21,0x7e6f,0x7ebd,0x7f0b,0x7f59,0x7fa7,0x7ff5,0x8042,0x8090,0x80dd,
		0x812b,0x8178,0x81c5,0x8212,0x825f,0x82ac,0x82f9,0x8345,0x8392,0x83de,0x842b,0x8477,0x84c3,0x8510,0x855c,0x85a8,
		0x85f4,0x863f,0x868b,0x86d7,0x8722,0x876e,0x87b9,0x8804,0x8850,0x889b,0x88e6,0x8931,0x897c,0x89c6,0x8a11,0x8a5c,
		0x8aa6,0x8af1,0x8b3b,0x8b85,0x8bcf,0x8c19,0x8c63,0x8cad,0x8cf7,0x8d41,0x8d8b,0x8dd4,0x8e1e,0x8e67,0x8eb1,0x8efa,
		0x8f43,0x8f8c,0x8fd5,0x901e,0x9067,0x90b0,0x90f8,0x9141,0x918a,0x91d2,0x921a,0x9263,0x92ab,0x92f3,0x933b,0x9383,
		0x93cb,0x9413,0x945b,0x94a2,0x94ea,0x9531,0x9579,0x95c0,0x9607,0x964f,0x9696,0x96dd,0x9724,0x976b,0x97b1,0x97f8,
		0x983f,0x9885,0x98cc,0x9912,0x9959,0x999f,0x99e5,0x9a2b,0x9a71,0x9ab7,0x9afd,0x9b43,0x9b89,0x9bce,0x9c14,0x9c5a,
		0x9c9f,0x9ce4,0x9d2a,0x9d6f,0x9db4,0x9df9,0x9e3e,0x9e83,0x9ec8,0x9f0d,0x9f52,0x9f96,0x9fdb,0xa01f,0xa064,0xa0a8,
		0xa0ec,0xa131,0xa175,0xa1b9,0xa1fd,0xa241,0xa285,0xa2c9,0xa30c,0xa350,0xa394,0xa3d7,0xa41b,0xa45e,0xa4a1,0xa4e5,
		0xa528,0xa56b,0xa5ae,0xa5f1,0xa634,0xa677,0xa6b9,0xa6fc,0xa73f,0xa781,0xa7c4,0xa806,0xa849,0xa88b,0xa8cd,0xa90f,
		0xa951,0xa993,0xa9d5,0xaa17,0xaa59,0xaa9b,0xaadd,0xab1e,0xab60,0xaba1,0xabe3,0xac24,0xac65,0xaca7,0xace8,0xad29,
		0xad6a,0xadab,0xadec,0xae2d,0xae6e,0xaeae,0xaeef,0xaf30,0xaf70,0xafb1,0xaff1,0xb031,0xb072,0xb0b2,0xb0f2,0xb132,
	};

	int32_t s32Result;
	int16_t s16Lable;
	int16_t s16Q = cParamQ;

	w32Param |= 1;
	if (!(w32Param & 0xFFFF0000)){
		w32Param <<= 16;
		s16Q += 16;
	}if (!(w32Param & 0xFF000000)){
		w32Param <<= 8;
		s16Q += 8;
	}if (!(w32Param & 0xF0000000)){
		w32Param <<= 4;
		s16Q += 4;
	}if (!(w32Param & 0xC0000000)){
		w32Param <<= 2;
		s16Q += 2;
	}if (!(w32Param & 0x80000000)){
		w32Param <<= 1;
		s16Q += 1;
	}
	w32Param = w32Param - 0x80000000L;
	s16Lable = (int16_t)(w32Param >> 22); /* 9bit */									  
	s32Result = ((int32_t)(g_s16SimpleLnTable[s16Lable]) + (int32_t)((31 - s16Q) * 0xB172L)) >> 1; /* LN2 Q16 = 0XB172L */

	return s32Result;
}


int32_t LUNA_API_SIM(luna_logsoftmax_i32o32)(const int32_t *src, int32_t *dst, uint32_t size)
{
	int32_t X = 0;
	int32_t Y = 0;
	int32_t E = 0;
	int32_t E_MAX = 0x80000000;
	int64_t E_SUM = 0;
	uint32_t A = 0x800000, B = 0, C = 0;
	int32_t LOG_SUM;
	const static int32_t p23[5] = { 57364 ,446161 ,2008107 , 5813551, 8388575 };

	for (int i = 0; i < size; i++){
		if (src[i] > E_MAX) {
			E_MAX = src[i];
		}
	}
	if (E_MAX == (int32_t)0x80000000) {
		E_MAX += 1;
	}
	for (int i = 0; i < size; i++) {
		dst[i] = luna_saturate_q63_to_q31((int64_t)(src[i]) - (int64_t)(E_MAX));
	}
	
	for (int i = 0; i < size; i++){
        X = ((int32_t*)dst)[i];
		X = shift_rasyms((int64_t)X * (int64_t)774541002,-31);

		E = X >> 23;
		E = E + 1;

		X = X & 0x7fffff;
		X = X - 0x800000;

		Y = p23[0];

		for (int j = 1; j < 5; j++)
		{
			Y = shift_rasym((int64_t)Y * (int64_t)X,-23) + p23[j];
		}
		((int32_t*)dst)[i] = shift_pure(Y, E);
	}
	for (int i = 0; i < size; i++) {
		E_SUM += dst[i];
	}
	E_SUM = shfit_floor_x05_int64(E_SUM, 8); 
	E_SUM = luna_saturate_q63_to_q31(E_SUM);
	LOG_SUM = ln_q15q15(E_SUM, 15);
	for (int i = 0; i < size; i++) {
		dst[i] = shfit_floor_x05_int64((int64_t)(src[i]) - (int64_t)(E_MAX), 10);
	}
	for (int i = 0; i < size; i++) {
		dst[i] = shfit_floor_x05_int64((int64_t)(dst[i]) - (int64_t)(LOG_SUM), 0);
	}
  	return 0;
}
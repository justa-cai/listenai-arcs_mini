/*
 * luna_complex_math.c
 *
 *  Created on: Sep 10, 2017
 *      Author: dwwang
 */

#include "luna/luna.h"
#include "luna/luna_complex_math.h"
#include "luna/cmd/luna_vector_cmd.h"
#include "luna_privates.h"

_FAST_DATA_ZI static LunaVectorParams_t  vector_params;

#define LUNA_COMPLEX_API_EXEC(src_addr1, src_addr2, dst_addr, points, q, m_dtype, luna_api)	\
			int ret = 0;	\
			vector_params.src1 = LUNA_SHARE_ADDR_OFFSET(src_addr1);	\
			vector_params.src2 = LUNA_SHARE_ADDR_OFFSET(src_addr2);	\
			vector_params.dst = LUNA_SHARE_ADDR_OFFSET(dst_addr);	\
			vector_params.size = points;	\
			vector_params.shift = q;	\
			vector_params.dtype = m_dtype; \
			ret = luna_execute_cmd(luna_api, (void *)&vector_params, sizeof(LunaVectorParams_t));	\
			return ret;


int32_t luna_clx_mul_i32i32o8(const int32_t *src1, const int32_t *src2, int8_t *dst, uint32_t size,uint32_t shift)
{
	LUNA_COMPLEX_API_EXEC(src1, src2, dst, size, shift, 0x1A, luna_api_vec_cplx_mul)
}

int32_t luna_clx_mul_i32i32o32(const int32_t *src1, const int32_t *src2, int32_t *dst, uint32_t size,uint32_t shift)
{
	LUNA_COMPLEX_API_EXEC(src1, src2, dst, size, shift, 0x3A, luna_api_vec_cplx_mul)
}

int32_t luna_clx_conj_mul_i32i32o32(const int32_t *src1, const int32_t *src2, int32_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_COMPLEX_API_EXEC(src1, src2, dst, size, shift, 0x3B, luna_api_vec_cplx_mul)
}

int32_t luna_clx_mul_real_i32i32o32(const int32_t *src1, const int32_t *src2, int32_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_COMPLEX_API_EXEC(src1, src2, dst, size, shift, 0x30, luna_api_vec_cplx_mul_real)
}

int32_t luna_clx_mul_out_real_i32i32o32(const int32_t *src1, const int32_t *src2, int32_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_COMPLEX_API_EXEC(src1, src2, dst, size, shift, 0x30, luna_api_vec_cplx_mul_ou_real)
}

int32_t luna_conjugate_i8o8(const int8_t *src, int8_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_COMPLEX_API_EXEC(src, NULL, dst, size, shift, 0x11, luna_api_vector_conj)
}

int32_t luna_conjugate_i32o32(const int32_t *src, int32_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_COMPLEX_API_EXEC(src, NULL, dst, size, shift, 0x33, luna_api_vector_conj)
}

int32_t luna_power_spectrum_i32o32(const int32_t *src, int32_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_COMPLEX_API_EXEC(src, NULL, dst, size, shift, 0x30, luna_api_vec_cplx_modulus)
}

int32_t luna_power_spectrum_i32o64(const int32_t *src, int64_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_COMPLEX_API_EXEC(src, NULL, dst, size, shift, 0x40, luna_api_vec_cplx_modulus)
}


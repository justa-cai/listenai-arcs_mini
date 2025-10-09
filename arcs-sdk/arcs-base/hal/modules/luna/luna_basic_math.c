/*
 * luna_basic_math.c
 *
 *  Created on: Sep 10, 2017
 *      Author: dwwang
 */

#include "luna/luna.h"
#include "luna/luna_basic_math.h"
#include "luna/cmd/luna_vector_cmd.h"
#include "luna_privates.h"

_FAST_DATA_ZI static LunaVectorParams_t  vector_params;

#define LUNA_VECTOR_API_EXEC(src_addr1, src_addr2, dst_addr, points, q, d_type, op_type, luna_api)	\
		int32_t ret = 0;	\
		vector_params.src1 = LUNA_SHARE_ADDR_OFFSET(src_addr1);	\
		vector_params.src2 = LUNA_SHARE_ADDR_OFFSET(src_addr2);	\
		vector_params.dst = LUNA_SHARE_ADDR_OFFSET(dst_addr);	\
		vector_params.size = points;	\
		vector_params.shift = q; \
		vector_params.dtype = d_type; \
		vector_params.api_type = op_type; \
		ret = luna_execute_cmd(luna_api, (void *)&vector_params, sizeof(LunaVectorParams_t)); \
		return ret;

#define LUNA_VECTOR_ONE_SRC_ADDR_API_EXEC(src_addr, value, dst_addr, points, q, d_type, op_type, luna_api)	\
		int32_t ret = 0;	\
		vector_params.src1 = LUNA_SHARE_ADDR_OFFSET(src_addr);	\
		vector_params.scalar = value;	\
		vector_params.dst = LUNA_SHARE_ADDR_OFFSET(dst_addr);	\
		vector_params.size = points;	\
		vector_params.shift = q; \
		vector_params.dtype = d_type; \
		vector_params.api_type = op_type; \
		ret = luna_execute_cmd(luna_api, (void *)&vector_params, sizeof(LunaVectorParams_t));	\
		return ret;

int32_t luna_add_i8i8o8(const int8_t *src1, const int8_t *src2, int8_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_VECTOR_API_EXEC(src1, src2, dst, size, shift, 0x11, 0, luna_api_vector_add_new)
}

int32_t luna_add_i8i8o32(const int8_t* src1, const int8_t* src2, int32_t* dst, uint32_t size, uint32_t shift)
{
	LUNA_VECTOR_API_EXEC(src1, src2, dst, size, shift, 0x31, 0, luna_api_vector_add_new)
}

int32_t luna_add_i32i32o8(const int32_t* src1, const int32_t* src2, int8_t* dst, uint32_t size, uint32_t shift)
{
	LUNA_VECTOR_API_EXEC(src1, src2, dst, size, shift, 0x13, 0, luna_api_vector_add_new)
}

int32_t luna_add_i32i32o32(const int32_t *src1, const int32_t *src2, int32_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_VECTOR_API_EXEC(src1, src2, dst, size, shift, 0x33, 0, luna_api_vector_add_new)
}

int32_t luna_sub_i8i8o8(const int8_t *src1, const int8_t *src2, int8_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_VECTOR_API_EXEC(src1, src2, dst, size, shift, 0x11, 1, luna_api_vector_add_new)
}

int32_t luna_sub_i8i8o32(const int8_t* src1, const int8_t* src2, int32_t* dst, uint32_t size, uint32_t shift)
{
	LUNA_VECTOR_API_EXEC(src1, src2, dst, size, shift, 0x31, 1, luna_api_vector_add_new)
}

int32_t luna_sub_i32i32o8(const int32_t *src1, const int32_t *src2, int8_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_VECTOR_API_EXEC(src1, src2, dst, size, shift, 0x13, 1, luna_api_vector_add_new)
}

int32_t luna_sub_i32i32o32(const int32_t *src1, const int32_t *src2, int32_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_VECTOR_API_EXEC(src1, src2, dst, size, shift, 0x33, 1, luna_api_vector_add_new)
}

int32_t luna_offset_i8i8o8(const  int8_t *src, const int8_t offset, int8_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_VECTOR_ONE_SRC_ADDR_API_EXEC(src, (offset&0xFF), dst, size, shift, 0x11, 2, luna_api_vector_add_new)
}

int32_t luna_offset_i8i8o32(const  int8_t *src, const int8_t offset, int32_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_VECTOR_ONE_SRC_ADDR_API_EXEC(src, (offset&0xFF), dst, size, shift, 0x31, 2, luna_api_vector_add_new)
}

int32_t luna_offset_i32i32o8(const  int32_t *src, const int32_t offset, int8_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_VECTOR_ONE_SRC_ADDR_API_EXEC(src, (offset&0xFFFFFFFF), dst, size, shift, 0x13, 2, luna_api_vector_add_new)
}

int32_t luna_offset_i32i32o32(const  int32_t *src, const int32_t offset, int32_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_VECTOR_ONE_SRC_ADDR_API_EXEC(src, (offset&0xFFFFFFFF), dst, size, shift, 0x33, 2, luna_api_vector_add_new)
}

int32_t luna_mul_i8i8o8(const int8_t *src1, const int8_t *src2, int8_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_VECTOR_API_EXEC(src1, src2, dst, size, shift, 0x11, 0, luna_api_vector_mul_new)
}

int32_t luna_mul_i8i8o32(const int8_t *src1, const int8_t *src2, int32_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_VECTOR_API_EXEC(src1, src2, dst, size, shift, 0x31, 0, luna_api_vector_mul_new)
}

int32_t luna_mul_i32i32o8(const int32_t *src1, const int32_t *src2, int8_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_VECTOR_API_EXEC(src1, src2, dst, size, shift, 0x13, 0, luna_api_vector_mul_new)
}

int32_t luna_mul_i32i32o32(const int32_t *src1, const int32_t *src2, int32_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_VECTOR_API_EXEC(src1, src2, dst, size, shift, 0x33, 0, luna_api_vector_mul_new)
}


int32_t luna_scale_i8i8o8(const int8_t *src, const int8_t scalar, int8_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_VECTOR_ONE_SRC_ADDR_API_EXEC(src, (scalar&0xFF), dst, size, shift, 0x11, 1, luna_api_vector_mul_new)
}

int32_t luna_scale_i8i8o32(const int8_t *src, const int8_t scalar, int32_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_VECTOR_ONE_SRC_ADDR_API_EXEC(src, (scalar&0xFF), dst, size, shift, 0x31, 1, luna_api_vector_mul_new)
}

int32_t luna_scale_i32i32o8(const int32_t *src, const int32_t scalar, int8_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_VECTOR_ONE_SRC_ADDR_API_EXEC(src, (scalar&0xFFFFFFFF), dst, size, shift, 0x13, 1, luna_api_vector_mul_new)
}

int32_t luna_scale_i32i32o32(const int32_t *src, const int32_t scalar, int32_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_VECTOR_ONE_SRC_ADDR_API_EXEC(src, (scalar&0xFFFFFFFF), dst, size, shift, 0x33, 1, luna_api_vector_mul_new)
}


int32_t luna_dot_prod_i8i8o8(const int8_t *src1, const int8_t *src2, int8_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_VECTOR_API_EXEC(src1, src2, dst, size, shift, 0x11, 2, luna_api_vector_mul_new)
}

int32_t luna_dot_prod_i8i8o32(const int8_t *src1, const int8_t *src2, int32_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_VECTOR_API_EXEC(src1, src2, dst, size, shift, 0x31, 2, luna_api_vector_mul_new)
}

int32_t luna_dot_prod_i32i32o8(const int32_t *src1, const int32_t *src2, int8_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_VECTOR_API_EXEC(src1, src2, dst, size, shift, 0x13, 2, luna_api_vector_mul_new)
}

int32_t luna_dot_prod_i32i32o32(const int32_t *src1, const int32_t *src2, int32_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_VECTOR_API_EXEC(src1, src2, dst, size, shift, 0x33, 2, luna_api_vector_mul_new)
}

int32_t luna_dot_prod_i32i32o64(const int32_t *src1, const int32_t *src2, int64_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_VECTOR_API_EXEC(src1, src2, dst, size, shift, 0x43, 2, luna_api_vector_mul_new)
}

int32_t luna_vector_sum_i8o8(const int8_t *src, int8_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_VECTOR_ONE_SRC_ADDR_API_EXEC(src, 0, dst, size, shift, 0x11, 3, luna_api_vector_mul_new)
}

int32_t luna_vector_sum_i8o32(const int8_t *src, int32_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_VECTOR_ONE_SRC_ADDR_API_EXEC(src, 0, dst, size, shift, 0x31, 3, luna_api_vector_mul_new)
}

int32_t luna_vector_sum_i32o8(const int32_t *src, int8_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_VECTOR_ONE_SRC_ADDR_API_EXEC(src, 0, dst, size, shift, 0x13, 3, luna_api_vector_mul_new)
}

int32_t luna_vector_sum_i32o32(const int32_t *src, int32_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_VECTOR_ONE_SRC_ADDR_API_EXEC(src, 0, dst, size, shift, 0x33, 3, luna_api_vector_mul_new)
}

int32_t luna_vector_sum_i32o64(const int32_t *src, int64_t *dst, uint32_t size, uint32_t shift)
{
	LUNA_VECTOR_ONE_SRC_ADDR_API_EXEC(src, 0, dst, size, shift, 0x43, 3, luna_api_vector_mul_new)
}


int32_t luna_cmp_vv_i8i8o8(const int8_t *src1, const int8_t *src2, int8_t *dst, uint32_t size, uint32_t cmp_mode)
{
	LUNA_VECTOR_API_EXEC(src1, src2, dst, size, cmp_mode, 0x1a, 0, luna_api_vector_cmp)
}

int32_t luna_cmp_vv_i32i32o32(const int32_t *src1, const int32_t *src2, int32_t *dst, uint32_t size, uint32_t cmp_mode)
{
	LUNA_VECTOR_API_EXEC(src1, src2, dst, size, cmp_mode, 0x3a, 0, luna_api_vector_cmp)
}

int32_t luna_cmp_vs_i8i8o8(const int8_t *src1, const int8_t scalar, int8_t *dst, uint32_t size, uint32_t cmp_mode)
{
	LUNA_VECTOR_ONE_SRC_ADDR_API_EXEC(src1, (scalar&0xFF), dst, size, cmp_mode, 0x1b, 0, luna_api_vector_cmp)
}

int32_t luna_cmp_vs_i32i32o32(const int32_t *src1, const int32_t scalar, int32_t *dst, uint32_t size, uint32_t cmp_mode)
{
	LUNA_VECTOR_ONE_SRC_ADDR_API_EXEC(src1, (scalar&0xFFFFFFFF), dst, size, cmp_mode, 0x3b, 0, luna_api_vector_cmp)
}

int32_t luna_max_i8o32(const int8_t *src, int32_t *dst, uint32_t size)
{
	LUNA_VECTOR_ONE_SRC_ADDR_API_EXEC(src, 0, dst, size, 0, 0x1A, 0, luna_api_vector_maxmin)
}

int32_t luna_max_i32o32(const int32_t *src, int32_t* dst, uint32_t size)
{
	LUNA_VECTOR_ONE_SRC_ADDR_API_EXEC(src, 0, dst, size, 0, 0x3A, 0, luna_api_vector_maxmin)
}

int32_t luna_min_i8o32(const int8_t *src, int32_t *dst, uint32_t size)
{
	LUNA_VECTOR_ONE_SRC_ADDR_API_EXEC(src, 0, dst, size, 0, 0x1B, 0, luna_api_vector_maxmin)
}

int32_t luna_min_i32o32(const int32_t *src, int32_t* dst, uint32_t size)
{
	LUNA_VECTOR_ONE_SRC_ADDR_API_EXEC(src, 0, dst, size, 0, 0x3B, 0, luna_api_vector_maxmin)
}

int32_t luna_div_i32i32o32(const int32_t *src1, const int32_t *src2, int32_t *dst, uint32_t size, uint32_t shift)
{
	int32_t ret = 0;
	const uint32_t once_div_size = 1024;
	vector_params.shift = shift;
	vector_params.dtype = 0;
	vector_params.api_type = 0;
	for (int i = 0; i < size; i += once_div_size)
	{
		uint32_t tmp_size = (size > (i + once_div_size)) ? once_div_size : (size > once_div_size ? (size - i) : size);
		vector_params.src1 = LUNA_SHARE_ADDR_OFFSET(src2 + i);
		vector_params.src2 = LUNA_SHARE_ADDR_OFFSET(src1 + i);
		vector_params.dst = LUNA_SHARE_ADDR_OFFSET(dst + i);
		vector_params.size = tmp_size;
		ret = luna_execute_cmd(luna_api_vector_div, (void *)&vector_params, sizeof(LunaVectorParams_t));
	}
	return ret;
}


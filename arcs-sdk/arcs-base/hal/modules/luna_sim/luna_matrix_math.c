/*
 * luna_matrix_math.c
 *
 *  Created on: Sep 10, 2017
 *      Author: dwwang
 */

#include "luna_sim/luna.h"
#include "luna_sim/luna_matrix_math.h"
#include "luna_sim/common/common.h"
#include "luna_sim/common/check.h"

#define MATRIX_L_SIZE (8*1024)   	
#define MATRIX_R_SIZE (16*1024)	

static int32_t LUNA_API_SIM(luna_trans_axis_common)(const void *src, void *dst, uint32_t *in_shape, uint32_t *axis, uint32_t n_dims, uint32_t precision)
{	
	uint32_t c, h, w, mode;
	uint32_t i_inv,o_inv,i_addr_inc,o_addr_inc,loop_num,plane_h,plane_w;
	//uint32_t precision = 8;

	c = in_shape[0];
	h = in_shape[1];
	w = in_shape[2];
	mode = (axis[2]<<8)|(axis[1]<<4)|(axis[0]);

	if (0x120==mode) { //hwc
		i_inv    = w*precision/8;
		o_inv    = h*precision/8;
		i_addr_inc = w*h*precision/8;
		o_addr_inc = w*h*precision/8;
		loop_num   = c;
		plane_h  = h;
		plane_w  = w;
	} else if(0x102==mode) { //hcw
		i_inv    = w*precision/8;
		o_inv    = h*c*precision/8;
		i_addr_inc = w*h*precision/8;
		o_addr_inc = h*precision/8;
		loop_num   = c;
		plane_h  = h;
		plane_w  = w;
	} else if(0x012==mode) { //chw
		i_inv    = w*h*precision/8;
		o_inv    = h*c*precision/8;
		i_addr_inc = w*precision/8;
		o_addr_inc = c*precision/8;
		loop_num   = h;
		plane_h  = c;
		plane_w  = w;
	} else if(0x021==mode) { //cwh
		i_inv    = w*h*precision/8;
		o_inv    = c*precision/8;
		i_addr_inc = w*precision/8;
		o_addr_inc = w*c*precision/8;
		loop_num   = h;
		plane_h  = c;
		plane_w  = w;
	} else if(0x201==mode) { //wch
		i_inv    = w*precision/8;
		o_inv    = w*c*precision/8;
		i_addr_inc = w*h*precision/8;
		o_addr_inc = w*precision/8;
		loop_num   = c;
		plane_h  = h;
		plane_w  = w;
	} else {
		return -1;
	}

	i_inv = i_inv/(precision/8);
	o_inv = o_inv/(precision/8);
	
	if (0x201==mode) {
		if (8==precision) {
			int i, j, l;
			int8_t *src_0, *dst_0;
			for (l = 0; l < loop_num; l++) {
				src_0 = (int8_t *)((char*)src + l*i_addr_inc);
				dst_0 = (int8_t *)((char*)dst + l*o_addr_inc);
				for (i = 0; i < plane_h; i++) {
					for (j = 0; j < plane_w; j++) {
						dst_0[i*o_inv + j] = src_0[i*i_inv + j];
					}
				}
			}
		}
		else if (32==precision) {
			int i, j, l;
			int32_t *src_0, *dst_0;
			for (l = 0; l < loop_num; l++) {
				src_0 = (int32_t *)((char*)src + l*i_addr_inc);
				dst_0 = (int32_t *)((char*)dst + l*o_addr_inc);
				for (i = 0; i < plane_h; i++) {
					for (j = 0; j < plane_w; j++) {
						dst_0[i*o_inv + j] = src_0[i*i_inv + j];
					}
				}
			}
		} else {
			return -1;
		}
	} else {
		if (8==precision) {
			int i, j, l;
			int8_t *src_0, *dst_0;
			for (l = 0; l < loop_num; l++) {
				src_0 = (int8_t *)((char*)src + l*i_addr_inc);
				dst_0 = (int8_t *)((char*)dst + l*o_addr_inc);
				for (i = 0; i < plane_h; i++) {
					for (j = 0; j < plane_w; j++) {
						dst_0[j*o_inv + i] = src_0[i*i_inv + j];
					}
				}
			}
		}
		else if (32==precision) {
			int i, j, l;
			int32_t *src_0, *dst_0;
			for (l = 0; l < loop_num; l++) {
				src_0 = (int32_t *)((char*)src + l*i_addr_inc);
				dst_0 = (int32_t *)((char*)dst + l*o_addr_inc);
				for (i = 0; i < plane_h; i++) {
					for (j = 0; j < plane_w; j++) {
						dst_0[j*o_inv + i] = src_0[i*i_inv + j];
					}
				}
			}
		} else {
			return -1;
		}
	}
	
	return 0;
}


int32_t LUNA_API_SIM(luna_mat_mul_i8i8o8)(const int8_t *src1, const int8_t *src2, int8_t *dst, uint32_t row, uint32_t col, uint32_t col2, uint32_t shift)
{
	LUNA_CHECK_PARAM_MAT_MUL(src1, src2, dst, row, col, col2, shift);
	{
		int8_t tmp_src1[MATRIX_L_SIZE];
		int8_t tmp_src2[MATRIX_R_SIZE];
		memcpy(tmp_src1, src1, sizeof(int8_t)*row *col);
		memcpy(tmp_src2, src2, sizeof(int8_t)*col *col2);
		{
			uint32_t m, s, n;
			q63_t q63_tmp, q63_dst;

			for (m = 0; m < row; m++) {
				for(s=0;s < col2; s++) {
					q63_dst = 0;
					for(n=0; n<col; n++) {
						q63_tmp = (q63_t)((q63_t)(tmp_src1[m * col + n]) * (q63_t)(tmp_src2[n * col2 + s]));
						q63_dst += q63_tmp;
					}
					q63_dst = shfit_floor_x05_int64(q63_dst, shift);
					dst[m * col2 + s] = luna_saturate_q63_to_q7(q63_dst);
				}
			}

		}
	}
	return 0;
}

int32_t LUNA_API_SIM(luna_mat_mul_i8i8o32)(const int8_t *src1, const int8_t *src2, int32_t *dst, uint32_t row, uint32_t col, uint32_t col2, uint32_t shift)
{
	LUNA_CHECK_PARAM_MAT_MUL(src1, src2, dst, row, col, col2, shift);
	{
		int8_t tmp_src1[MATRIX_L_SIZE];
		int8_t tmp_src2[MATRIX_R_SIZE];
		memcpy(tmp_src1, src1, sizeof(int8_t)*row *col);
		memcpy(tmp_src2, src2, sizeof(int8_t)*col *col2);
		{
			uint32_t m, s, n;
			q63_t q63_tmp, q63_dst;

			for (m = 0; m < row; m++) {
				for(s=0;s < col2; s++) {
					q63_dst = 0;
					for(n=0; n<col; n++) {
						q63_tmp = (q63_t)((q63_t)(tmp_src1[m * col + n]) * (q63_t)(tmp_src2[n * col2 + s]));
						q63_dst += q63_tmp;
					}
					q63_dst = shfit_floor_x05_int64(q63_dst, shift);
					dst[m * col2 + s] = luna_saturate_q63_to_q31(q63_dst);
				}
			}
		}
	}
	return 0;
}

int32_t LUNA_API_SIM(luna_mat_mul_i32i32o8)(const int32_t *src1, const int32_t *src2, int8_t *dst, uint32_t row, uint32_t col, uint32_t col2, uint32_t shift)
{
	LUNA_CHECK_PARAM_MAT_MUL(src1, src2, dst, row, col, col2, shift);
	{
		int32_t tmp_src1[MATRIX_L_SIZE / sizeof(int32_t)];
		int32_t tmp_src2[MATRIX_R_SIZE / sizeof(int32_t)];
		memcpy(tmp_src1, src1, sizeof(int32_t)*row *col);
		memcpy(tmp_src2, src2, sizeof(int32_t)*col *col2); 
		{
			uint32_t m, s, n;
			q63_t d;
			q63_t f_dst;
			q63_t sum[2];
			for (m = 0; m < row; m++)
			{
				for (s = 0; s < col2; s++)
				{
					sum[0] = sum[1] = 0;
					for (n = 0; n < col; n++)
					{
						d = (q63_t)((q63_t)(tmp_src1[m * col + n]) * (q63_t)(tmp_src2[n * col2 + s]));
						luna_q63_add_new(sum, d);
					}
					f_dst = luna_q63_shift(sum, shift);
					dst[m * col2 + s] = luna_saturation_int64_to_int8(f_dst);
				}
			}
		}
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_mat_mul_i32i32o32)(const int32_t *src1, const int32_t *src2, int32_t *dst, uint32_t row, uint32_t col, uint32_t col2, uint32_t shift)
{
	LUNA_CHECK_PARAM_MAT_MUL(src1, src2, dst, row, col, col2, shift);

	{
		int32_t tmp_src1[MATRIX_L_SIZE / sizeof(int32_t)];
		int32_t tmp_src2[MATRIX_R_SIZE / sizeof(int32_t)];
		memcpy(tmp_src1, src1, sizeof(int32_t)*row *col);
		memcpy(tmp_src2, src2, sizeof(int32_t)*col *col2);
		{
			uint32_t m, s, n;
			q63_t d;
			q63_t f_dst;
			q63_t sum[2];
			for (m = 0; m < row; m++) 
			{
				for (s = 0; s < col2; s++) 
				{
					sum[0] = sum[1] = 0;
					for (n = 0; n < col; n++)
					{
						d = (q63_t)((q63_t)(tmp_src1[m * col + n]) * (q63_t)(tmp_src2[n * col2 + s]));
						luna_q63_add_new(sum, d);
					}
					f_dst = luna_q63_shift(sum, shift);
					dst[m * col2 + s] = luna_saturation_int64_to_int32(f_dst);
				}
			}
		}
	}
	return 0;
}


int32_t LUNA_API_SIM(luna_mat_mul_inv_i8i8o8)(const int8_t *src1, const int8_t *src2, int8_t *dst, uint32_t row, uint32_t col, uint32_t col2, uint32_t i_inv1, uint32_t i_inv2, uint32_t o_inv, uint32_t shift)
{
	LUNA_CHECK_PARAM_MAT_MUL_INV(src1, src2, dst, row, col, col2, i_inv1, i_inv2, o_inv, shift);
	{
		uint32_t m, s, n;
		q63_t d;
		q63_t f_dst;
		q63_t sum[2];
		for (m = 0; m < row; m++) 
		{
			for (s = 0; s < col2; s++) 
			{
				sum[0] = sum[1] = 0;
				for (n = 0; n < col; n++)
				{
					d = (q63_t)((q63_t)(src1[m * i_inv1 + n]) * (q63_t)(src2[n * i_inv2 + s]));
					luna_q63_add_new(sum, d);
				}
				f_dst = luna_q63_shift(sum, shift);
				dst[m * o_inv + s] = luna_saturation_int64_to_int8(f_dst);
			}
		}
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_mat_mul_inv_i8i8o32)(const int8_t *src1, const int8_t *src2, int32_t *dst, uint32_t row, uint32_t col, uint32_t col2, uint32_t i_inv1, uint32_t i_inv2, uint32_t o_inv, uint32_t shift)
{
	LUNA_CHECK_PARAM_MAT_MUL_INV(src1, src2, dst, row, col, col2, i_inv1, i_inv2, o_inv, shift);
	{
		uint32_t m, s, n;
		q63_t d;
		q63_t f_dst;
		q63_t sum[2];
		for (m = 0; m < row; m++) 
		{
			for (s = 0; s < col2; s++) 
			{
				sum[0] = sum[1] = 0;
				for (n = 0; n < col; n++)
				{
					d = (q63_t)((q63_t)(src1[m * i_inv1 + n]) * (q63_t)(src2[n * i_inv2 + s]));
					luna_q63_add_new(sum, d);
				}
				f_dst = luna_q63_shift(sum, shift);
				dst[m * o_inv + s] = luna_saturation_int64_to_int32(f_dst);
			}
		}
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_mat_mul_inv_i32i32o8)(const int32_t *src1, const int32_t *src2, int8_t *dst, uint32_t row, uint32_t col, uint32_t col2, uint32_t i_inv1, uint32_t i_inv2, uint32_t o_inv, uint32_t shift)
{
	LUNA_CHECK_PARAM_MAT_MUL_INV(src1, src2, dst, row, col, col2, i_inv1, i_inv2, o_inv, shift);
	{
		uint32_t m, s, n;
		q63_t d;
		q63_t f_dst;
		q63_t sum[2];
		for (m = 0; m < row; m++) 
		{
			for (s = 0; s < col2; s++) 
			{
				sum[0] = sum[1] = 0;
				for (n = 0; n < col; n++)
				{
					d = (q63_t)((q63_t)(src1[m * i_inv1 + n]) * (q63_t)(src2[n * i_inv2 + s]));
					luna_q63_add_new(sum, d);
				}
				f_dst = luna_q63_shift(sum, shift);
				dst[m * o_inv + s] = luna_saturation_int64_to_int8(f_dst);
			}
		}
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_mat_mul_inv_i32i32o32)(const int32_t *src1, const int32_t *src2, int32_t *dst, uint32_t row, uint32_t col, uint32_t col2, uint32_t i_inv1, uint32_t i_inv2, uint32_t o_inv, uint32_t shift)
{
	LUNA_CHECK_PARAM_MAT_MUL_INV(src1, src2, dst, row, col, col2, i_inv1, i_inv2, o_inv, shift);
	{
		uint32_t m, s, n;
		q63_t d;
		q63_t f_dst;
		q63_t sum[2];
		for (m = 0; m < row; m++) 
		{
			for (s = 0; s < col2; s++) 
			{
				sum[0] = sum[1] = 0;
				for (n = 0; n < col; n++)
				{
					d = (q63_t)((q63_t)(src1[m * i_inv1 + n]) * (q63_t)(src2[n * i_inv2 + s]));
					luna_q63_add_new(sum, d);
				}
				f_dst = luna_q63_shift(sum, shift);
				dst[m * o_inv + s] = luna_saturation_int64_to_int32(f_dst);
			}
		}
	}

	return 0;
}


int32_t LUNA_API_SIM(luna_split_mat_mul_i8i8o8)(const int8_t *src1, const int8_t *src2, int8_t *dst, uint32_t row, uint32_t col, uint32_t col2, uint32_t shift)
{
	LUNA_CHECK_PARAM_MAT_MUL_SPLIT(src1, src2, dst, row, col, col2, split_num, shift);
	{
		{
			uint32_t m, s, n;
			q63_t q63_tmp, q63_dst;

			for (m = 0; m < row; m++) {
				for(s=0;s < col2; s++) {
					q63_dst = 0;
					for(n=0; n<col; n++) {
						q63_tmp = (q63_t)((q63_t)(src1[m * col + n]) * (q63_t)(src2[n * col2 + s]));
						q63_dst += q63_tmp;
					}
					q63_dst = shfit_floor_x05_int64(q63_dst, shift);
					dst[m * col2 + s] = luna_saturate_q63_to_q7(q63_dst);
				}
			}

		}
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_split_mat_mul_i8i8o32)(const int8_t *src1, const int8_t *src2, int32_t *dst, uint32_t row, uint32_t col, uint32_t col2, uint32_t shift)
{
	LUNA_CHECK_PARAM_MAT_MUL_SPLIT(src1, src2, dst, row, col, col2, split_num, shift);
	{
		{
			uint32_t m, s, n;
			q63_t q63_tmp, q63_dst;

			for (m = 0; m < row; m++) {
				for(s=0;s < col2; s++) {
					q63_dst = 0;
					for(n=0; n<col; n++) {
						q63_tmp = (q63_t)((q63_t)(src1[m * col + n]) * (q63_t)(src2[n * col2 + s]));
						q63_dst += q63_tmp;
					}
					q63_dst = shfit_floor_x05_int64(q63_dst, shift);
					dst[m * col2 + s] = luna_saturate_q63_to_q31(q63_dst);
				}
			}
		}
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_split_mat_mul_i32i32o8)(const int32_t *src1, const int32_t *src2, int8_t *dst, uint32_t row, uint32_t col, uint32_t col2, uint32_t shift)
{
	LUNA_CHECK_PARAM_MAT_MUL_SPLIT(src1, src2, dst, row, col, col2, split_num, shift);
	{
		{
			uint32_t m, s, n;
			q63_t d;
			q63_t f_dst;
			q63_t sum[2];
			for (m = 0; m < row; m++)
			{
				for (s = 0; s < col2; s++)
				{
					sum[0] = sum[1] = 0;
					for (n = 0; n < col; n++)
					{
						d = (q63_t)((q63_t)(src1[m * col + n]) * (q63_t)(src2[n * col2 + s]));
						luna_q63_add_new(sum, d);
					}
					f_dst = luna_q63_shift(sum, shift);
					dst[m * col2 + s] = luna_saturation_int64_to_int8(f_dst);
				}
			}
		}
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_split_mat_mul_i32i32o32)(const int32_t *src1, const int32_t *src2, int32_t *dst, uint32_t row, uint32_t col, uint32_t col2, uint32_t shift)
{
	LUNA_CHECK_PARAM_MAT_MUL_SPLIT(src1, src2, dst, row, col, col2, split_num, shift);
	{
		{
			uint32_t m, s, n;
			q63_t d;
			q63_t f_dst;
			q63_t sum[2];
			for (m = 0; m < row; m++) 
			{
				for (s = 0; s < col2; s++) 
				{
					sum[0] = sum[1] = 0;
					for (n = 0; n < col; n++)
					{
						d = (q63_t)((q63_t)(src1[m * col + n]) * (q63_t)(src2[n * col2 + s]));
						luna_q63_add_new(sum, d);
					}
					f_dst = luna_q63_shift(sum, shift);
					dst[m * col2 + s] = luna_saturation_int64_to_int32(f_dst);
				}
			}
		}
	}

	return 0;
}


int32_t LUNA_API_SIM(luna_group_mat_mul_i8i8o8)(const int8_t *src1, const int8_t *src2, int8_t *dst, uint32_t group_num, uint32_t row, uint32_t col, uint32_t col2, uint32_t shift)
{
	for (size_t g = 0; g < group_num; g++)
	{
		LUNA_API_SIM(luna_mat_mul_inv_i8i8o8)(src1 + g*col, src2 + g*col*col2, dst + g*col2, row, col, col2, col*group_num, col2, col2*group_num, shift);
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_group_mat_mul_i8i8o32)(const int8_t *src1, const int8_t *src2, int32_t *dst, uint32_t group_num, uint32_t row, uint32_t col, uint32_t col2, uint32_t shift)
{
	for (size_t g = 0; g < group_num; g++)
	{
		LUNA_API_SIM(luna_mat_mul_inv_i8i8o32)(src1 + g*col, src2 + g*col*col2, dst + g*col2, row, col, col2, col*group_num, col2, col2*group_num, shift);
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_group_mat_mul_i32i32o8)(const int32_t *src1, const int32_t *src2, int8_t *dst, uint32_t group_num, uint32_t row, uint32_t col, uint32_t col2, uint32_t shift)
{
	for (size_t g = 0; g < group_num; g++)
	{
		LUNA_API_SIM(luna_mat_mul_inv_i32i32o8)(src1 + g*col, src2 + g*col*col2, dst + g*col2, row, col, col2, col*group_num, col2, col2*group_num, shift);
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_group_mat_mul_i32i32o32)(const int32_t *src1, const int32_t *src2, int32_t *dst, uint32_t group_num, uint32_t row, uint32_t col, uint32_t col2, uint32_t shift)
{
	for (size_t g = 0; g < group_num; g++)
	{
		LUNA_API_SIM(luna_mat_mul_inv_i32i32o32)(src1 + g*col, src2 + g*col*col2, dst + g*col2, row, col, col2, col*group_num, col2, col2*group_num, shift);
	}

	return 0;
}


int32_t LUNA_API_SIM(luna_mat_trans_i8o8)(const int8_t *src, int8_t *dst, uint32_t row, uint32_t col)
{
	LUNA_CHECK_PARAM_MAT_TRANS(src, dst, row, col);
	{
		int i, j;
		int8_t buffer_tmp[MATRIX_R_SIZE];
		memcpy(buffer_tmp, src, row*col*sizeof(int8_t));
		for (i = 0; i < row; i++)
		{
			for (j = 0; j < col; j++)
			{
				dst[j*row + i] = buffer_tmp[i*col + j];
			}
		}
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_mat_trans_i32o32)(const int32_t *src, int32_t *dst, uint32_t row, uint32_t col)
{
	LUNA_CHECK_PARAM_MAT_TRANS(src, dst, row, col);
	{
		int i, j;
		int32_t buffer_tmp[MATRIX_R_SIZE / sizeof(int32_t)];
		memcpy(buffer_tmp, src, row*col*sizeof(int32_t));
		for (i = 0; i < row; i++)
		{
			for (j = 0; j < col; j++)
			{
				dst[j*row + i] = buffer_tmp[i*col + j];
			}
		}
	}

	return 0;
}


int32_t LUNA_API_SIM(luna_mat_trans_inv_i8o8)(const int8_t *src, int8_t *dst, uint32_t row, uint32_t col, uint32_t i_inv, uint32_t o_inv)
{
	LUNA_CHECK_PARAM_MAT_TRANS_INV(src, dst, row, col, i_inv, o_inv);
	{
		int i, j;
		for (i = 0; i < row; i++)
		{
			for (j = 0; j < col; j++)
			{
				dst[j*o_inv + i] = src[i*i_inv + j];
			}
		}
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_mat_trans_inv_i32o32)(const int32_t *src, int32_t *dst, uint32_t row, uint32_t col, uint32_t i_inv, uint32_t o_inv)
{
	LUNA_CHECK_PARAM_MAT_TRANS_INV(src, dst, row, col, i_inv, o_inv);
	{
		int i, j;
		for (i = 0; i < row; i++)
		{
			for (j = 0; j < col; j++)
			{
				dst[j*o_inv + i] = src[i*i_inv + j];
			}
		}
	}

	return 0;
}


int32_t LUNA_API_SIM(luna_split_mat_trans_i8o8)(const int8_t *src, int8_t *dst, uint32_t row, uint32_t col)
{
	LUNA_CHECK_PARAM_MAT_TRANS_SPLIT(src, dst, row, col, split_num);
	{
		int i, j;
		for (i = 0; i < row; i++)
		{
			for (j = 0; j < col; j++)
			{
				dst[j*row + i] = src[i*col + j];
			}
		}
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_split_mat_trans_i32o32)(const int32_t *src, int32_t *dst, uint32_t row, uint32_t col)
{
	LUNA_CHECK_PARAM_MAT_TRANS_SPLIT(src, dst, row, col, split_num);
	{
		int i, j;
		for (i = 0; i < row; i++)
		{
			for (j = 0; j < col; j++)
			{
				dst[j*row + i] = src[i*col + j];
			}
		}
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_trans_axis_i8o8)(const int8_t *src, int8_t *dst, uint32_t *in_shape, uint32_t *axis, uint32_t n_dims)
{
	LUNA_CHECK_PARAM_MAT_TRANS_3D(src, dst, in_shape, axis, n_dims);

	return LUNA_API_SIM(luna_trans_axis_common)(src, dst, in_shape, axis, n_dims, 8);
}

int32_t LUNA_API_SIM(luna_trans_axis_i32o32)(const int32_t *src, int32_t *dst, uint32_t *in_shape, uint32_t *axis, uint32_t n_dims)
{
	LUNA_CHECK_PARAM_MAT_TRANS_3D(src, dst, in_shape, axis, n_dims);

	return LUNA_API_SIM(luna_trans_axis_common)(src, dst, in_shape, axis, n_dims, 32);
}


int32_t LUNA_API_SIM(luna_split_mat_mul_bias_i8i8i32o8)(const int8_t *src1, const int8_t *src2, const int32_t *bias, int8_t *dst, uint32_t row, uint32_t col, uint32_t col2, uint32_t shift)
{
	LUNA_CHECK_PARAM_MAT_MUL(src1, src2, dst, row, col, col2, shift);
	{
		{
			uint32_t m, s, n;
			q63_t q63_tmp, q63_dst;

			for (m = 0; m < row; m++) {
				for(s=0;s < col2; s++) {
					if (bias != 0)
						q63_dst = bias[m];
					else 
						q63_dst = 0;
					for(n=0; n<col; n++) {
						q63_tmp = (q63_t)((q63_t)(src1[m * col + n]) * (q63_t)(src2[n * col2 + s]));
						q63_dst += q63_tmp;
					}
					q63_dst = shfit_floor_x05_int64(q63_dst, shift);
					dst[m * col2 + s] = luna_saturate_q63_to_q7(q63_dst);
				}
			}

		}
	}
	return 0;
}

int32_t LUNA_API_SIM(luna_split_mat_mul_bias_i8i8i32o32)(const int8_t *src1, const int8_t *src2, const int32_t *bias, int32_t *dst, uint32_t row, uint32_t col, uint32_t col2, uint32_t shift)
{
	LUNA_CHECK_PARAM_MAT_MUL(src1, src2, dst, row, col, col2, shift);
	{
		{
			uint32_t m, s, n;
			q63_t q63_tmp, q63_dst;
			//MxN * NxL + Mx1 = MxL
			for (m = 0; m < row; m++) {
				for(s=0;s < col2; s++) {
					if (bias != 0)
						q63_dst = bias[m];
					else 
						q63_dst = 0;
					for(n=0; n<col; n++) {
						q63_tmp = (q63_t)((q63_t)(src1[m * col + n]) * (q63_t)(src2[n * col2 + s]));
						q63_dst += q63_tmp;
					}
					q63_dst = shfit_floor_x05_int64(q63_dst, shift);
					dst[m * col2 + s] = luna_saturate_q63_to_q31(q63_dst);
				}
			}
		}
	}
	return 0;
}

int32_t LUNA_API_SIM(luna_split_mat_mul_bias_i32i32i32o8)(const int32_t *src1, const int32_t *src2, const int32_t *bias, int8_t *dst, uint32_t row, uint32_t col, uint32_t col2, uint32_t shift)
{
	LUNA_CHECK_PARAM_MAT_MUL(src1, src2, dst, row, col, col2, shift);
	{
		{
			uint32_t m, s, n;
			q63_t d;
			q63_t f_dst;
			q63_t sum[2];
			for (m = 0; m < row; m++)
			{
				for (s = 0; s < col2; s++)
				{
					sum[0] = sum[1] = 0;
					if (bias != 0)
						luna_q63_add_new(sum, bias[m]);
					for (n = 0; n < col; n++)
					{
						d = (q63_t)((q63_t)(src1[m * col + n]) * (q63_t)(src2[n * col2 + s]));
						luna_q63_add_new(sum, d);
					}
					f_dst = luna_q63_shift(sum, shift);
					dst[m * col2 + s] = luna_saturation_int64_to_int8(f_dst);
				}
			}
		}
	}

	return 0;
}

int32_t LUNA_API_SIM(luna_split_mat_mul_bias_i32i32i32o32)(const int32_t *src1, const int32_t *src2, const int32_t *bias, int32_t *dst, uint32_t row, uint32_t col, uint32_t col2, uint32_t shift)
{
	LUNA_CHECK_PARAM_MAT_MUL(src1, src2, dst, row, col, col2, shift);

	{
		{
			uint32_t m, s, n;
			q63_t d;
			q63_t f_dst;
			q63_t sum[2];
			for (m = 0; m < row; m++) 
			{
				for (s = 0; s < col2; s++) 
				{
					sum[0] = sum[1] = 0;
					if (bias != 0)
						luna_q63_add_new(sum, (q63_t)(bias[m]));
					for (n = 0; n < col; n++)
					{
						d = (q63_t)((q63_t)(src1[m * col + n]) * (q63_t)(src2[n * col2 + s]));
						luna_q63_add_new(sum, d);
					}
					f_dst = luna_q63_shift(sum, shift);
					dst[m * col2 + s] = luna_saturation_int64_to_int32(f_dst);
				}
			}
		}
	}
	return 0;
}


static int8_t luna_extract_bit4(int8_t bit8, int8_t odd)
{
	if (odd) {
		//1: <<(0+24),>>28  extract l4 + sign bit exten.
		return (((int32_t)bit8)<<(24))>>(28);
	} else {
		//0: <<(4+24),>>28  extract h4.
		return (((int32_t)bit8)<<(28))>>(28);
	}
}

int32_t LUNA_API_SIM(luna_split_mat_mul_bias_i4i8i32o8)(const int8_t *src1, const int8_t *src2, const int32_t *bias, int8_t *dst, uint32_t row, uint32_t col, uint32_t col2, uint32_t shift)
{
	LUNA_CHECK_PARAM_MAT_MUL(src1, src2, dst, row, col, (col2>>1), shift);
	{
		{
			uint32_t m, s, n;
			int64_t lvalue,rvalue,acc;

			for (m = 0; m < row; m++) {
				for(s=0;s < col2; s++) {
					if (bias != 0)
						acc = bias[m];
					else
						acc = 0;
					for(n=0; n<col; n++) {
						lvalue = luna_extract_bit4(src1[(m * col + n)>>1], (m * col + n)&0x01);
						rvalue = src2[n * col2 + s];
						acc += lvalue * rvalue;
					}
					acc = shfit_floor_x05_int64(acc, shift);
					dst[m * col2 + s] = luna_saturate_q63_to_q7(acc);
				}
			}
		}
	}
	return 0;
}

int32_t LUNA_API_SIM(luna_split_mat_mul_bias_i4i8i32o32)(const int8_t *src1, const int8_t *src2, const int32_t *bias, int32_t *dst, uint32_t row, uint32_t col, uint32_t col2, uint32_t shift)
{
	LUNA_CHECK_PARAM_MAT_MUL(src1, src2, dst, row, col, (col2>>1), shift);
	{
		{
			uint32_t m, s, n;
			int64_t lvalue,rvalue,acc;

			for (m = 0; m < row; m++) {
				for(s=0;s < col2; s++) {
					if (bias != 0)
						acc = bias[m];
					else
						acc = 0;
					for(n=0; n<col; n++) {
						int64_t lvalue = luna_extract_bit4(src1[(m * col + n)>>1], (m * col + n)&0x01);
						int64_t rvalue = src2[n * col2 + s];
						acc += lvalue * rvalue;
					}
					acc = shfit_floor_x05_int64(acc, shift);
					dst[m * col2 + s] = luna_saturate_q63_to_q31(acc);
				}
			}
		}
	}
	return 0;
}

int32_t LUNA_API_SIM(luna_split_mat_mul_bias_i8i4i32o8)(const int8_t *src1, const int8_t *src2, const int32_t *bias, int8_t *dst, uint32_t row, uint32_t col, uint32_t col2, uint32_t shift)
{
	LUNA_CHECK_PARAM_MAT_MUL(src1, src2, dst, row, col, (col2>>1), shift);
	{
		{
			uint32_t m, s, n;
			int64_t lvalue,rvalue,acc;

			for (m = 0; m < row; m++) {
				for(s=0;s < col2; s++) {
					if (bias != 0)
						acc = bias[m];
					else
						acc = 0;
					for(n=0; n<col; n++) {
						lvalue = src1[m * col + n];
						rvalue = luna_extract_bit4(src2[(n * col2 + s)>>1], (n * col2 + s)&0x01);
						acc += lvalue * rvalue;
					}
					acc = shfit_floor_x05_int64(acc, shift);
					dst[m * col2 + s] = luna_saturate_q63_to_q7(acc);
				}
			}
		}
	}
	return 0;
}

int32_t LUNA_API_SIM(luna_split_mat_mul_bias_i8i4i32o32)(const int8_t *src1, const int8_t *src2, const int32_t *bias, int32_t *dst, uint32_t row, uint32_t col, uint32_t col2, uint32_t shift)
{
	LUNA_CHECK_PARAM_MAT_MUL(src1, src2, dst, row, col, (col2>>1), shift);
	{
		{
			uint32_t m, s, n;
			int64_t lvalue,rvalue,acc;

			for (m = 0; m < row; m++) {
				for(s=0;s < col2; s++) {
					if (bias != 0)
						acc = bias[m];
					else
						acc = 0;
					for(n=0; n<col; n++) {
						lvalue = src1[m * col + n];
						rvalue = luna_extract_bit4(src2[(n * col2 + s)>>1], (n * col2 + s)&0x01);
						acc += lvalue * rvalue;
					}
					acc = shfit_floor_x05_int64(acc, shift);
					dst[m * col2 + s] = luna_saturate_q63_to_q31(acc);
				}
			}
		}
	}
	return 0;
}

int32_t LUNA_API_SIM(luna_mat_copy_i8o8)(int8_t* src, int8_t* dst, uint32_t channel, uint32_t row, uint32_t col,
	uint32_t i_planar_inv, uint32_t i_row_inv, uint32_t o_planar_inv, uint32_t o_row_inv)
{
	int8_t *p_src_r, *p_dst_r; 
	for (uint32_t c = 0; c < channel; c++)
	{
		for (uint32_t r = 0; r < row; r++)
		{
			p_src_r = src + c*i_planar_inv + r*i_row_inv;
			p_dst_r = dst + c*o_planar_inv + r*o_row_inv;
			memcpy(p_dst_r, p_src_r, col*sizeof(int8_t));
		}
	}
	return 0;
}

int32_t LUNA_API_SIM(luna_mat_copy_i32o32)(int32_t* src, int32_t* dst, uint32_t channel, uint32_t row, uint32_t col,
	uint32_t i_planar_inv, uint32_t i_row_inv, uint32_t o_planar_inv, uint32_t o_row_inv)
{
	int32_t *p_src_r, *p_dst_r; 
	for (uint32_t c = 0; c < channel; c++)
	{
		for (uint32_t r = 0; r < row; r++)
		{
			p_src_r = src + c*i_planar_inv + r*i_row_inv;
			p_dst_r = dst + c*o_planar_inv + r*o_row_inv;
			memcpy(p_dst_r, p_src_r, col*sizeof(int32_t));
		}
	}
	return 0;
}
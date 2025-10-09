#ifndef __LUNA_CHECK_H_SIM__
#define __LUNA_CHECK_H_SIM__

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "luna_sim/luna_math_types.h"
#include "luna_sim/luna_cnn_math.h"
#include "luna_sim/luna_basic_math.h"

#define USE_LUNA_CHECK 0
#if USE_LUNA_CHECK

#ifndef LUNA_LOG
#define LUNA_LOG printf
#endif 
#define LUNA_ASSERT(c, format, ...)  do { if (!(c)) { LUNA_LOG("[luna error][%s]"format"\n", __FUNCTION__, ##__VA_ARGS__); abort(); } } while(0)
#define SIZE_ALIGIN(size, aligin) ((((size) + ((aligin) -1))) & (~((aligin) -1)))

void luna_set_check_enable(int enable);
int32_t luna_is_check_enable();
void luna_set_sharedmem(void* addr, uint32_t size);

int32_t luna_check_addr(const void* addr, uint32_t size, uint32_t alignment, int32_t type);
int32_t luna_check_mat_mul_size(uint32_t row, uint32_t col, uint32_t col2, uint32_t in1_bytes, uint32_t in2_bytes, uint32_t out_bits);
int32_t luna_check_mat_tans_size(uint32_t row, uint32_t col, uint32_t in1_bytes);
int32_t luna_check_mat_tans_col234_size(uint32_t row, uint32_t col, uint32_t in1_bytes);

int32_t luna_check_conv_paras(conv_struct_t * pConv, uint32_t in_bits, uint32_t out_bits);
int32_t luna_check_deconv_paras(conv_struct_t * pConv, uint32_t in_bits, uint32_t out_bits);
int32_t luna_check_depthwise_paras(conv_struct_t * pConv, uint32_t in_bits, uint32_t out_bits);
int32_t luna_check_split_conv_paras(conv_struct_t * pConv, uint32_t in_bits, uint32_t out_bits, uint32_t split_num);

int32_t luna_check_dma_cpy(int chn, void *dst, void *src, int32_t size);
int32_t luna_check_dma_wait(int chn);

#define LUNA_SIZE_64K (64*1024)
#define LUNA_SIZE_32K (32*1024)
#define LUNA_MAX(val1, val2)  ((val1)>(val2)?(val1):val2)
#define LUNA_MIN(val1, val2)  ((val1)<(val2)?(val1):val2)

#define LUNA_CHECK_START() if (luna_is_check_enable()) {
#define LUNA_CHECK_END() }

#define LUNA_CHECK_EQ_1(name, val, val1) LUNA_ASSERT((val) == (val1), "%s(%d) == {%d} error!", (name), (val), (val1));
#define LUNA_CHECK_EQ_2(name, val, val1, val2) LUNA_ASSERT((val) == (val1) || (val) == (val2), "%s(%d) == %d/%d error!", (name), (val), (val1), (val2));
#define LUNA_CHECK_EQ_3(name, val, val1, val2, val3) LUNA_ASSERT((val) == (val1) || (val) == (val2) || (val) == (val3), "%s(%d) == %d/%d/%d error!", (name), (val), (val1), (val2), (val3));
#define LUNA_CHECK_EQ_4(name, val, val1, val2, val3, val4) LUNA_ASSERT((val) == (val1) || (val) == (val2) || (val) == (val3) || (val) == (val4), "%s(%d) == %d/%d/%d/%d error!", (name), (val), (val1), (val2), (val3), (val4));
#define LUNA_CHECK_LE(name, val, maxval) LUNA_ASSERT((val) <= (maxval), "%s(%d) <= %d error!", (name), (val), (maxval));
#define LUNA_CHECK_LT(name, val, maxval) LUNA_ASSERT((val) < (maxval), "%s(%d) < %d error!", (name), (val), (maxval));
#define LUNA_CHECK_GE(name, val, maxval) LUNA_ASSERT((val) >= (maxval), "%s(%d) >= %d error!", (name), (val), (maxval));
#define LUNA_CHECK_GT(name, val, maxval) LUNA_ASSERT((val) > (maxval), "%s(%d) > %d error!", (name), (val), (maxval));
#define LUNA_CHECK_ALIGNED(name, val, alignment) LUNA_ASSERT((val)%(alignment)==0, "%s(%d) %% %d == 0 error!", (name),  (val), (alignment));
#define LUNA_CHECK_BETWEEN(name, val, minval, maxval) LUNA_ASSERT((val) >= (minval) && (val) <= (maxval), "%s(%d) between [%d, %d] error!", (name), (val), (minval), (maxval));

#define LUNA_CHECK_ADDR(addr, size, type) LUNA_ASSERT(luna_check_addr((addr), sizeof((addr)[0])*(size), sizeof((addr)[0]), (type)), "luna addr invalid!");
#define LUNA_CHECK_MAT_MUL_SIZE(row,col,col2,in1_bits,in2_bits,out_bits) LUNA_ASSERT(luna_check_mat_mul_size(row, col, col2, in1_bits, in2_bits, out_bits), "luna mat mul size invalid!");
#define LUNA_CHECK_MAT_TRANS_SIZE(row,col, in_bits, out_bits) LUNA_ASSERT(luna_check_mat_tans_size(row, col, in_bits), "luna mat trans size invalid!");
#define LUNA_CHECK_MAT_TRANS_COL234_SIZE(row,col,in_bits, out_bits) LUNA_ASSERT(luna_check_mat_tans_col234_size(row, col, in_bits), "luna mat trans size invalid!");

#define LUNA_CHECK_CONV_SIZE(pConv, in_bits, out_bits) LUNA_ASSERT(luna_check_conv_paras(pConv, in_bits, out_bits), "luna conv size invalid!");
#define LUNA_CHECK_DECONV_SIZE(pConv, in_bits, out_bits) LUNA_ASSERT(luna_check_deconv_paras(pConv, in_bits, out_bits), "luna conv size invalid!");
#define LUNA_CHECK_DEPTHWISE_SIZE(pConv, in_bits, out_bits) LUNA_ASSERT(luna_check_depthwise_paras(pConv, in_bits, out_bits), "luna conv size invalid!");
#define LUNA_CHECK_SPLIT_CONV_SIZE(pConv, in_bits, out_bits, split_num) LUNA_ASSERT(luna_check_split_conv_paras(pConv, in_bits, out_bits, split_num), "luna split conv size invalid!");
#define LUNA_CHECK_ADDR_BIAS(pBias, conv_struct_) if(conv_struct_->is_bias == 1){ LUNA_CHECK_ADDR(pBias, conv_struct_->output_w, 0); }

// dma
#define LUNA_CHECK_DMA_CPY(chn, dst, src, size) LUNA_ASSERT(luna_check_dma_cpy(chn, dst, src, size), "luna dma cpy invalid!");
#define LUNA_CHECK_DMA_WAIT(chn)  LUNA_ASSERT(luna_check_dma_wait(chn), "luna dma wait invalid!");

#define LUNA_CHECK_PARAM_DMA_CPY(chn, dst, src, size) \
		LUNA_CHECK_START() \
		LUNA_CHECK_DMA_CPY(chn, dst, src, size) \
		LUNA_CHECK_END()

#define LUNA_CHECK_PARAM_DMA_WAIT(chn) \
		LUNA_CHECK_START() \
		LUNA_CHECK_DMA_WAIT(chn) \
		LUNA_CHECK_END()

// vector
#define LUNA_CHECK_PARAM_VEC_2SRC1DST(src1, src2, dst, src1_size, src2_size, dst_size, shift) \
	LUNA_CHECK_START() \
		LUNA_CHECK_ADDR(src1, src1_size, 0) \
		LUNA_CHECK_ADDR(src2, src2_size, 0) \
		LUNA_CHECK_ADDR(dst, dst_size, 1) \
		LUNA_CHECK_LT("shift", shift, 64) \
	LUNA_CHECK_END()

#define LUNA_CHECK_PARAM_VEC_1SRC1DST(src1, dst, src1_size, dst_size, shift) \
	LUNA_CHECK_START() \
		LUNA_CHECK_ADDR(src1, src1_size, 0) \
		LUNA_CHECK_ADDR(dst, dst_size, 1) \
		LUNA_CHECK_LT("shift", shift, 64) \
	LUNA_CHECK_END()

#define LUNA_CHECK_PARAM_VEC_CMP_VV(src1, src2, dst, size, cmp_mode) \
	LUNA_CHECK_START() \
		LUNA_CHECK_ADDR(src1, size, 0) \
		LUNA_CHECK_ADDR(src2, size, 0) \
		LUNA_CHECK_ADDR(dst, size, 1) \
		LUNA_CHECK_BETWEEN("cmp_mode", cmp_mode, LUNA_CMP_GREATER_THAN, LUNA_CMP_EQUAL)\
	LUNA_CHECK_END()

#define LUNA_CHECK_PARAM_VEC_CMP_VS(src1, dst, size, cmp_mode) \
	LUNA_CHECK_START() \
		LUNA_CHECK_ADDR(src1, size, 0) \
		LUNA_CHECK_ADDR(dst, size, 1) \
		LUNA_CHECK_BETWEEN("cmp_mode", cmp_mode, LUNA_CMP_GREATER_THAN, LUNA_CMP_EQUAL)\
	LUNA_CHECK_END()

#if 1
#define LUNA_CHECK_PARAM_VEC_DOT_32(src1, src2, dst, size, shift) \
	LUNA_CHECK_START() \
		LUNA_CHECK_ADDR(src1, size, 0) \
		LUNA_CHECK_ADDR(src2, size, 0) \
		LUNA_CHECK_ADDR(dst, size, 1) \
		LUNA_CHECK_LT("shift", shift, 64) \
	LUNA_CHECK_END()
#else
#define LUNA_CHECK_PARAM_VEC_DOT_32(src1, src2, dst, size, shift) \
	LUNA_CHECK_START() \
		LUNA_CHECK_ADDR(src1, size, 0) \
		LUNA_CHECK_ADDR(src2, size, 0) \
		LUNA_CHECK_ADDR(dst, size, 1) \
		LUNA_CHECK_LE("size", size, 512) \
		LUNA_CHECK_LT("shift", shift, 64) \
	LUNA_CHECK_END()
#endif 

#define LUNA_CHECK_PARAM_VEC_DIV_32(src1, q_src1, src2, q_src2, dst, q_out, size) \
	LUNA_CHECK_START() \
		LUNA_CHECK_ADDR(src1, size, 0) \
		LUNA_CHECK_ADDR(src2, size, 0) \
		LUNA_CHECK_ADDR(dst, size, 1) \
		LUNA_CHECK_LE("size", size, 8*1024) \
	LUNA_CHECK_END()

// complex
#define LUNA_CHECK_PARAM_CLX(src1, src2, dst, src1_size, src2_size, dst_size, shift) \
	LUNA_CHECK_START() \
		LUNA_CHECK_ADDR(src1, src1_size, 0) \
		LUNA_CHECK_ADDR(src2, src2_size, 0) \
		LUNA_CHECK_ADDR(dst, dst_size, 1) \
		LUNA_CHECK_LT("shift", shift, 64) \
	LUNA_CHECK_END()

// matrix
#define LUNA_CHECK_PARAM_MAT_MUL(src1, src2, dst, row, col, col2, shift) \
	LUNA_CHECK_START() \
		LUNA_CHECK_ADDR(src1, row*col, 0) \
		LUNA_CHECK_ADDR(src2, col*col2, 0) \
		LUNA_CHECK_ADDR(dst, row*col2, 1) \
		LUNA_CHECK_LT("shift", shift, 64) \
		LUNA_CHECK_MAT_MUL_SIZE(row, col, col2, sizeof(src1[0])*8, sizeof(src2[0])*8, sizeof(dst[0])*8) \
	LUNA_CHECK_END()

#define LUNA_CHECK_PARAM_MAT_MUL_INV(src1, src2, dst, row, col, col2, i_inv1, i_inv2, o_inv, shift) \
	LUNA_CHECK_START() \
		LUNA_CHECK_ADDR(src1, LUNA_MAX(row*col, (row-1)*i_inv1+col), 0) \
		LUNA_CHECK_ADDR(src2, LUNA_MAX(col*col2, (col-1)*i_inv2+col2), 0) \
		LUNA_CHECK_ADDR(dst, LUNA_MAX(row*col2, (row-1)*o_inv+col2), 1) \
		LUNA_CHECK_LT("shift", shift, 64) \
		LUNA_CHECK_MAT_MUL_SIZE(row, col, col2, sizeof(src1[0])*8, sizeof(src2[0])*8, sizeof(dst[0])*8) \
	LUNA_CHECK_END()

#define LUNA_CHECK_PARAM_MAT_MUL_SPLIT(src1, src2, dst, row, col, col2, split_num, shift) \
	LUNA_CHECK_START() \
		LUNA_CHECK_ADDR(src1, row*col, 0) \
		LUNA_CHECK_ADDR(src2, col*col2, 0) \
		LUNA_CHECK_ADDR(dst, row*col2, 1) \
		LUNA_CHECK_LT("shift", shift, 64) \
		LUNA_CHECK_ALIGNED("col2", col2, split_num) \
		LUNA_CHECK_MAT_MUL_SIZE(row, col, col2/split_num, sizeof(src1[0])*8, sizeof(src2[0])*8, sizeof(dst[0])*8) \
	LUNA_CHECK_END()

#define LUNA_CHECK_PARAM_MAT_TRANS(src1, dst, row, col) \
	LUNA_CHECK_START() \
		LUNA_CHECK_ADDR(src1, row*col, 0) \
		LUNA_CHECK_ADDR(dst, row*col, 1) \
		LUNA_CHECK_MAT_TRANS_SIZE(row, col, sizeof(src1[0])*8, sizeof(dst[0])*8) \
	LUNA_CHECK_END()

#define LUNA_CHECK_PARAM_MAT_TRANS_SPLIT(src1, dst, row, col, split_num) \
	LUNA_CHECK_START() \
		LUNA_CHECK_ADDR(src1, row*col, 0) \
		LUNA_CHECK_ADDR(dst, row*col, 1) \
		LUNA_CHECK_ALIGNED("col", col, split_num) \
		LUNA_CHECK_MAT_TRANS_SIZE(row, col/split_num, sizeof(src1[0])*8, sizeof(dst[0])*8) \
	LUNA_CHECK_END()

#define LUNA_CHECK_PARAM_MAT_TRANS_INV(src1, dst, row, col, i_inv, o_inv) \
	LUNA_CHECK_START() \
		LUNA_CHECK_ADDR(src1, LUNA_MAX(row*col, (row-1)*i_inv+col), 0) \
		LUNA_CHECK_ADDR(dst, LUNA_MAX(row*col, (col-1)*o_inv+row), 1) \
		LUNA_CHECK_MAT_TRANS_SIZE(row, col, sizeof(src1[0])*8, sizeof(dst[0])*8) \
	LUNA_CHECK_END()

#define LUNA_CHECK_PARAM_MAT_TRANS_COL234_INV(src1, dst, row, col, i_inv, o_inv) \
	LUNA_CHECK_START() \
		LUNA_CHECK_ADDR(src1, LUNA_MAX(row*col, (row-1)*i_inv+col), 0) \
		LUNA_CHECK_ADDR(dst, LUNA_MAX(row*col, (col-1)*o_inv+row), 1) \
		LUNA_CHECK_MAT_TRANS_COL234_SIZE(row, col, sizeof(src1[0])*8, sizeof(dst[0])*8) \
	LUNA_CHECK_END()

#define LUNA_CHECK_PARAM_MAT_TRANS_3D(src, dst, in_shape, axis, n_dims) \
	LUNA_CHECK_START() \
		LUNA_CHECK_ADDR(src, (in_shape[0])*(in_shape[1])*(in_shape[2]), 0) \
		LUNA_CHECK_ADDR(dst, (in_shape[0])*(in_shape[1])*(in_shape[2]), 1) \
	LUNA_CHECK_END()

// fft
#define LUNA_CHECK_PARAM_CFFT(src, dst, shift, src_size, dst_size) \
	LUNA_CHECK_START() \
		LUNA_CHECK_ADDR(src, src_size, 0) \
		LUNA_CHECK_ADDR(dst, dst_size, 1) \
		LUNA_CHECK_ADDR(shift, 1, 1) \
	LUNA_CHECK_END()

#define LUNA_CHECK_PARAM_CFFT_2SRC1DST(src1, src2, dst, shift, src1_size, src2_size, dst_size) \
	LUNA_CHECK_START() \
		LUNA_CHECK_ADDR(src1, src1_size, 0) \
		LUNA_CHECK_ADDR(src2, src2_size, 0) \
		LUNA_CHECK_ADDR(dst, dst_size, 1) \
		LUNA_CHECK_ADDR(shift, 1, 1) \
	LUNA_CHECK_END()

#define LUNA_CHECK_PARAM_CFFT_1SRC2DST(src, dst1, dst2, shift, src_size, dst1_size, dst2_size) \
	LUNA_CHECK_START() \
		LUNA_CHECK_ADDR(src, src_size, 0) \
		LUNA_CHECK_ADDR(dst1, dst1_size, 1) \
		LUNA_CHECK_ADDR(dst2, dst2_size, 1) \
		LUNA_CHECK_ADDR(shift, 1, 1) \
	LUNA_CHECK_END()

//cnn
#define LUNA_CHECK_PARAM_CONV(pInput, pWeight, pBias, pOutput, conv_struct_) \
	LUNA_CHECK_START() \
		LUNA_CHECK_ADDR(pInput, conv_struct_->input_c*conv_struct_->input_h*conv_struct_->input_w, 0) \
		LUNA_CHECK_ADDR(pWeight, conv_struct_->output_c*conv_struct_->input_c*conv_struct_->weight_h*conv_struct_->weight_w, 0) \
		LUNA_CHECK_ADDR_BIAS(pBias, conv_struct_) \
		LUNA_CHECK_ADDR(pOutput, conv_struct_->output_c*conv_struct_->output_h*conv_struct_->output_w, 1) \
		LUNA_CHECK_CONV_SIZE(conv_struct_, sizeof(pInput[0])*8, sizeof(pOutput[0])*8) \
	LUNA_CHECK_END()

#define LUNA_CHECK_PARAM_DECONV(pInput, pWeight, pBias, pOutput, conv_struct_) \
	LUNA_CHECK_START() \
		LUNA_CHECK_ADDR(pInput, conv_struct_->input_c*conv_struct_->input_h*conv_struct_->input_w, 0) \
		LUNA_CHECK_ADDR(pWeight, conv_struct_->output_c*conv_struct_->input_c*conv_struct_->weight_h*conv_struct_->weight_w, 0) \
		LUNA_CHECK_ADDR_BIAS(pBias, conv_struct_) \
		LUNA_CHECK_ADDR(pOutput, conv_struct_->output_c*conv_struct_->output_h*conv_struct_->output_w, 1) \
		LUNA_CHECK_DECONV_SIZE(conv_struct_, sizeof(pInput[0])*8, sizeof(pOutput[0])*8) \
	LUNA_CHECK_END()

#define LUNA_CHECK_PARAM_DEPTHWISE(pInput, pWeight, pBias, pOutput, conv_struct_) \
	LUNA_CHECK_START() \
		LUNA_CHECK_ADDR(pInput, conv_struct_->input_c*conv_struct_->input_h*conv_struct_->input_w, 0) \
		LUNA_CHECK_ADDR(pWeight, conv_struct_->input_c*conv_struct_->weight_h*conv_struct_->weight_w, 0) \
		LUNA_CHECK_ADDR_BIAS(pBias, conv_struct_) \
		LUNA_CHECK_ADDR(pOutput, conv_struct_->output_c*conv_struct_->output_h*conv_struct_->output_w, 1) \
		LUNA_CHECK_DEPTHWISE_SIZE(conv_struct_, sizeof(pInput[0])*8, sizeof(pOutput[0])*8) \
	LUNA_CHECK_END()

#define LUNA_CHECK_PARAM_SPLIT_CONV(pInput, pWeight, pBias, pOutput, split_num, conv_struct_) \
	LUNA_CHECK_START() \
		LUNA_CHECK_ADDR(pInput, conv_struct_->input_c*conv_struct_->input_h*conv_struct_->input_w, 0) \
		LUNA_CHECK_ADDR(pWeight, conv_struct_->output_c*conv_struct_->input_c*conv_struct_->weight_h*conv_struct_->weight_w, 0) \
		LUNA_CHECK_ADDR_BIAS(pBias, conv_struct_) \
		LUNA_CHECK_ADDR(pOutput, conv_struct_->output_c*conv_struct_->output_h*conv_struct_->output_w, 1) \
		LUNA_CHECK_SPLIT_CONV_SIZE(conv_struct_, sizeof(pInput[0])*8, sizeof(pOutput[0])*8, split_num) \
	LUNA_CHECK_END()

#else 

// dma
#define LUNA_CHECK_DMA_CPY(chn, dst, src, size)
#define LUNA_CHECK_DMA_WAIT(chn)
// vector
#define LUNA_CHECK_PARAM_VEC_2SRC1DST(src1, src2, dst, src1_size, src2_size, dst_size, shift)
#define LUNA_CHECK_PARAM_VEC_1SRC1DST(src1, dst, src1_size, dst_size, shift)
#define LUNA_CHECK_PARAM_VEC_CMP_VV(src1, src2, dst, size, cmp_mode)
#define LUNA_CHECK_PARAM_VEC_CMP_VS(src1, dst, size, cmp_mode)
#define LUNA_CHECK_PARAM_VEC_DOT_32(src1, src2, dst, size, shift)
#define LUNA_CHECK_PARAM_VEC_DIV_32(src1, q_src1, src2, q_src2, dst, q_out, size) 
// complex
#define LUNA_CHECK_PARAM_CLX(src1, src2, dst, src1_size, src2_size, dst_size, shift) 
// matrix
#define LUNA_CHECK_PARAM_MAT_MUL(src1, src2, dst, row, col, col2, shift) 
#define LUNA_CHECK_PARAM_MAT_MUL_INV(src1, src2, dst, row, col, col2, i_inv1, i_inv2, o_inv, shift) 
#define LUNA_CHECK_PARAM_MAT_MUL_SPLIT(src1, src2, dst, row, col, col2, split_num, shift)
#define LUNA_CHECK_PARAM_MAT_TRANS(src1, dst, row, col)
#define LUNA_CHECK_PARAM_MAT_TRANS_SPLIT(src1, dst, row, col, split_num)
#define LUNA_CHECK_PARAM_MAT_TRANS_INV(src1, dst, row, col, i_inv, o_inv)
#define LUNA_CHECK_PARAM_MAT_TRANS_COL234_INV(src1, dst, row, col, i_inv, o_inv)
#define LUNA_CHECK_PARAM_MAT_TRANS_3D(src, dst, in_shape, axis, n_dims)
// fft
#define LUNA_CHECK_PARAM_CFFT(src, dst, shift, src_size, dst_size)
#define LUNA_CHECK_PARAM_CFFT_2SRC1DST(src1, src2, dst, shift, src1_size, src2_size, dst_size) 
#define LUNA_CHECK_PARAM_CFFT_1SRC2DST(src, dst1, dst2, shift, src_size, dst1_size, dst2_size)
//cnn
#define LUNA_CHECK_PARAM_CONV(pInput, pWeight, pBias, pOutput, conv_struct_)
#define LUNA_CHECK_PARAM_DECONV(pInput, pWeight, pBias, pOutput, conv_struct_)
#define LUNA_CHECK_PARAM_DEPTHWISE(pInput, pWeight, pBias, pOutput, conv_struct_) 

#endif 


#endif //__LUNA_CHECK_H_SIM__

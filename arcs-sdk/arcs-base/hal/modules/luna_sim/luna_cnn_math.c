/*
 * luna_cnn_math.c
 *
 *  Created on: Aug 24, 2017
 *      Author: dwwang
 */

#include "luna_sim/luna.h"
#include "luna_sim/luna_cnn_math.h"
#include "luna_sim/common/common.h"
#include "luna_sim/common/check.h"

#ifndef LUNA_SIM_CNN_TYPE
#define LUNA_SIM_CNN_TYPE
typedef enum
{
	E_LUNA_SIM_CNN_TYPE_CONV = 0,
	E_LUNA_SIM_CNN_TYPE_DECONV = 1,
	E_LUNA_SIM_CNN_TYPE_DEPTH = 2,
	E_LUNA_SIM_CNN_TYPE_MAXPOOL = 3,
	E_LUNA_SIM_CNN_TYPE_MEANPOOL = 4
}E_LUNA_SIM_CNN_TYPE;
#endif

#if defined(WIN32) || defined(linux)
#define MAKE_TMP_STACK_BUFFER_NEW()	\
	int8_t conv_tmp_buf[128 * 1024];	\
	int8_t buffer_weight[256 * 1024];	\
	memset(conv_tmp_buf, 0, sizeof(conv_tmp_buf)); \
	memset(buffer_weight, 0, sizeof(buffer_weight));
#else
#define MAKE_TMP_STACK_BUFFER_NEW()	\
	int8_t buffer_weight[32 * 1024];	\
	memset(buffer_weight, 0, sizeof(buffer_weight));
#endif

#define SPLIT_CNN_PARAM_CHECK() \
	uint32_t split_num = 1; \
	uint32_t tmp_in_h = conv_struct_->input_h; \
	uint32_t in_without_h = ((conv_struct_->input_c + 7) / 8 * 8) * \
			((conv_struct_->input_w + 8 * conv_struct_->stride_w - 1) / (8 * conv_struct_->stride_w) * (8 * conv_struct_->stride_w)); \
	while ((tmp_in_h * in_without_h > 65536) || ((conv_struct_->output_h % split_num) != 0)) \
	{ \
		split_num += 1; \
		tmp_in_h = (conv_struct_->output_h * conv_struct_->stride_h) / split_num + conv_struct_->weight_h - conv_struct_->stride_h; \
		if ((split_num > conv_struct_->input_h) || (split_num > conv_struct_->output_h)) \
		{ \
			break; \
		} \
	} \
	int32_t cal_var0 = 0; \
	while (conv_struct_->padding_h_down) \
	{ \
		cal_var0 = conv_struct_->input_h_after_padding - conv_struct_->weight_h + conv_struct_->stride_h; \
		if (cal_var0 % conv_struct_->stride_h) \
		{ \
			conv_struct_->padding_h_down -= 1; \
			conv_struct_->input_h_after_padding -= 1; \
		} \
		else \
		{ \
			break; \
		} \
	}


static void do_input_reshape(int8_t* src, int8_t* dst, conv_struct_t* pConv, int8_t pad_val)
{
	uint32_t  i, j, k;
	uint32_t h_reshape, w_reshape;
	uint32_t h = (pConv->input_h-1) * pConv->stride_h + 1;
	uint32_t w = pConv->input_w * pConv->stride_w;
	// uint32_t w = (pConv->input_w-1) * pConv->stride_w + 1;
	
	int8_t tmp[10 * 1024];
	int8_t* pSrc = (int8_t*)src;
	int8_t* pDst = (int8_t*)tmp;

	memset(tmp, 0, 10*1024);
	h_reshape = (pConv->stride_h > 1) ? 1 : 0;
	w_reshape = (pConv->stride_w > 1) ? 1 : 0;

	/* do input_reshape */
	for (k = 0; k < pConv->input_c; k++) {
		for (i = 0; i < h; i++)
		{
			if (h_reshape && 0 != (i % pConv->stride_h))
			{
				for (j = 0; j < w; j++)
					pDst[j] = 0;
			}
			else
			{
				for (j = 0; j < w; j++)
				{
					if (w_reshape) {
						if (j % pConv->stride_w == 0)
						{
							pDst[j] = pSrc[j / pConv->stride_w];
						}
						else
						{
							pDst[j] = 0;
						}
					}
					else
					{
						pDst[j] = pSrc[j];
					}
				}
				pSrc += pConv->input_w;
			}

			pDst += w;

		}
	}

	// memcpy(tmp, pDst, pConv->input_c * h * w);

	/* do input padding */
	int c_in = pConv->input_c;
	int h_in = h;
	int w_in = w;
	int cstep_in = h_in * w_in;
	if (pConv->padding_h_up || pConv->padding_h_down || pConv->padding_w_left || pConv->padding_w_right) {
		int q = 0;
		char v = pad_val;
		int pad_top = pConv->padding_h_up;
		int pad_bottom = pConv->padding_h_down;
		int pad_left = pConv->padding_w_left;
		int pad_right = pConv->padding_w_right;

		int pad_out_h = h_in + pad_top + pad_bottom;
		int pad_out_w = w_in + pad_left + pad_right;
		int pad_step_out = pad_out_h * pad_out_w;

		for (q = 0; q < c_in; q++)
		{
			char *inptr = (char *)tmp + cstep_in * q;
			char *outptr = (char *)dst+ pad_step_out * q;

			int y = 0;
			// fill top
			for (; y < pad_top; y++)
			{
				int x = 0;
				for (; x < pad_out_w; x++)
				{
					outptr[x] = v;
				}
				outptr += pad_out_w;
			}
			// fill center
			for (; y < (pad_top + h_in); y++)
			{
				int x = 0;
				for (; x < pad_left; x++)
				{
					outptr[x] = v;
				}
				for (; x < (pad_left + w_in); x++)
				{
					outptr[x] = inptr[x - pad_left];
				}
				for (; x < pad_out_w; x++)
				{
					outptr[x] = v;
				}
				inptr += w_in;
				outptr += pad_out_w;
			}
			// fill bottom
			for (; y < pad_out_h; y++)
			{
				int x = 0;
				for (; x < pad_out_w; x++)
				{
					outptr[x] = v;
				}
				outptr += pad_out_w;
			}
		}
	}
	else {
		memcpy(dst, tmp, c_in *cstep_in * sizeof(int8_t));
	}

	return;
}

static void do_input_reshape_without_padding(int8_t* src, int8_t* dst, conv_struct_t* pConv)
{
	uint32_t  i, j, k;
	uint32_t h_reshape, w_reshape;
	uint32_t h = (pConv->input_h-1) * pConv->stride_h + 1;
	uint32_t w = pConv->input_w * pConv->stride_w;
	// uint32_t w = (pConv->input_w-1) * pConv->stride_w + 1;
	int8_t* pSrc = (int8_t*)src;
	int8_t* pDst = (int8_t*)dst;

	h_reshape = (pConv->stride_h > 1) ? 1 : 0;
	w_reshape = (pConv->stride_w > 1) ? 1 : 0;

	/* do input_reshape */
	for (k = 0; k < pConv->input_c; k++) {
		for (i = 0; i < h; i++)
		{
			if (h_reshape && 0 != (i % pConv->stride_h))
			{
				for (j = 0; j < w; j++)
					pDst[j] = 0;
			}
			else
			{
				for (j = 0; j < w; j++)
				{
					if (w_reshape) {
						if (j % pConv->stride_w == 0)
						{
							pDst[j] = pSrc[j / pConv->stride_w];
						}
						else
						{
							pDst[j] = 0;
						}
					}
					else
					{
						pDst[j] = pSrc[j];
					}
				}
				pSrc += pConv->input_w;
			}

			pDst += w;

		}
	}

#if !(defined(WIN32) || defined(linux))
	memcpy(src, dst, pConv->input_c * h * w);
#endif

	return;
}

static int op_padding_int8(const conv_struct_t *conv_struct_, int8_t *pInput, int8_t *pOutput, int8_t pad_val)
{
	int q = 0;
	const conv_struct_t *param = conv_struct_;
	int c_in = param->input_c;
	int h_in = param->input_h;
	int w_in = param->input_w;
	int cstep_in = h_in * w_in;


	if (param->padding_h_up || param->padding_h_down || param->padding_w_left || param->padding_w_right)
	{
		char v = pad_val;
		int pad_top = param->padding_h_up;
		int pad_bottom = param->padding_h_down;
		int pad_left = param->padding_w_left;
		int pad_right = param->padding_w_right;

		int pad_out_h = h_in + pad_top + pad_bottom;
		int pad_out_w = w_in + pad_left + pad_right;
		int pad_step_out = pad_out_h * pad_out_w;

		for (q = 0; q < c_in; q++)
		{
			char *inptr = (char *)pInput + cstep_in * q;
			char *outptr = (char *)pOutput+ pad_step_out * q;

			int y = 0;
			// fill top
			for (; y < pad_top; y++)
			{
				int x = 0;
				for (; x < pad_out_w; x++)
				{
					outptr[x] = v;
				}
				outptr += pad_out_w;
			}
			// fill center
			for (; y < (pad_top + h_in); y++)
			{
				int x = 0;
				for (; x < pad_left; x++)
				{
					outptr[x] = v;
				}
				for (; x < (pad_left + w_in); x++)
				{
					outptr[x] = inptr[x - pad_left];
				}
				for (; x < pad_out_w; x++)
				{
					outptr[x] = v;
				}
				inptr += w_in;
				outptr += pad_out_w;
			}
			// fill bottom
			for (; y < pad_out_h; y++)
			{
				int x = 0;
				for (; x < pad_out_w; x++)
				{
					outptr[x] = v;
				}
				outptr += pad_out_w;
			}
		}

	}
	else
	{
		memcpy(pOutput, pInput, c_in *cstep_in * sizeof(int8_t));
	}
	return 0;
}

/**
 * weight_q3_int8
 * Convert 4 bits of weight data to 8 bits
 **/
static int32_t weight_q3_int8(int8_t* pWeightIn, int8_t* pWeightOut, conv_struct_t* conv_struct_)
{
	if (NULL == pWeightIn || NULL == pWeightOut || NULL == conv_struct_)
	{
		return -1;
	}

	int32_t i;
	int32_t weight_size = conv_struct_->input_c * conv_struct_->weight_h * conv_struct_->weight_w * conv_struct_->output_c;
	int32_t weight_4bit_len = (weight_size + 1) / 2;

	for (i = 0; i < weight_4bit_len; i++)
	{
		if ((pWeightIn[i] & 0x0F) >= 0x08)//negative value
		{
			pWeightOut[2 * i] = pWeightIn[i] | 0xF0;
		}
		else
		{
			pWeightOut[2 * i] = pWeightIn[i] & 0x0F;
		}

		if ((2 * i + 1) < weight_size)
		{
			if (((pWeightIn[i] >> 4) & 0x0F) >= 0x08)
			{
				pWeightOut[2 * i + 1] = (pWeightIn[i] >> 4) | 0xF0;
			}
			else
			{
				pWeightOut[2 * i + 1] = (pWeightIn[i] >> 4) & 0x0F;
			}
		}
	}

	return 0;
}

/**
 * recover weight fot conv
 */
static void dereshape_weight_for_conv(int8_t *input_weight_T, int8_t *input_weight, conv_struct_t *conv_struct_, uint32_t in_bits)
{
	const int32_t c_aligned = 8;
	int32_t i, j, k, m, num, pos;
	int32_t kernel_w = conv_struct_->weight_w;
	int32_t kernel_h = conv_struct_->weight_h;
	int32_t kernel_c = conv_struct_->input_c;
	int32_t kernel_num = conv_struct_->output_c;
	int8_t* pIn = input_weight_T;
	int8_t* pOut = input_weight;

	num = 0;
	pos = 0;
	while(num < kernel_num)
	{
		for(i = 0; i < kernel_h; i++)
		{
			for(j = 0; j < kernel_w; j++)
			{
				k = 0;
				while(k < kernel_c)
				{
					// kernel num+0
					for(m = 0; m < 8; m++)
					{
						if((k + m) < kernel_c)
						{
							if (8 == in_bits)
							{
								pOut[num * kernel_w * kernel_h * kernel_c + kernel_w * kernel_h * (k + m) + kernel_w * i + j] = pIn[pos];
							}
							else if (4 == in_bits)
							{
								if (pos % 2 == 0)
								{
									if ((pIn[pos>>1] & 0x0F) >= 0x08)//negative value
										pOut[num * kernel_w * kernel_h * kernel_c + kernel_w * kernel_h * (k + m) + kernel_w * i + j] = pIn[pos>>1]|0xF0;
									else
										pOut[num * kernel_w * kernel_h * kernel_c + kernel_w * kernel_h * (k + m) + kernel_w * i + j] = pIn[pos>>1]&0x0F;
								}
								else
								{
									if (((pIn[pos>>1]>>4) & 0x0F) >= 0x08)//negative value
										pOut[num * kernel_w * kernel_h * kernel_c + kernel_w * kernel_h * (k + m) + kernel_w * i + j] = (pIn[pos>>1]>>4)|0xF0;
									else
										pOut[num * kernel_w * kernel_h * kernel_c + kernel_w * kernel_h * (k + m) + kernel_w * i + j] = (pIn[pos>>1]>>4)&0x0F;
								}
							}
						}

						pos++;
					}

					// kernel num+1
					if((num+1) < kernel_num)
					{
						for(m = 0; m < 8; m++)
						{
							if((k + m) < kernel_c)
							{
								if (8 == in_bits)
								{
									pOut[(num+1) * kernel_w * kernel_h * kernel_c + kernel_w * kernel_h * (k + m) + kernel_w * i + j] = pIn[pos];
								}
								else if (4 == in_bits)
								{
									if (pos % 2 == 0)
									{
										if ((pIn[pos>>1] & 0x0F) >= 0x08)//negative value
											pOut[(num+1) * kernel_w * kernel_h * kernel_c + kernel_w * kernel_h * (k + m) + kernel_w * i + j] = pIn[pos>>1]|0xF0;
										else
											pOut[(num+1) * kernel_w * kernel_h * kernel_c + kernel_w * kernel_h * (k + m) + kernel_w * i + j] = pIn[pos>>1]&0x0F;
									}
									else
									{
										if (((pIn[pos>>1]>>4) & 0x0F) >= 0x08)//negative value
											pOut[(num+1) * kernel_w * kernel_h * kernel_c + kernel_w * kernel_h * (k + m) + kernel_w * i + j] = (pIn[pos>>1]>>4)|0xF0;
										else
											pOut[(num+1) * kernel_w * kernel_h * kernel_c + kernel_w * kernel_h * (k + m) + kernel_w * i + j] = (pIn[pos>>1]>>4)&0x0F;
									}
								}
							}

							pos++;
						}
					}
					else
					{
						for(m = 0; m < 8; m++)
						{
							pos++;
						}
					}

					k += 8;
				}
			}
		}
		num += 2;
	}

	return;
}

/**
 * recover weight for depthwise
 */
static void dereshape_weight_for_depthwise(int8_t *input_weight_T, int8_t *input_weight, conv_struct_t *conv_struct_, uint32_t in_bits)
{
	const int32_t c_aligned = 8;
	int32_t i, j, k, depth, pos;
	int32_t kernel_c = conv_struct_->input_c;
	int32_t kernel_h = conv_struct_->weight_h;
	int32_t kernel_w = conv_struct_->weight_w;
	int8_t* pIn = input_weight_T;
	int8_t* pOut = input_weight;

	pos = 0;
	depth = 0;
	while(depth < kernel_c)
	{
		for(i = 0; i < kernel_h; i++)
		{
			for(j = 0; j < kernel_w; j++)
			{
				for(k = 0; k < c_aligned; k++)
				{
					if((depth+k) < kernel_c)
					{
						if (8 == in_bits)
						{
							pOut[kernel_w * kernel_h * (depth+k) + kernel_w * i + j] = pIn[pos];
						}
						else if (4 == in_bits)
						{
							if (0 == pos % 2)
							{
								if ((pIn[pos>>1] & 0x0F) >= 0x08)	//negative value
									pOut[kernel_w * kernel_h * (depth+k) + kernel_w * i + j] = pIn[pos >> 1] | 0xF0;
								else
									pOut[kernel_w * kernel_h * (depth+k) + kernel_w * i + j] = pIn[pos >> 1] & 0x0F;
							}
							else
							{
								if (((pIn[pos>>1]>>4) & 0x0F) >= 0x08)	//negative value
									pOut[kernel_w * kernel_h * (depth+k) + kernel_w * i + j] = (pIn[pos >> 1]>>4) | 0xF0;
								else
									pOut[kernel_w * kernel_h * (depth+k) + kernel_w * i + j] = (pIn[pos >> 1]>>4) & 0x0F;
							}
						}
					}

					pos++;
				}
			}
		}
		depth += c_aligned;
	}

	return;
}


static void parse_cnn_static_para(luna_cnn_static_para_t *conv_para_, conv_struct_t *conv_struct_)
{
	memset(conv_struct_, 0, sizeof(conv_struct_t));
	conv_struct_->input_c = conv_para_->in_c;
	conv_struct_->input_h = conv_para_->in_h;
	conv_struct_->input_w = conv_para_->in_w;
	conv_struct_->padding_w_left = conv_para_->pad_wl;
	conv_struct_->padding_w_right = conv_para_->pad_wr;
	conv_struct_->padding_h_up = conv_para_->pad_ht;
	conv_struct_->padding_h_down = conv_para_->pad_hb;
	conv_struct_->weight_w = conv_para_->k_w;
	conv_struct_->weight_h = conv_para_->k_h;
	conv_struct_->stride_w = conv_para_->s_w;
	conv_struct_->stride_h = conv_para_->s_h;
	conv_struct_->output_c = conv_para_->ou_c;
	conv_struct_->output_w = conv_para_->ou_w;
	conv_struct_->output_h = conv_para_->ou_h;
	conv_struct_->dilation_w = conv_para_->dilation_w;
	conv_struct_->dilation_h = conv_para_->dilation_h;
	conv_struct_->out_padding_w = conv_para_->out_padding_w;
	conv_struct_->out_padding_h = conv_para_->out_padding_h;
	conv_struct_->is_per_chn = conv_para_->is_per_chn;
	conv_struct_->is_bias = conv_para_->is_bias;
	conv_struct_->reserved = conv_para_->r4_judge_sw;	//relux_val
	uint32_t shift_h = (conv_para_->r23_shift >> 16);
	if (0 == (shift_h & (1 << 15)))
	{
		conv_struct_->activation_type = NO_ACTIVE;
		conv_struct_->positive_shift_type = (shift_h & (1 << 6)) >> 6;
		conv_struct_->positive_shift_value = (shift_h & 0x3F);
		conv_struct_->negative_shift_type = 0;
		conv_struct_->negative_shift_value = 0;
	}
	else if ((63 << 8) == (shift_h & (63 << 8)))
	{
		conv_struct_->activation_type = RELU;
		conv_struct_->positive_shift_type = (shift_h & (1 << 6)) >> 6;
		conv_struct_->positive_shift_value = (shift_h & 0x3F);
		conv_struct_->negative_shift_type = 0;
		conv_struct_->negative_shift_value = 0;
	}
	else
	{
		conv_struct_->activation_type = PRELU;
		conv_struct_->positive_shift_type = (shift_h & (1 << 6)) >> 6;
		conv_struct_->positive_shift_value = (shift_h & 0x3F);
		conv_struct_->negative_shift_type = conv_struct_->positive_shift_type;
		conv_struct_->negative_shift_value = ((shift_h & (0x3F << 8)) >> 8) - conv_struct_->positive_shift_value;
	}
	
	if ((1 << 11) & conv_para_->r23_shift)
	{
		conv_struct_->activation_type = RELUx;
	}

	conv_struct_->weight_bits = conv_para_->weight_bits;
	conv_struct_->ou_bits = conv_para_->ou_bits;
}

static int32_t luna_conv_calculate_new(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, void *p_out, conv_struct_t *conv_struct_, int32_t ou_bits, int32_t conv_type)
{
	// LUNA_CHECK_PARAM_CONV(p_in, p_weight, p_bias, p_out, conv_struct_);
	int32_t ret = 0;
	{
		MAKE_TMP_STACK_BUFFER_NEW()
		int32_t in_c, in_w, in_h, k_w, k_h, ou_c, ou_w, ou_h, s_h, s_w;
		int32_t i_in_c, i_in_w, i_in_h, i_k_w, i_k_h, i_ou_c, i_ou_w, i_ou_h;
		int32_t pad_ht = conv_struct_->padding_h_up;
		int32_t pad_hb = conv_struct_->padding_h_down;
		int32_t pad_wl = conv_struct_->padding_w_left;
		int32_t pad_wr = conv_struct_->padding_w_right;
		int32_t is_bias = conv_struct_->is_bias;
		int32_t is_per_chn = conv_struct_->is_per_chn;
		int32_t activation_type = conv_struct_->activation_type;
		int32_t pos_shift_type = conv_struct_->positive_shift_type;
		int32_t pos_shift_val = conv_struct_->positive_shift_value;
		int32_t neg_shift_type = conv_struct_->negative_shift_type;
		int32_t neg_shift_val = conv_struct_->negative_shift_value;
		int32_t w_bits = conv_struct_->weight_bits;
		int32_t dia_w = (conv_struct_->dilation_w == 0) ? 1 : conv_struct_->dilation_w;	//[1, 2, 4, 8]
		int32_t dia_h = (conv_struct_->dilation_h == 0) ? 1 : conv_struct_->dilation_h;
		int8_t  *p_tmp_out_int8 = NULL;
		int32_t *p_tmp_out_int32 = NULL;
		int8_t relux_val = conv_struct_->reserved;

		in_c = conv_struct_->input_c;
		in_h = conv_struct_->input_h;
		in_w = conv_struct_->input_w;
		ou_c = conv_struct_->output_c;
		ou_h = conv_struct_->output_h;
		ou_w = conv_struct_->output_w;
		k_h = conv_struct_->weight_h;
		k_w = conv_struct_->weight_w;
		s_h = conv_struct_->stride_h;
		s_w = conv_struct_->stride_w;

		if (E_LUNA_SIM_CNN_TYPE_DEPTH == conv_type)
		{
#if defined(WIN32) || defined(linux)
			memcpy(conv_tmp_buf, p_in, in_c * in_h * in_w);
			p_in = conv_tmp_buf;
#endif
			in_c = 1;
			dereshape_weight_for_depthwise(p_weight, buffer_weight, conv_struct_, w_bits);
		}
		else if (E_LUNA_SIM_CNN_TYPE_DECONV == conv_type)
		{
#if defined(WIN32) || defined(linux)
			do_input_reshape_without_padding((int8_t *)p_in, (int8_t *)conv_tmp_buf, conv_struct_);
			p_in = conv_tmp_buf;
#else
			do_input_reshape_without_padding((int8_t *)p_in, (int8_t *)p_out, conv_struct_);
#endif
			in_h = (in_h - 1) * s_h + 1;
			in_w = in_w * s_w;
			s_h = 1;
			s_w = 1;
			dereshape_weight_for_conv(p_weight, buffer_weight, conv_struct_, w_bits);
		}
		else
		{
#if defined(WIN32) || defined(linux)
			memcpy(conv_tmp_buf, p_in, in_c * in_h * in_w);
			p_in = conv_tmp_buf;
#endif
			dereshape_weight_for_conv(p_weight, buffer_weight, conv_struct_, w_bits);
		}

		// output channels
		for (i_ou_c = 0; i_ou_c < ou_c; i_ou_c++)
		{
			if (8 == ou_bits)
				p_tmp_out_int8 = (int8_t *)p_out + i_ou_c * (ou_w * ou_h);
			else
				p_tmp_out_int32 = (int32_t *)p_out + i_ou_c * (ou_w * ou_h);

			for (i_ou_h = 0; i_ou_h < ou_h; i_ou_h++)
			{
				for (i_ou_w = 0; i_ou_w < ou_w; i_ou_w++)
				{
					int32_t sum = 0;
					const int8_t* kptr = buffer_weight + (((k_w * k_h) * in_c) * i_ou_c);

					//input channels
					for (i_in_c = 0; i_in_c < in_c; i_in_c++)
					{
						const int8_t *data = p_in + ((in_w * in_h) * i_in_c);
						if (E_LUNA_SIM_CNN_TYPE_DEPTH == conv_type)
						{
							data = p_in + ((in_w * in_h) * i_ou_c);
						}

						//conv on each channel
						for (i_k_h = 0; i_k_h < k_h; i_k_h++)
						// for (i_k_h = 0; i_k_h < (dia_h * (k_h - 1) + 1); i_k_h += dia_h)
						{
							for (i_k_w = 0; i_k_w < k_w; i_k_w++)
							// for (i_k_w = 0; i_k_w < (dia_w * (k_w - 1) + 1); i_k_w += dia_w)
							{
								int h_input = i_ou_h * s_h + (dia_h * i_k_h) - pad_ht;
								int w_input = i_ou_w * s_w + (dia_w * i_k_w) - pad_wl;

								if (h_input >= 0 && h_input < in_h && w_input >= 0 && w_input < in_w)
								{
									int8_t weight = kptr[i_k_h * k_w + i_k_w];
									int8_t value = data[h_input * in_w + w_input];
									sum += value * weight;
								}
							}
						}

						kptr += k_w * k_h;
					}

					// add bias
					if (is_bias) {
						int32_t b_oft = (i_ou_c << 1) + (is_per_chn * i_ou_c >> 1);
						uint16_t *tmp_bias = (uint16_t *)p_bias;
						int32_t bias_val = (int32_t)(((uint32_t)tmp_bias[b_oft + 1] << 16) | (uint32_t)tmp_bias[b_oft]);
						int32_t tmp_sum = sum + bias_val;
						if (sum >= 0 && bias_val >= 0 && tmp_sum < 0) {
							tmp_sum = LUNA_INT32_MAX;
						}
						if (sum < 0 && bias_val < 0 && tmp_sum >= 0) {
							tmp_sum = LUNA_INT32_MIN;
						}
						sum = tmp_sum;
					}

					if (is_per_chn)
					{
						int32_t s_oft = (is_bias * ((i_ou_c >> 1) + 1) << 3) + i_ou_c;
						int8_t *tmp_shift = (int8_t *)p_bias;
						pos_shift_val = tmp_shift[s_oft];
					}

					// activation+shift
					switch (activation_type) {
					case RELU:
					case RELUx:
					{
						sum = (sum >= 0) ? sum : 0;
						if (ShiftType_FloorX05 == pos_shift_type) {
							sum = shfit_floor_x05_int32(sum, pos_shift_val);
						}
						else {
							sum = sum >> pos_shift_val;
						}
					}
					break;
					case PRELU:
					{
						if (sum >= 0) {
							if (ShiftType_FloorX05 == neg_shift_type) {
								sum = shfit_floor_x05_int32(sum, pos_shift_val);
							}
							else {
								sum = sum >> pos_shift_val;
							}
						}
						else {
							if (ShiftType_FloorX05 == neg_shift_type)
							{
								sum = shfit_floor_x05_int32(sum, pos_shift_val + neg_shift_val);
							}
							else
							{
								sum = sum >> (pos_shift_val + neg_shift_val);
							}
						}
					}
					break;
					case NO_ACTIVE:
					default:
						if (ShiftType_FloorX05 == pos_shift_type) {
							sum = shfit_floor_x05_int32(sum, pos_shift_val);
						}
						else {
							sum = sum >> pos_shift_val;
						}
						break;
					}

					if (8 == ou_bits)
					{
						p_tmp_out_int8[i_ou_h * ou_w + i_ou_w] = luna_saturate_q31_to_q7(sum);
						if (RELUx == activation_type)
						{
							if (p_tmp_out_int8[i_ou_h * ou_w + i_ou_w] > relux_val)
							{
								p_tmp_out_int8[i_ou_h * ou_w + i_ou_w] = relux_val;
							}
						}
					}
					else
					{
						p_tmp_out_int32[i_ou_h * ou_w + i_ou_w] = sum;
					}
				}
			}
		}
	}
	return ret;
}

static int32_t luna_pooling_calculate(const int8_t *p_in, void *p_out, conv_struct_t *conv_struct_, int32_t ou_bits, int32_t conv_type)
{
	// LUNA_CHECK_PARAM_CONV(p_in, p_weight, p_bias, p_out, conv_struct_);
	int32_t ret = 0;
	{
		MAKE_TMP_STACK_BUFFER_NEW()
		int32_t in_c, in_w, in_h, k_w, k_h, ou_c, ou_w, ou_h, s_h, s_w;
		int32_t i_in_c, i_in_w, i_in_h, i_k_w, i_k_h, i_ou_c, i_ou_w, i_ou_h;
		int32_t pad_ht = conv_struct_->padding_h_up;
		int32_t pad_hb = conv_struct_->padding_h_down;
		int32_t pad_wl = conv_struct_->padding_w_left;
		int32_t pad_wr = conv_struct_->padding_w_right;
		int32_t is_bias = conv_struct_->is_bias;
		int32_t activation_type = conv_struct_->activation_type;
		int32_t pos_shift_type = conv_struct_->positive_shift_type;
		int32_t pos_shift_val = conv_struct_->positive_shift_value;
		int32_t neg_shift_type = conv_struct_->negative_shift_type;
		int32_t neg_shift_val = conv_struct_->negative_shift_value;
		int8_t  *p_tmp_out_int8 = NULL;
		int32_t *p_tmp_out_int32 = NULL;

		in_c = conv_struct_->input_c;
		in_h = conv_struct_->input_h;
		in_w = conv_struct_->input_w;
		ou_c = conv_struct_->output_c;
		ou_h = conv_struct_->output_h;
		ou_w = conv_struct_->output_w;
		k_h = conv_struct_->weight_h;
		k_w = conv_struct_->weight_w;
		s_h = conv_struct_->stride_h;
		s_w = conv_struct_->stride_w;

#if defined(WIN32) || defined(linux)
		memcpy(conv_tmp_buf, p_in, in_c * in_h * in_w);
		p_in = conv_tmp_buf;
#endif
		// output channels
		for (i_ou_c = 0; i_ou_c < ou_c; i_ou_c++)
		{
			if (8 == ou_bits)
				p_tmp_out_int8 = (int8_t *)p_out + i_ou_c * (ou_w * ou_h);
			else
				p_tmp_out_int32 = (int32_t *)p_out + i_ou_c * (ou_w * ou_h);

			for (i_ou_h = 0; i_ou_h < ou_h; i_ou_h++)
			{
				for (i_ou_w = 0; i_ou_w < ou_w; i_ou_w++)
				{
					int32_t result_val = 0x80000000;
					const int8_t *data = p_in + ((in_w * in_h) * i_ou_c);

					if (E_LUNA_SIM_CNN_TYPE_MEANPOOL == conv_type)
					{
						result_val = 0;
					}

					//conv on each channel
					for (i_k_h = 0; i_k_h < k_h; i_k_h++)
					{
						for (i_k_w = 0; i_k_w < k_w; i_k_w++)
						{
							int h_input = i_ou_h * s_h + i_k_h - pad_ht;
							int w_input = i_ou_w * s_w + i_k_w - pad_wl;

							if (h_input >= 0 && h_input < in_h && w_input >= 0 && w_input < in_w)
							{
								if (E_LUNA_SIM_CNN_TYPE_MEANPOOL == conv_type)
								{
									result_val += data[h_input * in_w + w_input];
								}
								else
								{
									int8_t value = data[h_input * in_w + w_input];
									if (value > result_val)
									{
										result_val = value;
									}
								}
							}
						}
					}

					if (E_LUNA_SIM_CNN_TYPE_MEANPOOL == conv_type)
					{
						// activation+shift NO_ACTIVE
						if (ShiftType_FloorX05 == pos_shift_type) {
							result_val = shfit_floor_x05_int32(result_val, pos_shift_val);
						}
						else {
							result_val = result_val >> pos_shift_val;
						}
					}
					

					if (8 == ou_bits)
						p_tmp_out_int8[i_ou_h * ou_w + i_ou_w] = luna_saturate_q31_to_q7(result_val);
					else
						p_tmp_out_int32[i_ou_h * ou_w + i_ou_w] = result_val;
				}
			}
		}
	}
	return ret;
}


int32_t LUNA_API_SIM(luna_conv2d_i8i4o8)(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int8_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	conv_struct_t conv_struct_;
	parse_cnn_static_para(conv_para_, &conv_struct_);
	return luna_conv_calculate_new(p_in, p_weight, p_bias, (void *)p_out, &conv_struct_, 8, E_LUNA_SIM_CNN_TYPE_CONV);
}

int32_t LUNA_API_SIM(luna_conv2d_i8i4o32)(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int32_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	conv_struct_t conv_struct_;
	parse_cnn_static_para(conv_para_, &conv_struct_);
	return luna_conv_calculate_new(p_in, p_weight, p_bias, (void *)p_out, &conv_struct_, 32, E_LUNA_SIM_CNN_TYPE_CONV);
}

int32_t LUNA_API_SIM(luna_conv2d_i8i8o8)(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int8_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	conv_struct_t conv_struct_;
	parse_cnn_static_para(conv_para_, &conv_struct_);
	return luna_conv_calculate_new(p_in, p_weight, p_bias, (void *)p_out, &conv_struct_, 8, E_LUNA_SIM_CNN_TYPE_CONV);
}

int32_t LUNA_API_SIM(luna_conv2d_i8i8o32)(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int32_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	conv_struct_t conv_struct_;
	parse_cnn_static_para(conv_para_, &conv_struct_);
	return luna_conv_calculate_new(p_in, p_weight, p_bias, (void *)p_out, &conv_struct_, 32, E_LUNA_SIM_CNN_TYPE_CONV);
}

int32_t LUNA_API_SIM(luna_depthwise2d_i8i4o8)(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int8_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	conv_struct_t conv_struct_;
	parse_cnn_static_para(conv_para_, &conv_struct_);
	return luna_conv_calculate_new(p_in, p_weight, p_bias, (void *)p_out, &conv_struct_, 8, E_LUNA_SIM_CNN_TYPE_DEPTH);
}

int32_t LUNA_API_SIM(luna_depthwise2d_i8i4o32)(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int32_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	conv_struct_t conv_struct_;
	parse_cnn_static_para(conv_para_, &conv_struct_);
	return luna_conv_calculate_new(p_in, p_weight, p_bias, (void *)p_out, &conv_struct_, 32, E_LUNA_SIM_CNN_TYPE_DEPTH);
}

int32_t LUNA_API_SIM(luna_depthwise2d_i8i8o8)(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int8_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	conv_struct_t conv_struct_;
	parse_cnn_static_para(conv_para_, &conv_struct_);
	return luna_conv_calculate_new(p_in, p_weight, p_bias, (void *)p_out, &conv_struct_, 8, E_LUNA_SIM_CNN_TYPE_DEPTH);
}

int32_t LUNA_API_SIM(luna_depthwise2d_i8i8o32)(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int32_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	conv_struct_t conv_struct_;
	parse_cnn_static_para(conv_para_, &conv_struct_);
	return luna_conv_calculate_new(p_in, p_weight, p_bias, (void *)p_out, &conv_struct_, 32, E_LUNA_SIM_CNN_TYPE_DEPTH);
}

int32_t LUNA_API_SIM(luna_deconv2d_i8i4o8)(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int8_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	conv_struct_t conv_struct_;
	parse_cnn_static_para(conv_para_, &conv_struct_);
	return luna_conv_calculate_new(p_in, p_weight, p_bias, (void *)p_out, &conv_struct_, 8, E_LUNA_SIM_CNN_TYPE_DECONV);
}

int32_t LUNA_API_SIM(luna_deconv2d_i8i4o32)(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int32_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	conv_struct_t conv_struct_;
	parse_cnn_static_para(conv_para_, &conv_struct_);
	return luna_conv_calculate_new(p_in, p_weight, p_bias, (void *)p_out, &conv_struct_, 32, E_LUNA_SIM_CNN_TYPE_DECONV);
}

int32_t LUNA_API_SIM(luna_deconv2d_i8i8o8)(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int8_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	conv_struct_t conv_struct_;
	parse_cnn_static_para(conv_para_, &conv_struct_);
	return luna_conv_calculate_new(p_in, p_weight, p_bias, (void *)p_out, &conv_struct_, 8, E_LUNA_SIM_CNN_TYPE_DECONV);
}

int32_t LUNA_API_SIM(luna_deconv2d_i8i8o32)(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int32_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	conv_struct_t conv_struct_;
	parse_cnn_static_para(conv_para_, &conv_struct_);
	return luna_conv_calculate_new(p_in, p_weight, p_bias, (void *)p_out, &conv_struct_, 32, E_LUNA_SIM_CNN_TYPE_DECONV);
}

int32_t LUNA_API_SIM(luna_max_pooling2d_i8o8)(const int8_t *p_in, int8_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	conv_struct_t conv_struct_;
	parse_cnn_static_para(conv_para_, &conv_struct_);
	return luna_pooling_calculate(p_in, (void *)p_out, &conv_struct_, 8, E_LUNA_SIM_CNN_TYPE_MAXPOOL);
}

int32_t LUNA_API_SIM(luna_mean_pooling2d_i8o8)(const int8_t *p_in, int8_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	conv_struct_t conv_struct_;
	parse_cnn_static_para(conv_para_, &conv_struct_);
	return luna_pooling_calculate(p_in, (void *)p_out, &conv_struct_, 8, E_LUNA_SIM_CNN_TYPE_MEANPOOL);
}

int32_t LUNA_API_SIM(luna_mean_pooling2d_i8o32)(const int8_t *p_in, int32_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	conv_struct_t conv_struct_;
	parse_cnn_static_para(conv_para_, &conv_struct_);
	return luna_pooling_calculate(p_in, (void *)p_out, &conv_struct_, 32, E_LUNA_SIM_CNN_TYPE_MEANPOOL);
}


#define CONV1D_PARAM_CHECK(weight_en)	\
	{	\
		if (weight_en)	\
		{	\
			if ((conv_para_->in_h != 1) || (conv_para_->ou_h != 1) || (conv_para_->k_h != 1) || (conv_para_->s_h != 1))	\
			{	\
				printf("[%s][%d]invalid conv1d params, in_h:%d, ou_h:%d, k_h:%d, s_h:%d \r\n", conv_para_->in_h, conv_para_->ou_h, conv_para_->k_h, conv_para_->s_h);	\
				return -1;	\
			}	\
		}	\
		else \
		{	\
			if ((conv_para_->in_h != 1) || (conv_para_->ou_h != 1) || (conv_para_->s_h != 1))	\
			{	\
				printf("[%s][%d]invalid conv1d params, in_h:%d, ou_h:%d, s_h:%d \r\n", conv_para_->in_h, conv_para_->ou_h, conv_para_->s_h);	\
				return -1;	\
			}	\
		}	\
	}

int32_t LUNA_API_SIM(luna_conv1d_i8i4o8)(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int8_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	CONV1D_PARAM_CHECK(1)
	conv_struct_t conv_struct_;
	parse_cnn_static_para(conv_para_, &conv_struct_);
	return luna_conv_calculate_new(p_in, p_weight, p_bias, (void *)p_out, &conv_struct_, 8, E_LUNA_SIM_CNN_TYPE_CONV);
}

int32_t LUNA_API_SIM(luna_conv1d_i8i4o32)(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int32_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	CONV1D_PARAM_CHECK(1)
	conv_struct_t conv_struct_;
	parse_cnn_static_para(conv_para_, &conv_struct_);
	return luna_conv_calculate_new(p_in, p_weight, p_bias, (void *)p_out, &conv_struct_, 32, E_LUNA_SIM_CNN_TYPE_CONV);
}

int32_t LUNA_API_SIM(luna_conv1d_i8i8o8)(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int8_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	CONV1D_PARAM_CHECK(1)
	conv_struct_t conv_struct_;
	parse_cnn_static_para(conv_para_, &conv_struct_);
	return luna_conv_calculate_new(p_in, p_weight, p_bias, (void *)p_out, &conv_struct_, 8, E_LUNA_SIM_CNN_TYPE_CONV);
}

int32_t LUNA_API_SIM(luna_conv1d_i8i8o32)(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int32_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	CONV1D_PARAM_CHECK(1)
	conv_struct_t conv_struct_;
	parse_cnn_static_para(conv_para_, &conv_struct_);
	return luna_conv_calculate_new(p_in, p_weight, p_bias, (void *)p_out, &conv_struct_, 32, E_LUNA_SIM_CNN_TYPE_CONV);
}

int32_t LUNA_API_SIM(luna_depthwise1d_i8i4o8)(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int8_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	CONV1D_PARAM_CHECK(1)
	conv_struct_t conv_struct_;
	parse_cnn_static_para(conv_para_, &conv_struct_);
	return luna_conv_calculate_new(p_in, p_weight, p_bias, (void *)p_out, &conv_struct_, 8, E_LUNA_SIM_CNN_TYPE_DEPTH);
}

int32_t LUNA_API_SIM(luna_depthwise1d_i8i4o32)(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int32_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	CONV1D_PARAM_CHECK(1)
	conv_struct_t conv_struct_;
	parse_cnn_static_para(conv_para_, &conv_struct_);
	return luna_conv_calculate_new(p_in, p_weight, p_bias, (void *)p_out, &conv_struct_, 32, E_LUNA_SIM_CNN_TYPE_DEPTH);
}

int32_t LUNA_API_SIM(luna_depthwise1d_i8i8o8)(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int8_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	CONV1D_PARAM_CHECK(1)
	conv_struct_t conv_struct_;
	parse_cnn_static_para(conv_para_, &conv_struct_);
	return luna_conv_calculate_new(p_in, p_weight, p_bias, (void *)p_out, &conv_struct_, 8, E_LUNA_SIM_CNN_TYPE_DEPTH);
}

int32_t LUNA_API_SIM(luna_depthwise1d_i8i8o32)(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int32_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	CONV1D_PARAM_CHECK(1)
	conv_struct_t conv_struct_;
	parse_cnn_static_para(conv_para_, &conv_struct_);
	return luna_conv_calculate_new(p_in, p_weight, p_bias, (void *)p_out, &conv_struct_, 32, E_LUNA_SIM_CNN_TYPE_DEPTH);
}

int32_t LUNA_API_SIM(luna_deconv1d_i8i4o8)(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int8_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	CONV1D_PARAM_CHECK(1)
	conv_struct_t conv_struct_;
	parse_cnn_static_para(conv_para_, &conv_struct_);
	return luna_conv_calculate_new(p_in, p_weight, p_bias, (void *)p_out, &conv_struct_, 8, E_LUNA_SIM_CNN_TYPE_DECONV);
}

int32_t LUNA_API_SIM(luna_deconv1d_i8i4o32)(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int32_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	CONV1D_PARAM_CHECK(1)
	conv_struct_t conv_struct_;
	parse_cnn_static_para(conv_para_, &conv_struct_);
	return luna_conv_calculate_new(p_in, p_weight, p_bias, (void *)p_out, &conv_struct_, 32, E_LUNA_SIM_CNN_TYPE_DECONV);
}

int32_t LUNA_API_SIM(luna_deconv1d_i8i8o8)(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int8_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	CONV1D_PARAM_CHECK(1)
	conv_struct_t conv_struct_;
	parse_cnn_static_para(conv_para_, &conv_struct_);
	return luna_conv_calculate_new(p_in, p_weight, p_bias, (void *)p_out, &conv_struct_, 8, E_LUNA_SIM_CNN_TYPE_DECONV);
}

int32_t LUNA_API_SIM(luna_deconv1d_i8i8o32)(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int32_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	CONV1D_PARAM_CHECK(1)
	conv_struct_t conv_struct_;
	parse_cnn_static_para(conv_para_, &conv_struct_);
	return luna_conv_calculate_new(p_in, p_weight, p_bias, (void *)p_out, &conv_struct_, 32, E_LUNA_SIM_CNN_TYPE_DECONV);
}

int32_t LUNA_API_SIM(luna_max_pooling1d_i8o8)(const int8_t *p_in, int8_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	CONV1D_PARAM_CHECK(0)
	conv_struct_t conv_struct_;
	parse_cnn_static_para(conv_para_, &conv_struct_);
	return luna_pooling_calculate(p_in, (void *)p_out, &conv_struct_, 8, E_LUNA_SIM_CNN_TYPE_MAXPOOL);
}

int32_t LUNA_API_SIM(luna_mean_pooling1d_i8o8)(const int8_t *p_in, int8_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	CONV1D_PARAM_CHECK(0)
	conv_struct_t conv_struct_;
	parse_cnn_static_para(conv_para_, &conv_struct_);
	return luna_pooling_calculate(p_in, (void *)p_out, &conv_struct_, 8, E_LUNA_SIM_CNN_TYPE_MEANPOOL);
}

int32_t LUNA_API_SIM(luna_mean_pooling1d_i8o32)(const int8_t *p_in, int32_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	CONV1D_PARAM_CHECK(0)
	conv_struct_t conv_struct_;
	parse_cnn_static_para(conv_para_, &conv_struct_);
	return luna_pooling_calculate(p_in, (void *)p_out, &conv_struct_, 32, E_LUNA_SIM_CNN_TYPE_MEANPOOL);
}


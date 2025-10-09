#include <math.h>

#include "luna/luna_cnn_tools.h"

typedef struct luna_cnn_infmap_param
{
	uint32_t c_map_cnt0;
    uint32_t xc_map_cnt1;
	union
	{
		uint32_t kw_map_cnt2;
		uint32_t kh_map_cnt2;
	};
	union
	{
		uint32_t xkw_map_cnt3;
		uint32_t xkh_map_cnt3;
	};
	union
	{
		uint32_t kh_map_cnt4;
		uint32_t kw_map_cnt4;
	};
    union
	{
		uint32_t xkh_map_cnt5;
		uint32_t xkw_map_cnt5;
	};
    uint32_t w_map_cnt6;
    uint32_t xw_map_cnt7;
    uint32_t h_map_cnt8;
    uint32_t xh_map_cnt9;
    uint32_t g_map_cnt10;
    uint32_t xg_map_cnt11;
	union
	{
		uint32_t g_map_cnt10_last;
		uint32_t gs_map_cnt12;
	};	
	uint32_t xgs_map_cnt13;

    uint32_t c_stride_inf;
    uint32_t c_stride_ker;
	uint32_t kw_stride_inf;
    uint32_t kw_stride_ker;
    uint32_t kh_stride_inf;
    uint32_t kh_stride_ker;
    uint32_t w_stride_inf;
    uint32_t w_stride_ker;
    uint32_t h_stride_inf;
    uint32_t h_stride_ker;
    uint32_t g_stride_inf;
    uint32_t g_stride_ker;
	uint32_t gs_stride_inf;
	uint32_t gs_stride_ker;
	uint32_t inf_base_addr_1st;
	uint32_t inf_base_addr;
	uint32_t ker_base_addr;
}luna_cnn_infmap_param_t;

typedef struct luna_cnn_pe_core_cfg
{
	uint16_t ow_first_num;
	uint16_t ow_last_num;
	uint16_t ow_1st_num;
	uint16_t ow_minus_1st;
	uint16_t ow_1st_msk;
	uint16_t ow_msk;
	uint16_t ow_num;
	// uint16_t stride_w_addr;
	// int32_t w_modify;
	// uint32_t master_xw_cnt7;
	int16_t kw_padd_l_ext;
	int16_t kw_padd_r_ext;
	int16_t w_padd_l_ext;
	int16_t w_padd_r_ext;
	int16_t w_padd_mask;
	int16_t kernel_w_nor;
	int16_t paddin_wl_nor;
	int16_t paddin_wr_nor;
	int32_t pad_hb_real_1st;
	int32_t pad_hb_real;
	int32_t pad_hb_real_last;
	int32_t pad_hb_real_1st_last;
}luna_cnn_pe_core_cfg_t;

typedef struct luna_cnn_split_kernel_struct
{
	int32_t split_k_num;
	int32_t split_k_n;
	int32_t split_k_n_last;
	int32_t k_addr_inc;
	int32_t k_o_addr_inc;
	int32_t b_addr_inc;
}luna_cnn_split_kernel_t;

typedef struct luna_cnn_split_input_struct
{
	int32_t split_in_num;
	int32_t split_in_h_1st_last;
	int32_t split_in_h_1st;
	int32_t split_in_h_last;
	int32_t split_in_h;
	int32_t split_ou_h;
	int32_t i_addr_inc_1st;
	int32_t i_addr_inc;
	int32_t o_addr_inc;

	int32_t pad_ht_1st;
	int32_t pad_ht;
	int32_t split_ou_h_1st;
	int32_t split_ou_h_last;
	int32_t o_addr_inc_1st;
}luna_cnn_split_input_t;

/**
 * shift must be 2^
 **/
static int32_t luna_ceil_sim(int32_t x, int32_t shift)
{
	if (x & ~(0xFFFFFFFF << shift)) {
		return (x >> shift) + 1;
	}
	else {
		return (x >> shift);
	}
}

static int32_t luna_cnn_pe_core_config(conv_struct_t *conv_st, luna_cnn_split_input_t *split_in_t, luna_cnn_pe_core_cfg_t *pe_core, int32_t cnn_layer_type)
{
	conv_struct_t *p_conv = (conv_struct_t *)conv_st;

	int32_t in_w = p_conv->input_w;
	int32_t ou_w = p_conv->output_w;
	int32_t k_w = p_conv->weight_w;
	int32_t k_h = p_conv->weight_h;
	int32_t s_w = p_conv->stride_w;
	int32_t s_h = p_conv->stride_h;
	int32_t pad_wl = p_conv->padding_w_left;
	int32_t pad_wr = p_conv->padding_w_right;
	int32_t pad_ht = p_conv->padding_h_up;
	int32_t pad_hb = p_conv->padding_h_down;
	int32_t dia_w = (p_conv->dilation_w == 0) ? 1 : p_conv->dilation_w;
	int32_t dia_h = (p_conv->dilation_h == 0) ? 1 : p_conv->dilation_h;

	int32_t s_w_d = s_w;
	int32_t s_h_d = s_h;

	int32_t cnn_layer = cnn_layer_type;

	if (LUNA_DECONV == cnn_layer || LUNA_DECONV1D == cnn_layer)
	{
		s_w = 1;
		s_h = 1;
	}

	uint32_t s_w_x4 = (s_w << 2);
	uint32_t log2n_s_w = s_w >> 1;
	uint32_t mask_case = luna_ceil_sim((pad_wl & (s_w_x4 - 1)), log2n_s_w) & 3;
	switch (mask_case)
	{
		case 0:
		{
			pe_core->ow_1st_num = 1;
			pe_core->ow_1st_msk = 8;
			switch (ou_w)
			{
				case 1:
					pe_core->ow_msk = 8;
				break;
				default:
					pe_core->ow_msk = 0;
				break;
			}
		}
		break;
		case 1:
		{
			pe_core->ow_1st_num = 2;
			pe_core->ow_1st_msk = 12;
			switch (ou_w)
			{
				case 1:
					pe_core->ow_msk = 4;
				break;
				case 2:
					pe_core->ow_msk = 12;
				break;
				default:
					pe_core->ow_msk = 0;
				break;
			}
		}
		break;
		case 2:
		{
			pe_core->ow_1st_num = 3;
			pe_core->ow_1st_msk = 14;
			switch (ou_w)
			{
				case 1:
					pe_core->ow_msk = 2;
				break;
				case 2:
					pe_core->ow_msk = 6;
				break;
				case 3:
					pe_core->ow_msk = 14;
				break;
				default:
					pe_core->ow_msk = 0;
				break;
			}
		}
		break;
		case 3:
		{
			pe_core->ow_1st_num = 4;
			pe_core->ow_1st_msk = 15;
			switch (ou_w)
			{
				case 1:
					pe_core->ow_msk = 1;
				break;
				case 2:
					pe_core->ow_msk = 3;
				break;
				case 3:
					pe_core->ow_msk = 7;
				break;
				case 4:
					pe_core->ow_msk = 15;
				break;
				default:
					pe_core->ow_msk = 0;
				break;
			}
		}
		break;
		default:
		break;
	}

	if (ou_w < pe_core->ow_1st_num)
	{
		pe_core->ow_first_num = ou_w;
	}
	else
	{
		pe_core->ow_first_num = pe_core->ow_1st_num;
	}
	pe_core->ow_minus_1st = ou_w - pe_core->ow_first_num;
	if (0 == (pe_core->ow_minus_1st & 0x3))
	{
		pe_core->ow_last_num = 4;
	}
	else
	{
		pe_core->ow_last_num = pe_core->ow_minus_1st & 0x3;
	}
	pe_core->ow_num = luna_ceil_sim(pe_core->ow_minus_1st, 2);

	uint16_t iw_remainder = 0;
	uint16_t kw_remainder = 0;
	if (LUNA_DECONV == cnn_layer || LUNA_DECONV1D == cnn_layer)
	{
		pe_core->kw_padd_l_ext = (s_w - (pad_wl & (s_w - 1))) & (s_w - 1);
		int32_t cond1 = luna_ceil_sim((k_w + pe_core->kw_padd_l_ext), (log2n_s_w + 2));
		int32_t cond2 = luna_ceil_sim((pad_wl + pe_core->kw_padd_l_ext), (log2n_s_w + 2));
		if (cond1 == cond2)
		{
			pe_core->w_padd_l_ext = luna_ceil_sim(pad_wl, (log2n_s_w + 2)) - 1;
			pe_core->w_padd_mask = 0;
		}
		else
		{
			pe_core->w_padd_l_ext = luna_ceil_sim(pad_wl, (log2n_s_w + 2));
			pe_core->w_padd_mask = (((pad_wl + pe_core->kw_padd_l_ext) & (s_w_x4 - 1)) != 0);
		}
		pe_core->paddin_wl_nor = pad_wl + pe_core->kw_padd_l_ext;
		pe_core->kernel_w_nor = k_w + pe_core->kw_padd_l_ext;
		if (0 == ((in_w * s_w_d) & (3)))
		{
			iw_remainder = s_w_x4;
		}
		else
		{
			iw_remainder = (in_w * s_w_d) & (3);
		}
		if (0 == (pe_core->kernel_w_nor & (s_w_x4 - 1)))
		{
			kw_remainder = s_w_x4;
		}
		else
		{
			kw_remainder = pe_core->kernel_w_nor & (s_w_x4 - 1);
		}
		pe_core->paddin_wr_nor = ou_w * s_w + k_w - s_w - (in_w * s_w_d) - pad_wl;
		if ((iw_remainder + pe_core->paddin_wr_nor) <= kw_remainder)
		{
			pe_core->w_padd_r_ext = 0;
		}
		else
		{
			pe_core->w_padd_r_ext = luna_ceil_sim((iw_remainder + pe_core->paddin_wr_nor - kw_remainder), (log2n_s_w + 2));
		}

		pe_core->pad_hb_real_1st = 0;
		pe_core->pad_hb_real = 0;
		pe_core->pad_hb_real_last = pad_hb;
		pe_core->pad_hb_real_1st_last = pad_hb;
	}
	else
	{
		pe_core->kw_padd_l_ext = (s_w - (pad_wl & (s_w - 1))) & (s_w - 1);
		int32_t cond1 = luna_ceil_sim(((k_w - 1) * dia_w + 1 + pe_core->kw_padd_l_ext), (log2n_s_w + 2));
		int32_t cond2 = luna_ceil_sim((pad_wl + pe_core->kw_padd_l_ext), (log2n_s_w + 2));
		if (cond1 == cond2)
		{
			pe_core->w_padd_l_ext = luna_ceil_sim(pad_wl, (log2n_s_w + 2)) - 1;
			pe_core->w_padd_mask = 0;
		}
		else
		{
			pe_core->w_padd_l_ext = luna_ceil_sim(pad_wl, (log2n_s_w + 2));
			pe_core->w_padd_mask = (((pad_wl + pe_core->kw_padd_l_ext) & (s_w_x4 - 1)) != 0);
		}
		pe_core->paddin_wl_nor = pad_wl + pe_core->kw_padd_l_ext;
		pe_core->kernel_w_nor = (k_w - 1) * dia_w + 1 + pe_core->kw_padd_l_ext;
		if (0 == (in_w & (s_w_x4 - 1)))
		{
			iw_remainder = s_w_x4;
		}
		else
		{
			iw_remainder = in_w & (s_w_x4 - 1);
		}
		if (0 == (pe_core->kernel_w_nor & (s_w_x4 - 1)))
		{
			kw_remainder = s_w_x4;
		}
		else
		{
			kw_remainder = pe_core->kernel_w_nor & (s_w_x4 - 1);
		}
		pe_core->paddin_wr_nor = ou_w * s_w + (k_w - 1) * dia_w + 1 - s_w - in_w - pad_wl;
		if ((iw_remainder + pe_core->paddin_wr_nor) <= kw_remainder)
		{
			pe_core->w_padd_r_ext = 0;
		}
		else
		{
			pe_core->w_padd_r_ext = luna_ceil_sim((iw_remainder + pe_core->paddin_wr_nor - kw_remainder), (log2n_s_w + 2));
		}

		int32_t split_ou_h = split_in_t->split_ou_h;
		int32_t in_h_mid = split_in_t->split_in_h;
		int32_t in_h_1st = split_in_t->split_in_h_1st;
		int32_t in_h_last = split_in_t->split_in_h_last;
		int32_t in_h_1st_last = split_in_t->split_in_h_1st_last;
		pe_core->pad_hb_real_1st = (split_ou_h - 1) * s_h + (k_h - 1) * dia_h + 1 - pad_ht - in_h_1st;
		pe_core->pad_hb_real = (split_ou_h - 1) * s_h + (k_h - 1) * dia_h + 1 - in_h_mid;
		pe_core->pad_hb_real_last = (split_ou_h - 1) * s_h + (k_h - 1) * dia_h + 1 - in_h_last;
		pe_core->pad_hb_real_1st_last = (split_ou_h - 1) * s_h + (k_h - 1) * dia_h + 1 - pad_ht - in_h_1st_last;
	}

	return 0;
}

static int32_t luna_cnn_shift_config(conv_struct_t *conv_st, int32_t *shift)
{
	int32_t active_type = conv_st->activation_type;
	int32_t pos_shift_val = conv_st->positive_shift_value;
	int32_t pos_shift_type = conv_st->positive_shift_type;
	int32_t neg_shift_val = conv_st->negative_shift_value;
	int32_t neg_shift_type = conv_st->negative_shift_type;
	int32_t is_per_chn = conv_st->is_per_chn;
	uint32_t com_shift = 0x8004;

	if (is_per_chn)
	{
		pos_shift_val = 0;
		com_shift |= (is_per_chn << 7);
	}

	uint32_t shift_type = 0;
	if (RELU == active_type)
	{
		shift_type = pos_shift_type << 6;
		com_shift |= ((pos_shift_val + shift_type + (63 << 8) + (1 << 15)) << 16);
	}
	else if (PRELU == active_type)
	{
		shift_type = neg_shift_type << 6;
		com_shift |= (((pos_shift_val + shift_type) + ((pos_shift_val + neg_shift_val + shift_type) << 8) + (1 << 15)) << 16);
	}
	else if (RELUx == active_type)
	{
		shift_type = pos_shift_type << 6;
		com_shift |= (1 << 11);
		com_shift |= ((pos_shift_val + shift_type + (63 << 8) + (1 << 15)) << 16);
	}
	else if (NO_ACTIVE == active_type)
	{
		shift_type = pos_shift_type << 6;
		com_shift |= (((pos_shift_val + shift_type) + ((pos_shift_val + shift_type) << 8)) << 16);
	}
	*shift = com_shift;

	return 0;
}

static int32_t luna_cnn_split_kernel_config(conv_struct_t *conv_st, luna_cnn_split_kernel_t *split_k_t, int32_t cnn_layer_type)
{
	int32_t is_bias =  conv_st->is_bias;
	int32_t is_per_chn = conv_st->is_per_chn;
	int32_t ou_bits = conv_st->ou_bits;
	int32_t w_bits = conv_st->weight_bits;
	// split kernel
	uint32_t split_k_num = 1;
	uint32_t k_n = conv_st->output_c;
	uint32_t k_without_n = (luna_ceil_sim(conv_st->input_c, 3) << 3) * conv_st->weight_w * conv_st->weight_h;

	if ((LUNA_CONV != cnn_layer_type) && (LUNA_DECONV != cnn_layer_type) && (LUNA_CONV1D != cnn_layer_type) && (LUNA_DECONV1D != cnn_layer_type))
	{
		return 0;
	}

	int32_t target_elements = CONV_WEIGHT_CONDITION / k_without_n;
	int32_t remainder = CONV_WEIGHT_CONDITION % k_without_n;
	if ((luna_ceil_sim(k_n, 1) << 1) * k_without_n > CONV_WEIGHT_CONDITION)
	{
		// k_n = (0 == remainder) ? target_elements : ((target_elements + 1) & ~1);
		k_n = target_elements & (~1);
		split_k_num = (conv_st->output_c + (k_n - 1)) / k_n;
	}
	// while (k_n * k_without_n > CONV_WEIGHT_CONDITION || ((k_n % 2) && (split_k_num > 1)))
	// {
	// 	split_k_num++;
	// 	k_n = (conv_st->output_c + (split_k_num - 1)) / split_k_num;
	// 	if (split_k_num >= conv_st->output_c)
	// 	{
	// 		// printf("[%s][%d]kernel split invalid, split_num:%d, kernel_num:%d \r\n", __func__, __LINE__, split_k_num, conv_st->output_c);
	// 		// return -1;
	// 		break;
	// 	}
	// }

	split_k_t->split_k_num = split_k_num;
	split_k_t->split_k_n = k_n;
	split_k_t->split_k_n_last = conv_st->output_c - k_n * (split_k_num - 1);
	split_k_t->k_addr_inc = (luna_ceil_sim(conv_st->input_c, 3) << 3) * conv_st->weight_w * conv_st->weight_h * k_n * w_bits >> 3;
	split_k_t->k_o_addr_inc = k_n * conv_st->output_h * conv_st->output_w * (ou_bits >> 3);
	if (is_bias && is_per_chn)
	{
		split_k_t->b_addr_inc = k_n * 5;	// bias is 32bit, per_chn is 8bit
	}
	else if (is_bias)
	{
		split_k_t->b_addr_inc = k_n * 4;
	}
	else if (is_per_chn)
	{
		split_k_t->b_addr_inc = k_n;
	}
	else
	{
		split_k_t->b_addr_inc = 0;
	}
	

	return 0;
}

static int32_t luna_conv_split_input_config(conv_struct_t *conv_st, luna_cnn_split_input_t *split_in_t, int32_t cnn_layer_type)
{
	conv_struct_t *p_conv = (conv_struct_t *)conv_st;

	int32_t in_c = p_conv->input_c;
	int32_t in_h = p_conv->input_h;
	int32_t in_w = p_conv->input_w;
	int32_t ou_w = p_conv->output_w;
	int32_t ou_h = p_conv->output_h;
	int32_t k_h = p_conv->weight_h;
	int32_t k_w = p_conv->weight_w;
	int32_t s_h = p_conv->stride_h;
	int32_t s_w = p_conv->stride_w;
	int32_t pad_ht = p_conv->padding_h_up;
	int32_t pad_hb = p_conv->padding_h_down;
	int32_t dia_w = (p_conv->dilation_w == 0) ? 1 : p_conv->dilation_w;
	int32_t dia_h = (p_conv->dilation_h == 0) ? 1 : p_conv->dilation_h;
	int32_t ou_bits = p_conv->ou_bits;
	uint32_t log2n_s_w = s_w >> 1;

	uint32_t split_in_num = 1;
	uint32_t tmp_in_h = in_h;
	uint32_t in_without_h = (luna_ceil_sim(in_c, 3) << 3) * (luna_ceil_sim(in_w, 2 + log2n_s_w) << (2 + log2n_s_w));

	if (cnn_layer_type < LUNA_CONV1D)	//CNN1D no support split
	{
		while ((tmp_in_h * in_without_h > CONV_IN_CONDITION) || ((ou_h % split_in_num) != 0))
		{
			split_in_num += 1;
			tmp_in_h = (ou_h * s_h) / split_in_num + ((k_h - 1) * dia_h + 1) - s_h;
			if ((split_in_num >= in_h) || (split_in_num >= ou_h))
			{
				break;
			}
		}

		if (split_in_num > 1)
		{
			int32_t cal_var0 = 0;
			while (pad_hb)
			{
				cal_var0 = in_h + pad_ht + pad_hb - ((k_h - 1) * dia_h + 1) + s_h;
				if (cal_var0 % s_h)
				{
					pad_hb = pad_hb - 1;
				}
				else
				{
					break;
				}
			}
			p_conv->padding_h_down = pad_hb;

			/////////////////check split condition////////////////
			cal_var0 = in_h + pad_ht + pad_hb - ((k_h - 1) * dia_h + 1) + s_h;
			int32_t cal_var1 = floor(cal_var0 / s_h);
			int32_t cal_var2 = floor(cal_var1 / split_in_num);
			int32_t cal_var3 = cal_var2 * s_h + 1 - pad_ht;
			int32_t cal_var4 = cal_var2 * s_h + 1 - pad_hb;
			if ((cal_var0 % s_h) != 0 || (cal_var1 % split_in_num) != 0 || (cal_var3 <= 0) || (cal_var4 <= 0) || (tmp_in_h * in_without_h > CONV_IN_CONDITION))
			{
				printf("[%s][%d]input split invalid, split_num:%d \r\n", __func__, __LINE__, split_in_num);
				return -1;
			}
			/////////////////////////////////////////////////////
		}
	}

	split_in_t->split_in_num = split_in_num;
	split_in_t->split_in_h = ((ou_h * s_h) / split_in_num) + ((k_h - 1) * dia_h + 1) - s_h;
	split_in_t->split_in_h_1st_last = in_h;
	split_in_t->split_in_h_1st = split_in_t->split_in_h - pad_ht;
	split_in_t->split_in_h_last = split_in_t->split_in_h - pad_hb;
	split_in_t->split_ou_h = ou_h / split_in_num;
	split_in_t->i_addr_inc_1st = in_w * (split_in_t->split_in_h - ((k_h - 1) * dia_h + 1) + s_h - pad_ht);
	split_in_t->i_addr_inc = in_w * (split_in_t->split_in_h - ((k_h - 1) * dia_h + 1) + s_h);
	split_in_t->o_addr_inc = ou_w * split_in_t->split_ou_h * (ou_bits >> 3);

	return 0;
}

static int32_t luna_deconv_split_input_config(conv_struct_t *conv_st, luna_cnn_split_input_t *split_in_t, int32_t cnn_layer_type)
{
	conv_struct_t *p_conv = (conv_struct_t *)conv_st;

	int32_t in_c = p_conv->input_c;
	int32_t in_h = p_conv->input_h;
	int32_t in_w = p_conv->input_w;
	int32_t ou_w = p_conv->output_w;
	int32_t ou_h = p_conv->output_h;
	int32_t k_h = p_conv->weight_h;
	int32_t k_w = p_conv->weight_w;
	int32_t s_h = p_conv->stride_h;
	int32_t s_w = p_conv->stride_w;
	int32_t pad_ht = p_conv->padding_h_up;
	int32_t pad_hb = p_conv->padding_h_down;
	int32_t ou_bits = p_conv->ou_bits;
	uint32_t log2n_s_w = s_w >> 1;
	uint32_t log2n_s_h = s_h >> 1;

	uint32_t split_in_num = 1;
	uint32_t tmp_in_h = in_h;
	uint32_t in_without_h = (luna_ceil_sim(in_c, 3) << 3) * (luna_ceil_sim(in_w, 2 + log2n_s_w) << (2 + log2n_s_w));

	int32_t overlap_num = luna_ceil_sim((k_h - 1), log2n_s_h);;

	if (cnn_layer_type < LUNA_CONV1D)	//CNN1D no support split
	{
		while (((tmp_in_h + overlap_num) * in_without_h > CONV_IN_CONDITION) || ((in_h % split_in_num) != 0))
		{
			split_in_num += 1;
			tmp_in_h = in_h / split_in_num;
			if ((split_in_num >= in_h) || (split_in_num >= ou_h))
			{
				break;
			}
		}

		if (split_in_num > 1)
		{
			/////////////////check split condition////////////////
			int32_t condition_1 = ((in_h % split_in_num) == 0);
			int32_t condition_2 = (((in_h / split_in_num - 1) * s_h + 1) >= k_h);
			if (!condition_1 || !condition_2 || ((1 == s_h) && (1 == s_w)) || ((tmp_in_h + overlap_num) * in_without_h > CONV_IN_CONDITION))
			{
				printf("[%s][%d]input split invalid, split_num:%d \r\n", __func__, __LINE__, split_in_num);
				return -1;
			}
			/////////////////////////////////////////////////////
		}
	}

	int32_t s_h_d = s_h;
	int32_t s_w_d = s_w;
	s_h = 1;
	s_w = 1;

	split_in_t->pad_ht_1st = pad_ht;
	if (0 == ((k_h - 1) & (s_h_d - 1)))
	{
		split_in_t->pad_ht = s_h_d - 1;
	}
	else
	{
		split_in_t->pad_ht = ((k_h - 1) & (s_h_d - 1)) - 1;
	}
	// if (1 == k_h)
	// {
	// 	overlap_num = 0;
	// 	split_in_t->pad_ht = s_h_d - 1;
	// }
	// else if (1 == (k_h - s_h_d) || k_h <= s_h_d)
	// {
	// 	overlap_num = 1;
	// 	split_in_t->pad_ht = k_h - 2;
	// }
	// else if (1 == (k_h - 2 * s_h_d) || k_h <= (2 * s_h_d))
	// {
	// 	overlap_num = 2;
	// 	split_in_t->pad_ht = k_h - s_h_d - 2;
	// }
	// else if (1 == (k_h - 3 * s_h_d) || k_h <= (3 * s_h_d))
	// {
	// 	overlap_num = 3;
	// 	split_in_t->pad_ht = k_h - 2 * s_h_d - 2;
	// }
	// else if (1 == (k_h - 4 * s_h_d) || k_h <= (4 * s_h_d))
	// {
	// 	overlap_num = 4;
	// 	split_in_t->pad_ht = k_h - 3 * s_h_d - 2;
	// }
	// else if (1 == (k_h - 5 * s_h_d) || k_h <= (5 * s_h_d))
	// {
	// 	overlap_num = 5;
	// 	split_in_t->pad_ht = k_h - 4 * s_h_d - 2;
	// }
	// else
	// {
	// 	overlap_num = 6;
	// 	split_in_t->pad_ht = k_h - 5 * s_h_d - 2;
	// }

	split_in_t->split_in_num = split_in_num;
	split_in_t->split_in_h_1st_last = in_h;
	split_in_t->split_in_h_1st = in_h / split_in_num;
	split_in_t->split_in_h = split_in_t->split_in_h_1st + overlap_num;
	split_in_t->split_in_h_last = 0;
	split_in_t->i_addr_inc_1st = in_w * (split_in_t->split_in_h_1st - overlap_num);
	split_in_t->i_addr_inc = in_w * split_in_t->split_in_h_1st;

	split_in_t->split_ou_h_1st = (split_in_t->split_in_h_1st - 1) * s_h_d + 1 + split_in_t->pad_ht_1st - k_h + 1;
	split_in_t->split_ou_h = (split_in_t->split_in_h - 1) * s_h_d + 1 + split_in_t->pad_ht - k_h + 1;
	split_in_t->split_ou_h_last = (split_in_t->split_in_h - 1) * s_h_d + 1 + split_in_t->pad_ht + pad_hb - k_h + 1;
	split_in_t->o_addr_inc_1st = ou_w * split_in_t->split_ou_h_1st * (ou_bits >> 3);
	split_in_t->o_addr_inc = ou_w * split_in_t->split_ou_h * (ou_bits >> 3);

	return 0;
}

static int32_t luna_conv_infmap_para_cfg(conv_struct_t *conv_st, luna_cnn_pe_core_cfg_t *pe_core, luna_cnn_infmap_param_t *inf_param, int32_t k_n, int32_t k_n_last, int32_t split_ou_h)
{
	conv_struct_t *p_conv = conv_st;
	luna_cnn_infmap_param_t *p_para = inf_param;
	int32_t dia_w = (p_conv->dilation_w == 0) ? 1 : p_conv->dilation_w;
	int32_t dia_h = (p_conv->dilation_h == 0) ? 1 : p_conv->dilation_h;
	uint32_t log2n_s_w = p_conv->stride_w >> 1;
	uint32_t log2n_dia_w = (dia_w == 8) ? 3 : (dia_w >> 1);

	p_para->c_map_cnt0 = luna_ceil_sim(p_conv->input_c, 3);
    p_para->xc_map_cnt1 = 0;
    p_para->kw_map_cnt2 = (p_conv->weight_w - 1) * dia_w + 1 + pe_core->kw_padd_l_ext;
    p_para->xkw_map_cnt3 = 3;
    p_para->kh_map_cnt4 = (p_conv->weight_h - 1) * dia_h + 1;
	p_para->xkh_map_cnt5 = 1;
    p_para->w_map_cnt6 = luna_ceil_sim(p_conv->output_w - 1 - luna_ceil_sim(p_conv->padding_w_left, log2n_s_w), 2) + 1 + pe_core->w_padd_l_ext;
    p_para->xw_map_cnt7 = 0;
    p_para->h_map_cnt8 = split_ou_h;
    p_para->xh_map_cnt9 = 16;
    p_para->g_map_cnt10 = luna_ceil_sim(k_n, 1) + 1;
	p_para->g_map_cnt10_last = luna_ceil_sim(k_n_last, 1) + 1;
    p_para->xg_map_cnt11 = 0;
    p_para->c_stride_inf = 1;
    p_para->c_stride_ker = 1;
	p_para->kw_stride_inf = luna_ceil_sim(p_conv->input_c, 3);
    p_para->kw_stride_ker = luna_ceil_sim(p_conv->input_c, 3);
    p_para->kh_stride_inf = luna_ceil_sim(p_conv->input_c, 3) * luna_ceil_sim(p_conv->input_w, 2 + log2n_s_w) * p_conv->stride_w;
    p_para->kh_stride_ker = luna_ceil_sim(p_conv->input_c, 3) * p_conv->weight_w;
    p_para->w_stride_inf = luna_ceil_sim(p_conv->input_c, 3) * p_conv->stride_w;
    p_para->w_stride_ker = 0;
    p_para->h_stride_inf = luna_ceil_sim(p_conv->input_c, 3) * luna_ceil_sim(p_conv->input_w, 2 + log2n_s_w) * p_conv->stride_w * p_conv->stride_h;
    p_para->h_stride_ker = 0;
    p_para->g_stride_inf = 0;
    p_para->g_stride_ker = p_conv->weight_w * p_conv->weight_h * luna_ceil_sim(p_conv->input_c, 3);

	if ((0 == pe_core->w_padd_l_ext) && (0 == p_conv->padding_h_up))
	{
		p_para->inf_base_addr_1st = 0;
	}
	else
	{
		p_para->inf_base_addr_1st = (uint32_t)(1 << 32) - pe_core->w_padd_l_ext * luna_ceil_sim(p_conv->input_c, 3) * p_conv->stride_w - p_conv->padding_h_up * p_para->kh_stride_inf;
	}
	if (0 == pe_core->w_padd_l_ext)
	{
		p_para->inf_base_addr = 0;
	}
	else
	{
		p_para->inf_base_addr = (uint32_t)(1 << 32) - pe_core->w_padd_l_ext * luna_ceil_sim(p_conv->input_c, 3) * p_conv->stride_w;
	}

	if (0 == pe_core->kw_padd_l_ext)
	{
		p_para->ker_base_addr = 0;
	}
	else
	{
		p_para->ker_base_addr = (uint32_t)(1 << 32) - luna_ceil_sim(pe_core->kw_padd_l_ext, log2n_dia_w) * luna_ceil_sim(p_conv->input_c, 3);
	}

	return 0;
}

static int32_t luna_depthwise_infmap_para_cfg(conv_struct_t *conv_st, luna_cnn_pe_core_cfg_t *pe_core, luna_cnn_infmap_param_t *inf_param, int32_t split_ou_h)
{
	conv_struct_t* p_conv = conv_st;
	luna_cnn_infmap_param_t *p_para = inf_param;
	uint32_t log2n_s_w = p_conv->stride_w >> 1;

	p_para->c_map_cnt0 = 0;
    p_para->xc_map_cnt1 = 0;
    p_para->kh_map_cnt2 = p_conv->weight_h;
    p_para->xkh_map_cnt3 = 1;
    p_para->kw_map_cnt4 = p_conv->weight_w + pe_core->kw_padd_l_ext;
    p_para->xkw_map_cnt5 = 3;
    p_para->w_map_cnt6 = luna_ceil_sim(p_conv->output_w - 1 - luna_ceil_sim(p_conv->padding_w_left, log2n_s_w), 2) + 1 + pe_core->w_padd_l_ext;
    p_para->xw_map_cnt7 = 0;
    p_para->h_map_cnt8 = split_ou_h;	//p_conv->output_h;
    p_para->xh_map_cnt9 = 16;
    p_para->g_map_cnt10 = 4;
    p_para->xg_map_cnt11 = 0;
	p_para->gs_map_cnt12 = luna_ceil_sim(p_conv->input_c, 3) + 1;
	p_para->xgs_map_cnt13 = 0;

    p_para->c_stride_inf = 0;
    p_para->c_stride_ker = 0;
	p_para->kw_stride_inf = luna_ceil_sim(p_conv->input_c, 3);

    p_para->kw_stride_ker = 1;
    p_para->kh_stride_inf = luna_ceil_sim(p_conv->input_c, 3) * luna_ceil_sim(p_conv->input_w, 2 + log2n_s_w) * p_conv->stride_w;
    p_para->kh_stride_ker = p_conv->weight_w;
    p_para->w_stride_inf = luna_ceil_sim(p_conv->input_c, 3) * p_conv->stride_w;
    p_para->w_stride_ker = 0;
    p_para->h_stride_inf = luna_ceil_sim(p_conv->input_c, 3) * luna_ceil_sim(p_conv->input_w, 2 + log2n_s_w) * p_conv->stride_w * p_conv->stride_h;
    p_para->h_stride_ker = 0;

    p_para->g_stride_inf = 0;
    p_para->g_stride_ker = 0;
	p_para->gs_stride_inf = 1;
	p_para->gs_stride_ker = p_conv->weight_w * p_conv->weight_h;

	if ((0 == pe_core->w_padd_l_ext) && (0 == p_conv->padding_h_up))
	{
		p_para->inf_base_addr_1st = 0;
	}
	else
	{
		p_para->inf_base_addr_1st = (uint32_t)(1 << 32) - pe_core->w_padd_l_ext * luna_ceil_sim(p_conv->input_c, 3) * p_conv->stride_w - p_conv->padding_h_up * p_para->kh_stride_inf;
	}
	if (0 == pe_core->w_padd_l_ext)
	{
		p_para->inf_base_addr = 0;
	}
	else
	{
		p_para->inf_base_addr = (uint32_t)(1 << 32) - pe_core->w_padd_l_ext * luna_ceil_sim(p_conv->input_c, 3) * p_conv->stride_w;
	}

	if (0 == pe_core->kw_padd_l_ext)
	{
		p_para->ker_base_addr = 0;
	}
	else
	{
		p_para->ker_base_addr = (uint32_t)(1 << 32) - pe_core->kw_padd_l_ext * 1;
	}


	return 0;
}

static int32_t luna_deconv_infmap_para_cfg(conv_struct_t *conv_st, luna_cnn_pe_core_cfg_t *pe_core, luna_cnn_infmap_param_t *inf_param, int32_t k_n, int32_t k_n_last, int32_t pad_ht_1st, int32_t pad_ht)
{
	conv_struct_t* p_conv = conv_st;
	luna_cnn_infmap_param_t *p_para = inf_param;
	uint32_t log2n_s_w = p_conv->stride_w >> 1;
	uint32_t log2n_s_h = p_conv->stride_h >> 1;
	int32_t s_w = 1, s_h = 1;

	p_para->c_map_cnt0 = luna_ceil_sim(p_conv->input_c, 3);
    p_para->xc_map_cnt1 = 0;
    p_para->kw_map_cnt2 = p_conv->weight_w;
    p_para->xkw_map_cnt3 = 3;
    p_para->kh_map_cnt4 = p_conv->weight_h;
	p_para->xkh_map_cnt5 = 1;
    p_para->w_map_cnt6 = luna_ceil_sim(p_conv->output_w - 1 - p_conv->padding_w_left, 2) + 1 + pe_core->w_padd_l_ext;
    p_para->xw_map_cnt7 = 0;
    // p_para->h_map_cnt8 = p_conv->output_h;
    p_para->xh_map_cnt9 = 16;
	p_para->g_map_cnt10 = luna_ceil_sim(k_n, 1) + 1;
	p_para->g_map_cnt10_last = luna_ceil_sim(k_n_last, 1) + 1;
    p_para->xg_map_cnt11 = 0;

    p_para->c_stride_inf = 1;
    p_para->c_stride_ker = 1;
	p_para->kw_stride_inf = luna_ceil_sim(p_conv->input_c, 3);
    p_para->kw_stride_ker = luna_ceil_sim(p_conv->input_c, 3);
    p_para->kh_stride_inf = luna_ceil_sim(p_conv->input_c, 3) * luna_ceil_sim(p_conv->input_w, 2);
    p_para->kh_stride_ker = luna_ceil_sim(p_conv->input_c, 3) * p_conv->weight_w;
    p_para->w_stride_inf = luna_ceil_sim(p_conv->input_c, 3);
    p_para->w_stride_ker = 0;
    p_para->h_stride_inf = luna_ceil_sim(p_conv->input_c, 3) * luna_ceil_sim(p_conv->input_w, 2);
    p_para->h_stride_ker = 0;

    p_para->g_stride_inf = 0;
    p_para->g_stride_ker = p_conv->weight_w * p_conv->weight_h * luna_ceil_sim(p_conv->input_c, 3);

	if ((0 == pe_core->w_padd_l_ext) && (0 == pad_ht_1st))
	{
		p_para->inf_base_addr_1st = 0;
	}
	else
	{
		p_para->inf_base_addr_1st = (uint32_t)(1 << 32) - luna_ceil_sim(pe_core->w_padd_l_ext, log2n_s_w) * luna_ceil_sim(p_conv->input_c, 3) - luna_ceil_sim(pad_ht_1st, log2n_s_h) * p_para->kh_stride_inf;
	}
	if ((0 == pe_core->w_padd_l_ext) && (0 == pad_ht))
	{
		p_para->inf_base_addr = 0;
	}
	else
	{
		p_para->inf_base_addr = (uint32_t)(1 << 32) - luna_ceil_sim(pe_core->w_padd_l_ext, log2n_s_w) * luna_ceil_sim(p_conv->input_c, 3) - luna_ceil_sim(pad_ht, log2n_s_h) * p_para->kh_stride_inf;
	}

	if (0 == pe_core->kw_padd_l_ext)
	{
		p_para->ker_base_addr = 0;
	}
	else
	{
		p_para->ker_base_addr = (uint32_t)(1 << 32) - pe_core->kw_padd_l_ext * luna_ceil_sim(p_conv->input_c, 3);
	}

	return 0;
}


int32_t luna_split_conv_para_pack(conv_struct_t *conv_st, luna_cnn_static_para_t *p_cnn_static_para, int32_t cnn_layer_type)
{
	int ret = -1;
	luna_cnn_static_para_t *p_para = (luna_cnn_static_para_t *)p_cnn_static_para;
	conv_struct_t *p_conv = (conv_struct_t *)conv_st;

	memset(p_para, 0, sizeof(luna_cnn_static_para_t));
	
	int32_t in_c = p_conv->input_c;
	int32_t in_h = p_conv->input_h;
	int32_t in_w = p_conv->input_w;
	int32_t ou_c = p_conv->output_c;
	int32_t ou_h = p_conv->output_h;
	int32_t ou_w = p_conv->output_w;
	int32_t k_h = p_conv->weight_h;
	int32_t k_w = p_conv->weight_w;
	int32_t s_h = p_conv->stride_h;
	int32_t s_w = p_conv->stride_w;
	int32_t pad_wl = p_conv->padding_w_left;
	int32_t pad_wr = p_conv->padding_w_right;
	int32_t pad_ht = p_conv->padding_h_up;
	int32_t pad_hb = p_conv->padding_h_down;
	int32_t dia_w = (p_conv->dilation_w == 0) ? 1 : p_conv->dilation_w;
	int32_t dia_h = (p_conv->dilation_h == 0) ? 1 : p_conv->dilation_h;
	int32_t is_bias = p_conv->is_bias;
	int32_t is_per_chn = p_conv->is_per_chn;
	int32_t data_mem_type = p_conv->data_mem_type;
	
	int32_t ou_bits = p_conv->ou_bits;
	int32_t weight_bits = p_conv->weight_bits;
	int32_t cnn_layer = cnn_layer_type;

	uint32_t log2n_s_w = s_w >> 1;
	uint32_t log2n_s_h = s_h >> 1;

	int32_t shift = 0;
	ret = luna_cnn_shift_config(p_conv, &shift);

	// split kernel
	luna_cnn_split_kernel_t split_k_t;
	memset(&split_k_t, 0, sizeof(luna_cnn_split_kernel_t));
	ret = luna_cnn_split_kernel_config(p_conv, &split_k_t, cnn_layer);
	if (ret)
	{
		printf("[%s][%d]split kernel failed \r\n", __func__, __LINE__);
		return ret;
	}

	// split input
	luna_cnn_split_input_t split_in_t;
	memset(&split_in_t, 0, sizeof(luna_cnn_split_input_t));
	if (LUNA_DECONV == cnn_layer || LUNA_DECONV1D == cnn_layer)
	{
		ret = luna_deconv_split_input_config(p_conv, &split_in_t, cnn_layer);
	}
	else
	{
		ret = luna_conv_split_input_config(p_conv, &split_in_t, cnn_layer);
	}
	if (ret)
	{
		printf("[%s][%d]split input failed \r\n", __func__, __LINE__);
		return ret;
	}

	luna_cnn_pe_core_cfg_t pe_core_cfg;
	memset(&pe_core_cfg, 0, sizeof(luna_cnn_pe_core_cfg_t));
	ret = luna_cnn_pe_core_config(p_conv, &split_in_t, &pe_core_cfg, cnn_layer);
	
	uint32_t tmp_l = 0;
	uint32_t tmp_h = 0;
	luna_cnn_infmap_param_t inf_param;
	memset(&inf_param, 0, sizeof(luna_cnn_infmap_param_t));
	switch (cnn_layer) {
		case LUNA_CONV1D:
		case LUNA_CONV:
		{
			luna_conv_infmap_para_cfg(p_conv, &pe_core_cfg, &inf_param, split_k_t.split_k_n, split_k_t.split_k_n_last, split_in_t.split_ou_h);

			// load_weight
			tmp_l = ((luna_ceil_sim(in_c, 3) * k_w * k_h * luna_ceil_sim(split_k_t.split_k_n, 1)) << 1) & 0xFF;
			p_para->r8_c0_W = (uint16_t)tmp_l;
			tmp_l = ((luna_ceil_sim(in_c, 3) * k_w * k_h * luna_ceil_sim(split_k_t.split_k_n, 1)) << 1) >> 8;
			p_para->r9_c0_W = (uint16_t)tmp_l;
			tmp_l = ((luna_ceil_sim(in_c, 3) * k_w * k_h * luna_ceil_sim(split_k_t.split_k_n_last, 1)) << 1) & 0xFF;
			p_para->r8_c0_W_last = (uint16_t)tmp_l;
			tmp_l = ((luna_ceil_sim(in_c, 3) * k_w * k_h * luna_ceil_sim(split_k_t.split_k_n_last, 1)) << 1) >> 8;
			p_para->r9_c0_W_last = (uint16_t)tmp_l;
			p_para->r18_W = (luna_ceil_sim(in_c, 3) * k_w * k_h * luna_ceil_sim(split_k_t.split_k_n, 1)) << 4;
			p_para->r18_W_last = (luna_ceil_sim(in_c, 3) * k_w * k_h * luna_ceil_sim(split_k_t.split_k_n_last, 1)) << 4;

			// calculation
			p_para->r8_st_rw_0 = (inf_param.c_map_cnt0 & 0xFF) + ((inf_param.xc_map_cnt1 & 0xFF) << 8) + ((inf_param.kh_map_cnt4 & 0xFF) << 16) + ((inf_param.xkh_map_cnt5 & 0xFF) << 24);
			p_para->r9_st_rw_0 = (inf_param.c_map_cnt0 >> 8) + ((inf_param.kh_map_cnt4 >> 8) << 16);
			p_para->r8_st_rw_1 = (inf_param.kw_map_cnt2 & 0xFF) + ((inf_param.xkw_map_cnt3 & 0xFF) << 8) + ((inf_param.w_map_cnt6 & 0xFF) << 16) + ((inf_param.xw_map_cnt7 & 0xFF) << 24);
			p_para->r9_st_rw_1 = (inf_param.kw_map_cnt2 >> 8) + ((inf_param.w_map_cnt6 >> 8) << 16);
			p_para->r8_st_rw_2 = (inf_param.h_map_cnt8 & 0xFF) + ((inf_param.xh_map_cnt9 & 0xFF) << 8) + ((inf_param.g_map_cnt10 & 0xFF) << 16) + ((inf_param.xg_map_cnt11 & 0xFF) << 24);
			p_para->r9_st_rw_2 = (inf_param.h_map_cnt8 >> 8) + ((inf_param.g_map_cnt10 >> 8) << 16);
			p_para->r8_st_rw_3 = (inf_param.h_map_cnt8 & 0xFF) + ((inf_param.xh_map_cnt9 & 0xFF) << 8) + ((inf_param.g_map_cnt10_last & 0xFF) << 16) + ((inf_param.xg_map_cnt11 & 0xFF) << 24);
			p_para->r9_st_rw_3 = (inf_param.h_map_cnt8 >> 8) + ((inf_param.g_map_cnt10_last >> 8) << 16);

			uint32_t log2n_dia_w = (dia_w == 8) ? 3 : (dia_w >> 1);
			uint32_t log2n_dia_h = (dia_h == 8) ? 3 : (dia_h >> 1);
			p_para->r17 = (1 << 25) + 1 + (log2n_dia_w << 4) + (log2n_dia_h << 6) + (is_bias << 13) + (is_per_chn << 15);
			tmp_l = (k_w + pe_core_cfg.kw_padd_l_ext + (log2n_s_w << 4) + (log2n_s_h << 6) + (pe_core_cfg.paddin_wl_nor << 8) + (pad_ht << 12));
			tmp_h = (((k_h - 1) * dia_h + 1 - pe_core_cfg.pad_hb_real_1st_last - 1) << 2) + (pe_core_cfg.w_padd_l_ext << 8) + (pe_core_cfg.w_padd_r_ext << 10) + (pe_core_cfg.kw_padd_l_ext << 13) + (pe_core_cfg.w_padd_mask << 15);
			p_para->r18_1st_last = (tmp_h << 16) + tmp_l;	//
			tmp_h = (((k_h - 1) * dia_h + 1 - pe_core_cfg.pad_hb_real_1st - 1) << 2) + (pe_core_cfg.w_padd_l_ext << 8) + (pe_core_cfg.w_padd_r_ext << 10) + (pe_core_cfg.kw_padd_l_ext << 13) + (pe_core_cfg.w_padd_mask << 15);
			p_para->r18_1st = (tmp_h << 16) + tmp_l;	//
			tmp_l = (k_w + pe_core_cfg.kw_padd_l_ext + (log2n_s_w << 4) + (log2n_s_h << 6) + (pe_core_cfg.paddin_wl_nor << 8));
			tmp_h = (((k_h - 1) * dia_h + 1 - pe_core_cfg.pad_hb_real_last - 1) << 2) + (pe_core_cfg.w_padd_l_ext << 8) + (pe_core_cfg.w_padd_r_ext << 10) + (pe_core_cfg.kw_padd_l_ext << 13) + (pe_core_cfg.w_padd_mask << 15);
			p_para->r18_last = (tmp_h << 16) + tmp_l;	//
			tmp_h = (((k_h - 1) * dia_h + 1 - pe_core_cfg.pad_hb_real - 1) << 2) + (pe_core_cfg.w_padd_l_ext << 8) + (pe_core_cfg.w_padd_r_ext << 10) + (pe_core_cfg.kw_padd_l_ext << 13) + (pe_core_cfg.w_padd_mask << 15);
			p_para->r18_mid = (tmp_h << 16) + tmp_l;	//

			p_para->r27_integr = luna_ceil_sim(in_c, 3) * k_h - 1;
			p_para->r18_ch_len = (ou_w * split_in_t.split_ou_h * ou_bits) >> 3;
			p_para->r19_ch_num = split_k_t.split_k_n;
			p_para->r19_ch_num_last = split_k_t.split_k_n_last;
		}
		break;
		case LUNA_DEPTHWISE1D:
		case LUNA_MAX_POOLING1D:
		case LUNA_MEAN_POOLING1D:
		case LUNA_DEPTHWISE:
		case LUNA_MAX_POOLING:
		case LUNA_MEAN_POOLING:
		{
			luna_depthwise_infmap_para_cfg(p_conv, &pe_core_cfg, &inf_param, split_in_t.split_ou_h);

			// load_weight
			tmp_l = (luna_ceil_sim(in_c, 3) * k_w * k_h) & 0xFF;
			p_para->r8_c0_W = (uint16_t)tmp_l;
			tmp_l = (luna_ceil_sim(in_c, 3) * k_w * k_h) >> 8;
			p_para->r9_c0_W = (uint16_t)tmp_l;
			p_para->r18_W = (luna_ceil_sim(in_c, 3) * k_w * k_h) << 3;

			// calculation
			p_para->r8_st_rw_0 = (inf_param.c_map_cnt0 & 0xFF) + ((inf_param.xc_map_cnt1 & 0xFF) << 8) + ((inf_param.kh_map_cnt2 & 0xFF) << 16) + ((inf_param.xkh_map_cnt3 & 0xFF) << 24);
			p_para->r9_st_rw_0 = (inf_param.c_map_cnt0 >> 8) + ((inf_param.kh_map_cnt2 >> 8) << 16);
			p_para->r8_st_rw_1 = (inf_param.kw_map_cnt4 & 0xFF) + ((inf_param.xkw_map_cnt5 & 0xFF) << 8) + ((inf_param.w_map_cnt6 & 0xFF) << 16) + ((inf_param.xw_map_cnt7 & 0xFF) << 24);
			p_para->r9_st_rw_1 = (inf_param.kw_map_cnt4 >> 8) + ((inf_param.w_map_cnt6 >> 8) << 16);
			p_para->r8_st_rw_2 = (inf_param.h_map_cnt8 & 0xFF) + ((inf_param.xh_map_cnt9 & 0xFF) << 8) + ((inf_param.g_map_cnt10 & 0xFF) << 16) + ((inf_param.xg_map_cnt11 & 0xFF) << 24);
			p_para->r9_st_rw_2 = (inf_param.h_map_cnt8 >> 8) + ((inf_param.g_map_cnt10 >> 8) << 16);
			p_para->r8_st_rw_3 = (inf_param.gs_map_cnt12 & 0xFF) + ((inf_param.xgs_map_cnt13 & 0xFF) << 8);
			p_para->r9_st_rw_3 = (inf_param.gs_map_cnt12 >> 8);
			p_para->r11_gs_0 = (inf_param.gs_stride_inf & 0xFF);
			p_para->r13_gs_0 = (inf_param.gs_stride_inf >> 8);
			p_para->r11_gs_1 = (inf_param.gs_stride_ker & 0xFF);
			p_para->r13_gs_1 = (inf_param.gs_stride_ker >> 8);

			p_para->r17 = (1 << 25) + 1 + (is_bias << 13) + (is_per_chn << 15);

			tmp_l = (pe_core_cfg.kernel_w_nor + (log2n_s_w << 4) + (log2n_s_h << 6) + (pe_core_cfg.paddin_wl_nor << 8) + (pad_ht << 12));		
			tmp_h = ((k_h - pe_core_cfg.pad_hb_real_1st_last - 1) << 2) + (pe_core_cfg.w_padd_l_ext << 8) + (pe_core_cfg.w_padd_r_ext << 10) + (pe_core_cfg.kw_padd_l_ext << 13) + (pe_core_cfg.w_padd_mask << 15);
			p_para->r18_1st_last = (tmp_h << 16) + tmp_l;	//
			tmp_h = ((k_h - pe_core_cfg.pad_hb_real_1st - 1) << 2) + (pe_core_cfg.w_padd_l_ext << 8) + (pe_core_cfg.w_padd_r_ext << 10) + (pe_core_cfg.kw_padd_l_ext << 13) + (pe_core_cfg.w_padd_mask << 15);
			p_para->r18_1st = (tmp_h << 16) + tmp_l;	//
			tmp_l = (pe_core_cfg.kernel_w_nor + (log2n_s_w << 4) + (log2n_s_h << 6) + (pe_core_cfg.paddin_wl_nor << 8));	
			tmp_h = ((k_h - pe_core_cfg.pad_hb_real_last - 1) << 2) + (pe_core_cfg.w_padd_l_ext << 8) + (pe_core_cfg.w_padd_r_ext << 10) + (pe_core_cfg.kw_padd_l_ext << 13) + (pe_core_cfg.w_padd_mask << 15);
			p_para->r18_last = (tmp_h << 16) + tmp_l;	//
			tmp_h = ((k_h - pe_core_cfg.pad_hb_real - 1) << 2) + (pe_core_cfg.w_padd_l_ext << 8) + (pe_core_cfg.w_padd_r_ext << 10) + (pe_core_cfg.kw_padd_l_ext << 13) + (pe_core_cfg.w_padd_mask << 15);
			p_para->r18_mid = (tmp_h << 16) + tmp_l;	//

			p_para->r27_integr = k_h - 1;
			p_para->r18_ch_len = (ou_w * split_in_t.split_ou_h * ou_bits) >> 3;
			p_para->r19_ch_num = in_c;
		}
		break;
		case LUNA_DECONV1D:
		case LUNA_DECONV:
		{
			int32_t s_w_d = p_conv->stride_w;	// for deconv
			int32_t s_h_d = p_conv->stride_h;
			s_w = 1;
			s_h = 1;

			luna_deconv_infmap_para_cfg(p_conv, &pe_core_cfg, &inf_param, split_k_t.split_k_n, split_k_t.split_k_n_last, split_in_t.pad_ht_1st, split_in_t.pad_ht);

			// load_weight
			tmp_l = ((luna_ceil_sim(in_c, 3) * k_w * k_h * luna_ceil_sim(split_k_t.split_k_n, 1)) << 1) & 0xFF;
			p_para->r8_c0_W = (uint16_t)tmp_l;
			tmp_l = ((luna_ceil_sim(in_c, 3) * k_w * k_h * luna_ceil_sim(split_k_t.split_k_n, 1)) << 1) >> 8;
			p_para->r9_c0_W = (uint16_t)tmp_l;
			tmp_l = ((luna_ceil_sim(in_c, 3) * k_w * k_h * luna_ceil_sim(split_k_t.split_k_n_last, 1)) << 1) & 0xFF;
			p_para->r8_c0_W_last = (uint16_t)tmp_l;
			tmp_l = ((luna_ceil_sim(in_c, 3) * k_w * k_h * luna_ceil_sim(split_k_t.split_k_n_last, 1)) << 1) >> 8;
			p_para->r9_c0_W_last = (uint16_t)tmp_l;
			p_para->r18_W = (luna_ceil_sim(in_c, 3) * k_w * k_h * luna_ceil_sim(split_k_t.split_k_n, 1)) << 4;
			p_para->r18_W_last = (luna_ceil_sim(in_c, 3) * k_w * k_h * luna_ceil_sim(split_k_t.split_k_n_last, 1)) << 4;

			// calculation
			p_para->r8_st_rw_0 = (inf_param.c_map_cnt0 & 0xFF) + ((inf_param.xc_map_cnt1 & 0xFF) << 8) + ((inf_param.kh_map_cnt4 & 0xFF) << 16) + ((inf_param.xkh_map_cnt5 & 0xFF) << 24);
			p_para->r9_st_rw_0 = (inf_param.c_map_cnt0 >> 8) + ((inf_param.kh_map_cnt4 >> 8) << 16);
			p_para->r8_st_rw_1 = (inf_param.kw_map_cnt2 & 0xFF) + ((inf_param.xkw_map_cnt3 & 0xFF) << 8) + ((inf_param.w_map_cnt6 & 0xFF) << 16) + ((inf_param.xw_map_cnt7 & 0xFF) << 24);
			p_para->r9_st_rw_1 = (inf_param.kw_map_cnt2 >> 8) + ((inf_param.w_map_cnt6 >> 8) << 16);
			
			p_para->r8_st_rw_2 = (ou_h & 0xFF) + ((inf_param.xh_map_cnt9 & 0xFF) << 8) + ((inf_param.g_map_cnt10 & 0xFF) << 16) + ((inf_param.xg_map_cnt11 & 0xFF) << 24);
			p_para->r9_st_rw_2 = (ou_h >> 8) + ((inf_param.g_map_cnt10 >> 8) << 16);

			p_para->r8_st_rw_4 = (split_in_t.split_ou_h_1st & 0xFF) + ((inf_param.xh_map_cnt9 & 0xFF) << 8) + ((inf_param.g_map_cnt10 & 0xFF) << 16) + ((inf_param.xg_map_cnt11 & 0xFF) << 24);
			p_para->r9_st_rw_4 = (split_in_t.split_ou_h_1st >> 8) + ((inf_param.g_map_cnt10 >> 8) << 16);
			p_para->r8_st_rw_5 = (split_in_t.split_ou_h_last & 0xFF) + ((inf_param.xh_map_cnt9 & 0xFF) << 8) + ((inf_param.g_map_cnt10 & 0xFF) << 16) + ((inf_param.xg_map_cnt11 & 0xFF) << 24);
			p_para->r9_st_rw_5 = (split_in_t.split_ou_h_last >> 8) + ((inf_param.g_map_cnt10 >> 8) << 16);

			p_para->r8_st_rw_3 = (split_in_t.split_ou_h & 0xFF) + ((inf_param.xh_map_cnt9 & 0xFF) << 8) + ((inf_param.g_map_cnt10_last & 0xFF) << 16) + ((inf_param.xg_map_cnt11 & 0xFF) << 24);
			p_para->r9_st_rw_3 = (split_in_t.split_ou_h >> 8) + ((inf_param.g_map_cnt10_last >> 8) << 16);

			p_para->r17 = (1 << 25) + 4 + (is_bias << 13) + (is_per_chn << 15);

			tmp_l = (pe_core_cfg.kernel_w_nor + (log2n_s_w << 4) + (log2n_s_h << 6) + (pe_core_cfg.paddin_wl_nor << 8) + (split_in_t.pad_ht_1st << 12));
			tmp_h = ((k_h - pe_core_cfg.pad_hb_real_1st_last - 1) << 2) + (pe_core_cfg.w_padd_l_ext << 8) + (pe_core_cfg.w_padd_r_ext << 10) + (pe_core_cfg.kw_padd_l_ext << 13) + (pe_core_cfg.w_padd_mask << 15);
			p_para->r18_1st_last = (tmp_h << 16) + tmp_l;	//
			tmp_h = ((k_h - pe_core_cfg.pad_hb_real_1st - 1) << 2) + (pe_core_cfg.w_padd_l_ext << 8) + (pe_core_cfg.w_padd_r_ext << 10) + (pe_core_cfg.kw_padd_l_ext << 13) + (pe_core_cfg.w_padd_mask << 15);
			p_para->r18_1st = (tmp_h << 16) + tmp_l;	//
			tmp_l = (pe_core_cfg.kernel_w_nor + (log2n_s_w << 4) + (log2n_s_h << 6) + (pe_core_cfg.paddin_wl_nor << 8) + (split_in_t.pad_ht << 12));
			tmp_h = ((k_h - pe_core_cfg.pad_hb_real_last - 1) << 2) + (pe_core_cfg.w_padd_l_ext << 8) + (pe_core_cfg.w_padd_r_ext << 10) + (pe_core_cfg.kw_padd_l_ext << 13) + (pe_core_cfg.w_padd_mask << 15);
			p_para->r18_last = (tmp_h << 16) + tmp_l;	//
			tmp_h = ((k_h - pe_core_cfg.pad_hb_real - 1) << 2) + (pe_core_cfg.w_padd_l_ext << 8) + (pe_core_cfg.w_padd_r_ext << 10) + (pe_core_cfg.kw_padd_l_ext << 13) + (pe_core_cfg.w_padd_mask << 15);
			p_para->r18_mid = (tmp_h << 16) + tmp_l;	//

			p_para->r27_integr = luna_ceil_sim(in_c, 3) * k_h - 1;
			p_para->r18_ch_len = (ou_w * ou_h * ou_bits) >> 3;
			p_para->r18_ch_len_1st = (ou_w * split_in_t.split_ou_h_1st * ou_bits) >> 3;
			p_para->r18_ch_len_last = (ou_w * split_in_t.split_ou_h_last * ou_bits) >> 3;
			p_para->r18_ch_len_mid = (ou_w * split_in_t.split_ou_h * ou_bits) >> 3;
			p_para->r19_ch_num = split_k_t.split_k_n;
			p_para->r19_ch_num_last = split_k_t.split_k_n_last;
		}
		break;
		default:
		break;
	}

	// load feature map
	tmp_l = luna_ceil_sim(in_w, 3) & 0xFF;
	tmp_h = split_in_t.split_in_h_1st_last & 0xFF;
	p_para->r8_c0_c2_1st_last = (tmp_h << 16) + tmp_l;
	tmp_h = split_in_t.split_in_h_1st & 0xFF;
	p_para->r8_c0_c2_1st = (tmp_h << 16) + tmp_l;
	tmp_h = split_in_t.split_in_h_last & 0xFF;
	p_para->r8_c0_c2_last = (tmp_h << 16) + tmp_l;
	tmp_h = split_in_t.split_in_h & 0xFF;
	p_para->r8_c0_c2_mid = (tmp_h << 16) + tmp_l;

	tmp_l = luna_ceil_sim(in_w, 3) >> 8;
	tmp_h = split_in_t.split_in_h_1st_last >> 8;
	p_para->r9_c0_c2_1st_last = (tmp_h << 16) + tmp_l;
	tmp_h = split_in_t.split_in_h_1st >> 8;
	p_para->r9_c0_c2_1st = (tmp_h << 16) + tmp_l;
	tmp_h = split_in_t.split_in_h_last >> 8;
	p_para->r9_c0_c2_last = (tmp_h << 16) + tmp_l;
	tmp_h = split_in_t.split_in_h >> 8;
	p_para->r9_c0_c2_mid = (tmp_h << 16) + tmp_l;

	p_para->r8_c4 = (uint16_t)(in_c & 0xFF);
	p_para->r9_c4 = (uint16_t)(in_c >> 8);

	p_para->r10_saddr_h = (uint16_t)(in_w & 0xFF);
	p_para->r11_saddr_l = (uint16_t)(((in_w * in_h) & 0xFF) << 8);
	p_para->r12_saddr_h = (uint16_t)(in_w >> 8);
	p_para->r13_saddr_l = (uint16_t)(((in_w * in_h) >> 8) << 8);
	p_para->infmap_intv = in_w * in_h;
	p_para->r18_w_l = in_w;
	p_para->r19_h_l_1st_last = split_in_t.split_in_h_1st_last;
	p_para->r19_h_l_1st = split_in_t.split_in_h_1st;
	p_para->r19_h_l_last = split_in_t.split_in_h_last;
	p_para->r19_h_l_mid = split_in_t.split_in_h;
	p_para->r20_c_l = in_c;
	p_para->r21_sw_h = (s_w >> 1) << 8;

	// calculation

	// infmap P0 address
	p_para->r10_c_kh_0 = (inf_param.c_stride_inf & 0xFF) + ((inf_param.kh_stride_inf & 0xFF) << 16);
	p_para->r11_kw_w_0 = (inf_param.kw_stride_inf & 0xFF) + ((inf_param.w_stride_inf & 0xFF) << 16);
	p_para->r12_c_kh_0 = (inf_param.c_stride_inf >> 8) + ((inf_param.kh_stride_inf >> 8) << 16);
	p_para->r13_kw_w_0 = (inf_param.kw_stride_inf >> 8) + ((inf_param.w_stride_inf >> 8) << 16);
	p_para->r14_p0_addr_1st = inf_param.inf_base_addr_1st;
	p_para->r14_p0_addr = inf_param.inf_base_addr;
	p_para->r10_h_g_0 = (inf_param.h_stride_inf & 0xFF) + ((inf_param.g_stride_inf & 0xFF) << 16);
	p_para->r12_h_g_0 = (inf_param.h_stride_inf >> 8) + ((inf_param.g_stride_inf >> 8) << 16);
	// infmap P1 address
	p_para->r10_c_kh_1 = (inf_param.c_stride_ker & 0xFF) + ((inf_param.kh_stride_ker & 0xFF) << 16);
	p_para->r11_kw_w_1 = (inf_param.kw_stride_ker & 0xFF) + ((inf_param.w_stride_ker & 0xFF) << 16);
	p_para->r12_c_kh_1 = (inf_param.c_stride_ker >> 8) + ((inf_param.kh_stride_ker >> 8) << 16);
	p_para->r13_kw_w_1 = (inf_param.kw_stride_ker >> 8) + ((inf_param.w_stride_ker >> 8) << 16);
	p_para->r14_p1_addr = inf_param.ker_base_addr;
	p_para->r10_h_g_1 = (inf_param.h_stride_ker & 0xFF) + ((inf_param.g_stride_ker & 0xFF) << 16);
	p_para->r12_h_g_1 = (inf_param.h_stride_ker >> 8) + ((inf_param.g_stride_ker >> 8) << 16);

	p_para->r16_h = (((log2n_s_w + 3) << 8) | 0x03) << 16;
	p_para->r4_judge_sw = (s_w != 1);

	p_para->r23_shift = shift;	//shift
	p_para->r22_ow = pe_core_cfg.ow_first_num + (pe_core_cfg.ow_last_num << 4) + (pe_core_cfg.ow_1st_msk << 8) + (pe_core_cfg.ow_msk << 12) + (pe_core_cfg.ow_num << 16);
	// p_para->r27_integr = luna_ceil_sim(in_c, 3) * k_h - 1;
	// p_para->r18_ch_len = (ou_w * split_in_t.split_ou_h * ou_bits) >> 3;
	// p_para->r19_ch_num = split_k_t.split_k_n;
	// p_para->r19_ch_num_last = split_k_t.split_k_n_last;
	p_para->r21_interval = (ou_w * ou_h * ou_bits) >> 3;
	p_para->is_bias = is_bias;
	p_para->is_per_chn = is_per_chn;
	p_para->data_mem_type = data_mem_type;
	p_para->weight_bits = weight_bits;
	p_para->ou_bits = ou_bits;

	p_para->k_n_loop_num = split_k_t.split_k_num;
	p_para->in_loop_num = split_in_t.split_in_num;
	p_para->i_addr_inc_1st = split_in_t.i_addr_inc_1st;
	p_para->i_addr_inc = split_in_t.i_addr_inc;
	p_para->o_addr_inc_1st = split_in_t.o_addr_inc_1st;
	p_para->o_addr_inc = split_in_t.o_addr_inc;
	p_para->k_o_addr_inc = split_k_t.k_o_addr_inc;
	p_para->k_addr_inc = split_k_t.k_addr_inc;
	p_para->b_addr_inc = split_k_t.b_addr_inc;

	p_para->r4_judge_sw = p_conv->reserved;	// for relux value

	// for parse static cnn para
	p_para->in_c = p_conv->input_c;
	p_para->in_h = p_conv->input_h;
	p_para->in_w = p_conv->input_w;
	p_para->ou_c = p_conv->output_c;
	p_para->ou_h = p_conv->output_h;
	p_para->ou_w = p_conv->output_w;
	p_para->k_h = p_conv->weight_h;
	p_para->k_w = p_conv->weight_w;
	p_para->s_h = p_conv->stride_h;
	p_para->s_w = p_conv->stride_w;
	p_para->pad_wl = p_conv->padding_w_left;
	p_para->pad_wr = p_conv->padding_w_right;
	p_para->pad_ht = p_conv->padding_h_up;
	p_para->pad_hb = p_conv->padding_h_down;
	p_para->dilation_h = p_conv->dilation_h;
	p_para->dilation_w = p_conv->dilation_w;
	p_para->out_padding_h = p_conv->out_padding_h;
	p_para->out_padding_w = p_conv->out_padding_w;

	return ret;
}

// int32_t luna_split_deconv_para_pack(conv_struct_t *conv_st, luna_cnn_static_para_t *p_cnn_static_para, int32_t cnn_layer_type)
// {
// 	return 0;
// }

int32_t luna_deconv_torch2convst(conv_struct_t *conv_struct_)
{
	conv_struct_t *p_conv = conv_struct_;
	p_conv->padding_w_left = p_conv->weight_w - p_conv->padding_w_left - 1;
	p_conv->padding_w_right = p_conv->weight_w - p_conv->padding_w_right - p_conv->stride_w + p_conv->out_padding_w;
	p_conv->padding_h_up = p_conv->weight_h - p_conv->padding_h_up - 1;
	p_conv->padding_h_down = p_conv->weight_h - p_conv->padding_h_down - 1 + p_conv->out_padding_h;

	// int32_t input_w_after_padding = p_conv->input_w * p_conv->stride_w + p_conv->padding_w_left + p_conv->padding_w_right;
	// int32_t input_h_after_padding = (p_conv->input_h - 1) * p_conv->stride_h + 1 + p_conv->padding_h_up + p_conv->padding_h_down;
	// int32_t output_w = input_w_after_padding - p_conv->weight_w + 1;
	// int32_t output_h = input_h_after_padding - p_conv->weight_h + 1;

	// p_conv->output_w = output_w;
	// p_conv->output_h = output_h;

	return 0;
}

//////////////////////reshape weight//////////////////////
int32_t reshape_weight_for_conv(int8_t *input_weight, int8_t *input_weight_T, conv_struct_t *conv_struct_)
{
	const int32_t c_aligned = 8;
	int32_t i, j, k, m, num, pos;
	int32_t kernel_w = conv_struct_->weight_w;
	int32_t kernel_h = conv_struct_->weight_h;
	int32_t kernel_c = conv_struct_->input_c;
	int32_t kernel_num = conv_struct_->output_c;
	int8_t* pIn = input_weight;
	int8_t* pOut = input_weight_T;

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
							pOut[pos] = pIn[num * kernel_w * kernel_h * kernel_c + kernel_w * kernel_h * (k + m) + kernel_w * i + j];
						else
							pOut[pos] = 0;
						pos++;
					}

					// kernel num+1
					if((num+1) < kernel_num)
					{
						for(m = 0; m < 8; m++)
						{
							if((k + m) < kernel_c)
								pOut[pos] = pIn[(num+1) * kernel_w * kernel_h * kernel_c + kernel_w * kernel_h * (k + m) + kernel_w * i + j];
							else
								pOut[pos] = 0;
							pos++;
						}
					}
					else
					{
						for(m = 0; m < 8; m++)
						{
							pOut[pos] = 0;
							pos++;
						}
					}

					k += 8;
				}
			}
		}
		num += 2;
	}

	int32_t weight_T_size = ceil(kernel_c * 1.f / c_aligned) * c_aligned * kernel_w * kernel_h * ceil(kernel_num / 2.0f) * 2;

	return weight_T_size;
}

int32_t reshape_weight_for_conv_4bit(int8_t *input_weight, int8_t *input_weight_T, conv_struct_t *conv_struct_)
{
	const int32_t c_aligned = 8;
	int32_t i, j, k, m, num, pos;
	int32_t kernel_w = conv_struct_->weight_w;
	int32_t kernel_h = conv_struct_->weight_h;
	int32_t kernel_c = conv_struct_->input_c;
	int32_t kernel_num = conv_struct_->output_c;
	int8_t* pIn = input_weight;
	int8_t* pOut = input_weight_T;

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
					int32_t in_oft_l = 0;
					int32_t in_oft_h = 0;
					// kernel num+0
					for(m = 0; m < 8; m+=2)
					{
						if ((k + m + 1) < kernel_c)
						{
							in_oft_l = num * kernel_w * kernel_h * kernel_c + kernel_w * kernel_h * (k + m) + kernel_w * i + j;
							in_oft_h = num * kernel_w * kernel_h * kernel_c + kernel_w * kernel_h * (k + m + 1) + kernel_w * i + j;
							pOut[pos] = (in_oft_l & 0x1) ? ((pIn[in_oft_l >> 1] >> 4) & 0xF) : ((pIn[in_oft_l >> 1]) & 0xF);
							pOut[pos] |= (in_oft_h & 0x1) ? (((pIn[in_oft_h >> 1] >> 4) & 0xF) << 4) : (((pIn[in_oft_h >> 1]) & 0xF) << 4);
						}
						else if ((k + m) < kernel_c)
						{
							in_oft_l = num * kernel_w * kernel_h * kernel_c + kernel_w * kernel_h * (k + m) + kernel_w * i + j;
							pOut[pos] = (in_oft_l & 0x1) ? ((pIn[in_oft_l >> 1] >> 4) & 0xF) : ((pIn[in_oft_l >> 1]) & 0xF);
							pOut[pos] &= 0x0F;
						}
						else
						{
							pOut[pos] = 0;
						}
						pos++;
					}

					// kernel num+1
					if((num + 1) < kernel_num)
					{
						for(m = 0; m < 8; m+=2)
						{
							if ((k + m + 1) < kernel_c)
							{
								in_oft_l = (num+1) * kernel_w * kernel_h * kernel_c + kernel_w * kernel_h * (k + m) + kernel_w * i + j;
								in_oft_h = (num+1) * kernel_w * kernel_h * kernel_c + kernel_w * kernel_h * (k + m + 1) + kernel_w * i + j;
								pOut[pos] = (in_oft_l & 0x1) ? ((pIn[in_oft_l >> 1] >> 4) & 0xF) : ((pIn[in_oft_l >> 1]) & 0xF);
								pOut[pos] |= (in_oft_h & 0x1) ? (((pIn[in_oft_h >> 1] >> 4) & 0xF) << 4) : (((pIn[in_oft_h >> 1]) & 0xF) << 4);
							}
							else if ((k + m) < kernel_c)
							{
								in_oft_l = (num+1) * kernel_w * kernel_h * kernel_c + kernel_w * kernel_h * (k + m) + kernel_w * i + j;
								pOut[pos] = (in_oft_l & 0x1) ? ((pIn[in_oft_l >> 1] >> 4) & 0xF) : ((pIn[in_oft_l >> 1]) & 0xF);
								pOut[pos] &= 0x0F;
							}
							else
							{
								pOut[pos] = 0;
							}
							pos++;
						}
					}
					else
					{
						for(m = 0; m < 8; m+=2)
						{
							pOut[pos] = 0;
							pos++;
						}
					}

					k += 8;
				}
			}
		}
		num += 2;
	}

	int32_t weight_T_size = ceil(kernel_c * 1.f / c_aligned) * c_aligned * kernel_w * kernel_h * ceil(kernel_num / 2.0f) * 2;

	return weight_T_size;
}

int32_t reshape_weight_for_depthwise(int8_t *input_weight, int8_t *input_weight_T, conv_struct_t *conv_struct_)
{
	const int32_t c_aligned = 8;
	int32_t i, j, k, depth, pos;
	int32_t kernel_c = conv_struct_->input_c;
	int32_t kernel_h = conv_struct_->weight_h;
	int32_t kernel_w = conv_struct_->weight_w;
	int8_t* pIn = input_weight;
	int8_t* pOut = input_weight_T;

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
						pOut[pos] = pIn[kernel_w * kernel_h * (depth+k) + kernel_w * i + j];
					else
						pOut[pos] = 0;

					pos++;
				}
			}
		}
		depth += c_aligned;
	}

	int32_t weight_T_size = ceil(kernel_c * 1.f / c_aligned) * c_aligned * kernel_w * kernel_h;

	return weight_T_size;
}

int32_t reshape_weight_for_depthwise_4bit(int8_t *input_weight, int8_t *input_weight_T, conv_struct_t *conv_struct_)
{
	const int32_t c_aligned = 8;
	int32_t i, j, k, depth, pos;
	int32_t kernel_c = conv_struct_->input_c;
	int32_t kernel_h = conv_struct_->weight_h;
	int32_t kernel_w = conv_struct_->weight_w;
	int8_t* pIn = input_weight;
	int8_t* pOut = input_weight_T;

	pos = 0;
	depth = 0;
	while(depth < kernel_c)
	{
		for(i = 0; i < kernel_h; i++)
		{
			for(j = 0; j < kernel_w; j++)
			{
				int32_t in_oft_l = 0;
				int32_t in_oft_h = 0;
				for(k = 0; k < c_aligned; k+=2)
				{
					if ((depth + k + 1) < kernel_c)
					{
						in_oft_l = kernel_w * kernel_h * (depth + k) + kernel_w * i + j;
						in_oft_h = kernel_w * kernel_h * (depth + k + 1) + kernel_w * i + j;
						pOut[pos] = (in_oft_l & 0x1) ? ((pIn[in_oft_l >> 1] >> 4) & 0xF) : ((pIn[in_oft_l >> 1]) & 0xF);
						pOut[pos] |= (in_oft_h & 0x1) ? (((pIn[in_oft_h >> 1] >> 4) & 0xF) << 4) : (((pIn[in_oft_h >> 1]) & 0xF) << 4);
					}
					else if ((depth + k) < kernel_c)
					{
						in_oft_l = kernel_w * kernel_h * (depth + k) + kernel_w * i + j;
						pOut[pos] = (in_oft_l & 0x1) ? ((pIn[in_oft_l >> 1] >> 4) & 0xF) : ((pIn[in_oft_l >> 1]) & 0xF);
						pOut[pos] &= 0x0F;
					}
					else
					{
						pOut[pos] = 0;
					}
					pos++;
				}
			}
		}
		depth += c_aligned;
	}

	int32_t weight_T_size = (int32_t)(ceil(kernel_c * 1.f / c_aligned) * c_aligned * kernel_w * kernel_h);

	return weight_T_size;
}

int32_t reshape_weight_for_deconv(int8_t *input_weight, int8_t *input_weight_T, conv_struct_t *conv_struct_)
{
	// step1 transpose(1, 0, 2, 3), [c_in, c_out, h, w] -> [c_out, c_in, h, w]
	int32_t c_in = conv_struct_->input_c;
	int32_t c_ou = conv_struct_->output_c;
	int32_t k_h_w = conv_struct_->weight_h * conv_struct_->weight_w;
	int32_t i_inv, o_inv, i_addr_inc, o_addr_inc, loop_num, plane_h, plane_w;
	i_inv    = k_h_w;
	o_inv    = k_h_w * c_in;
	i_addr_inc = k_h_w * c_ou;
	o_addr_inc = k_h_w;
	loop_num   = c_in;
	plane_h  = c_ou;
	plane_w  = k_h_w;

	int i, j, l;
	int8_t *src_0, *dst_0;
	for (l = 0; l < loop_num; l++)
	{
		src_0 = (int8_t *)((int8_t*)input_weight + l*i_addr_inc);
		dst_0 = (int8_t *)((int8_t*)input_weight_T + l*o_addr_inc);
		for (i = 0; i < plane_h; i++)
		{
			for (j = 0; j < plane_w; j++)
			{
				dst_0[i * o_inv + j] = src_0[i * i_inv + j];
			}
		}
	}

	// step2 flip(h, w), [c_out, c_in, h, w] -> [c_out, c_in, h_t, w_t]
	for (l = 0; l < c_in * c_ou; l++)
	{
		src_0 = (int8_t *)((int8_t *)input_weight_T + l * k_h_w);
		dst_0 = (int8_t *)((int8_t *)input_weight + l * k_h_w);
		for (i = 0; i < k_h_w; i++)
		{
			dst_0[i] = src_0[k_h_w - 1 - i];
		}
	}

	// step3 reshape for conv
	return reshape_weight_for_conv(input_weight, input_weight_T, conv_struct_);
}

int32_t reshape_weight_for_deconv_4bit(int8_t *input_weight, int8_t *input_weight_T, conv_struct_t *conv_struct_)
{
	// step1 transpose(1, 0, 2, 3), [c_in, c_out, h, w] -> [c_out, c_in, h, w]
	int32_t c_in = conv_struct_->input_c;
	int32_t c_ou = conv_struct_->output_c;
	int32_t k_h_w = conv_struct_->weight_h * conv_struct_->weight_w;
	int32_t i_inv, o_inv, i_addr_inc, o_addr_inc, loop_num, plane_h, plane_w;
	i_inv    = k_h_w;
	o_inv    = k_h_w * c_in;
	i_addr_inc = k_h_w * c_ou;
	o_addr_inc = k_h_w;
	loop_num   = c_in;
	plane_h  = c_ou;
	plane_w  = k_h_w;

	int i, j, l;
	int8_t *src_0, *dst_0;
	int32_t dst_0_oft, src_0_oft;
	memset(input_weight_T, 0, c_in * c_ou * k_h_w);
	for (l = 0; l < loop_num; l++)
	{
		src_0 = (int8_t *)((int8_t*)input_weight + l*i_addr_inc);
		dst_0 = (int8_t *)((int8_t*)input_weight_T + l*o_addr_inc);
		for (i = 0; i < plane_h; i++)
		{
			for (j = 0; j < plane_w; j++)
			{
				src_0_oft = i * i_inv + j;
				dst_0_oft = i * o_inv + j;
				if (dst_0_oft & 0x1)	//h_4bit
				{
					dst_0[dst_0_oft >> 1] |= (src_0_oft & 0x1) ? (src_0[src_0_oft >> 1] & 0xF0) : ((src_0[src_0_oft >> 1] << 4));
				}
				else
				{
					dst_0[dst_0_oft >> 1] |= (src_0_oft & 0x1) ? (src_0[src_0_oft >> 1] >> 4) : ((src_0[src_0_oft >> 1] & 0xF));
				}
			}
		}
	}

	// step2 flip(h, w), [c_out, c_in, h, w] -> [c_out, c_in, h_t, w_t]
	memset(input_weight, 0, c_in * c_ou * k_h_w);
	for (l = 0; l < c_in * c_ou; l++)
	{
		src_0 = (int8_t *)((int8_t *)input_weight_T + l * k_h_w);
		dst_0 = (int8_t *)((int8_t *)input_weight + l * k_h_w);
		for (i = 0; i < k_h_w; i+=2)
		{
			dst_0[i >> 1] = (src_0[(k_h_w - 1 - i) >> 1] << 4) | (src_0[(k_h_w - 1 - i) >> 1] >> 4);
		}
	}

	// step3 reshape for conv
	return reshape_weight_for_conv_4bit(input_weight, input_weight_T, conv_struct_);
}

/*
 * luna_cnn_math.c
 *
 *  Created on: Feb 20, 2024
 *      Author: yyzhang
 */


#include "luna/luna.h"
#include "luna/isa/luna_bits.h"
#include "luna/luna_cnn_math.h"
#include "luna/cmd/luna_cnn_cmd.h"
#include "luna_privates.h"

_FAST_DATA_ZI static luna_cnn_para_t luna_cnn_paras_;

// extern void HAL_InvalidateDCache_by_Addr(uint32_t *addr, uint32_t dsize);
// extern void HAL_FlushInvalidateDCache_by_Addr(uint32_t *addr, uint32_t dsize);

static int32_t luna_cnn2d_calculate(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int8_t *p_out, luna_cnn_static_para_t *conv_para_, uint32_t *cmd)
{
	// HAL_InvalidateDCache_by_Addr(&luna_cnn_paras_, sizeof(luna_cnn_paras_));
	// HAL_FlushInvalidateDCache_by_Addr(&luna_cnn_paras_, sizeof(luna_cnn_paras_));
	luna_cnn_paras_.cnn_static_para = conv_para_;
	if (p_in >= LUNA_FLASH_MEM_BASE)
	{
		reg_write(ADDR_LUNA_AHB_BADDR, 0x30000000);
		luna_cnn_paras_.input_addr_oft = LUNA_FLASH_ADDR_OFFSET(p_in);
	}
	else if (p_in >= LUNA_PSRAM_MEM_BASE)
	{
		reg_write(ADDR_LUNA_AHB_BADDR, 0x28000000);
		luna_cnn_paras_.input_addr_oft = LUNA_PSRAM_ADDR_OFFSET(p_in);
	}
	else
	{
		luna_cnn_paras_.input_addr_oft = LUNA_SHARE_ADDR_OFFSET(p_in);
	}

	if (p_weight >= LUNA_FLASH_MEM_BASE)
	{
		reg_write(ADDR_LUNA_AHB_BADDR, 0x30000000);
		luna_cnn_paras_.weight_addr_oft = LUNA_FLASH_ADDR_OFFSET(p_weight);
		luna_cnn_paras_.bias_addr_oft = LUNA_FLASH_ADDR_OFFSET(p_bias);
	}
	else if (p_weight >= LUNA_PSRAM_MEM_BASE)
	{
		reg_write(ADDR_LUNA_AHB_BADDR, 0x28000000);
		luna_cnn_paras_.weight_addr_oft = LUNA_PSRAM_ADDR_OFFSET(p_weight);
		luna_cnn_paras_.bias_addr_oft = LUNA_PSRAM_ADDR_OFFSET(p_bias);
	}
	else
	{
		luna_cnn_paras_.weight_addr_oft = LUNA_SHARE_ADDR_OFFSET(p_weight);
		luna_cnn_paras_.bias_addr_oft = LUNA_SHARE_ADDR_OFFSET(p_bias);
	}

	luna_cnn_paras_.output_addr_oft = LUNA_SHARE_ADDR_OFFSET(p_out);
	return luna_execute_cmd(cmd, &luna_cnn_paras_, sizeof(luna_cnn_para_t));
}

int32_t luna_conv2d_i8i4o8(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int8_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	return luna_cnn2d_calculate(p_in, p_weight, p_bias, p_out, conv_para_, luna_api_split_cnn);
}

int32_t luna_conv2d_i8i4o32(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int32_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	return luna_cnn2d_calculate(p_in, p_weight, p_bias, p_out, conv_para_, luna_api_split_cnn);
}

int32_t luna_conv2d_i8i8o8(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int8_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	return luna_cnn2d_calculate(p_in, p_weight, p_bias, p_out, conv_para_, luna_api_split_cnn);
}

int32_t luna_conv2d_i8i8o32(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int32_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	return luna_cnn2d_calculate(p_in, p_weight, p_bias, p_out, conv_para_, luna_api_split_cnn);
}

int32_t luna_depthwise2d_i8i4o8(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int8_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	return luna_cnn2d_calculate(p_in, p_weight, p_bias, p_out, conv_para_, luna_api_split_depthwise);
}

int32_t luna_depthwise2d_i8i4o32(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int32_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	return luna_cnn2d_calculate(p_in, p_weight, p_bias, p_out, conv_para_, luna_api_split_depthwise);
}

int32_t luna_depthwise2d_i8i8o8(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int8_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	return luna_cnn2d_calculate(p_in, p_weight, p_bias, p_out, conv_para_, luna_api_split_depthwise);
}

int32_t luna_depthwise2d_i8i8o32(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int32_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	return luna_cnn2d_calculate(p_in, p_weight, p_bias, p_out, conv_para_, luna_api_split_depthwise);
}


int32_t luna_deconv2d_i8i4o8(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int8_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	return luna_cnn2d_calculate(p_in, p_weight, p_bias, p_out, conv_para_, luna_api_split_deconv);
}

int32_t luna_deconv2d_i8i4o32(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int32_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	return luna_cnn2d_calculate(p_in, p_weight, p_bias, p_out, conv_para_, luna_api_split_deconv);
}

int32_t luna_deconv2d_i8i8o8(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int8_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	return luna_cnn2d_calculate(p_in, p_weight, p_bias, p_out, conv_para_, luna_api_split_deconv);
}

int32_t luna_deconv2d_i8i8o32(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int32_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	return luna_cnn2d_calculate(p_in, p_weight, p_bias, p_out, conv_para_, luna_api_split_deconv);
}


int32_t luna_max_pooling2d_i8o8(const int8_t *p_in, int8_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	luna_cnn_paras_.cnn_static_para = conv_para_;
	if (p_in >= LUNA_FLASH_MEM_BASE)
	{
		luna_cnn_paras_.input_addr_oft = LUNA_FLASH_ADDR_OFFSET(p_in);
	}
	else if (p_in >= LUNA_PSRAM_MEM_BASE)
	{
		luna_cnn_paras_.input_addr_oft = LUNA_PSRAM_ADDR_OFFSET(p_in);
	}
	else
	{
		luna_cnn_paras_.input_addr_oft = LUNA_SHARE_ADDR_OFFSET(p_in);
	}

	if (p_out >= LUNA_FLASH_MEM_BASE)
	{
		luna_cnn_paras_.output_addr_oft = LUNA_FLASH_ADDR_OFFSET(p_out);
	}
	else if (p_out >= LUNA_PSRAM_MEM_BASE)
	{
		luna_cnn_paras_.output_addr_oft = LUNA_PSRAM_ADDR_OFFSET(p_out);
	}
	else
	{
		luna_cnn_paras_.output_addr_oft = LUNA_SHARE_ADDR_OFFSET(p_out);
	}
	return luna_execute_cmd(luna_api_split_maxpool, &luna_cnn_paras_, sizeof(luna_cnn_para_t));
}

int32_t luna_mean_pooling2d_i8o8(const int8_t *p_in, int8_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	luna_cnn_paras_.cnn_static_para = conv_para_;
	if (p_in >= LUNA_FLASH_MEM_BASE)
	{
		luna_cnn_paras_.input_addr_oft = LUNA_FLASH_ADDR_OFFSET(p_in);
	}
	else if (p_in >= LUNA_PSRAM_MEM_BASE)
	{
		luna_cnn_paras_.input_addr_oft = LUNA_PSRAM_ADDR_OFFSET(p_in);
	}
	else
	{
		luna_cnn_paras_.input_addr_oft = LUNA_SHARE_ADDR_OFFSET(p_in);
	}

	if (p_out >= LUNA_FLASH_MEM_BASE)
	{
		luna_cnn_paras_.output_addr_oft = LUNA_FLASH_ADDR_OFFSET(p_out);
	}
	else if (p_out >= LUNA_PSRAM_MEM_BASE)
	{
		luna_cnn_paras_.output_addr_oft = LUNA_PSRAM_ADDR_OFFSET(p_out);
	}
	else
	{
		luna_cnn_paras_.output_addr_oft = LUNA_SHARE_ADDR_OFFSET(p_out);
	}
	return luna_execute_cmd(luna_api_split_meanpool, &luna_cnn_paras_, sizeof(luna_cnn_para_t));
}

int32_t luna_mean_pooling2d_i8o32(const int8_t *p_in, int32_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	luna_cnn_paras_.cnn_static_para = conv_para_;
	if (p_in >= LUNA_FLASH_MEM_BASE)
	{
		luna_cnn_paras_.input_addr_oft = LUNA_FLASH_ADDR_OFFSET(p_in);
	}
	else if (p_in >= LUNA_PSRAM_MEM_BASE)
	{
		luna_cnn_paras_.input_addr_oft = LUNA_PSRAM_ADDR_OFFSET(p_in);
	}
	else
	{
		luna_cnn_paras_.input_addr_oft = LUNA_SHARE_ADDR_OFFSET(p_in);
	}

	if (p_out >= LUNA_FLASH_MEM_BASE)
	{
		luna_cnn_paras_.output_addr_oft = LUNA_FLASH_ADDR_OFFSET(p_out);
	}
	else if (p_out >= LUNA_PSRAM_MEM_BASE)
	{
		luna_cnn_paras_.output_addr_oft = LUNA_PSRAM_ADDR_OFFSET(p_out);
	}
	else
	{
		luna_cnn_paras_.output_addr_oft = LUNA_SHARE_ADDR_OFFSET(p_out);
	}
	return luna_execute_cmd(luna_api_split_meanpool, &luna_cnn_paras_, sizeof(luna_cnn_para_t));
}


int32_t luna_conv1d_i8i4o8(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int8_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	return luna_cnn2d_calculate(p_in, p_weight, p_bias, p_out, conv_para_, luna_api_split_cnn);
}

int32_t luna_conv1d_i8i4o32(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int32_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	return luna_cnn2d_calculate(p_in, p_weight, p_bias, p_out, conv_para_, luna_api_split_cnn);
}

int32_t luna_conv1d_i8i8o8(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int8_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	return luna_cnn2d_calculate(p_in, p_weight, p_bias, p_out, conv_para_, luna_api_split_cnn);
}

int32_t luna_conv1d_i8i8o32(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int32_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	return luna_cnn2d_calculate(p_in, p_weight, p_bias, p_out, conv_para_, luna_api_split_cnn);
}

int32_t luna_depthwise1d_i8i4o8(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int8_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	return luna_cnn2d_calculate(p_in, p_weight, p_bias, p_out, conv_para_, luna_api_split_depthwise);
}

int32_t luna_depthwise1d_i8i4o32(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int32_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	return luna_cnn2d_calculate(p_in, p_weight, p_bias, p_out, conv_para_, luna_api_split_depthwise);
}

int32_t luna_depthwise1d_i8i8o8(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int8_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	return luna_cnn2d_calculate(p_in, p_weight, p_bias, p_out, conv_para_, luna_api_split_depthwise);
}

int32_t luna_depthwise1d_i8i8o32(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int32_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	return luna_cnn2d_calculate(p_in, p_weight, p_bias, p_out, conv_para_, luna_api_split_depthwise);
}


int32_t luna_deconv1d_i8i4o8(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int8_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	return luna_cnn2d_calculate(p_in, p_weight, p_bias, p_out, conv_para_, luna_api_split_deconv);
}

int32_t luna_deconv1d_i8i4o32(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int32_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	return luna_cnn2d_calculate(p_in, p_weight, p_bias, p_out, conv_para_, luna_api_split_deconv);
}

int32_t luna_deconv1d_i8i8o8(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int8_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	return luna_cnn2d_calculate(p_in, p_weight, p_bias, p_out, conv_para_, luna_api_split_deconv);
}

int32_t luna_deconv1d_i8i8o32(const int8_t *p_in, int8_t *p_weight, int32_t *p_bias, int32_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	return luna_cnn2d_calculate(p_in, p_weight, p_bias, p_out, conv_para_, luna_api_split_deconv);
}


int32_t luna_max_pooling1d_i8o8(const int8_t *p_in, int8_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	luna_cnn_paras_.cnn_static_para = conv_para_;
	if (p_in >= LUNA_FLASH_MEM_BASE)
	{
		luna_cnn_paras_.input_addr_oft = LUNA_FLASH_ADDR_OFFSET(p_in);
	}
	else if (p_in >= LUNA_PSRAM_MEM_BASE)
	{
		luna_cnn_paras_.input_addr_oft = LUNA_PSRAM_ADDR_OFFSET(p_in);
	}
	else
	{
		luna_cnn_paras_.input_addr_oft = LUNA_SHARE_ADDR_OFFSET(p_in);
	}

	if (p_out >= LUNA_FLASH_MEM_BASE)
	{
		luna_cnn_paras_.output_addr_oft = LUNA_FLASH_ADDR_OFFSET(p_out);
	}
	else if (p_out >= LUNA_PSRAM_MEM_BASE)
	{
		luna_cnn_paras_.output_addr_oft = LUNA_PSRAM_ADDR_OFFSET(p_out);
	}
	else
	{
		luna_cnn_paras_.output_addr_oft = LUNA_SHARE_ADDR_OFFSET(p_out);
	}
	return luna_execute_cmd(luna_api_split_maxpool, &luna_cnn_paras_, sizeof(luna_cnn_para_t));
}

int32_t luna_mean_pooling1d_i8o8(const int8_t *p_in, int8_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	luna_cnn_paras_.cnn_static_para = conv_para_;
	if (p_in >= LUNA_FLASH_MEM_BASE)
	{
		luna_cnn_paras_.input_addr_oft = LUNA_FLASH_ADDR_OFFSET(p_in);
	}
	else if (p_in >= LUNA_PSRAM_MEM_BASE)
	{
		luna_cnn_paras_.input_addr_oft = LUNA_PSRAM_ADDR_OFFSET(p_in);
	}
	else
	{
		luna_cnn_paras_.input_addr_oft = LUNA_SHARE_ADDR_OFFSET(p_in);
	}

	if (p_out >= LUNA_FLASH_MEM_BASE)
	{
		luna_cnn_paras_.output_addr_oft = LUNA_FLASH_ADDR_OFFSET(p_out);
	}
	else if (p_out >= LUNA_PSRAM_MEM_BASE)
	{
		luna_cnn_paras_.output_addr_oft = LUNA_PSRAM_ADDR_OFFSET(p_out);
	}
	else
	{
		luna_cnn_paras_.output_addr_oft = LUNA_SHARE_ADDR_OFFSET(p_out);
	}
	return luna_execute_cmd(luna_api_split_meanpool, &luna_cnn_paras_, sizeof(luna_cnn_para_t));
}

int32_t luna_mean_pooling1d_i8o32(const int8_t *p_in, int32_t *p_out, luna_cnn_static_para_t *conv_para_)
{
	luna_cnn_paras_.cnn_static_para = conv_para_;
	if (p_in >= LUNA_FLASH_MEM_BASE)
	{
		luna_cnn_paras_.input_addr_oft = LUNA_FLASH_ADDR_OFFSET(p_in);
	}
	else if (p_in >= LUNA_PSRAM_MEM_BASE)
	{
		luna_cnn_paras_.input_addr_oft = LUNA_PSRAM_ADDR_OFFSET(p_in);
	}
	else
	{
		luna_cnn_paras_.input_addr_oft = LUNA_SHARE_ADDR_OFFSET(p_in);
	}

	if (p_out >= LUNA_FLASH_MEM_BASE)
	{
		luna_cnn_paras_.output_addr_oft = LUNA_FLASH_ADDR_OFFSET(p_out);
	}
	else if (p_out >= LUNA_PSRAM_MEM_BASE)
	{
		luna_cnn_paras_.output_addr_oft = LUNA_PSRAM_ADDR_OFFSET(p_out);
	}
	else
	{
		luna_cnn_paras_.output_addr_oft = LUNA_SHARE_ADDR_OFFSET(p_out);
	}
	return luna_execute_cmd(luna_api_split_meanpool, &luna_cnn_paras_, sizeof(luna_cnn_para_t));
}


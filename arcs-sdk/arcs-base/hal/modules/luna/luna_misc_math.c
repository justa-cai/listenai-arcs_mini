#include "luna/luna.h"
#include "luna/isa/luna_bits.h"
#include "luna/luna_misc_math.h"
#include "luna/cmd/luna_misc_cmd.h"
#include "luna_privates.h"

_FAST_DATA_ZI static LunaMiscActvParams_t	luna_misc_actv_params;
_FAST_DATA_ZI static LunaMiscMemParams_t 	luna_misc_mem_params;
_FAST_DATA_ZI static LunaMiscLutParams_t	luna_misc_lut_params;
_FAST_DATA_ZI static LunaExpParams_t		luna_misc_exp_params;
_FAST_DATA_ZI static LunaSoftmaxParams_t	luna_misc_softmax_params;
_FAST_DATA_VI static LunaMemcopyParams_t 	luna_misc_mem_new_params;

static uint32_t floor_u32(uint32_t x)
{
	return x;
}

static uint32_t mod_u32(uint32_t x, uint32_t m)
{
	return (x % m);
}

static uint32_t ceil_u32(uint32_t x, uint32_t m)
{
	return (x + (m-1)) / (m);
}

static int32_t luna_exp_convert_param(uint32_t i_addr, uint32_t o_addr, uint32_t i_num, LunaExpParams_t *p_settings)
{
	uint32_t t0, t1, t2, t3;
	uint32_t in_num, in_mask, ou_num, r8_value_low, r9_value_low;
	const uint32_t intval=0;

	i_addr = (uintptr_t)LUNA_SHARE_ADDR_OFFSET(i_addr);
	o_addr = (uintptr_t)LUNA_SHARE_ADDR_OFFSET(o_addr);
	in_num = ceil_u32(i_num * 32, 64);
	in_mask= mod_u32(i_num * 32, 64)/8;
	ou_num = i_num * 4;

	r8_value_low = mod_u32(in_num,256)+mod_u32(intval,256)*(1<<8);
	r9_value_low = floor_u32(in_num/256)+floor_u32(intval/256)*(1<<8);

	p_settings->src = i_addr;
	p_settings->dst = o_addr;
	p_settings->size = i_num;

	p_settings->master_s0_r8_L = r8_value_low;
	p_settings->master_s0_r9_L = r9_value_low;
	// uint32_t master_s0_r10_L;
	p_settings->master_s0_r14 = i_addr;
	// uint32_t master_s2_r10_L;
	p_settings->master_s2_r14 = o_addr;
	p_settings->master_r15_L = (0+in_mask*(1<<4))*(1<<8);
	p_settings->iow_r20 = o_addr;
	p_settings->iow_r18 = ou_num;
	// uint32_t iow_r19;
	// uint32_t iow_r21;
	return 0;
}

static int32_t luna_softmax_convert_param(uint32_t i_addr, uint32_t o_addr, uint32_t length, LunaSoftmaxParams_t *p_settings)
{
	if (i_addr >= LUNA_FLASH_MEM_BASE) {
		reg_write(ADDR_LUNA_AHB_BADDR, 0x30000000);
		i_addr = (uintptr_t)LUNA_FLASH_ADDR_OFFSET(i_addr);	
		p_settings->src_sel = 1;
	} else if (i_addr >= LUNA_PSRAM_MEM_BASE) {
		reg_write(ADDR_LUNA_AHB_BADDR, 0x28000000);
		i_addr = (uintptr_t)LUNA_PSRAM_ADDR_OFFSET(i_addr);	
		p_settings->src_sel = 1;
	} else {
		i_addr = (uintptr_t)LUNA_SHARE_ADDR_OFFSET(i_addr);
		p_settings->src_sel = 0;
	}
	o_addr = (uintptr_t)LUNA_SHARE_ADDR_OFFSET(o_addr);

	p_settings->i_addr = i_addr;
	p_settings->o_addr = o_addr;
	p_settings->length = length;
	p_settings->in_num = ceil_u32(length*32,64);

	p_settings->in_num_offs = mod_u32(length*32,64)/32;
	p_settings->in_mask = mod_u32(length*32,64)/8;
	p_settings->r8_value_low = mod_u32(p_settings->in_num,256);
	p_settings->r9_value_low = floor_u32(p_settings->in_num/256);

	p_settings->ou_num = length*4;

	p_settings->maxmin_pe_r26 = ((0x0002) << 16) | ((4+2+floor_u32(p_settings->in_num_offs/4)) << 12) | (((mod_u32(p_settings->in_num_offs,4))*4)<<8) | 0x22;
	p_settings->maxmin_pe_27 = p_settings->in_num-1;
	p_settings->sum_master_s3_r15_L = p_settings->in_mask*(1<<12);
	p_settings->sum_pe_r27 = ceil_u32(length,2)-1;
	p_settings->scale_master_s3_r15_L = p_settings->in_mask*(1<<12);
	p_settings->scale_pe_r23_H = 38;

	return 0;
}

static int32_t luna_memcpy_convert_param(uint32_t i_addr, uint32_t o_addr, uint32_t length, LunaMemcopyParams_t *p_settings)
{
	uint32_t in_num = ceil_u32(length,8);
	uint32_t addr_intv = ceil_u32(floor_u32(in_num/256),2)*256*8 + mod_u32(in_num,256)*8;

	if (i_addr >= LUNA_FLASH_MEM_BASE) {
		reg_write(ADDR_LUNA_AHB_BADDR, 0x30000000);
		p_settings->i_addr = LUNA_FLASH_ADDR_OFFSET(i_addr);
		p_settings->src_sel = 1;
	}
	else if (i_addr >= LUNA_PSRAM_MEM_BASE) {
		reg_write(ADDR_LUNA_AHB_BADDR, 0x28000000);
		p_settings->i_addr = LUNA_PSRAM_ADDR_OFFSET(i_addr);
		p_settings->src_sel = 1;
	}
	else {
		p_settings->i_addr = LUNA_SHARE_ADDR_OFFSET(i_addr);
		p_settings->src_sel = 0;
	}
	int32_t ret = 0;
	p_settings->o_addr = LUNA_SHARE_ADDR_OFFSET(o_addr);
	p_settings->length = length;

	p_settings->master_s0_r8 = ((2)<<16)|(mod_u32(in_num,256));
	if ((mod_u32(addr_intv,65536) > 32767)) {
		p_settings->master_s0_r9 = ((1)<<16)|(ceil_u32(floor_u32(in_num/256),2));
	} else  {
		p_settings->master_s0_r9 = ((0)<<16)|(ceil_u32(floor_u32(in_num/256),2));
	}
	if(addr_intv <= 16383) {
        p_settings->master_s0_r10 = ((mod_u32(addr_intv,256)*256)<<16)|(8);
		p_settings->master_s0_r12 = ((floor_u32(addr_intv/256)*256)<<16)|(0);
	} else {
		p_settings->master_s0_r10 = ((mod_u32(addr_intv,256)*(1<<8) + floor_u32(addr_intv/65536))<<16)|(8);
		if((mod_u32(addr_intv,65536) > 32767)) {
			p_settings->master_s0_r12 = ((floor_u32(mod_u32(addr_intv,65536)/256)*(1<<8) + 1)<<16)|(0);
		} else {
			p_settings->master_s0_r12 = ((floor_u32(mod_u32(addr_intv,65536)/256)*(1<<8))<<16)|(0);
		}
	}
	p_settings->master_s0_r14 = p_settings->i_addr;
	p_settings->iow_r20 = p_settings->o_addr;
	p_settings->iow_r18 = length;
	p_settings->iow_r19 = 0x1;
	p_settings->iow_r21 = 0x0;

	return 0;
}

int32_t luna_memcpy_i8o8(int8_t* dst, int8_t* src, uint32_t size)
{
	int ret = luna_memcpy_convert_param(src, dst, size, &luna_misc_mem_new_params);
	ret = luna_execute_cmd(luna_api_psrammemcpy, &luna_misc_mem_new_params, sizeof(luna_misc_mem_new_params));
	return ret;
}

int32_t luna_psrammemcpy_i8o8(int8_t* dst, int8_t* src, uint32_t size)
{
	int ret = luna_memcpy_convert_param(src, dst, size, &luna_misc_mem_new_params);
	ret = luna_execute_cmd(luna_api_psrammemcpy, &luna_misc_mem_new_params, sizeof(luna_misc_mem_new_params));
	return ret;
}

int32_t luna_memset_i8o8(int8_t *dst, int8_t value, uint32_t size)
{
	int32_t ret = 0;
	luna_misc_mem_params.dst = LUNA_SHARE_ADDR_OFFSET(dst);
	luna_misc_mem_params.size = size;
	luna_misc_mem_params.value = value&0xFF;
	luna_misc_mem_params.inbits = 8;
	ret = luna_execute_cmd(luna_api_memset, &luna_misc_mem_params, sizeof(LunaMiscMemParams_t));
	return ret;
}

int32_t luna_memset_i16o16(int16_t *dst, int16_t value, uint32_t size)
{
	int32_t ret = 0;
	luna_misc_mem_params.dst = LUNA_SHARE_ADDR_OFFSET(dst);
	luna_misc_mem_params.size = size;
	luna_misc_mem_params.value = value&0xFFFF;
	luna_misc_mem_params.inbits = 16;
	ret = luna_execute_cmd(luna_api_memset, &luna_misc_mem_params, sizeof(LunaMiscMemParams_t));
	return ret;
}

int32_t luna_memset_i32o32(int32_t *dst, int32_t value, uint32_t size)
{
	int32_t ret = 0;
	luna_misc_mem_params.dst = LUNA_SHARE_ADDR_OFFSET(dst);
	luna_misc_mem_params.size = size;
	luna_misc_mem_params.value = value&0xFFFFFFFF;
	luna_misc_mem_params.inbits = 32;
	ret = luna_execute_cmd(luna_api_memset, &luna_misc_mem_params, sizeof(LunaMiscMemParams_t));
	return ret;
}

int32_t luna_relu_i8o8(const int8_t *src, int8_t *dst, uint32_t size, uint32_t shift)
{

	luna_misc_actv_params.src = LUNA_SHARE_ADDR_OFFSET(src);
	luna_misc_actv_params.dst = LUNA_SHARE_ADDR_OFFSET(dst);
	luna_misc_actv_params.size = size;
	luna_misc_actv_params.pos_shift = shift;
	luna_misc_actv_params.neg_shift = 63;
	luna_misc_actv_params.inout_bits = (8<<8)|(8);
	return luna_execute_cmd(luna_api_activate, &luna_misc_actv_params, sizeof(LunaMiscActvParams_t));
}

int32_t luna_relu_i8o32(const int8_t *src, int32_t *dst, uint32_t size, uint32_t shift)
{
	luna_misc_actv_params.src = LUNA_SHARE_ADDR_OFFSET(src);
	luna_misc_actv_params.dst = LUNA_SHARE_ADDR_OFFSET(dst);
	luna_misc_actv_params.size = size;
	luna_misc_actv_params.pos_shift = shift;
	luna_misc_actv_params.neg_shift = 63;
	luna_misc_actv_params.inout_bits = (32<<8)|(8);
	return luna_execute_cmd(luna_api_activate, &luna_misc_actv_params, sizeof(LunaMiscActvParams_t));
}

int32_t luna_relu_i32o8(const int32_t *src, int8_t *dst, uint32_t size, uint32_t shift)
{
	luna_misc_actv_params.src = LUNA_SHARE_ADDR_OFFSET(src);
	luna_misc_actv_params.dst = LUNA_SHARE_ADDR_OFFSET(dst);
	luna_misc_actv_params.size = size;
	luna_misc_actv_params.pos_shift = shift;
	luna_misc_actv_params.neg_shift = 63;
	luna_misc_actv_params.inout_bits = (8<<8)|(32);
	return luna_execute_cmd(luna_api_activate, &luna_misc_actv_params, sizeof(LunaMiscActvParams_t));
}

int32_t luna_relu_i32o32(const int32_t *src, int32_t *dst, uint32_t size, uint32_t shift)
{
	luna_misc_actv_params.src = LUNA_SHARE_ADDR_OFFSET(src);
	luna_misc_actv_params.dst = LUNA_SHARE_ADDR_OFFSET(dst);
	luna_misc_actv_params.size = size;
	luna_misc_actv_params.pos_shift = shift;
	luna_misc_actv_params.neg_shift = 63;
	luna_misc_actv_params.inout_bits = (32<<8)|(32);
	return luna_execute_cmd(luna_api_activate, &luna_misc_actv_params, sizeof(LunaMiscActvParams_t));
}

int32_t luna_prelu_i8o8(const int8_t *src, uint32_t slope, int8_t *dst, uint32_t size, uint32_t shift)
{
	luna_misc_actv_params.src = LUNA_SHARE_ADDR_OFFSET(src);
	luna_misc_actv_params.dst = LUNA_SHARE_ADDR_OFFSET(dst);
	luna_misc_actv_params.size = size;
	luna_misc_actv_params.pos_shift = shift;
	luna_misc_actv_params.neg_shift = slope + shift;
	luna_misc_actv_params.inout_bits = (8<<8)|(8);
	return luna_execute_cmd(luna_api_activate, &luna_misc_actv_params, sizeof(LunaMiscActvParams_t));
}

int32_t luna_prelu_i8o32(const int8_t *src, uint32_t slope, int32_t *dst, uint32_t size, uint32_t shift)
{
	luna_misc_actv_params.src = LUNA_SHARE_ADDR_OFFSET(src);
	luna_misc_actv_params.dst = LUNA_SHARE_ADDR_OFFSET(dst);
	luna_misc_actv_params.size = size;
	luna_misc_actv_params.pos_shift = shift;
	luna_misc_actv_params.neg_shift = slope + shift;
	luna_misc_actv_params.inout_bits = (32<<8)|(8);
	return luna_execute_cmd(luna_api_activate, &luna_misc_actv_params, sizeof(LunaMiscActvParams_t));
}

int32_t luna_prelu_i32o8(const int32_t *src, uint32_t slope, int8_t *dst, uint32_t size, uint32_t shift)
{
	luna_misc_actv_params.src = LUNA_SHARE_ADDR_OFFSET(src);
	luna_misc_actv_params.dst = LUNA_SHARE_ADDR_OFFSET(dst);
	luna_misc_actv_params.size = size;
	luna_misc_actv_params.pos_shift = shift;
	luna_misc_actv_params.neg_shift = slope + shift;
	luna_misc_actv_params.inout_bits = (8<<8)|(32);
	return luna_execute_cmd(luna_api_activate, &luna_misc_actv_params, sizeof(LunaMiscActvParams_t));
}

int32_t luna_prelu_i32o32(const int32_t *src, uint32_t slope, int32_t *dst, uint32_t size, uint32_t shift)
{
	luna_misc_actv_params.src = LUNA_SHARE_ADDR_OFFSET(src);
	luna_misc_actv_params.dst = LUNA_SHARE_ADDR_OFFSET(dst);
	luna_misc_actv_params.size = size;
	luna_misc_actv_params.pos_shift = shift;
	luna_misc_actv_params.neg_shift = slope + shift;
	luna_misc_actv_params.inout_bits = (32<<8)|(32);
	return luna_execute_cmd(luna_api_activate, &luna_misc_actv_params, sizeof(LunaMiscActvParams_t));
}


int32_t luna_relux_i8o8(const int8_t *src, int8_t x, int8_t *dst, uint32_t size, uint32_t shift)
{
	luna_misc_actv_params.src = LUNA_SHARE_ADDR_OFFSET(src);
	luna_misc_actv_params.dst = LUNA_SHARE_ADDR_OFFSET(dst);
	luna_misc_actv_params.size = size;
	luna_misc_actv_params.pos_shift = (((shift)+(63<<8)+(1<<15))<<16)|(0x0800);
	luna_misc_actv_params.neg_shift = ((x));
	luna_misc_actv_params.inout_bits = (8<<8)|(8);
	return luna_execute_cmd(luna_api_activate_relux, &luna_misc_actv_params, sizeof(LunaMiscActvParams_t));
}

int32_t luna_relux_i32o8(const int32_t *src, const int8_t x, int8_t *dst, uint32_t size, uint32_t shift)
{
	luna_misc_actv_params.src = LUNA_SHARE_ADDR_OFFSET(src);
	luna_misc_actv_params.dst = LUNA_SHARE_ADDR_OFFSET(dst);
	luna_misc_actv_params.size = size;
	luna_misc_actv_params.pos_shift = (((shift)+(63<<8)+(1<<15))<<16)|(0x0800);
	luna_misc_actv_params.neg_shift = ((x));
	luna_misc_actv_params.inout_bits = (8<<8)|(32);
	return luna_execute_cmd(luna_api_activate_relux, &luna_misc_actv_params, sizeof(LunaMiscActvParams_t));
}

int32_t luna_sigmoid_i32o32(const int32_t *src, int32_t *dst, uint32_t size)
{
	int32_t ret = 0;
	if (src >= LUNA_FLASH_MEM_BASE) {
		reg_write(ADDR_LUNA_AHB_BADDR, 0x30000000);
		luna_misc_lut_params.src = (uintptr_t)LUNA_FLASH_ADDR_OFFSET(src);	
		luna_misc_lut_params.src_sel = 1;
	} else if (src >= LUNA_PSRAM_MEM_BASE) {
		reg_write(ADDR_LUNA_AHB_BADDR, 0x28000000);
		luna_misc_lut_params.src = (uintptr_t)LUNA_PSRAM_ADDR_OFFSET(src);	
		luna_misc_lut_params.src_sel = 1;
	} else {
		luna_misc_lut_params.src = (uintptr_t)LUNA_SHARE_ADDR_OFFSET(src);
		luna_misc_lut_params.src_sel = 0;
	}
	luna_misc_lut_params.dst = LUNA_SHARE_ADDR_OFFSET(dst);
	luna_misc_lut_params.size = size;
	luna_misc_lut_params.type = 0;
	luna_misc_lut_params.inout_bits= (32<<8)|32;

	ret = luna_execute_cmd(luna_api_lut, &luna_misc_lut_params, sizeof(luna_misc_lut_params));

	return ret;
}

int32_t luna_tanh_i32o32(const int32_t *src, int32_t *dst, uint32_t size)
{
	int32_t ret = 0;
	if (src >= LUNA_FLASH_MEM_BASE) {
		reg_write(ADDR_LUNA_AHB_BADDR, 0x30000000);
		luna_misc_lut_params.src = (uintptr_t)LUNA_FLASH_ADDR_OFFSET(src);	
		luna_misc_lut_params.src_sel = 1;
	} else if (src >= LUNA_PSRAM_MEM_BASE) {
		reg_write(ADDR_LUNA_AHB_BADDR, 0x28000000);
		luna_misc_lut_params.src = (uintptr_t)LUNA_PSRAM_ADDR_OFFSET(src);	
		luna_misc_lut_params.src_sel = 1;
	} else {
		luna_misc_lut_params.src = (uintptr_t)LUNA_SHARE_ADDR_OFFSET(src);
		luna_misc_lut_params.src_sel = 0;
	}
	luna_misc_lut_params.dst = LUNA_SHARE_ADDR_OFFSET(dst);
	luna_misc_lut_params.size = size;
	luna_misc_lut_params.type = 1;
	luna_misc_lut_params.inout_bits= (32<<8)|32;

	ret = luna_execute_cmd(luna_api_lut, &luna_misc_lut_params, sizeof(luna_misc_lut_params));

	return 0;
}

int32_t luna_sigmoid_i32o8(const int32_t *src, int8_t *dst, uint32_t size)
{
	int32_t ret = 0;
	if (src >= LUNA_FLASH_MEM_BASE) {
		reg_write(ADDR_LUNA_AHB_BADDR, 0x30000000);
		luna_misc_lut_params.src = (uintptr_t)LUNA_FLASH_ADDR_OFFSET(src);	
		luna_misc_lut_params.src_sel = 1;
	} else if (src >= LUNA_PSRAM_MEM_BASE) {
		reg_write(ADDR_LUNA_AHB_BADDR, 0x28000000);
		luna_misc_lut_params.src = (uintptr_t)LUNA_PSRAM_ADDR_OFFSET(src);	
		luna_misc_lut_params.src_sel = 1;
	} else {
		luna_misc_lut_params.src = (uintptr_t)LUNA_SHARE_ADDR_OFFSET(src);
		luna_misc_lut_params.src_sel = 0;
	}
	luna_misc_lut_params.dst = LUNA_SHARE_ADDR_OFFSET(dst);
	luna_misc_lut_params.size = size;
	luna_misc_lut_params.type = 0;
	luna_misc_lut_params.inout_bits= (8<<8)|32;

	ret = luna_execute_cmd(luna_api_lut, &luna_misc_lut_params, sizeof(luna_misc_lut_params));

	return ret;
}

int32_t luna_tanh_i32o8(const int32_t *src, int8_t *dst, uint32_t size)
{
	int32_t ret = 0;
	if (src >= LUNA_FLASH_MEM_BASE) {
		reg_write(ADDR_LUNA_AHB_BADDR, 0x30000000);
		luna_misc_lut_params.src = (uintptr_t)LUNA_FLASH_ADDR_OFFSET(src);	
		luna_misc_lut_params.src_sel = 1;
	} else if (src >= LUNA_PSRAM_MEM_BASE) {
		reg_write(ADDR_LUNA_AHB_BADDR, 0x28000000);
		luna_misc_lut_params.src = (uintptr_t)LUNA_PSRAM_ADDR_OFFSET(src);	
		luna_misc_lut_params.src_sel = 1;
	} else {
		luna_misc_lut_params.src = (uintptr_t)LUNA_SHARE_ADDR_OFFSET(src);
		luna_misc_lut_params.src_sel = 0;
	}
	luna_misc_lut_params.dst = LUNA_SHARE_ADDR_OFFSET(dst);
	luna_misc_lut_params.size = size;
	luna_misc_lut_params.type = 1;
	luna_misc_lut_params.inout_bits= (8<<8)|32;

	ret = luna_execute_cmd(luna_api_lut, &luna_misc_lut_params, sizeof(luna_misc_lut_params));

	return 0;
}

int32_t luna_exp_i32o32(const int32_t *src, int32_t *dst, uint32_t size)
{
	int ret = 0;
	int size_0 = 0, stride = 2048;

	for (int offset = 0; offset < size; offset += stride) {
		size_0 =  size - offset; 
		if (size_0 > stride) {
			size_0 = stride;
		}
		ret = luna_exp_convert_param((uintptr_t)(src + offset), (uintptr_t)(dst + offset), size_0, &luna_misc_exp_params);
		ret |= luna_execute_cmd(luna_api_exp, &luna_misc_exp_params, sizeof(luna_misc_exp_params));
	}

	return ret;
}


int32_t luna_softmax_i32o32(const int32_t *src, int32_t *dst, uint32_t size)
{
	int ret = 0;
	ret = luna_softmax_convert_param((uintptr_t)src, (uintptr_t)dst, size, &luna_misc_softmax_params);
	ret |= luna_execute_cmd(luna_api_softmax, &luna_misc_softmax_params, sizeof(luna_misc_softmax_params));
	return ret;
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


int32_t luna_logsoftmax_i32o32(const int32_t *src, int32_t *dst, uint32_t size)
{
	int32_t ret = 0;
	int32_t E_MAX = 0x80000000;
	int32_t E_SUM = 0;
	int32_t LOG_SUM = 0;
	
	ret |= luna_max_i32o32(src, dst, size); 
	E_MAX = *dst;
	if (E_MAX == (int32_t)0x80000000) {
		E_MAX += 1;
	}
	ret |= luna_offset_i32i32o32(src, -E_MAX, dst, size, 0);
	ret |= luna_exp_i32o32(dst, dst, size); 
	ret |= luna_vector_sum_i32o32(dst, dst, size, 8); 
	E_SUM = *dst; 
	LOG_SUM = ln_q15q15(E_SUM, 15);
	ret |= luna_offset_i32i32o32(src, -E_MAX, dst, size, 10);
	ret |= luna_offset_i32i32o32(dst, -LOG_SUM, dst, size, 0);

  	return ret;
}

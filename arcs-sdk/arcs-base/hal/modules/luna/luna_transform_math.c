#include "luna/luna.h"
#include "luna/luna_transform_math.h"
#include "luna/cmd/luna_fft_cmd.h"
#include "luna_privates.h"

_FAST_DATA_ZI static LunaFFTSettings_t luna_fft_params;


static uint32_t mod(uint32_t x, uint32_t m)
{
	return (x & m);
}

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


static int ones_32(uint32_t n)
{
	unsigned int c = 0;
	for (c = 0; n; ++c)
	{
		n &= (n - 1); 
	}
	return c;
}

static uint32_t floor_log2_32(uint32_t x)
{
	x |= (x >> 1);
	x |= (x >> 2);
	x |= (x >> 4);
	x |= (x >> 8);
	x |= (x >> 16);

	return (ones_32(x >> 1));
}

static int32_t luna_fft_convert_param(uint32_t i_addr, 
	uint32_t o_addr, 
	uint32_t fft_length, 
	uint32_t fft_log2, 
	uint32_t shift, 
	uint32_t complex,
	uint32_t mode0, 
	uint32_t mode1, 
	uint32_t mode2,
	LunaFFTSettings_t *p_settings)
{
	i_addr = LUNA_SHARE_ADDR_OFFSET(i_addr);
	o_addr = LUNA_SHARE_ADDR_OFFSET(o_addr);
	//shift = LUNA_SHARE_ADDR_OFFSET(shift);

	p_settings->i_addr = i_addr;  //0
	p_settings->o_addr = o_addr;
	p_settings->fft_length = fft_length;
	p_settings->fft_log2 = fft_log2;
	p_settings->shift = shift; 	//16
	p_settings->mode0 = mode0;   // mode >= 1, {'FFT','IFFT','CR256-FFT','CR256-IFFT','CR257-FFT','CR257-IFFT'};
	p_settings->mode1 = mode1;   // mode >= 1, {'NORMAL','SHORTEN','REALONLY'};
	p_settings->mode2 = mode2;	  // mode >= 1, {'SHIFT'};
	if (mode2) {
		p_settings->left_shift = 20-fft_log2+(*((uint32_t*)shift));
		p_settings->right_shift = 20<<16;  //H
	} else {
		p_settings->left_shift = 20;
		p_settings->right_shift = 20<<16; //H
	}
	
	//load input data
	if (1==mode0||2==mode0) {
		p_settings->lid_master_s0_r8_L = mod_u32(fft_length,256); //0
		p_settings->lid_master_s0_r9_L = floor_u32(fft_length/256);
	} else {
		p_settings->lid_master_s0_r8_L = mod_u32(fft_length/2,256); //0
		p_settings->lid_master_s0_r9_L = floor_u32(fft_length/2/256);
	}
	if (complex) {
		p_settings->lid_master_s0_r10_L = 8;
		p_settings->lid_master_s0_r15_L = 0x0;
	} else {
		p_settings->lid_master_s0_r10_L = 4;
		p_settings->lid_master_s0_r15_L = 0x4012;
	}
	
	p_settings->lid_master_s0_r14 = i_addr; //16
	p_settings->lid_slave0_r18 = (0x0<<16)|(fft_length*2/2);
	p_settings->lid_slave0_r19 = 0x1;
	p_settings->lid_slave0_r20 = 0x2;

	p_settings->lid_slave0_r21 = 0x0; //32
	//conj
	p_settings->lidc_master_s0_r8_L = mod_u32(fft_length/2,256);
	p_settings->lidc_master_s0_r9_L = floor_u32(fft_length/2/256);
	p_settings->lidc_master_s0_r10_L = 0xf8;

	p_settings->lidc_master_s0_12_L = 0xff; //48

	p_settings->lidc_master_s0_r14 = i_addr+4*fft_length+7;
	p_settings->lidc_master_s0_r17_H = 0x4000;
	if (3==mode0||4==mode0){
		p_settings->lidc_seti_mem_r12 = 0;
	}
	// if (5==mode0||6==mode0){ 	//TODO: other ????
	// 	p_settings->lidc_seti_mem_r12 = x0_mid;  //unused
	// } 

	//pe
	p_settings->cc_pe_r23 = 0; //64 unused
	if (3==mode0||4==mode0||5==mode0||6==mode0){
		p_settings->cc_pe_r25 = (0x3030<<16)|(0x3030); 
	} else {
		p_settings->cc_pe_r25 = (0x0c0c<<16)|(0x0c0c); 
	}
	
	p_settings->cc_pe_r26 = 0; //unused
	if (2==mode0||4==mode0||6==mode0) {
		p_settings->cc_pe_r15 = 0x0040; 
	} else {
		p_settings->cc_pe_r15 = 0;
	}

	p_settings->cc_slave0_r18 = fft_length*2/2;  //80
	p_settings->cc_master_s0_r8 = 0;
	p_settings->cc_master_s0_r9 = 0;
	p_settings->cc_master_s2_r10 = 0; 

	//output
	if (1==mode1) {
		p_settings->so_master_s0_r8 = ((mod_u32(fft_length/4,256)+2*256)<<16)|(mod_u32(fft_length/2,256)); //96
		p_settings->so_master_s0_r9 = ((floor_u32(fft_length/4/256))<<16)|(floor_u32(fft_length/2/256));
	} else if (3==mode1) {
		p_settings->so_master_s0_r8 = ((mod_u32(fft_length/4,256)+2*256)<<16)|(2+mod_u32(fft_length/2,256)*256); //96
		p_settings->so_master_s0_r9 = ((floor_u32(fft_length/4/256)<<16)|(0+floor_u32(fft_length/2/256)*256));
	} else {
		p_settings->so_master_s0_r8 = ((mod_u32(fft_length/4,256)+2*256)<<16)|(mod_u32(fft_length/2/2,256)); //96
		p_settings->so_master_s0_r9 = ((floor_u32(fft_length/4/256))<<16)|(floor_u32(fft_length/2/2/256));
	}
	if (3==mode1) {
		p_settings->so_master_s2_r10 = (1<<13)/fft_length*256;
	} else {
		p_settings->so_master_s2_r10 = (1<<13)/fft_length;
	}
	
	p_settings->so_master_s2_r14 = 0;

	if (3==mode1) {
		p_settings->so_master_s3_r16_L = 0x4004; //112
	} else {
		p_settings->so_master_s3_r16_L = 0x4000; //112
	}

	p_settings->so_master_s3_r15_L = 0; //unused
	p_settings->so_iow_r20 = o_addr;  
	if (1==mode1) {
		p_settings->so_iow_18 = fft_length*8;
	} else {
		p_settings->so_iow_18 = fft_length/2*8;
	}
	p_settings->so_iow_19 = 1;	//128
	p_settings->so_iow_21 = 0;

	return 0;
}

int32_t luna_cfft_i32o32(const int32_t *src, int32_t *dst, int32_t *fft_rshift, e_fft_points points)
{
	int ret;

	ret = luna_fft_convert_param(src, dst, points, floor_log2_32(points), fft_rshift, 1, 1, 1, 0, &luna_fft_params);
	ret |= luna_execute_cmd(luna_fft_cmd, (void *)&luna_fft_params, sizeof(luna_fft_params));

	return 0;
}

int32_t luna_cifft_i32o32(const int32_t *src, int32_t *dst, int32_t *fft_rshift, e_fft_points points)
{
	int ret;
	if (*fft_rshift >= 0) {
		ret = luna_fft_convert_param(src, dst, points, floor_log2_32(points), fft_rshift, 1, 2, 1, 1, &luna_fft_params);
	} else {
		ret = luna_fft_convert_param(src, dst, points, floor_log2_32(points), fft_rshift, 1, 2, 1, 0, &luna_fft_params);
	}
	ret |= luna_execute_cmd(luna_fft_cmd, (void *)&luna_fft_params, sizeof(luna_fft_params));
	return ret;
}

int32_t luna_rfft_i32o32(const int32_t *src, int32_t *dst, int32_t *fft_rshift, e_fft_points points)
{
	int ret;
	ret = luna_fft_convert_param(src, dst, points, floor_log2_32(points), fft_rshift, 0, 1, 2, 0, &luna_fft_params);
	ret |= luna_execute_cmd(luna_fft_cmd, (void *)&luna_fft_params, sizeof(luna_fft_params));
	return ret;
}

int32_t luna_rifft_i32o32(const int32_t *src, int32_t *dst, int32_t *fft_rshift, e_fft_points points)
{
	int ret;
	if (*fft_rshift >= 0) {
		ret = luna_fft_convert_param(src, dst, points, floor_log2_32(points), fft_rshift, 1, 4, 3, 1, &luna_fft_params);
	} else {
		ret = luna_fft_convert_param(src, dst, points, floor_log2_32(points), fft_rshift, 1, 4, 3, 0, &luna_fft_params);
	}
	ret |= luna_execute_cmd(luna_fft_cmd, (void *)&luna_fft_params, sizeof(luna_fft_params));
	return ret;
}
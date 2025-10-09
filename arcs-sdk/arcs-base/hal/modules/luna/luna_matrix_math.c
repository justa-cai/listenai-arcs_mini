#include "luna/luna.h"
#include "luna/luna_matrix_math.h"
#include "luna/cmd/luna_matrix_cmd.h"
#include "luna_privates.h"
#include "luna/isa/luna_bits.h"

//#define LUNA_TEST 

#if defined(LUNA_TEST)

// for test (N <= 512, 2*512*1 = 1K, 2*512*4 = 4K)
#define MATRIX_L_SIZE (4*1024)
// for test (N <= 512, 4*512*1 = 2K, 2*512*4 = 4K)
#define MATRIX_R_SIZE (4*1024)
// for test (col <= 512, 8*512*1 = 4K, 2*512*4 = 4K)
#define MATRIX_TRANS_SIZE (4*1024) 

#else   

#define MATRIX_L_SIZE (8*1024)
#define MATRIX_R_SIZE (16*1024)
#define MATRIX_TRANS_SIZE (16*1024) 

#endif 

//
_FAST_DATA_ZI  static LunaMatrixSettings_t luna_mat_mul_params;
_FAST_DATA_ZI  static LunaMatrixTransposeSettings_t luna_mat_trans_params;
_FAST_DATA_ZI  static LunaMatrixCopySettings_t luna_mat_copy_params;

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

static int32_t luna_mat_mul_convert_param(	
	uint32_t i_addr_m0,
	uint32_t i_addr_m1,
	uint32_t i_addr_bias,
	uint32_t o_addr,
	uint32_t M,
	uint32_t N,
	uint32_t L,
	uint32_t i_precision,
	uint32_t o_precision,  
	uint32_t i_interval0,
	uint32_t i_interval1,
	uint32_t o_interval,
	uint32_t o_rshift,
	uint32_t loop_num_0,
	uint32_t loop_num_1,
	uint32_t i_addr_m0_inc,
	uint32_t i_addr_m1_inc,
	uint32_t i_addr_bias_inc,
	uint32_t o_addr_inc_0,
	uint32_t o_addr_inc_1,
	uint32_t m0_bit4_en,
	uint32_t m1_bit4_en,
	uint32_t bias_en,
	LunaMatrixSettings_t *p_settings)
{
	uint32_t ou_type, outlayer_mode;
	uint32_t s_cnt, t_cnt, r_cnt, h_cnt, r_addr, h_addr, s_rota, h_rota, dp_w, dp_c, dp_h, mmac, \
		bs_l, bs_r, shift, shift_mode, in_type, split, intgr_l, dprol, dpror, addtr, outlay_merge; 
	uint32_t p0_rota, p1_rota;
	uint32_t t0, t1, t2, t3;
	uint32_t r8_value_low, r8_value_hig, r9_value_low, r9_value_hig;

	uint32_t m0_in_flash = 0;
	uint32_t m1_in_flash = 0; //20x4
	uint32_t bias_in_flash = 0;
	// uint32_t m0_bit4_en = 0;
	// uint32_t m1_bit4_en = 0;
	// uint32_t bias_en = 0;	//24x4
	if (i_addr_m0 >= LUNA_FLASH_MEM_BASE) {
		reg_write(ADDR_LUNA_AHB_BADDR, 0x30000000);
		i_addr_m0 = (uintptr_t)LUNA_FLASH_ADDR_OFFSET(i_addr_m0);	
		m0_in_flash = 1;
	} else if (i_addr_m0 >= LUNA_PSRAM_MEM_BASE) {
		reg_write(ADDR_LUNA_AHB_BADDR, 0x28000000);
		i_addr_m0 = (uintptr_t)LUNA_PSRAM_ADDR_OFFSET(i_addr_m0);	
		m0_in_flash = 1;
	} else {
		i_addr_m0 = (uintptr_t)LUNA_SHARE_ADDR_OFFSET(i_addr_m0);
	}

	if (i_addr_m1 >= LUNA_FLASH_MEM_BASE) {
		reg_write(ADDR_LUNA_AHB_BADDR, 0x30000000);
		i_addr_m1 = (uintptr_t)LUNA_FLASH_ADDR_OFFSET(i_addr_m1);
		m1_in_flash = 1;
	} else if (i_addr_m1 >= LUNA_PSRAM_MEM_BASE) {
		reg_write(ADDR_LUNA_AHB_BADDR, 0x28000000);
		i_addr_m1 = (uintptr_t)LUNA_PSRAM_ADDR_OFFSET(i_addr_m1);	
		m1_in_flash = 1;
	} else {
		i_addr_m1 = (uintptr_t)LUNA_SHARE_ADDR_OFFSET(i_addr_m1);
	}
	
	if (i_addr_bias >= LUNA_FLASH_MEM_BASE) {
		reg_write(ADDR_LUNA_AHB_BADDR, 0x30000000);
		i_addr_bias = (uintptr_t)LUNA_FLASH_ADDR_OFFSET(i_addr_bias);
		bias_in_flash = 1;
	} else if (i_addr_bias >= LUNA_PSRAM_MEM_BASE) {
		reg_write(ADDR_LUNA_AHB_BADDR, 0x28000000);
		i_addr_bias = (uintptr_t)LUNA_PSRAM_ADDR_OFFSET(i_addr_bias);	
		bias_in_flash = 1;
	} else {
		i_addr_bias = (uintptr_t)LUNA_SHARE_ADDR_OFFSET(i_addr_bias);
	}
	
	o_addr = (uintptr_t)LUNA_SHARE_ADDR_OFFSET(o_addr);

	p_settings->i_addr_m0 = i_addr_m0;
	p_settings->i_addr_m1 = i_addr_m1;
	p_settings->o_addr = o_addr;
	p_settings->M = M;

	p_settings->N = N;
	p_settings->L = L;
	p_settings->i_precision = i_precision;
	p_settings->o_precision = o_precision;
	
	p_settings->i_interval0 = i_interval0;  //4bit:number of elems, 8bit/32bit:size of bytes.
	p_settings->i_interval1 = i_interval1;  //4bit:number of elems, 8bit/32bit:size of bytes.
	p_settings->o_interval = o_interval; 
	p_settings->loop_num_0 = loop_num_0;
	p_settings->loop_num_1 = loop_num_1;

	p_settings->i_addr_m0_inc = i_addr_m0_inc; //4bit: size of bytes.
	p_settings->i_addr_m1_inc = i_addr_m1_inc; //4bit: size of bytes.
	p_settings->o_addr_inc_0 = o_addr_inc_0; 
	p_settings->o_addr_inc_1 = o_addr_inc_1; 

	p_settings->i_addr_bias = i_addr_bias;
	p_settings->i_addr_bias_inc = i_addr_bias_inc;

	p_settings->m0_in_flash = m0_in_flash;
	p_settings->m1_in_flash = m1_in_flash;
	p_settings->bias_in_flash = bias_in_flash;
	p_settings->m0_bit4_en = m0_bit4_en;
	p_settings->m1_bit4_en = m1_bit4_en;
	p_settings->bias_en = bias_en;

	if (8 == o_precision) {
		ou_type=0;
        outlayer_mode=3;
	} else if (32 == o_precision) {
		ou_type=2;
        outlayer_mode=1;
	} else {
		LUNA_LOG("unspport o_precision, o_precision = %d\n", o_precision);
		return -1;
	}
	if (8 == i_precision) {
		s_cnt = ceil_u32(N,8);
		if(N>8)
			t_cnt = 0;
		else
			t_cnt = 1;
		r_cnt = ceil_u32(L,4);
		h_cnt = ceil_u32(M,2);
		r_addr= ceil_u32(N,8);
		h_addr= ceil_u32(N,8);
		s_rota= 3;
		h_rota= 2;
		dp_w  = 0;
		dp_c  = 1;
		dp_h  = 2;
		mmac  = 0x0000;
		bs_l  = 5;
		bs_r  = 3;
		shift = o_rshift;
		shift_mode=3;
		in_type=3;
		split  =1;
		intgr_l=ceil_u32(N,8);
		dprol = 2;
		dpror = 2;
		addtr= 0;
		outlay_merge=1;

		p0_rota = 768;
        p1_rota = 4;
	} else if (32 == i_precision) {
		s_cnt = N;
		if(N>2)
			t_cnt = 0;
		else
			t_cnt = 1;
		r_cnt = ceil_u32(L,2);
		h_cnt = ceil_u32(M,2);
		r_addr= N;
		h_addr= N;
		s_rota= 3;
		h_rota= 3;
		dp_w  = 3;
		dp_c  = 0;
		dp_h  = 3;
		mmac  = 0x2202;
		bs_l  = 1;
		bs_r  = 0;
		shift = o_rshift;
		shift_mode=3;
		in_type=3;
		split  =0;
		intgr_l=ceil_u32(N,1);
		dprol= 3;
		dpror= 3;
		addtr= 0;
		outlay_merge=0;

		p0_rota = 0;
        p1_rota = 3;
	} else {
		LUNA_LOG("unspport i_precision, i_precision = %d\n", i_precision);
		return -1;
	}
	// load left matrix
	p_settings->llm_slave0_r18 = N;
	p_settings->llm_slave0_r19 = 0x1;
	p_settings->llm_slave0_r20 = M;
	p_settings->llm_slave0_r21 = 0x0;

	if(((i_interval0 <= 16383) && (~m0_bit4_en)) || ((i_interval0/2 <= 16383) && (m0_bit4_en))) {
		p_settings->llm_master_s0_i_interval0 = 1;
	} else {
		p_settings->llm_master_s0_i_interval0 = 0;
	}

	r8_value_low = mod_u32(ceil_u32(N*i_precision,64),256)+mod_u32(0,256)*(1<<8);
	r8_value_hig = mod_u32(M,256)+mod_u32(0,256)*(1<<8);
	r9_value_low = floor_u32(ceil_u32(N*i_precision,64)/256)+floor_u32(0/256.0)*(1<<8);
	r9_value_hig = floor_u32(M/256.0)+floor_u32(0/256.0)*(1<<8);

	p_settings->llm_master_s0_r8 = (r8_value_hig<<16)|r8_value_low;

	if(((mod_u32(i_interval0,65536) > 32767) & (~m0_bit4_en)) || ((mod_u32(i_interval0/2,65536) > 32767) & m0_bit4_en)) {
		p_settings->llm_master_s0_r9 = (1<<16)|r9_value_low;
	} else {
		p_settings->llm_master_s0_r9 = (r9_value_hig<<16)|r9_value_low;
	}
	if (m0_bit4_en) { //bytes
		if(i_interval0/2 <= 16383) {
			p_settings->llm_master_s2_r10 = ((mod_u32(i_interval0/2,256)*256)<<16)|(0x4);
			p_settings->llm_master_s2_r12_H = (floor_u32(i_interval0/2/256)*256); //H
		} else {
			p_settings->llm_master_s2_r10 = ((mod_u32(i_interval0/2,256)*(1<<8) + floor_u32(i_interval0/2/65536))<<16)|(0x4);
			if(mod_u32(i_interval0/2,65536) > 32767) {
				p_settings->llm_master_s2_r12_H = floor_u32(mod_u32(i_interval0/2,65536)/256)*256 + 1; //H
			} else {
				p_settings->llm_master_s2_r12_H = floor_u32(mod_u32(i_interval0/2,65536)/256)*256; //H
			}
		}
	} else { //bytes
		if(i_interval0 <= 16383) {
			p_settings->llm_master_s2_r10 = ((mod_u32(i_interval0,256)*256)<<16)|(0x8);
			p_settings->llm_master_s2_r12_H = (floor_u32(i_interval0/256)*256); //H
		} else {
			p_settings->llm_master_s2_r10 = ((mod_u32(i_interval0,256)*256 + floor_u32(i_interval0/65536))<<16)|(0x8);
			if(mod_u32(i_interval0,65536) > 32767) {
				p_settings->llm_master_s2_r12_H = floor_u32(mod_u32(i_interval0,65536)/256)*256 + 1; //H
			} else {
				p_settings->llm_master_s2_r12_H = floor_u32(mod_u32(i_interval0,65536)/256)*256; //H
			}
		}
		
	}
	p_settings->llm_master_s2_r14 = i_addr_m0;
	if(((i_interval0 > 16383) & (~m0_bit4_en)) || ((i_interval0/2 > 16383) & m0_bit4_en)) {
		p_settings->llm_master_s2_i_interval0 = 1;
	} else{
		p_settings->llm_master_s2_i_interval0 = 0;
	}

	// load right matrix
	p_settings->lrm_slave0_r18 = L;
	p_settings->lrm_slave0_r19 = 0x1;
	p_settings->lrm_slave0_r20 = N;
	p_settings->lrm_slave0_r21 = 0x0;

	if(((i_interval1 <= 16383) && (~m1_bit4_en)) || ((i_interval1/2 <= 16383) && (m1_bit4_en))) {
		p_settings->lrm_master_s0_i_interval1 = 1;
	} else {
		p_settings->lrm_master_s0_i_interval1 = 0;
	}

	p_settings->lrm_master_s0_r8 = (mod_u32(N,256)<<16)|(mod_u32(ceil_u32(L*i_precision,64),256));
	if(((mod_u32(i_interval1/2,65536) > 32767) & m1_bit4_en) || ((mod_u32(i_interval1,65536) > 32767) & (~m1_bit4_en))) {
		p_settings->lrm_master_s0_r9 = (1<<16)|(floor_u32(ceil_u32(L*i_precision,64)/256));
	} else {
		p_settings->lrm_master_s0_r9 = (floor_u32(N/256)<<16)|(floor_u32(ceil_u32(L*i_precision,64)/256));
	}

	if (m1_bit4_en) {
		if(i_interval1/2 <= 16383) {
			p_settings->lrm_master_s2_r10 = ((mod_u32(i_interval1/2,256)*256)<<16)|(0x4);
			p_settings->lrm_master_s2_r12_H = ((floor_u32(i_interval1/2/256)*256)); //H
		} else {
			p_settings->lrm_master_s2_r10 = ((mod_u32(i_interval1/2,256)*256 + floor_u32(i_interval1/2/65536))<<16)|(0x4);	
			if(mod_u32(i_interval1/2,65536) > 32767) {
				p_settings->lrm_master_s2_r12_H = floor_u32(mod_u32(i_interval1/2,65536)/256)*256 + 1;
			} else {
				p_settings->lrm_master_s2_r12_H = floor_u32(mod_u32(i_interval1/2,65536)/256)*256;
			}
		}
	} else {
		if(i_interval1 <= 16383) {
			p_settings->lrm_master_s2_r10 = ((mod_u32(i_interval1,256)*256)<<16)|(0x8);
			p_settings->lrm_master_s2_r12_H = ((floor_u32(i_interval1/256)*256)); //H
		} else {
			p_settings->lrm_master_s2_r10 = ((mod_u32(i_interval1,256)*256 + floor_u32(i_interval1/65536))<<16)|(0x8);
			if(mod_u32(i_interval1,65536) > 32767) {
				p_settings->lrm_master_s2_r12_H = floor(mod_u32(i_interval1,65536)/256)*256 + 1;
			} else {
				p_settings->lrm_master_s2_r12_H = floor(mod_u32(i_interval1,65536)/256)*256;
			}
		}
	}
	p_settings->lrm_master_s2_r14 = i_addr_m1;
	if(((i_interval1 > 16383) & (~m1_bit4_en)) || ((i_interval1/2 > 16383) & m1_bit4_en) ) {
		p_settings->lrm_master_s2_i_interval1 = 1;
	} else{
		p_settings->lrm_master_s2_i_interval1 = 0;
	}

	// caculation
	p_settings->cc_master_s0_r8 = ((mod_u32(r_cnt,256)+16*256)<<16)|(mod_u32(s_cnt,256)+mod_u32(t_cnt,256)*256);
	p_settings->cc_master_s0_r9 = (floor_u32(r_cnt/256)<<16)|(floor_u32(s_cnt/256)+floor_u32(t_cnt/256)*256);
	p_settings->cc_master_s00_r8 = ((mod_u32(0,256)+0*256)<<16)|(mod_u32(h_cnt,256)+0*256);
	p_settings->cc_master_s00_r9 = ((0)<<16) | (floor_u32(h_cnt/256)+0*256);
	
	p_settings->cc_master_s2_r10 = ((mod_u32(r_addr,256))<<16)|(0x1);
	p_settings->cc_master_s2_r11 = 0;
	p_settings->cc_master_s2_r12 = ((floor_u32(r_addr/256)+0*(1<<8))<<16)|(0x0);
	p_settings->cc_master_s2_r14 = ((floor_u32(0/(1<<16)))<<16) |(mod_u32(0,1<<16));
	
	p_settings->cc_master_s22_r10_H = 0;  //H
	p_settings->cc_master_s22_r11 = mod_u32(h_addr,256);
	p_settings->cc_master_s22_r12 = 0;
	p_settings->cc_master_s22_r13 = floor_u32(h_addr/256);
	
	p_settings->cc_master_select3_r15_L =  dp_w*16+dp_h*4+dp_c;
	p_settings->cc_master_select3_r16_L = p0_rota;
	p_settings->cc_master_select33_r15_L = 0;
	p_settings->cc_master_select33_r16_L = p1_rota;

	p_settings->cc_pe_r24_L = mmac;  //L
	p_settings->cc_pe_r23_H = shift; //H
	p_settings->cc_pe_r26 = (((outlay_merge<<12)|((split*4+addtr)<<8)|(ou_type<<4)|(in_type))<<16)|(((shift_mode*(1<<6))<<8)|(bs_r<<4)|(bs_l));
	p_settings->cc_pe_r27 = intgr_l-1;

	p_settings->cc_iow_r20 = o_addr;
	p_settings->cc_iow_r18 = L*o_precision/8;
	p_settings->cc_iow_r19 = M;
	p_settings->cc_iow_r21 = o_interval;

	return 0;
}

static int luna_mat_trans3d_wch_convert_param(
	uint32_t i_addr,
	uint32_t o_addr,
	uint32_t h,
	uint32_t w,
	uint32_t i_inv,  // bytes
	uint32_t o_inv,  // bytes
	uint32_t precision, // 8/32
	uint32_t loop_num, 
	uint32_t i_addr_inc,
	uint32_t o_addr_inc, 
	LunaMatrixCopySettings_t* p_settings)
{
	i_addr = (uintptr_t)LUNA_SHARE_ADDR_OFFSET(i_addr);
	o_addr = (uintptr_t)LUNA_SHARE_ADDR_OFFSET(o_addr);

	p_settings->src = i_addr; //r0
	p_settings->dst = o_addr; //r1
	p_settings->row = h;  //r2
	p_settings->col = w;

	p_settings->i_inv = i_inv;//r3   //16
	p_settings->o_inv = o_inv; //r4
	p_settings->precision = precision; 
	p_settings->loop_num = loop_num; //r5
	
	p_settings->i_addr_inc = i_addr_inc; //32
	p_settings->o_addr_inc = o_addr_inc;

	//config 
	p_settings->loop_r22 = i_addr;
	p_settings->loop_r5 = i_addr + i_inv;
	p_settings->loop_r23 = i_addr_inc;
	p_settings->loop_r24 = o_addr;
	p_settings->loop_r6 = o_addr_inc;
	p_settings->loop_r26 = loop_num;

	p_settings->lm_master_s0_r8 = (mod_u32(ceil_u32(h,1),256) <<16)|(mod_u32(ceil_u32(w*precision,64),256) + 6*256); //S   
	p_settings->lm_master_s0_r9 = (floor_u32(ceil_u32(h,1)/256) << 16) |(floor_u32(ceil_u32(w*precision,64)/256)); //T

	if (i_inv <= 16383) {
		p_settings->lm_master_s2_r10 = ((mod_u32(i_inv,256)*256)<<16)|(8); //SADDR  //32
		p_settings->lm_master_s2_r12_H = ((floor_u32(i_inv/256)*256)<<16)|(0); //SADDR
	} else {
		p_settings->lm_master_s2_r10 = ((mod_u32(i_inv,256)*(1<<8) + floor_u32(i_inv/65536))<<16)|(8); //SADDR  //32
		if (mod_u32(i_inv, 65536) > 32767) {
			p_settings->lm_master_s2_r12_H = (((floor_u32(mod_u32(i_inv,65536)/256)*(1<<8) + 1))<<16)|(0); //SADDR
		} else {
			p_settings->lm_master_s2_r12_H =  ((floor_u32(mod_u32(i_inv,65536)/256)*(1<<8))<<16)|(0); //SADDR
		}
	}
	
	p_settings->lm_master_s2_r14 = i_addr; //P0-BASEADDR
	//uint32_t lm_master_s2_r14_2; //P0-BASEADDR

	p_settings->sm_iow_r20 = o_addr; //address  //48
	p_settings->sm_iow_r18 = (floor_u32(w*precision/8/(1<<16))<<16)|(mod_u32(w*precision/8,1<<16)); //channel length
	p_settings->sm_iow_r19 = h; //channel number
	p_settings->sm_iow_r21 = o_inv; //interval

	if (i_inv <= 16383) {
		p_settings->lm_master_s0_i_interval0 = 1;
	} else {
		p_settings->lm_master_s0_i_interval0 = 0;
	}

	return 0;
}

static int luna_mat_trans_convert_param(
	uint32_t i_addr,
	uint32_t o_addr,
	uint32_t row,
	uint32_t col,
	uint32_t i_inv,  // bytes
	uint32_t o_inv,  // bytes
	uint32_t precision, // 8/32
	uint32_t loop_num, 
	uint32_t i_addr_inc,
	uint32_t o_addr_inc, 
	uint32_t loop_num2,
	uint32_t i_addr_inc2,
	uint32_t o_addr_inc2, 
	LunaMatrixTransposeSettings_t* p_settings)
{
	uint32_t t0, t1, t2, t3;
	uint32_t i_addr_inv = i_inv*2;

	i_addr = (uintptr_t)LUNA_SHARE_ADDR_OFFSET(i_addr);
	o_addr = (uintptr_t)LUNA_SHARE_ADDR_OFFSET(o_addr);

	p_settings->src = (uintptr_t)i_addr;
	p_settings->dst = (uintptr_t)o_addr;
	p_settings->row = row;
	p_settings->col = col;
	p_settings->i_inv = i_inv;
	p_settings->o_inv = o_inv;
	p_settings->precision = precision;
	p_settings->loop_num = loop_num;
	p_settings->i_addr_inc = i_addr_inc;
	p_settings->o_addr_inc = o_addr_inc;
	p_settings->loop_num2 = loop_num2;
	p_settings->i_addr_inc2 = i_addr_inc2;
	p_settings->o_addr_inc2 = o_addr_inc2;

	p_settings->lm_master_s0_r8 = (mod_u32(ceil_u32(row,2),256)<<16) | (mod_u32(ceil_u32(col*precision,64),256));
	if(mod_u32(i_addr_inv,65536) > 32767){
		p_settings->lm_master_s0_r9 = (1<<16) | floor_u32(ceil_u32(col*precision,64)/256);
	} else {
		p_settings->lm_master_s0_r9 = (floor_u32(ceil_u32(row,2)/256)<<16) | floor_u32(ceil_u32(col*precision,64)/256); 
	}
	
	if(i_addr_inv <= 16383) {
		p_settings->lm_master_s0_i_interval0 = 1;
		p_settings->lm_master_s2_r10 = ((mod_u32(i_inv*2,256)*256)<<16)|(0x8);
		p_settings->lm_master_s2_r12_H = (floor_u32(i_inv*2/256)*256); //TODO H
	} else {
		p_settings->lm_master_s0_i_interval0 = 0;
		p_settings->lm_master_s2_r10 = ((mod_u32(i_addr_inv,256)*(1<<8) + floor_u32(i_addr_inv/65536))<<16)|(0x8);
		if(mod_u32(i_addr_inv,65536) > 32767) {
			p_settings->lm_master_s2_r12_H = (floor_u32(mod_u32(i_addr_inv,65536)/256)*(1<<8) + 1); //TODO H
		} else {
			p_settings->lm_master_s2_r12_H = (floor_u32(mod_u32(i_addr_inv,65536)/256)*(1<<8)); //TODO H
		}
	}
	p_settings->lm_master_s2_r14 = i_addr; 
	p_settings->lm_master_s2_r14_2 = i_addr + i_inv;
	if (8==precision) {
		p_settings->lm_master_s3_r15 = 3*16+0*4+1;
	} else if (32==precision){
		p_settings->lm_master_s3_r15 = 1*16+0*4+2;
	}

	p_settings->lm_slave0_r18 = 2*col;
	p_settings->lm_slave0_r20 = ceil_u32(row,2);

	p_settings->sm_master_s0_r8 = ((mod_u32(ceil_u32(col,2),256))<<16)|(mod_u32(ceil_u32(row*precision,64),256)+4*256);
	p_settings->sm_master_s0_r9 = ((floor_u32(ceil_u32(col,2)/256))<<16)|(floor_u32(ceil_u32(row*precision,64)/256)); 
	if (8==precision) {
		p_settings->sm_master_s2_r10 = ((mod_u32(ceil_u32(row,8),256))<<16)|(0x1);
		p_settings->sm_master_s2_r12 = ((floor_u32(ceil_u32(row,8)/256))); //TODO H
	} else if (32==precision){
		p_settings->sm_master_s2_r10 = (0x1<<16)|((ceil_u32(col,2))&0xff);
		p_settings->sm_master_s2_r12 = (0<<16)|((ceil_u32(col,2))/256); 
	}

	p_settings->sm_iow_r20 = o_addr;
	p_settings->sm_iow_r18 = (row*precision)/8;
	p_settings->sm_iow_r19 = col;
	p_settings->sm_iow_r21 = o_inv;

	return 0;
}

static int luna_mat_trans_common(
	const void* i_addr,
	void* o_addr,
	uint32_t row,
	uint32_t col,
	uint32_t i_inv,   //elemsize
	uint32_t o_inv,   //elemsize
	uint32_t precision// 8/32
	)
{
	int32_t ret = 0;
	const uint32_t align_limit = MATRIX_TRANS_SIZE;
	uint32_t loop_row, align_r, align_c, align_size;
	uint32_t row_0, row_1, col_0, i_inv_0, o_inv_0, i_addr_inc, o_addr_inc, i_addr_1, o_addr_1;
	uint32_t in_bytes = precision>>3;
	LunaMatrixTransposeSettings_t* p_settings = &luna_mat_trans_params;

	if (8==precision) {
		align_r = 8, align_c = 2;
	} else { //32bit
		align_r = 2, align_c = 2;
	}
	row_0 = row;
	col_0 = col;
	while (1){
		align_size = ((ceil_u32(row_0, align_r)*align_r)) * ((ceil_u32(col_0, align_c)*align_c))*in_bytes;
		if (align_size <= align_limit) {
			break;
		}
		row_0 -= 1;
	};
	if (0 == row_0) {
		//LUNA_LOG("mat trans row_0 param error, row=%d, col=%d\n", row, col);
		return -10000;
	}
	loop_row = row/row_0;
	row_1 = row%row_0;

	i_inv_0 = i_inv*in_bytes;
	o_inv_0 = o_inv*in_bytes;
	i_addr_inc = row_0*i_inv*in_bytes;
	o_addr_inc = row_0*in_bytes;
#if defined(LUNA_TEST)
	LUNA_LOG("mat trans row=%d, col=%d, loop_row=%d, row_0=%d, row_1=%d, align_size=%d\n", row, col, loop_row, row_0, row_1, align_size);
#endif 
	if (loop_row > 0) {	
		i_addr_1 = (uintptr_t)i_addr;
		o_addr_1 = (uintptr_t)o_addr;

		ret |= luna_mat_trans_convert_param(i_addr_1, o_addr_1, 
			row_0, col_0, i_inv_0, o_inv_0, precision, 
			loop_row, i_addr_inc, o_addr_inc, 
			1, 0, 0, p_settings);
			ret |= luna_execute_cmd(luna_matrix_transpose_cmd, p_settings, 0);
	}
	
	if (row_1 > 0) {
		i_addr_1 = (uintptr_t)i_addr + loop_row*i_addr_inc;
		o_addr_1 = (uintptr_t)o_addr + loop_row*o_addr_inc;

		ret |= luna_mat_trans_convert_param(i_addr_1, o_addr_1,
		row_1, col_0, i_inv_0, o_inv_0, precision, 
		1, i_addr_inc, o_addr_inc, 
		1, 0, 0, p_settings);
		ret |= luna_execute_cmd(luna_matrix_transpose_cmd, p_settings, 0);
	}

	return ret;
}

static int luna_mat_trans_common_for_split_col(
	const void* i_addr,
	void* o_addr,
	uint32_t row,
	uint32_t col,
	uint32_t i_inv,   //elemsize
	uint32_t o_inv,   //elemsize
	uint32_t precision// 8/32
	)
{
	int32_t ret = 0;
	const uint32_t align_limit = MATRIX_TRANS_SIZE;
	uint32_t loop_row, loop_col, align_r, align_c, align_size;
	uint32_t row_0, row_1, col_0, col_1, i_inv_0, o_inv_0, i_addr_inc, o_addr_inc, i_addr_1, o_addr_1;
	uint32_t in_bytes = precision>>3;
	LunaMatrixTransposeSettings_t* p_settings = &luna_mat_trans_params;

	if (8==precision) {
		align_r = 8, align_c = 2;
	} else { //32bit
		align_r = 2, align_c = 2;
	}
	row_0 = row;
	col_0 = align_limit / ((ceil_u32(row_0, align_r)*align_r)*in_bytes);
	loop_col = col/col_0;
	col_1 = col%col_0;

	if (0 == col_0) {
		LUNA_LOG("mat trans col_0 param error, row=%d, col=%d\n", row, col);
		return -10000;
	}

	i_inv_0 = i_inv*in_bytes;
	o_inv_0 = o_inv*in_bytes;
	i_addr_inc = col_0*in_bytes;
	o_addr_inc = col_0*row_0*in_bytes;
#if defined(LUNA_TEST)
	LUNA_LOG("mat trans row=%d, col=%d, loop_col=%d, col_0=%d, row_1=%d, align_size=%d\n", row, col, loop_col, col_0, row_1, align_size);
#endif 
	if (loop_col > 0) {
		i_addr_1 = (uintptr_t)i_addr;
		o_addr_1 = (uintptr_t)o_addr;

		ret |= luna_mat_trans_convert_param(i_addr_1, o_addr_1, 
			row_0, col_0, i_inv_0, o_inv_0, precision, 
			loop_col, i_addr_inc, o_addr_inc,
			1, 0, 0, p_settings);
			ret |= luna_execute_cmd(luna_matrix_transpose_cmd, p_settings, 0);
	}
	
	if (col_1 > 0) {
		i_addr_1 = (uintptr_t)i_addr + loop_col*i_addr_inc;
		o_addr_1 = (uintptr_t)o_addr + loop_col*o_addr_inc;

		ret |= luna_mat_trans_convert_param(i_addr_1, o_addr_1,
		row_0, col_1, i_inv_0, o_inv_0, precision, 
		1, i_addr_inc, o_addr_inc, 
		1, 0, 0, p_settings);
		ret |= luna_execute_cmd(luna_matrix_transpose_cmd, p_settings, 0);
	}

	return ret;
}

static uint32_t aligned_size(uint32_t size, uint32_t alignment)
{
	return ((ceil_u32(size, alignment)*alignment));
}

static int32_t luna_mat_mul_common(	
	const void* i_addr_m0,
	const void* i_addr_m1,
	const void* i_addr_bias,
	void* o_addr,
	uint32_t M,
	uint32_t N,
	uint32_t L,
	uint32_t i_inv0,
	uint32_t i_inv1,
	uint32_t o_inv,
	uint32_t i_precision,
	uint32_t o_precision,  
	uint32_t o_rshift,
	uint32_t m0_bit4_en,
	uint32_t m1_bit4_en,
	uint32_t bias_en)
{
	int32_t ret = 0;
	uint32_t M_0, M_1, N_0, L_0, L_1, i_interval0_0, i_interval1_0, o_interval_0;
	uint32_t i_addr_m0_inc, i_addr_m1_inc, o_addr_inc_0, o_addr_inc_1, i_addr_bias_inc;
	uint32_t i_addr_m0_0, i_addr_m1_0, i_addr_bias_0, o_addr_0;
	uint32_t loop_M = 0, loop_L = 0, align_M, align_N, align_L, align_size_L, align_size_R;
	uint32_t in_bytes = i_precision>>3;
	uint32_t out_bytes = o_precision>>3;
	const uint32_t L_limit_size = MATRIX_L_SIZE;  
	const uint32_t R_limit_size = MATRIX_R_SIZE;  
	LunaMatrixSettings_t *p_settings = &luna_mat_mul_params;
	if (8==i_precision) { //8bit
		align_M = 2, align_N = 8, align_L = 4;
	} else { //32bit
		align_M = 2, align_N = 1, align_L = 2;
	}

	// M N 
	// N L 4bit L%2==0  => L_loop = L => L_0=1 crash   
	M_0 = M; N_0 = N; L_0 = L;
	M_1 = 0; L_1 = 0;

	while (1){
		align_size_L = ((ceil_u32(M_0, align_M)*align_M)) * ((ceil_u32(N_0, align_N)*align_N))*in_bytes;
		if (align_size_L <= L_limit_size) {  
			break;  
		}		  
		M_0 -= 1;		
	};
	if (0 == M_0) {
		LUNA_LOG("mat mul M_0 param error, M=%d, N=%d, L=%d\n", M, N, L);
		return -10000;
	}
	loop_M = M/M_0;
	M_1 = M % M_0;

	while (1){
		align_size_R = ((ceil_u32(N_0, align_N)*align_N)) * ((ceil_u32(L_0, align_L)*align_L))*in_bytes;
		if (m1_bit4_en) {
			if (align_size_R <= R_limit_size && (L%L_0==0) && (L_0%2==0)) {
				break;
			}
		} else {
			if (align_size_R <= R_limit_size && (L%L_0==0)) {
				break;
			}
		}

		L_0 -= 1;		
	};
	if (0 == L_0) {
		LUNA_LOG("mat mul L_0 param error, M=%d, N=%d, L=%d\n", M, N, L);
		return -10000;
	}
	loop_L = L/L_0;
	L_1 = L % L_0;

#if defined(LUNA_TEST)
	LUNA_LOG("mat mul M=%d, N=%d, L=%d, loop_M=%d, M_0=%d, M_1=%d, align_size_L=%d, loop_L=%d, L_0=%d, L_1=%d, align_size_R=%d\n", 
		M, N, L, 
		loop_M, M_0, M_1, align_size_L, 
		loop_L, L_0, L_1, align_size_R);
#endif 

	i_interval0_0 = i_inv0*in_bytes;
	i_interval1_0 = i_inv1*in_bytes;
	o_interval_0 = o_inv*out_bytes;

	i_addr_m0_inc = M_0*i_inv0*in_bytes;
	i_addr_m1_inc = L_0*in_bytes;
	i_addr_bias_inc = M_0*sizeof(int32_t);
	o_addr_inc_0 = M_0*o_inv*out_bytes;
	o_addr_inc_1 = L_0*out_bytes;

	if (m0_bit4_en && ((i_addr_m0_inc&0x1) ||(i_interval0_0&0x1))) {
		LUNA_LOG("m0_bit4_en param error, i_addr_m0_inc=%d, i_interval0_0=%d", i_addr_m0_inc, i_interval0_0);
		return -20000;
	}
	if (m1_bit4_en && ((i_addr_m1_inc&0x1) ||(i_interval1_0&0x1))) {
		LUNA_LOG("m1_bit4_en param error, i_addr_m1_inc=%d, i_interval1_0=%d", i_addr_m1_inc, i_interval1_0);
		return -30000;
	}

	if (m0_bit4_en) {
		//i_interval0_0 /=2;
		i_addr_m0_inc /=2;
	}
	if (m1_bit4_en) {
		//i_interval1_0 /= 2;
		i_addr_m1_inc /= 2;
	}
	//A00, A01
	//A10, A11
	if (loop_M > 0) {
		if (loop_L >0) {
			i_addr_m0_0 = (uintptr_t)i_addr_m0;
			i_addr_m1_0 = (uintptr_t)i_addr_m1;
			i_addr_bias_0 = (uintptr_t)i_addr_bias;
			o_addr_0 = (uintptr_t)o_addr;
//			loop_M = 1;
//			loop_L = 1;
			ret |= luna_mat_mul_convert_param(i_addr_m0_0, i_addr_m1_0, i_addr_bias_0, o_addr_0, M_0, N_0, L_0,
				i_precision, o_precision, i_interval0_0, i_interval1_0, o_interval_0, o_rshift, 
				loop_M, loop_L, i_addr_m0_inc, i_addr_m1_inc, i_addr_bias_inc, o_addr_inc_0, o_addr_inc_1, 
				m0_bit4_en,m1_bit4_en,bias_en,p_settings);
			ret |= luna_execute_cmd(luna_matrix_mul_cmd, p_settings, 0);
		} 
		if (L_1>0){
			i_addr_m0_0 = (uintptr_t)i_addr_m0;
			i_addr_m1_0 = (uintptr_t)i_addr_m1 + loop_L*i_addr_m1_inc;
			i_addr_bias_0 = (uintptr_t)i_addr_bias;
			o_addr_0 = (uintptr_t)o_addr + loop_L*o_addr_inc_1;
			
			ret |= luna_mat_mul_convert_param(i_addr_m0_0, i_addr_m1_0, i_addr_bias_0, o_addr_0, M_0, N_0, L_1,
				i_precision, o_precision, i_interval0_0, i_interval1_0, o_interval_0, o_rshift, 
				loop_M, 1, i_addr_m0_inc, i_addr_m1_inc, i_addr_bias_inc, o_addr_inc_0, o_addr_inc_1, 
				m0_bit4_en,m1_bit4_en,bias_en,p_settings);
			ret |= luna_execute_cmd(luna_matrix_mul_cmd, p_settings, 0);
		}
	}
	if (M_1 > 0) {
		if (loop_L >0) {
			i_addr_m0_0 = (uintptr_t)i_addr_m0 + loop_M*i_addr_m0_inc;
			i_addr_m1_0 = (uintptr_t)i_addr_m1;
			i_addr_bias_0 = (uintptr_t)i_addr_bias + loop_M*M_0*sizeof(int32_t);
			o_addr_0 = (uintptr_t)o_addr + loop_M*o_addr_inc_0;
			
			ret |= luna_mat_mul_convert_param(i_addr_m0_0, i_addr_m1_0, i_addr_bias_0, o_addr_0, M_1, N_0, L_0,
				i_precision, o_precision, i_interval0_0, i_interval1_0, o_interval_0, o_rshift, 
				1, loop_L, i_addr_m0_inc, i_addr_m1_inc, i_addr_bias_inc, o_addr_inc_0, o_addr_inc_1, 
				m0_bit4_en,m1_bit4_en,bias_en,p_settings);
			ret |= luna_execute_cmd(luna_matrix_mul_cmd, p_settings, 0);
		} 
		
		if (L_1>0){
			i_addr_m0_0 = (uintptr_t)i_addr_m0 + loop_M*i_addr_m0_inc;
			i_addr_m1_0 = (uintptr_t)i_addr_m1 + loop_L*i_addr_m1_inc;
			i_addr_bias_0 = (uintptr_t)i_addr_bias + loop_M*M_0*sizeof(int32_t);
			o_addr_0 = (uintptr_t)o_addr + loop_M*o_addr_inc_0 + loop_L*o_addr_inc_1;
			
			ret |= luna_mat_mul_convert_param(i_addr_m0_0, i_addr_m1_0, i_addr_bias_0, o_addr_0, M_1, N_0, L_1,
				i_precision, o_precision, i_interval0_0, i_interval1_0, o_interval_0, o_rshift, 
				1, 1, i_addr_m0_inc, i_addr_m1_inc, i_addr_bias_inc, o_addr_inc_0, o_addr_inc_1, 
				m0_bit4_en,m1_bit4_en,bias_en,p_settings);
			ret |= luna_execute_cmd(luna_matrix_mul_cmd, p_settings, 0);
		}
	}
	return ret;
}

static int luna_trans_axis_common(
	const void* i_addr,
	void* o_addr,
	uint32_t c,
	uint32_t h,
	uint32_t w,
	uint32_t precision, // 8/32
	uint32_t mode,
	uint32_t loop_num)
{
	if (0x201==mode) {
		int ret = 0;
		uint32_t loop_num, i_inv, o_inv, i_addr_inc, o_addr_inc;
		LunaMatrixCopySettings_t* p_settings = &luna_mat_copy_params;

		loop_num  = c; 
		i_inv     = w*precision/8; 
		o_inv     = w*c*precision/8; 
		i_addr_inc = w*h*precision/8;  
		o_addr_inc = w*precision/8;  
		ret |= luna_mat_trans3d_wch_convert_param((uintptr_t)i_addr, (uintptr_t)o_addr, h, w, i_inv, o_inv, precision, 
			loop_num, i_addr_inc, o_addr_inc, p_settings);
		ret |= luna_execute_cmd(luna_matrix_transpose_3d_wch_cmd, p_settings, sizeof(*p_settings));

		return ret;
	} else {
		int ret = 0;
		uint32_t i_inv, o_inv, i_addr_inc2, o_addr_inc2, loop_num2, plane_h, plane_w;
		uint32_t i_addr_inc, o_addr_inc;
		uint32_t in_bytes = precision>>3;
		uint32_t loop_num_0 = 0, align_r = 8, align_c = 2, align_size = 0, align_limit = MATRIX_TRANS_SIZE; 
		uint32_t row_0, row_1, col_0, i_inv_0, o_inv_0;
		uint32_t i_addr_1, o_addr_1;
		LunaMatrixTransposeSettings_t* p_settings = &luna_mat_trans_params;
		if (8==precision) {
			align_r = 8, align_c = 2;
		} else { //32bit
			align_r = 2, align_c = 2;
		}
		
		if (0x120==mode) { //hwc
			i_inv    = w*precision/8;
			o_inv    = h*precision/8;
			i_addr_inc2 = w*h*precision/8;
			o_addr_inc2 = w*h*precision/8;
			loop_num2   = c;
			plane_h  = h;
			plane_w  = w;
		} else if(0x102==mode) { //hcw
			i_inv    = w*precision/8;
			o_inv    = h*c*precision/8;
			i_addr_inc2 = w*h*precision/8;
			o_addr_inc2 = h*precision/8;
			loop_num2   = c;
			plane_h  = h;
			plane_w  = w;
		} else if(0x012==mode) { //chw
			i_inv    = w*h*precision/8;
			o_inv    = h*c*precision/8;
			i_addr_inc2 = w*precision/8;
			o_addr_inc2 = c*precision/8;
			loop_num2   = h;
			plane_h  = c;
			plane_w  = w;
		} else if(0x021==mode) { //cwh
			i_inv    = w*h*precision/8;
			o_inv    = c*precision/8;
			i_addr_inc2 = w*precision/8;
			o_addr_inc2 = w*c*precision/8;
			loop_num2   = h;
			plane_h  = c;
			plane_w  = w;
		} else if(0x201==mode) { //wch
			i_inv    = w*precision/8;
			o_inv    = w*c*precision/8;
			i_addr_inc2 = w*h*precision/8;
			o_addr_inc2 = w*precision/8;
			loop_num2  = c;
			plane_h  = h;
			plane_w  = w;
		} else {
			LUNA_LOG("trans3d, unspport mode, mode = %d\n", mode);
			return -1;
		}
		

		col_0 = plane_w;
		row_0 = plane_h;
		while (1){
			align_size = ((ceil_u32(row_0, align_r)*align_r)) * ((ceil_u32(col_0, align_c)*align_c))*in_bytes;
			if (align_size <= align_limit) {
				break;
			}
			row_0 -= 1;		
		};
		if (0 == row_0) {
			LUNA_LOG("trans3d, row_0 is zero\n");
			return -20000;
		}

		//row_0 = align_limit / (((ceil_u32(col_0, align_c)*align_c))*in_bytes);
		if (row_0 < plane_h) {
			loop_num = plane_h/row_0;
			row_1 = plane_h % row_0;
		} else {
			row_0 = plane_h;
			loop_num = 1;
			row_1 = 0;
		}
		
		i_inv_0 = i_inv;
		o_inv_0 = o_inv;

		if (0x120==mode) { //hwc
			i_addr_inc = row_0*plane_w*in_bytes;
			o_addr_inc = row_0*in_bytes;
		} else if(0x102==mode) { //hcw
			i_addr_inc = row_0*plane_w*in_bytes;
			o_addr_inc = row_0*in_bytes;
		} else if(0x012==mode) { //chw
			i_addr_inc = row_0*h*plane_w*in_bytes;
			o_addr_inc = row_0*in_bytes;
		} else if(0x021==mode) { //cwh
			i_addr_inc = row_0*h*plane_w*in_bytes;
			o_addr_inc = row_0*in_bytes;
		} else if(0x201==mode) { //wch
			i_addr_inc = row_0*plane_w*in_bytes;
			o_addr_inc = row_0*in_bytes;
		} else {
			LUNA_LOG("trans3d, unspport mode, mode = %d\n", mode);
			return -1;
		}
#if defined(LUNA_TEST)
		LUNA_LOG("trans3d, loop_num = %d, row_0 = %d, row_1 = %d\n", loop_num, row_0, row_1);
#endif 
		i_addr_1 = (uintptr_t)i_addr + loop_num*i_addr_inc;
		o_addr_1 = (uintptr_t)o_addr + loop_num*o_addr_inc;

		ret |= luna_mat_trans_convert_param((uintptr_t)i_addr, (uintptr_t)o_addr, 
		row_0, col_0, i_inv_0, o_inv_0, precision, 
		loop_num, i_addr_inc, o_addr_inc, 
		loop_num2, i_addr_inc2, o_addr_inc2, p_settings);

		ret |= luna_execute_cmd(luna_matrix_transpose_cmd, p_settings, 0);
		
		if (row_1 > 0) {
			ret |= luna_mat_trans_convert_param(i_addr_1, o_addr_1,
			row_1, col_0, i_inv_0, o_inv_0, precision, 
			1, i_addr_inc, o_addr_inc, 
			loop_num2, i_addr_inc2, o_addr_inc2, p_settings);

			ret |= luna_execute_cmd(luna_matrix_transpose_cmd, p_settings, 0);
		}

		return ret;
	}
}

static int luna_mat_copy_common(
	const void* i_addr,
	void* o_addr,
	uint32_t c,
	uint32_t h,
	uint32_t w,
	uint32_t i_planar_inv, 
	uint32_t i_row_inv, 
	uint32_t o_planar_inv, 
	uint32_t o_row_inv,
	uint32_t precision // 8/32
	)
{
	int ret = 0;
	uint32_t loop_num, i_inv, o_inv, i_addr_inc, o_addr_inc;
	LunaMatrixCopySettings_t* p_settings = &luna_mat_copy_params;

	loop_num  = c; 
	i_inv     = i_row_inv*precision/8; 
	o_inv     = o_row_inv*precision/8; 
	i_addr_inc = i_planar_inv*precision/8;  
	o_addr_inc = o_planar_inv*precision/8;  
	ret |= luna_mat_trans3d_wch_convert_param((uintptr_t)i_addr, (uintptr_t)o_addr, h, w, i_inv, o_inv, precision, 
		loop_num, i_addr_inc, o_addr_inc, p_settings);
	ret |= luna_execute_cmd(luna_matrix_transpose_3d_wch_cmd, p_settings, sizeof(*p_settings));

	return ret;
}
int32_t luna_mat_mul_i8i8o8(const int8_t *src1, const int8_t *src2, int8_t *dst, uint32_t row, uint32_t col, uint32_t col2, uint32_t shift)
{
	return luna_mat_mul_common(src1, src2, 0, dst, row, col, col2, col, col2, col2, 8, 8, shift, 0,0,0);
}

int32_t luna_mat_mul_i8i8o32(const int8_t *src1, const int8_t *src2, int32_t *dst, uint32_t row, uint32_t col, uint32_t col2, uint32_t shift)
{
	return luna_mat_mul_common(src1, src2, 0, dst, row, col, col2, col, col2, col2, 8, 32, shift, 0,0,0);
}

int32_t luna_mat_mul_i32i32o8(const int32_t *src1, const int32_t *src2, int8_t *dst, uint32_t row, uint32_t col, uint32_t col2, uint32_t shift)
{
	return luna_mat_mul_common(src1, src2, 0, dst, row, col, col2, col, col2, col2, 32, 8, shift, 0,0,0);
}

int32_t luna_mat_mul_i32i32o32(const int32_t *src1, const int32_t *src2, int32_t *dst, uint32_t row, uint32_t col, uint32_t col2, uint32_t shift)
{
	return luna_mat_mul_common(src1, src2, 0, dst, row, col, col2, col, col2, col2, 32, 32, shift, 0,0,0);
}


int32_t luna_mat_mul_inv_i8i8o8(const int8_t *src1, const int8_t *src2, int8_t *dst, uint32_t row, uint32_t col, uint32_t col2, uint32_t i_inv1, uint32_t i_inv2, uint32_t o_inv, uint32_t shift)
{
	return luna_mat_mul_common(src1, src2, 0, dst, row, col, col2, i_inv1, i_inv2, o_inv, 8, 8, shift, 0,0,0);
}

int32_t luna_mat_mul_inv_i8i8o32(const int8_t *src1, const int8_t *src2, int32_t *dst, uint32_t row, uint32_t col, uint32_t col2, uint32_t i_inv1, uint32_t i_inv2, uint32_t o_inv, uint32_t shift)
{
	return luna_mat_mul_common(src1, src2, 0, dst, row, col, col2, i_inv1, i_inv2, o_inv, 8, 32, shift, 0,0,0);
}

int32_t luna_mat_mul_inv_i32i32o8(const int32_t *src1, const int32_t *src2, int8_t *dst, uint32_t row, uint32_t col, uint32_t col2, uint32_t i_inv1, uint32_t i_inv2, uint32_t o_inv, uint32_t shift)
{
	return luna_mat_mul_common(src1, src2, 0, dst, row, col, col2, i_inv1, i_inv2, o_inv, 32, 8, shift, 0,0,0);
}

int32_t luna_mat_mul_inv_i32i32o32(const int32_t *src1, const int32_t *src2, int32_t *dst, uint32_t row, uint32_t col, uint32_t col2, uint32_t i_inv1, uint32_t i_inv2, uint32_t o_inv, uint32_t shift)
{
	return luna_mat_mul_common(src1, src2, 0, dst, row, col, col2, i_inv1, i_inv2, o_inv, 32, 32, shift, 0,0,0);
}


int32_t luna_split_mat_mul_i8i8o8(const int8_t *src1, const int8_t *src2, int8_t *dst, uint32_t row, uint32_t col, uint32_t col2, uint32_t shift)
{
	return luna_mat_mul_common(src1, src2, 0, dst, row, col, col2, col, col2, col2, 8, 8, shift, 0,0,0);
}

int32_t luna_split_mat_mul_i8i8o32(const int8_t *src1, const int8_t *src2, int32_t *dst, uint32_t row, uint32_t col, uint32_t col2, uint32_t shift)
{
	return luna_mat_mul_common(src1, src2, 0, dst, row, col, col2, col, col2, col2, 8, 32, shift, 0,0,0);
}

int32_t luna_split_mat_mul_i32i32o8(const int32_t *src1, const int32_t *src2, int8_t *dst, uint32_t row, uint32_t col, uint32_t col2, uint32_t shift)
{
	return luna_mat_mul_common(src1, src2, 0, dst, row, col, col2, col, col2, col2, 32, 8, shift, 0,0,0);
}

int32_t luna_split_mat_mul_i32i32o32(const int32_t *src1, const int32_t *src2, int32_t *dst, uint32_t row, uint32_t col, uint32_t col2, uint32_t shift)
{
	return luna_mat_mul_common(src1, src2, 0, dst, row, col, col2, col, col2, col2, 32, 32, shift, 0,0,0);
}


int32_t luna_group_mat_mul_i8i8o8(const int8_t *src1, const int8_t *src2, int8_t *dst, uint32_t group_num, uint32_t row, uint32_t col, uint32_t col2, uint32_t shift)
{
	for (size_t g = 0; g < group_num; g++)
	{
		luna_mat_mul_inv_i8i8o8(src1 + g*col, src2 + g*col*col2, dst + g*col2, row, col, col2, col*group_num, col2, col2*group_num, shift);
	}

	return 0;
}

int32_t luna_group_mat_mul_i8i8o32(const int8_t *src1, const int8_t *src2, int32_t *dst, uint32_t group_num, uint32_t row, uint32_t col, uint32_t col2, uint32_t shift)
{
	for (size_t g = 0; g < group_num; g++)
	{
		luna_mat_mul_inv_i8i8o32(src1 + g*col, src2 + g*col*col2, dst + g*col2, row, col, col2, col*group_num, col2, col2*group_num, shift);
	}

	return 0;
}

int32_t luna_group_mat_mul_i32i32o8(const int32_t *src1, const int32_t *src2, int8_t *dst, uint32_t group_num, uint32_t row, uint32_t col, uint32_t col2, uint32_t shift)
{
	for (size_t g = 0; g < group_num; g++)
	{
		luna_mat_mul_inv_i32i32o8(src1 + g*col, src2 + g*col*col2, dst + g*col2, row, col, col2, col*group_num, col2, col2*group_num, shift);
	}

	return 0;
}

int32_t luna_group_mat_mul_i32i32o32(const int32_t *src1, const int32_t *src2, int32_t *dst, uint32_t group_num, uint32_t row, uint32_t col, uint32_t col2, uint32_t shift)
{
	for (size_t g = 0; g < group_num; g++)
	{
		luna_mat_mul_inv_i32i32o32(src1 + g*col, src2 + g*col*col2, dst + g*col2, row, col, col2, col*group_num, col2, col2*group_num, shift);
	}

	return 0;
}


int32_t luna_mat_trans_i8o8(const int8_t *src1, int8_t *dst, uint32_t row, uint32_t col)
{
	return luna_mat_trans_common(src1, dst, row, col, col, row, 8);
}

int32_t luna_mat_trans_i32o32(const int32_t *src1, int32_t *dst, uint32_t row, uint32_t col)
{
	return luna_mat_trans_common(src1, dst, row, col, col, row, 32);
}


int32_t luna_mat_trans_inv_i8o8(const int8_t *src, int8_t *dst, uint32_t row, uint32_t col, uint32_t i_inv, uint32_t o_inv)
{
	return luna_mat_trans_common(src, dst, row, col, i_inv, o_inv, 8);
}

int32_t luna_mat_trans_inv_i32o32(const int32_t *src, int32_t *dst, uint32_t row, uint32_t col, uint32_t i_inv, uint32_t o_inv)
{
	return luna_mat_trans_common(src, dst, row, col, i_inv, o_inv, 32);
}


int32_t luna_split_mat_trans_i8o8(const int8_t *src, int8_t *dst, uint32_t row, uint32_t col)
{
	int ret = luna_mat_trans_common(src, dst, row, col, col, row, 8);
	if (-10000 == ret) {
		ret = luna_mat_trans_common_for_split_col(src, dst, row, col, col, row, 8);
	}
	return ret;
}

int32_t luna_split_mat_trans_i32o32(const int32_t *src, int32_t *dst, uint32_t row, uint32_t col)
{
	int ret = luna_mat_trans_common(src, dst, row, col, col, row, 32);
	if (-10000 == ret) {
		ret = luna_mat_trans_common_for_split_col(src, dst, row, col, col, row, 32);
	}
	return ret;
}


int32_t luna_trans_axis_i8o8(const int8_t *src, int8_t *dst, uint32_t *in_shape, uint32_t *axis, uint32_t n_dims)
{
	int ret;
	uint32_t mode = (axis[2]<<8)|(axis[1]<<4)|(axis[0]<<0);
	ret = luna_trans_axis_common(src, dst, in_shape[0], in_shape[1], in_shape[2], 8, mode, 1);
	return ret;
}

int32_t luna_trans_axis_i32o32(const int32_t *src, int32_t *dst, uint32_t *in_shape, uint32_t *axis, uint32_t n_dims)
{
	int ret;
	uint32_t mode = (axis[2]<<8)|(axis[1]<<4)|(axis[0]<<0);
	ret = luna_trans_axis_common(src, dst, in_shape[0], in_shape[1], in_shape[2], 32, mode, 1);
	return ret;
}



int32_t luna_split_mat_mul_bias_i8i8i32o8(const int8_t *src1, const int8_t *src2, const int32_t *bias, int8_t *dst,
		 uint32_t row, uint32_t col, uint32_t col2, uint32_t shift)
{
	return luna_mat_mul_common(src1, src2, bias, dst, row, col, col2, col, col2, col2, 8, 8, shift, 0,0,bias>0); 
}

int32_t luna_split_mat_mul_bias_i8i8i32o32(const int8_t *src1, const int8_t *src2, const int32_t *bias, int32_t *dst,
	 uint32_t row, uint32_t col, uint32_t col2, uint32_t shift)
{
	return luna_mat_mul_common(src1, src2, bias, dst, row, col, col2, col, col2, col2, 8, 32, shift, 0,0,bias>0);
}

int32_t luna_split_mat_mul_bias_i32i32i32o8(const int32_t *src1, const int32_t *src2, const int32_t *bias, int8_t *dst,
	 uint32_t row, uint32_t col, uint32_t col2, uint32_t shift)
{
	return luna_mat_mul_common(src1, src2, bias, dst, row, col, col2, col, col2, col2, 32, 8, shift, 0,0,bias>0);
}

int32_t luna_split_mat_mul_bias_i32i32i32o32(const int32_t *src1, const int32_t *src2, const int32_t *bias, int32_t *dst,
	 uint32_t row, uint32_t col, uint32_t col2, uint32_t shift)
{
	return luna_mat_mul_common(src1, src2, bias, dst, row, col, col2, col, col2, col2, 32, 32, shift, 0,0,bias>0);
}


int32_t luna_split_mat_mul_bias_i4i8i32o8(const int8_t *src1, const int8_t *src2, const int32_t *bias, int8_t *dst,
	 uint32_t row, uint32_t col, uint32_t col2, uint32_t shift)
{
	return luna_mat_mul_common(src1, src2, bias, dst, row, col, col2, col, col2, col2, 8, 8, shift, 1,0,bias>0);
}

int32_t luna_split_mat_mul_bias_i4i8i32o32(const int8_t *src1, const int8_t *src2, const int32_t *bias, int32_t *dst,
	 uint32_t row, uint32_t col, uint32_t col2, uint32_t shift)
{
	return luna_mat_mul_common(src1, src2, bias, dst, row, col, col2, col, col2, col2, 8, 32, shift, 1,0,bias>0);
}

int32_t luna_split_mat_mul_bias_i8i4i32o8(const int8_t *src1, const int8_t *src2, const int32_t *bias, int8_t *dst,
	 uint32_t row, uint32_t col, uint32_t col2, uint32_t shift)
{
	return luna_mat_mul_common(src1, src2, bias, dst, row, col, col2, col, col2, col2, 8, 8, shift, 0,1,bias>0);
}

int32_t luna_split_mat_mul_bias_i8i4i32o32(const int8_t *src1, const int8_t *src2, const int32_t *bias, int32_t *dst,
	 uint32_t row, uint32_t col, uint32_t col2, uint32_t shift)
{
	return luna_mat_mul_common(src1, src2, bias, dst, row, col, col2, col, col2, col2, 8, 32, shift, 0,1,bias>0);
}

int32_t luna_mat_copy_i8o8(int8_t* src, int8_t* dst, uint32_t channel, uint32_t row, uint32_t col, 
	uint32_t i_planar_inv, uint32_t i_row_inv, uint32_t o_planar_inv, uint32_t o_row_inv)
{
	return luna_mat_copy_common(src, dst, channel, row, col, i_planar_inv, i_row_inv, o_planar_inv, o_row_inv, 8);	
}

int32_t luna_mat_copy_i32o32(int32_t* src, int32_t* dst, uint32_t channel, uint32_t row, uint32_t col, 
	uint32_t i_planar_inv, uint32_t i_row_inv, uint32_t o_planar_inv, uint32_t o_row_inv)
{
	return luna_mat_copy_common(src, dst, channel, row, col, i_planar_inv, i_row_inv, o_planar_inv, o_row_inv, 32);
}

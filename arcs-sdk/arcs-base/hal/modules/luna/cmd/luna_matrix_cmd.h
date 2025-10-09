#ifndef __LUNA_MATRIX_H__
#define __LUNA_MATRIX_H__

#include "stdint.h"
#include "../luna_privates.h"

typedef struct LunaMatrixTransposeSettings
{
	uint32_t src; //r0
	uint32_t dst; //r1
	uint32_t row;  //r2
	uint32_t col;

	uint32_t i_inv;//r3   //16
	uint32_t o_inv; //r4
	uint32_t precision; 
	uint32_t loop_num; //r5
	
	uint32_t i_addr_inc; //32
	uint32_t o_addr_inc;
	uint32_t loop_num2;
	uint32_t i_addr_inc2;
	
	uint32_t o_addr_inc2; //48

	//Load Matrix
	uint32_t lm_master_s0_r8; //S
	uint32_t lm_master_s0_r9; //T
	uint32_t lm_master_s2_r10; //SADDR
	uint32_t lm_master_s2_r12_H; //SADDR
	uint32_t lm_master_s2_r14; //P0-BASEADDR
	uint32_t lm_master_s2_r14_2; //P0-BASEADDR
	uint32_t lm_master_s3_r15;//
	uint32_t lm_slave0_r18; //W
	uint32_t lm_slave0_r20; //C
	//Output Matrix
	uint32_t sm_master_s0_r8;
	uint32_t sm_master_s0_r9;
	uint32_t sm_master_s2_r10; //SADDR
	uint32_t sm_master_s2_r12; //SADDR
	uint32_t sm_iow_r20; //address
	uint32_t sm_iow_r18; //channel length
	uint32_t sm_iow_r19; //channel number
	uint32_t sm_iow_r21; //interval

	uint32_t lm_master_s0_i_interval0; //68
}LunaMatrixTransposeSettings_t;

typedef struct LunaMatrixCopySettings
{
	uint32_t src; //r0
	uint32_t dst; //r1
	uint32_t row;  //r2
	uint32_t col;

	uint32_t i_inv;//r3   //16
	uint32_t o_inv; //r4
	uint32_t precision; 
	uint32_t loop_num; //r5
	
	uint32_t i_addr_inc; //32
	uint32_t o_addr_inc;

	//config 
	uint32_t loop_r22;  // 40 + 0
	uint32_t loop_r5;
	uint32_t loop_r23;	
	uint32_t loop_r24;

	uint32_t loop_r6;    // 16
	uint32_t loop_r26;
	uint32_t lm_master_s0_r8; //S   
	uint32_t lm_master_s0_r9; //T

	uint32_t lm_master_s2_r10; //SADDR  //32
	uint32_t lm_master_s2_r12_H; //SADDR
	uint32_t lm_master_s2_r14; //P0-BASEADDR
	uint32_t lm_master_s2_r14_2; //P0-BASEADDR

	uint32_t sm_iow_r20; //address  //48
	uint32_t sm_iow_r18; //channel length
	uint32_t sm_iow_r19; //channel number
	uint32_t sm_iow_r21; //interval

	uint32_t lm_master_s0_i_interval0; //64
}LunaMatrixCopySettings_t;

typedef struct LunaMatrixTranspose3DSettings
{
	uint32_t i_addr; //0
	uint32_t o_addr;
	uint32_t c;
	uint32_t h;  

	uint32_t w; //16
	uint32_t precision;
	uint32_t mode; // hwc: 0x120, hcw:0x102, chw:0x012, cwh:0x021, wch:0x201
	uint32_t i_addr_inv; //28

	//config 
	uint32_t loop_r22;  //0
	uint32_t loop_r5;
	uint32_t loop_r23;
	uint32_t loop_r24;

	uint32_t loop_r6;    //16
	uint32_t loop_r26;
	//Load Matrix
	uint32_t lm_master_s0_r8; //S
	uint32_t lm_master_s0_r9; //T

	uint32_t lm_master_s2_r10; //SADDR    //32
	uint32_t lm_master_s2_r12; //SADDR
	uint32_t lm_master_s2_r14; //P0-BASEADDR
	uint32_t lm_master_s2_r14_2; //P0-BASEADDR

	uint32_t lm_master_s3_r15;//   //48
	uint32_t lm_slave0_r18; //W
	uint32_t lm_slave0_r20; //C
	//Output Matrix
	uint32_t sm_master_s0_r8;

	uint32_t sm_master_s0_r9;   //64
	uint32_t sm_master_s2_r10; //SADDR
	uint32_t sm_master_s2_r12; //SADDR
	uint32_t sm_master_s3_r15_L;

	uint32_t sm_master_s3_r16_L;  //80
	uint32_t sm_iow_r20; //address
	uint32_t sm_iow_r18; //channel length
	uint32_t sm_iow_r19; //channel number

	uint32_t sm_iow_r21; //interval //96
}LunaMatrixTranspose3DSettings_t;

typedef struct LunaMatrixSettings
{
	// common
	uint32_t i_addr_m0;
	uint32_t i_addr_m1;
	uint32_t o_addr;
	uint32_t M;

	uint32_t N;   //4x4
	uint32_t L;
	uint32_t i_precision; 
	uint32_t o_precision;  
	
	uint32_t i_interval0; //8x4
	uint32_t i_interval1;
	uint32_t o_interval; 
	uint32_t loop_num_0;
	
	uint32_t loop_num_1; //12x4
	uint32_t i_addr_m0_inc;
	uint32_t i_addr_m1_inc;
	uint32_t o_addr_inc_0;

	uint32_t o_addr_inc_1; //16x4
	uint32_t i_addr_bias;  
	uint32_t i_addr_bias_inc; 
	uint32_t m0_in_flash;

	uint32_t m1_in_flash; //20x4
	uint32_t bias_in_flash;
	uint32_t m0_bit4_en;
	uint32_t m1_bit4_en;

	uint32_t bias_en; 	//24x4

	// ll
	uint32_t llm_slave0_r18; 
	uint32_t llm_slave0_r19;
	uint32_t llm_slave0_r20;
	uint32_t llm_slave0_r21;
	uint32_t llm_master_s0_r8;
	uint32_t llm_master_s0_r9;
	uint32_t llm_master_s2_r10;
	uint32_t llm_master_s2_r12_H;
	uint32_t llm_master_s2_r14;
	// lr
	uint32_t lrm_slave0_r18;
	uint32_t lrm_slave0_r19;
	uint32_t lrm_slave0_r20;
	uint32_t lrm_slave0_r21;
	uint32_t lrm_master_s0_r8;
	uint32_t lrm_master_s0_r9;
	uint32_t lrm_master_s2_r10;
	uint32_t lrm_master_s2_r12_H;
	uint32_t lrm_master_s2_r14;
	// calc 
	uint32_t cc_master_s0_r8;
	uint32_t cc_master_s0_r9;
	uint32_t cc_master_s00_r8;
	uint32_t cc_master_s00_r9;
	uint32_t cc_master_s2_r10;
	uint32_t cc_master_s2_r11;
	uint32_t cc_master_s2_r12;
	uint32_t cc_master_s2_r14;
	uint32_t cc_master_s22_r10_H;
	uint32_t cc_master_s22_r11;
	uint32_t cc_master_s22_r12;
	uint32_t cc_master_s22_r13;
	uint32_t cc_master_select3_r15_L;
	uint32_t cc_master_select3_r16_L;
	uint32_t cc_master_select33_r15_L;
	uint32_t cc_master_select33_r16_L;
	uint32_t cc_pe_r24_L;
	uint32_t cc_pe_r23_H;
	uint32_t cc_pe_r26;
	uint32_t cc_pe_r27;
	uint32_t cc_iow_r20;
	uint32_t cc_iow_r18;
	uint32_t cc_iow_r19;
	uint32_t cc_iow_r21;
	//
	uint32_t llm_master_s2_i_interval0; //168
	uint32_t lrm_master_s2_i_interval1; //172
	uint32_t llm_master_s0_i_interval0; //176
	uint32_t lrm_master_s0_i_interval1; //180
} LunaMatrixSettings_t;

extern __luna_cmd_attr__ uint32_t luna_matrix_mul_cmd[];
extern __luna_cmd_attr__ uint32_t luna_matrix_transpose_cmd[];
extern __luna_cmd_attr__ uint32_t luna_matrix_transpose_3d_cmd[];
extern __luna_cmd_attr__ uint32_t luna_matrix_mul_bias_cmd[];
extern __luna_cmd_attr__ uint32_t luna_matrix_transpose_3d_wch_cmd[];

#endif


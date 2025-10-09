#ifndef __LUNA_API_MISC_H__
#define __LUNA_API_MISC_H__

#include "stdint.h"
#include "../luna_privates.h"

typedef struct LunaMiscActvParams
{
	uint32_t *src;
	uint32_t *dst;
	uint32_t size;
	uint32_t pos_shift;
	uint32_t neg_shift;
	uint32_t inout_bits;
}LunaMiscActvParams_t;

typedef struct LunaMiscMemParams
{
	uint32_t *src;
	uint32_t *dst;
	uint32_t size;
	uint32_t value;
	uint32_t inbits;
}LunaMiscMemParams_t;

typedef struct LunaMiscLutParams
{
	uint32_t *src;
	uint32_t *dst;
	uint32_t size;
	uint32_t type;
	uint32_t inout_bits;
	uint32_t src_sel;
}LunaMiscLutParams_t;

typedef struct LunaExpParams
{
	uint32_t src;
	uint32_t dst;
	uint32_t size;
	uint32_t master_s0_r8_L;
	uint32_t master_s0_r9_L;
	// uint32_t master_s0_r10_L;
	uint32_t master_s0_r14;
	// uint32_t master_s2_r10_L;
	uint32_t master_s2_r14;
	uint32_t master_r15_L;
	uint32_t iow_r20;
	uint32_t iow_r18;
	// uint32_t iow_r19;
	// uint32_t iow_r21;
}LunaExpParams_t;

typedef struct LunaSoftmaxParams
{
	uint32_t i_addr; //0
	uint32_t o_addr;
	uint32_t length;
	uint32_t in_num;

	uint32_t in_num_offs; //16
	uint32_t in_mask;
	uint32_t r8_value_low;
	uint32_t r9_value_low;

	uint32_t ou_num; //32

	uint32_t maxmin_pe_r26;  //36 + 0
	uint32_t maxmin_pe_27;
	uint32_t sum_master_s3_r15_L;
	uint32_t sum_pe_r27;

	uint32_t scale_master_s3_r15_L; //16  //52
	uint32_t scale_pe_r23_H;  //56
	uint32_t src_sel; //60
}LunaSoftmaxParams_t;

typedef struct LunaMemcopyParams
{
	uint32_t i_addr; //0
	uint32_t o_addr;
	uint32_t length;
	uint32_t in_num;

	uint32_t src_sel; //16 0:sharememory 1:flash

	uint32_t master_s0_r8; //20 + 0
	uint32_t master_s0_r9;
	uint32_t master_s0_r10;
	uint32_t master_s0_r12;

	uint32_t master_s0_r14; //16
	uint32_t master_s0_t;  //true: 1, false: 0
	uint32_t master_s1_t;
	uint32_t master_s2_t;  //true: 1, false: 0

	uint32_t iow_r20; //32
	uint32_t iow_r18;
	uint32_t iow_r19;
	uint32_t iow_r21;
}LunaMemcopyParams_t;

extern __luna_cmd_attr__ uint32_t luna_api_memset[];
extern __luna_cmd_attr__ uint32_t luna_api_memcpy[];
extern __luna_cmd_attr__ uint32_t luna_api_activate[];
extern __luna_cmd_attr__ uint32_t luna_api_lut[];
extern __luna_cmd_attr__ uint32_t luna_api_exp[];
extern __luna_cmd_attr__ uint32_t luna_api_activate_relux[];
extern __luna_cmd_attr__ uint32_t luna_api_softmax[];
extern __luna_cmd_attr__ uint32_t luna_api_psrammemcpy[];

#endif


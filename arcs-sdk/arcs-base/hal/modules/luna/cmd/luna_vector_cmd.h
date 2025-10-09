#ifndef __LUNA_VECTOR_H__
#define __LUNA_VECTOR_H__

#include "stdint.h"
#include "../luna_privates.h"

typedef struct LunaVectorParams
{
	uint32_t *src1;	// r0
	union {	// r1
		uint32_t *src2;
		uint32_t scalar;
	};	
	uint32_t *dst;	// r2
	uint32_t size;	// r3
	uint32_t shift;	// r4
	uint32_t dtype;	// r5
	uint32_t api_type;  // r6
}LunaVectorParams_t;

extern __luna_cmd_attr__ uint32_t luna_api_vector_add_new[];
extern __luna_cmd_attr__ uint32_t luna_api_vector_mul_new[];
extern __luna_cmd_attr__ uint32_t luna_api_vector_cmp[];
extern __luna_cmd_attr__ uint32_t luna_api_vector_maxmin[];
extern __luna_cmd_attr__ uint32_t luna_api_vector_conj[];
extern __luna_cmd_attr__ uint32_t luna_api_vec_cplx_mul[];
extern __luna_cmd_attr__ uint32_t luna_api_vec_cplx_mul_real[];
extern __luna_cmd_attr__ uint32_t luna_api_vec_cplx_mul_ou_real[];
extern __luna_cmd_attr__ uint32_t luna_api_vec_cplx_modulus[];
extern __luna_cmd_attr__ uint32_t luna_api_vector_div[];
#endif


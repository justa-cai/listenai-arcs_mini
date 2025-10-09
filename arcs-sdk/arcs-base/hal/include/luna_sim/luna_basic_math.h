/***************************************************************************
 * .h                                                  *
 *                                                                         *
 * Copyright (C) 2020 listenai Co.Ltd                   			       *
 * All rights reserved.                                                    *
 ***************************************************************************/
#ifndef __LUNA_BASIC_MATH_H_SIM__
#define __LUNA_BASIC_MATH_H_SIM__


#include "luna_math_types.h"

#ifndef MASK_SIM_TYPEDEF
typedef enum
{
	LUNA_CMP_GREATER_THAN = 0,
	LUNA_CMP_GREATER_OR_EQUAL,
	LUNA_CMP_LESS_THAN,
	LUNA_CMP_LESS_OR_EQUAL,
	LUNA_CMP_EQUAL,
} VEC_CMP_MODE;
#endif //MASK_SIM_TYPEDEF


/**
 * @brief Addition of q7 vectors.
 * @param[in]       *src1 points to the first input vector.
 * @param[in]       *src2 points to the second input vector.
 * @param[out]      *dst  points to the output vector.
 * @param[in]       size  size of the vectors.
 * @return none.
 *
 * Ouput results will be saturated in Q7 range [0x80 0x7F].
 */
int32_t LUNA_API_SIM(luna_add_i8i8o8)(const  int8_t *src1, const int8_t *src2, int8_t *dst, uint32_t size, uint32_t shift);
int32_t LUNA_API_SIM(luna_add_i8i8o32)(const  int8_t* src1, const int8_t* src2, int32_t* dst, uint32_t size, uint32_t shift);
int32_t LUNA_API_SIM(luna_add_i32i32o8)(const  int32_t *src1, const int32_t *src2, int8_t *dst, uint32_t size, uint32_t shift);
int32_t LUNA_API_SIM(luna_add_i32i32o32)(const  int32_t *src1, const int32_t *src2, int32_t *dst, uint32_t size, uint32_t shift);

int32_t LUNA_API_SIM(luna_sub_i8i8o8)(const  int8_t *src1, const int8_t *src2, int8_t *dst, uint32_t size, uint32_t shift);
int32_t LUNA_API_SIM(luna_sub_i8i8o32)(const  int8_t* src1, const int8_t* src2, int32_t* dst, uint32_t size, uint32_t shift);
int32_t LUNA_API_SIM(luna_sub_i32i32o8)(const  int32_t *src1, const int32_t *src2, int8_t *dst, uint32_t size, uint32_t shift);
int32_t LUNA_API_SIM(luna_sub_i32i32o32)(const  int32_t *src1, const int32_t *src2, int32_t *dst, uint32_t size, uint32_t shift);

int32_t LUNA_API_SIM(luna_mul_i8i8o8)(const  int8_t *src1, const int8_t *src2, int8_t *dst, uint32_t size, uint32_t shift);
int32_t LUNA_API_SIM(luna_mul_i8i8o32)(const  int8_t *src1, const int8_t *src2, int32_t *dst, uint32_t size, uint32_t shift);
int32_t LUNA_API_SIM(luna_mul_i32i32o8)(const  int32_t *src1, const int32_t *src2, int8_t *dst, uint32_t size, uint32_t shift);
int32_t LUNA_API_SIM(luna_mul_i32i32o32)(const  int32_t *src1, const int32_t *src2, int32_t *dst, uint32_t size, uint32_t shift);

int32_t LUNA_API_SIM(luna_scale_i8i8o8)(const  int8_t *src1, const int8_t scalar, int8_t *dst, uint32_t size, uint32_t shift);
int32_t LUNA_API_SIM(luna_scale_i8i8o32)(const  int8_t *src1, const int8_t scalar, int32_t *dst, uint32_t size, uint32_t shift);
int32_t LUNA_API_SIM(luna_scale_i32i32o8)(const  int32_t *src1, const int32_t scalar, int8_t *dst, uint32_t size, uint32_t shift);
int32_t LUNA_API_SIM(luna_scale_i32i32o32)(const  int32_t *src1, const int32_t scalar, int32_t *dst, uint32_t size, uint32_t shift);

int32_t LUNA_API_SIM(luna_dot_prod_i8i8o8)(const int8_t *src1, const int8_t *src2, int8_t *dst, uint32_t size, uint32_t shift);
int32_t LUNA_API_SIM(luna_dot_prod_i8i8o32)(const int8_t *src1, const int8_t *src2, int32_t *dst, uint32_t size, uint32_t shift);
int32_t LUNA_API_SIM(luna_dot_prod_i32i32o8)(const int32_t *src1, const int32_t *src2, int8_t *dst, uint32_t size, uint32_t shift);
int32_t LUNA_API_SIM(luna_dot_prod_i32i32o32)(const int32_t *src1, const int32_t *src2, int32_t *dst, uint32_t size, uint32_t shift);
int32_t LUNA_API_SIM(luna_dot_prod_i32i32o64)(const int32_t *src1, const int32_t *src2, int64_t *dst, uint32_t size, uint32_t shift);

int32_t LUNA_API_SIM(luna_vector_sum_i8o8)(const int8_t *src, int8_t *dst, uint32_t size, uint32_t shift);
int32_t LUNA_API_SIM(luna_vector_sum_i8o32)(const int8_t *src, int32_t *dst, uint32_t size, uint32_t shift);
int32_t LUNA_API_SIM(luna_vector_sum_i32o8)(const int32_t *src, int8_t *dst, uint32_t size, uint32_t shift);
int32_t LUNA_API_SIM(luna_vector_sum_i32o32)(const int32_t *src, int32_t *dst, uint32_t size, uint32_t shift);
int32_t LUNA_API_SIM(luna_vector_sum_i32o64)(const int32_t *src, int64_t *dst, uint32_t size, uint32_t shift);

int32_t LUNA_API_SIM(luna_offset_i8i8o8)(const  int8_t *src, const int8_t offset, int8_t *dst, uint32_t size, uint32_t shift);
int32_t LUNA_API_SIM(luna_offset_i8i8o32)(const  int8_t *src, const int8_t offset, int32_t *dst, uint32_t size, uint32_t shift);
int32_t LUNA_API_SIM(luna_offset_i32i32o8)(const  int32_t *src, const int32_t offset, int8_t *dst, uint32_t size, uint32_t shift);
int32_t LUNA_API_SIM(luna_offset_i32i32o32)(const  int32_t *src, const int32_t offset, int32_t *dst, uint32_t size, uint32_t shift);

int32_t LUNA_API_SIM(luna_cmp_vv_i8i8o8)(const int8_t *src1, const int8_t *src2, int8_t *dst, uint32_t size, uint32_t cmp_mode);
int32_t LUNA_API_SIM(luna_cmp_vv_i32i32o32)(const int32_t *src1, const int32_t *src2, int32_t *dst, uint32_t size, uint32_t cmp_mode);
int32_t LUNA_API_SIM(luna_cmp_vs_i8i8o8)(const int8_t *src1, const int8_t scalar, int8_t *dst, uint32_t size, uint32_t cmp_mode);
int32_t LUNA_API_SIM(luna_cmp_vs_i32i32o32)(const int32_t *src1, const int32_t scalar, int32_t *dst, uint32_t size, uint32_t cmp_mode);

int32_t LUNA_API_SIM(luna_max_i8o32)(const  int8_t *src, int32_t *dst, uint32_t size);
int32_t LUNA_API_SIM(luna_min_i8o32)(const  int8_t *src, int32_t *dst, uint32_t size);
int32_t LUNA_API_SIM(luna_max_i32o32)(const  int32_t *src, int32_t* dst, uint32_t size);
int32_t LUNA_API_SIM(luna_min_i32o32)(const  int32_t *src, int32_t *dst, uint32_t size);

int32_t LUNA_API_SIM(luna_div_i32i32o32)(const int32_t *src1, const int32_t *src2, int32_t *dst, uint32_t size, uint32_t shift);


#endif // __LUNA_BASIC_MATH_H_SIM__

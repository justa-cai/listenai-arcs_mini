/**
 ****************************************************************************************
 *
 * @file rf_fxp.h
 *
 * @brief definitions and declarations of Fixed and Floating Conversion
 *
 * Copyright (C) ListenAI 2020-2023
 *
 * Created on: Nov 30, 2023
 *
 *      Author: leifeng
 *
 ****************************************************************************************
 */

#ifndef _RF_FXP_H_
#define _RF_FXP_H_

/***********************************************************************************
 * Macros for Fixed and Floating Conversion
 ***********************************************************************************/
#define FRACT_BITS_8                   8
#define FRACT_BITS_9                   9
#define FRACT_BITS_10                  10
#define FRACT_BITS_11                  11
#define FRACT_BITS_12                  12
#define FRACT_BITS_13                  13
#define FRACT_BITS_14                  14
#define FRACT_BITS_15                  15

#define FRACT_BITS_A_SQUARE            11
#define FRACT_BITS_S_MATRIX            11
#define FRACT_BITS_TEMP_MATRIX         11
#define FRACT_BITS_FOURIER_MATRIX      11
#define FRACT_BITS_INV_COEFF_MATRIX    11
#define FRACT_BITS_INV_SUM_SQ_WT_MAT   11

#define FRACT_BITS_COMP_VECTOR         15

#define FRACT_BITS_RX_IQ_C_MATRIX      11

#define FRACT_BITS_DPD                 13

#define FRACT_BITS_GENERIC             FRACT_BITS_15

#define FRACT_BITS                     FRACT_BITS_GENERIC

#define FIXED_ONE(_fract_bits)         (1 << (_fract_bits))
#define FIXED_HALF(_fract_bits)        ((1 << (_fract_bits))*0.4996)
#define INT2FIXED(x)                   ((x) << FRACT_BITS)
#define FLOAT2FIXED_16Q10(x)           ((int)((x) * (1 << FRACT_BITS_10)))
#define FLOAT2FIXED_16Q11(x)           ((int)((x) * (1 << FRACT_BITS_11)))
#define FLOAT2FIXED_16Q13(x)           ((int)((x) * (1 << FRACT_BITS_13)))
#define FIXED2INT(x)                   ((x) >> FRACT_BITS)
#define FIXED2DOUBLE(x)                (((double)(x)) / (1 << FRACT_BITS))
#define FIXED2FLOAT(x)                 (((float)(x)) / (1 << FRACT_BITS))
#define MULT(x, y)                     (int)(((long long)(x) * (long long)(y))>>FRACT_BITS)
#define FIXED_MULT(x, y, q)            (int)(((long long)(x) * (long long)(y))>>(q))
#define FIXED_DIV(x, y, q)             (int32_t)(((int64_t)(x) << (q)) / (int32_t)(y))

#define FLOAT2INT16(x, qpoint)         ((int16_t)((x) * (1 << (qpoint))))
#define FLOAT2UINT16(x, qpoint)        ((uint16_t)((x) * (1 << (qpoint))))
#define FLOAT2INT32(x, qpoint)         ((int32_t)((x) * (1 << (qpoint))))
#define FLOAT2UINT32(x, qpoint)        ((uint32_t)((x) * (1 << (qpoint))))
#define INT2FLOAT(x, qpoint)           (((float)(x)) / (1 << (qpoint)))

#ifndef abs
#define abs(x)   ((x)>=0?(x):-(x))
#endif

#define max(a, b)   ((a)>=(b)?(a):(b))
#define min(a, b)   ((a)<=(b)?(a):(b))

extern void Fix16(float* pIn, int16_t* pOut, int qpoint, int num);
extern void Fix16Unsigned(float* pIn, uint16_t* pOut, int qpoint, int num);
extern void Fix16Real(float* pIn, uint16_t* pOut, int qpoint, int num);
extern void Fix32(float* pIn, int32_t* pOut, int qpoint, int num);
extern void Fix32Unsigned(float* pIn, uint32_t* pOut, int qpoint, int num);
extern void Fix32Real(float* pIn, uint32_t* pOut, int qpoint, int num);

extern int32_t fix_ceil(int32_t x, int32_t shift);
extern int32_t fix_floor(int32_t x, int32_t shift);
extern int32_t fix_round(int32_t x, int32_t shift);
extern uint16_t fix_sqrt(uint32_t x);

#endif  /* _RF_FXP_H_ */


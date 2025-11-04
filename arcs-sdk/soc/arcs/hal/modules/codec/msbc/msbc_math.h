/**
 ****************************************************************************************
 *
 * @file msbc_math.h
 *
 * @brief msbc math
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */


#include "msbc_port.h"

#define fabs(x) ((x) < 0 ? -(x) : (x))
/* C does not provide an explicit arithmetic shift right but this will
   always be correct and every compiler *should* generate optimal code */
//#ifndef SBC_RDA585X
#define ASR(val, bits) ((-2 >> 1 == -1) ? \
		 ((INT32)(val)) >> (bits) : ((INT32) (val)) / (1 << (bits)))
//#else
//#define ASR(val, bits) ((val) >> (bits))
//#define ASR(val, bits) (((INT32)val) >> (bits))
//#define ASR(val, bits) (((INT32)val) >> (bits))
//#endif //SBC_RDA585X

#define SCALE_SPROTO4_TBL	12
#define SCALE_SPROTO8_TBL	14
#define SCALE_NPROTO4_TBL	11
#define SCALE_NPROTO8_TBL	11
#define SCALE4_STAGED1_BITS	15
#define SCALE4_STAGED2_BITS	16
#define SCALE8_STAGED1_BITS	15
#define SCALE8_STAGED2_BITS	16

#define SCALE4_STAGED1(src) ASR(src, SCALE4_STAGED1_BITS)
#define SCALE4_STAGED2(src) ASR(src, SCALE4_STAGED2_BITS)
#define SCALE8_STAGED1(src) ASR(src, SCALE8_STAGED1_BITS)
#define SCALE8_STAGED2(src) ASR(src, SCALE8_STAGED2_BITS)

#define SBC_FIXED_0(val) { val = 0; }
#define MUL(a, b)        ((a) * (b))
#if defined(__arm__) && (!defined(__thumb__) || defined(__thumb2__))
#define MULA(a, b, res) ({				\
		int tmp = res;			\
		__asm__(				\
			"mla %0, %2, %3, %0"		\
			: "=&r" (tmp)			\
			: "0" (tmp), "r" (a), "r" (b));	\
		tmp; })
#else
#define MULA(a, b, res)  ((a) * (b) + (res))
#endif

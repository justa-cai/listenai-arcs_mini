/*
 * types.h
 *
 *  Created on: 2019年6月18日
 *      Author: gequn
 */

#ifndef INCLUDE_TYPES_H_
#define INCLUDE_TYPES_H_

typedef signed char         s8_t;
typedef signed short        s16_t;
typedef signed int          s32_t;
typedef signed long long    s64_t;

typedef unsigned char       u8_t;
typedef unsigned short      u16_t;
typedef unsigned int        u32_t;
typedef unsigned long long  u64_t;

#ifdef __clang__
typedef long  off_t;
#endif

#endif /* INCLUDE_TYPES_H_ */

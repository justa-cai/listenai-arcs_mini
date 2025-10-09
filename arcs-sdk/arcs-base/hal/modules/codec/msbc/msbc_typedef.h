/**
 ****************************************************************************************
 *
 * @file msbc_typedef.h
 *
 * @brief msbc typedef
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */
#ifndef __MSBC_TYPE_H__
#define __MSBC_TYPE_H__

#define CHAR_BIT 8
#define MB_LEN_MAX 2
#define SCHAR_MIN (-128)
#define SCHAR_MAX 127
#define UCHAR_MAX 255

typedef unsigned  char 	UINT8;
typedef short			INT16;
typedef unsigned short	UINT16;
typedef int 			INT32;
typedef unsigned int	UINT32;
typedef int 			BOOL;
typedef unsigned  char 	BYTE;
//typedef __int64			INT64;

#if 0
typedef int int32_t ;
typedef unsigned short	uint16_t ;
typedef unsigned  char 	uint8_t ;
typedef short			int16_t ;
typedef int 			int32_t ;
typedef unsigned int 	u_int32_t ;
typedef unsigned int 	uint32_t ;
typedef unsigned __int64    uint64_t;
#endif

typedef unsigned int  size_t;
typedef signed int  ssize_t;

typedef int   intptr_t; 
typedef unsigned   uintptr_t; 

#define  TRUE 		1
#define  FALSE		0    	 

#endif
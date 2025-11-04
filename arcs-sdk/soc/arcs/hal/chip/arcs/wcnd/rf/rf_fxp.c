/**
 ****************************************************************************************
 *
 * @file rf_fxp.c
 *
 * @brief functions of fix point number calc
 *
 * Copyright (C) ListenAI 2020-2023
 *
 * Created on: Nov 30, 2023
 *
 *      Author: leifeng
 *
 ****************************************************************************************
 */


#include <stdio.h>
#include <stdint.h>
#include <string.h>
//#include "rf_fxp.h"

void Fix16(float *pIn, int16_t *pOut, int qpoint, int num)
{
	for (int i = 0; i < num; i++)
	{
		pOut[i] = (int16_t)((pIn[i]) * (1 << qpoint));
	}
}

void Fix16Unsigned(float *pIn, uint16_t *pOut, int qpoint, int num)
{
	for (int i = 0; i < num; i++)
	{
		pOut[i] = (uint16_t)((pIn[i]) * (1 << qpoint));
	}
}

void Fix16Real(float *pIn, uint16_t *pOut, int qpoint, int num)
{
}

void Fix32(float *pIn, int32_t *pOut, int qpoint, int num)
{
	for (int i = 0; i < num; i++)
	{
		pOut[i] = (int32_t)((pIn[i]) * (1 << qpoint));
	}
}

void Fix32Unsigned(float *pIn, uint32_t *pOut, int qpoint, int num)
{
	for (int i = 0; i < num; i++)
	{
		pOut[i] = (uint32_t)((pIn[i]) * (1 << qpoint));
	}
}

void Fix32Real(float *pIn, uint32_t *pOut, int qpoint, int num)
{
}

int32_t fix_ceil(int32_t x, int32_t shift)
{
	if (x & ~(0xFFFFFFFF << shift)) {
		return (x >> shift) + 1;
	}
	else {
		return (x >> shift);
	}
}

int32_t fix_floor(int32_t x, int32_t shift)
{
	return (x >> shift);
}

int32_t fix_round(int32_t x, int32_t shift)
{
	int32_t val = x;

	if (shift > 0) {
		val = val >> (shift - 1);
		val = (val & 0x1) + (val >> 1);
	}

	return val;
}

uint16_t fix_sqrt(uint32_t x)
{
    uint32_t rem = 0;
    uint32_t root = 0;
    uint32_t divisor = 0;
    int i = 0;
    for( i = 0; i < 16; i++)
    {
        root <<= 1;
        rem = ((rem << 2) + (x >> 30));
        x <<= 2;
        divisor = (root << 1) + 1;
        if (divisor <= rem)
        {
            rem -= divisor;
            root++;
        }
    }
    return (uint16_t)root;
}

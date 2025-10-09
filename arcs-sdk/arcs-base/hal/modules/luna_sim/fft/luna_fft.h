/*
 * luna_fft.h
 *
 *  Created on: 2013-8-5
 *      Author: monkeyzx
 */

#ifndef _ZX_FFT_H
#define _ZX_FFT_H

#include <stdint.h>

#define USE_SHIFT_MERGE 1

typedef struct  
{
	int64_t real;
	int64_t imag;
}complex;

int32_t luna_cfft(const complex *in, complex *out, int32_t N, int32_t ifft_mode, int32_t in_shift, int32_t conj_mode);


#endif /* ZX_FFT_H_ */


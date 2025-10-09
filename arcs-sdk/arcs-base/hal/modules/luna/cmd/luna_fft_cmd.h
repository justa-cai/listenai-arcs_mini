#ifndef __LUNA_API_FFT_H__
#define __LUNA_API_FFT_H__

#include "stdint.h"
#include "../luna_privates.h"

typedef struct LunaFFTSettings
{
	uint32_t i_addr;  //0
	uint32_t o_addr;
	uint32_t fft_length;
	uint32_t fft_log2;

	uint32_t shift; 	//16
	uint32_t mode0;   // mode >= 1, {'FFT','IFFT','CR256-FFT','CR256-IFFT','CR257-FFT','CR257-IFFT'};
	uint32_t mode1;   // mode >= 1, {'NORMAL','SHORTEN','REALONLY'};
	uint32_t mode2;	  // mode >= 1, {'SHIFT'};

	uint32_t left_shift; //20-log2(fft_length)+fft_shift+ifft_shift, 0x0010 0000 //32
	uint32_t right_shift; //20

	//load input data
	uint32_t lid_master_s0_r8_L; //0
	uint32_t lid_master_s0_r9_L;
	uint32_t lid_master_s0_r10_L;
	uint32_t lid_master_s0_r15_L;

	uint32_t lid_master_s0_r14; //16
	uint32_t lid_slave0_r18;
	uint32_t lid_slave0_r19;
	uint32_t lid_slave0_r20;

	uint32_t lid_slave0_r21; //32
	//conj
	uint32_t lidc_master_s0_r8_L;
	uint32_t lidc_master_s0_r9_L;
	uint32_t lidc_master_s0_r10_L; //unused

	uint32_t lidc_master_s0_12_L; //48  unused

	uint32_t lidc_master_s0_r14;
	uint32_t lidc_master_s0_r17_H; //unused
	uint32_t lidc_seti_mem_r12;

	//pe
	uint32_t cc_pe_r23; //64 unused
	uint32_t cc_pe_r25; 
	uint32_t cc_pe_r26; //unused
	uint32_t cc_pe_r15; 

	uint32_t cc_slave0_r18;  //80
	uint32_t cc_master_s0_r8; //unused
	uint32_t cc_master_s0_r9; //unused
	uint32_t cc_master_s2_r10; //unused

	//output
	uint32_t so_master_s0_r8; //96
	uint32_t so_master_s0_r9;
	uint32_t so_master_s2_r10;
	uint32_t so_master_s2_r14;

	uint32_t so_master_s3_r16_L; //112
	uint32_t so_master_s3_r15_L; //unused
	uint32_t so_iow_r20;  
	uint32_t so_iow_18;

	uint32_t so_iow_19;	//128
	uint32_t so_iow_21;
}LunaFFTSettings_t;

extern __luna_cmd_attr__ uint32_t luna_fft_cmd[];

#endif


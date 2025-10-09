/**
 ****************************************************************************************
 *
 * @file src.h
 *
 * @brief resample 8k to 16k and 16k to 8k.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

/// 
///   @file src.h
///   This file defines VPP MSBC API structures and functions.
/// 

#ifndef SRC_MSBC_H
#define SRC_MSBC_H

void SrcForMsbcInit(void);

void SrcSample8kTo16k(short *in_8k, short *out_16k, int sample_size);

void SrcSample16kTo8k(short *in_16k, short *out_8k, int sample_size);

#endif  // vpp_SBC_DEC_VOC_H



/**
 ****************************************************************************************
 *
 * @file msbc.h
 *
 * @brief msbc dec&enc
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */


#ifndef __MSBC_H
#define __MSBC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
//#include <stdint.h>

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>

#include "msbc_port.h"
#include "src.h"


/* sampling frequency */
#define SBC_FREQ_16000		0x00
#define SBC_FREQ_32000		0x01
#define SBC_FREQ_44100		0x02
#define SBC_FREQ_48000		0x03

/* blocks */
#define SBC_BLK_4		0x00
#define SBC_BLK_8		0x01
#define SBC_BLK_12		0x02
#define SBC_BLK_16		0x03

/* channel mode */
#define SBC_MODE_MONO		0x00
#define SBC_MODE_DUAL_CHANNEL	0x01
#define SBC_MODE_STEREO		0x02
#define SBC_MODE_JOINT_STEREO	0x03

/* allocation method */
#define SBC_AM_LOUDNESS		0x00
#define SBC_AM_SNR		0x01

/* subbands */
#define SBC_SB_4		0x00
#define SBC_SB_8		0x01

/* data endianess */
#define SBC_LE			0x00
#define SBC_BE			0x01

struct msbc_struct {
	unsigned long flags;
	void *priv;
	void *priv_alloc_base;
};

typedef struct msbc_struct msbc_t;

int msbc_init(msbc_t *msbc, unsigned long flags);
int msbc_reinit(msbc_t *msbc, unsigned long flags);
int msbc_init_msbc(msbc_t *msbc, unsigned long flags, uint8_t *msbc_priv_mem);
int msbc_init_a2dp(msbc_t *msbc, unsigned long flags,
					const void *conf, size_t conf_len);
int msbc_reinit_a2dp(msbc_t *msbc, unsigned long flags,
					const void *conf, size_t conf_len);

ssize_t msbc_parse(msbc_t *msbc, const void *input, size_t input_len);

/* Decodes ONE input block into ONE output block */
ssize_t msbc_decode(msbc_t *msbc, const void *input, size_t input_len,
			void *output, size_t output_len, size_t *written);
/* Encodes ONE input block into ONE output block */
ssize_t msbc_encode(msbc_t *msbc, const void *input, size_t input_len,
			void *output, size_t output_len, size_t *written);

/* Returns the output block size in bytes */
size_t msbc_get_frame_length(msbc_t *msbc);

/* Returns the time one input/output block takes to play in msec*/
unsigned msbc_get_frame_duration(msbc_t *msbc);

/* Returns the input block size in bytes */
size_t msbc_get_codesize(msbc_t *msbc);

const char *msbc_get_implementation_info(msbc_t *msbc);
void msbc_finish(msbc_t *msbc);

#ifdef __cplusplus
}
#endif

#endif /* __SBC_H */

/**
 ****************************************************************************************
 *
 * @file app_msbc.c
 *
 * @brief APP OS Task implementation
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */
#include "msbc.h"
#include "aud_common.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define MSBC_SAMPLES_PER_FRAME    120
#define MSBC_ENCODED_SIZE    57

int16_t msbc_head[4] = 
{
    0x0801, 0x3801, 0xc801, 0xf801
};

int8_t const msbc_mute_frame[MSBC_ENCODED_SIZE] = 
{
	0xAD,0x00,0x00,0xC5,0x00,0x00,0x00,0x00,0x77,0x6D,0xB6,0xDD,0xDB,0x6D,0xB7,0x76,
	0xDB,0x6D,0xDD,0xB6,0xDB,0x77,0x6D,0xB6,0xDD,0xDB,0x6D,0xB7,0x76,0xDB,0x6D,0xDD,
	0xB6,0xDB,0x77,0x6D,0xB6,0xDD,0xDB,0x6D,0xB7,0x76,0xDB,0x6D,0xDD,0xB6,0xDB,0x77,
	0x6D,0xB6,0xDD,0xDB,0x6D,0xB7,0x76,0xDB,0x6C
};

msbc_t   *msbc_env = NULL;

uint8_t app_msbc_start(void)
{
    uint8_t status = 0;


    CLOGD("app msbc start");

    msbc_env = (msbc_t *)AUD_MALLOC(sizeof(msbc_t));
    memset(msbc_env, 0x00, sizeof(msbc_t));

    status = msbc_init(msbc_env, 0);

    CLOGD("msbc init status:0x%x",status);

    return status;
}

uint8_t app_msbc_dec(uint16_t in_len, uint8_t *in_data, uint16_t *out_len, uint8_t *out_data, int16_t *consume_len, uint8_t frame_num)
{
    uint8_t status = 0;

    uint32_t time1, time2;

    *consume_len = msbc_decode(msbc_env, in_data, in_len, out_data, MSBC_SAMPLES_PER_FRAME, (size_t *)out_len);
    return status;
}

uint8_t app_msbc_enc(uint16_t in_len, uint8_t *in_data, uint16_t *out_len, uint8_t *out_data, int16_t *consume_len, uint8_t frame_num)
{
    uint8_t status = 0;
	static uint8_t sq_num = 0;

    uint32_t time1, time2;

    //msbc sq
    memcpy(out_data, &msbc_head[sq_num++], 2);
    sq_num &= 0x03;

    if(frame_num == 0)//mute
    {
        memcpy(out_data+2, msbc_mute_frame, MSBC_ENCODED_SIZE);
    }
    else
    {
        *consume_len = msbc_encode(msbc_env, in_data, in_len, out_data+2, MSBC_ENCODED_SIZE, (size_t *)out_len);
    }

    *out_len += 3;

    return status;
}

uint8_t app_msbc_stop(void)
{
    CLOGD("app msbc stop!");

    if(msbc_env)
    {
        msbc_finish(msbc_env);
        AUD_FREE(msbc_env);
        msbc_env = NULL;
    }
    return 0;
}

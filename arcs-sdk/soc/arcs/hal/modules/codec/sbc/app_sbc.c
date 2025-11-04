/**
 ****************************************************************************************
 *
 * @file app_os_task.c
 *
 * @brief APP OS Task implementation
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */
#include "sbc.h"
#include "aud_common.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef RESAMPLE_CONFIG
#include "aud_mgr_buf.h"
extern int resample_44100_to_48000(short *pInPutData, short *pOutPutdata, int uiDecSize,int ch);
#endif

#define SBC_MAX_DEC_OUT_LEN     4096

sbc_t   *sbc_env = NULL;

#ifdef RESAMPLE_CONFIG
#define SBC_RESAMPLE_SIZE   (5*1024)
aud_out_buf_info_t resample_out_buf;
#endif
uint8_t app_sbc_start(uint16_t sample_rate, uint8_t channels)
{
    uint8_t status = 0;


    CLOGD("app sbc start, sample_rate:%d, channels:0x%x",\
          sample_rate, channels);

    sbc_env = (sbc_t *)AUD_MALLOC(sizeof(sbc_t));
    memset(sbc_env, 0x00, sizeof(sbc_t));

    status = sbc_init(sbc_env, 0);

    CLOGD("sbc init status:0x%x",status);

#ifdef RESAMPLE_CONFIG
    ///check sample.
    if(sample_rate == 44100)
    {
        CLOGD("app sbc mgr resample,%d to 48000.", sample_rate);
        resample_out_buf.buf = (uint8_t *)AUD_MALLOC(SBC_RESAMPLE_SIZE);
        resample_out_buf.buf_size = SBC_RESAMPLE_SIZE;
        resample_out_buf.data_len = 0;
        CLOGD("resample init sta:0x%x", status);
    }
#endif
    return status;
}

//extern uint32_t bt_cur_time_us_get();
uint8_t app_find_sync_header(uint16_t in_len, uint8_t *in_data,int16_t *consume_len)
{
    uint8_t status = 0xff;
    uint16_t consume_sync_len = 0;
    while(consume_sync_len < in_len)
    {
        if(in_data[consume_sync_len] == 0x9c)
        {
            status = 0;
            break;
        }
        consume_sync_len++;
    }
    *consume_len = consume_sync_len;
    CLOGD("app_find_sync_header,in_len:%d,sync:%d", in_len, consume_sync_len);
    return status;
}
uint8_t app_sbc_dec(uint16_t in_len, uint8_t *in_data, uint16_t *out_len, uint8_t *out_data, int16_t *consume_len, uint8_t frame_num)
{
    uint8_t status = 0;
    uint32_t data_out_len = 0;
    int16_t data_consum_len = 0;
    uint8_t *sbc_out = out_data;
    int8_t frame_cnt = frame_num;
    uint8_t cousume_num = 0;
    *out_len = 0;
    *consume_len = 0;

    uint32_t time1, time2;
    while(frame_cnt--)
    {
    
        if(in_data[0] != 0x9c)
        {
            CLOGD("sbc input data err:0x%x,data:0x%x", in_data, (uint32_t)((in_data[3]<<24) | (in_data[2]<<16) | (in_data[1]<<8) | (in_data[0])));
            app_find_sync_header(in_len, in_data, &data_consum_len);
            in_len -= data_consum_len;
            in_data += data_consum_len;
            *consume_len += data_consum_len;
        }
        //time1 = bt_cur_time_us_get();
        data_consum_len = sbc_decode(sbc_env, in_data, in_len, sbc_out, SBC_MAX_DEC_OUT_LEN, (size_t *)&data_out_len);
        //time2 = bt_cur_time_us_get();
        //CLOGD("sbc dec:consume:%d,remain_len:%d, outlen:%d, frame_cnt:%d,time:%d",data_consum_len, in_len, data_out_len, frame_cnt, time2 - time1);
        if(data_consum_len <= 0)
        {
            CLOGD("sbc dec err");
            ///remove this err data.
            *consume_len += 32;
            status = 0xff;
            break;
        }
        cousume_num++;
#ifdef RESAMPLE_CONFIG
        if(sbc_env->frequency == SBC_FREQ_44100)
        {
            uint16_t sbc_out_len = data_out_len * frame_num;
            ///max resample out length.
            uint32_t resample_out_len = 0;
            uint8_t ch = sbc_env->mode == 0 ? 1 : 2;
            //time1 = bt_cur_time_us_get();
            resample_out_len = (resample_44100_to_48000((int16_t *)sbc_out, (int16_t *)(resample_out_buf.buf + resample_out_buf.data_len), data_out_len/2, ch) * 2);
            //time2 = bt_cur_time_us_get();
            resample_out_buf.data_len += resample_out_len;
            //CLOGD("resample,data_len:%d,outlen:%d, time:%d",resample_out_buf.data_len, resample_out_len,  time2 - time1);
            if(resample_out_buf.data_len >= sbc_out_len)
            {
                memcpy(out_data, resample_out_buf.buf, sbc_out_len);
                resample_out_buf.data_len -= sbc_out_len;
                memcpy(resample_out_buf.buf, resample_out_buf.buf + sbc_out_len, resample_out_buf.data_len);
                
                in_len -= data_consum_len;
                *consume_len += data_consum_len;
                *out_len = sbc_out_len;
                //CLOGD("resample_out,outlen:%d,remain_len:%d",*out_len, resample_out_buf.data_len);
                break;
            }
        }
#endif
        in_len -= data_consum_len;
        in_data += data_consum_len;
        sbc_out += data_out_len;
        
        *consume_len += data_consum_len;
        *out_len += data_out_len;

    };
    
    //CLOGD("sbc dec end,consume:%d,remain_len:%d, outlen:%d, consum_num:%d",*consume_len, in_len, *out_len, cousume_num);

    return ((status << 4) | cousume_num);
}

uint8_t app_sbc_stop(void)
{
    CLOGD("app sbc stop!");

#ifdef RESAMPLE_CONFIG
    if(sbc_env->frequency == SBC_FREQ_44100)
    {
        AUD_FREE(resample_out_buf.buf);
        resample_out_buf.buf = NULL;
        resample_out_buf.buf_size = 0;
        resample_out_buf.data_len = 0;
    }
#endif

    if(sbc_env)
    {
        sbc_finish(sbc_env);
        AUD_FREE(sbc_env);
        sbc_env = NULL;
    }
    return 0;
}

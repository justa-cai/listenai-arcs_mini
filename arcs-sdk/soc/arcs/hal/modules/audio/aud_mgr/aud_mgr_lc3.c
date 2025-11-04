/**
 ****************************************************************************************
 * @file aud_mgr_lc3.c
 *
 * @brief  audio manager source
 *
 * Copyright (C) Listenai 2023
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup AUDIO
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include <string.h>            // For memset
#include "aud_mgr.h"
#include "aud_mgr_lc3.h"
#include "aud_pro.h"

/*
 * MACROS
 ****************************************************************************************
 */
/// just dump iso data for debug.
#ifdef WIN32
#define WIN32_DUMP_ISO_PCM
#ifdef WIN32_DUMP_ISO_PCM
#include <stdio.h>

FILE *iso_pcm_file = NULL;
#endif
#endif

/*
 * DEFINES
 ****************************************************************************************
 */


/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */
 
/*
 * LOCAL FUNCTIONS DECLARATION
 ****************************************************************************************
 */

/*
 * LOCAL VARIABLES
 ****************************************************************************************
 */

/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */
aud_mgr_lc3_t *lc3_mgr_env = NULL;

/*
 * LOCAL FUNCTIONS
 ****************************************************************************************
 */
/*
 * GLOBAL FUNCTIONS
 ****************************************************************************************
 */
uint16_t audio_mgr_get_lc3_svr(audio_service_t *lc3_svr)
{
    if(lc3_svr)
    {
        lc3_svr->aud_start            = app_mgr_lc3_start;
        lc3_svr->aud_stop             = app_mgr_lc3_stop;
        lc3_svr->aud_pause            = app_mgr_lc3_pause;
        lc3_svr->aud_resume           = app_mgr_lc3_resume;
        lc3_svr->aud_get_media_info   = NULL;
        lc3_svr->aud_msg_handle       = app_mgr_lc3_msg_handle;
        
        return AUD_ERROR_NO;
    }
    else
    {
        return AUD_ERROR_PARAM_NULL;
    }
}

uint16_t app_mgr_lc3_start(aud_play_info_t *aud_info, aud_cb_t *audio_cb, aud_3party_process_t *aud_3party_process)
{
    uint8_t status = AUD_ERROR_NO;
    uint16_t one_frame_output_size = 0;
    if(lc3_mgr_env == NULL)
    {
        lc3_mgr_env = AUD_MALLOC(sizeof(aud_mgr_lc3_t));
        if(lc3_mgr_env == NULL)
        {
            return AUD_ERROR_BUF_NO_RESOURCE;
        }
        else
        {
            memset(lc3_mgr_env, 0x00, sizeof(aud_mgr_lc3_t));
        }
    }
    /// infomation init
    lc3_mgr_env->lc3_info.ch             = aud_info->aud_ch;
    lc3_mgr_env->lc3_info.freq           = aud_info->aud_sample;
    lc3_mgr_env->lc3_info.out_bits       = aud_info->aud_bits_wide;
    lc3_mgr_env->lc3_info.hr_mode        = aud_info->dummy;
    lc3_mgr_env->lc3_info.frame_ms       = aud_info->frame_dur;
    lc3_mgr_env->lc3_info.per_frame_len  = aud_info->frame_len;

    lc3_mgr_env->audio_cb = audio_cb;
    lc3_mgr_env->lc3_3party_pro = aud_3party_process;
    /// init input buf
    aud_buf_init(&lc3_mgr_env->lc3_in_buf, AUD_LC3_INPUT_BUF_SIZE);
    /// init input pkt dscp buf
    aud_buf_init(&lc3_mgr_env->lc3_pkt_dcsp.pkt_dscp, AUD_LC3_PKT_DSCP_BUF_SIZE);
    /// init output buf
    one_frame_output_size = (lc3_mgr_env->lc3_info.frame_ms * lc3_mgr_env->lc3_info.ch * lc3_mgr_env->lc3_info.freq * 2) / 10000;
    aud_buf_init(&lc3_mgr_env->lc3_out_buf, one_frame_output_size * AUD_LC3_PLAY_FRAME_NUM);
    /// process buf
    lc3_mgr_env->lc3_pro_buf.dec_in_len = lc3_mgr_env->lc3_info.per_frame_len;
    lc3_mgr_env->lc3_pro_buf.dec_out_len = one_frame_output_size;
    aud_pro_buf_init(&lc3_mgr_env->lc3_pro_buf, lc3_mgr_env->lc3_pro_buf.dec_in_len, lc3_mgr_env->lc3_pro_buf.dec_out_len, \
                     0, 0);
    /// init codec
    return app_mgr_lc3_codec_init_req();
}
uint16_t app_mgr_lc3_stop(void)
{
    /// deinit codec
	return app_mgr_lc3_codec_deinit_req();

}
uint16_t app_mgr_lc3_pause(void)
{
	return app_mgr_lc3_stop();
}
uint16_t app_mgr_lc3_resume(aud_play_info_t *aud_info, aud_cb_t *audio_cb, aud_3party_process_t *aud_3party_process)
{
	return app_mgr_lc3_start(aud_info, audio_cb, aud_3party_process);
}


uint16_t app_mgr_lc3_codec_init_req(void)
{
    aud_pro_init_t init_info;
    init_info.aud_type = AUD_TYPE_LC3;
    init_info.sample_rate = lc3_mgr_env->lc3_info.freq;
    init_info.ch = lc3_mgr_env->lc3_info.ch;
    init_info.frame_ms = lc3_mgr_env->lc3_info.frame_ms;
    init_info.out_bits = lc3_mgr_env->lc3_info.out_bits;
    init_info.hr_mode = lc3_mgr_env->lc3_info.hr_mode;
    //app_lc3_start(lc3_mgr_env->lc3_info.freq, lc3_mgr_env->lc3_info.ch, lc3_mgr_env->lc3_info.frame_ms, \
    //                lc3_mgr_env->lc3_info.out_bits, lc3_mgr_env->lc3_info.hr_mode);
    lc3_mgr_env->lc3_state = AUD_MGR_STATE_PRO_INITING;
    return aud_send_msg_to_pro(AUD_PRO_MSG_INIT_REQ, sizeof(aud_pro_init_t), &init_info);
}

uint16_t app_mgr_lc3_codec_deinit_req(void)
{
    aud_pro_deinit_t deinit_info;
    deinit_info.aud_type = AUD_TYPE_LC3;
    //app_lc3_stop();
#ifdef WIN32_DUMP_ISO_PCM
    fclose(iso_pcm_file);
    iso_pcm_file = NULL;
#endif
    return aud_send_msg_to_pro(AUD_PRO_MSG_DEINIT_REQ, sizeof(aud_pro_deinit_t), &deinit_info);
}

uint16_t app_mgr_lc3_decode_req(uint8_t audio_bfi)
{
    aud_pro_dec_t dec_info;
    dec_info.aud_type = AUD_TYPE_LC3;

    //CLOGD("[LC3]dec req!");

    dec_info.in_len = lc3_mgr_env->lc3_pro_buf.dec_in_len;
    dec_info.in_data = lc3_mgr_env->lc3_pro_buf.dec_in_buf;
    dec_info.out_data = lc3_mgr_env->lc3_pro_buf.dec_out_buf;
    AUD_STATE_SET(lc3_mgr_env->lc3_state, AUD_MGR_STATE_PROCESSING);
    return aud_send_msg_to_pro(AUD_PRO_MSG_DEC_REQ, sizeof(aud_pro_dec_t), &dec_info);
}

uint16_t app_mgr_lc3_encode_req(void)
{
    aud_pro_enc_t enc_info;
    enc_info.aud_type = AUD_TYPE_LC3;
    
    enc_info.in_len = lc3_mgr_env->lc3_pro_buf.enc_in_len;
    enc_info.in_data = lc3_mgr_env->lc3_pro_buf.enc_in_buf;
    enc_info.out_data = lc3_mgr_env->lc3_pro_buf.enc_out_buf;
    return aud_send_msg_to_pro(AUD_PRO_MSG_ENC_REQ, sizeof(aud_pro_enc_t), &enc_info);
}

uint16_t app_mgr_lc3_check_start_process(void)
{
	uint16_t status = AUD_ERROR_NO;
    if(lc3_mgr_env->lc3_pkt_dcsp.frame_cnt >= AUD_LC3_START_FRAME_NUM)
    {
        aud_packet_info_t pkt_info;
        
        CLOGD("[LC3]start process!");
        aud_buf_data_out(&lc3_mgr_env->lc3_in_buf, lc3_mgr_env->lc3_pro_buf.dec_in_len, lc3_mgr_env->lc3_pro_buf.dec_in_buf);
        aud_buf_data_out(&lc3_mgr_env->lc3_pkt_dcsp.pkt_dscp, sizeof(aud_packet_info_t), (uint8_t *)&pkt_info);
        lc3_mgr_env->lc3_pkt_dcsp.frame_cnt--;
        lc3_mgr_env->lc3_pkt_dcsp.packet_cnt--;
        app_mgr_lc3_decode_req(0);
    }
    else
    {
        CLOGD("[LC3]wait more packet:%d", lc3_mgr_env->lc3_pkt_dcsp.frame_cnt);
        status = AUD_ERROR_NOT_ENOUGH_DATA;
    }
    return status;
}

uint16_t app_mgr_lc3_process_one_frame(void)
{
    //CLOGD("[LC3]pro one frame,cnt:%d, out_buf:%d", lc3_mgr_env->lc3_pkt_dcsp.frame_cnt, aud_buf_free_len(&lc3_mgr_env->lc3_out_buf));
	uint16_t status = AUD_ERROR_NO;

    if((lc3_mgr_env->lc3_pkt_dcsp.frame_cnt >= AUD_LC3_MIN_FRAME_NUM) \
        && (aud_buf_free_len(&lc3_mgr_env->lc3_out_buf) >= lc3_mgr_env->lc3_pro_buf.dec_out_len) \
        && (AUD_STATE_GET(lc3_mgr_env->lc3_state, AUD_MGR_STATE_PRO_MSK) != AUD_MGR_STATE_PROCESSING))
    {
        aud_packet_info_t pkt_info;
        
        aud_buf_data_out(&lc3_mgr_env->lc3_in_buf, lc3_mgr_env->lc3_pro_buf.dec_in_len, lc3_mgr_env->lc3_pro_buf.dec_in_buf);
        aud_buf_data_out(&lc3_mgr_env->lc3_pkt_dcsp.pkt_dscp, sizeof(aud_packet_info_t), (uint8_t *)&pkt_info);
        lc3_mgr_env->lc3_pkt_dcsp.frame_cnt--;
        lc3_mgr_env->lc3_pkt_dcsp.packet_cnt--;
        
        //CLOGD("[LC3]input data len:%d, frame cnt: %d, dec len:%d ",aud_buf_data_len(&lc3_mgr_env->lc3_in_buf), lc3_mgr_env->lc3_pkt_dcsp.frame_cnt, lc3_mgr_env->lc3_pro_buf.dec_in_len);
        app_mgr_lc3_decode_req(0);
    }
    else
    {
        CLOGD("[LC3]pro one frame no resource,state:%d, cnt:%d, out_buf:%d", AUD_STATE_GET(lc3_mgr_env->lc3_state, AUD_MGR_STATE_PROCESSING), \
            lc3_mgr_env->lc3_pkt_dcsp.frame_cnt, aud_buf_free_len(&lc3_mgr_env->lc3_out_buf));
        status = AUD_ERROR_NOT_ENOUGH_DATA;
    }
    return status;
}

uint16_t app_mgr_lc3_check_pcm_play(void)
{
    if(aud_buf_data_len(&lc3_mgr_env->lc3_out_buf) >= (lc3_mgr_env->lc3_pro_buf.dec_out_len * AUD_LC3_MIN_PLAY_PCM_NUM))
    {
        /// to do 
        CLOGD("[LC3]pcm_start play!");
    }
    return 0;
}

uint16_t app_mgr_lc3_rcv_data(uint16_t len, uint8_t *lc3_data)
{
    aud_packet_info_t pkt_info;
    uint16_t pkt_sta;  // 0: ok, 1: invalid, 2:lost
    
    uint32_t time_stamp = (lc3_data[3] << 24) | (lc3_data[2] << 16) | (lc3_data[1] << 8) | lc3_data[0];
    pkt_info.seq = (lc3_data[5] << 8) | lc3_data[4];
    pkt_info.packet_sta_len = ((lc3_data[7] << 8) | (lc3_data[6])) ;
    pkt_info.frame_num = 1;

    pkt_sta = AUD_LC3_PKT_STA_GET(pkt_info.packet_sta_len);
    if(pkt_sta)
    {
	    CLOGD("[LC3]rcv sta:%d len:%d, time:%d, seq:%d",pkt_sta, \
        AUD_LC3_PKT_LEN_GET(pkt_info.packet_sta_len),time_stamp, pkt_info.seq);
        return pkt_sta;
    }
    
    if((aud_buf_free_len(&lc3_mgr_env->lc3_in_buf) >= AUD_LC3_PKT_LEN_GET(pkt_info.packet_sta_len)) \
        && (aud_buf_free_len(&lc3_mgr_env->lc3_pkt_dcsp.pkt_dscp) >= sizeof(aud_packet_info_t)))
    {
        pkt_info.packet_pos = lc3_mgr_env->lc3_in_buf.write_pos;
        lc3_mgr_env->lc3_pkt_dcsp.packet_cnt++;
        lc3_mgr_env->lc3_pkt_dcsp.frame_cnt += pkt_info.frame_num;
        aud_buf_data_in(&lc3_mgr_env->lc3_pkt_dcsp.pkt_dscp, sizeof(aud_packet_info_t), (uint8_t *)&pkt_info);
        aud_buf_data_in(&lc3_mgr_env->lc3_in_buf, AUD_LC3_PKT_LEN_GET(pkt_info.packet_sta_len), &lc3_data[8]);
        //CLOGD("[LC3]buf data len:%d, frame cnt: %d ",aud_buf_data_len(&lc3_mgr_env->lc3_in_buf), lc3_mgr_env->lc3_pkt_dcsp.frame_cnt);
    }
    else
    {
        CLOGD("[LC3]aud buf full! buf free len:%d", aud_buf_free_len(&lc3_mgr_env->lc3_in_buf));
    }
    if((lc3_mgr_env->lc3_state & AUD_MGR_STATE_PRO_MSK) > AUD_MGR_STATE_PRO_INITING)
    {
        if(AUD_STATE_GET(lc3_mgr_env->lc3_state, AUD_MGR_STATE_PRO_MSK) == AUD_MGR_STATE_PRO_INITED)
        {
            app_mgr_lc3_check_start_process();
        }
        else
        {
            app_mgr_lc3_process_one_frame();
        }
    }
    return 0;
}

uint16_t app_mgr_lc3_req_cmp(aud_pro_cmp_t *cmp_info)
{
	uint16_t status = AUD_ERROR_NO;
    switch(cmp_info->op_id)
    {
        case AUD_PRO_MSG_INIT_REQ: 
        {
            AUD_STATE_SET(lc3_mgr_env->lc3_state, AUD_MGR_STATE_PRO_INITED);
            lc3_mgr_env->audio_cb->cb_aud_start_ind(AUD_TYPE_LC3, AUD_ERROR_NO);
        }break;
        case AUD_PRO_MSG_DEINIT_REQ: 
        {
            AUD_STATE_SET(lc3_mgr_env->lc3_state, AUD_MGR_STATE_IDLE);
            if(lc3_mgr_env)
            {
                lc3_mgr_env->audio_cb->cb_aud_stop_ind(AUD_TYPE_LC3, AUD_ERROR_NO);
                /// deinit input buf
                aud_buf_deinit(&lc3_mgr_env->lc3_in_buf);
                /// deinit input pkt dscp buf
                aud_buf_deinit(&lc3_mgr_env->lc3_pkt_dcsp.pkt_dscp);
                /// deinit out buf
                aud_buf_deinit(&lc3_mgr_env->lc3_out_buf);
                /// deinit process buf
                aud_pro_buf_deinit(&lc3_mgr_env->lc3_pro_buf);

                AUD_FREE(lc3_mgr_env);
                lc3_mgr_env = NULL;
            }
        }break;
        case AUD_PRO_MSG_ENC_REQ: 
        {
            AUD_STATE_SET(lc3_mgr_env->lc3_state, AUD_MGR_STATE_PROCESSED);
            lc3_mgr_env->lc3_pro_buf.enc_out_len = cmp_info->out_len;
        }break;
        case AUD_PRO_MSG_DEC_REQ: 
        {
            if(cmp_info->status != 0)
            {
                CLOGD("[LC3]dec cmp error, status:%d, len:%d", cmp_info->status, lc3_mgr_env->lc3_pro_buf.dec_out_len);
            }
            AUD_STATE_SET(lc3_mgr_env->lc3_state, AUD_MGR_STATE_PROCESSED);
            lc3_mgr_env->lc3_pro_buf.dec_out_len = cmp_info->out_len;
            aud_buf_data_in(&lc3_mgr_env->lc3_out_buf, lc3_mgr_env->lc3_pro_buf.dec_out_len, lc3_mgr_env->lc3_pro_buf.dec_out_buf);
#ifdef WIN32_DUMP_ISO_PCM
            ///save iso in file.
            {
                if(iso_pcm_file == NULL)
                {
                    iso_pcm_file = fopen("iso_data.pcm", "wb+");
                }
        
                if(iso_pcm_file != NULL)
                {
                    fwrite(lc3_mgr_env->lc3_pro_buf.dec_out_buf, 1, lc3_mgr_env->lc3_pro_buf.dec_out_len, iso_pcm_file);
                    fflush(iso_pcm_file);
                }
                else
                {
                    CLOGD("[LC3]open file failed!");
                }
            }
#endif

            if(AUD_STATE_GET(lc3_mgr_env->lc3_state, AUD_MGR_STATE_PLAY_MSK) != AUD_MGR_STATE_PCM_PLAYING)
            {
                app_mgr_lc3_check_pcm_play();
                AUD_STATE_SET(lc3_mgr_env->lc3_state, AUD_MGR_STATE_PCM_PLAYING);
            }
            app_mgr_lc3_process_one_frame();
            /// debug simulator pcm isr
            aud_send_msg_to_mgr(AUD_MSG_PLAY_DATA_IND, 0, NULL);
        }break;
        default :
        {
        	status = AUD_ERROR_NO;
        }
        break;
    }
    return status;
}

uint16_t app_mgr_lc3_msg_handle(uint16_t aud_msg_id, uint16_t len, uint8_t *msg_data)
{
    uint8_t msg_free = 1;
    switch(aud_msg_id)
    {
        case AUD_MSG_RCV_DATA_IND :
        {
            //CLOGD("[LC3]:buf_addr:0x%x", *(uint32_t *)msg_data);
            app_mgr_lc3_rcv_data(len, msg_data);
        }
        break;
        case AUD_MSG_PLAY_DATA_IND :
        {   
            /// to do
            // debug
            CLOGD("[LC3]:PLAY IND");
            aud_buf_data_out(&lc3_mgr_env->lc3_out_buf, lc3_mgr_env->lc3_pro_buf.dec_out_len, lc3_mgr_env->lc3_pro_buf.dec_out_buf);
            app_mgr_lc3_process_one_frame();
            msg_free = 0;
        }
        break;
        case AUD_MSG_SYNC_INFO_IND :
        {
            
        }
        break;
        case AUD_MSG_MUTE :
        {
            
        }
        break;
        case AUD_MSG_PRO_CMP_IND : 
        {
            app_mgr_lc3_req_cmp((aud_pro_cmp_t *)msg_data);
        }
        break;
        default : break;
    }
    return msg_free;
}


/// @} AUDIO




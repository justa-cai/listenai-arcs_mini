/**
 ****************************************************************************************
 * @file aud_mgr.c
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
#include "aud_mgr_sbc.h"
#include "aud_pro.h"
#include "aud_dac.h"

/*
 * MACROS
 ****************************************************************************************
 */

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
aud_mgr_sbc_t *sbc_mgr_env = NULL;

/*
 * LOCAL FUNCTIONS
 ****************************************************************************************
 */
/*
 * GLOBAL FUNCTIONS
 ****************************************************************************************
 */
uint16_t audio_mgr_get_sbc_svr(audio_service_t *sbc_svr)
{
    if(sbc_svr)
    {
        sbc_svr->aud_start            = app_mgr_sbc_start;
        sbc_svr->aud_stop             = app_mgr_sbc_stop;
        sbc_svr->aud_pause            = app_mgr_sbc_pause;
        sbc_svr->aud_resume           = app_mgr_sbc_resume;
        sbc_svr->aud_get_media_info   = NULL;
        sbc_svr->aud_msg_handle       = app_mgr_sbc_msg_handle;
        
        return AUD_ERROR_NO;
    }
    else
    {
        return AUD_ERROR_PARAM_NULL;
    }
}

uint16_t app_mgr_sbc_start(aud_play_info_t *aud_info, aud_cb_t *audio_cb, aud_3party_process_t *aud_3party_process)
{
    uint8_t status = AUD_ERROR_NO;
    uint16_t one_frame_output_size = 0;
    
    CLOGD("app sbc mgr start, sbc_mgr_env:0x%x", sbc_mgr_env);
    if(sbc_mgr_env == NULL)
    {
        sbc_mgr_env = AUD_MALLOC(sizeof(aud_mgr_sbc_t));
        if(sbc_mgr_env == NULL)
        {
            return AUD_ERROR_BUF_NO_RESOURCE;
        }
        else
        {
            memset(sbc_mgr_env, 0x00, sizeof(aud_mgr_sbc_t));
        }
    }
    else
    {
        memset(sbc_mgr_env, 0x00, sizeof(aud_mgr_sbc_t));
    }
    /// infomation init
    sbc_mgr_env->sbc_info.ch             = aud_info->aud_ch;
    sbc_mgr_env->sbc_info.freq           = aud_info->aud_sample;
    sbc_mgr_env->sbc_info.frame_ms       = aud_info->frame_dur;

    sbc_mgr_env->audio_cb = audio_cb;
    sbc_mgr_env->sbc_3party_pro = aud_3party_process;
    /// init input buf
    aud_buf_init(&sbc_mgr_env->sbc_in_buf, AUD_SBC_INPUT_BUF_SIZE);
    /// init input pkt dscp buf
    aud_buf_init(&sbc_mgr_env->sbc_pkt_dcsp.pkt_dscp, AUD_SBC_PKT_DSCP_BUF_SIZE);
    /// init output buf
    one_frame_output_size = 256  * sbc_mgr_env->sbc_info.ch;
    aud_buf_init(&sbc_mgr_env->sbc_out_buf, one_frame_output_size * AUD_SBC_PLAY_FRAME_NUM);
    
    CLOGD("aud buf init, size in:%d, out:%d, dscp:%d", sbc_mgr_env->sbc_in_buf.buf_size, sbc_mgr_env->sbc_out_buf.buf_size, sbc_mgr_env->sbc_pkt_dcsp.pkt_dscp.buf_size);
    /// process buf
    sbc_mgr_env->sbc_pro_buf.dec_in_len = 0;
    sbc_mgr_env->sbc_pro_buf.dec_out_len = one_frame_output_size * AUD_SBC_REQ_DEC_FRAME_NUM;
    aud_pro_buf_init(&sbc_mgr_env->sbc_pro_buf, AUD_SBC_DEC_IN_BUF_SIZE, 0, \
                     0, 0);
    /// init codec
    return app_mgr_sbc_codec_init_req();
}
uint16_t app_mgr_sbc_stop(void)
{
    app_dac_out_stop();
    /// deinit codec
    return app_mgr_sbc_codec_deinit_req();

}
uint16_t app_mgr_sbc_pause(void)
{
	return app_mgr_sbc_stop();
}
uint16_t app_mgr_sbc_resume(aud_play_info_t *aud_info, aud_cb_t *audio_cb, aud_3party_process_t *aud_3party_process)
{
	return app_mgr_sbc_start(aud_info, audio_cb, aud_3party_process);
}


uint16_t app_mgr_sbc_codec_init_req(void)
{
    aud_pro_init_t init_info;
    init_info.aud_type = AUD_TYPE_SBC;
    init_info.sample_rate = sbc_mgr_env->sbc_info.freq;
    init_info.ch = sbc_mgr_env->sbc_info.ch;
    init_info.frame_ms = sbc_mgr_env->sbc_info.frame_ms;
    //app_sbc_start(sbc_mgr_env->sbc_info.freq, sbc_mgr_env->sbc_info.ch, sbc_mgr_env->sbc_info.frame_ms, \
    //                sbc_mgr_env->sbc_info.out_bits, sbc_mgr_env->sbc_info.hr_mode);
    sbc_mgr_env->sbc_state = AUD_MGR_STATE_PRO_INITING;
    
    CLOGD("aud mgr sbc init req.");
    return aud_send_msg_to_pro(AUD_PRO_MSG_INIT_REQ, sizeof(aud_pro_init_t), &init_info);
}

uint16_t app_mgr_sbc_codec_deinit_req(void)
{
    aud_pro_deinit_t deinit_info;
    deinit_info.aud_type = AUD_TYPE_SBC;
    //app_sbc_stop();
    return aud_send_msg_to_pro(AUD_PRO_MSG_DEINIT_REQ, sizeof(aud_pro_deinit_t), &deinit_info);
}

uint16_t app_mgr_sbc_decode_req(uint8_t audio_bfi)
{
    aud_pro_dec_t dec_info;
    dec_info.aud_type = AUD_TYPE_SBC;

    //CLOGD("[sbc]dec req!,addr:0x%x, header:0x%x", sbc_mgr_env->sbc_pro_buf.dec_in_buf, sbc_mgr_env->sbc_pro_buf.dec_in_buf[0]);

    dec_info.in_len = sbc_mgr_env->sbc_pro_buf.dec_in_len;
    dec_info.in_data = sbc_mgr_env->sbc_pro_buf.dec_in_buf;
    dec_info.out_data = sbc_mgr_env->sbc_pro_buf.dec_out_buf;
    dec_info.dec_frame_num = AUD_SBC_REQ_DEC_FRAME_NUM;
    AUD_STATE_SET(sbc_mgr_env->sbc_state, AUD_MGR_STATE_PROCESSING);
    return aud_send_msg_to_pro(AUD_PRO_MSG_DEC_REQ, sizeof(aud_pro_dec_t), &dec_info);
}

uint16_t app_mgr_sbc_encode_req(void)
{
    aud_pro_enc_t enc_info;
    enc_info.aud_type = AUD_TYPE_SBC;
    
    enc_info.in_len = sbc_mgr_env->sbc_pro_buf.enc_in_len;
    enc_info.in_data = sbc_mgr_env->sbc_pro_buf.enc_in_buf;
    enc_info.out_data = sbc_mgr_env->sbc_pro_buf.enc_out_buf;
    return aud_send_msg_to_pro(AUD_PRO_MSG_ENC_REQ, sizeof(aud_pro_enc_t), &enc_info);
}

uint16_t app_mgr_sbc_check_start_process(void)
{
    if(sbc_mgr_env->sbc_pkt_dcsp.frame_cnt >= AUD_SBC_START_FRAME_NUM)
    {
        aud_packet_info_t pkt_info;
        
        CLOGD("[sbc]start process!");
        aud_buf_data_out(&sbc_mgr_env->sbc_in_buf, AUD_SBC_DEC_IN_BUF_SIZE, sbc_mgr_env->sbc_pro_buf.dec_in_buf);
        sbc_mgr_env->sbc_pro_buf.dec_in_len = AUD_SBC_DEC_IN_BUF_SIZE;
        //point to sbc_out_buf!!!
        sbc_mgr_env->sbc_pro_buf.dec_out_buf = aud_buf_data_in_ptr(&sbc_mgr_env->sbc_out_buf);
        //aud_buf_data_out(&sbc_mgr_env->sbc_pkt_dcsp.pkt_dscp, sizeof(aud_packet_info_t), &pkt_info);
        //sbc_mgr_env->sbc_pkt_dcsp.frame_cnt--;
        //sbc_mgr_env->sbc_pkt_dcsp.packet_cnt--;
        app_mgr_sbc_decode_req(0);
    }
    else
    {
        CLOGD("[sbc]wait more packet:%d", sbc_mgr_env->sbc_pkt_dcsp.frame_cnt);
    }
    return 0;
}

uint16_t app_mgr_sbc_process_one_frame(uint8_t type)
{
    //CLOGD("pro,in:%d %d,outfre:%d", sbc_mgr_env->sbc_pkt_dcsp.frame_cnt, aud_buf_data_len(&sbc_mgr_env->sbc_in_buf), aud_buf_free_len(&sbc_mgr_env->sbc_out_buf));
    //CLOGD("ring buf,addr:0x%x, header:0x%x", &sbc_mgr_env->sbc_in_buf.buf[sbc_mgr_env->sbc_in_buf.read_pos], sbc_mgr_env->sbc_in_buf.buf[sbc_mgr_env->sbc_in_buf.read_pos]);

    if((sbc_mgr_env->sbc_pkt_dcsp.frame_cnt >= AUD_SBC_MIN_FRAME_NUM) \
        && (aud_buf_free_len(&sbc_mgr_env->sbc_out_buf) >= sbc_mgr_env->sbc_pro_buf.dec_out_len) \
        && (AUD_STATE_GET(sbc_mgr_env->sbc_state, AUD_MGR_STATE_PRO_MSK) != AUD_MGR_STATE_PROCESSING))
    {
        aud_packet_info_t pkt_info;
        aud_buf_data_out(&sbc_mgr_env->sbc_in_buf, AUD_SBC_DEC_IN_BUF_SIZE - sbc_mgr_env->sbc_pro_buf.dec_in_len, \
                        sbc_mgr_env->sbc_pro_buf.dec_in_buf + sbc_mgr_env->sbc_pro_buf.dec_in_len);
        //CLOGD("sbc data addr:0x%x, header:0x%x", sbc_mgr_env->sbc_pro_buf.dec_in_len, sbc_mgr_env->sbc_pro_buf.dec_in_buf[0]);
        sbc_mgr_env->sbc_pro_buf.dec_in_len = AUD_SBC_DEC_IN_BUF_SIZE;
        //point to sbc_out_buf!!!
        sbc_mgr_env->sbc_pro_buf.dec_out_buf = aud_buf_data_in_ptr(&sbc_mgr_env->sbc_out_buf);
        //aud_buf_data_out(&sbc_mgr_env->sbc_pkt_dcsp.pkt_dscp, sizeof(aud_packet_info_t), &pkt_info);
        //sbc_mgr_env->sbc_pkt_dcsp.frame_cnt--;
        //sbc_mgr_env->sbc_pkt_dcsp.packet_cnt--;
        app_mgr_sbc_decode_req(0);
    }
    else
    {
        if((AUD_STATE_GET(sbc_mgr_env->sbc_state, AUD_MGR_STATE_PRO_MSK) != AUD_MGR_STATE_PROCESSING) && (sbc_mgr_env->sbc_pkt_dcsp.frame_cnt >= AUD_SBC_MIN_FRAME_NUM))
        {
            static uint8_t log_count = 10;
            if(log_count++ >= 10)
            {
                //CLOGD("[sbc]pro fail,tp:%d,sta:%d,len:%d, cnt:%d,out_fre:%d", type, AUD_STATE_GET(sbc_mgr_env->sbc_state, AUD_MGR_STATE_PRO_MSK), \
                //    aud_buf_data_len(&sbc_mgr_env->sbc_in_buf), sbc_mgr_env->sbc_pkt_dcsp.frame_cnt, aud_buf_free_len(&sbc_mgr_env->sbc_out_buf));
                log_count = 0;
            }
        }
    }
    //check play buf is empty and to start play pcm.
    if(sbc_mgr_env->sbc_play_empty == 1)
    {
        app_mgr_sbc_pcm_play();
    }
    return 0;
}

//void app_mgr_sbc_pcm_play_isr(uint8_t status)
//{
//    aud_play_ind_t ind;
//    ind.status = status;
//    //CLOGI("pcm play isr");
//    aud_send_msg_to_mgr(AUD_MSG_PLAY_DATA_IND, sizeof(aud_play_ind_t), &ind);
//} 

void app_mgr_sbc_pcm_play_isr(uint8_t status)
{
    aud_play_ind_t ind;
    ind.status = status;

    aud_send_isr_msg_to_mgr(AUD_MSG_PLAY_DATA_IND, sizeof(aud_play_ind_t), &ind);
}

uint16_t app_mgr_sbc_check_pcm_play(void)
{
    CLOGD("[sbc]check play start!:%d", aud_buf_data_len(&sbc_mgr_env->sbc_out_buf));

    if(aud_buf_data_len(&sbc_mgr_env->sbc_out_buf) >= (sbc_mgr_env->sbc_pro_buf.dec_out_len * (AUD_SBC_START_FRAME_NUM / AUD_SBC_REQ_DEC_FRAME_NUM)))
    {
        aud_dac_out_cfg_t dac_cfg;
        dac_cfg.ch = sbc_mgr_env->sbc_info.ch;
        dac_cfg.out_bits = 16;
        dac_cfg.sample_rate = sbc_mgr_env->sbc_info.freq;
        dac_cfg.play_len = sbc_mgr_env->sbc_pro_buf.dec_out_len;
        dac_cfg.play_buf = aud_buf_data_out_ptr(&sbc_mgr_env->sbc_out_buf);
        dac_cfg.play_cb = (dac_play_isr_cb)app_mgr_sbc_pcm_play_isr;
        app_dac_out_init(&dac_cfg);
        CLOGD("[sbc]check play start ok!");
        sbc_mgr_env->sbc_play_empty = 0;
        AUD_STATE_SET(sbc_mgr_env->sbc_state, AUD_MGR_STATE_PCM_PLAYING);
    }
    return 0;
}

uint16_t app_mgr_sbc_pcm_play(void)
{
	uint8_t status = 0xff;
    if(aud_buf_data_len(&sbc_mgr_env->sbc_out_buf) >= (sbc_mgr_env->sbc_pro_buf.dec_out_len * (AUD_SBC_MIN_PLAY_PCM_NUM / AUD_SBC_REQ_DEC_FRAME_NUM)))
    {
        aud_dac_out_cfg_t dac_cfg;
        sbc_mgr_env->sbc_play_empty = 0;
        dac_cfg.ch = sbc_mgr_env->sbc_info.ch;
        dac_cfg.out_bits = 16;
        dac_cfg.sample_rate = sbc_mgr_env->sbc_info.freq;
        dac_cfg.play_len = sbc_mgr_env->sbc_pro_buf.dec_out_len;
        dac_cfg.play_buf = aud_buf_data_out_ptr(&sbc_mgr_env->sbc_out_buf);
        dac_cfg.play_cb = (dac_play_isr_cb)app_mgr_sbc_pcm_play_isr;
        app_dac_out_play(&dac_cfg);
        status = 0;
        CLOGD("continu:%d",aud_buf_data_len(&sbc_mgr_env->sbc_out_buf));
    }
    else
    {
        //CLOGD("continu failed!:%d",aud_buf_data_len(&sbc_mgr_env->sbc_out_buf));
    }
    return status;
}

uint16_t app_mgr_sbc_rcv_data(uint16_t len, uint8_t *pkt_data)
{
    bt_aud_pkt_info_t *bt_pkt = (bt_aud_pkt_info_t *)pkt_data;

    aud_packet_info_t pkt_info;

    pkt_info.seq = bt_pkt->seq;
    pkt_info.packet_sta_len = bt_pkt->len;
    pkt_info.frame_num = bt_pkt->frame_num;
    if((bt_pkt->data[0] != 0x9c) || (bt_pkt->frame_num == 0))
    {
        CLOGD("[sbc]rcv err data:s:%d,l:%d,n:%d,h:0x%x",pkt_info.seq, \
            pkt_info.packet_sta_len, pkt_info.frame_num, bt_pkt->data[0]);
        return 0xff;
    }

    //if(AUD_SBC_PKT_STA_GET(pkt_info.packet_sta_len))
    //{
    //    CLOGD("[sbc]rcv sta:%d len:%d, seq:%d",AUD_SBC_PKT_STA_GET(pkt_info.packet_sta_len), \
    //    AUD_SBC_PKT_LEN_GET(pkt_info.packet_sta_len), pkt_info.seq);
    //    return;
    //}
    
    //if((aud_buf_free_len(&sbc_mgr_env->sbc_in_buf) >= AUD_SBC_PKT_LEN_GET(pkt_info.packet_sta_len)) \
        //&& (aud_buf_free_len(&sbc_mgr_env->sbc_pkt_dcsp.pkt_dscp) >= sizeof(aud_packet_info_t)))
    if((aud_buf_free_len(&sbc_mgr_env->sbc_in_buf) >= AUD_SBC_PKT_LEN_GET(pkt_info.packet_sta_len)))
    {
        //pkt_info.packet_pos = sbc_mgr_env->sbc_in_buf.write_pos;
        //sbc_mgr_env->sbc_pkt_dcsp.packet_cnt++;
        sbc_mgr_env->sbc_pkt_dcsp.frame_cnt += pkt_info.frame_num;
        //aud_buf_data_in(&sbc_mgr_env->sbc_pkt_dcsp.pkt_dscp, sizeof(aud_packet_info_t), &pkt_info);
        aud_buf_data_in(&sbc_mgr_env->sbc_in_buf, AUD_SBC_PKT_LEN_GET(pkt_info.packet_sta_len), bt_pkt->data);
        if(sbc_mgr_env->sbc_pkt_dcsp.frame_cnt > 15)
        {
            //CLOGD("[sbc]r len:%d,cnt:%d ",aud_buf_data_len(&sbc_mgr_env->sbc_in_buf), sbc_mgr_env->sbc_pkt_dcsp.frame_cnt);
        }
    }
    else
    {
        static uint8_t log_count = 20;
        if(log_count++ >= 20)
        {
            CLOGD("[sbc]full,free:%d,cnt:%d", aud_buf_free_len(&sbc_mgr_env->sbc_in_buf), sbc_mgr_env->sbc_pkt_dcsp.frame_cnt);
            log_count = 0;
        }
    }

    if((sbc_mgr_env->sbc_state & AUD_MGR_STATE_PRO_MSK) > AUD_MGR_STATE_PRO_INITING)
    {
        if(AUD_STATE_GET(sbc_mgr_env->sbc_state, AUD_MGR_STATE_PRO_MSK) == AUD_MGR_STATE_PRO_INITED)
        {
            CLOGD("[sbc]state:%d", sbc_mgr_env->sbc_state);
            app_mgr_sbc_check_start_process();
        }
        else
        {
            app_mgr_sbc_process_one_frame(1);
        }
    }
    return 0;
}

uint16_t app_mgr_sbc_req_cmp(aud_pro_cmp_t *cmp_info)
{
    switch(cmp_info->op_id)
    {
        case AUD_PRO_MSG_INIT_REQ: 
        {
            AUD_STATE_SET(sbc_mgr_env->sbc_state, AUD_MGR_STATE_PRO_INITED);
            sbc_mgr_env->audio_cb->cb_aud_start_ind(AUD_TYPE_SBC, AUD_ERROR_NO);
        }break;
        case AUD_PRO_MSG_DEINIT_REQ: 
        {
            AUD_STATE_SET(sbc_mgr_env->sbc_state, AUD_MGR_STATE_IDLE);
            if(sbc_mgr_env)
            {
                /// deinit input buf
                aud_buf_deinit(&sbc_mgr_env->sbc_in_buf);
                /// deinit input pkt dscp buf
                aud_buf_deinit(&sbc_mgr_env->sbc_pkt_dcsp.pkt_dscp);
                /// deinit out buf
                aud_buf_deinit(&sbc_mgr_env->sbc_out_buf);
                /// deinit process buf
                aud_pro_buf_deinit(&sbc_mgr_env->sbc_pro_buf);

                sbc_mgr_env->audio_cb->cb_aud_stop_ind(AUD_TYPE_SBC, AUD_ERROR_NO);

                AUD_FREE(sbc_mgr_env);
                sbc_mgr_env = NULL;
            }
            CLOGD("sbc mgr deinit cmp!");
        }break;
        case AUD_PRO_MSG_ENC_REQ: 
        {
            AUD_STATE_SET(sbc_mgr_env->sbc_state, AUD_MGR_STATE_PROCESSED);
            sbc_mgr_env->sbc_pro_buf.enc_out_len = cmp_info->out_len;
        }break;
        case AUD_PRO_MSG_DEC_REQ: 
        {
            uint8_t status = cmp_info->status >> 4;
            uint8_t consume_num = cmp_info->status & 0xf;
            
            AUD_STATE_SET(sbc_mgr_env->sbc_state, AUD_MGR_STATE_PROCESSED);
            if(status != 0)
            {
                CLOGD("[sbc]dec cmp error, status:%d, consum:%d, out:%d", cmp_info->status, cmp_info->consume_len,cmp_info->out_len);
            }
            else
            {
                sbc_mgr_env->sbc_pkt_dcsp.frame_cnt -= consume_num;
                sbc_mgr_env->sbc_pro_buf.dec_out_len = cmp_info->out_len;
                //aud_buf_data_in(&sbc_mgr_env->sbc_out_buf, sbc_mgr_env->sbc_pro_buf.dec_out_len, sbc_mgr_env->sbc_pro_buf.dec_out_buf);
                aud_buf_data_async_in(&sbc_mgr_env->sbc_out_buf, sbc_mgr_env->sbc_pro_buf.dec_out_len);
                sbc_mgr_env->sbc_pro_buf.dec_in_len -= cmp_info->consume_len;
                ///move remain data to the begain of buf.
                memcpy(sbc_mgr_env->sbc_pro_buf.dec_in_buf, sbc_mgr_env->sbc_pro_buf.dec_in_buf + cmp_info->consume_len, sbc_mgr_env->sbc_pro_buf.dec_in_len);
            }
            //CLOGD("[sbc]d len:%d,cnt:%d ",aud_buf_data_len(&sbc_mgr_env->sbc_in_buf), sbc_mgr_env->sbc_pkt_dcsp.frame_cnt);
            
            if(AUD_STATE_GET(sbc_mgr_env->sbc_state, AUD_MGR_STATE_PLAY_MSK) != AUD_MGR_STATE_PCM_PLAYING)
            {
                app_mgr_sbc_check_pcm_play();
            }
            app_mgr_sbc_process_one_frame(0);
#if (AUD_SBC_SIMU_PCM_PLAY == 1)
            /// debug simulator pcm isr
            aud_send_msg_to_mgr(AUD_MSG_PLAY_DATA_IND, 0, NULL);
#endif
        }break;
        default :break;
    }
    return 0;
}

uint16_t app_mgr_sbc_msg_handle(uint16_t aud_msg_id, uint16_t len, uint8_t *msg_data)
{
    uint8_t msg_free = 1;

    switch(aud_msg_id)
    {
        case AUD_MSG_RCV_DATA_IND :
        {
            //CLOGD("[sbc]:buf_addr:0x%x", *(uint32_t *)msg_data);
            app_mgr_sbc_rcv_data(len, msg_data);
        }
        break;
        case AUD_MSG_PLAY_DATA_IND :
        {   
            aud_play_ind_t *ind = (aud_play_ind_t *)msg_data;
            if(ind->status != 0)
            {
                CLOGD("PLAY IND ERROR,pcm len:%d",aud_buf_data_len(&sbc_mgr_env->sbc_out_buf));
            }
            else
            {

                aud_buf_data_async_out(&sbc_mgr_env->sbc_out_buf, sbc_mgr_env->sbc_pro_buf.dec_out_len);
                
                //CLOGD("PLAY IND,pcm len:%d",aud_buf_data_len(&sbc_mgr_env->sbc_out_buf));
                if(aud_buf_data_len(&sbc_mgr_env->sbc_out_buf) >= (sbc_mgr_env->sbc_pro_buf.dec_out_len * (AUD_SBC_MIN_PLAY_PCM_NUM / AUD_SBC_REQ_DEC_FRAME_NUM)))
                {
                    aud_dac_out_cfg_t dac_cfg;
                    dac_cfg.ch = sbc_mgr_env->sbc_info.ch;
                    dac_cfg.out_bits = 16;
                    dac_cfg.sample_rate = sbc_mgr_env->sbc_info.freq;
                    dac_cfg.play_len = sbc_mgr_env->sbc_pro_buf.dec_out_len;
                    dac_cfg.play_buf = aud_buf_data_out_ptr(&sbc_mgr_env->sbc_out_buf);
                    dac_cfg.play_cb = (dac_play_isr_cb)app_mgr_sbc_pcm_play_isr;
                    app_dac_out_play(&dac_cfg);
                    //CLOGD("play:0x%x", dac_cfg.play_buf);
                }
                else
                {
                    sbc_mgr_env->sbc_play_empty = 1;
                    CLOGD("empty:%d", aud_buf_data_len(&sbc_mgr_env->sbc_out_buf));
                }

#if (AUD_SBC_SIMU_PCM_PLAY == 0)
                app_mgr_sbc_process_one_frame(2);
#endif
            }
            ///isr msg use fixed buf.
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
            app_mgr_sbc_req_cmp((aud_pro_cmp_t *)msg_data);
        }
        break;
        default : break;
    }
    return msg_free;
}


/// @} AUDIO




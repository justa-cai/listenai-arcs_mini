/**
 ****************************************************************************************
 * @file aud_pro.c
 *
 * @brief  audio process source
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
#include "aud_pro.h"
#include "aud_mgr.h"

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
extern uint8_t app_msbc_start(void);
extern uint8_t app_msbc_stop(void);
extern uint8_t app_msbc_enc(uint16_t in_len, uint8_t *in_data, uint16_t *out_len, uint8_t *out_data, int16_t *consume_len, uint8_t frame_num);
extern uint8_t app_msbc_dec(uint16_t in_len, uint8_t *in_data, uint16_t *out_len, uint8_t *out_data, int16_t *consume_len, uint8_t frame_num);

extern uint8_t app_sbc_start(uint16_t sample_rate, uint8_t channels);
extern uint8_t app_sbc_stop(void);
extern uint8_t app_sbc_dec(uint16_t in_len, uint8_t *in_data, uint16_t *out_len, uint8_t *out_data, int16_t *consume_len, uint8_t frame_num);
/*
 * LOCAL VARIABLES
 ****************************************************************************************
 */

/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */

/*
 * LOCAL FUNCTIONS
 ****************************************************************************************
 */

/*
 * GLOBAL FUNCTIONS
 ****************************************************************************************
 */
 uint16_t aud_pro_init(aud_pro_init_t *init_info)
{
    uint16_t status = AUD_ERROR_NO;

    switch(init_info->aud_type)
    {
        case AUD_TYPE_LC3 :
        {
            //status = app_lc3_start(init_info->sample_rate, init_info->ch, init_info->frame_ms, init_info->out_bits, init_info->hr_mode);
        }break;
        case AUD_TYPE_SBC :
        {
            status = app_sbc_start(init_info->sample_rate, init_info->ch);
        }break;
        case AUD_TYPE_MSBC :
        {
            status = app_msbc_start();
        }break;

        
        default : break;
    }
    return status;
}

uint16_t aud_pro_deinit(aud_pro_deinit_t *deinit_info)
{
    uint16_t status = AUD_ERROR_NO;

    switch(deinit_info->aud_type)
    {
        case AUD_TYPE_LC3 :
        {
            //status = app_lc3_stop();
        }break;
        case AUD_TYPE_SBC :
        {
            status = app_sbc_stop();
        }break;
        case AUD_TYPE_MSBC :
        {
            status = app_msbc_stop();
        }break;
        default : break;
    }
    return status;
}
uint16_t aud_pro_enc_req(aud_pro_enc_t *enc_req)
{
    uint16_t status = AUD_ERROR_NO;
    switch(enc_req->aud_type)
    {
        case AUD_TYPE_LC3 :
        {
        }break;
        case AUD_TYPE_SBC :
        {
        }break;
        case AUD_TYPE_MSBC :
        {
            status = app_msbc_enc(enc_req->in_len, enc_req->in_data, &enc_req->out_len, enc_req->out_data, (int16_t *)&enc_req->consum_len, enc_req->enc_frame_num);
        }break;

        
        default : break;
    }
    return status;
}
uint16_t aud_pro_dec_req(aud_pro_dec_t *dec_req)
{
    uint16_t status = AUD_ERROR_NO;

    switch(dec_req->aud_type)
    {
        case AUD_TYPE_LC3 :
        {
            //CLOGD("[LC3] dec start!");
            //status = app_lc3_dec(dec_req->in_len, dec_req->in_data, &dec_req->out_len, dec_req->out_data, dec_req->aud_bfi);
            //CLOGD("[LC3] dec end!");
        }break;
        case AUD_TYPE_SBC :
        {
            //CLOGD("[SBC] dec s");
            status = app_sbc_dec(dec_req->in_len, dec_req->in_data, &dec_req->out_len, dec_req->out_data, (int16_t *)&dec_req->consum_len, dec_req->dec_frame_num);
            //CLOGD("[SBC] dec e");
        }break;
        case AUD_TYPE_MSBC :
        {
            //CLOGD("[SBC] dec s");
            status = app_msbc_dec(dec_req->in_len, dec_req->in_data, &dec_req->out_len, dec_req->out_data, (int16_t *)&dec_req->consum_len, dec_req->dec_frame_num);
            //CLOGD("[SBC] dec e");
        }break;

        default : break;
    }
    return status;
}

uint8_t aud_pro_msg_handle(uint16_t aud_msg_id, uint16_t len, uint8_t *msg_data)
{
    uint8_t msg_free = 1;

    aud_pro_cmp_t cmp_info;
    cmp_info.op_id = aud_msg_id;
    cmp_info.status = AUD_ERROR_MSG_ID_NO;

    switch(aud_msg_id)
    {
        case AUD_PRO_MSG_INIT_REQ :
        {
            aud_pro_init_t *init_info = (aud_pro_init_t *)msg_data;
            cmp_info.status = aud_pro_init((aud_pro_init_t *)msg_data);
            cmp_info.aud_type = init_info->aud_type;
        }
        break;
        case AUD_PRO_MSG_DEINIT_REQ :
        {
            aud_pro_deinit_t *deinit_info = (aud_pro_deinit_t *)msg_data;
            cmp_info.status = aud_pro_deinit(deinit_info);
            cmp_info.aud_type = deinit_info->aud_type;
        }
        break;
        case AUD_PRO_MSG_ENC_REQ :
        {
            aud_pro_enc_t *enc_req = (aud_pro_enc_t *)msg_data;
            cmp_info.status = aud_pro_enc_req(enc_req);
            cmp_info.aud_type = enc_req->aud_type;
            cmp_info.out_len = enc_req->out_len;
        }
        break;
        case AUD_PRO_MSG_DEC_REQ :
        {
            aud_pro_dec_t *dec_req = (aud_pro_dec_t *)msg_data;
            cmp_info.status = aud_pro_dec_req((aud_pro_dec_t *)msg_data);
            cmp_info.aud_type = dec_req->aud_type;
            cmp_info.out_len = dec_req->out_len;
            cmp_info.consume_len = dec_req->consum_len;
        }
        break;
        default : break;
    }

    aud_send_msg_to_mgr(AUD_MSG_PRO_CMP_IND, sizeof(aud_pro_cmp_t), &cmp_info);
    return msg_free;
}


/// @} AUDIO




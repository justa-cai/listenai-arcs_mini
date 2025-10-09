/**
 ****************************************************************************************
 *
 * @file aud_mgr.h
 *
 * @brief audio modules
 *
 * Copyright (C) Listenai.com 2023
 *
 *
 ****************************************************************************************
 */

#ifndef AUD_PRO_H_
#define AUD_PRO_H_

/**
 ****************************************************************************************
 * @addtogroup AUDIO
 * @{
 * @name audio process api
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include <stdint.h>
#include "aud_common.h"

/*
 * MACRO DEFINITIONS
 ****************************************************************************************
 */
 
/*
 * DEFINES
 ****************************************************************************************
 */
enum aud_pro_msg
{
   AUD_PRO_MSG_INIT_REQ,
   AUD_PRO_MSG_DEINIT_REQ,

   AUD_PRO_MSG_ENC_REQ,

   AUD_PRO_MSG_DEC_REQ,
};

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */
typedef struct aud_pro_init
{
    uint8_t   aud_type;
    
    uint8_t   ch;
    uint8_t   frame_ms;
    uint8_t   out_bits;
    uint16_t  sample_rate;
    uint8_t   hr_mode;
}aud_pro_init_t;

typedef struct aud_pro_deinit
{
    uint8_t   aud_type;
}aud_pro_deinit_t;

typedef struct aud_pro_enc
{
    uint8_t   aud_type;
    uint8_t   aud_bfi;
    
    uint8_t   enc_frame_num;
    uint16_t  consum_len;
    uint16_t  in_len;
    uint16_t  out_len;
    uint8_t   *in_data;
    uint8_t   *out_data;
}aud_pro_enc_t;

typedef struct aud_pro_dec
{
    uint8_t   aud_type;
    uint8_t   aud_bfi;

    uint8_t   dec_frame_num;
    uint16_t  consum_len;
    uint16_t  in_len;
    uint16_t  out_len;
    uint8_t   *in_data;
    uint8_t   *out_data;
}aud_pro_dec_t;

typedef struct aud_pro_cmp
{
    uint8_t   aud_type;
    uint8_t   op_id;
    
    uint8_t   status;
    uint16_t  out_len;
    uint16_t  consume_len;
}aud_pro_cmp_t;


/*@TRACE*/
/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
 */
uint8_t aud_pro_msg_handle(uint16_t aud_msg_id, uint16_t len, uint8_t *msg_data);
uint16_t aud_pro_init(aud_pro_init_t *init_info);

/// @} audio process api
/// @} AUDIO

#endif // AUD_PRO_H_

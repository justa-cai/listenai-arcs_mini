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

#ifndef AUD_MGR_BUF_H_
#define AUD_MGR_BUF_H_

/**
 ****************************************************************************************
 * @addtogroup AUDIO
 * @{
 * @name audio manager api
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include <stdint.h>
#include"aud_common.h"
/*
 * MACRO DEFINITIONS
 ****************************************************************************************
 */
 
/*
 * DEFINES
 ****************************************************************************************
 */
enum buf_state
{
   AUD_BUF_EMPTY = 0,
   AUD_BUF_VAILD,
   AUD_BUF_FULL,
};

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */
 /// data from app or host
typedef struct aud_buf_info_
{
    uint16_t  buf_size;
    
    uint16_t  read_pos;
    uint16_t  write_pos;
    uint8_t   read_ring;
    uint8_t   write_ring;

    uint8_t   *buf;
}aud_buf_info_t;
/// put data to codec
typedef struct aud_out_buf_info
{
    uint16_t  buf_size;
    uint16_t  data_len;

    uint8_t   *buf;
}aud_out_buf_info_t;
/// buf interface for audio process task
typedef struct aud_pro_buf_info
{
    /// dec buf remain data length.
    uint16_t   dec_in_len;
    uint16_t   dec_out_len;
    uint8_t   *dec_in_buf;
    uint8_t   *dec_out_buf;

    uint16_t   enc_in_len;
    uint16_t   enc_out_len;
    uint8_t   *enc_in_buf;
    uint8_t   *enc_out_buf;
}aud_pro_buf_info_t;
/// all receive audio information
typedef struct aud_packet_info
{
    uint16_t  packet_pos;
    // 15:14 pky sta 0:ok,1:invalid, 2:lost
    uint16_t  packet_sta_len;

    //uint32_t  time_stamp;

    uint16_t  seq;
    uint16_t  frame_num;
}aud_packet_info_t;
/// discription for receive audio data
typedef struct aud_rcv_packet_dscp
{
    uint16_t  packet_cnt;
    uint16_t  frame_cnt;

    uint16_t  frame_per_pkt;

    aud_buf_info_t pkt_dscp;
}aud_rcv_packet_dscp_t;

/*@TRACE*/
/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
 */
void aud_buf_reset(aud_buf_info_t *aud_buf);
uint8_t aud_buf_init(aud_buf_info_t *aud_buf, uint16_t size);
uint8_t aud_buf_deinit(aud_buf_info_t *aud_buf);
uint16_t aud_buf_data_len(aud_buf_info_t *aud_buf);
uint16_t aud_buf_data_in(aud_buf_info_t *aud_buf, uint16_t data_len, uint8_t *data_in);
uint16_t aud_buf_data_out(aud_buf_info_t *aud_buf, uint16_t data_len, uint8_t *data_out);
uint16_t aud_buf_free_len(aud_buf_info_t *aud_buf);

uint8_t aud_out_buf_init(aud_out_buf_info_t *aud_buf, uint16_t size);
uint8_t aud_out_buf_deinit(aud_out_buf_info_t *aud_buf);
uint8_t aud_pro_buf_init(aud_pro_buf_info_t *aud_pro_buf, uint16_t dec_in_size, uint16_t dec_out_size, uint16_t enc_in_size, uint16_t enc_out_size);
uint8_t aud_pro_buf_deinit(aud_pro_buf_info_t *aud_pro_buf);
uint8_t *aud_buf_data_in_ptr(aud_buf_info_t *aud_buf);
uint8_t *aud_buf_data_out_ptr(aud_buf_info_t *aud_buf);
uint16_t aud_buf_data_async_in(aud_buf_info_t *aud_buf, uint16_t data_len);
uint16_t aud_buf_data_async_out(aud_buf_info_t *aud_buf, uint16_t data_len);


/// @} audio manager api
/// @} AUDIO

#endif // AUD_MGR_BUF_H_

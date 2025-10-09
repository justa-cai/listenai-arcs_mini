/**
 ****************************************************************************************
 *
 * @file aud_mgr_lc3.h
 *
 * @brief audio modules
 *
 * Copyright (C) Listenai.com 2023
 *
 *
 ****************************************************************************************
 */

#ifndef AUD_MGR_LC3_H_
#define AUD_MGR_LC3_H_

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
#include "aud_mgr_buf.h"

/*
 * MACRO DEFINITIONS
 ****************************************************************************************
 */

#define AUD_LC3_PKT_STA_GET(a)           ((a & 0xc000) >> 14)
#define AUD_LC3_PKT_LEN_GET(a)           (a & 0xfff)

#define AUD_LC3_START_FRAME_NUM          (3)
#define AUD_LC3_MIN_FRAME_NUM            (1)
#define AUD_LC3_MIN_PLAY_PCM_NUM         (2)

#define AUD_LC3_PLAY_FRAME_NUM           (2)
#define AUD_LC3_INPUT_BUF_SIZE           (2*1024)
#define AUD_LC3_PKT_DSCP_BUF_SIZE        (256)
#define AUD_LC3_PRO_INPUT_BUF_SIZE       (256)

/*
 * DEFINES
 ****************************************************************************************
 */

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */
  /// audio lc3 info
 typedef struct aud_lc3_info
{
    uint16_t  freq;
    uint8_t   ch;
    uint8_t   frame_ms;
    uint8_t   out_bits;
    uint8_t   hr_mode;
    uint16_t  per_frame_len;
}aud_lc3_info_t;
 typedef struct aud_mgr_lc3
{
    uint8_t                  lc3_state;
    /// lc3 info
    aud_lc3_info_t           lc3_info;
    aud_buf_info_t           lc3_in_buf;
    aud_rcv_packet_dscp_t    lc3_pkt_dcsp;
    aud_buf_info_t           lc3_out_buf;
    aud_pro_buf_info_t       lc3_pro_buf;

    aud_cb_t                 *audio_cb;
    aud_3party_process_t     *lc3_3party_pro;
}aud_mgr_lc3_t;
/*@TRACE*/
/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
 */
uint16_t app_mgr_lc3_start(aud_play_info_t *aud_info, aud_cb_t *audio_cb, aud_3party_process_t *aud_3party_process);
uint16_t app_mgr_lc3_stop(void);
uint16_t app_mgr_lc3_pause(void);
uint16_t app_mgr_lc3_resume(aud_play_info_t *aud_info, aud_cb_t *audio_cb, aud_3party_process_t *aud_3party_process);
uint16_t app_mgr_lc3_codec_init_req(void);
uint16_t app_mgr_lc3_codec_deinit_req(void);
uint16_t app_mgr_lc3_decode_req(uint8_t audio_bfi);
uint16_t app_mgr_lc3_encode_req(void);
uint16_t app_mgr_lc3_check_start_process(void);
uint16_t app_mgr_lc3_process_one_frame(void);
uint16_t app_mgr_lc3_check_pcm_play(void);
uint16_t app_mgr_lc3_rcv_data(uint16_t len, uint8_t *lc3_data);
uint16_t app_mgr_lc3_msg_handle(uint16_t aud_msg_id, uint16_t len, uint8_t *msg_data);
uint16_t audio_mgr_get_lc3_svr(audio_service_t *lc3_svr);

/// @} audio manager api
/// @} AUDIO

#endif // AUD_MGR_LC3_H_

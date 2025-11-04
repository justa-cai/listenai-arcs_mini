/**
 ****************************************************************************************
 *
 * @file aud_mgr_sbc.h
 *
 * @brief audio modules
 *
 * Copyright (C) Listenai.com 2023
 *
 *
 ****************************************************************************************
 */

#ifndef AUD_MGR_SBC_H_
#define AUD_MGR_SBC_H_

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

#define AUD_SBC_PKT_STA_GET(a)           ((a & 0xc000) >> 14)
#define AUD_SBC_PKT_LEN_GET(a)           (a & 0xfff)

#define AUD_SBC_REQ_DEC_FRAME_NUM        (4)

#define AUD_SBC_START_FRAME_NUM          (8)
#define AUD_SBC_MIN_FRAME_NUM            (4)
#define AUD_SBC_MIN_PLAY_PCM_NUM         (4)


#define AUD_SBC_PLAY_FRAME_NUM           (4*2)
#define AUD_SBC_INPUT_BUF_SIZE           (6*1024)
#define AUD_SBC_DEC_IN_BUF_SIZE          (512)
#define AUD_SBC_PKT_DSCP_BUF_SIZE        (256)
#define AUD_SBC_PRO_INPUT_BUF_SIZE       (256)

#define AUD_SBC_SIMU_PCM_PLAY            (0)

/*
 * DEFINES
 ****************************************************************************************
 */

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */
  /// audio sbc info
 typedef struct aud_sbc_info
{
    uint16_t  freq;
    uint8_t   ch;
    uint8_t   frame_ms;
}aud_sbc_info_t;
 typedef struct aud_mgr_sbc
{
    uint8_t                  sbc_state;
    uint8_t                  sbc_play_empty;
    /// sbc info
    aud_sbc_info_t           sbc_info;
    aud_buf_info_t           sbc_in_buf;
    aud_rcv_packet_dscp_t    sbc_pkt_dcsp;
    aud_buf_info_t           sbc_out_buf;
    aud_pro_buf_info_t       sbc_pro_buf;

    aud_cb_t                 *audio_cb;
    aud_3party_process_t     *sbc_3party_pro;
}aud_mgr_sbc_t;

typedef struct aud_play_ind
{
    uint8_t   status;

}aud_play_ind_t;

/*@TRACE*/
/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
 */
uint16_t app_mgr_sbc_start(aud_play_info_t *aud_info, aud_cb_t *audio_cb, aud_3party_process_t *aud_3party_process);
uint16_t app_mgr_sbc_stop(void);
uint16_t app_mgr_sbc_pause(void);
uint16_t app_mgr_sbc_resume(aud_play_info_t *aud_info, aud_cb_t *audio_cb, aud_3party_process_t *aud_3party_process);
uint16_t app_mgr_sbc_codec_init_req(void);
uint16_t app_mgr_sbc_codec_deinit_req(void);
uint16_t app_mgr_sbc_decode_req(uint8_t audio_bfi);
uint16_t app_mgr_sbc_encode_req(void);
uint16_t app_mgr_sbc_check_start_process(void);
uint16_t app_mgr_sbc_process_one_frame(uint8_t type);
uint16_t app_mgr_sbc_check_pcm_play(void);
uint16_t app_mgr_sbc_rcv_data(uint16_t len, uint8_t *lc3_data);
uint16_t app_mgr_sbc_msg_handle(uint16_t aud_msg_id, uint16_t len, uint8_t *msg_data);
uint16_t app_mgr_sbc_pcm_play(void);
uint16_t audio_mgr_get_sbc_svr(audio_service_t *sbc_svr);

/// @} audio manager api
/// @} AUDIO

#endif // AUD_MGR_SBC_H_

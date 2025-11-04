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

#ifndef AUD_MGR_H_
#define AUD_MGR_H_

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
#include "aud_common.h"

/*
 * MACRO DEFINITIONS
 ****************************************************************************************
 */
 
/*
 * DEFINES
 ****************************************************************************************
 */
enum aud_msg
{
   AUD_MSG_NONE,

   AUD_MSG_RCV_DATA_IND,
   AUD_MSG_PLAY_DATA_IND,
   AUD_MSG_SYNC_INFO_IND,
   AUD_MSG_MUTE,
   AUD_MSG_PRO_CMP_IND
};

/// aud manger state machine
enum aud_mgr_state
{
    /// Aud state idle
    AUD_MGR_STATE_IDLE = 0,
    /// Aud codec initing 
    AUD_MGR_STATE_PRO_INITING,
    /// Aud codec inited 
    AUD_MGR_STATE_PRO_INITED,
    /// Aud codec processing
    AUD_MGR_STATE_PROCESSING,
    /// Aud codec processed
    AUD_MGR_STATE_PROCESSED,
    AUD_MGR_STATE_PRO_MSK = 0x0f,
    
    /// Aud codec start play pcm
    AUD_MGR_STATE_PCM_PLAYING = 0x10,
    
    AUD_MGR_STATE_PLAY_MSK = 0xf0,
};

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */
typedef struct aud_pcm_info
{
    uint8_t   ch;
    uint8_t   frame_ms;
    uint8_t   out_bits;
    uint8_t   hr_mode;
    uint16_t  in_frame_len;
    
    uint8_t   *buf;
}aud_pcm_info_t;
 /// aud play info
 typedef struct aud_play_info
{
    /// Aud sample
    uint16_t             aud_sample;
    /// Aud ch
    uint8_t              aud_ch;
    /// Aud bits wide, 16 or 24
    uint8_t              aud_bits_wide;
    /// one frame duration ms
    uint16_t             frame_dur;
    /// one frame length
    uint16_t             frame_len;

    /// dummy
    uint16_t             dummy;
}aud_play_info_t;

/// aud play info
 typedef struct aud_media_info
{
    /// audio play infomation
    aud_play_info_t aud_info;
    /// play position
    uint32_t play_position;
    /// bits rate
    uint16_t bit_rate;
    /// id3 info 
    char * id3;
}aud_media_info_t;

typedef struct aud_3party_process
{
    /**
     ****************************************************************************************
     * @brief 3 party process init
     ****************************************************************************************
     */
    void (*aud_3party_init)(uint8_t dummy,void *aud_param);

    /**
     ****************************************************************************************
     * @brief 3 party process
     ****************************************************************************************
     */
    void (*aud_3party_process)(uint8_t dummy,uint8_t data_len, uint8_t *input, uint8_t *output);

    /**
     ****************************************************************************************
     * @brief 3 party process
     ****************************************************************************************
     */
    void (*aud_3party_deinit)(uint8_t dummy);
}aud_3party_process_t;

typedef struct audio_service
{
    /**
     ****************************************************************************************
     * @brief aud play start
     ****************************************************************************************
     */
    uint16_t (*aud_start) (aud_play_info_t *aud_info, aud_cb_t *audio_cb, aud_3party_process_t *aud_3party_process);
    /**
     ****************************************************************************************
     * @brief aud play stop
     ****************************************************************************************
     */
    uint16_t (*aud_stop) (void);
    /**
     ****************************************************************************************
     * @brief aud play pause
     ****************************************************************************************
     */
    uint16_t (*aud_pause) (void);
    /**
     ****************************************************************************************
     * @brief aud play resume
     ****************************************************************************************
     */
    uint16_t (*aud_resume) (aud_play_info_t *aud_info, aud_cb_t *audio_cb, aud_3party_process_t *aud_3party_process);
    /**
     ****************************************************************************************
     * @brief aud get media infomation
     ****************************************************************************************
     */
    uint16_t (*aud_get_media_info) (aud_media_info_t *media_info);
    /**
     ****************************************************************************************
     * @brief aud message handle
     ****************************************************************************************
     */
    uint16_t (*aud_msg_handle) (uint16_t aud_msg_id, uint16_t len, uint8_t *msg_data);
}audio_service_t;



/*@TRACE*/
/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
 */
audio_service_t *audio_mgr_install_svr(uint8_t audio_type);
audio_service_t *audio_mgr_uninstall_svr(audio_service_t * aud_svr);
uint16_t aud_send_msg_to_mgr(uint16_t msg_id, uint16_t len, void *event);
uint16_t aud_send_isr_msg_to_mgr(uint16_t msg_id, uint16_t len, void *event);
uint16_t aud_send_msg_to_pro(uint16_t msg_id,  uint16_t len, void *event);
/// @} audio manager api
/// @} AUDIO

#endif // AUD_MGR_H_

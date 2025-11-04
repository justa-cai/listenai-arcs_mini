/**
 ****************************************************************************************
 *
 * @file app_aud.h
 *
 * @brief audio modules
 *
 * Copyright (C) Listenai.com 2023
 *
 *
 ****************************************************************************************
 */

#ifndef APP_AUD_H_
#define APP_AUD_H_

/**
 ****************************************************************************************
 * @addtogroup AUDIO
 * @{
 * @name application audio api
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "aud_common.h"
#include "aud_mgr.h"

/*
 * MACRO DEFINITIONS
 ****************************************************************************************
 */
#define AUD_MAX_PLAYER        (1)


#define AUD_PLAYER1           (0)
#define AUD_PLAYER2           (1)
#define AUD_PLAYER3           (2)

/* 
 * DEFINES
 ****************************************************************************************
 */
/// aud player state machine
enum app_aud_state
{
    /// Aud state idle
    APP_AUD_STATE_IDLE = 0,
    /// Aud start
    APP_AUD_STATE_START,
    /// Aud prepare set codec and play env, wait enough data.
    APP_AUD_STATE_PREPARE,
    /// Setting scan response data
    APP_AUD_STATE_PLAY,
    /// Starting advertising activity
    APP_AUD_STATE_STOP,

    /// app audio start, but audio player is active
    APP_AUD_STATE_START_PENDING             = 0x10,
};

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */

/// Application audio environment structure
typedef struct app_aud_tag
{
    /// app aud callback
    aud_cb_t                  *app_aud_cb;
    /// aud state @see enum app_aud_state
    uint8_t                   aud_state;
    /// aud_type @see enum app_aud_type
    uint8_t                   aud_type;
    /// 
    audio_service_t           *aud_server;
    /// audio 3 party interface
    aud_3party_process_t      *aud_3party;
}app_aud_tag_t;

/*@TRACE*/
/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */
uint8_t app_audio_service_register(uint8_t aud_type, uint8_t player_id, aud_cb_t          *aud_cb, aud_3party_process_t *aud_3party_process);
uint8_t app_audio_service_unregister(uint8_t player_id);
uint8_t app_audio_start(uint8_t player_id, aud_play_info_t *play_info);
uint8_t app_audio_stop(uint8_t player_id);
uint8_t app_audio_pause(uint8_t player_id);
uint8_t app_audio_resume(uint8_t player_id, aud_play_info_t *play_info);
uint8_t app_audio_get_media_info(uint8_t player_id, aud_media_info_t *media_info);
uint8_t app_audio_msg_handle(uint8_t player_id, uint8_t aud_msg_id, uint16_t len, uint8_t *data);

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
 */

/// @} application audio api
/// @} AUDIO

#endif // APP_AUD_H_

/**
 ****************************************************************************************
 * @file app_aud.c
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
#include "aud_mgr.h"
#include "aud_pro.h"
#include "app_aud.h"

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
static void app_aud_start_ind(uint8_t type, uint16_t status);
static void app_aud_stop_ind(uint8_t type, uint16_t status);
/*
 * LOCAL VARIABLES
 ****************************************************************************************
 */

/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */
 app_aud_tag_t app_aud_env[AUD_MAX_PLAYER];

/*
 * LOCAL FUNCTIONS
 ****************************************************************************************
 */

/*
 * GLOBAL FUNCTIONS
 ****************************************************************************************
 */
uint8_t app_audio_service_register(uint8_t aud_type, uint8_t player_id, aud_cb_t *aud_cb, aud_3party_process_t *aud_3party_process)
{
    app_aud_tag_t *p_aud_env = &app_aud_env[player_id];
    
    p_aud_env->aud_type = aud_type;
    p_aud_env->app_aud_cb = aud_cb;
    p_aud_env->aud_server = audio_mgr_install_svr(aud_type);
    /// use 3 party algo to process post audio data in audio process task.
    p_aud_env->aud_3party = aud_3party_process;
    return 0;
}

uint8_t app_audio_service_unregister(uint8_t player_id)
{
    app_aud_tag_t *p_aud_env = &app_aud_env[player_id];

    p_aud_env->aud_type = AUD_TYPE_NONE;
    p_aud_env->app_aud_cb = NULL;
    audio_mgr_uninstall_svr(p_aud_env->aud_server);
    p_aud_env->aud_server = NULL;
    p_aud_env->aud_3party = NULL;
    return 0;
}

uint8_t app_audio_start(uint8_t player_id, aud_play_info_t *play_info)
{
    app_aud_tag_t *p_aud_env = &app_aud_env[player_id];
    uint8_t status = 0xff;

    if(p_aud_env->aud_server && p_aud_env->aud_server->aud_start)
    {
        if(p_aud_env->aud_state != APP_AUD_STATE_IDLE)
        {
            app_audio_stop(player_id);
        }
        if(p_aud_env->aud_server->aud_start(play_info, p_aud_env->app_aud_cb, p_aud_env->aud_3party) == AUD_ERROR_NO)
        {
            p_aud_env->aud_state = APP_AUD_STATE_START;
            status = 0;
        }
        
    }
    return status;
}

uint8_t app_audio_pause(uint8_t player_id)
{
    app_aud_tag_t *p_aud_env = &app_aud_env[player_id];
    uint8_t status = 0xff;

    if(p_aud_env->aud_server && p_aud_env->aud_server->aud_pause)
    {
        p_aud_env->aud_server->aud_pause();
        status = 0;
    }
    return status;
}

uint8_t app_audio_resume(uint8_t player_id, aud_play_info_t *play_info)
{
    app_aud_tag_t *p_aud_env = &app_aud_env[player_id];
    uint8_t status = 0xff;

    if(p_aud_env->aud_server && p_aud_env->aud_server->aud_resume)
    {
        p_aud_env->aud_server->aud_resume(play_info, p_aud_env->app_aud_cb, p_aud_env->aud_3party);
        status = 0;
    }
    return status;
}

uint8_t app_audio_stop(uint8_t player_id)
{
    app_aud_tag_t *p_aud_env = &app_aud_env[player_id];
    uint8_t status = 0xff;

    if(p_aud_env->aud_server && p_aud_env->aud_server->aud_stop)
    {
        p_aud_env->aud_server->aud_stop();
        p_aud_env->aud_state = APP_AUD_STATE_IDLE;
        status = 0;
    }
    return status;
}

uint8_t app_audio_get_media_info(uint8_t player_id, aud_media_info_t *media_info)
{
    app_aud_tag_t *p_aud_env = &app_aud_env[player_id];
    uint8_t status = 0xff;

    if(p_aud_env->aud_server && p_aud_env->aud_server->aud_get_media_info)
    {
        p_aud_env->aud_server->aud_get_media_info(media_info);
        status = 0;
    }
    return status;
}

uint8_t app_audio_msg_handle(uint8_t player_id, uint8_t aud_msg_id, uint16_t len, uint8_t *data)
{
    app_aud_tag_t *p_aud_env = &app_aud_env[player_id];
    uint8_t msg_free = 1;

    if(p_aud_env->aud_server && p_aud_env->aud_server->aud_msg_handle)
    {
        msg_free = p_aud_env->aud_server->aud_msg_handle(aud_msg_id, len, data);
    }
    return msg_free;
}

/// @} AUDIO




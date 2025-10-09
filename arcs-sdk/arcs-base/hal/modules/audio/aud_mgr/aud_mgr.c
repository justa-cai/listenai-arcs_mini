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
#include "log_print.h"

#include "aud_mgr.h"
#include "aud_mgr_sbc.h"
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
audio_service_t *audio_mgr_install_svr(uint8_t audio_type)
{
    uint8_t status = AUD_ERROR_TYPE_NOT_SUPPORT;
    
    audio_service_t *aud_svr = (audio_service_t *)AUD_MALLOC(sizeof(audio_service_t));

    CLOGD("aud mgr ins type:%d", audio_type);
    switch(audio_type)
    {
        case AUD_TYPE_LC3 :
        {
            //status =audio_mgr_get_lc3_svr(aud_svr);
        }
        break;
        case AUD_TYPE_SBC :
        {
            status = audio_mgr_get_sbc_svr(aud_svr);
        }
        break;
        case AUD_TYPE_AAC :
        {
        }
        break;
        case AUD_TYPE_MP3 :
        {
        }
        break;
        case AUD_TYPE_MSBC :
        {
        }
        break;
        default : break;
    }

    if(status)
    {
        AUD_FREE(aud_svr);
        aud_svr = NULL;
    }
    return aud_svr;
}

audio_service_t *audio_mgr_uninstall_svr(audio_service_t * aud_svr)
{
    CLOGD("audio_mgr_uninstall_svr:0x%x",aud_svr);

    if(aud_svr)
    {
        AUD_FREE(aud_svr);
        aud_svr = NULL;
    }
    return 0;
}
/*
 * LOCAL FUNCTIONS
 ****************************************************************************************
 */

/*
 * GLOBAL FUNCTIONS
 ****************************************************************************************
 */


/// @} AUDIO




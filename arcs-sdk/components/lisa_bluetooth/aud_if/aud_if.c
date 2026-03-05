/*
 * aud_if.c
 *
 *  audio interface functions
 */

/*
 * INCLUDES
 ****************************************************************************************
 */
#include <string.h>
#include <assert.h>
#include <stdlib.h>    // standard lib functions
#include <stddef.h>    // standard definitions
#include <stdint.h>    // standard integer definition
#include <stdbool.h>   // boolean definition

#include "log_print.h"
#include "nvs.h"
#include "aud_if.h"
#include "aud_os_task.h"
#include "bt_os_task.h"

#include "codec_lc3.h"

/// just dump iso data for debug.
#define WIN32_DUMP_PCM
#ifdef WIN32_DUMP_PCM
#include <stdio.h>

FILE *pcm_file = NULL;
#endif

/*
 * LOCAL FUNCTIONS DECLARATION
 ****************************************************************************************
 */
static void aud_if_start_ind(uint8_t type, uint16_t status);
static void app_if_stop_ind(uint8_t type, uint16_t status);
/*
 * LOCAL VARIABLES
 ****************************************************************************************
 */


/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */
static const os_task_cb_t aud_os_if_cb = {
    .cb_os_init          = aud_if_init,
    .cb_os_msg_handle    = aud_if_msg_handle,
    .cb_os_user_schedule = aud_if_user_schedule,
};
aud_cb_t aud_if_cb = 
{
    .cb_aud_start_ind = aud_if_start_ind,
    .cb_aud_stop_ind = app_if_stop_ind,
};

btos_msg_isr_t play_isr_msg;

/*
 * LOCAL FUNCTIONS
 ****************************************************************************************
 */
static void aud_if_start_ind(uint8_t type, uint16_t status)
{

}
static void app_if_stop_ind(uint8_t type, uint16_t status)
{
    CLOGD("app_if_stop_ind");
    app_audio_service_unregister(AUD_PLAYER1);
}

/**
 ****************************************************************************************
 * @brief initialize platform
 *
 *
 ****************************************************************************************
 */

/*
 * GLOBAL FUNCTIONS
 ****************************************************************************************
 */
os_task_cb_t *aud_if_get_cb(void)
{
    return (os_task_cb_t *)&aud_os_if_cb;
}
void aud_if_init(uint8_t type)
{
}
#if 0
void aud_if_msg_handle(btos_event_t* msg)
{
    btos_event_t *event = msg;
    if(event->msg_body)
    {
        switch(event->msg_body->msg_id)
        {
            case AUD_OS_START_EVT :
            {
                CLOGD("aud start");
                app_lc3_start(8000, 1, 100, 16, 0);
            }break;
            case AUD_OS_STOP_EVT :
            {
                CLOGD("aud stop");
                app_lc3_stop();
            }break;
            case AUD_OS_RCV_DATA_EVT :
            {
                uint8_t pcm[512];
                uint16_t out_len = 0;
                app_lc3_dec(event->msg_body->param_len, event->msg_body->param, &out_len, pcm, 0);
                
                CLOGD("app lc3 dec, in_len:%d, out_len:%d", event->msg_body->param_len, out_len);
#ifdef WIN32_DUMP_PCM
                ///save iso in file.
                {
                    if(pcm_file == NULL)
                    {
                        pcm_file = fopen("pcm_data.pcm", "wb+");
                    }

                    if(pcm_file != NULL)
                    {
                        fwrite(pcm, 2, out_len, pcm_file);
                        fflush(pcm_file);
                    }
                }
#endif
            }break;
            default:
            {

            }break;
        }
    }
}
#endif
uint8_t aud_if_switch_msg_id(uint16_t app_msg_id)
{
    uint16_t aud_msg_id = app_msg_id;

    switch(app_msg_id)
    {
        case AUD_OS_RCV_DATA_EVT : 
        {
            aud_msg_id = AUD_MSG_RCV_DATA_IND;
        }
        break;
        default : break;
    }
    return aud_msg_id;
}

uint8_t aud_if_send_cfm_msg(uint16_t msg_id, uint8_t status, uint16_t len, uint8_t *param)
{
    btos_event_t ev;
    uint16_t task_id = OS_TASK_ID_IDLE;


    switch(msg_id)
    {
        case AUD_OS_RCV_DATA_EVT :
        {
            ev.msg_body = btos_malloc(sizeof(btos_msg_t) + len);
            
            ev.msg_body->msg_id = BT_OS_DATA_SEND_CNF_EVT;
            task_id = ev.msg_body->msg_id >> 8;
            memcpy(&ev.msg_body->param, param, len);
        }break;

        default : 
            break;
    }
    return btos_send_event(task_id, &ev, BTOS_TASK_MAX_DELAY);
}

uint8_t aud_if_msg_handle(btos_event_t* msg)
{
    uint8_t status = 0;
    btos_event_t *event = msg;
    uint8_t msg_free = 1;
    
    if(event->msg_body)
    {
        switch(event->msg_body->msg_id)
        {
            /// app or bt stask message
            case AUD_OS_START_EVT :
            {
                aud_play_info_t audio_info;
                aud_codec_info_t codec_info;
                
                memcpy(&codec_info, event->msg_body->param, sizeof(aud_codec_info_t));
                CLOGD("aud start, type:%d", codec_info.aud_type);
                audio_info.aud_ch = codec_info.aud_ch;
                audio_info.aud_sample = codec_info.aud_sample;
                audio_info.aud_bits_wide = codec_info.aud_bits_wide;
                audio_info.frame_dur = codec_info.frame_dur;
                audio_info.frame_len = codec_info.frame_len;
                audio_info.dummy = 0;
                app_audio_service_register(codec_info.aud_type, AUD_PLAYER1, &aud_if_cb, NULL);
                app_audio_start(AUD_PLAYER1, &audio_info);
            }break;
            case AUD_OS_STOP_EVT :
            {
                CLOGD("aud stop");
                app_audio_stop(AUD_PLAYER1);
            }break;
            case AUD_OS_PAUSE_EVT :
            {
                CLOGD("aud pause");
                app_audio_pause(AUD_PLAYER1);
            }break;
            case AUD_OS_RESUME_EVT :
            {
                aud_play_info_t audio_info;
                aud_codec_info_t codec_info;
                
                CLOGD("aud resume");
                memcpy(&codec_info, event->msg_body->param, sizeof(aud_codec_info_t));
                audio_info.aud_ch = codec_info.aud_ch;
                audio_info.aud_sample = codec_info.aud_sample;
                audio_info.aud_bits_wide = codec_info.aud_bits_wide;
                audio_info.frame_dur = codec_info.frame_dur;
                audio_info.frame_len = audio_info.frame_len;
                audio_info.dummy = 0;
                app_audio_service_register(codec_info.aud_type, AUD_PLAYER1, &aud_if_cb, NULL);
                app_audio_resume(AUD_PLAYER1, &audio_info);
            }break;
            case AUD_OS_MEIDA_INFO_EVT :
            {
                aud_media_info_t media_info;
                CLOGD("aud get media info");
                app_audio_get_media_info(AUD_PLAYER1, &media_info);
            }break;
            case AUD_OS_RCV_DATA_EVT :
            {
                //CLOGD("aud rcv data,len:%d, addr:0x%x", event->msg_body->param_len, event->msg_body->param);
                //CLOGD("[ISO_DATA AUD OS]:buf_addr:0x%x", (event->msg_body->param[3] << 24) | (event->msg_body->param[2] << 16) | (event->msg_body->param[1] << 8) | event->msg_body->param[0]);
                msg_free = app_audio_msg_handle(AUD_PLAYER1, aud_if_switch_msg_id(event->msg_body->msg_id), event->msg_body->param_len, event->msg_body->param);
                aud_if_send_cfm_msg(event->msg_body->msg_id, AUD_ERROR_NO, event->msg_body->param_len, event->msg_body->param);
            }break;

            /// audio pro and mgr message
            default:
            {
                msg_free = app_audio_msg_handle(AUD_PLAYER1, aud_if_switch_msg_id(event->msg_body->msg_id), event->msg_body->param_len, event->msg_body->param);
                //CLOGD("aud rcv default message,id:%d", event->msg_body->msg_id);
            }break;
        }
    }
    return msg_free;
}

uint8_t aud_if_user_schedule(void)
{
    /// if need
	return 0;
}
/// for aud mgr & aud pro send msg.
uint8_t aud_if_msg_send_to_mgr(uint16_t msg_id, uint16_t len, void *event)
{
    btos_event_t ev;

    ev.msg_body = btos_malloc(sizeof(btos_msg_t) + len);
    ev.msg_body->msg_id = msg_id;
    ev.msg_body->param_len = len;
    memcpy(ev.msg_body->param, event, len);
    return btos_send_event(TASK_ID_AUD_MGR, &ev, BTOS_TASK_MAX_DELAY);
}

uint8_t aud_if_msg_isr_send_to_mgr(uint16_t msg_id, uint16_t len, void *event, void *body)
{
    btos_event_t ev;

    ev.msg_body = (btos_msg_t *)&play_isr_msg;
    ev.msg_body->msg_id = msg_id;
    ev.msg_body->param_len = len;
    memcpy(ev.msg_body->param, event, len);
    return btos_send_event_isr(TASK_ID_AUD_MGR, &ev);
}

uint8_t aud_if_msg_send_to_pro(uint16_t msg_id, uint16_t len, void *event)
{
    btos_event_t ev;

    ev.msg_body = btos_malloc(sizeof(btos_msg_t) + len);
    ev.msg_body->msg_id = msg_id;
    ev.msg_body->param_len = len;
    memcpy(ev.msg_body->param, event, len);
    return btos_send_event(TASK_ID_AUD_PRO, &ev, BTOS_TASK_MAX_DELAY);
}



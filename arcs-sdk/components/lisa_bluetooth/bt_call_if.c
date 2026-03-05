/*
 * bt_call_if.c
 *
 *  bt stack interface functions
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

//#include "plf.h"

#include "ble_task.h"
#include "ble_drv.h"
#include "ble_plf_config.h"
#include "ble_gap.h"
#include "ble_prf.h"
#include "aud_common.h"

#include "bt_stack_if.h"
#include "bt_call_if.h"
#include "aud_os_task.h"

#if (BT_STACK_PRESENT && BT_CALL_PRESENT)
#include "bt_hfp.h"

/*
 * LOCAL FUNCTIONS DECLARATION
 ****************************************************************************************
 */

static void hfp_enable_cmp(uint16_t status);
static void hfp_disable_cmp(uint16_t status);
static void hfp_receive_media_from_peer(uint8_t conidx, uint8_t pkt_sta, uint16_t len, uint8_t *data);
static void hfp_send_media_cmp(uint8_t conidx, uint8_t status, uint8_t *data);
uint8_t hfp_send_media_to_peer(uint8_t conidx, uint16_t len, uint8_t *data);

uint8_t bt_stack_hfp_send_start(uint8_t conidx, uint8_t codec);
void bt_stack_hfp_send_stop(uint8_t conidx, uint8_t status);
void bt_stack_hfp_send_data(uint8_t conidx, uint8_t pkt_sta, uint16_t len, uint8_t *data);

/*
 * LOCAL VARIABLES
 ****************************************************************************************
 */

static const bt_hfp_cb_t bt_hfp_cb = {
    .cb_hfp_enable_cmp         = hfp_enable_cmp,
    .cb_hfp_disable_cmp        = hfp_disable_cmp,
    ///register callback in bt stack if.
    .cb_hfp_aud_start_ind      = NULL,//hfp_aud_start_ind,
    .cb_hfp_aud_stop_ind       = NULL,//hfp_aud_stop_ind,
    .cb_hfp_media_ind          = hfp_receive_media_from_peer,
    .cb_hfp_send_media_cmp     = hfp_send_media_cmp,
};


/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */

/*
 * LOCAL FUNCTIONS
 ****************************************************************************************
 */
static void hfp_enable_cmp(uint16_t status)
{
    CLOGD("hfp enable cmp!");
}

static void hfp_disable_cmp(uint16_t status)
{
    CLOGD("hfp disable cmp!");
}

void hfp_aud_start_ind(uint8_t conidx, uint16_t codec, uint16_t status)
{
    CLOGD("hfp start! conidx:0x%x,codec:%d,status:0x%x",conidx, codec, status);
    if(status == 0)
    {
        bt_stack_hfp_send_start(conidx, codec);
    }
}

void hfp_aud_stop_ind(uint8_t conidx, uint16_t conhdl, uint16_t reason)
{
    CLOGD("hfp stop!, conidx:%d, reason:0x%x", conidx, reason);
    bt_stack_hfp_send_stop(conidx, (uint8_t)reason);
}
///app receive sco aud data from peer.
static void hfp_receive_media_from_peer(uint8_t conidx, uint8_t pkt_sta, uint16_t len, uint8_t *data)
{
    //uint32_t debug_data = data[len - 3] | (data[len - 2] << 8) | (data[len - 1] << 16) | (data[len] << 24);
    //CLOGD("s:%d,0x%x", seq, debug_data);
    //if(seq | 0x1f == 0)
    //{
        //CLOGD("s:%d", seq);
    //}
    bt_stack_hfp_send_data(conidx, pkt_sta, len, data);

#if BT_CALL_SCO_SEND_DUMMY
    ///dummy data send 
    
    uint8_t trans_buf[120];
    memcpy(trans_buf, data, len);
    hfp_send_media_to_peer(conidx, len, trans_buf);
#endif

}
///app send sco aud data to peer.
uint8_t hfp_send_media_to_peer(uint8_t conidx, uint16_t len, uint8_t *data)
{
    return app_hfp_send_aud_to_peer(conidx, len, data);
}

static void hfp_send_media_cmp(uint8_t conidx, uint8_t status, uint8_t *data)
{
    ///to do 
    ///if need to free data;
    //CLOGD("cmp:0x%x-0x%x",status,data);
    #if 0
    if(data != NULL)
    {
        ke_free(data);
    }
    #endif
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

uint8_t hfp_aud_type_switch(uint8_t bt_codec)
{
    uint8_t aud_type = AUD_TYPE_NONE;
    switch(bt_codec)
    {
        case HFP_MEDIA_CODEC_CVSD : 
            aud_type = AUD_TYPE_CVSD;
            break;
        case HFP_MEDIA_CODEC_MSBC : 
            aud_type = AUD_TYPE_MSBC;
            break;
        default : 
            aud_type = AUD_TYPE_NONE;
            break;
    }
    return aud_type;
}

void bt_stack_hfp_enable(bt_hfp_cfg_t *cfg)
{
    CLOGD("hfp en, role:%d,feats:0x%x", cfg->hfp_role, cfg->hfp_feats);
    app_hfp_enable(cfg->hfp_role, cfg->hfp_feats, &bt_hfp_cb);
}

void bt_stack_hfp_disable(void)
{
    CLOGD("hfp dis");
    app_hfp_disable();
}

void bt_stack_hfp_send_data(uint8_t conidx, uint8_t pkt_sta, uint16_t len, uint8_t *data)
{
    btos_event_t ev;
    bt_call_pkt_info_t *pkt_info;
    uint16_t ev_len = sizeof(btos_msg_t) + sizeof(bt_call_pkt_info_t);
    
    ev.msg_body = btos_malloc(ev_len);
    ev.msg_body->msg_id = AUD_OS_RCV_DATA_EVT;
    ev.msg_body->param_len = ev_len;
    
    pkt_info = (bt_call_pkt_info_t *)ev.msg_body->param;
    pkt_info->conidx = conidx;
    pkt_info->pkt_sta = pkt_sta;
    pkt_info->len = len;
    pkt_info->data = data;

    btos_send_event(OS_TASK_ID_AUD, &ev, BTOS_TASK_MAX_DELAY);
}

uint8_t bt_stack_hfp_send_start(uint8_t conidx, uint8_t codec)
{
    uint8_t status = BT_HFP_ERR_NO_ERROR;
    btos_event_t ev;
    aud_codec_info_t *aud_info;

    ev.msg_body = btos_malloc(sizeof(btos_msg_t) + sizeof(aud_codec_info_t));
    ev.msg_body->msg_id = AUD_OS_START_EVT;
    ev.msg_body->param_len = sizeof(aud_codec_info_t);

    aud_info = (aud_codec_info_t *)ev.msg_body->param;
    aud_info->conidx = conidx;
    aud_info->aud_type = hfp_aud_type_switch(codec);
    aud_info->aud_ch = 1;
    if(codec == HFP_MEDIA_CODEC_CVSD)
    {
        aud_info->aud_sample = 8000;

    }
    else
    {
        aud_info->aud_sample = 16000;

    }

    return btos_send_event(OS_TASK_ID_AUD, &ev, BTOS_TASK_MAX_DELAY);
}

void bt_stack_hfp_send_stop(uint8_t conidx, uint8_t status)
{
    btos_event_t ev;

    ev.msg_body = btos_malloc(sizeof(btos_msg_t) + 2);
    ev.msg_body->msg_id = AUD_OS_STOP_EVT;
    ev.msg_body->param_len = 2;
    *ev.msg_body->param = conidx;
    *(ev.msg_body->param + 1) = status;
    btos_send_event(OS_TASK_ID_AUD, &ev, BTOS_TASK_MAX_DELAY);
}

#endif

/*
 * bt_stack_if.c
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
#include "bt_config.h"  // BT 配置宏定义

#include "ble_task.h"
#include "ble_drv.h"
#include "ble_plf_config.h"
#include "ble_gap.h"
#include "ble_prf.h"
#include "ble_lea.h"
#include "aud_common.h"

#include "bt_stack_if.h"
#include "bt_music_if.h"
#include "aud_os_task.h"

#if (BT_STACK_PRESENT && BT_MUSIC_PRESENT)
#include "bt_a2dp.h"

/*
 * LOCAL FUNCTIONS DECLARATION
 ****************************************************************************************
 */

static void a2dp_enable_cmp(uint16_t status);
static void a2dp_revoke_cmp(uint16_t status);
static void a2dp_start_ind(uint8_t conidx, uint8_t codec, uint8_t ch, uint16_t sample_rate);
static void a2dp_stop_ind(uint8_t conidx, uint8_t status);
static void a2dp_media_ind(uint8_t conidx, uint8_t frame_num, uint16_t seq, uint16_t len, uint8_t *data);

uint8_t bt_stack_a2dp_send_start(uint8_t conidx, uint8_t codec, uint8_t ch, uint16_t sample_rate);
void bt_stack_a2dp_send_stop(uint8_t conidx, uint8_t status);
void bt_stack_a2dp_send_data(uint8_t conidx, uint8_t frame_num, uint16_t seq, uint16_t len, uint8_t *data);

/*
 * LOCAL VARIABLES
 ****************************************************************************************
 */
static bt_a2dp_cfg_t a2dp_cfg = {
    .a2dp_role = BT_A2DP_SINK,
};

static const bt_a2dp_cb_t bt_a2dp_cb = {
    .cb_a2dp_enable_cmp         = a2dp_enable_cmp,
    .cb_a2dp_revoke_cmp         = a2dp_revoke_cmp,
    .cb_a2dp_start_ind          = a2dp_start_ind,
    .cb_a2dp_stop_ind           = a2dp_stop_ind,
    .cb_a2dp_media_ind          = a2dp_media_ind,
};


/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */
extern struct ble_rf_api lsip_rf;

/*
 * LOCAL FUNCTIONS
 ****************************************************************************************
 */
static void a2dp_enable_cmp(uint16_t status)
{
    CLOGI("a2dp enable cmp!");
}

static void a2dp_revoke_cmp(uint16_t status)
{
    CLOGI("a2dp revoke cmp!");
}

static void a2dp_start_ind(uint8_t conidx, uint8_t codec, uint8_t ch, uint16_t sample_rate)
{
    CLOGI("a2dp start! codec:%d, ch:%d, sample_rate:%d");
    bt_stack_a2dp_send_start(conidx, codec, ch, sample_rate);
}

static void a2dp_stop_ind(uint8_t conidx, uint8_t status)
{
    CLOGI("a2dp stop!, conidx:%d, sta:0x%x", conidx, status);
    bt_stack_a2dp_send_stop(conidx, status);
}

static void a2dp_media_ind(uint8_t conidx, uint8_t frame_num, uint16_t seq, uint16_t len, uint8_t *data)
{
    //uint32_t debug_data = data[len - 3] | (data[len - 2] << 8) | (data[len - 1] << 16) | (data[len] << 24);
    //CLOGI("s:%d,0x%x", seq, debug_data);
    //if(seq | 0x1f == 0)
    //{
        //CLOGI("s:%d", seq);
    //}
    bt_stack_a2dp_send_data(conidx, frame_num, seq, len, data);

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

uint8_t a2dp_aud_type_switch(uint8_t bt_codec)
{
    uint8_t aud_type = AUD_TYPE_NONE;
    switch(bt_codec)
    {
        case A2DP_MEDIA_CODEC_SBC : 
            aud_type = AUD_TYPE_SBC;
            break;
        case A2DP_MEDIA_CODEC_MPEG1_2_AUDIO : 
            aud_type = AUD_TYPE_MP3;
            break;
        case A2DP_MEDIA_CODEC_MPEG2_4_AAC : 
            aud_type = AUD_TYPE_AAC;
            break;
        default : 
            aud_type = AUD_TYPE_NONE;
            break;
    }
    return aud_type;
}

void bt_stack_a2dp_enable(uint8_t role)
{
    CLOGD("a2dp en, role:%d", role);
    app_a2dp_enable(role, &bt_a2dp_cb);
}

void bt_stack_a2dp_disable(void)
{
    CLOGD("a2dp dis");
    app_a2dp_disable();
}

void bt_stack_a2dp_send_data(uint8_t conidx, uint8_t frame_num, uint16_t seq, uint16_t len, uint8_t *data)
{
    btos_event_t ev;
    bt_aud_pkt_info_t *pkt_info;
    uint16_t ev_len = sizeof(btos_msg_t) + sizeof(bt_aud_pkt_info_t);
    
    ev.msg_body = btos_malloc(ev_len);
    ev.msg_body->msg_id = AUD_OS_RCV_DATA_EVT;
    ev.msg_body->param_len = ev_len;
    
    pkt_info = (bt_aud_pkt_info_t *)ev.msg_body->param;
    pkt_info->conidx = conidx;
    pkt_info->frame_num = frame_num;
    pkt_info->seq = seq;
    pkt_info->len = len;
    pkt_info->data = data;

    btos_send_event(OS_TASK_ID_AUD, &ev, BTOS_TASK_MAX_DELAY);
}

uint8_t bt_stack_a2dp_send_start(uint8_t conidx, uint8_t codec, uint8_t ch, uint16_t sample_rate)
{
    uint8_t status = BT_AUD_ERR_NO_ERROR;
    btos_event_t ev;
    aud_codec_info_t *aud_info;

    ev.msg_body = btos_malloc(sizeof(btos_msg_t) + sizeof(aud_codec_info_t));
    ev.msg_body->msg_id = AUD_OS_START_EVT;
    ev.msg_body->param_len = sizeof(aud_codec_info_t);

    aud_info = (aud_codec_info_t *)ev.msg_body->param;
    aud_info->conidx = conidx;
    aud_info->aud_type = a2dp_aud_type_switch(codec);
    aud_info->aud_ch = ch;
    aud_info->aud_sample = sample_rate;

    return btos_send_event(OS_TASK_ID_AUD, &ev, BTOS_TASK_MAX_DELAY);
}

void bt_stack_a2dp_send_stop(uint8_t conidx, uint8_t status)
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

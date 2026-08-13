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

#include "bt_stack_cfg.h"
#include "bt_call_hal.h"
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

uint8_t bt_stack_hfp_send_start(uint8_t conidx, uint8_t codec);
void bt_stack_hfp_send_stop(uint8_t conidx, uint8_t status);
void bt_stack_hfp_send_data(uint8_t conidx, uint8_t pkt_sta, uint16_t len, uint8_t *data);

/*
 * LOCAL VARIABLES
 ****************************************************************************************
 */

extern const bt_hfp_cb_t bt_hfp_cb;

/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */

/*
 * LOCAL FUNCTIONS
 ****************************************************************************************
 */

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
        case HFP_MEDIA_CODEC_AUTO :
        case HFP_MEDIA_CODEC_CVSD :
        case HFP_MEDIA_CODEC_SCO :
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

/**
 * @brief HFP 连接状态更新
 * @param conidx 连接索引
 * @param connected true-连接，false-断开
 */
void bt_stack_hfp_connection_update(uint8_t conidx, bool connected)
{
    btos_event_t ev;
    
    ev.msg_body = btos_malloc(sizeof(btos_msg_t) + 1);
    ev.msg_body->msg_id = AUD_OS_HFP_CONNECTION_UPDATE_EVT;
    ev.msg_body->param_len = 1;
    
    // conidx (0xFF 表示断开)
    *ev.msg_body->param = connected ? conidx : 0xFF;
    
    btos_send_event(OS_TASK_ID_AUD, &ev, BTOS_TASK_MAX_DELAY);
    
    CLOGD("HFP connection event sent: conidx=%d, connected=%d", conidx, connected);
}

#endif

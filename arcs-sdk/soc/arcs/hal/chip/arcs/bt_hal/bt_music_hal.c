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

#include "ble_task.h"
#include "ble_drv.h"
#include "ble_plf_config.h"
#include "ble_gap.h"
#include "ble_prf.h"
#include "ble_lea.h"
#include "aud_common.h"

#include "bt_stack_cfg.h"
#include "bt_music_hal.h"
#include "aud_os_task.h"

#if (BT_STACK_PRESENT && BT_MUSIC_PRESENT)
#include "bt_a2dp.h"
#include "bt_avrcp.h"
/*
 * LOCAL FUNCTIONS DECLARATION
 ****************************************************************************************
 */

static void a2dp_enable_cmp(uint16_t status);
static void a2dp_revoke_cmp(uint16_t status);
static void a2dp_start_ind(uint8_t conidx, uint8_t codec, uint8_t ch, uint16_t sample_rate);
static void a2dp_stop_ind(uint8_t conidx, uint8_t status);
static void a2dp_media_ind(uint8_t conidx, uint8_t frame_num, uint16_t seq, uint16_t len, uint8_t *data);
static void avrcp_connect_cmp(uint16_t status);
static void avrcp_disconnect_cmp(uint16_t status);
static void avrcp_avrcp_press_cmp(uint16_t status);
static void avrcp_notify_cmp(uint16_t status);
static void avrcp_media_cmp(uint16_t status);
/*
 * LOCAL VARIABLES
 ****************************************************************************************
 */
extern const bt_a2dp_cb_t bt_a2dp_cb;

extern const bt_avrcp_cb_t bt_avrcp_cb;
/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */
extern struct ble_rf_api lsip_rf;

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

void bt_stack_a2dp_caps_set(uint8_t role, bt_a2dp_meida_caps_cfg_t *caps_cfg)
{
    CLOGD("a2dp caps set, role:%d", role);
    app_a2dp_set_media_caps(role, caps_cfg);
}

void bt_stack_a2dp_enable(bt_a2dp_cfg_t *a2dp_cfg)
{
    CLOGD("a2dp en, role:%d", a2dp_cfg->a2dp_role);
    app_a2dp_enable(a2dp_cfg->a2dp_role, a2dp_cfg->aac_support, &bt_a2dp_cb);
    app_avrcp_register(&bt_avrcp_cb);
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

/**
 * @brief A2DP 媒体数据发送完成通知
 * @param conidx 连接索引
 * 
 * @note 当蓝牙协议栈确认媒体数据发送完成时调用此接口，
 *       通知 adapter 层可以继续发送下一包数据
 */
void bt_stack_a2dp_send_media_rsp(uint8_t conidx)
{
    btos_event_t ev;
    
    ev.msg_body = btos_malloc(sizeof(btos_msg_t) + 1);
    if (!ev.msg_body) {
        CLOGE("Failed to allocate memory for media rsp event");
        return;
    }
    
    ev.msg_body->msg_id = AUD_OS_A2DP_MEDIA_RSP_EVT;
    ev.msg_body->param_len = 1;
    *ev.msg_body->param = conidx;
    
    btos_send_event(OS_TASK_ID_AUD, &ev, BTOS_TASK_MAX_DELAY);
}

/**
 * @brief A2DP 连接状态更新
 * @param conidx 连接索引
 * @param connected true-连接，false-断开
 */
void bt_stack_a2dp_connection_update(uint8_t conidx, bool connected)
{
    btos_event_t ev;
    
    ev.msg_body = btos_malloc(sizeof(btos_msg_t) + 1);
    ev.msg_body->msg_id = AUD_OS_A2DP_CONNECTION_UPDATE_EVT;
    ev.msg_body->param_len = 1;
    
    // conidx (0xFF 表示断开)
    *ev.msg_body->param = connected ? conidx : 0xFF;
    
    btos_send_event(OS_TASK_ID_AUD, &ev, BTOS_TASK_MAX_DELAY);
    
    CLOGD("A2DP connection event sent: conidx=%d, connected=%d", conidx, connected);
}

#endif

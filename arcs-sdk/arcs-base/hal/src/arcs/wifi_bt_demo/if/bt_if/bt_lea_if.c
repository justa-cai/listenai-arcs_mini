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

#include "bt_stack_if.h"
#include "bt_lea_if.h"
#include "aud_os_task.h"

#if (LEA_PRESENT)
/*
 * LOCAL FUNCTIONS DECLARATION
 ****************************************************************************************
 */
static void lea_enable_cmp(uint16_t status);
static void lea_svc_discover_cmp(uint8_t conidx, uint16_t status);
static void lea_get_ind(uint16_t attr, uint8_t conidx, uint16_t status, uint16_t len, uint8_t *p_data);
static void lea_set_ind(uint16_t attr, uint8_t conidx, uint16_t status);
static void lea_sub_ind(uint16_t sub_id, uint8_t conidx, uint16_t status, uint16_t len, uint8_t *p_data);
static void lea_evt_ind(uint16_t event_id, uint16_t status, uint16_t len, uint8_t *data);

/*
 * LOCAL VARIABLES
 ****************************************************************************************
 */
static ble_lea_cfg_t ble_lea_cfg = {
    .lea_role = 1,
    .svc_mask = LEA_SERVICE_MASK,
};

static const ble_lea_cb_t ble_lea_cb = {
    .cb_lea_enable_cmp         = lea_enable_cmp,
    .cb_lea_svc_discover_cmp   = lea_svc_discover_cmp,
    .cb_lea_get_ind            = lea_get_ind,
    .cb_lea_set_ind            = lea_set_ind,
    .cb_lea_sub_ind            = lea_sub_ind,
    .cb_lea_event_ind          = lea_evt_ind,
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
static void lea_enable_cmp(uint16_t status)
{

}

static void lea_svc_discover_cmp(uint8_t conidx, uint16_t status)
{

}

static void lea_get_ind(uint16_t attr, uint8_t conidx, uint16_t status, uint16_t len, uint8_t *p_data)
{

}

static void lea_set_ind(uint16_t attr, uint8_t conidx, uint16_t status)
{

}

static void lea_sub_ind(uint16_t sub_id, uint8_t conidx, uint16_t status, uint16_t len, uint8_t *p_data)
{

}

static void lea_evt_ind(uint16_t event_id, uint16_t status, uint16_t len, uint8_t *data)
{
    switch(event_id)
    {
        case LEA_TMAP_RCV_ISO_DATA :
        {
            //CLOGD("[ISO_DATA TMAP]:buf_addr:0x%x", (*(data + 3) << 24) | (*(data + 2) << 16) | (*(data + 1) << 8) | *data);
            bt_stack_lea_send_data(0, len, data);
        }break;
        
        case LEA_TMAP_BIG_ESTABLE :
        case LEA_TMAP_CIG_ESTABLE :
        {
            bap_iso_codec_info_t *codec_info = (bap_iso_codec_info_t *)(data[0] | (data[1] << 8) \
                                                | (data[2] << 16) | (data[3] << 24));
            bt_stack_lea_send_start(codec_info);
        }break;
        
        case LEA_TMAP_BIG_SYNC_TERMINATE :
        case LEA_TMAP_CIG_DISCONNECT :
        {
            bt_stack_lea_send_stop(0, NULL);
        }break;
        default : break;
    }
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
 #if 0
uint8_t debug_adv_date[] = {0x02,0x01,0x05,
0x03,0x03,0x12,0x18,0x05,0xff,0x66,0x79,0x30,0x02,0x03,0x19,0xc1,0x03,0x06,0x08,0x32,0x33,0x34,0x35,0x36};
#endif
uint8_t bt_stack_switch_aud_info(bap_iso_codec_info_t *lea_info, aud_codec_info_t *aud_info)
{
    uint8_t status = LEA_ERR_NO_ERROR;
    switch(lea_info->codec)
    {
        case AUD_CODEC_LC3 : 
            aud_info->aud_type = AUD_TYPE_LC3;
            break;
        case AUD_CODEC_VENDOR : 
            aud_info->aud_type = AUD_TYPE_VENDOR;
            break;
        default : 
            status = LEA_ERR_ERROR_CODEC;
            return status;
            break;
    }
    switch(lea_info->codec_cfg.sample_rate)
    {
        case GEN_CFG_SAMPLE_8000HZ : 
            aud_info->aud_sample = AUD_SAMPLE_8000HZ;
            break;
        case GEN_CFG_SAMPLE_11025HZ : 
            aud_info->aud_sample = AUD_SAMPLE_11025HZ;
            break;
        case GEN_CFG_SAMPLE_16000HZ : 
            aud_info->aud_sample = AUD_SAMPLE_16000HZ;
            break;
        case GEN_CFG_SAMPLE_22050HZ : 
            aud_info->aud_sample = AUD_SAMPLE_22050HZ;
            break;
        case GEN_CFG_SAMPLE_24000HZ : 
            aud_info->aud_sample = AUD_SAMPLE_24000HZ;
            break;
        case GEN_CFG_SAMPLE_32000HZ : 
            aud_info->aud_sample = AUD_SAMPLE_32000HZ;
            break;
        case GEN_CFG_SAMPLE_44100HZ : 
            aud_info->aud_sample = AUD_SAMPLE_44100HZ;
            break;
        case GEN_CFG_SAMPLE_48000HZ : 
            aud_info->aud_sample = AUD_SAMPLE_48000HZ;
            break;
        case GEN_CFG_SAMPLE_88200HZ : 
            aud_info->aud_sample = AUD_SAMPLE_88200HZ;
            break;
        case GEN_CFG_SAMPLE_96000HZ : 
            aud_info->aud_sample = AUD_SAMPLE_96000HZ;
            break;
        case GEN_CFG_SAMPLE_176400HZ : 
            aud_info->aud_sample = AUD_SAMPLE_176400HZ;
            break;
        case GEN_CFG_SAMPLE_192000HZ : 
            aud_info->aud_sample = AUD_SAMPLE_192000HZ;
            break;
        case GEN_CFG_SAMPLE_384000HZ : 
            aud_info->aud_sample = AUD_SAMPLE_384000HZ;
            break;
        default : 
            status = LEA_ERR_ERROR_SAMPLE;
            return status;
            break;

    }
    switch(lea_info->codec_cfg.frame_dur)
    {
        case GEN_CFG_FRAME_DUR_7_5MS : 
            aud_info->frame_dur = 75;
            break;
        case GEN_CFG_FRAME_DUR_10MS : 
            aud_info->frame_dur = 100;
            break;
        default : 
            status = LEA_ERR_ERROR_FRAME_DUR;
            return status;
            break;
    }
    /// one channel
    aud_info->aud_ch = 1;
    /// 16bits
    aud_info->aud_bits_wide = 16;
    aud_info->ch_alloc = lea_info->codec_cfg.ch_alloc;
    aud_info->frame_len = lea_info->codec_cfg.oct_per_frame;
}
void bt_stack_lea_enable(void)
{
    CLOGD("bt_stack_lea_enable, svc_mask:%x", LEA_SERVICE_MASK);
    app_lea_enable(&ble_lea_cfg, &ble_lea_cb);
}

void bt_stack_lea_send_data(uint8_t conidx, uint16_t len, uint8_t *data)
{
    btos_event_t ev;
    bt_aud_pkt_info_t *pkt_info;
    uint16_t ev_len = sizeof(btos_msg_t) + sizeof(bt_aud_pkt_info_t);
    
    ev.msg_body = btos_malloc(ev_len);
    ev.msg_body->msg_id = AUD_OS_RCV_DATA_EVT;
    ev.msg_body->param_len = ev_len;
    
    pkt_info = ev.msg_body->param;
    pkt_info->conidx = conidx;
    pkt_info->frame_num = 1;
    pkt_info->seq = (data[5] << 8) | data[4];
    pkt_info->len = len;
    pkt_info->data = data;

    btos_send_event(OS_TASK_ID_AUD, &ev, BTOS_TASK_MAX_DELAY);
}

uint8_t bt_stack_lea_send_start(uint8_t conidx, bap_iso_codec_info_t *info)
{
    uint8_t status = LEA_ERR_NO_ERROR;
    btos_event_t ev;
    aud_codec_info_t *codec_info;
    ev.msg_body = btos_malloc(sizeof(btos_msg_t) + sizeof(aud_codec_info_t));
    ev.msg_body->msg_id = AUD_OS_START_EVT;
    ev.msg_body->param_len = sizeof(aud_codec_info_t);
    codec_info = ev.msg_body->param;
    bt_stack_switch_aud_info(info, codec_info);
    btos_send_event(OS_TASK_ID_AUD, &ev, BTOS_TASK_MAX_DELAY);
}

void bt_stack_lea_send_stop(uint8_t conidx, uint16_t len, uint8_t *data)
{
    btos_event_t ev;
    
    ev.msg_body = btos_malloc(sizeof(btos_msg_t));
    ev.msg_body->msg_id = AUD_OS_STOP_EVT;
    ev.msg_body->param_len = 0;
    btos_send_event(OS_TASK_ID_AUD, &ev, BTOS_TASK_MAX_DELAY);
}

#endif

/*
 * bt_app_if.c
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

#include "bt_stack_if.h"
#include "bt_ble_if.h"
#include "bt_app_if.h"
#include "bt_os_task.h"

#include "hogpd_msg.h"
#include "hogpd.h"
#include "bass.h"
#include "diss.h"
#include "netcfg_bles.h"

#ifndef ASSERT_ERR
#define ASSERT_ERR( x ) configASSERT( x )
#endif

/*
 * LOCAL FUNCTIONS DECLARATION
 ****************************************************************************************
 */
extern uint8_t bt_stack_ble_hid_send(uint8_t conidx, uint8_t report_idx, uint8_t length, uint8_t* value);
extern void bt_stack_ble_adv_start(ble_adv_cfg_t *adv_cfg);
extern uint8_t bt_stack_nvs_get(uint8_t param_id, uint8_t * lengthPtr, uint8_t *buf);
extern void bt_stack_ble_adv_stop(uint8_t adv_id);

/*
 * LOCAL VARIABLES
 ****************************************************************************************
 */


/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */
#if (ADV_USER_DATA)
extern ble_gap_cfg_t bt_stack_dev_cfg;

uint8_t manu_data[18] =
{
    0xab, 0x0a,
    0xa1, 0xdc, 0xa8, 0x76, 0x83, 0x65, 0x73, 0x83,
    0x72, 0x65, 0x82, 0x67, 0x83, 0x68, 0x00, 0x78,
};
#endif
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
uint8_t app_ble_adv_start(uint8_t adv_id, uint8_t adv_type)
{
    uint8_t status = 0;

    btos_event_t ev;
    ble_adv_info_t *adv_info;
    uint16_t ev_len = sizeof(btos_msg_t) + sizeof(ble_adv_info_t);
    
    ev.msg_body = btos_malloc(ev_len);
    ev.msg_body->msg_id = BT_OS_ADV_START_EVT;
    ev.msg_body->param_len = ev_len;
    
    adv_info = (ble_adv_info_t *)ev.msg_body->param;
    adv_info->adv_id = adv_id;
    adv_info->adv_type = adv_type;

    //CLOGD("adv start:%d", voice_cnt_s++);

    status = btos_send_event(OS_TASK_ID_BT, &ev, (uint32_t)BTOS_TASK_MAX_DELAY);
    return status;
}

uint8_t app_ble_adv_stop(uint8_t adv_id)
{
    btos_event_t ev;
    ble_adv_info_t *adv_info;
    uint16_t ev_len = sizeof(btos_msg_t) + sizeof(ble_adv_info_t);
    
    ev.msg_body = btos_malloc(ev_len);
    ev.msg_body->msg_id = BT_OS_ADV_STOP_EVT;
    ev.msg_body->param_len = ev_len;
    
    adv_info = (ble_adv_info_t *)ev.msg_body->param;
    adv_info->adv_id = adv_id;
    adv_info->adv_type = 0;

    //CLOGD("adv start:%d", voice_cnt_s++);

    return btos_send_event(OS_TASK_ID_BT, &ev, (uint32_t)BTOS_TASK_MAX_DELAY);
}
uint8_t app_ble_hogpd_hid_send(uint8_t conidx, uint8_t report_idx, uint8_t length, uint8_t* value)
{
    uint8_t status;

    btos_event_t ev;
    ble_hogpd_info_t *hogpd_info;
    uint16_t ev_len = sizeof(btos_msg_t) + sizeof(ble_hogpd_info_t) + length;
    
    ev.msg_body = btos_malloc(ev_len);
    ev.msg_body->msg_id = BT_OS_HID_SEND_EVT;
    ev.msg_body->param_len = ev_len;
    
    hogpd_info = (ble_hogpd_info_t *)ev.msg_body->param;
    hogpd_info->conidx = conidx;
    hogpd_info->report_idx = report_idx;
    hogpd_info->len = length;
    memcpy(hogpd_info->value, value, length);

    //CLOGD("app hid s,idx:%d, len:%d", report_idx, length);

    status = btos_send_event(OS_TASK_ID_BT, &ev, (uint32_t)BTOS_TASK_MAX_DELAY);
    return status;
}

//uint16_t voice_cnt_s = 0;
//uint16_t voice_cnt_r = 0;
//uint16_t voice_cnt_r_c = 0;

uint8_t app_ble_voice_data_send(uint8_t conidx, uint8_t report_idx, uint8_t length, uint8_t* value)
{
    btos_event_t ev;
    ble_hogpd_info_t *hogpd_info;
    uint16_t ev_len = sizeof(btos_msg_t) + sizeof(ble_hogpd_info_t) + length;
    uint8_t status = 0xff;
    ev.msg_body = btos_malloc(ev_len);
    if(ev.msg_body == NULL)
    {
        CLOGD("ev.msg_body null!");
        return status;
    }
    ev.msg_body->msg_id = BT_OS_VOICE_DATA_SEND_EVT;
    ev.msg_body->param_len = ev_len;
    
    hogpd_info = (ble_hogpd_info_t *)ev.msg_body->param;
    hogpd_info->conidx = conidx;
    hogpd_info->report_idx = report_idx;
    hogpd_info->len = length;
    memcpy(hogpd_info->value, value, length);

    //CLOGD("voc s:%d", voice_cnt_s++);

    return btos_send_event(OS_TASK_ID_BT, &ev, (uint32_t)BTOS_TASK_MAX_DELAY);
}

uint8_t app_ble_netcfg_bles_send_notify(uint8_t conidx, uint8_t op, uint8_t state, uint8_t length, uint8_t* value)
{
    btos_event_t ev;
    ble_net_cfg_info_t *netcfg_info;
    uint16_t ev_len = sizeof(btos_msg_t) + sizeof(ble_net_cfg_info_t) + length;
    uint8_t status = 0xff;
    ev.msg_body = btos_malloc(ev_len);
    if(ev.msg_body == NULL)
    {
        CLOGD("ev.msg_body null!");
        return status;
    }
    ev.msg_body->msg_id = BT_OS_NET_CFG_SEND_EVT;
    ev.msg_body->param_len = ev_len;
    
    netcfg_info = (ble_net_cfg_info_t *)ev.msg_body->param;
    netcfg_info->conidx = conidx;
    netcfg_info->status = state;
    netcfg_info->op = op;
    netcfg_info->len = length;
    memcpy(netcfg_info->value, value, length);

    CLOGD("app_ble_netcfg_bles_send_notify");

    return btos_send_event(OS_TASK_ID_BT, &ev, (uint32_t)BTOS_TASK_MAX_DELAY);
}

#if (ADV_USER_DATA)
uint8_t ble_gen_user_adv_data(uint8_t *p_data)
{
    uint8_t nb_uuid = 1;

    uint16_t uuids[1] = {HID_UUID};

    // Remaining Length
    uint8_t rem_len = LEGA_ADV_DATA_LEN - 3;

    uint8_t *p_buf = p_data;
    uint8_t length = 0;

#if 0
    /// add uuid 16 list
    *p_buf ++ = 1 + nb_uuid * 2;
    *p_buf ++ = 0x03;//GAP_AD_TYPE_COMPLETE_LIST_16_BIT_UUID;

    memcpy(p_buf, uuids, nb_uuid*2);
    p_buf += nb_uuid*2;
    length += nb_uuid*2+2;
    /// add appearance
    *p_buf ++ = 3;
    *p_buf ++ = 0x19;//GAP_AD_TYPE_APPEARANCE;
    *p_buf ++ = remote_bt_cfg.appearance & 0xff;
    *p_buf ++ = (remote_bt_cfg.appearance>>8) & 0xff;
    length += 4;
#endif
#if 1
    /// add Manufacturer specific
    *p_buf ++ = sizeof(manu_data) + 1;
    *p_buf ++ = 0xff; //GAP_AD_TYPE_MANU_SPECIFIC_DATA;
    memcpy(p_buf, manu_data, 18);
    p_buf += sizeof(manu_data);
    length += (sizeof(manu_data) + 2);
#endif
    // Sanity check
    ASSERT_ERR(rem_len >= LEGA_ADV_DATA_LEN - 3);

    // Get remaining space in the Advertising Data - 2 bytes are used for name length/flag
    rem_len -= length;

    // Check if additional data can be added to the Advertising data - 2 bytes needed for type and length
    if (rem_len > 2)
    {
        uint8_t dev_name_length = MIN(bt_stack_dev_cfg.name_len, (rem_len - 2));

        // Device name length
        *p_buf = dev_name_length + 1;
        // Device name flag (check if device name is complete or not)
        *(p_buf + 1) = (dev_name_length == bt_stack_dev_cfg.name_len) ? 0x09 : 0x08;//GAP_AD_TYPE_COMPLETE_NAME : GAP_AD_TYPE_SHORTENED_NAME;
        // Copy device name
        memcpy(p_buf + 2, bt_stack_dev_cfg.name, dev_name_length);

        // Update advertising data length
        length += (dev_name_length + 2);
    }
    return length;
}
#endif
/// user app handler
uint8_t app_ble_adv_start_handler(ble_adv_info_t *adv_info)
{
    uint8_t status = 0;
    CLOGD("app_ble_adv_start_handler:%d", adv_info->adv_type);

    ble_adv_cfg_t adv_param;
    memset(&adv_param, 0x00, sizeof(ble_adv_cfg_t));
    adv_param.adv_id = adv_info->adv_id;
    adv_param.adv_type = GAPM_ADV_TYPE_LEGACY;
    adv_param.intv_min = BLE_ADV_MIN;
    adv_param.intv_max = BLE_ADV_MAX;

    switch(adv_info->adv_type)
    {
        case BLE_ADV_GEN :
        case BLE_ADV_GEN_PAIRED :
        {
            adv_param.disc_mode = GAPM_ADV_MODE_GEN_DISC;
            adv_param.flags = GAPM_ADV_PROP_CONNECTABLE | GAPM_ADV_PROP_SCANNABLE;
            adv_param.adv_filter = GAP_ADV_SCAN_ANY_CON_ANY;
            #if (ADV_USER_DATA)
            uint8_t adv_user_data[LEGA_ADV_DATA_LEN];
            adv_param.adv_param.gen_adv.user_data_len = ble_gen_user_adv_data(adv_user_data);
            adv_param.adv_param.gen_adv.user_data = (uint8_t*)adv_user_data;
            #else
            adv_param.adv_param.gen_adv.user_data_len = 0;
            #endif
            adv_param.adv_param.gen_adv.rsp_data_len = sizeof(APP_UUID_DATA) - 1;
            adv_param.adv_param.gen_adv.rsp_data = (uint8_t*)APP_UUID_DATA;
            #if WHITE_LIST_ADV_ENABLE
            if(adv_info->adv_type == BLE_ADV_GEN_PAIRED)
            {
                adv_param.adv_filter = GAP_ADV_SCAN_WLST_CON_WLST;
            }
            #endif
            bt_stack_ble_adv_start(&adv_param);
        }break;
        case BLE_ADV_DIR :
        {
            uint16_t len = 6;
            if(bt_stack_nvs_get(NVS_ID_PEER_ADDRESS, (uint8_t*)&len, adv_param.adv_param.dir_adv.peer_addr) == NVDS_OK) 
            {
                adv_param.disc_mode = GAPM_ADV_MODE_NON_DISC;
                adv_param.flags = GAPM_ADV_PROP_CONNECTABLE | GAPM_ADV_PROP_DIRECTED;
                adv_param.adv_filter = GAP_ADV_SCAN_ANY_CON_WLST;

                bt_stack_ble_adv_start(&adv_param);
            }
            else
            {
                status = 0x01;
                CLOGW("dir adv fail, no peer!");
            }
        }break;
        case BLE_ADV_DIR_HDC :
        {
            uint16_t len = 6;
            if(bt_stack_nvs_get(NVS_ID_PEER_ADDRESS, (uint8_t*)&len, adv_param.adv_param.dir_adv.peer_addr) == NVDS_OK) 
            {
                adv_param.disc_mode = GAPM_ADV_MODE_NON_DISC;
                adv_param.flags = GAPM_ADV_PROP_CONNECTABLE | GAPM_ADV_PROP_DIRECTED | GAPM_ADV_PROP_HDC;
                adv_param.adv_filter = GAP_ADV_SCAN_ANY_CON_ANY;

                bt_stack_ble_adv_start(&adv_param);
            }
            else
            {
                status = 0x02;
                CLOGW("hdc adv fail, no peer!");
            }

        }break;
    }
    return status;
}

uint8_t app_ble_adv_stop_handler(ble_adv_info_t *adv_info)
{
    bt_stack_ble_adv_stop(adv_info->adv_id);
    return 0;
}

uint8_t app_ble_hogpd_hid_send_handler(ble_hogpd_info_t *hogpd_info)
{
    //CLOGD("app hid s hdl,idx:%d, len:%d", hogpd_info->report_idx, hogpd_info->len);
    return bt_stack_ble_hid_send(hogpd_info->conidx, hogpd_info->report_idx, hogpd_info->len, hogpd_info->value);
}

uint8_t app_ble_voice_data_send_handler(ble_hogpd_info_t *hogpd_info)
{
    //static uint32_t time1 = 0, time2 = 0;
    uint8_t status = 0;
    
    //CLOGD("voc r:%d", voice_cnt_r++);
    //time1 = co_time_us_get();
    //CLOGD("app hid s hdl,idx:%d, len:%d", hogpd_info->report_idx, hogpd_info->len);
    status = bt_stack_ble_hid_send(hogpd_info->conidx, hogpd_info->report_idx, 20, &hogpd_info->value[0]);
    status = bt_stack_ble_hid_send(hogpd_info->conidx, hogpd_info->report_idx, 20, &hogpd_info->value[20]);
    status = bt_stack_ble_hid_send(hogpd_info->conidx, hogpd_info->report_idx, 20, &hogpd_info->value[40]);
    //time2 = co_time_us_get();
    //CLOGD("voc r:%d t:%d", voice_cnt_r_c++, time2 - time1);
    return status;
}

uint8_t app_ble_netcfg_bles_send_notify_handler(ble_net_cfg_info_t *netcfg_info)
{
    uint8_t status = 0;
    status = ble_netcfg_bles_send_notify(netcfg_info->conidx, netcfg_info->op, netcfg_info->status);

    return status;
}

uint8_t app_ble_connected_state(uint8_t conidx)
{
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();
    return stack_env->bt_ble_connected;
}



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

#include "bt_app_if.h"
#include "bt_ble_if.h"
#include "bt_os_task.h"

#include "hogpd_msg.h"
#include "hogpd.h"
#include "bass.h"
#include "diss.h"
#include "netcfg_bles.h"

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

void* btos_buf_alloc_proc(uint16_t msg_id, void * msg_body, uint32_t msg_body_len, btos_event_t *evn_ptr)
{

    CLOGD("btos_buf_alloc_proc: msg_id 0x%x", msg_id);
    evn_ptr->msg_body = btos_malloc(sizeof(btos_msg_t) + msg_body_len);
    if(evn_ptr->msg_body == NULL)
    {
        CLOGD("btos_buf_alloc_proc null!");
        return NULL;
    }

    evn_ptr->msg_body->msg_id = msg_id;
    evn_ptr->msg_body->param_len = msg_body_len;//?sizeof(btos_msg_t) + msg_body_len
    if(0 !=msg_body_len)
    {
        memcpy(evn_ptr->msg_body->param, msg_body, strlen(msg_body_len));
    }

    return evn_ptr;
}



uint8_t app_ble_adv_start(uint8_t adv_id, uint8_t adv_type)
{
    ble_adv_info_t adv_info;

    adv_info.adv_id = adv_id;
    adv_info.adv_type = adv_type;


#ifdef CFG_AMP_IPC

    return btos_send_app_evt_api(OS_TASK_ID_BT, BT_OS_ADV_START_EVT, &adv_info, sizeof(ble_adv_info_t), (uint32_t)BTOS_TASK_MAX_DELAY);
#else
    btos_event_t ev;

    btos_buf_alloc_proc(BT_OS_ADV_START_EVT, &adv_info, sizeof(ble_adv_info_t), &ev);

    return btos_send_event(OS_TASK_ID_BT, &ev, (uint32_t)BTOS_TASK_MAX_DELAY);
#endif

}

uint8_t app_ble_adv_stop(uint8_t adv_id)
{


    ble_adv_info_t adv_info;

    adv_info.adv_id = adv_id;
    adv_info.adv_type = 0;

#ifdef CFG_AMP_IPC

    return btos_send_app_evt_api(OS_TASK_ID_BT, BT_OS_ADV_STOP_EVT, &adv_info, sizeof(ble_adv_info_t), (uint32_t)BTOS_TASK_MAX_DELAY);
#else
    btos_event_t ev;

    btos_buf_alloc_proc(BT_OS_ADV_STOP_EVT, &adv_info, sizeof(ble_adv_info_t), &ev);

    return btos_send_event(OS_TASK_ID_BT, &ev, (uint32_t)BTOS_TASK_MAX_DELAY);
#endif


}
uint8_t app_ble_hogpd_hid_send(uint8_t conidx, uint8_t report_idx, uint8_t length, uint8_t* value)
{

    ble_hogpd_info_t hogpd_info;
    hogpd_info.conidx = conidx;
    hogpd_info.report_idx = report_idx;
    hogpd_info.len = length;
    memcpy(hogpd_info.value, value, length);

#ifdef CFG_AMP_IPC
    
    return btos_send_app_evt_api(OS_TASK_ID_BT, BT_OS_HID_SEND_EVT, &hogpd_info, sizeof(ble_hogpd_info_t) + length, (uint32_t)BTOS_TASK_MAX_DELAY);
#else
    btos_event_t ev;

    btos_buf_alloc_proc(BT_OS_HID_SEND_EVT, &hogpd_info, sizeof(ble_hogpd_info_t) + length, &ev);

    return btos_send_event(OS_TASK_ID_BT, &ev, (uint32_t)BTOS_TASK_MAX_DELAY);
#endif


}

//uint16_t voice_cnt_s = 0;
//uint16_t voice_cnt_r = 0;
//uint16_t voice_cnt_r_c = 0;

uint8_t app_ble_voice_data_send(uint8_t conidx, uint8_t report_idx, uint8_t length, uint8_t* value)
{

    ble_hogpd_info_t hogpd_info;
    hogpd_info.conidx = conidx;
    hogpd_info.report_idx = report_idx;
    hogpd_info.len = length;
    memcpy(hogpd_info.value, value, length);
    

#ifdef CFG_AMP_IPC

    return btos_send_app_evt_api(OS_TASK_ID_BT, BT_OS_VOICE_DATA_SEND_EVT, &hogpd_info, sizeof(ble_hogpd_info_t) + length, (uint32_t)BTOS_TASK_MAX_DELAY);
#else
    btos_event_t ev;

    btos_buf_alloc_proc(BT_OS_VOICE_DATA_SEND_EVT, &hogpd_info, sizeof(ble_hogpd_info_t) + length, &ev);

    return btos_send_event(OS_TASK_ID_BT, &ev, (uint32_t)BTOS_TASK_MAX_DELAY);
#endif


}

uint8_t app_ble_netcfg_bles_send_notify(uint8_t conidx, uint8_t op, uint8_t state, uint8_t length, uint8_t* value)
{

    ble_net_cfg_info_t netcfg_info;
    netcfg_info.conidx = conidx;
    netcfg_info.status = state;
    netcfg_info.op = op;
    netcfg_info.len = length;
    memcpy(netcfg_info.value, value, length);
    


#ifdef CFG_AMP_IPC
    
    return btos_send_app_evt_api(OS_TASK_ID_BT, BT_OS_NET_CFG_SEND_EVT, &netcfg_info, sizeof(ble_net_cfg_info_t) + length, (uint32_t)BTOS_TASK_MAX_DELAY);
#else
    btos_event_t ev;

    btos_buf_alloc_proc(BT_OS_NET_CFG_SEND_EVT, &netcfg_info, sizeof(ble_net_cfg_info_t) + length, &ev);

    return btos_send_event(OS_TASK_ID_BT, &ev, (uint32_t)BTOS_TASK_MAX_DELAY);
#endif


}

///max length is not more than HOGPD_REPORT_MAX_LEN
uint8_t app_ble_user_data_send(uint8_t conidx, uint8_t length, uint8_t* value)
{
    uint8_t status = 0xff;

    status = app_ble_hogpd_hid_send(conidx, HIDS_GDE_ACK_INDEX, length, value);
    return status;
}

uint8_t app_ble_user_data_rcv(uint8_t length, uint8_t* value)
{
    uint8_t status = 0xff;

    CLOGD("app_ble_user_date_rcv,len:%d, data:0x%x%x%x%x", length, value[3], value[2], value[1], value[0]);
    return status;
}

uint8_t app_ble_scan_param(uint8_t type, uint8_t phy, uint16_t scan_intv, uint16_t scan_win)
{
    uint8_t status = 0;

    btos_event_t ev;
    ble_scan_info_t *scan_info;
    uint16_t ev_len = sizeof(btos_msg_t) + sizeof(ble_scan_info_t);

    ev.msg_body = btos_malloc(ev_len);
    ev.msg_body->msg_id = BT_OS_SCAN_START_EVT;
    ev.msg_body->param_len = ev_len;

    scan_info = (ble_scan_info_t *)ev.msg_body->param;
    scan_info->scan_param_dft = 0;
    scan_info->scan_type = type;
    scan_info->scan_phy  = phy;
    scan_info->scan_intv = scan_intv;
    scan_info->scan_win  = scan_win;

    //CLOGD("adv start:%d", voice_cnt_s++);

    status = btos_send_event(OS_TASK_ID_BT, &ev, (uint32_t)BTOS_TASK_MAX_DELAY);
    return status;
}

uint8_t app_ble_scan_stop(uint8_t scan_id)
{
    uint8_t status = 0;

    btos_event_t ev;
    ble_scan_info_t *scan_info;
    uint16_t ev_len = sizeof(btos_msg_t) + sizeof(ble_scan_info_t);

    ev.msg_body = btos_malloc(ev_len);
    ev.msg_body->msg_id = BT_OS_SCAN_STOP_EVT;
    ev.msg_body->param_len = ev_len;

    scan_info = (ble_scan_info_t *)ev.msg_body->param;
    scan_info->scan_id = scan_id;

    //CLOGD("adv start:%d", voice_cnt_s++);

    status = btos_send_event(OS_TASK_ID_BT, &ev, (uint32_t)BTOS_TASK_MAX_DELAY);
    return status;

}

uint8_t app_ble_scan_start(uint8_t scan_id)
{
    uint8_t status = 0;

    btos_event_t ev;
    ble_scan_info_t *scan_info;
    uint16_t ev_len = sizeof(btos_msg_t) + sizeof(ble_scan_info_t);

    ev.msg_body = btos_malloc(ev_len);
    ev.msg_body->msg_id = BT_OS_SCAN_START_EVT;
    ev.msg_body->param_len = ev_len;

    scan_info = (ble_scan_info_t *)ev.msg_body->param;
    scan_info->scan_id = scan_id;
    scan_info->scan_param_dft = 1;

    //CLOGD("adv start:%d", voice_cnt_s++);

    status = btos_send_event(OS_TASK_ID_BT, &ev, (uint32_t)BTOS_TASK_MAX_DELAY);
    return status;
}

uint8_t app_ble_conn(gap_bdaddr_t addr, uint8_t phy, uint16_t conn_intv_min, uint16_t conn_intv_max, uint16_t conn_latency, uint16_t conn_super_to)
{

    uint8_t status;

    btos_event_t ev;
    ble_conn_info_t *conn_info;
    uint16_t ev_len = sizeof(btos_msg_t) + sizeof(ble_conn_info_t);

    ev.msg_body = btos_malloc(ev_len);
    ev.msg_body->msg_id = BT_OS_CONNECT_EVT;
    ev.msg_body->param_len = ev_len;

    conn_info = (ble_conn_info_t *)ev.msg_body->param;
    conn_info->addr = addr;
    conn_info->phy  = phy;
    conn_info->conn_intv_min = conn_intv_min;
    conn_info->conn_intv_max = conn_intv_max;
    conn_info->conn_intv_max = conn_latency;
    conn_info->conn_super_to = conn_super_to;

    //CLOGD("app hid s,idx:%d, len:%d", report_idx, length);

    status = btos_send_event(OS_TASK_ID_BT, &ev, (uint32_t)BTOS_TASK_MAX_DELAY);
    return status;

}

uint8_t app_ble_conn_update(uint8_t conidx, uint16_t conn_intv_min, uint16_t conn_intv_max, uint16_t latency, uint16_t super_to)
{

    uint8_t status;

    btos_event_t ev;
    ble_conn_update_info_t *conn_update_info;
    uint16_t ev_len = sizeof(btos_msg_t) + sizeof(ble_conn_update_info_t);

    ev.msg_body = btos_malloc(ev_len);
    ev.msg_body->msg_id = BT_OS_CONNECT_UPDATE_EVT;
    ev.msg_body->param_len = ev_len;

    conn_update_info = (ble_conn_update_info_t *)ev.msg_body->param;
    conn_update_info->conhdl = conidx;
    conn_update_info->intv_min = conn_intv_min;
    conn_update_info->intv_max = conn_intv_max;
    conn_update_info->latency = latency;
    conn_update_info->time_out = super_to;
    //CLOGD("app hid s,idx:%d, len:%d", report_idx, length);

    status = btos_send_event(OS_TASK_ID_BT, &ev, (uint32_t)BTOS_TASK_MAX_DELAY);
    return status;
}

uint8_t app_ble_dsiconnect(uint8_t conidx, uint8_t reason)
{

    uint8_t status;

    btos_event_t ev;
    ble_disconn_info_t *disconn_info;
    uint16_t ev_len = sizeof(btos_msg_t) + sizeof(ble_disconn_info_t);

    ev.msg_body = btos_malloc(ev_len);
    ev.msg_body->msg_id = BT_OS_DISCONNECT_EVT;
    ev.msg_body->param_len = ev_len;

    disconn_info = (ble_disconn_info_t *)ev.msg_body->param;
    disconn_info->conidx = conidx;
    disconn_info->reason = reason;

    //CLOGD("app hid s,idx:%d, len:%d", report_idx, length);

    status = btos_send_event(OS_TASK_ID_BT, &ev, (uint32_t)BTOS_TASK_MAX_DELAY);
    return status;


}




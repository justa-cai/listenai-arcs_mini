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

#include "bt_stack_cfg.h"
#include "bt_ble_if.h"
#include "bt_app_if.h"
#include "bt_os_task.h"

#include "bt_stack_hal.h"
#include "bt_app_hal.h"
#include "bt_ble_hal.h"
#include "lisa_bluetooth.h"

#include "hogpd_msg.h"
#include "hogpd.h"
#include "bass.h"
#include "diss.h"

/*
 * LOCAL FUNCTIONS DECLARATION
 ****************************************************************************************
 */
extern uint8_t bt_stack_ble_hid_send(uint8_t conidx, uint8_t report_idx, uint8_t length, uint8_t* value);
extern void bt_stack_ble_adv_start(ble_adv_cfg_t *adv_cfg);
extern uint8_t bt_stack_nvs_get(uint8_t param_id, uint8_t * lengthPtr, uint8_t *buf);
extern void bt_stack_ble_adv_stop(uint8_t adv_id);
extern uint8_t app_ble_adv_start_handler(ble_adv_info_t *adv_info);

/*
 * LOCAL VARIABLES
 ****************************************************************************************
 */


/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */
app_handler_by_user_cb_t app_bt_user_handler;

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

uint8_t app_ble_disconnect(uint8_t conidx, uint8_t reason)
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

/// handler
uint8_t app_ble_adv_start_handler(ble_adv_info_t *adv_info)
{
    if(app_bt_user_handler.cb_app_ble_adv_start_handler)
    {
        return app_bt_user_handler.cb_app_ble_adv_start_handler(adv_info);
    }
    else
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
                {
                    uint8_t adv_data_len = 0;
                    const uint8_t *adv_data = lisa_bt_get_adv_data(&adv_data_len);
                    adv_param.adv_param.gen_adv.user_data_len = adv_data_len;
                    adv_param.adv_param.gen_adv.user_data = (uint8_t *)adv_data;

                    uint8_t rsp_data_len = 0;
                    const uint8_t *rsp_data = lisa_bt_get_scan_rsp_data(&rsp_data_len);
                    adv_param.adv_param.gen_adv.rsp_data_len = rsp_data_len;
                    adv_param.adv_param.gen_adv.rsp_data = (uint8_t *)rsp_data;
                }
            #if WHITE_LIST_ADV_ENABLE
                if(adv_info->adv_type == BLE_ADV_GEN_PAIRED)
                {
                    adv_param.adv_filter = GAP_ADV_SCAN_WLST_CON_WLST;
                }
            #endif
                CLOGD("adv user_data_len:%d", adv_param.adv_param.gen_adv.user_data_len);
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
}

uint8_t app_ble_adv_stop_handler(ble_adv_info_t *adv_info)
{
    bt_stack_ble_adv_stop(adv_info->adv_id);
    return 0;
}

uint8_t app_ble_conn_handler(ble_conn_info_t *conn_info)
{
    uint8_t status = 0;
    bt_stack_ble_connect(conn_info->addr, conn_info->phy, conn_info->conn_intv_min, conn_info->conn_intv_max, conn_info->conn_latency, conn_info->conn_super_to);
    return status;
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

    if(app_bt_user_handler.cb_app_ble_netcfg_bles_send_notify_handler)
    {
        return app_bt_user_handler.cb_app_ble_netcfg_bles_send_notify_handler(netcfg_info);
    }
    return status;
}

uint8_t app_ble_connected_state(uint8_t conidx)
{
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();
    return stack_env->bt_ble_connected;
}

#if (BT_STACK_PRESENT)

uint8_t app_bt_scan(uint8_t enable)
{
    uint8_t status = 0;

    btos_event_t ev;
    bt_scan_info_t *scan_info;
    uint16_t ev_len = sizeof(btos_msg_t) + sizeof(bt_scan_info_t);

    ev.msg_body = btos_malloc(ev_len);
    ev.msg_body->msg_id = BT_OS_BT_SCAN_EVT;
    ev.msg_body->param_len = ev_len;

    scan_info = (bt_scan_info_t *)ev.msg_body->param;

    scan_info->scan_en = enable;

    //CLOGD("adv start:%d", voice_cnt_s++);

    status = btos_send_event(OS_TASK_ID_BT, &ev, (uint32_t)BTOS_TASK_MAX_DELAY);
    return status;
}

uint8_t app_bt_inq_start(uint8_t disc_mode, uint8_t max_count)
{
    uint8_t status = 0;

    btos_event_t ev;
    bt_inquiry_info_t *inquiry_info;
    uint16_t ev_len = sizeof(btos_msg_t) + sizeof(bt_inquiry_info_t);

    ev.msg_body = btos_malloc(ev_len);
    ev.msg_body->msg_id = BT_OS_BT_INQ_START_EVT;
    ev.msg_body->param_len = ev_len;

    inquiry_info = (bt_inquiry_info_t *)ev.msg_body->param;

    inquiry_info->disc_mode = disc_mode;
    inquiry_info->max_count = max_count;

    //CLOGD("adv start:%d", voice_cnt_s++);

    status = btos_send_event(OS_TASK_ID_BT, &ev, (uint32_t)BTOS_TASK_MAX_DELAY);
    return status;
}

uint8_t app_bt_inq_stop(void)
{
    uint8_t status = 0;

    btos_event_t ev;
    uint16_t ev_len = sizeof(btos_msg_t);

    ev.msg_body = btos_malloc(ev_len);
    ev.msg_body->msg_id = BT_OS_BT_INQ_STOP_EVT;
    ev.msg_body->param_len = 0;


    status = btos_send_event(OS_TASK_ID_BT, &ev, (uint32_t)BTOS_TASK_MAX_DELAY);
    return status;
}

uint8_t app_bt_conn(gap_bdaddr_t addr, uint8_t type, uint16_t clk_off, uint8_t page_scan_rep_mode)
{
    uint8_t status = 0;

    btos_event_t ev;
    bt_connect_info_t *connect_info;
    uint16_t ev_len = sizeof(btos_msg_t) + sizeof(bt_connect_info_t);

    ev.msg_body = btos_malloc(ev_len);
    ev.msg_body->msg_id = BT_OS_BT_CONNECT_EVT;
    ev.msg_body->param_len = ev_len;

    connect_info = (bt_connect_info_t *)ev.msg_body->param;

    connect_info->addr    = addr;
    connect_info->type    = type;
    connect_info->clk_off = clk_off;
    connect_info->page_scan_rep_mode = page_scan_rep_mode;

    //CLOGD("adv start:%d", voice_cnt_s++);

    status = btos_send_event(OS_TASK_ID_BT, &ev, (uint32_t)BTOS_TASK_MAX_DELAY);
    return status;

}

uint8_t app_bt_conn_cancel(void)
{
    uint8_t status = 0;

    btos_event_t ev;
    uint16_t ev_len = sizeof(btos_msg_t);

    ev.msg_body = btos_malloc(ev_len);
    ev.msg_body->msg_id = BT_OS_BT_CONNECT_CANCEL_EVT;
    ev.msg_body->param_len = 0;

    status = btos_send_event(OS_TASK_ID_BT, &ev, (uint32_t)BTOS_TASK_MAX_DELAY);
    return status;
}

uint8_t app_bt_disconnect(uint8_t conidx, uint8_t reason)
{
    app_ble_disconnect(conidx, reason);
}

#endif

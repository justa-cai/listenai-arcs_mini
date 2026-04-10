/*
 * bt_stack_hal.c
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

#include "bt_stack_hal.h"
#include "bt_ble_hal.h"
#include "bt_classic_hal.h"
#include "bt_app_hal.h"

#include "bt_stack_cfg.h"
#include "bt_ble_if.h"
#include "bt_app_if.h"

#include "atcmd_bt_if.h"
#if CONFIG_BT_AUDIO
#include "aud_common.h"
#endif

#if BT_STACK_PRESENT
#include "bt_classic_if.h"
#include "bt_call_hal.h"
#include "bt_music_hal.h"
#endif

/*
 * MACROS
 ****************************************************************************************
 */


/*
 * DEFINES
 ****************************************************************************************
 */

/*
 * LOCAL FUNCTIONS DECLARATION
 ****************************************************************************************
 */
extern void bt_platform_init(uint32_t flag);
extern void lsip_isr_notify_reg(void *notify);
extern void app_bt_data_free(void *ptr);
extern uint8_t app_ble_netcfg_bles_send_notify_handler(ble_net_cfg_info_t *netcfg_info);

static void bt_stack_init(uint8_t init_state);
static uint8_t bt_stack_can_sleep();
static void bt_stack_sleep(uint16_t sleep_state);
static void bt_stack_user_schdule();

static void bt_stack_enable_cmp(uint16_t status);
static void bt_stack_conn_ind(uint8_t conidx, uint16_t conhdl, gap_bdaddr_t *peer_addr);
static void bt_stack_disc_ind(uint8_t conidx, uint16_t conhdl, uint16_t reason);
static void bt_stack_key_req(uint8_t conidx, uint8_t key_type, uint32_t key);
static void bt_stack_bond_ind(uint8_t conidx, uint16_t status);
static void bt_stack_info_ind(uint8_t conidx, uint8_t type, ble_info_data_t *data);
static void bt_stack_actv_start_ind(uint8_t type, uint8_t actv_id, int16_t status);
static void bt_stack_actv_stop_ind(uint8_t type, uint8_t actv_id, int16_t status);

/*
 * LOCAL VARIABLES
 ****************************************************************************************
 */

btos_msg_t notify_msg = 
{
    .msg_id = BT_OS_NOTIFY_EVT,
    .param_len = 0,
};

extern struct plf_sys_config bt_stack_plf_cfg;
extern ble_gap_cfg_t bt_stack_dev_cfg;

static const ble_task_cb_t bt_stack_cb = {
    .cb_ble_init        = bt_stack_init,
    .cb_ble_reset_cmp   = bt_stack_reset_cmp,
    .cb_ble_can_sleep   = bt_stack_can_sleep,
    .cb_ble_sleep       = bt_stack_sleep,
    .cb_user_schedule   = bt_stack_user_schdule,
};

static const os_task_cb_t bt_stack_os_cb = {
    .cb_os_init          = bt_stack_if_init,
    .cb_os_msg_handle    = bt_stack_if_msg_handle,
    .cb_os_user_schedule = bt_stack_if_user_schedule,
};

const ble_gap_cb_t bt_stack_gap_cb =
{
    .cb_ble_enable_cmp     = bt_stack_enable_cmp,
    .cb_ble_conn_ind       = bt_stack_ble_conn_ind,
    .cb_ble_conn_update    = bt_stack_ble_para_update_ind,
    .cb_ble_disc_ind       = bt_stack_disc_ind,
    .cb_ble_key_req        = bt_stack_key_req,
    .cb_ble_bond_ind       = bt_stack_bond_ind,
    .cb_ble_info_ind       = bt_stack_info_ind,
    .cb_ble_adv_report_ind = bt_stack_ble_adv_report_ind,
    .cb_ble_actv_start_ind = bt_stack_actv_start_ind,
    .cb_ble_actv_stop_ind  = bt_stack_actv_stop_ind,
};

#if BT_STACK_PRESENT
extern bt_gap_cfg_t bt_stack_classic_dev_cfg;
extern void bt_stack_classic_enable_cmp(uint16_t status);
extern void bt_stack_classic_conn_ind(uint8_t conidx, uint16_t conhdl, gap_bdaddr_t *peer_addr);
extern void bt_stack_classic_discover_ind(gap_bdaddr_t *peer_addr, uint16_t clk_off, int8_t rssi, uint8_t mode, uint32_t cod, struct gap_dev_name *name);
extern void bt_stack_classic_disc_ind(uint8_t conidx, uint16_t conhdl, uint16_t reason);
extern void bt_stack_classic_bond_ind(uint8_t conidx, uint16_t status);
#if BT_CALL_PRESENT
extern void hfp_aud_start_ind(uint8_t conidx, uint16_t codec, uint16_t status);
extern void hfp_aud_stop_ind(uint8_t conidx, uint16_t conhdl, uint16_t reason);
#endif

const bt_gap_cb_t bt_stack_classic_gap_cb =
{
    .cb_bt_enable_cmp  = bt_stack_classic_enable_cmp,
    .cb_bt_conn_ind = bt_stack_classic_conn_ind,
#if BT_CALL_PRESENT
    .cb_bt_aud_conn_ind = hfp_aud_start_ind,
    .cb_bt_aud_disc_ind = hfp_aud_stop_ind,
#endif
    .cb_bt_discover_ind  = bt_stack_classic_discover_ind,
};
#endif

/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */
extern struct ble_rf_api lsip_rf;
bt_stack_if_env_tag_t bt_stack_env;

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
 
static void bt_stack_init(uint8_t init_state)
{
    CLOGD("bt_stack_init,state:%d", init_state);

    if(init_state == BLE_TASK_INIT)
    {
        ble_task_reset();
    }
#if (BT_EMB_PRESENT || BLE_EMB_PRESENT)
    if (init_state == BLE_TASK_RST || init_state == BLE_TASK_INIT)
    {
        // Reset the RF
        lsip_rf.reset();
    }
#endif //(BT_EMB_PRESENT || BLE_EMB_PRESENT)

#if (AUDIO_SYNC_SUPPORT)
    audio_sync_init();
#endif //(AUDIO_SYNC_SUPPORT)
}

static uint8_t bt_stack_can_sleep()
{
    return 0;
}

static void bt_stack_sleep(uint16_t sleep_state)
{
// TODO:
}

static void bt_stack_user_schdule()
{
    //app_schedule();
}

static void bt_stack_enable_cmp(uint16_t status)
{
    bt_stack_ble_enable_cmp(status);
}

static void bt_stack_disc_ind(uint8_t conidx, uint16_t conhdl, uint16_t reason)
{
    if(conidx < BLE_CONNECTION_MAX)
    {
        bt_stack_ble_disc_ind(conidx, conhdl, reason);
    }
#if BT_STACK_PRESENT
    else
    {
        bt_stack_classic_disc_ind(conidx, conhdl, reason);
    }
#endif

}

static void bt_stack_key_req(uint8_t conidx, uint8_t key_type, uint32_t key)
{
    ble_gap_key_cfm(conidx, 1, 123456);
}

static void bt_stack_bond_ind(uint8_t conidx, uint16_t status)
{
    if((conidx & ~GAP_ENCRYPT_REQ) < BLE_CONNECTION_MAX)
    {
        bt_stack_ble_bond_ind(conidx, status);
    }
#if BT_STACK_PRESENT
    else
    {
        bt_stack_classic_bond_ind(conidx, status);
    }
#endif
}
static void bt_stack_actv_start_ind(uint8_t type, uint8_t actv_id, int16_t status)
{
    switch(type)
    {
        case GAPM_ACTV_TYPE_ADV :
        case GAPM_ACTV_TYPE_SCAN :
        case GAPM_ACTV_TYPE_INIT :
        case GAPM_ACTV_TYPE_PER_SYNC :
            {
                CLOGD("bt actv start:%d,%d", type, actv_id);
            }
            break;
        default : break;
    }
}

static void bt_stack_actv_stop_ind(uint8_t type, uint8_t actv_id, int16_t status)
{
    switch(type)
    {
        case GAPM_ACTV_TYPE_ADV :
        case GAPM_ACTV_TYPE_SCAN :
        case GAPM_ACTV_TYPE_INIT :
        case GAPM_ACTV_TYPE_PER_SYNC :
            {
                CLOGD("bt actv stop:%d,%d", type, actv_id);
            }
            break;
        default : break;
    }
}

/*
 * GLOBAL FUNCTIONS
 ****************************************************************************************
 */

uint8_t bt_send_schedule_notify_isr(void)
{
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();
    
    if(stack_env->bt_notify_pending_num < BT_NOTIFY_PENDING_MAX)
    {
        btos_event_t ev;
        
        stack_env->bt_notify_pending_num++;
        ev.msg_body = &notify_msg;
        btos_send_event_isr(OS_TASK_ID_BT, &ev);
        
        //CLOGD("S:%d", stack_env->bt_notify_pending_num);
    }
    else
    {
        static uint16_t s_cnt = 0;
        if(s_cnt++ >= 200)
        {
            CLOGD("n o:%d!\n", stack_env->bt_notify_pending_num);
            s_cnt = 0;
        }
    }
    return 0;
}

uint8_t bt_send_schedule_notify(void)
{
    uint8_t status = 0;
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();
    
    if(stack_env->bt_notify_pending_num < BT_NOTIFY_PENDING_MAX)
    {
        btos_event_t ev;
        
        stack_env->bt_notify_pending_num++;
        ev.msg_body = &notify_msg;
        status = btos_send_event(OS_TASK_ID_BT, &ev, (uint32_t)BTOS_TASK_MAX_DELAY);
        
        //CLOGD("S:%d!\n", stack_env->bt_notify_pending_num);
    }
    else
    {
        static uint16_t s_cnt = 0;
        if(s_cnt++ >= 200)
        {
            CLOGD("n o:%d!", stack_env->bt_notify_pending_num);
            s_cnt = 0;
        }
        //CLOGD("s");
    }
    return status;
}

void bt_rcv_schedule_notify(void)
{
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();

    if(stack_env->bt_notify_pending_num == BT_NOTIFY_PENDING_MAX)
    {
        static uint16_t r_cnt = 0;
        if(r_cnt++ >= 800)
        {
            CLOGD("n r");
            r_cnt = 0;
        }
    }
    if(stack_env->bt_notify_pending_num > 0)
    {
        stack_env->bt_notify_pending_num--;
    }
    //CLOGD("R:%d!\n", stack_env->bt_notify_pending_num);
}
bt_stack_if_env_tag_t *bt_stack_if_get_env(void)
{
    return &bt_stack_env;
}
os_task_cb_t *bt_stack_if_get_cb(void)
{
    return (os_task_cb_t *)&bt_stack_os_cb;
}
void bt_stack_if_init(uint8_t type)
{
    CLOGD("bt if init:%d", type);
    bt_platform_init(0);

    memset(&bt_stack_env, 0x00, sizeof(bt_stack_if_env_tag_t));

    plf_set_config(&bt_stack_plf_cfg);
    ble_task_pre_init(&bt_stack_cb);
    lsip_isr_notify_reg(bt_send_schedule_notify_isr);
    ble_task_init();
    bt_send_schedule_notify();
}

void bt_stack_info_ind(uint8_t conidx, uint8_t type, ble_info_data_t *data)
{
    bt_stack_ble_info_ind(conidx, type, data);
}

uint8_t bt_stack_if_msg_handle(btos_event_t* msg)
{
    btos_event_t *event = msg;
    uint8_t msg_free = 1;
    
    if(event->msg_body)
    {
        switch(event->msg_body->msg_id)
        {
            case BT_OS_NOTIFY_EVT :
            {
                bt_rcv_schedule_notify();
                msg_free = 0;
            }break;
#if BT_AT_CMD_PRESENT
            case BT_OS_AT_SEND_EVT :
            {
                bt_at_cmd_msg_handle((bt_at_cmd_t*)event->msg_body->param);
            }break;
#endif
            case BT_OS_OPEN_EVT :
            {
            }break;
            case BT_OS_CLOSE_EVT :
            {
            }break;
            case BT_OS_INIT_EVT :
            {
                bt_stack_if_init(0);
            }break;
            case BT_OS_SCAN_START_EVT :
            {
                ble_scan_info_t *scan_info = (ble_scan_info_t *)event->msg_body->param;
                if (scan_info->scan_param_dft == 1)
                {
                    bt_stack_ble_scan_start(GAP_SCAN_ID_0, BLE_SCAN_TYPE, BLE_SCAN_PHY, BLE_SCAN_INTV, BLE_SCAN_WIN);
                }
                else
                {
                    bt_stack_ble_scan_start(GAP_SCAN_ID_0, scan_info->scan_type, scan_info->scan_phy, scan_info->scan_intv, scan_info->scan_win);
                }
            }break;
            case BT_OS_SCAN_STOP_EVT:
            {
                bt_stack_ble_scan_stop(GAP_ADV_ID_0);
            }break;
            case BT_OS_PER_SYNC_START_EVT :
            {
                gap_per_adv_bdaddr_t addr = {{0x0d, 0xe1, 0x0a, 0xe8, 0x07, 0xc0}, 0x00, 0x00};
                if(event->msg_body->param_len == 0)
                {
                    bt_stack_ble_pre_sync_start(GAPM_PER_SYNC_TYPE_GENERAL, &addr, \
                                                GAPM_REPORT_ADV_EN_BIT|GAPM_REPORT_BIGINFO_EN_BIT, 0x00, 100);
                }
                else
                {
                    uint8_t type = *event->msg_body->param;
                }
            }break;
            case BT_OS_PER_SYNC_STOP_EVT :
            {
                bt_stack_ble_pre_sync_stop();
            }break;
            case BT_OS_ADV_START_EVT :
            {
                ble_adv_info_t *adv_info = (ble_adv_info_t *)event->msg_body->param;
                app_ble_adv_start_handler(adv_info);
            }break;
            case BT_OS_ADV_STOP_EVT :
            {
                ble_adv_info_t *adv_info = (ble_adv_info_t *)event->msg_body->param;
                app_ble_adv_stop_handler(adv_info);
            }break;
            case BT_OS_CONNECT_EVT:
            {
                ble_conn_info_t *conn_info = (ble_conn_info_t *)event->msg_body->param;
                app_ble_conn_handler(conn_info);
            }break;
            case BT_OS_CONNECT_UPDATE_EVT:
            {
                ble_conn_update_info_t *con_upd_info = (ble_conn_update_info_t *)event->msg_body->param;
                bt_stack_ble_conn_update(con_upd_info->conhdl, con_upd_info->intv_min, con_upd_info->intv_max,con_upd_info->latency,con_upd_info->time_out);
            }break;
            case BT_OS_DISCONNECT_EVT:
            {
                ble_disconn_info_t *disconn_info = (ble_disconn_info_t *)event->msg_body->param;
                bt_stack_ble_disconnect(disconn_info->conidx, disconn_info->reason);
            }break;
            case BT_OS_HID_SEND_EVT :
            {
                ble_hogpd_info_t *hogpd_info = (ble_hogpd_info_t *)event->msg_body->param;
                app_ble_hogpd_hid_send_handler(hogpd_info);
            }break;
            case BT_OS_VOICE_DATA_SEND_EVT :
            {
                ble_hogpd_info_t *hogpd_info = (ble_hogpd_info_t *)event->msg_body->param;
                app_ble_voice_data_send_handler(hogpd_info);
            }break;
            case BT_OS_NET_CFG_SEND_EVT :
            {
                ble_net_cfg_info_t *netcfg_info = (ble_net_cfg_info_t *)event->msg_body->param;
                app_ble_netcfg_bles_send_notify_handler(netcfg_info);
            }break;
#if (BT_STACK_PRESENT)
            case BT_OS_BT_SCAN_EVT:
            {
                bt_scan_info_t *scan_info = (bt_scan_info_t *)event->msg_body->param;
                bt_stack_bt_scan(scan_info->scan_en);
            }break;
            case BT_OS_BT_INQ_START_EVT:
            {
                bt_inquiry_info_t *inquiry_info = (bt_inquiry_info_t *)event->msg_body->param;
                bt_stack_bt_inquiry(inquiry_info->disc_mode, inquiry_info->max_count);
            }break;
            case BT_OS_BT_INQ_STOP_EVT:
            {
                bt_stack_bt_inquiry_stop();
            }break;
            case BT_OS_BT_CONNECT_EVT:
            {
                bt_connect_info_t *connect_info = (bt_connect_info_t *)event->msg_body->param;
                bt_stack_bt_connect(connect_info->addr, connect_info->type, connect_info->clk_off, connect_info->page_scan_rep_mode);
            }break;
            case BT_OS_BT_CONNECT_CANCEL_EVT:
            {
                bt_stack_bt_connect_cancel();
            }break;
            case BT_OS_BT_DISCONNECT_EVT:
            {
                ble_disconn_info_t *disconnect_info = (ble_disconn_info_t *)event->msg_body->param;
                app_bt_disconnect(disconnect_info->conidx, disconnect_info->reason);
            }break;
            /*
             * A2DP
             */
            case BT_OS_A2DP_SEND_MEDIA_EVT:
            {
                bt_a2dp_send_media_info_t *info = (bt_a2dp_send_media_info_t *)event->msg_body->param;
                app_a2dp_send_media_to_peer(info->conidx, info->frame_num, info->len, info->data);
            } break;

            case BT_OS_A2DP_ENABLE_EVT:
            {
                bt_a2dp_enable_info_t *info = (bt_a2dp_enable_info_t *)event->msg_body->param;
                bt_a2dp_cfg_t cfg;
                cfg.a2dp_role = info->a2dp_role;
                bt_stack_a2dp_enable(&cfg);
            } break;

            case BT_OS_A2DP_CONNECT_EVT:
            {
                bt_a2dp_connect_info_t *info = (bt_a2dp_connect_info_t *)event->msg_body->param;
                app_a2dp_connect(info->conidx, info->role);
            } break;

            case BT_OS_A2DP_START_EVT:
            {
                bt_a2dp_start_info_t *info = (bt_a2dp_start_info_t *)event->msg_body->param;
                app_a2dp_start(info->conidx);
            } break;

#if BT_CALL_PRESENT
            /*
             * HFP
             */
            case BT_OS_HFP_ENABLE_EVT:
            {
                bt_hfp_enable_info_t *info = (bt_hfp_enable_info_t *)event->msg_body->param;
                bt_hfp_cfg_t cfg;
                cfg.hfp_role = info->hfp_role;
                cfg.hfp_feats = info->hfp_feats;
                bt_stack_hfp_enable(&cfg);
            } break;

            case BT_OS_HFP_CONNECT_EVT:
            {
                bt_hfp_connect_info_t *info = (bt_hfp_connect_info_t *)event->msg_body->param;
                app_hfp_connect(info->conidx, info->peer_role);
            } break;

            case BT_OS_HFP_SET_CODEC_TYPE_EVT:
            {
                bt_hfp_set_codec_info_t *info = (bt_hfp_set_codec_info_t *)event->msg_body->param;
                app_hfp_set_codec_type(info->conidx, info->codec_type);
            } break;

            case BT_OS_HFP_CALL_START_EVT:
            {
                bt_hfp_call_start_info_t *info = (bt_hfp_call_start_info_t *)event->msg_body->param;
                app_hfp_call_start(info->conidx, info->call_idx);
            } break;

            case BT_OS_HFP_CALL_ADD_AUDIO_EVT:
            {
                bt_hfp_call_add_audio_info_t *info = (bt_hfp_call_add_audio_info_t *)event->msg_body->param;
                app_hfp_call_add_audio(info->conidx, info->codec_type);
            } break;

            case BT_OS_HFP_CALL_REMOVE_AUDIO_EVT:
            {
                bt_hfp_call_remove_audio_info_t *info = (bt_hfp_call_remove_audio_info_t *)event->msg_body->param;
                app_hfp_call_remove_audio(info->conidx, info->reason);
            } break;

            case BT_OS_HFP_CALL_INCOMMING_EVT:
            {
                bt_hfp_call_incomming_info_t *info = (bt_hfp_call_incomming_info_t *)event->msg_body->param;
                app_hfp_call_incomming(info->conidx, info->idx);
            } break;

            case BT_OS_HFP_SEND_AUD_EVT:
            {
                bt_hfp_send_aud_info_t *info = (bt_hfp_send_aud_info_t *)event->msg_body->param;
                app_hfp_send_aud_to_peer(info->conidx, info->len, info->data);
            } break;
#endif // BT_CALL_PRESENT

            /*
             * AVRCP
             */
            case BT_OS_AVRCP_PLAY_STATUS_SET_EVT:
            {
                bt_avrcp_play_status_info_t *info = (bt_avrcp_play_status_info_t *)event->msg_body->param;
                app_avrcp_play_status_set(info->conidx, info->play_status);
            } break;

            /*
             * GAP
             */
            case BT_OS_GAP_AUTH_REQ_EVT:
            {
                bt_gap_auth_req_info_t *info = (bt_gap_auth_req_info_t *)event->msg_body->param;
                bt_gap_auth_req(info->conidx, info->sec_req);
            } break;

            case BT_OS_GAP_SAVE_LK_EVT:
            {
                bt_gap_save_lk_info_t *info = (bt_gap_save_lk_info_t *)event->msg_body->param;
                bt_gap_save_lk_mem_to_nvs(info->conidx);
            } break;

            case BT_OS_BT_SET_ASIC_CVSD_EVT:
            {
                bt_set_asic_cvsd_info_t *info = (bt_set_asic_cvsd_info_t *)event->msg_body->param;
                app_bt_set_asic_cvsd_en(info->enable);
            } break;

            /*
             * Audio HAL
             */
            case BT_OS_AUD_A2DP_SEND_START_EVT:
            {
                bt_aud_a2dp_send_start_info_t *info = (bt_aud_a2dp_send_start_info_t *)event->msg_body->param;
                bt_stack_a2dp_send_start(info->conidx, info->codec, info->ch, info->sample_rate);
            } break;

            case BT_OS_AUD_A2DP_SEND_STOP_EVT:
            {
                bt_aud_a2dp_send_stop_info_t *info = (bt_aud_a2dp_send_stop_info_t *)event->msg_body->param;
                bt_stack_a2dp_send_stop(info->conidx, info->status);
            } break;

            case BT_OS_AUD_A2DP_SEND_DATA_EVT:
            {
                bt_aud_a2dp_send_data_info_t *info = (bt_aud_a2dp_send_data_info_t *)event->msg_body->param;
                bt_stack_a2dp_send_data(info->conidx, info->frame_num, info->seq, info->len, info->data);
            } break;

#if BT_CALL_PRESENT
            case BT_OS_AUD_HFP_SEND_START_EVT:
            {
                bt_aud_hfp_send_start_info_t *info = (bt_aud_hfp_send_start_info_t *)event->msg_body->param;
                bt_stack_hfp_send_start(info->conidx, info->codec_type);
            } break;

            case BT_OS_AUD_HFP_SEND_STOP_EVT:
            {
                bt_aud_hfp_send_stop_info_t *info = (bt_aud_hfp_send_stop_info_t *)event->msg_body->param;
                bt_stack_hfp_send_stop(info->conidx, info->reason);
            } break;

            case BT_OS_AUD_HFP_SEND_DATA_EVT:
            {
                bt_aud_hfp_send_data_info_t *info = (bt_aud_hfp_send_data_info_t *)event->msg_body->param;
                bt_stack_hfp_send_data(info->conidx, info->pkt_sta, info->len, info->data);
            } break;
#endif // BT_CALL_PRESENT
            /*
             * BLE whitelist / resolve list
             */
            case BT_OS_BLE_ADD_WLIST_EVT:
            {
                bt_stack_ble_add_paired_to_wlist();
            } break;

            case BT_OS_BLE_ADD_RLIST_EVT:
            {
                ble_gap_add_paired_rpa_to_rlist();
            } break;
#endif
#if LEA_PRESENT
            case BT_OS_LEA_SCAN_EVT :
            {
            }break;
            case BT_OS_LEA_BMR_START_EVT :
            {
                if(event->msg_body->param_len == 0x06)
                {
                    bt_lea_bmr_start(event->msg_body->param);
                }
                else
                {
                    bt_lea_bmr_start(NULL);
                }
            }break;
            case BT_OS_LEA_BMR_STOP_EVT :
            {
                bt_lea_bmr_stop();
            }break;
            case BT_OS_LEA_UMR_START_EVT :
            {
                if(event->msg_body->param_len == 0x06)
                {
                    bt_lea_umr_start(event->msg_body->param);
                }
                else
                {
                    bt_lea_umr_start(NULL);
                }
            }break;
            case BT_OS_LEA_UMR_STOP_EVT :
            {
                bt_lea_umr_stop();
            }break;
#endif
#if BT_STACK_PRESENT
            case BT_OS_DATA_SEND_CNF_EVT :
            {
                bt_aud_pkt_info_t *pkt_info = (bt_aud_pkt_info_t *)event->msg_body->param;
                //CLOGD("bt free rcv data:0x%x", pkt_info->data);
                app_bt_data_free(pkt_info->data);
            }break;
#endif
            default:
            {
                CLOGD("bt if msg err,id:%d", event->msg_body->msg_id);
            }break;
        }
    }
    else
    {
        CLOGD("bt if body err %x", event->msg_body);
    }
    // check bt task.
    ble_task_execute();
    return msg_free;
}

uint8_t bt_stack_if_user_schedule(void)
{
    /// if need
    return 1;
}

#if (!BT_STACK_NVDS_SUPPORT)
uint8_t bt_stack_nvs_get(uint8_t param_id, uint8_t * lengthPtr, uint8_t *buf)
{
    return (NVDS_FAIL);
}
uint8_t bt_stack_nvs_set(uint8_t param_id, uint8_t length, uint8_t *buf)
{
    return (NVDS_FAIL);
}
uint8_t bt_stack_nvs_del(uint8_t param_id)
{
    return (NVDS_FAIL);
}
#else
uint8_t bt_stack_nvs_get(uint8_t param_id, uint8_t * lengthPtr, uint8_t *buf)
{
    uint32_t len = 0;
    uint8_t ret = 0;
    len = *lengthPtr;
    ret = nvds_get(param_id, (size_t *)&len, buf);
    *lengthPtr = len;
    //CLOGD("nvs_get,id:0x%x, ret:%d", param_id, ret);
    return ret;
}
uint8_t bt_stack_nvs_set(uint8_t param_id, uint8_t length, uint8_t *buf)
{
    uint8_t ret = 0;
    ret = nvds_put(param_id, length, buf);
    //CLOGD("nvs_set,id:0x%x, ret:%d", param_id, ret);
    return ret;
}
uint8_t bt_stack_nvs_del(uint8_t param_id)
{
    return nvds_del(param_id);
}
#endif // (!NVDS_SUPPORT)


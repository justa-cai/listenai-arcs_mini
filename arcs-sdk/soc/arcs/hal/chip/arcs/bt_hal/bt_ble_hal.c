/*
 * bt_ble_if.c
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

#include "bt_stack_hal.h"
#include "bt_ble_hal.h"
#include "bt_lea_hal.h"

#include "hogpd_msg.h"
#include "hogpd.h"
#include "bass.h"
#include "diss.h"

/*
 * LOCAL FUNCTIONS DECLARATION
 ****************************************************************************************
 */
extern uint16_t dis_profile_get_cb(uint8_t conidx, uint8_t att_idx, uint8_t *p_value, uint16_t max_len, uint16_t *ret_len);
extern uint8_t app_ble_adv_start(uint8_t adv_id, uint8_t adv_type);
extern uint8_t bt_stack_nvs_get(uint8_t param_id, uint8_t * lengthPtr, uint8_t *buf);
extern uint8_t bt_stack_nvs_del(uint8_t param_id);
extern void gapc_con_param_clear_peer_feat(uint8_t conidx);
extern uint8_t ble_gap_get_ltk_nocon(gap_addr_t * addr, uint8_t *p_ltk);
extern uint8_t app_hid_rcv_data(uint8_t conidx, uint16_t index, uint16_t length, uint16_t offset, uint8_t *data);

void bt_stack_ble_hid_rcv(uint8_t conidx, uint16_t index, uint16_t length, uint16_t offset, uint8_t *data);
static void bt_stack_ble_hid_send_cmp(uint32_t token, uint8_t val_id);
static void  bt_stack_ble_hid_read_cmp(uint32_t token, uint8_t val_id);
uint8_t *bt_stack_vbat_percent_get(void);
void bt_stack_ble_parameter_update_by_timer(uint32_t milli_seconds);
/*
 * LOCAL VARIABLES
 ****************************************************************************************
 */


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
bool bt_stack_ble_connected()
{
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();

    return stack_env->bt_ble_connected;
}

void bt_stack_ble_adv_start(ble_adv_cfg_t *adv_cfg)
{
    uint16_t uuid[2];
    gap_bdaddr_t peer = {0};
    ///set device uuid.
    uuid[0] = HID_UUID;
    CLOGD("bt_stack_ble_adv_start:%d", adv_cfg->adv_param.gen_adv.user_data_len);

    ble_gap_adv_prepare(adv_cfg->adv_id, adv_cfg->adv_type, adv_cfg->disc_mode, adv_cfg->flags, adv_cfg->adv_filter, adv_cfg->intv_min, adv_cfg->intv_max, peer, 0);
    
    if(adv_cfg->adv_param.gen_adv.user_data_len == 0)
    {
        ble_gap_adv_gen_data(adv_cfg->adv_id, 1, uuid);
    }
    else
    {
        ble_gap_adv_user_data(adv_cfg->adv_id, adv_cfg->adv_param.gen_adv.user_data_len, adv_cfg->adv_param.gen_adv.user_data);
    }
    ble_gap_adv_set_data(adv_cfg->adv_id, 0, NULL, adv_cfg->adv_param.gen_adv.rsp_data_len, adv_cfg->adv_param.gen_adv.rsp_data);
    ble_gap_adv_start(adv_cfg->adv_id);
}

void bt_stack_ble_adv_stop(uint8_t adv_id)
{
    ble_gap_adv_stop(adv_id);
}

void bt_stack_ble_connect(gap_bdaddr_t addr, uint8_t phy, uint16_t conn_intv_min, uint16_t conn_intv_max,
                    uint16_t latency, uint16_t super_to)
{
    ble_gap_connect(addr, phy, conn_intv_min, conn_intv_max, latency, super_to);
}

void bt_stack_ble_conn_update(uint8_t conidx, uint16_t conn_intv_min, uint16_t conn_intv_max, uint16_t latency, uint16_t super_to)
{
    CLOGI("ble conn update");

    ble_gap_connect_update(conidx, conn_intv_min, conn_intv_max, latency, super_to);
}

void bt_stack_ble_disconnect(uint8_t conidx, uint8_t reason)
{
    ble_gap_disconnect(conidx, reason);
}

void bt_stack_ble_scan_start(uint8_t scan_id, uint8_t type, uint8_t phy, uint16_t scan_intv, uint16_t scan_win)
{
    ble_gap_scan_prepare(scan_id);
    ble_gap_scan_start(scan_id, type, phy, scan_intv, scan_win, 0);
}

void bt_stack_ble_scan_stop(uint8_t scan_id)
{
    ble_gap_scan_stop(scan_id);
}

void bt_stack_ble_pre_sync_start(uint8_t type, gap_per_adv_bdaddr_t *adv_addr, uint8_t report_en, uint8_t past_conidx,uint16_t time_out)
{
    ble_gap_per_sync_start(type, adv_addr, report_en, past_conidx, time_out);
}

void bt_stack_ble_pre_sync_stop(void)
{
    ble_gap_per_sync_stop();
}

void bt_stack_ble_parameter_update(void)
{
    CLOGI("ble para upd");
    if(ble_gap_get_con_param_dis() != 0)
    {
        CLOGD("clear con param feat");
        gapc_con_param_clear_peer_feat(0);
    }
    ble_gap_connect_update(0, BLE_CON_INTERVAL_MIN, BLE_CON_INTERVAL_MAX, BLE_CON_LATENCY, BLE_CON_SUPERVISION_TIMEOUT);
}

void bt_stack_ble_parameter_update_cb(TimerHandle_t time_id)
{
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();

    btos_timer_cancel(time_id);

    if (!stack_env->bt_ble_connected) {
        return;
    }

    bt_stack_ble_parameter_update();
    ble_gap_entry_latency(GAP_EXIT_LATENCY_CONNECT);
#if BLE_VOICE_SIMULATOR
    bt_stack_ble_start_voice_dummy(100);
#endif
}

void bt_stack_ble_parameter_update_by_timer(uint32_t milli_seconds)
{
       TimerHandle_t update_id = btos_timer_creat(TIMER_TYPE_SINGLE, milli_seconds, bt_stack_ble_parameter_update_cb);
}

void bt_stack_ble_print_link_key(void)
{
    gap_bdaddr_t peer = {0};
    uint8_t ltk[32];
    uint8_t len = GAP_BD_ADDR_LEN;

    bt_stack_nvs_get(NVS_ID_PEER_ADDRESS, &len, peer.addr);
    CLOGD("peer: %02x:%02x:%02x:%02x:%02x:%02x",\
                peer.addr[5],peer.addr[4],peer.addr[3],\
                peer.addr[2],peer.addr[1],peer.addr[0]);
    uint8_t ret = ble_gap_get_ltk_nocon((gap_addr_t *)&peer, ltk);
    CLOGD("ltk:0x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x", \
                ltk[15], ltk[14], ltk[13], ltk[12],ltk[11], ltk[10], ltk[9], ltk[8], \
                ltk[7], ltk[6], ltk[5], ltk[4],ltk[3], ltk[2], ltk[1], ltk[0]);
}

void bt_stack_ble_add_paired_to_wlist(void)
{
    uint8_t i = 0;

    gap_bdaddr_t paired_peer[NVS_COUNT_LTK] = {0};
    gap_bdaddr_t *peer_addr = paired_peer;
    uint8_t num = ble_gap_get_paired_addr(paired_peer);
    ble_gap_get_dev_info(GAP_INFO_WHITE_LIST_SIZE);

    if(num > BLE_MAX_WLIST_NUM)
    {
        num = BLE_MAX_WLIST_NUM;
    }

    ble_gap_wl_list_set(num, paired_peer);
#if 0
    for(i = 0; i < num; i++)
    {
        CLOGD("PAIRED TYPE:%d, ADDR:0x%2x%2x%2x%2x%2x%2x",peer_addr->addr_type, peer_addr->addr[0], peer_addr->addr[1], peer_addr->addr[2], \
        peer_addr->addr[3], peer_addr->addr[4], peer_addr->addr[5]);
        peer_addr++;
    }
#endif
}

uint8_t *bt_stack_vbat_percent_get(void)
{
    static uint8_t vbat_percent = 80;
    return &vbat_percent;
}

uint8_t bt_stack_ble_adv_search_data(uint8_t length, uint8_t *p_data, uint8_t search_type, uint8_t *search_len, uint8_t **search_data)
{
    uint8_t status = 1;
    uint8_t *p_cursor = p_data;
    uint8_t *p_end_cursor = p_data + length;

    ///begain search every adv data.
    while ((p_cursor + 1) < p_end_cursor)
    {
        ///AD type
        uint8_t ad_type = *(p_cursor + 1);

        //CLOGD("dongle_adv_search_data,type:%d,%d,%d", ad_type, search_type, *p_cursor);
        if (ad_type == search_type)
        {
            ///get adv data len
            *search_len = *p_cursor;
            *search_data = p_cursor + 1;
            break;
        }
        /* Go to next advertising info */
        p_cursor += (*p_cursor + 1);
    }

    /// not find adv type that we hope.
    if (p_cursor == p_end_cursor)
    {
         status = 0;
    }

    return (status);
}


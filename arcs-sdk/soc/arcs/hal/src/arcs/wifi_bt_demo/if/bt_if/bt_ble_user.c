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

/// Message callback handle from APP
const hogpd_cb_t bt_stack_ble_hogpd_msg_cb =
{
    .cb_read_cmp = bt_stack_ble_hid_read_cmp,
    .cb_notify_cmp = bt_stack_ble_hid_send_cmp,
    .cb_write_cmp = NULL,

    .cb_read_ind = NULL,
    .cb_notify_ind = NULL,
    .cb_write_ind = bt_stack_ble_hid_rcv,
};
/// Message callback handle from APP
const diss_cb_t bt_stack_ble_diss_msg_cb =
{
    .cb_value_get = NULL,//dis_profile_get_cb,
};

static const uint8_t hid_report_map[] =
{
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x06,                    // USAGE (Keyboard)
    0xa1, 0x01,                    // COLLECTION (Application)

    0x85, HIDS_KB_REPORT_ID,       //   REPORT_ID (Keyboard)
    0x05, 0x07,                    //   USAGE_PAGE (Keyboard)
    0x19, 0x4b,                    //   USAGE_MINIMUM (Keyboard PageUp)
    0x29, 0x52,                    //   USAGE_MAXIMUM (Keyboard UpArrow)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x95, 0x08,                    //   REPORT_COUNT (8)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)

    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x81, 0x03,                    //   INPUT (Cnst,Var,Abs)

    0x95, 0x05,                    //   REPORT_COUNT (5)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x05, 0x08,                    //   USAGE_PAGE (LEDs)
    0x19, 0x01,                    //   USAGE_MINIMUM (Num Lock)
    0x29, 0x05,                    //   USAGE_MAXIMUM (Kana)
    0x91, 0x02,                    //   OUTPUT (Data,Var,Abs)

    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x75, 0x03,                    //   REPORT_SIZE (3)
    0x91, 0x03,                    //   OUTPUT (Cnst,Var,Abs)

    0x95, 0x6,                     //   REPORT_COUNT (6)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0xff,                    //   LOGICAL_MAXIMUM (101)
    0x05, 0x07,                    //   USAGE_PAGE (Keyboard)
    0x19, 0x00,                    //   USAGE_MINIMUM (Reserved (no event indicated))
    0x29, 0xff,                    //   USAGE_MAXIMUM (Keyboard Application)
    0x81, 0x00,                    //   INPUT (Data,Ary,Abs)
    0xc0,                          //   END_COLLECTION

    //mouse
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x02,                    // USAGE (Mouse)
    0xa1, 0x01,                    // COLLECTION (Application)
    
    0x85, HIDS_MOUSE_REPORT_ID,    //   REPORT_ID (Mouse)
    0x09, 0x01,                    //   USAGE_PAGE (Pointer)
    0xa1, 0x00,                    //   COLLECTION (PHYSICAL)
    0x05, 0x09,                    //   USAGE_PAGE (BUTTON)
    0x19, 0x01,                    //   USAGE_MINIMUM (1)
    0x29, 0x03,                    //   USAGE_MAXIMUM (5)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
    0x95, 0x05,                    //   REPORT_COUNT (5)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x75, 0x03,                    //   REPORT_SIZE (3)
    0x81, 0x01,                    //   INPUT (CONSTANT); 3 bit padding
    0x05, 0x01,                    //   USAGE_PAGE (Generic Desktop)
    0x09, 0x30,                    //   USAGE (X)
    0x09, 0x31,                    //   USAGE (Y)
    0x09, 0x38,                    //   USAGE (Wheel)
    0x15, 0x81,                    //   LOGICAL_MINIMUM (-127)
    0x25, 0x7f,                    //   LOGICAL_MAXIMUM (127)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x95, 0x03,                    //   REPORT_SIZE (3)
    0x81, 0x06,                    //   INPUT (Data,Var,Rel); 3 position bytes(X,Y,Wheel)
    0xc0,
    0xc0,
    //  media
    0x05, 0x0C,                     // USAGE_PAGE (Consumer Devices)
    0x09, 0x01,                     // USAGE (Consumer Control)
    0xA1, 0x01,                     // COLLECTION (Application)
    0x85, HIDS_MEDIA_REPORT_ID,     // REPORT_ID (3)
    0x19, 0x00,                     // USAGE_MINIMUM (0x00)
    0x2A, 0x9C, 0x02,               // USAGE_MAXIMUM (0x02 0xc9)
    0x15, 0x00,                     // LOGICAL_MINIMUM (0x00)
    0x26, 0x9C, 0x02,               // LOGICAL_MAXIMUM (0x02 0xc9)
    0x95, 0x01,                     // REPORT_COUNT (1)
    0x75, 0x10,                     // REPORT_SIZE (0x10)
    0x81, 0x00,                     //INPUT (Data,Ary,Abs)
    0xC0,                           //      END_COLLECTION
    //voice data report
    0x05 , 0x0C,                    //      Usage Page (Consumer Devices)
    0x09 , 0x01,                    //    Usage (Consumer Control)
    0xA1 , 0x01,                    //    Collection (Application)
    0x85 , HIDS_VOICE_DATA_IN_REPORT_ID,                               //    Report ID=0xFC
    0x95 , 0xff,                    //    REPORT_COUNT (20)
    0x75 , 0x08,                    //    REPORT_SIZE (8)
    0x15 , 0x00,                    //    LOGICAL_MINIMUM (0)
    0x26 , 0xFF , 0x00,             //    LOGICAL_MAXIMUM (255)
    0x81 , 0x00,                    //    INPUT (Data,Ary,Abs)
    0xC0,                           //      END_COLLECTION
    //gde ack in
    0x05 , 0x0C,                    //      Usage Page (Consumer Devices)
    0x09 , 0x01,                    //    Usage (Consumer Control)
    0xA1 , 0x01,                    //    Collection (Application)
    0x85 , HIDS_GDE_ACK_IN_REPORT_ID,//   Report ID=0xF8
    0x95 , 0xff,                    //    REPORT_COUNT (20)
    0x75 , 0x08,                    //    REPORT_SIZE (8)
    0x15 , 0x00,                    //    LOGICAL_MINIMUM (0)
    0x26 , 0xFF , 0x00,             //    LOGICAL_MAXIMUM (255)
    0x81 , 0x00,                    //    INPUT (Data,Ary,Abs)
    0xC0,                           //      END_COLLECTION

    //gde feedback
    0x05 , 0x0C,                    //      Usage Page (Consumer Devices)
    0x09 , 0x01,                    //    Usage (Consumer Control)
    0xA1 , 0x01,                    //    Collection (Application)
    0x85 , HIDS_GDE_FEEDBACK_IN_REPORT_ID,                             //    Report ID=0xF9
    0x95 , 0xff,                    //    REPORT_COUNT (1)
    0x75 , 0x08,                    //    REPORT_SIZE (8)
    0x15 , 0x00,                    //    LOGICAL_MINIMUM (0)
    0x26 , 0xFF , 0x00,             //    LOGICAL_MAXIMUM (255)
    0x81 , 0x00,                    //    INPUT (Data,Ary,Abs)
    0xC0,                           //      END_COLLECTION
};
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
 #if 0
uint8_t debug_adv_date[] = {0x02,0x01,0x05,
0x03,0x03,0x12,0x18,0x05,0xff,0x66,0x79,0x30,0x02,0x03,0x19,0xc1,0x03,0x06,0x08,0x32,0x33,0x34,0x35,0x36};
#endif
void bt_stack_ble_enable_cmp(uint16_t status)
{
    uint16_t flags = 0;
    uint16_t uuid[2];
    uint8_t len = GAP_BD_ADDR_LEN;

    //uuid[0] = HID_UUID;
    // start adv
    gap_bdaddr_t peer = {0};

    CLOGD("ble enable cmp, sta:%d", status);
    /// debug: print memory info
    //struct plf_mem_info info;
    //plf_get_mem_info(&info);
    //CLOGD("memory info:");
    //CLOGD("Stack free=%d, heap_free=%d, noret_free=%d", info.stack_free, info.heap_free, info.non_ret_free);

    /// profile init
    ble_diss_init((diss_cb_t *)&bt_stack_ble_diss_msg_cb);
    //enable bass service
    ble_bass_init();
    ble_bass_enable(0, bt_stack_vbat_percent_get());
    // enable hid service
    uint8_t svc_features = HOGPD_CFG_KEYBOARD | HOGPD_CFG_MOUSE | HOGPD_CFG_PROTO_MODE | HOGPD_CFG_REPORT_NTF_EN;
    uint8_t report_char_cfg = HOGPD_CFG_REPORT_IN;
    hogpd_report_map_t report_map = {sizeof(hid_report_map), 0, (uint8_t *)hid_report_map};
    ble_hogpd_init(svc_features, report_char_cfg, (hogpd_cb_t*)&bt_stack_ble_hogpd_msg_cb, &report_map);
    ble_hogpd_enable(0);

#if OTA!=0
    otas_init(0, 0);
#endif

#if BLE_PEER_FEAT_CON_PARAM_DIS
    ble_gap_set_con_param_dis(1);
#endif

    ///start adv
    //app_ble_adv_start(0, BLE_ADV_GEN);
}

void bt_stack_ble_conn_ind(uint8_t conidx, uint16_t conhdl, gap_bdaddr_t *peer_addr)
{
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();

    stack_env->bt_ble_connected = 1;
    CLOGD("ble connected!");
    /// save last device address
    bt_stack_nvs_set(NVS_ID_PEER_ADDRESS, GAP_BD_ADDR_LEN, peer_addr->addr);

    // some bt dev send connected ind that have connected param,will impact smp and gatt,so exit latency until param update.
    ble_gap_exit_latency(GAP_EXIT_LATENCY_CONNECT);

    /// get remote feature
    ble_gap_get_con_info(conidx, GAP_INFO_FETURES);
}

void bt_stack_ble_disc_ind(uint8_t conidx, uint16_t conhdl, uint16_t reason)
{
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();
    gap_bdaddr_t peer = {0};
    uint8_t len = GAP_BD_ADDR_LEN;

    stack_env->bt_ble_connected = 0;
    stack_env->bt_ble_encryption = 0;

    bt_stack_nvs_get(NVS_ID_PEER_ADDRESS, &len, peer.addr);

    CLOGI("dis addr: 0x%x%x%x%x%x%x", peer.addr[0], peer.addr[1], peer.addr[2], \
            peer.addr[3], peer.addr[4], peer.addr[5]);
    CLOGI("dis reason: %d", reason);
    /// profile reinit
    hogpd_report_map_t report_map = {sizeof(hid_report_map), 0, (uint8_t *)hid_report_map};
    hogpd_init_report_map(1, &report_map);
    /// clear all exit latency
    ble_gap_entry_latency(GAP_EXIT_LATENCY_ALL);
    ///clear hid count
    stack_env->bt_hid_send_cnt = 0;
}

void bt_stack_ble_key_req(uint8_t conidx, uint8_t key_type, uint32_t key)
{
    ble_gap_key_cfm(conidx, 1, 123456);
}

void bt_stack_ble_bond_ind(uint8_t conidx, uint16_t status)
{
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();
    uint8_t info = status >> 8;
    uint8_t value = status & 0xff;
    CLOGI("bond info: %d, value: %d", info, value);
    switch(info)
    {
        case GAP_PAIRING_SUCCEED :
            {
#if WHITE_LIST_ADD
                bt_stack_ble_add_paired_to_wlist();
#endif
#if RESOVLE_LIST_ADD
                ble_gap_add_paired_rpa_to_rlist();
#endif
                stack_env->bt_ble_encryption = 1;
            }
            break;
        case GAP_PAIRING_FAILED :
            {
                ble_gap_delete_bond(NULL);  //clear all
                bt_stack_nvs_del(NVS_ID_PEER_ADDRESS);
                CLOGI("bond error,user dis: 0x18");
                ble_gap_disconnect(0, 0x18);
            }
            break;
        case GAP_IRK_EXCH :
        case GAP_LTK_EXCH :
            {
                CLOGI("ltk save suc");
            }
            break;
        case GAP_LINK_ENCRYPTED :
            {
                //ble_gap_mtu_exch(conidx, 0);
                if(BLE_CON_PHY & GAP_PHY_LE_2MBPS)
                {
                    ble_gap_get_con_info(conidx, GAP_INFO_FETURES);
                }
            }
            break;
        case GAP_LINK_ENCRYPT_REQ :
            {
                //CLOGI("encrtpt request: %d", value);
                if(0 == value) 
                {
                    stack_env->bt_ble_encryption = 1;
                }
            }
            break;
        default : break;
    }

    if(stack_env->bt_ble_encryption == 1)
    {
        bt_stack_ble_parameter_update_by_timer(6000);
    }
}


void bt_stack_ble_para_update_ind(uint8_t conidx, uint16_t interval, uint16_t latency, uint16_t super_to)
{
    CLOGI("interval: %d; latency: %d; super: %d", interval, latency, super_to);
}

void bt_stack_ble_info_ind(uint8_t conidx, uint8_t type, ble_info_data_t *data)
{
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();

    if(type == GAP_INFO_FETURES)
    {
        stack_env->bt_ble_peer_info.phy_2m = data->features[1] & 0x01;
        if((stack_env->bt_ble_peer_info.phy_2m) && (BLE_CON_PHY != GAP_PHY_LE_1MBPS))
        {
            ble_gap_get_con_info(conidx, GAP_INFO_VERSION);
        }
    }
    else if(type == GAP_INFO_VERSION)
    {
        stack_env->bt_ble_peer_info.bt_core = data->lmp_version;
        stack_env->bt_ble_peer_info.comp_id = data->compid;

        // Check 2M
        // set connect phy, support 2m and bt core later than 0x0a(core 5.1);
        if(stack_env->bt_ble_peer_info.phy_2m && (BLE_CON_PHY & GAP_PHY_LE_2MBPS) && (stack_env->bt_ble_peer_info.bt_core >= 0x0a))
        {
            CLOGI("2M");
            ble_gap_set_phy(conidx, BLE_CON_PHY, BLE_CON_PHY, 0);
        }
    }
}

void bt_stack_ble_adv_report_ind(uint8_t flag, gap_bdaddr_t *peer_addr, int8_t rssi, uint8_t len, uint8_t *data)
{
    CLOGD("adv report ind, flag :%d, addr:%2x, %2x, %2x, %2x", flag, peer_addr->addr[0], peer_addr->addr[1], \
                                                                        peer_addr->addr[2], peer_addr->addr[3]);
}

uint8_t bt_stack_ble_hid_send(uint8_t conidx, uint8_t report_idx, uint8_t length, uint8_t* value)
{
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();
    uint8_t status = 0;
    uint16_t max_send_cnt = BT_STACK_BLE_HOGPD_HID_MAX_COUNT;

    //CLOGD("hid idx:%d, len:%d", report_idx, length);
    if(stack_env->bt_ble_connected == 1)
    {
        /// remain 5 pkt for ctrl & cmd.
        if(report_idx == HIDS_VOICE_DATA_INDEX)
        {
            max_send_cnt = BT_STACK_BLE_HOGPD_HID_MAX_COUNT - 5;
        }
        else
        {
            max_send_cnt = BT_STACK_BLE_HOGPD_HID_MAX_COUNT;
        }

        if (stack_env->bt_hid_send_cnt < max_send_cnt) 
        {
            status = ble_hogpd_report_upd(conidx, report_idx, length, value);
            if(status)
            {
                CLOGW("hid err status:%d", status);
            } 
            stack_env->bt_hid_send_cnt++;
        } 
        else 
        {
            status = 0x01;
            CLOGW("OverFlow");
        }
        return status;
    }
    else
    {
        status = 0x02;
        CLOGW("bt dis");
    }
    return status;
}

static void bt_stack_ble_hid_send_cmp(uint32_t token, uint8_t val_id)
{
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();

    if(stack_env->bt_hid_send_cnt != 0)
    {
        stack_env->bt_hid_send_cnt--;
    }

    //if(stack_env->bt_hid_send_cnt == 0)
    //{
    //    CLOGD("hid cmp:%d", stack_env->bt_hid_send_cnt);
    //}
}

static void  bt_stack_ble_hid_read_cmp(uint32_t token, uint8_t val_id)
{
    ///to do;
}

void bt_stack_ble_hid_rcv(uint8_t conidx, uint16_t index, uint16_t length, uint16_t offset, uint8_t *data)
{
    /// send hid data to application to handle.
    //app_hid_rcv_data(conidx, index, length, offset, data);
}

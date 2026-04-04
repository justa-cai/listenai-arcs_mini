/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
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
//#include "bt_lea_if.h"
#include "bt_app_if.h"

#include "hogpd_msg.h"
#include "hogpd.h"
#include "bass.h"
#include "diss.h"
#include "netcfg_bles.h"
#include "lisa_log.h"
#define TAG "bt_ble_if"

/*
 * LOCAL FUNCTIONS DECLARATION
 ****************************************************************************************
 */
extern uint16_t dis_profile_get_cb(uint8_t conidx, uint8_t att_idx, uint8_t *p_value, uint16_t max_len, uint16_t *ret_len);
extern uint8_t app_ble_adv_start(uint8_t adv_id, uint8_t adv_type);
extern uint8_t bt_stack_nvs_get(uint8_t param_id, uint8_t * lengthPtr, uint8_t *buf);
extern uint8_t bt_stack_nvs_set(uint8_t param_id, uint8_t length, uint8_t *buf);
extern uint8_t bt_stack_nvs_del(uint8_t param_id);

// 自定义 NVS ID 用于存储 Peer IRK (使用应用特定槽位)
#define NVS_ID_PEER_IRK_BASE    0xA0   // 起始槽位
#define NVS_ID_PEER_IRK_MAX     8      // 最多8个设备
extern void gapc_con_param_clear_peer_feat(uint8_t conidx);
extern uint8_t ble_gap_get_ltk_nocon(gap_addr_t * addr, uint8_t *p_ltk);
extern uint8_t app_hid_rcv_data(uint8_t conidx, uint16_t index, uint16_t length, uint16_t offset, uint8_t *data);
extern uint16_t netcfg_bles_profile_set_cb(uint8_t conidx, uint8_t att_idx, uint16_t op, uint8_t *p_value);

// RPA 解析列表相关函数声明 (从预编译库反编译分析得出)
extern uint8_t ble_gap_get_paired_ral_info(void *ral_list, uint8_t max_count);
extern void ble_gap_ral_list_set(uint8_t count, void *ral_list);

// RAL 设备结构 (40 字节) - 基于反编译分析
typedef struct {
    uint8_t addr[6];          // 6 字节 - BD Address
    uint8_t addr_type;        // 1 字节 - 地址类型 (0=public, 1=random)
    uint8_t peer_irk[16];     // 16 字节 - Peer IRK
    uint8_t local_irk[16];    // 16 字节 - Local IRK
    uint8_t privacy_mode;     // 1 字节 - 隐私模式
} gap_ral_dev_t;

void bt_stack_ble_hid_rcv(uint8_t conidx, uint16_t index, uint16_t length, uint16_t offset, uint8_t *data);
static void bt_stack_ble_add_paired_to_ralist(void);
static void bt_stack_ble_hid_send_cmp(uint32_t token, uint8_t val_id);
static void  bt_stack_ble_hid_read_cmp(uint32_t token, uint8_t val_id);
uint8_t *bt_stack_vbat_percent_get(void);
void bt_stack_ble_parameter_update_by_timer(uint32_t milli_seconds);

#if (BT_EMB_PRESENT)
#if (BT_STACK_PRESENT)
extern void bt_classic_scan_enable(uint8_t enable);
extern void bt_gap_discover_create(uint8_t own_addr_type);
extern void bt_gap_discover_start(uint8_t actv_idx, uint8_t disc_mode, uint8_t max_count, bool get_name);
extern void bt_gap_connect(gap_bdaddr_t addr, uint8_t type, uint16_t clk_off, uint8_t page_scan_rep_mode);
#endif
#endif

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
static const netcfg_bles_cb_t netcfg_app_cb =
{
    .cb_value_set = netcfg_bles_profile_set_cb,
};

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

__attribute__((weak)) void app_ble_init_cmp(void)
{
}

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

    LISA_LOGI(TAG, "ble enable cmp, sta:%d", status);
    /// debug: print memory info
    //struct plf_mem_info info;
    //plf_get_mem_info(&info);
    //CLOGD("memory info:");
    //CLOGD("Stack free=%d, heap_free=%d, noret_free=%d", info.stack_free, info.heap_free, info.non_ret_free);
    /// profile init
    app_ble_init_cmp();
#if 0
    //enable bass service
    ble_bass_init();
    ble_bass_enable(0, bt_stack_vbat_percent_get());
    //enable net config.
    ble_netcfg_bles_init((netcfg_bles_cb_t *)&netcfg_app_cb);
#if 0
    //enable diss.
    ble_diss_init((diss_cb_t *)&bt_stack_ble_diss_msg_cb);
#endif

#if BLE_VOICE_SIMULATOR
    // enable hid service
    uint8_t svc_features = HOGPD_CFG_KEYBOARD | HOGPD_CFG_MOUSE | HOGPD_CFG_PROTO_MODE | HOGPD_CFG_REPORT_NTF_EN;
    uint8_t report_char_cfg = HOGPD_CFG_REPORT_IN;
    hogpd_report_map_t report_map = {sizeof(hid_report_map), 0, (uint8_t *)hid_report_map};
    ble_hogpd_init(svc_features, report_char_cfg, (hogpd_cb_t*)&bt_stack_ble_hogpd_msg_cb, &report_map);
    ble_hogpd_enable(0);
#endif

#if OTA!=0
    otas_init(0, 0);
#endif
#endif
#if BLE_PEER_FEAT_CON_PARAM_DIS
    ble_gap_set_con_param_dis(1);
#endif

#if 0
    ///start adv
    app_ble_adv_start(0, BLE_ADV_GEN);
#endif
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

#if BLE_VOICE_SIMULATOR
    bt_stack_ble_parameter_update_by_timer(6000);
#endif
}

void bt_stack_ble_disc_ind(uint8_t conidx, uint16_t conhdl, uint16_t reason)
{
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();
    gap_bdaddr_t peer = {0};
    uint8_t len = GAP_BD_ADDR_LEN;

    stack_env->bt_ble_connected = 0;
    stack_env->bt_ble_encryption = 0;

    bt_stack_nvs_get(NVS_ID_PEER_ADDRESS, &len, peer.addr);

    LISA_LOGI(TAG, "disconnect device addr: %02X:%02X:%02X:%02X:%02X:%02X", peer.addr[0], peer.addr[1], peer.addr[2],
            peer.addr[3], peer.addr[4], peer.addr[5]);
    LISA_LOGI(TAG, "disconnect reason: %d", reason);
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

    LISA_LOGI(TAG, "bond status: %d, condix: %d", status, (conidx & GAP_ENCRYPT_REQ));
    #if 1
    if(status == 10) // Link encrypted
    {
        //ble_gap_mtu_exch(conidx, 0);
        if(BLE_CON_PHY & GAP_PHY_LE_2MBPS)
        {
            ble_gap_get_con_info(conidx, GAP_INFO_FETURES);
        }
    }
    else
    {
        /// encrtpt request
        if(conidx & GAP_ENCRYPT_REQ)
        {
            LISA_LOGI(TAG, "encrtpt request: %d", status);
            if(1 == status)
            {
                // 加密请求失败 - LTK 不匹配或 RPA 无法解析
                // 尝试触发重新配对流程
                LISA_LOGW(TAG, "Encryption request failed, triggering re-pair");
                ble_gap_auth_req(conidx & 0x7F, GAP_SEC_UNAUTH);
            }
            else if(0 == status)
            {
                stack_env->bt_ble_encryption = 1;
            }
        }
        else/// bond indicate
        {
            if(0 == status)
            {
#if WHITE_LIST_ADD
                bt_stack_ble_add_paired_to_wlist();
#endif
#if RESOVLE_LIST_ADD
                LISA_LOGI(TAG, "Pairing success, updating RAL");
                // 获取对端 BD 地址信息
                ble_gap_get_con_info(conidx & 0x7F, GAP_INFO_BDADDR);
                // 尝试使用显式 RAL API
                bt_stack_ble_add_paired_to_ralist();
                // 也调用原有的函数
                ble_gap_add_paired_rpa_to_rlist();
                // 查询 RAL 状态
                ble_gap_get_dev_info(GAP_INFO_RAL_LIST_SIZE);
#endif
                stack_env->bt_ble_encryption = 1;
            }
            else if(1 == status)
            {
                // 配对请求 - 需要显式触发认证流程
                LISA_LOGI(TAG, "Pairing request (status=1, condix=0), triggering auth");
                // 提取实际连接索引（bits 0-6）并触发配对
                // GAP_SEC_UNAUTH = 1 (无认证配对，Just Works 模式)
                ble_gap_auth_req(conidx & 0x7F, GAP_SEC_UNAUTH);
            }
        }

        if(0 == status)
        {
            bt_stack_ble_parameter_update_by_timer(6000);
        }

    }
    #endif
}

void bt_stack_ble_para_update_ind(uint8_t conidx, uint16_t interval, uint16_t latency, uint16_t super_to)
{
    LISA_LOGI(TAG, "interval: %d; latency: %d; super: %d", interval, latency, super_to);
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
#if RESOVLE_LIST_ADD
    else if(type == GAP_INFO_RAL_LIST_SIZE)
    {
        LISA_LOGI(TAG, "RAL list size: operation=%d, size=%d", data->operation, data->size);
    }
    else if(type == GAP_INFO_WHITE_LIST_SIZE)
    {
        LISA_LOGI(TAG, "White list size: operation=%d, size=%d", data->operation, data->size);
    }
    else if(type == GAP_INFO_BDADDR)
    {
        // 获取对端地址 - 这可能是 RPA 或身份地址
        LISA_LOGI(TAG, "Peer BD addr: %02X:%02X:%02X:%02X:%02X:%02X, type=%d",
            data->addr.addr[5], data->addr.addr[4], data->addr.addr[3],
            data->addr.addr[2], data->addr.addr[1], data->addr.addr[0],
            data->addr.addr_type);
        // 保存对端地址到 NVS（覆盖之前的 RPA）
        bt_stack_nvs_set(NVS_ID_PEER_ADDRESS, GAP_BD_ADDR_LEN, data->addr.addr);
    }
#endif
}

void bt_stack_ble_adv_report_ind(uint8_t flag, gap_bdaddr_t *peer_addr, int8_t rssi, uint8_t len, uint8_t *data)
{
    LISA_LOGI(TAG, "adv report ind, flag :%d, addr:%2x, %2x, %2x, %2x", flag, peer_addr->addr[0], peer_addr->addr[1],
        peer_addr->addr[2], peer_addr->addr[3]);
}
bool bt_stack_ble_connected()
{
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();

    return stack_env->bt_ble_connected;
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

void bt_stack_ble_adv_start(ble_adv_cfg_t *adv_cfg)
{
    uint16_t uuid[2];
    gap_bdaddr_t peer = {0};
    ///set device uuid.
    uuid[0] = HID_UUID;
    LISA_LOGI(TAG, "bt_stack_ble_adv_start:%d", adv_cfg->adv_param.gen_adv.user_data_len);

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

void bt_stack_bt_scan(uint8_t scan_en)
{
    bt_classic_scan_enable(scan_en);
}

void bt_stack_bt_inquiry(uint8_t disc_mode, uint8_t max_count)
{
    bt_gap_discover_create(0);
    bt_gap_discover_start(0, 0, max_count, false);
}

void bt_stack_bt_connect(gap_bdaddr_t addr, uint8_t type, uint16_t clk_off, uint8_t page_scan_rep_mode)
{
    bt_gap_connect(addr, type, clk_off, page_scan_rep_mode);
}

void bt_stack_ble_scan_start(uint8_t scan_id, uint8_t type, uint8_t phy, uint16_t scan_intv, uint16_t scan_win)
{
    ble_gap_scan_prepare(scan_id);
    ble_gap_scan_start(scan_id, type, phy, scan_intv, scan_win);
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
    LISA_LOGI(TAG, "ble para update");
    if(ble_gap_get_con_param_dis() != 0)
    {
        LISA_LOGI(TAG, "clear connect param failed");
        gapc_con_param_clear_peer_feat(0);
    }
    ble_gap_connect_update(0, BLE_CON_INTERVAL_MIN, BLE_CON_INTERVAL_MAX, BLE_CON_LATENCY, BLE_CON_SUPERVISION_TIMEOUT);
}

void bt_stack_ble_parameter_update_cb(TimerHandle_t time_id)
{
    LISA_LOGI(TAG, "ble para update by timer");
    bt_stack_ble_parameter_update();
    ble_gap_entry_latency(GAP_EXIT_LATENCY_CONNECT);
    btos_timer_cancel(time_id);
#if BLE_VOICE_SIMULATOR
    bt_voice_send_by_timer_start(100);
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
    LISA_LOGI(TAG, "peer: %02x:%02x:%02x:%02x:%02x:%02x",\
                peer.addr[5],peer.addr[4],peer.addr[3],\
                peer.addr[2],peer.addr[1],peer.addr[0]);
    uint8_t ret = ble_gap_get_ltk_nocon((gap_addr_t *)&peer, ltk);
    LISA_LOGI(TAG, "ltk:0x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x", \
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
        LISA_LOGI(TAG, "PAIRED TYPE:%d, ADDR:0x%2x%2x%2x%2x%2x%2x",peer_addr->addr_type, peer_addr->addr[0], peer_addr->addr[1], peer_addr->addr[2], \
        peer_addr->addr[3], peer_addr->addr[4], peer_addr->addr[5]);
        peer_addr++;
    }
#endif
}

#if RESOVLE_LIST_ADD

// 读取并显示存储在自定义 NVS 槽位中的 Peer IRK
// 用于验证数据稳定性和调试
static void bt_stack_read_stored_peer_irk(void)
{
    LISA_LOGI(TAG, "Reading stored Peer IRK from custom NVS slots");

    for (uint8_t i = 0; i < NVS_ID_PEER_IRK_MAX; i++) {
        uint8_t nvs_id = NVS_ID_PEER_IRK_BASE + i;
        uint8_t peer_irk_data[23];  // 6字节地址 + 1字节类型 + 16字节IRK = 23字节
        uint8_t len = sizeof(peer_irk_data);

        if (bt_stack_nvs_get(nvs_id, &len, peer_irk_data) == 0 && len == sizeof(peer_irk_data)) {
            uint8_t *addr = &peer_irk_data[0];
            uint8_t addr_type = peer_irk_data[6];
            uint8_t *irk = &peer_irk_data[7];

            // 检查 IRK 是否全零
            bool has_data = false;
            for (uint8_t j = 0; j < 16; j++) {
                if (irk[j] != 0) { has_data = true; break; }
            }

            LISA_LOGI(TAG, "  NVS[0x%02X]: addr=%02X:%02X:%02X:%02X:%02X:%02X, type=%d, IRK=%02X%02X%02X%02X...%02X%02X%02X%02X %s",
                nvs_id,
                addr[5], addr[4], addr[3], addr[2], addr[1], addr[0],
                addr_type,
                irk[0], irk[1], irk[2], irk[3],
                irk[12], irk[13], irk[14], irk[15],
                has_data ? "(valid)" : "(zero)");
        }
    }
}

// 使用 RAL API 手动设置解析列表
static void bt_stack_ble_add_paired_to_ralist(void)
{
    // 首先读取之前存储的 Peer IRK，用于对比
    bt_stack_read_stored_peer_irk();

    gap_bdaddr_t paired_addr[NVS_COUNT_LTK] = {0};
    gap_ral_dev_t ral_devs[NVS_COUNT_LTK] = {0};
    uint8_t paired_num = 0;
    uint8_t local_irk[16] = {0};
    uint8_t irk_len = 16;

    LISA_LOGI(TAG, "Getting paired device addresses and RAL info from NVS");

    // 获取配对设备的地址
    paired_num = ble_gap_get_paired_addr(paired_addr);
    LISA_LOGI(TAG, "Got %d paired devices", paired_num);

    // 打印配对设备地址
    for (uint8_t i = 0; i < paired_num; i++) {
        LISA_LOGI(TAG, "Paired[%d]: addr=%02X:%02X:%02X:%02X:%02X:%02X, type=%d",
            i,
            paired_addr[i].addr[5], paired_addr[i].addr[4], paired_addr[i].addr[3],
            paired_addr[i].addr[2], paired_addr[i].addr[1], paired_addr[i].addr[0],
            paired_addr[i].addr_type);
    }

    // 获取本地 IRK
    bt_stack_nvs_get(NVS_ID_LOC_IRK, &irk_len, local_irk);

    // ✅ 新方案：直接从 LTK 数据中提取 Peer IRK！
    // LTK 结构 (36 bytes):
    //   Offset 0-5:   BD Address (reversed)
    //   Offset 6:     Address type/flags
    //   Offset 7-22:  LTK (16 bytes)
    //   Offset 23-30: RAND or Peer IRK part 1?
    //   Offset 31-32: EDIV
    //   Offset 33-35: Padding/Peer IRK part 2?
    //
    // 实际发现：bytes 20-35 看起来像是 Peer IRK！
    // 让我们尝试从不同的偏移量提取 Peer IRK

    uint8_t extracted_peer_irk[NVS_COUNT_LTK][16] = {0};
    bool has_valid_peer_irk[NVS_COUNT_LTK] = {false};

    for (uint8_t i = 0; i < paired_num && i < 3; i++) {
        uint8_t ltk_data[36] = {0};
        uint8_t ltk_len = 36;
        uint8_t nvs_id = NVS_ID_LTK_FIRST + i;

        if (bt_stack_nvs_get(nvs_id, &ltk_len, ltk_data) == 0) {
            LISA_LOGI(TAG, "LTK[%d] (NVS ID=0x%02X) raw dump:", i, nvs_id);
            // 完整打印 36 字节，分析结构
            LISA_LOGI(TAG, "  %02X %02X %02X %02X %02X %02X | %02X | %02X %02X %02X %02X %02X %02X %02X %02X",
                ltk_data[0], ltk_data[1], ltk_data[2], ltk_data[3], ltk_data[4], ltk_data[5],
                ltk_data[6],
                ltk_data[7], ltk_data[8], ltk_data[9], ltk_data[10], ltk_data[11], ltk_data[12], ltk_data[13], ltk_data[14]);
            LISA_LOGI(TAG, "  %02X %02X %02X %02X %02X %02X %02X %02X | %02X %02X %02X %02X | %02X %02X %02X %02X | %02X %02X %02X",
                ltk_data[15], ltk_data[16], ltk_data[17], ltk_data[18], ltk_data[19], ltk_data[20], ltk_data[21], ltk_data[22],
                ltk_data[23], ltk_data[24], ltk_data[25], ltk_data[26],
                ltk_data[27], ltk_data[28], ltk_data[29], ltk_data[30],
                ltk_data[31], ltk_data[32], ltk_data[33], ltk_data[34], ltk_data[35]);
            LISA_LOGI(TAG, "  Off: 0-5=Addr(rev), 6=Type, 7-22=LTK?, 23-26=?, 27-30=?, 31-34=?, 35=?");

            // 地址是否为 RPA？检查前2位的高位比特
            // RPA: top 2 bits = 0x40 (0100xxxx)
            bool is_rpa = (ltk_data[5] & 0xC0) == 0x40;
            LISA_LOGI(TAG, "  Address type: %s (byte[5]=0x%02X)", is_rpa ? "RPA" : "Static/Public", ltk_data[5]);

            // 尝试多种 Peer IRK 位置
            // 位置 1: bytes 20-35 (16字节)
            memcpy(extracted_peer_irk[i], &ltk_data[20], 16);
            bool irk_valid_1 = false;
            for (uint8_t j = 0; j < 16; j++) {
                if (extracted_peer_irk[i][j] != 0) { irk_valid_1 = true; break; }
            }

            // 位置 2: bytes 23-35 (13字节) - 不够16字节
            // 位置 3: 检查 bytes 7-22 (LTK) 的熵值

            has_valid_peer_irk[i] = irk_valid_1;

            LISA_LOGI(TAG, "  Peer IRK candidate @ 20-35: %02X%02X%02X%02X...%02X%02X%02X%02X (valid:%d)",
                extracted_peer_irk[i][0], extracted_peer_irk[i][1], extracted_peer_irk[i][2], extracted_peer_irk[i][3],
                extracted_peer_irk[i][12], extracted_peer_irk[i][13], extracted_peer_irk[i][14], extracted_peer_irk[i][15],
                irk_valid_1);

            // 🔥 与之前存储的 Peer IRK 进行对比，验证稳定性
            uint8_t nvs_id = NVS_ID_PEER_IRK_BASE + i;
            uint8_t stored_data[23];  // 6字节地址 + 1字节类型 + 16字节IRK = 23字节
            uint8_t stored_len = sizeof(stored_data);
            if (bt_stack_nvs_get(nvs_id, &stored_len, stored_data) == 0 && stored_len == sizeof(stored_data)) {
                uint8_t *stored_irk = &stored_data[7];
                bool match = true;
                for (uint8_t j = 0; j < 16; j++) {
                    if (extracted_peer_irk[i][j] != stored_irk[j]) {
                        match = false;
                        break;
                    }
                }
                if (match) {
                    LISA_LOGI(TAG, "  ✅ Peer IRK matches stored value (stable across reboots)");
                } else {
                    LISA_LOGW(TAG, "  ⚠️ Peer IRK differs from stored value (data changed!)");
                    LISA_LOGI(TAG, "     Stored: %02X%02X%02X%02X...%02X%02X%02X%02X",
                        stored_irk[0], stored_irk[1], stored_irk[2], stored_irk[3],
                        stored_irk[12], stored_irk[13], stored_irk[14], stored_irk[15]);
                }
            } else {
                LISA_LOGI(TAG, "  No previously stored Peer IRK found (first pairing?)");
            }
        }
    }

    // 🔥 存储提取的 Peer IRK 到自定义 NVS 槽位
    // 这样可以在重启后恢复，即使 LTK 数据发生变化也能保持稳定
    LISA_LOGI(TAG, "Storing extracted Peer IRK to custom NVS slots");
    for (uint8_t i = 0; i < paired_num && i < NVS_ID_PEER_IRK_MAX; i++) {
        uint8_t nvs_id = NVS_ID_PEER_IRK_BASE + i;
        uint8_t peer_irk_data[23];  // 6字节地址 + 1字节类型 + 16字节IRK = 23字节

        // 复制地址和 IRK 到存储结构
        memcpy(peer_irk_data, paired_addr[i].addr, 6);
        peer_irk_data[6] = paired_addr[i].addr_type;
        memcpy(&peer_irk_data[7], extracted_peer_irk[i], 16);

        // 存储到 NVS
        uint8_t ret = bt_stack_nvs_set(nvs_id, sizeof(peer_irk_data), peer_irk_data);
        if (ret == 0) {
            LISA_LOGI(TAG, "  Stored Peer IRK[%d] to NVS ID=0x%02X: addr=%02X:%02X:...%02X, IRK=%02X%02X...%02X%02X",
                i, nvs_id,
                paired_addr[i].addr[5], paired_addr[i].addr[4], paired_addr[i].addr[0],
                extracted_peer_irk[i][0], extracted_peer_irk[i][1],
                extracted_peer_irk[i][14], extracted_peer_irk[i][15]);
        } else {
            LISA_LOGW(TAG, "  Failed to store Peer IRK[%d] to NVS ID=0x%02X, ret=%d", i, nvs_id, ret);
        }
    }

    // 不再使用 ble_gap_get_paired_ral_info()，它返回全零数据！
    // 直接使用从 LTK 提取的 Peer IRK 构建 RAL
    LISA_LOGI(TAG, "Building RAL from extracted Peer IRK data");

    // 构建 RAL 条目 - 使用从 LTK 提取的 Peer IRK
    uint8_t final_ral_num = 0;

    if (paired_num > 0) {
        LISA_LOGI(TAG, "Building RAL: using paired addresses with extracted Peer IRK from LTK");

        for (uint8_t i = 0; i < paired_num && i < 3; i++) {
            // 复制配对设备的地址（这是正确的身份地址）
            memcpy(ral_devs[i].addr, paired_addr[i].addr, 6);
            ral_devs[i].addr_type = paired_addr[i].addr_type;

            // 使用从 LTK 提取的 Peer IRK
            if (has_valid_peer_irk[i]) {
                memcpy(ral_devs[i].peer_irk, extracted_peer_irk[i], 16);
                LISA_LOGI(TAG, "RAL[%d]: addr=%02X:%02X:%02X:%02X:%02X:%02X, Peer IRK: %02X%02X...%02X%02X",
                    i,
                    ral_devs[i].addr[5], ral_devs[i].addr[4], ral_devs[i].addr[3],
                    ral_devs[i].addr[2], ral_devs[i].addr[1], ral_devs[i].addr[0],
                    ral_devs[i].peer_irk[0], ral_devs[i].peer_irk[1],
                    ral_devs[i].peer_irk[14], ral_devs[i].peer_irk[15]);
            } else {
                memset(ral_devs[i].peer_irk, 0, 16);
                LISA_LOGW(TAG, "RAL[%d]: addr=%02X:%02X:%02X:%02X:%02X:%02X, NO Peer IRK",
                    i,
                    ral_devs[i].addr[5], ral_devs[i].addr[4], ral_devs[i].addr[3],
                    ral_devs[i].addr[2], ral_devs[i].addr[1], ral_devs[i].addr[0]);
            }

            // 设置本地 IRK
            memcpy(ral_devs[i].local_irk, local_irk, 16);

            // 隐私模式
            ral_devs[i].privacy_mode = 0;

            final_ral_num++;
        }
    }

    if (final_ral_num > 0) {
        LISA_LOGI(TAG, "Setting RAL with %d devices", final_ral_num);
        ble_gap_ral_list_set(final_ral_num, ral_devs);
    }
}
#endif

uint8_t *bt_stack_vbat_percent_get(void)
{
    static uint8_t vbat_percent = 80;
    return &vbat_percent;
}
void bt_stack_ble_hid_rcv(uint8_t conidx, uint16_t index, uint16_t length, uint16_t offset, uint8_t *data)
{
    /// send hid data to application to handle.
    //app_hid_rcv_data(conidx, index, length, offset, data);
    if(index == HIDS_GDE_DATA_INDEX)
    {
        app_ble_user_data_rcv(length, data);
    }
}

/**
 ****************************************************************************************
 *
 * @file atcmd_ble_gap.c
 *
 * @brief BLE GAP Implementation
 *
 * Copyright (C) ListenAI  2024-2025
 *
 ****************************************************************************************
 */
#include <string.h>
#include <assert.h>
#include <stdlib.h>    // standard lib functions
#include <stddef.h>    // standard definitions
#include <stdint.h>    // standard integer definition
#include <stdbool.h>   // boolean definition

#include "atcmd.h"
#include "atcmd_ble_gap.h"
#include "atcmd_ble_gatt.h"
#include "log_print.h"
#include "nvs.h"

#include "ble_task.h"
#include "ble_drv.h"
#include "ble_plf_config.h"
#include "ble_gap.h"
#include "ble_prf.h"
#include "bt_os_task.h"
// #include "lisa_log.h"

#include "hogpd_msg.h"
#include "hogpd.h"
#include "bass.h"
#include "diss.h"
#include "netcfg_bles.h"


uint8_t g_ble_conn_status = BLE_CONN_STATUS_DISCONNECT;

static void atcmd_bt_stack_enable_cmp(uint16_t status);
static void atcmd_bt_stack_ble_conn_ind(uint8_t conidx, uint16_t conhdl, gap_bdaddr_t *peer_addr);
static void atcmd_bt_stack_ble_para_update_ind(uint8_t conidx, uint16_t interval, uint16_t latency, uint16_t super_to);
static void atcmd_bt_stack_disc_ind(uint8_t conidx, uint16_t conhdl, uint16_t reason);
static void atcmd_bt_stack_key_req(uint8_t conidx, uint8_t key_type, uint32_t key);
static void atcmd_bt_stack_bond_ind(uint8_t conidx, uint16_t status);
static void atcmd_bt_stack_info_ind(uint8_t conidx, uint8_t type, ble_info_data_t *data);
static void atcmd_bt_stack_ble_adv_report_ind(uint8_t flag, gap_bdaddr_t *peer_addr, int8_t rssi, uint8_t len, uint8_t *data);
static void atcmd_bt_stack_actv_start_ind(uint8_t type, uint8_t actv_id, int16_t status);
static void atcmd_bt_stack_actv_stop_ind(uint8_t type, uint8_t actv_id, int16_t status);

static const char *TAG = "bt_stack_if";

/*
 ****************************************************************************************
 * BLE GAP Callbacks
 ****************************************************************************************
 */
const ble_gap_cb_t atcmd_bt_stack_gap_cb =
{
    .cb_ble_enable_cmp     = atcmd_bt_stack_enable_cmp,
    .cb_ble_conn_ind       = atcmd_bt_stack_ble_conn_ind,
    .cb_ble_conn_update    = atcmd_bt_stack_ble_para_update_ind,
    .cb_ble_disc_ind       = atcmd_bt_stack_disc_ind,
    .cb_ble_key_req        = atcmd_bt_stack_key_req,
    .cb_ble_bond_ind       = atcmd_bt_stack_bond_ind,
    .cb_ble_info_ind       = atcmd_bt_stack_info_ind,
    .cb_ble_adv_report_ind = atcmd_bt_stack_ble_adv_report_ind,
    .cb_ble_actv_start_ind = atcmd_bt_stack_actv_start_ind,
    .cb_ble_actv_stop_ind  = atcmd_bt_stack_actv_stop_ind,
};

static void atcmd_bt_stack_enable_cmp(uint16_t status)
{
    if (status == GAP_NO_ERROR) {

        atcmd_ble_gatt_init();
#if BLE_PEER_FEAT_CON_PARAM_DIS
        ble_gap_set_con_param_dis(1);
#endif
        atcmd_rspinfor("BLEINIT:OK");

        AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "BLE initialized successfully");
    } else {

        atcmd_rspinfor("BLEINIT:FAIL,%d", status);
        AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_ERROR, "BLE initialization failed: %d", status);
    }
}

static void atcmd_bt_stack_ble_conn_ind(uint8_t conidx, uint16_t conhdl, gap_bdaddr_t *peer_addr)
{
    /// save last device address
    bt_stack_nvs_set(NVS_ID_PEER_ADDRESS, GAP_BD_ADDR_LEN, peer_addr->addr);

    // some bt dev send connected ind that have connected param,will impact smp and gatt,so exit latency until param update.
    ble_gap_exit_latency(GAP_EXIT_LATENCY_CONNECT);

    /// get remote feature
    ble_gap_get_con_info(conidx, GAP_INFO_FETURES);

    atcmd_rspinfor("BLECONN:%d,%d,"MACSTR",%d", 
                   conidx, conhdl,
                   MAC2STR(peer_addr->addr), 
                   peer_addr->addr_type);   
    g_ble_conn_status = BLE_CONN_STATUS_CONNECT;
    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_INFO, "BLE connected: conidx=%d, conhdl=%d", conidx, conhdl);

}

static void atcmd_bt_stack_ble_para_update_ind(uint8_t conidx, uint16_t interval, uint16_t latency, uint16_t super_to)
{
    atcmd_rspinfor("BLECONNUPDATE:%d,%d,%d,%d", conidx, interval, latency, super_to);
    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_INFO, "interval: %d; latency: %d; super: %d", interval, latency, super_to);
}

static void atcmd_bt_stack_disc_ind(uint8_t conidx, uint16_t conhdl, uint16_t reason)
{
    if(conidx < BLE_CONNECTION_MAX)
    {
        atcmd_rspinfor("BLEDISCONN:%d,%d,%d", conidx, conhdl, reason);
        gap_bdaddr_t peer = {0};
        uint8_t len = GAP_BD_ADDR_LEN;
        bt_stack_nvs_get(NVS_ID_PEER_ADDRESS, &len, peer.addr);

        AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_INFO, "disconnect addr: "MACSTR, MAC2STR(peer.addr));
        AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_INFO, "disconnect reason: %d", reason);
        g_ble_conn_status = BLE_CONN_STATUS_DISCONNECT;
        atcmd_ble_gatt_hogpd_reinit();

        /// clear all exit latency
        ble_gap_entry_latency(GAP_EXIT_LATENCY_ALL);
    }
}

static void atcmd_bt_stack_key_req(uint8_t conidx, uint8_t key_type, uint32_t key)
{
    ble_gap_key_cfm(conidx, 1, 123456);
    atcmd_rspinfor("BLEKEYREQ:%d,%d,%06u", conidx, key_type, key);
    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "BLE key request: conidx=%d, type=%d, key=%06u", conidx, key_type, key);
}

static void atcmd_bt_stack_bond_ind(uint8_t conidx, uint16_t status)
{

    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "bond status: %d, condix: %d", status, (conidx & GAP_ENCRYPT_REQ));
    if((conidx & ~GAP_ENCRYPT_REQ) < BLE_CONNECTION_MAX)
    {
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
                AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "encrtpt request: %d", status);
                if(1 == status) 
                {
                    #if (BT_STACK_NVDS_SUPPORT)
                    ble_gap_delete_bond(NULL);  //clear all
                    bt_stack_nvs_del(NVS_ID_PEER_ADDRESS);
                    #if RESOVLE_LIST_ADD
                    ble_gap_add_paired_rpa_to_rlist();
                    #endif
                    CLOGI("user dis: 0x18");
                    ble_gap_disconnect(0, 0x18);
                    #endif
                }
                else if(0 == status) 
                {
                }
            }
            else/// bond indicate
            {
                if(0 == status) 
                {
                    g_ble_conn_status = BLE_CONN_STATUS_CONNECT_PAIRED;
                    atcmd_rspinfor("BLEBOND:%d,OK", conidx);
    #if WHITE_LIST_ADD
                    bt_stack_ble_add_paired_to_wlist();
    #endif
    #if RESOVLE_LIST_ADD
                    ble_gap_add_paired_rpa_to_rlist();
    #endif
                } 
                else if(1 == status) 
                {
                #if (BT_STACK_NVDS_SUPPORT)
                    ble_gap_delete_bond(NULL);  //clear all
                    bt_stack_nvs_del(NVS_ID_PEER_ADDRESS);
                    #if RESOVLE_LIST_ADD
                    ble_gap_add_paired_rpa_to_rlist();
                    #endif
                    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_INFO, "user dis: 0x18");
                    ble_gap_disconnect(0, 0x18);
                #endif
                }
            }

            if(0 == status)
            {
                // bt_stack_ble_parameter_update_by_timer(6000);
            }

        }
#endif
    }

}

static void atcmd_bt_stack_info_ind(uint8_t conidx, uint8_t type, ble_info_data_t *data)
{
    if (!data) return;

    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "BLE info callback: type=%d", type);
    switch (type) {
        case GAP_INFO_BDADDR:
            atcmd_rspinfor("BLEADDR:"MACSTR",%d", 
                          MAC2STR(data->addr.addr), 
                          data->addr.addr_type);
            break;
        case GAP_INFO_NAME:
            if (data->name) {
                atcmd_rspinfor("BLENAME:%.*s", data->length, data->name);
            }
            break;
        case GAP_INFO_RSSI:
            atcmd_rspinfor("BLERSSI:%d,%d", conidx, data->rssi);
            break;
        case GAP_INFO_FETURES:
            atcmd_rspinfor("BLEFEATURES:%d,%d,%d,%d,%d,%d,%d,%d",
                          conidx,
                          data->page,
                          data->features[0],
                          data->features[1],
                          data->features[2],
                          data->features[3],
                          data->features[4],
                          data->features[5],
                          data->features[6],
                          data->features[7]);
            break;
        case GAP_INFO_VERSION:
            atcmd_rspinfor("BLEVERSION:%d,%d,%d,%d",
                          conidx,
                          data->lmp_version,
                          data->subvers,
                          data->compid);
            break;
        default:
            AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_ERROR, "BLE info callback: type=%d not exist", type);
            break;
    }
}

static void atcmd_bt_stack_ble_adv_report_ind(uint8_t flag, gap_bdaddr_t *peer_addr, int8_t rssi, uint8_t len, uint8_t *data)
{
    if (!peer_addr || !data) return;

    // Output scan result
    atcmd_rspinfor("BLESCAN:"MACSTR",%d,%d,%d", 
                  MAC2STR(peer_addr->addr), 
                  peer_addr->addr_type, 
                  rssi, 
                  len);
    
    // Print raw advertising data
    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "+BLEADVDATA:");
    for (int i = 0; i < len; i++) {
        _AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "%02X", data[i]);
    }
    _AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "\r\n");
}

static void atcmd_bt_stack_actv_start_ind(uint8_t type, uint8_t actv_id, int16_t status)
{
    if (status == GAP_NO_ERROR) {
        switch(type)
        {
            case GAPM_ACTV_TYPE_ADV :
            case GAPM_ACTV_TYPE_SCAN :
            case GAPM_ACTV_TYPE_INIT :
            case GAPM_ACTV_TYPE_PER_SYNC :
                {
                    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "bt actv start:%d,%d", type, actv_id);
                }
                break;
            default : break;
        }
    } else {
        AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_ERROR, "bt actv start failed:%d,%d,%d", type, actv_id, status);
    }
}

static void atcmd_bt_stack_actv_stop_ind(uint8_t type, uint8_t actv_id, int16_t status)
{
    if (status == GAP_NO_ERROR) {
        switch(type)
        {
            case GAPM_ACTV_TYPE_ADV :
            case GAPM_ACTV_TYPE_SCAN :
            case GAPM_ACTV_TYPE_INIT :
            case GAPM_ACTV_TYPE_PER_SYNC :
                {
                    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "bt actv stop:%d,%d", type, actv_id);
                }
                break;
            default : break;
        }
    } else {
        AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_ERROR, "bt actv stop failed:%d,%d,%d", type, actv_id, status);
    }
}

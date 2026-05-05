/**
 ****************************************************************************************
 *
 * @file atcmd_ble.c
 *
 * @brief BLE Host AT Command Implementation
 *
 * Copyright (C) ListenAI  2024-2025
 *
 ****************************************************************************************
 */

#include <stdbool.h>
#include <stdint.h>
#include "atcmd.h"
#include "atcmd_hash.h"
#include "log_print.h"
#include "nvs.h"
#include "ble_plf_config.h"
#include "ls_event.h"
#include "rtos_al.h"
#include "atcmd_ble.h"
// Include BLE GAP header
#include "ble_gap.h"
#include "bt_os_task.h"
#include "atcmd_ble_gap.h"

// BLE state management
typedef struct {
    bool initialized;
    bool adv_started;
    bool scan_started;
    uint8_t conn_cnt;
    uint8_t adv_id;
    uint8_t scan_id;
} ble_state_t;

static ble_state_t g_ble_state = {0};

// BLE configuration
ble_gap_cfg_t gbt_stack_dev_cfg = {
    .addr = {{0x44, 0x55, 0x66, 0x03, 0x23, 0x20}, 0},
    .name_len = sizeof(DEVICE_NAME),
    .name = DEVICE_NAME,
    .appearance = GAP_APP_GENERIC_MEDIA_PLAYER, // hid_keyboard
    .iocap = GAP_IO_CAP_NO_INPUT_NO_OUTPUT,
    .auth = GAP_SEC_NOT_ENC,
    .pairing_mode = GAPM_PAIRING_LEGACY,
};

// Advertising parameters
typedef struct {
    uint8_t adv_id;
    uint8_t type;
    uint8_t disc_mode;
    uint16_t flags;
    uint8_t filter_pol;
    uint32_t intv_min;
    uint32_t intv_max;
    gap_bdaddr_t peer_addr;
} ble_adv_param_t;

static ble_adv_param_t g_adv_param = {
    .adv_id = GAP_ADV_ID_0,
    .type = GAPM_ADV_TYPE_LEGACY,
    .disc_mode = GAPM_ADV_MODE_GEN_DISC,
    .flags = GAPM_ADV_PROP_CONNECTABLE | GAPM_ADV_PROP_SCANNABLE,
    .filter_pol = GAP_ADV_SCAN_ANY_CON_ANY,
    .intv_min = 160,  // 100ms
    .intv_max = 320,  // 200ms
    .peer_addr = {0},
};

// Advertising and Scan Data storage
typedef struct {
    uint8_t ble_adv_data[BLE_ADV_DATA_LEN];
    uint16_t ble_adv_data_len;
    uint8_t ble_scan_rsp_data[BLE_ADV_DATA_LEN];
    uint16_t ble_scan_rsp_data_len;
} ble_adv_data_t;

static ble_adv_data_t g_adv_data = {
    .ble_adv_data = {0x02, 0x01, 0x05, 
                     0x05, 0xFF, 0x66, 0x79, 0x30, 0x02, 
                     0x09, 0x09, 0x41, 0x52, 0x43, 0x53, 0x5F, 0x42, 0x4C, 0x45
                    },
    .ble_adv_data_len = 28,
    .ble_scan_rsp_data = {0x03, 0x19, 0xC1, 0x03, 0x08, 0xFF, 0x49, 
                          0x46, 0x4C, 0x59, 0x20, 0x52, 0x43},
    .ble_scan_rsp_data_len = 13,
};

typedef struct {
    uint8_t scan_id;
    uint8_t type;
    uint8_t phy;
    uint16_t scan_intv;
    uint16_t scan_win;
} ble_scan_param_t;

static ble_scan_param_t g_scan_param = {
    .scan_id = GAP_SCAN_ID_0,
    .type = GAPM_SCAN_TYPE_GEN_DISC,
    .phy = GAP_PHY_LE_1MBPS,
    .scan_intv = 160,
    .scan_win = 80,
};

// Connection parameters
typedef struct {
    uint8_t conidx;
    uint16_t conhdl;
    gap_bdaddr_t peer_addr;
    bool connected;
} ble_conn_info_t;

#define MAX_BLE_CONNECTIONS 4
static ble_conn_info_t g_conn_info[MAX_BLE_CONNECTIONS] = {0};

typedef struct {
    uint8_t conidx;
    uint16_t conn_intv_min;
    uint16_t conn_intv_max;
    uint16_t latency;
    uint16_t super_to;
} ble_conn_update_param_t;

typedef struct {
    uint8_t conidx;
    uint8_t info_type;
} ble_get_conn_info_param_t;

// Helper function: parse MAC address
static int atcmd_ble_parse_mac(char *str, uint8_t *addr)
{
    if (!str || !addr || strlen(str) < 17)
        return -1;

    char *ptr = str;
    for (int i = 0; i < 6; i++) {
        char *next;
        long int hex = strtol(ptr, &next, 16);
        if (((unsigned)hex > 255) || ((hex == 0) && (next == ptr)) ||
            ((i < 5) && (*next != ':')) ||
            ((i == 5) && (*next != '\0')))
            return -1;
        addr[i] = (uint8_t)hex;
        ptr = ++next;
    }
    return 0;
}

// Helper function: find connection by index
static ble_conn_info_t* find_conn_by_idx(uint8_t conidx)
{
    for (int i = 0; i < MAX_BLE_CONNECTIONS; i++) {
        if (g_conn_info[i].connected && g_conn_info[i].conidx == conidx) {
            return &g_conn_info[i];
        }
    }
    return NULL;
}

// Helper function: add connection
static ble_conn_info_t* add_connection(uint8_t conidx, uint16_t conhdl, gap_bdaddr_t *peer_addr)
{
    for (int i = 0; i < MAX_BLE_CONNECTIONS; i++) {
        if (!g_conn_info[i].connected) {
            g_conn_info[i].conidx = conidx;
            g_conn_info[i].conhdl = conhdl;
            memcpy(&g_conn_info[i].peer_addr, peer_addr, sizeof(gap_bdaddr_t));
            g_conn_info[i].connected = true;
            g_ble_state.conn_cnt++;
            return &g_conn_info[i];
        }
    }
    return NULL;
}

// Helper function: remove connection
static void remove_connection(uint8_t conidx)
{
    ble_conn_info_t *conn = find_conn_by_idx(conidx);
    if (conn) {
        memset(conn, 0, sizeof(ble_conn_info_t));
        if (g_ble_state.conn_cnt > 0) {
            g_ble_state.conn_cnt--;
        }
    }
}

/*
 ****************************************************************************************
 * BLE GAP Callbacks
 ****************************************************************************************
 */

// static void ble_host_enable_cmp_cb(uint16_t status)
// {
//     if (status == GAP_NO_ERROR) {
//         g_ble_state.initialized = true;
//         atcmd_rspdata("BLEINIT:OK");
//         CLOGI("BLE initialized successfully");
//     } else {
//         atcmd_rspdata("BLEINIT:FAIL,%d", status);
//         CLOGE("BLE initialization failed: %d", status);
//     }
// }

// static void ble_host_adv_report_cb(uint8_t flag, gap_bdaddr_t *peer_addr, int8_t rssi, uint8_t len, uint8_t *data)
// {
//     if (!peer_addr || !data) return;

//     // Output scan result
//     atcmd_rspdata("BLESCAN:"MACSTR",%d,%d,%d", 
//                   MAC2STR(peer_addr->addr), 
//                   peer_addr->addr_type, 
//                   rssi, 
//                   len);
    
//     // Print raw advertising data
//     AT_PRINTF("+BLEADVDATA:");
//     for (int i = 0; i < len; i++) {
//         AT_PRINTF("%02X", data[i]);
//     }
//     AT_PRINTF("\r\n");
// }

// static void ble_host_actv_start_cb(uint8_t type, uint8_t actv_id, int16_t status)
// {
//     if (status == GAP_NO_ERROR) {
//         switch (type) {
//             case GAPM_ACTV_TYPE_ADV:
//                 g_ble_state.adv_started = true;
//                 atcmd_rspdata("BLEADVSTART:OK");
//                 CLOGI("BLE advertising started, id=%d", actv_id);
//                 break;
//             case GAPM_ACTV_TYPE_SCAN:
//                 g_ble_state.scan_started = true;
//                 atcmd_rspdata("BLESCANSTART:OK");
//                 CLOGI("BLE scan started, id=%d", actv_id);
//                 break;
//             default:
//                 break;
//         }
//     } else {
//         switch (type) {
//             case GAPM_ACTV_TYPE_ADV:
//                 atcmd_rspdata("BLEADVSTART:FAIL,%d", status);
//                 break;
//             case GAPM_ACTV_TYPE_SCAN:
//                 atcmd_rspdata("BLESCANSTART:FAIL,%d", status);
//                 break;
//             default:
//                 break;
//         }
//     }
// }

// static void ble_host_actv_stop_cb(uint8_t type, uint8_t actv_id, int16_t status)
// {
//     if (status == GAP_NO_ERROR) {
//         switch (type) {
//             case GAPM_ACTV_TYPE_ADV:
//                 g_ble_state.adv_started = false;
//                 atcmd_rspdata("BLEADVSTOP:OK");
//                 CLOGI("BLE advertising stopped");
//                 break;
//             case GAPM_ACTV_TYPE_SCAN:
//                 g_ble_state.scan_started = false;
//                 atcmd_rspdata("BLESCANSTOP:OK");
//                 CLOGI("BLE scan stopped");
//                 break;
//             default:
//                 break;
//         }
//     } else {
//         switch (type) {
//             case GAPM_ACTV_TYPE_ADV:
//                 atcmd_rspdata("BLEADVSTOP:FAIL,%d", status);
//                 break;
//             case GAPM_ACTV_TYPE_SCAN:
//                 atcmd_rspdata("BLESCANSTOP:FAIL,%d", status);
//                 break;
//             default:
//                 break;
//         }
//     }
// }

// static void ble_host_conn_cb(uint8_t conidx, uint16_t conhdl, gap_bdaddr_t *peer_addr)
// {
//     if (add_connection(conidx, conhdl, peer_addr)) {
//         atcmd_rspdata("BLECONN:%d,%d,"MACSTR",%d", 
//                       conidx, conhdl,
//                       MAC2STR(peer_addr->addr), 
//                       peer_addr->addr_type);
//         CLOGI("BLE connected: conidx=%d, conhdl=%d", conidx, conhdl);
//     } else {
//         CLOGE("Failed to add connection");
//     }
// }

// static void ble_host_conn_update_cb(uint8_t conidx, uint16_t interval, uint16_t latency, uint16_t super_to)
// {
//     atcmd_rspdata("BLECONNUPDATE:%d,%d,%d,%d", conidx, interval, latency, super_to);
//     CLOGI("BLE connection updated: conidx=%d", conidx);
// }

// static void ble_host_disc_cb(uint8_t conidx, uint16_t conhdl, uint16_t reason)
// {
//     remove_connection(conidx);
//     atcmd_rspdata("BLEDISCONN:%d,%d,%d", conidx, conhdl, reason);
//     CLOGI("BLE disconnected: conidx=%d, reason=%d", conidx, reason);
// }

// static void ble_host_key_req_cb(uint8_t conidx, uint8_t key_type, uint32_t key)
// {
//     atcmd_rspdata("BLEKEYREQ:%d,%d,%06u", conidx, key_type, key);
//     CLOGI("BLE key request: conidx=%d, type=%d, key=%06u", conidx, key_type, key);
// }

// static void ble_host_bond_cb(uint8_t conidx, uint16_t status)
// {
//     if (status == GAP_NO_ERROR) {
//         atcmd_rspdata("BLEBOND:%d,OK", conidx);
//         CLOGI("BLE bonded: conidx=%d", conidx);
//     } else {
//         atcmd_rspdata("BLEBOND:%d,FAIL,%d", conidx, status);
//         CLOGE("BLE bond failed: conidx=%d, status=%d", conidx, status);
//     }
// }

// static void ble_host_info_cb(uint8_t conidx, uint8_t type, ble_info_data_t *data)
// {
//     if (!data) return;

//     switch (type) {
//         case GAP_INFO_BDADDR:
//             atcmd_rspdata("BLEADDR:"MACSTR",%d", 
//                           MAC2STR(data->addr.addr), 
//                           data->addr.addr_type);
//             break;
//         case GAP_INFO_NAME:
//             if (data->name) {
//                 atcmd_rspdata("BLENAME:%.*s", data->length, data->name);
//             }
//             break;
//         case GAP_INFO_RSSI:
//             atcmd_rspdata("BLERSSI:%d,%d", conidx, data->rssi);
//             break;
//         default:
//             CLOGI("BLE info callback: type=%d", type);
//             break;
//     }
// }

// static void ble_host_event_cmp_cb(uint8_t type, uint16_t status)
// {
//     CLOGI("BLE event complete: type=%d, status=%d", type, status);
// }

/*
 ****************************************************************************************
 * AT Command Implementations
 ****************************************************************************************
 */

uint8_t atcmd_send_event_and_register_handle(uint16_t event_id, void *params, uint16_t params_len, event_handler_t handler_func, void *handler_user_data)
{
    btos_event_t ev;

    ev.msg_body = btos_malloc(sizeof(btos_msg_t) + sizeof(atcmd_msg_t)  + params_len);
    ev.msg_body->msg_id = BT_OS_AT_SEND_EVT;
    ev.msg_body->param_len = sizeof(atcmd_msg_t) + params_len;

    atcmd_msg_t *at_cmd = (atcmd_msg_t *)ev.msg_body->param;
    at_cmd->event_id = event_id;
    at_cmd->data_len = params_len;
    if(params_len > 0)
        memcpy(at_cmd->data, params, params_len);
    reg_event_handler(event_id, handler_func, handler_user_data);

    return btos_send_event(OS_TASK_ID_BT, &ev, BTOS_TASK_MAX_DELAY);
}

void atcmd_event_ble_init_handler(char *buf, int buf_len, int flags, void* handler_user_data)
{
    // Enable BLE stack
    ble_gap_enable(&gbt_stack_dev_cfg, &atcmd_bt_stack_gap_cb);
}
/**
 * AT+BLEINIT - Initialize BLE stack
 * AT+BLEINIT=<role>
 * <role>: 0=disabled, 1=client, 2=server, 3=both
 */
static int atcmd_bleinit(int type, char *params)
{
    if (type == ATCMD_PARAM) {
        atcmd_rspinfor("BLEINIT=<role>");
        atcmd_rspinfor("<role>: 0=disable, 1=client, 2=server, 3=both");
        return ATCMD_OK;
    }
    else if (type == ATCMD_EXEC) {
        int role = 3; // default: both client and server
        
        if (params) {
            char *token = atcmd_next_token(&params);
            if (token) {
                role = atoi(token);
            }
        }

        if (role == 0) {
            // Deinitialize BLE
            g_ble_state.initialized = false;
            atcmd_rspinfor("BLEINIT:DEINIT");
            return ATCMD_OK;
        }

        if(true == g_ble_state.initialized) {
            atcmd_rspinfor("BLEINIT:ALREADY_INIT");
            return ATCMD_OK;
        }
        g_ble_state.initialized = true;

        atcmd_send_event_and_register_handle(EVENT_ATCMD_BLE_INIT, NULL, 0, atcmd_event_ble_init_handler,  &g_adv_param);

        return ATCMD_OK;
    }
    else { // ATCMD_QUERY
        atcmd_rspdata("BLEINIT:%d", g_ble_state.initialized ? 1 : 0);
        return ATCMD_OK;
    }
}


/**
 * AT+BLEADDR - Get/Set BLE address
 * AT+BLEADDR? - Query current address
 * AT+BLEADDR=<addr_type>,<addr> - Set address
 */
static int atcmd_bleaddr(int type, char *params)
{
    if (type == ATCMD_PARAM) {
        atcmd_rspinfor("BLEADDR=<addr_type>,<addr>");
        atcmd_rspinfor("<addr_type>: 0=public, 1=random");
        atcmd_rspinfor("<addr>: MAC address (XX:XX:XX:XX:XX:XX)");
        return ATCMD_OK;
    }
    else if (type == ATCMD_EXEC) {
        uint8_t addr_type = 0;
        uint8_t addr[GAP_BD_ADDR_LEN] = {0};
        
        char *token = atcmd_next_token(&params);
        if (token) {
            addr_type = atoi(token);
        }
        
        token = atcmd_next_token(&params);
        if (token) {
            if (atcmd_ble_parse_mac(token, addr) != 0) {
                atcmd_rspinfor("BLEADDR:INVALID_MAC");
                return ATCMD_ERROR;
            }
        }
        
        // Set public address
        gbt_stack_dev_cfg.addr.addr_type = addr_type;
        memcpy(gbt_stack_dev_cfg.addr.addr, addr, GAP_BD_ADDR_LEN);
        atcmd_rspinfor("BLEADDR:OK");
        return ATCMD_OK;
    }
    else { // ATCMD_QUERY
        if (!g_ble_state.initialized) {
            atcmd_rspdata("BLEADDR:NOT_INIT");
            return ATCMD_ERROR;
        }
        
        // Query local address
        atcmd_rspdata("BLEADDR:"MACSTR",%d", 
                    MAC2STR(gbt_stack_dev_cfg.addr.addr), 
                    gbt_stack_dev_cfg.addr.addr_type);
        return ATCMD_OK;
    }
}

/**
 * AT+BLENAME - Get/Set device name
 * AT+BLENAME? - Query current name
 * AT+BLENAME=<name> - Set name
 */
static int atcmd_blename(int type, char *params)
{
    if (type == ATCMD_PARAM) {
        atcmd_rspinfor("BLENAME=<name>");
        atcmd_rspinfor("<name>: Device name (max 50 chars)");
        return ATCMD_OK;
    }
    else if (type == ATCMD_EXEC) {
        char *token = atcmd_next_token(&params);
        if (!token) {
            atcmd_rspinfor("BLENAME:INVALID_PARAM");
            return ATCMD_ERROR;
        }
        
        int name_len = strlen(token);
        if (name_len > GAP_NAME_LEN) {
            name_len = GAP_NAME_LEN;
        }
        
        memset(gbt_stack_dev_cfg.name, 0, GAP_NAME_LEN);
        memcpy(gbt_stack_dev_cfg.name, token, name_len);
        gbt_stack_dev_cfg.name_len = name_len;
        
        atcmd_rspinfor("BLENAME:OK");
        return ATCMD_OK;
    }
    else { // ATCMD_QUERY
        if (!g_ble_state.initialized) {
            atcmd_rspdata("BLENAME:NOT_INIT");
            return ATCMD_ERROR;
        }
        
        // Query device name
        atcmd_rspdata("BLENAME:%.*s", gbt_stack_dev_cfg.name_len, gbt_stack_dev_cfg.name);
        return ATCMD_OK;
    }
}

void atcmd_event_ble_adv_param_handler(char *buf, int buf_len, int flags, void* handler_user_data)
{
    uint8_t len = GAP_BD_ADDR_LEN;
    ble_adv_param_t *adv_param = (ble_adv_param_t *)handler_user_data;
    if (!adv_param) return;
    memset(&adv_param->peer_addr, 0, sizeof(gap_bdaddr_t));

    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "BLE ADV PARAM UPDATED: adv_id=%d, type=%d, disc_mode=%d, flags=%d, filter_pol=%d, intv_min=%d, intv_max=%d",
          adv_param->adv_id,
          adv_param->type,
          adv_param->disc_mode,
          adv_param->flags,
          adv_param->filter_pol,
          adv_param->intv_min,
          adv_param->intv_max);

    if (adv_param->flags & GAPM_ADV_PROP_DIRECTED) 
    {
        if (bt_stack_nvs_get(NVS_ID_PEER_ADDRESS, &len, adv_param->peer_addr.addr) != NVDS_OK) {
            atcmd_rspinfor("BLEADVPARAM:FAIL,NO_PEER_ADDR");
            AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_ERROR, "No peer address found for directed advertising");
            return;
        } else {
            AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "Using peer address: "MACSTR, MAC2STR(adv_param->peer_addr.addr));
        }
    }

    ble_gap_adv_prepare(adv_param->adv_id, 
                        adv_param->type,
                        adv_param->disc_mode,
                        adv_param->flags,
                        adv_param->filter_pol,
                        adv_param->intv_min,
                        adv_param->intv_max,
                        adv_param->peer_addr,
                        0);
    atcmd_rspinfor("BLEADVPARAM:OK");
    return;
}
 
/**
 * AT+BLEADVPARAM - Set advertising parameters
 * AT+BLEADVPARAM=<adv_id>,<type>,<disc_mode>,<flags>,<filter_policy>,<intv_min>,<intv_max>
 */
static int atcmd_bleadvparam(int type, char *params)
{
    if (type == ATCMD_PARAM) {
        atcmd_rspinfor("BLEADVPARAM=<adv_id>,<type>,<disc_mode>,<flags>,<filter_pol>,<intv_min>,<intv_max>");
        atcmd_rspinfor("<adv_id>: 0-2");
        atcmd_rspinfor("<type>: 0=legacy, 1=extended");
        atcmd_rspinfor("<disc_mode>: 0=non-disc, 1=general, 2=limited");
        atcmd_rspinfor("<flags>: 0=non-connectable, 1=connectable");
        atcmd_rspinfor("<filter_pol>: 0=any, 1=wl, 2=wl_dir");
        atcmd_rspinfor("<intv_min>: Min interval (units of 0.625ms)");
        atcmd_rspinfor("<intv_max>: Max interval (units of 0.625ms)");
        return ATCMD_OK;
    }
    else if (type == ATCMD_EXEC) {
        if (!g_ble_state.initialized) {
            atcmd_rspinfor("BLEADVPARAM:NOT_INIT");
            return ATCMD_ERROR;
        }
        
        int adv_id = 0, adv_type = 0, disc_mode = 1;
        int flags = 0, filter_pol = 0;
        int intv_min = 160, intv_max = 320;
        
        char *token;
        
        token = atcmd_next_token(&params);
        if (token) adv_id = atoi(token);
        
        token = atcmd_next_token(&params);
        if (token) adv_type = atoi(token);
        
        token = atcmd_next_token(&params);
        if (token) disc_mode = atoi(token);
        
        token = atcmd_next_token(&params);
        if (token) flags = atoi(token);
        
        token = atcmd_next_token(&params);
        if (token) filter_pol = atoi(token);
        
        token = atcmd_next_token(&params);
        if (token) intv_min = atoi(token);
        
        token = atcmd_next_token(&params);
        if (token) intv_max = atoi(token);
        
        // Update advertising parameters
        g_adv_param.adv_id = adv_id;
        g_adv_param.type = adv_type;
        g_adv_param.disc_mode = disc_mode;
        g_adv_param.flags = flags;
        g_adv_param.filter_pol = filter_pol;
        g_adv_param.intv_min = intv_min;
        g_adv_param.intv_max = intv_max;
        
        atcmd_send_event_and_register_handle(EVENT_ATCMD_BLE_ADV_PARAM, NULL, 0, atcmd_event_ble_adv_param_handler, &g_adv_param);
        return ATCMD_OK;
    }
    else { // ATCMD_QUERY
        atcmd_rspdata("BLEADVPARAM:%d,%d,%d,%d,%d,%d,%d",
                      g_adv_param.adv_id,
                      g_adv_param.type,
                      g_adv_param.disc_mode,
                      g_adv_param.flags,
                      g_adv_param.filter_pol,
                      g_adv_param.intv_min,
                      g_adv_param.intv_max);
        return ATCMD_OK;
    }
}

void atcmd_event_ble_adv_data_handler(char *buf, int buf_len, int flags, void* handler_user_data)
{
    ble_adv_data_t *adv_data = (ble_adv_data_t *)handler_user_data;
    if (!adv_data) return;

    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "BLE ADV DATA UPDATED: len=%d", adv_data->ble_adv_data_len);

    ble_gap_adv_user_data(GAP_ADV_ID_0, g_adv_data.ble_adv_data_len, g_adv_data.ble_adv_data);
    atcmd_rspinfor("BLEADVDATA:OK");
    return;
}

/**
 * AT+BLEADVDATA - Set/Query advertising data
 * AT+BLEADVDATA=<adv_data> - Set advertising data
 * AT+BLEADVDATA? - Query advertising data
 * Data in hex format (e.g., "020106" for flags)
 */
static int atcmd_bleadvdata(int type, char *params)
{
    if (type == ATCMD_PARAM) {
        atcmd_rspinfor("BLEADVDATA=<adv_data>");
        atcmd_rspinfor("<adv_data>: Advertising data in hex (max 62 chars = 31 bytes)");
        return ATCMD_OK;
    }
    else if (type == ATCMD_EXEC) {
        if (!g_ble_state.initialized) {
            atcmd_rspinfor("BLEADVDATA:NOT_INIT");
            return ATCMD_ERROR;
        }
        
        char *token = atcmd_next_token(&params);
        if (!token) {
            atcmd_rspinfor("BLEADVDATA:INVALID_PARAM");
            return ATCMD_ERROR;
        }
        
        int len = strlen(token);
        if (len % 2 != 0 || len > 62) {
            atcmd_rspinfor("BLEADVDATA:INVALID_FORMAT");
            return ATCMD_ERROR;
        }
        
        uint16_t adv_len = len / 2;
        if (adv_len > BLE_ADV_DATA_LEN) {
            adv_len = BLE_ADV_DATA_LEN;
        }
        
        // Clear and update advertising data in g_adv_data structure
        memset(g_adv_data.ble_adv_data, 0, BLE_ADV_DATA_LEN);
        for (int i = 0; i < adv_len; i++) {
            sscanf(&token[i * 2], "%2hhx", &g_adv_data.ble_adv_data[i]);
        }
        g_adv_data.ble_adv_data_len = adv_len;
        
        atcmd_send_event_and_register_handle(EVENT_ATCMD_BLE_ADV_DATA, NULL, 0, atcmd_event_ble_adv_data_handler, &g_adv_data);
        return ATCMD_OK;
    }
    else { // ATCMD_QUERY
        if (!g_ble_state.initialized) {
            atcmd_rspdata("BLEADVDATA:NOT_INIT");
            return ATCMD_ERROR;
        }
        
        // Output advertising data in hex format
        atcmd_rspdata("BLEADVDATA:");
        for (int i = 0; i < g_adv_data.ble_adv_data_len; i++) {
            AT_PRINTF("%02X", g_adv_data.ble_adv_data[i]);
        }
        AT_PRINTF("\r\n");
        
        atcmd_rspdata("BLEADVDATA:LEN=%d", g_adv_data.ble_adv_data_len);
        return ATCMD_OK;
    }
}

void atcmd_event_ble_scan_rsp_data_handler(char *buf, int buf_len, int flags, void* handler_user_data)
{
    ble_adv_data_t *adv_data = (ble_adv_data_t *)handler_user_data;
    if (!adv_data) return;

    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "BLE SCAN RSP DATA UPDATED: len=%d", adv_data->ble_scan_rsp_data_len);

    ble_gap_adv_set_data(GAP_ADV_ID_0, 0, NULL, g_adv_data.ble_scan_rsp_data_len, g_adv_data.ble_scan_rsp_data);
    atcmd_rspinfor("BLESCANRSPDATA:OK");
    return;
}

/**
 * AT+BLESCANRSPDATA - Set/Query scan response data
 * AT+BLESCANRSPDATA=<scan_rsp_data> - Set scan response data
 * AT+BLESCANRSPDATA? - Query scan response data
 * Data in hex format
 */
static int atcmd_blescanrspdata(int type, char *params)
{
    if (type == ATCMD_PARAM) {
        atcmd_rspinfor("BLESCANRSPDATA=<scan_rsp_data>");
        atcmd_rspinfor("<scan_rsp_data>: Scan response data in hex (max 62 chars = 31 bytes)");
        return ATCMD_OK;
    }
    else if (type == ATCMD_EXEC) {
        if (!g_ble_state.initialized) {
            atcmd_rspinfor("BLESCANRSPDATA:NOT_INIT");
            return ATCMD_ERROR;
        }
        
        char *token = atcmd_next_token(&params);
        if (!token) {
            atcmd_rspinfor("BLESCANRSPDATA:INVALID_PARAM");
            return ATCMD_ERROR;
        }
        
        int len = strlen(token);
        if (len % 2 != 0 || len > 62) {
            atcmd_rspinfor("BLESCANRSPDATA:INVALID_FORMAT");
            return ATCMD_ERROR;
        }
        
        uint16_t rsp_len = len / 2;
        if (rsp_len > BLE_ADV_DATA_LEN) {
            rsp_len = BLE_ADV_DATA_LEN;
        }
        
        // Clear and update scan response data in g_adv_data structure
        memset(g_adv_data.ble_scan_rsp_data, 0, BLE_ADV_DATA_LEN);
        for (int i = 0; i < rsp_len; i++) {
            sscanf(&token[i * 2], "%2hhx", &g_adv_data.ble_scan_rsp_data[i]);
        }
        g_adv_data.ble_scan_rsp_data_len = rsp_len;
        
        
        atcmd_send_event_and_register_handle(EVENT_ATCMD_BLE_ADV_SCANRSP_DATA, NULL, 0, atcmd_event_ble_scan_rsp_data_handler, &g_adv_data);
        return ATCMD_OK;
    }
    else { // ATCMD_QUERY
        if (!g_ble_state.initialized) {
            atcmd_rspdata("BLESCANRSPDATA:NOT_INIT");
            return ATCMD_ERROR;
        }
        
        // Output scan response data in hex format
        atcmd_rspdata("BLESCANRSPDATA:");
        for (int i = 0; i < g_adv_data.ble_scan_rsp_data_len; i++) {
            AT_PRINTF("%02X", g_adv_data.ble_scan_rsp_data[i]);
        }
        AT_PRINTF("\r\n");

        atcmd_rspdata("BLESCANRSPDATA:LEN=%d", g_adv_data.ble_scan_rsp_data_len);
        return ATCMD_OK;
    }
}

void atcmd_event_ble_adv_start_handler(char *buf, int buf_len, int flags, void* handler_user_data)
{
    
    if (!buf || buf_len == 0) return;
    uint8_t adv_id= *((uint8_t *)buf);
    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "BLE ADV STARTED: adv_id=%d", adv_id);
    ble_gap_adv_start(adv_id);
    atcmd_rspinfor("BLEADVSTART:OK");
    return;
}
/**
 * AT+BLEADVSTART - Start advertising
 * AT+BLEADVSTART=<adv_id>
 */
static int atcmd_bleadvstart(int type, char *params)
{
    if (type == ATCMD_PARAM) {
        atcmd_rspinfor("BLEADVSTART=<adv_id>");
        atcmd_rspinfor("<adv_id>: Advertising ID (0-2)");
        return ATCMD_OK;
    }
    else if (type == ATCMD_EXEC) {
        if (!g_ble_state.initialized) {
            atcmd_rspinfor("BLEADVSTART:NOT_INIT");
            return ATCMD_ERROR;
        }
        
        uint8_t adv_id = g_adv_param.adv_id;
        
        if (params) {
            char *token = atcmd_next_token(&params);
            if (token) {
                adv_id = atoi(token);
            }
        }
        // Start advertising
        atcmd_send_event_and_register_handle(EVENT_ATCMD_BLE_ADV_START, &adv_id, sizeof(adv_id), atcmd_event_ble_adv_start_handler, &g_ble_state);
        
        return ATCMD_OK;
    }
    else { // ATCMD_QUERY
        atcmd_rspdata("BLEADVSTART:%d", g_ble_state.adv_started ? 1 : 0);
        return ATCMD_OK;
    }
}

void atcmd_event_ble_adv_stop_handler(char *buf, int buf_len, int flags, void* handler_user_data)
{
    if (!buf || buf_len == 0) return;
    uint8_t adv_id= *((uint8_t *)buf);
    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "BLE ADV STOPPED: adv_id=%d", adv_id);
    ble_gap_adv_stop(adv_id);
    atcmd_rspinfor("BLEADVSTOP:OK");
    return;
}

/**
 * AT+BLEADVSTOP - Stop advertising
 * AT+BLEADVSTOP=<adv_id>
 */
static int atcmd_bleadvstop(int type, char *params)
{
    if (type == ATCMD_PARAM) {
        atcmd_rspinfor("BLEADVSTOP=<adv_id>");
        atcmd_rspinfor("<adv_id>: Advertising ID (0-2)");
        return ATCMD_OK;
    }
    else if (type == ATCMD_EXEC) {
        if (!g_ble_state.initialized) {
            atcmd_rspinfor("BLEADVSTOP:NOT_INIT");
            return ATCMD_ERROR;
        }
        
        uint8_t adv_id = g_adv_param.adv_id;
        
        if (params) {
            char *token = atcmd_next_token(&params);
            if (token) {
                adv_id = atoi(token);
            }
        }
        
        atcmd_send_event_and_register_handle(EVENT_ATCMD_BLE_ADV_STOP, &adv_id, sizeof(adv_id), atcmd_event_ble_adv_stop_handler, &g_ble_state);
        return ATCMD_OK;
    }
    else { // ATCMD_QUERY
        return ATCMD_UNKNOWN;
    }
}

void atcmd_event_ble_disconnect_handler(char *buf, int buf_len, int flags, void* handler_user_data)
{
    if (!buf || buf_len == 0) return;

    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "BLE DISCONNECT REQUESTED: reason=%d", *((uint8_t *)handler_user_data));
    // Here we would call the BLE stack function to disconnect
    uint8_t adv_id= *((uint8_t *)buf);
    ble_gap_disconnect(adv_id, *((uint8_t *)handler_user_data));
    atcmd_rspinfor("BLEDISCONNECT:OK");
    return;
}
/**
 * AT+BLEDISCONNECT - Disconnect from a connected device
 * AT+BLEDISCONNECT=<adv_id>,<reason>
 */
static int atcmd_bledisconn(int type, char *params)
{
    if (type == ATCMD_PARAM) {
        atcmd_rspinfor("BLEDISCONNECT=<adv_id>,<reason>");
        atcmd_rspinfor("<adv_id>: Advertising ID (0-2)");
        atcmd_rspinfor("<reason>: Reason code for disconnection");
        return ATCMD_OK;
    }
    else if (type == ATCMD_EXEC) {
        if (!g_ble_state.initialized) {
            atcmd_rspinfor("BLEDISCONNECT:NOT_INIT");
            return ATCMD_ERROR;
        }

        uint8_t adv_id = g_adv_param.adv_id;
        if (params) {
            char *token = atcmd_next_token(&params);
            if (token) {
                adv_id = atoi(token);
            }

            token = atcmd_next_token(&params);
            if (token) {
                g_default_dis_reason = atoi(token);
            }
        }

        atcmd_send_event_and_register_handle(EVENT_ATCMD_BLE_DISCONNECT, &adv_id, sizeof(adv_id), atcmd_event_ble_disconnect_handler, &g_default_dis_reason);
        return ATCMD_OK;
    }
    else { // ATCMD_QUERY
        return ATCMD_UNKNOWN;
    }
}

void atcmd_event_ble_scan_param_handler(char *buf, int buf_len, int flags, void* handler_user_data)
{
    ble_scan_param_t *scan_param = (ble_scan_param_t *)handler_user_data;
    if (!scan_param) return;

    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "BLE SCAN PARAM UPDATED: scan_id=%d, type=%d, phy=%d, intv=%d, win=%d",
          scan_param->scan_id,
          scan_param->type,
          scan_param->phy,
          scan_param->scan_intv,
          scan_param->scan_win);

    ble_gap_scan_prepare(scan_param->scan_id);
    atcmd_rspinfor("BLESCANPARAM:OK");
    return;
}

/**
 * AT+BLESCANPARAM - Set scan parameters
 * AT+BLESCANPARAM=<scan_id>,<type>,<phy>,<interval>,<window>
 */
static int atcmd_blescanparam(int type, char *params)
{
    if (type == ATCMD_PARAM) {
        atcmd_rspinfor("BLESCANPARAM=<scan_id>,<type>,<phy>,<interval>,<window>");
        atcmd_rspinfor("<scan_id>: 0-2");
        atcmd_rspinfor("<type>: 0=general, 1=limited, 2=observer");
        atcmd_rspinfor("<phy>: 1=1M, 2=2M, 4=Coded");
        atcmd_rspinfor("<interval>: Scan interval (units of 0.625ms)");
        atcmd_rspinfor("<window>: Scan window (units of 0.625ms)");
        return ATCMD_OK;
    }
    else if (type == ATCMD_EXEC) {
        if (!g_ble_state.initialized) {
            atcmd_rspinfor("BLESCANPARAM:NOT_INIT");
            return ATCMD_ERROR;
        }
        
        int scan_id = 0, scan_type = 0, phy = 1;
        int interval = 160, window = 80;
        
        char *token;
        
        token = atcmd_next_token(&params);
        if (token) scan_id = atoi(token);
        
        token = atcmd_next_token(&params);
        if (token) scan_type = atoi(token);
        
        token = atcmd_next_token(&params);
        if (token) phy = atoi(token);
        
        token = atcmd_next_token(&params);
        if (token) interval = atoi(token);
        
        token = atcmd_next_token(&params);
        if (token) window = atoi(token);
        
        // 更新扫描参数
        g_scan_param.scan_id = scan_id;
        g_scan_param.type = scan_type;
        g_scan_param.phy = phy;
        g_scan_param.scan_intv = interval;
        g_scan_param.scan_win = window;
        
        atcmd_send_event_and_register_handle(EVENT_ATCMD_BLE_SCAN_PARAM, NULL, 0, atcmd_event_ble_scan_param_handler, &g_scan_param);
        return ATCMD_OK;
    }
    else { // ATCMD_QUERY
        atcmd_rspdata("BLESCANPARAM:%d,%d,%d,%d,%d",
                      g_scan_param.scan_id,
                      g_scan_param.type,
                      g_scan_param.phy,
                      g_scan_param.scan_intv,
                      g_scan_param.scan_win);
        return ATCMD_OK;
    }
}

void atcmd_event_ble_scan_start_handler(char *buf, int buf_len, int flags, void* handler_user_data)
{
    ble_scan_param_t *scan_param = (ble_scan_param_t *)buf;
    if (!scan_param || buf_len != sizeof(ble_scan_param_t)) return;

    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "BLE SCAN STARTED: scan_id=%d", scan_param->scan_id);
    
    ble_gap_scan_start(scan_param->scan_id, 
                       scan_param->type,
                       scan_param->phy,
                       scan_param->scan_intv,
                       scan_param->scan_win);
    atcmd_rspinfor("BLESCANSTART:OK");
    return;
}

/**
 * AT+BLESCANSTART - Start BLE scan
 * AT+BLESCANSTART=<scan_id>
 */
static int atcmd_blescanstart(int type, char *params)
{
    if (type == ATCMD_PARAM) {
        atcmd_rspinfor("BLESCANSTART=<scan_id>");
        atcmd_rspinfor("<scan_id>: Scan ID (0-2)");
        return ATCMD_OK;
    }
    else if (type == ATCMD_EXEC) {
        if (!g_ble_state.initialized) {
            atcmd_rspinfor("BLESCANSTART:NOT_INIT");
            return ATCMD_ERROR;
        }
        
        uint8_t scan_id = g_scan_param.scan_id;
        
        if (params) {
            char *token = atcmd_next_token(&params);
            if (token) {
                scan_id = atoi(token);
            }
        }
        
        // 使用当前扫描参数但更新scan_id
        ble_scan_param_t scan_param_copy = g_scan_param;
        scan_param_copy.scan_id = scan_id;
        
        atcmd_send_event_and_register_handle(EVENT_ATCMD_BLE_SCAN_START, &scan_param_copy, sizeof(scan_param_copy), atcmd_event_ble_scan_start_handler, NULL);
        return ATCMD_OK;
    }
    else { // ATCMD_QUERY
        atcmd_rspdata("BLESCANSTART:%d", g_ble_state.scan_started ? 1 : 0);
        return ATCMD_OK;
    }
}

void atcmd_event_ble_scan_stop_handler(char *buf, int buf_len, int flags, void* handler_user_data)
{
    uint8_t *scan_id = (uint8_t *)handler_user_data;
    if (!scan_id) return;

    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "BLE SCAN STOPPED: scan_id=%d", *scan_id);
    ble_gap_scan_stop(*scan_id);
    atcmd_rspinfor("BLESCANSTOP:OK");
    return;
}

/**
 * AT+BLESCANSTOP - Stop BLE scan
 * AT+BLESCANSTOP=<scan_id>
 */
static int atcmd_blescanstop(int type, char *params)
{
    if (type == ATCMD_PARAM) {
        atcmd_rspinfor("BLESCANSTOP=<scan_id>");
        atcmd_rspinfor("<scan_id>: Scan ID (0-2)");
        return ATCMD_OK;
    }
    else if (type == ATCMD_EXEC) {
        if (!g_ble_state.initialized) {
            atcmd_rspinfor("BLESCANSTOP:NOT_INIT");
            return ATCMD_ERROR;
        }
        
        uint8_t scan_id = g_scan_param.scan_id;
        
        if (params) {
            char *token = atcmd_next_token(&params);
            if (token) {
                scan_id = atoi(token);
            }
        }
        
        atcmd_send_event_and_register_handle(EVENT_ATCMD_BLE_SCAN_STOP, &scan_id, sizeof(scan_id), atcmd_event_ble_scan_stop_handler, NULL);
        return ATCMD_OK;
    }
    else { // ATCMD_QUERY
        return ATCMD_UNKNOWN;
    }
}

void atcmd_event_ble_connect_handler(char *buf, int buf_len, int flags, void* handler_user_data)
{
    typedef struct {
        gap_bdaddr_t peer_addr;
        uint8_t phy;
        uint16_t conn_intv_min;
        uint16_t conn_intv_max;
        uint16_t latency;
        uint16_t super_to;
    } ble_connect_param_t;
    
    ble_connect_param_t *conn_param = (ble_connect_param_t *)handler_user_data;
    if (!conn_param) return;

    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "BLE CONNECT: "MACSTR, MAC2STR(conn_param->peer_addr.addr));
    
    ble_gap_connect(conn_param->peer_addr,
                    conn_param->phy,
                    conn_param->conn_intv_min,
                    conn_param->conn_intv_max,
                    conn_param->latency,
                    conn_param->super_to);
    atcmd_rspinfor("BLECONNECT:OK");
    return;
}

/**
 * AT+BLECONNECT - Connect to a BLE device
 * AT+BLECONNECT=<addr_type>,<addr>,<phy>,<intv_min>,<intv_max>,<latency>,<timeout>
 */
static int atcmd_bleconnect(int type, char *params)
{
    if (type == ATCMD_PARAM) {
        atcmd_rspinfor("BLECONNECT=<addr_type>,<addr>,<phy>,<intv_min>,<intv_max>,<latency>,<timeout>");
        atcmd_rspinfor("<addr_type>: 0=public, 1=random");
        atcmd_rspinfor("<addr>: MAC address (XX:XX:XX:XX:XX:XX)");
        atcmd_rspinfor("<phy>: 1=1M, 2=2M, 4=Coded");
        atcmd_rspinfor("<intv_min>: Min connection interval (units of 1.25ms)");
        atcmd_rspinfor("<intv_max>: Max connection interval (units of 1.25ms)");
        atcmd_rspinfor("<latency>: Connection latency");
        atcmd_rspinfor("<timeout>: Supervision timeout (units of 10ms)");
        return ATCMD_OK;
    }
    else if (type == ATCMD_EXEC) {
        if (!g_ble_state.initialized) {
            atcmd_rspinfor("BLECONNECT:NOT_INIT");
            return ATCMD_ERROR;
        }
        
        typedef struct {
            gap_bdaddr_t peer_addr;
            uint8_t phy;
            uint16_t conn_intv_min;
            uint16_t conn_intv_max;
            uint16_t latency;
            uint16_t super_to;
        } ble_connect_param_t;
        
        static ble_connect_param_t conn_param;
        memset(&conn_param, 0, sizeof(conn_param));
        
        // 默认参数
        conn_param.phy = GAP_PHY_LE_1MBPS;
        conn_param.conn_intv_min = 8;
        conn_param.conn_intv_max = 8;
        conn_param.latency = 0;
        conn_param.super_to = 500;
        
        char *token;
        
        // 地址类型
        token = atcmd_next_token(&params);
        if (token) {
            conn_param.peer_addr.addr_type = atoi(token);
        }
        
        // MAC地址
        token = atcmd_next_token(&params);
        if (!token) {
            atcmd_rspinfor("BLECONNECT:INVALID_ADDR");
            return ATCMD_ERROR;
        }
        if (atcmd_ble_parse_mac(token, conn_param.peer_addr.addr) != 0) {
            atcmd_rspinfor("BLECONNECT:INVALID_MAC");
            return ATCMD_ERROR;
        }
        
        // PHY
        token = atcmd_next_token(&params);
        if (token) conn_param.phy = atoi(token);
        
        // 连接间隔最小值
        token = atcmd_next_token(&params);
        if (token) conn_param.conn_intv_min = atoi(token);
        
        // 连接间隔最大值
        token = atcmd_next_token(&params);
        if (token) conn_param.conn_intv_max = atoi(token);
        
        // 延迟
        token = atcmd_next_token(&params);
        if (token) conn_param.latency = atoi(token);
        
        // 超时
        token = atcmd_next_token(&params);
        if (token) conn_param.super_to = atoi(token);
        
        atcmd_send_event_and_register_handle(EVENT_ATCMD_BLE_CONNECT, &conn_param, sizeof(conn_param), atcmd_event_ble_connect_handler, NULL);
        return ATCMD_OK;
    }
    else { // ATCMD_QUERY
        return ATCMD_UNKNOWN;
    }
}

void atcmd_event_ble_connect_cancel_handler(char *buf, int buf_len, int flags, void* handler_user_data)
{
    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "BLE CONNECT CANCEL");
    ble_gap_connect_cancel();
    atcmd_rspinfor("BLECONNCANCEL:OK");
    return;
}

/**
 * AT+BLECONNCANCEL - Cancel ongoing connection
 * AT+BLECONNCANCEL
 */
static int atcmd_bleconncancel(int type, char *params)
{
    if (type == ATCMD_PARAM) {
        atcmd_rspinfor("BLECONNCANCEL - Cancel ongoing connection");
        return ATCMD_OK;
    }
    else if (type == ATCMD_EXEC) {
        if (!g_ble_state.initialized) {
            atcmd_rspinfor("BLECONNCANCEL:NOT_INIT");
            return ATCMD_ERROR;
        }
        
        uint8_t dummy = 0;
        atcmd_send_event_and_register_handle(EVENT_ATCMD_BLE_CONNECT_CANCEL, &dummy, sizeof(dummy), atcmd_event_ble_connect_cancel_handler, NULL);
        return ATCMD_OK;
    }
    else { // ATCMD_QUERY
        return ATCMD_UNKNOWN;
    }
}

void atcmd_event_ble_connect_update_handler(char *buf, int buf_len, int flags, void* handler_user_data)
{
    
    ble_conn_update_param_t *param = (ble_conn_update_param_t *)buf;
    if (!param || buf_len != sizeof(ble_conn_update_param_t)) return;

    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "BLE CONN UPDATE: conidx=%d, min=%d, max=%d, latency=%d, timeout=%d",
          param->conidx, param->conn_intv_min, param->conn_intv_max, param->latency, param->super_to);
    
    ble_gap_connect_update(param->conidx, param->conn_intv_min, param->conn_intv_max, 
                          param->latency, param->super_to);
    atcmd_rspinfor("BLECONNUPDATE:OK");
    return;
}

/**
 * AT+BLECONNUPDATE - Update connection parameters
 * AT+BLECONNUPDATE=<conidx>,<intv_min>,<intv_max>,<latency>,<timeout>
 */
static int atcmd_bleconnupdate(int type, char *params)
{
    if (type == ATCMD_PARAM) {
        atcmd_rspinfor("BLECONNUPDATE=<conidx>,<intv_min>,<intv_max>,<latency>,<timeout>");
        atcmd_rspinfor("<conidx>: Connection index");
        atcmd_rspinfor("<intv_min>: Min connection interval (units of 1.25ms)");
        atcmd_rspinfor("<intv_max>: Max connection interval (units of 1.25ms)");
        atcmd_rspinfor("<latency>: Connection latency");
        atcmd_rspinfor("<timeout>: Supervision timeout (units of 10ms)");
        return ATCMD_OK;
    }
    else if (type == ATCMD_EXEC) {
        if (!g_ble_state.initialized) {
            atcmd_rspinfor("BLECONNUPDATE:NOT_INIT");
            return ATCMD_ERROR;
        }
        
        static ble_conn_update_param_t update_param;
        memset(&update_param, 0, sizeof(update_param));
        
        char *token;
        
        // Connection index
        token = atcmd_next_token(&params);
        if (!token) {
            atcmd_rspinfor("BLECONNUPDATE:INVALID_PARAM");
            return ATCMD_ERROR;
        }
        update_param.conidx = atoi(token);
        
        // Min interval
        token = atcmd_next_token(&params);
        if (!token) {
            atcmd_rspinfor("BLECONNUPDATE:INVALID_PARAM");
            return ATCMD_ERROR;
        }
        update_param.conn_intv_min = atoi(token);
        
        // Max interval
        token = atcmd_next_token(&params);
        if (!token) {
            atcmd_rspinfor("BLECONNUPDATE:INVALID_PARAM");
            return ATCMD_ERROR;
        }
        update_param.conn_intv_max = atoi(token);
        
        // Latency
        token = atcmd_next_token(&params);
        if (!token) {
            atcmd_rspinfor("BLECONNUPDATE:INVALID_PARAM");
            return ATCMD_ERROR;
        }
        update_param.latency = atoi(token);
        
        // Timeout
        token = atcmd_next_token(&params);
        if (!token) {
            atcmd_rspinfor("BLECONNUPDATE:INVALID_PARAM");
            return ATCMD_ERROR;
        }
        update_param.super_to = atoi(token);
        
        atcmd_send_event_and_register_handle(EVENT_ATCMD_BLE_CONNECT_UPDATE, &update_param, sizeof(update_param), atcmd_event_ble_connect_update_handler, NULL);
        return ATCMD_OK;
    }
    else { // ATCMD_QUERY
        return ATCMD_UNKNOWN;
    }
}

void atcmd_event_ble_get_conn_info_handler(char *buf, int buf_len, int flags, void* handler_user_data)
{
    
    ble_get_conn_info_param_t *param = (ble_get_conn_info_param_t *)buf;
    if (!param || buf_len != sizeof(ble_get_conn_info_param_t)) return;

    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "BLE GET CONN INFO: conidx=%d, type=%d", param->conidx, param->info_type);
    
    ble_gap_get_con_info(param->conidx, param->info_type);
    atcmd_rspinfor("BLECONNINFO:OK");
    return;
}

/**
 * AT+BLECONNINFO - Get connection information
 * AT+BLECONNINFO=<conidx>,<info_type>
 * info_type: 0=BDADDR, 1=NAME, 2=APPEARANCE, 3=VERSION, 4=FEATURES, 5=RSSI
 */
static int atcmd_bleconninfo(int type, char *params)
{
    if (type == ATCMD_PARAM) {
        atcmd_rspinfor("BLECONNINFO=<conidx>,<info_type>");
        atcmd_rspinfor("<conidx>: Connection index");
        atcmd_rspinfor("<info_type>: 0=BDADDR, 1=NAME, 2=APPEARANCE, 3=VERSION, 4=FEATURES, 5=RSSI");
        return ATCMD_OK;
    }
    else if (type == ATCMD_EXEC) {
        if (!g_ble_state.initialized) {
            atcmd_rspinfor("BLECONNINFO:NOT_INIT");
            return ATCMD_ERROR;
        }
        
        static ble_get_conn_info_param_t info_param;
        memset(&info_param, 0, sizeof(info_param));
        
        char *token;
        
        // Connection index
        token = atcmd_next_token(&params);
        if (!token) {
            atcmd_rspinfor("BLECONNINFO:INVALID_PARAM");
            return ATCMD_ERROR;
        }
        info_param.conidx = atoi(token);
        
        // Info type
        token = atcmd_next_token(&params);
        if (!token) {
            atcmd_rspinfor("BLECONNINFO:INVALID_PARAM");
            return ATCMD_ERROR;
        }
        info_param.info_type = atoi(token);
        
        atcmd_send_event_and_register_handle(EVENT_ATCMD_BLE_GET_CONN_INFO, &info_param, sizeof(info_param), atcmd_event_ble_get_conn_info_handler, NULL);
        return ATCMD_OK;
    }
    else { // ATCMD_QUERY
        return ATCMD_UNKNOWN;
    }
}

void atcmd_event_ble_get_dev_info_handler(char *buf, int buf_len, int flags, void* handler_user_data)
{
    uint8_t *info_type = (uint8_t *)buf;
    if (!info_type || buf_len != sizeof(uint8_t)) return;

    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "BLE GET DEV INFO: type=%d", *info_type);
    /*
        // for get info
        enum gap_dev_info_type
        {
            GAP_INFO_BDADDR,
            GAP_INFO_NAME,
            GAP_INFO_APPEARANCE,
            GAP_INFO_VERSION,
            GAP_INFO_FETURES,
            GAP_INFO_RSSI,
            GAP_INFO_WHITE_LIST_SIZE,
            GAP_INFO_RAL_LIST_SIZE,
            GAP_INFO_PAL_LIST_SIZE,
        };
    */
    ble_gap_get_dev_info(*info_type);
    atcmd_rspinfor("BLEDEVINFO:OK");
    return;
}

/**
 * AT+BLEDEVINFO - Get local device information
 * AT+BLEDEVINFO=<info_type>
 * info_type: 0=BDADDR, 1=NAME, 6=WHITE_LIST_SIZE, 7=RAL_LIST_SIZE
 */
static int atcmd_bledevinfo(int type, char *params)
{
    if (type == ATCMD_PARAM) {
        atcmd_rspinfor("BLEDEVINFO=<info_type>");
        atcmd_rspinfor("<info_type>: 0=BDADDR, 1=NAME, 6=WL_SIZE, 7=RAL_SIZE, 8=PAL_SIZE");
        return ATCMD_OK;
    }
    else if (type == ATCMD_EXEC) {
        if (!g_ble_state.initialized) {
            atcmd_rspinfor("BLEDEVINFO:NOT_INIT");
            return ATCMD_ERROR;
        }
        
        uint8_t info_type = 0;
        
        char *token = atcmd_next_token(&params);
        if (!token) {
            atcmd_rspinfor("BLEDEVINFO:INVALID_PARAM");
            return ATCMD_ERROR;
        }
        info_type = atoi(token);
        
        atcmd_send_event_and_register_handle(EVENT_ATCMD_BLE_GET_DEV_INFO, &info_type, sizeof(info_type), atcmd_event_ble_get_dev_info_handler, NULL);
        return ATCMD_OK;
    }
    else { // ATCMD_QUERY
        return ATCMD_UNKNOWN;
    }
}

void atcmd_event_ble_adv_state_handler(char *buf, int buf_len, int flags, void* handler_user_data)
{
    uint8_t *adv_id = (uint8_t *)buf;
    if (!adv_id || buf_len != sizeof(uint8_t)) return;

    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "BLE ADV STATE REQUESTED: adv_id=%d", *adv_id);
    uint8_t state = ble_gap_get_adv_state(*adv_id);
    atcmd_rspdata("BLEADVSTATE:%d,%d", *adv_id, state);
    return;
}
/**
 * AT+BLEADVSTATE - Get advertising state
 * AT+BLEADVSTATE=<adv_id>
 */
static int atcmd_bleadvstate(int type, char *params)
{
    if (type == ATCMD_PARAM) {
        atcmd_rspinfor("BLEADVSTATE=<adv_id>");
        atcmd_rspinfor("<adv_id>: Advertising ID (0-2)");
        return ATCMD_OK;
    }
    else if (type == ATCMD_EXEC) {
        if (!g_ble_state.initialized) {
            atcmd_rspinfor("BLEADVSTATE:NOT_INIT");
            return ATCMD_ERROR;
        }
        
        uint8_t adv_id = 0;
        
        char *token = atcmd_next_token(&params);
        if (token) {
            adv_id = atoi(token);
        }
        
        atcmd_send_event_and_register_handle(EVENT_ATCMD_BLE_ADV_STATE, &adv_id, sizeof(adv_id), atcmd_event_ble_adv_state_handler, NULL);
        return ATCMD_OK;
    }
    else { // ATCMD_QUERY
        return ATCMD_UNKNOWN;
    }
}

void atcmd_event_ble_auth_req_handler(char *buf, int buf_len, int flags, void* handler_user_data)
{
    typedef struct {
        uint8_t conidx;
        uint8_t sec_lvl;
    } ble_auth_req_param_t;
    
    ble_auth_req_param_t *param = (ble_auth_req_param_t *)buf;
    if (!param || buf_len != sizeof(ble_auth_req_param_t)) return;

    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "BLE AUTH REQ: conidx=%d, sec_lvl=%d", param->conidx, param->sec_lvl);
    
    ble_gap_auth_req(param->conidx, param->sec_lvl);
    atcmd_rspinfor("BLEAUTHREQ:OK");
    return;
}

/**
 * AT+BLEAUTHREQ - Request authentication/encryption
 * AT+BLEAUTHREQ=<conidx>,<sec_lvl>
 * sec_lvl: 0=NOT_ENC, 1=UNAUTH, 2=AUTH, 3=SECURE_CON
 */
static int atcmd_bleauthreq(int type, char *params)
{
    if (type == ATCMD_PARAM) {
        atcmd_rspinfor("BLEAUTHREQ=<conidx>,<sec_lvl>");
        atcmd_rspinfor("<conidx>: Connection index");
        atcmd_rspinfor("<sec_lvl>: 0=NOT_ENC, 1=UNAUTH, 2=AUTH, 3=SECURE_CON");
        return ATCMD_OK;
    }
    else if (type == ATCMD_EXEC) {
        if (!g_ble_state.initialized) {
            atcmd_rspinfor("BLEAUTHREQ:NOT_INIT");
            return ATCMD_ERROR;
        }
        
        typedef struct {
            uint8_t conidx;
            uint8_t sec_lvl;
        } ble_auth_req_param_t;
        
        static ble_auth_req_param_t auth_param;
        memset(&auth_param, 0, sizeof(auth_param));
        
        char *token;
        
        // Connection index
        token = atcmd_next_token(&params);
        if (!token) {
            atcmd_rspinfor("BLEAUTHREQ:INVALID_PARAM");
            return ATCMD_ERROR;
        }
        auth_param.conidx = atoi(token);
        
        // Security level
        token = atcmd_next_token(&params);
        if (!token) {
            atcmd_rspinfor("BLEAUTHREQ:INVALID_PARAM");
            return ATCMD_ERROR;
        }
        auth_param.sec_lvl = atoi(token);
        
        atcmd_send_event_and_register_handle(EVENT_ATCMD_BLE_AUTH_REQ, &auth_param, sizeof(auth_param), atcmd_event_ble_auth_req_handler, NULL);
        return ATCMD_OK;
    }
    else { // ATCMD_QUERY
        return ATCMD_UNKNOWN;
    }
}

void atcmd_event_ble_key_cfm_handler(char *buf, int buf_len, int flags, void* handler_user_data)
{
    typedef struct {
        uint8_t conidx;
        uint8_t accept;
        uint32_t key;
    } ble_key_cfm_param_t;
    
    ble_key_cfm_param_t *param = (ble_key_cfm_param_t *)buf;
    if (!param || buf_len != sizeof(ble_key_cfm_param_t)) return;

    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "BLE KEY CFM: conidx=%d, accept=%d, key=%u", 
              param->conidx, param->accept, param->key);
    
    ble_gap_key_cfm(param->conidx, param->accept, param->key);
    atcmd_rspinfor("BLEKEYCFM:OK");
    return;
}

/**
 * AT+BLEKEYCFM - Confirm pairing key
 * AT+BLEKEYCFM=<conidx>,<accept>,<key>
 */
static int atcmd_blekeycfm(int type, char *params)
{
    if (type == ATCMD_PARAM) {
        atcmd_rspinfor("BLEKEYCFM=<conidx>,<accept>,<key>");
        atcmd_rspinfor("<conidx>: Connection index");
        atcmd_rspinfor("<accept>: 0=reject, 1=accept");
        atcmd_rspinfor("<key>: 6-digit passkey (000000-999999)");
        return ATCMD_OK;
    }
    else if (type == ATCMD_EXEC) {
        if (!g_ble_state.initialized) {
            atcmd_rspinfor("BLEKEYCFM:NOT_INIT");
            return ATCMD_ERROR;
        }
        
        typedef struct {
            uint8_t conidx;
            uint8_t accept;
            uint32_t key;
        } ble_key_cfm_param_t;
        
        static ble_key_cfm_param_t key_param;
        memset(&key_param, 0, sizeof(key_param));
        
        char *token;
        
        // Connection index
        token = atcmd_next_token(&params);
        if (!token) {
            atcmd_rspinfor("BLEKEYCFM:INVALID_PARAM");
            return ATCMD_ERROR;
        }
        key_param.conidx = atoi(token);
        
        // Accept/reject
        token = atcmd_next_token(&params);
        if (!token) {
            atcmd_rspinfor("BLEKEYCFM:INVALID_PARAM");
            return ATCMD_ERROR;
        }
        key_param.accept = atoi(token);
        
        // Key (only needed if accept)
        token = atcmd_next_token(&params);
        if (token) {
            key_param.key = atoi(token);
        }
        
        atcmd_send_event_and_register_handle(EVENT_ATCMD_BLE_KEY_CFM, &key_param, sizeof(key_param), atcmd_event_ble_key_cfm_handler, NULL);
        return ATCMD_OK;
    }
    else { // ATCMD_QUERY
        return ATCMD_UNKNOWN;
    }
}

void atcmd_event_ble_delete_bond_handler(char *buf, int buf_len, int flags, void* handler_user_data)
{
    gap_bdaddr_t *addr = (gap_bdaddr_t *)buf;
    if (!addr || buf_len != sizeof(gap_bdaddr_t)) return;
    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "BLE DELETE BOND");
    
    // If addr is all zeros, delete all bonds, otherwise delete specific device
    bool delete_all = true;
    if (addr) {
        for (int i = 0; i < GAP_BD_ADDR_LEN; i++) {
            if (addr->addr[i] != 0) {
                delete_all = false;
                break;
            }
        }
    }
    
    if (delete_all) {
        ble_gap_delete_bond(NULL);
        atcmd_rspinfor("BLEDELBOND:ALL_OK");
    } else {
        ble_gap_delete_bond(addr);
        atcmd_rspinfor("BLEDELBOND:OK");
    }
    return;
}

/**
 * AT+BLEDELBOND - Delete bond information
 * AT+BLEDELBOND - Delete all bonds
 * AT+BLEDELBOND=<addr_type>,<addr> - Delete specific device bond
 */
static int atcmd_bledelbond(int type, char *params)
{
    if (type == ATCMD_PARAM) {
        atcmd_rspinfor("BLEDELBOND - Delete all bonds");
        atcmd_rspinfor("BLEDELBOND=<addr_type>,<addr> - Delete specific bond");
        atcmd_rspinfor("<addr_type>: 0=public, 1=random");
        atcmd_rspinfor("<addr>: MAC address (XX:XX:XX:XX:XX:XX)");
        return ATCMD_OK;
    }
    else if (type == ATCMD_EXEC) {
        if (!g_ble_state.initialized) {
            atcmd_rspinfor("BLEDELBOND:NOT_INIT");
            return ATCMD_ERROR;
        }
        
        static gap_bdaddr_t bond_addr;
        memset(&bond_addr, 0, sizeof(bond_addr));
        
        if (params && *params) {
            char *token;
            
            // Address type
            token = atcmd_next_token(&params);
            if (token) {
                bond_addr.addr_type = atoi(token);
            }
            
            // MAC address
            token = atcmd_next_token(&params);
            if (token) {
                if (atcmd_ble_parse_mac(token, bond_addr.addr) != 0) {
                    atcmd_rspinfor("BLEDELBOND:INVALID_MAC");
                    return ATCMD_ERROR;
                }
            }
        }
        
        atcmd_send_event_and_register_handle(EVENT_ATCMD_BLE_DELETE_BOND, &bond_addr, sizeof(bond_addr), atcmd_event_ble_delete_bond_handler, NULL);
        return ATCMD_OK;
    }
    else { // ATCMD_QUERY
        return ATCMD_UNKNOWN;
    }
}

void atcmd_event_ble_paired_list_handler(char *buf, int buf_len, int flags, void* handler_user_data)
{
    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "BLE PAIRED LIST REQUESTED");
    
    gap_bdaddr_t paired_addrs[8]; // Maximum 8 paired devices
    memset(paired_addrs, 0, sizeof(paired_addrs));
    
    uint8_t count = ble_gap_get_paired_addr(paired_addrs);
    
    atcmd_rspdata("BLEPAIREDLIST:COUNT=%d", count);
    for (uint8_t i = 0; i < count; i++) {
        atcmd_rspdata("BLEPAIREDLIST:%d,"MACSTR",%d", 
                     i,
                     MAC2STR(paired_addrs[i].addr),
                     paired_addrs[i].addr_type);
    }
    return;
}

/**
 * AT+BLEPAIREDLIST - Get paired devices list
 * AT+BLEPAIREDLIST?
 */
static int atcmd_blepairedlist(int type, char *params)
{
    if (type == ATCMD_PARAM) {
        atcmd_rspinfor("BLEPAIREDLIST? - Query paired devices");
        return ATCMD_OK;
    }
    else if (type == ATCMD_EXEC) {
        atcmd_rspinfor("BLEPAIREDLIST:USE_QUERY");
        return ATCMD_ERROR;
    }
    else { // ATCMD_QUERY
        if (!g_ble_state.initialized) {
            atcmd_rspdata("BLEPAIREDLIST:NOT_INIT");
            return ATCMD_ERROR;
        }
        
        uint8_t dummy = 0;
        atcmd_send_event_and_register_handle(EVENT_ATCMD_BLE_PAIRED_LIST, &dummy, sizeof(dummy), atcmd_event_ble_paired_list_handler, NULL);
        return ATCMD_OK;
    }
}

void atcmd_event_ble_set_phy_handler(char *buf, int buf_len, int flags, void* handler_user_data)
{
    typedef struct {
        uint8_t conidx;
        uint8_t rx_phy;
        uint8_t tx_phy;
        uint8_t phy_opt;
    } ble_set_phy_param_t;
    
    ble_set_phy_param_t *param = (ble_set_phy_param_t *)buf;
    if (!param || buf_len != sizeof(ble_set_phy_param_t)) return;

    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "BLE SET PHY: conidx=%d, rx_phy=%d, tx_phy=%d, opt=%d",
              param->conidx, param->rx_phy, param->tx_phy, param->phy_opt);
    
    uint8_t status = ble_gap_set_phy(param->conidx, param->rx_phy, param->tx_phy, param->phy_opt);
    atcmd_rspinfor("BLESETPHY:STATUS=%d", status);
    return;
}

/**
 * AT+BLESETPHY - Set PHY mode
 * AT+BLESETPHY=<conidx>,<rx_phy>,<tx_phy>,<phy_opt>
 * phy: 1=1M, 2=2M, 4=Coded
 */
static int atcmd_blesetphy(int type, char *params)
{
    if (type == ATCMD_PARAM) {
        atcmd_rspinfor("BLESETPHY=<conidx>,<rx_phy>,<tx_phy>,<phy_opt>");
        atcmd_rspinfor("<conidx>: Connection index");
        atcmd_rspinfor("<rx_phy>: RX PHY (1=1M, 2=2M, 4=Coded)");
        atcmd_rspinfor("<tx_phy>: TX PHY (1=1M, 2=2M, 4=Coded)");
        atcmd_rspinfor("<phy_opt>: PHY options (0=no preference)");
        return ATCMD_OK;
    }
    else if (type == ATCMD_EXEC) {
        if (!g_ble_state.initialized) {
            atcmd_rspinfor("BLESETPHY:NOT_INIT");
            return ATCMD_ERROR;
        }
        
        typedef struct {
            uint8_t conidx;
            uint8_t rx_phy;
            uint8_t tx_phy;
            uint8_t phy_opt;
        } ble_set_phy_param_t;
        
        static ble_set_phy_param_t phy_param;
        memset(&phy_param, 0, sizeof(phy_param));
        
        char *token;
        
        // Connection index
        token = atcmd_next_token(&params);
        if (!token) {
            atcmd_rspinfor("BLESETPHY:INVALID_PARAM");
            return ATCMD_ERROR;
        }
        phy_param.conidx = atoi(token);
        
        // RX PHY
        token = atcmd_next_token(&params);
        if (!token) {
            atcmd_rspinfor("BLESETPHY:INVALID_PARAM");
            return ATCMD_ERROR;
        }
        phy_param.rx_phy = atoi(token);
        
        // TX PHY
        token = atcmd_next_token(&params);
        if (!token) {
            atcmd_rspinfor("BLESETPHY:INVALID_PARAM");
            return ATCMD_ERROR;
        }
        phy_param.tx_phy = atoi(token);
        
        // PHY option
        token = atcmd_next_token(&params);
        if (token) {
            phy_param.phy_opt = atoi(token);
        }
        
        atcmd_send_event_and_register_handle(EVENT_ATCMD_BLE_SET_PHY, &phy_param, sizeof(phy_param), atcmd_event_ble_set_phy_handler, NULL);
        return ATCMD_OK;
    }
    else { // ATCMD_QUERY
        return ATCMD_UNKNOWN;
    }
}

void atcmd_event_ble_mtu_exch_handler(char *buf, int buf_len, int flags, void* handler_user_data)
{
    typedef struct {
        uint8_t conidx;
        uint8_t user_lid;
    } ble_mtu_exch_param_t;
    
    ble_mtu_exch_param_t *param = (ble_mtu_exch_param_t *)buf;
    if (!param || buf_len != sizeof(ble_mtu_exch_param_t)) return;

    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "BLE MTU EXCH: conidx=%d, user_lid=%d", param->conidx, param->user_lid);
    
    uint8_t status = ble_gap_mtu_exch(param->conidx, param->user_lid);
    atcmd_rspinfor("BLEMTUEXCH:STATUS=%d", status);
    return;
}

/**
 * AT+BLEMTUEXCH - Exchange MTU
 * AT+BLEMTUEXCH=<conidx>,<user_lid>
 */
static int atcmd_blemtuexch(int type, char *params)
{
    if (type == ATCMD_PARAM) {
        atcmd_rspinfor("BLEMTUEXCH=<conidx>,<user_lid>");
        atcmd_rspinfor("<conidx>: Connection index");
        atcmd_rspinfor("<user_lid>: User local identifier (usually 0)");
        return ATCMD_OK;
    }
    else if (type == ATCMD_EXEC) {
        if (!g_ble_state.initialized) {
            atcmd_rspinfor("BLEMTUEXCH:NOT_INIT");
            return ATCMD_ERROR;
        }
        
        typedef struct {
            uint8_t conidx;
            uint8_t user_lid;
        } ble_mtu_exch_param_t;
        
        static ble_mtu_exch_param_t mtu_param;
        memset(&mtu_param, 0, sizeof(mtu_param));
        
        char *token;
        
        // Connection index
        token = atcmd_next_token(&params);
        if (!token) {
            atcmd_rspinfor("BLEMTUEXCH:INVALID_PARAM");
            return ATCMD_ERROR;
        }
        mtu_param.conidx = atoi(token);
        
        // User LID
        token = atcmd_next_token(&params);
        if (token) {
            mtu_param.user_lid = atoi(token);
        }
        
        atcmd_send_event_and_register_handle(EVENT_ATCMD_BLE_MTU_EXCH, &mtu_param, sizeof(mtu_param), atcmd_event_ble_mtu_exch_handler, NULL);
        return ATCMD_OK;
    }
    else { // ATCMD_QUERY
        return ATCMD_UNKNOWN;
    }
}

/**
 * AT+BLEGETMTU - Get peer MTU
 * AT+BLEGETMTU=<conidx>
 */
static int atcmd_blegetmtu(int type, char *params)
{
    if (type == ATCMD_PARAM) {
        atcmd_rspinfor("BLEGETMTU=<conidx>");
        atcmd_rspinfor("<conidx>: Connection index");
        return ATCMD_OK;
    }
    else if (type == ATCMD_EXEC) {
        if (!g_ble_state.initialized) {
            atcmd_rspinfor("BLEGETMTU:NOT_INIT");
            return ATCMD_ERROR;
        }
        
        uint8_t conidx = 0;
        
        char *token = atcmd_next_token(&params);
        if (!token) {
            atcmd_rspinfor("BLEGETMTU:INVALID_PARAM");
            return ATCMD_ERROR;
        }
        conidx = atoi(token);
        
        uint16_t mtu = ble_gap_get_peer_mtu(conidx);
        atcmd_rspdata("BLEGETMTU:%d,%d", conidx, mtu);
        return ATCMD_OK;
    }
    else { // ATCMD_QUERY
        return ATCMD_UNKNOWN;
    }
}

void atcmd_event_ble_exit_latency_handler(char *buf, int buf_len, int flags, void* handler_user_data)
{
    uint8_t *latency_type = (uint8_t *)buf;
    if (!latency_type || buf_len != sizeof(uint8_t)) return;

    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "BLE EXIT LATENCY: type=%d", *latency_type);
    
    ble_gap_exit_latency(*latency_type);
    atcmd_rspinfor("BLEEXITLATENCY:OK");
    return;
}

/**
 * AT+BLEEXITLATENCY - Exit low power latency mode
 * AT+BLEEXITLATENCY=<type>
 * type: 1=CONNECT, 2=AUDIO, 4=OTA, 255=ALL
 */
static int atcmd_bleexitlatency(int type, char *params)
{
    if (type == ATCMD_PARAM) {
        atcmd_rspinfor("BLEEXITLATENCY=<type>");
        atcmd_rspinfor("<type>: 1=CONNECT, 2=AUDIO, 4=OTA, 255=ALL");
        return ATCMD_OK;
    }
    else if (type == ATCMD_EXEC) {
        if (!g_ble_state.initialized) {
            atcmd_rspinfor("BLEEXITLATENCY:NOT_INIT");
            return ATCMD_ERROR;
        }
        
        uint8_t latency_type = 0xFF; // Default: ALL
        
        char *token = atcmd_next_token(&params);
        if (token) {
            latency_type = atoi(token);
        }
        
        atcmd_send_event_and_register_handle(EVENT_ATCMD_BLE_EXIT_LATENCY, &latency_type, sizeof(latency_type), atcmd_event_ble_exit_latency_handler, NULL);
        return ATCMD_OK;
    }
    else { // ATCMD_QUERY
        return ATCMD_UNKNOWN;
    }
}

void atcmd_event_ble_entry_latency_handler(char *buf, int buf_len, int flags, void* handler_user_data)
{
    uint8_t *latency_type = (uint8_t *)buf;
    if (!latency_type || buf_len != sizeof(uint8_t)) return;

    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "BLE ENTRY LATENCY: type=%d", *latency_type);
    
    ble_gap_entry_latency(*latency_type);
    atcmd_rspinfor("BLEENTRYLATENCY:OK");
    return;
}

/**
 * AT+BLEENTRYLATENCY - Entry low power latency mode
 * AT+BLEENTRYLATENCY=<type>
 * type: 1=CONNECT, 2=AUDIO, 4=OTA, 255=ALL
 */
static int atcmd_bleentrylatency(int type, char *params)
{
    if (type == ATCMD_PARAM) {
        atcmd_rspinfor("BLEENTRYLATENCY=<type>");
        atcmd_rspinfor("<type>: 1=CONNECT, 2=AUDIO, 4=OTA, 255=ALL");
        return ATCMD_OK;
    }
    else if (type == ATCMD_EXEC) {
        if (!g_ble_state.initialized) {
            atcmd_rspinfor("BLEENTRYLATENCY:NOT_INIT");
            return ATCMD_ERROR;
        }
        
        uint8_t latency_type = 0xFF; // Default: ALL
        
        char *token = atcmd_next_token(&params);
        if (token) {
            latency_type = atoi(token);
        }
        
        atcmd_send_event_and_register_handle(EVENT_ATCMD_BLE_ENTRY_LATENCY, &latency_type, sizeof(latency_type), atcmd_event_ble_entry_latency_handler, NULL);
        return ATCMD_OK;
    }
    else { // ATCMD_QUERY
        return ATCMD_UNKNOWN;
    }
}

void atcmd_event_ble_set_con_param_dis_handler(char *buf, int buf_len, int flags, void* handler_user_data)
{
    uint8_t *flag = (uint8_t *)buf;
    if (!flag || buf_len != sizeof(uint8_t)) return;

    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "BLE SET CON PARAM DIS: flag=%d", *flag);
    
    ble_gap_set_con_param_dis(*flag);
    atcmd_rspinfor("BLECONPARAMDIS:OK");
    return;
}

/**
 * AT+BLECONPARAMDIS - Disable connection parameter update
 * AT+BLECONPARAMDIS=<flag>
 * flag: 0=enable update, 1=disable update
 */
static int atcmd_bleconparamdis(int type, char *params)
{
    if (type == ATCMD_PARAM) {
        atcmd_rspinfor("BLECONPARAMDIS=<flag>");
        atcmd_rspinfor("<flag>: 0=enable update, 1=disable update");
        return ATCMD_OK;
    }
    else if (type == ATCMD_EXEC) {
        if (!g_ble_state.initialized) {
            atcmd_rspinfor("BLECONPARAMDIS:NOT_INIT");
            return ATCMD_ERROR;
        }
        
        uint8_t flag = 1; // Default: disable
        
        char *token = atcmd_next_token(&params);
        if (token) {
            flag = atoi(token);
        }
        
        atcmd_send_event_and_register_handle(EVENT_ATCMD_BLE_SET_CON_PARAM_DIS, &flag, sizeof(flag), atcmd_event_ble_set_con_param_dis_handler, NULL);
        return ATCMD_OK;
    }
    else { // ATCMD_QUERY
        if (!g_ble_state.initialized) {
            atcmd_rspdata("BLECONPARAMDIS:NOT_INIT");
            return ATCMD_ERROR;
        }
        
        uint8_t flag = ble_gap_get_con_param_dis();
        atcmd_rspdata("BLECONPARAMDIS:%d", flag);
        return ATCMD_OK;
    }
}

void atcmd_event_ble_whitelist_handler(char *buf, int buf_len, int flags, void* handler_user_data)
{
    typedef struct {
        uint8_t size;
        gap_bdaddr_t addrs[8]; // Max 8 devices
    } ble_whitelist_param_t;
    
    ble_whitelist_param_t *param = (ble_whitelist_param_t *)buf;
    if (!param || buf_len != sizeof(ble_whitelist_param_t)) return;

    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "BLE WHITELIST: size=%d", param->size);
    
    ble_gap_wl_list_set(param->size, param->addrs);
    atcmd_rspinfor("BLEWHITELIST:OK");
    return;
}

/**
 * AT+BLEWHITELIST - Set whitelist
 * AT+BLEWHITELIST=<count>,<addr_type1>,<addr1>,...
 */
static int atcmd_blewhitelist(int type, char *params)
{
    if (type == ATCMD_PARAM) {
        atcmd_rspinfor("BLEWHITELIST=<count>,<addr_type1>,<addr1>,...");
        atcmd_rspinfor("<count>: Number of devices (max 8)");
        atcmd_rspinfor("<addr_typeN>: Address type (0=public, 1=random)");
        atcmd_rspinfor("<addrN>: MAC address (XX:XX:XX:XX:XX:XX)");
        return ATCMD_OK;
    }
    else if (type == ATCMD_EXEC) {
        if (!g_ble_state.initialized) {
            atcmd_rspinfor("BLEWHITELIST:NOT_INIT");
            return ATCMD_ERROR;
        }
        
        typedef struct {
            uint8_t size;
            gap_bdaddr_t addrs[8];
        } ble_whitelist_param_t;
        
        static ble_whitelist_param_t wl_param;
        memset(&wl_param, 0, sizeof(wl_param));
        
        char *token;
        
        // Device count
        token = atcmd_next_token(&params);
        if (!token) {
            atcmd_rspinfor("BLEWHITELIST:INVALID_PARAM");
            return ATCMD_ERROR;
        }
        wl_param.size = atoi(token);
        
        if (wl_param.size > 8) {
            wl_param.size = 8;
        }
        
        // Parse each device address
        for (uint8_t i = 0; i < wl_param.size; i++) {
            // Address type
            token = atcmd_next_token(&params);
            if (!token) {
                atcmd_rspinfor("BLEWHITELIST:INVALID_PARAM");
                return ATCMD_ERROR;
            }
            wl_param.addrs[i].addr_type = atoi(token);
            
            // MAC address
            token = atcmd_next_token(&params);
            if (!token) {
                atcmd_rspinfor("BLEWHITELIST:INVALID_PARAM");
                return ATCMD_ERROR;
            }
            if (atcmd_ble_parse_mac(token, wl_param.addrs[i].addr) != 0) {
                atcmd_rspinfor("BLEWHITELIST:INVALID_MAC");
                return ATCMD_ERROR;
            }
        }
        
        atcmd_send_event_and_register_handle(EVENT_ATCMD_BLE_WHITELIST, &wl_param, sizeof(wl_param), atcmd_event_ble_whitelist_handler, NULL);
        return ATCMD_OK;
    }
    else { // ATCMD_QUERY
        return ATCMD_UNKNOWN;
    }
}

void atcmd_event_ble_adv_gen_handler(char *buf, int buf_len, int flags, void* handler_user_data)
{
    typedef struct {
        uint8_t adv_id;
        uint8_t nb_uuid;
        uint16_t uuids[8]; // Max 8 UUIDs
    } ble_adv_gen_param_t;
    
    ble_adv_gen_param_t *param = (ble_adv_gen_param_t *)buf;
    if (!param || buf_len != sizeof(ble_adv_gen_param_t)) return;

    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "BLE ADV GEN: adv_id=%d, nb_uuid=%d", param->adv_id, param->nb_uuid);
    
    ble_gap_adv_gen_data(param->adv_id, param->nb_uuid, param->uuids);
    atcmd_rspinfor("BLEADVGEN:OK");
    return;
}

/**
 * AT+BLEADVGEN - Generate advertising data from UUIDs
 * AT+BLEADVGEN=<adv_id>,<count>,<uuid1>,<uuid2>,...
 */
static int atcmd_bleadvgen(int type, char *params)
{
    if (type == ATCMD_PARAM) {
        atcmd_rspinfor("BLEADVGEN=<adv_id>,<count>,<uuid1>,<uuid2>,...");
        atcmd_rspinfor("<adv_id>: Advertising ID (0-2)");
        atcmd_rspinfor("<count>: Number of UUIDs (max 8)");
        atcmd_rspinfor("<uuidN>: 16-bit UUID in hex");
        return ATCMD_OK;
    }
    else if (type == ATCMD_EXEC) {
        if (!g_ble_state.initialized) {
            atcmd_rspinfor("BLEADVGEN:NOT_INIT");
            return ATCMD_ERROR;
        }
        
        typedef struct {
            uint8_t adv_id;
            uint8_t nb_uuid;
            uint16_t uuids[8];
        } ble_adv_gen_param_t;
        
        static ble_adv_gen_param_t gen_param;
        memset(&gen_param, 0, sizeof(gen_param));
        
        char *token;
        
        // Advertising ID
        token = atcmd_next_token(&params);
        if (!token) {
            atcmd_rspinfor("BLEADVGEN:INVALID_PARAM");
            return ATCMD_ERROR;
        }
        gen_param.adv_id = atoi(token);
        
        // UUID count
        token = atcmd_next_token(&params);
        if (!token) {
            atcmd_rspinfor("BLEADVGEN:INVALID_PARAM");
            return ATCMD_ERROR;
        }
        gen_param.nb_uuid = atoi(token);
        
        if (gen_param.nb_uuid > 8) {
            gen_param.nb_uuid = 8;
        }
        
        // Parse UUIDs
        for (uint8_t i = 0; i < gen_param.nb_uuid; i++) {
            token = atcmd_next_token(&params);
            if (!token) {
                atcmd_rspinfor("BLEADVGEN:INVALID_PARAM");
                return ATCMD_ERROR;
            }
            // Parse hex UUID
            sscanf(token, "%hx", &gen_param.uuids[i]);
        }
        
        atcmd_send_event_and_register_handle(EVENT_ATCMD_BLE_ADV_GEN, &gen_param, sizeof(gen_param), atcmd_event_ble_adv_gen_handler, NULL);
        return ATCMD_OK;
    }
    else { // ATCMD_QUERY
        return ATCMD_UNKNOWN;
    }
}

void atcmd_event_ble_per_sync_start_handler(char *buf, int buf_len, int flags, void* handler_user_data)
{
    typedef struct {
        uint8_t sync_type;
        gap_per_adv_bdaddr_t adv_addr;
        uint8_t report_en;
        uint8_t past_conidx;
        uint16_t timeout;
    } ble_per_sync_param_t;
    
    ble_per_sync_param_t *param = (ble_per_sync_param_t *)buf;
    if (!param || buf_len != sizeof(ble_per_sync_param_t)) return;

    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "BLE PER SYNC START: type=%d, timeout=%d", param->sync_type, param->timeout);
    
    ble_gap_per_sync_start(param->sync_type, &param->adv_addr, param->report_en, param->past_conidx, param->timeout);
    atcmd_rspinfor("BLEPERSYNCSTART:OK");
    return;
}

/**
 * AT+BLEPERSYNCSTART - Start periodic advertising sync
 * AT+BLEPERSYNCSTART=<type>,<addr_type>,<addr>,<sid>,<report_en>,<timeout>
 */
static int atcmd_blepersyncstart(int type, char *params)
{
    if (type == ATCMD_PARAM) {
        atcmd_rspinfor("BLEPERSYNCSTART=<type>,<addr_type>,<addr>,<sid>,<report_en>,<timeout>");
        atcmd_rspinfor("<type>: 0=general, 1=selective, 2=past");
        atcmd_rspinfor("<addr_type>: Address type (0=public, 1=random)");
        atcmd_rspinfor("<addr>: MAC address");
        atcmd_rspinfor("<sid>: Advertising SID");
        atcmd_rspinfor("<report_en>: Report enable flags");
        atcmd_rspinfor("<timeout>: Timeout in units of 10ms (100ms-163.84s)");
        return ATCMD_OK;
    }
    else if (type == ATCMD_EXEC) {
        if (!g_ble_state.initialized) {
            atcmd_rspinfor("BLEPERSYNCSTART:NOT_INIT");
            return ATCMD_ERROR;
        }
        
        typedef struct {
            uint8_t sync_type;
            gap_per_adv_bdaddr_t adv_addr;
            uint8_t report_en;
            uint8_t past_conidx;
            uint16_t timeout;
        } ble_per_sync_param_t;
        
        static ble_per_sync_param_t sync_param;
        memset(&sync_param, 0, sizeof(sync_param));
        
        char *token;
        
        // Sync type
        token = atcmd_next_token(&params);
        if (!token) {
            atcmd_rspinfor("BLEPERSYNCSTART:INVALID_PARAM");
            return ATCMD_ERROR;
        }
        sync_param.sync_type = atoi(token);
        
        // Address type
        token = atcmd_next_token(&params);
        if (token) {
            sync_param.adv_addr.addr_type = atoi(token);
        }
        
        // MAC address
        token = atcmd_next_token(&params);
        if (token) {
            if (atcmd_ble_parse_mac(token, sync_param.adv_addr.addr) != 0) {
                atcmd_rspinfor("BLEPERSYNCSTART:INVALID_MAC");
                return ATCMD_ERROR;
            }
        }
        
        // Advertising SID
        token = atcmd_next_token(&params);
        if (token) {
            sync_param.adv_addr.adv_sid = atoi(token);
        }
        
        // Report enable
        token = atcmd_next_token(&params);
        if (token) {
            sync_param.report_en = atoi(token);
        }
        
        // Timeout
        token = atcmd_next_token(&params);
        if (token) {
            sync_param.timeout = atoi(token);
        } else {
            sync_param.timeout = 500; // Default: 5s
        }
        
        atcmd_send_event_and_register_handle(EVENT_ATCMD_BLE_PER_SYNC_START, &sync_param, sizeof(sync_param), atcmd_event_ble_per_sync_start_handler, NULL);
        return ATCMD_OK;
    }
    else { // ATCMD_QUERY
        return ATCMD_UNKNOWN;
    }
}

void atcmd_event_ble_per_sync_stop_handler(char *buf, int buf_len, int flags, void* handler_user_data)
{
    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "BLE PER SYNC STOP");
    
    ble_gap_per_sync_stop();
    atcmd_rspinfor("BLEPERSYNCSTOP:OK");
    return;
}

/**
 * AT+BLEPERSYNCSTOP - Stop periodic advertising sync
 * AT+BLEPERSYNCSTOP
 */
static int atcmd_blepersyncstop(int type, char *params)
{
    if (type == ATCMD_PARAM) {
        atcmd_rspinfor("BLEPERSYNCSTOP - Stop periodic advertising sync");
        return ATCMD_OK;
    }
    else if (type == ATCMD_EXEC) {
        if (!g_ble_state.initialized) {
            atcmd_rspinfor("BLEPERSYNCSTOP:NOT_INIT");
            return ATCMD_ERROR;
        }
        
        uint8_t dummy = 0;
        atcmd_send_event_and_register_handle(EVENT_ATCMD_BLE_PER_SYNC_STOP, &dummy, sizeof(dummy), atcmd_event_ble_per_sync_stop_handler, NULL);
        return ATCMD_OK;
    }
    else { // ATCMD_QUERY
        return ATCMD_UNKNOWN;
    }
}

void atcmd_event_ble_test_tx_handler(char *buf, int buf_len, int flags, void* handler_user_data)
{
    typedef struct {
        uint8_t channel;
        uint8_t tx_pkt_payload;
        uint8_t tx_data_length;
        uint8_t phy;
        int8_t tx_pwr_lvl;
    } ble_test_tx_param_t;
    
    ble_test_tx_param_t *param = (ble_test_tx_param_t *)buf;
    if (!param || buf_len != sizeof(ble_test_tx_param_t)) return;

    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "BLE TEST TX: ch=%d, phy=%d, pwr=%d", 
              param->channel, param->phy, param->tx_pwr_lvl);
    
    ble_gap_test_mode_tx(param->channel, param->tx_pkt_payload, param->tx_data_length, 
                        param->phy, param->tx_pwr_lvl);
    atcmd_rspinfor("BLETESTTX:OK");
    return;
}

/**
 * AT+BLETESTTX - Start TX test mode
 * AT+BLETESTTX=<ch>,<payload>,<len>,<phy>,<pwr>
 */
static int atcmd_bletesttx(int type, char *params)
{
    if (type == ATCMD_PARAM) {
        atcmd_rspinfor("BLETESTTX=<ch>,<payload>,<len>,<phy>,<pwr>");
        atcmd_rspinfor("<ch>: TX channel (0-39)");
        atcmd_rspinfor("<payload>: Payload type (0-7)");
        atcmd_rspinfor("<len>: Data length (0-255)");
        atcmd_rspinfor("<phy>: PHY (1=1M, 2=2M, 3=Coded S8, 4=Coded S2)");
        atcmd_rspinfor("<pwr>: TX power level in dBm (-127 to +20)");
        return ATCMD_OK;
    }
    else if (type == ATCMD_EXEC) {
        typedef struct {
            uint8_t channel;
            uint8_t tx_pkt_payload;
            uint8_t tx_data_length;
            uint8_t phy;
            int8_t tx_pwr_lvl;
        } ble_test_tx_param_t;
        
        static ble_test_tx_param_t test_param;
        memset(&test_param, 0, sizeof(test_param));
        
        char *token;
        
        // Channel
        token = atcmd_next_token(&params);
        if (!token) {
            atcmd_rspinfor("BLETESTTX:INVALID_PARAM");
            return ATCMD_ERROR;
        }
        test_param.channel = atoi(token);
        
        // Payload type
        token = atcmd_next_token(&params);
        if (!token) {
            atcmd_rspinfor("BLETESTTX:INVALID_PARAM");
            return ATCMD_ERROR;
        }
        test_param.tx_pkt_payload = atoi(token);
        
        // Data length
        token = atcmd_next_token(&params);
        if (!token) {
            atcmd_rspinfor("BLETESTTX:INVALID_PARAM");
            return ATCMD_ERROR;
        }
        test_param.tx_data_length = atoi(token);
        
        // PHY
        token = atcmd_next_token(&params);
        if (!token) {
            atcmd_rspinfor("BLETESTTX:INVALID_PARAM");
            return ATCMD_ERROR;
        }
        test_param.phy = atoi(token);
        
        // TX power
        token = atcmd_next_token(&params);
        if (!token) {
            atcmd_rspinfor("BLETESTTX:INVALID_PARAM");
            return ATCMD_ERROR;
        }
        test_param.tx_pwr_lvl = atoi(token);
        
        atcmd_send_event_and_register_handle(EVENT_ATCMD_BLE_TEST_TX, &test_param, sizeof(test_param), atcmd_event_ble_test_tx_handler, NULL);
        return ATCMD_OK;
    }
    else { // ATCMD_QUERY
        return ATCMD_UNKNOWN;
    }
}

void atcmd_event_ble_test_rx_handler(char *buf, int buf_len, int flags, void* handler_user_data)
{
    typedef struct {
        uint8_t channel;
        uint8_t modulation_idx;
        uint8_t slot_dur;
        uint8_t phy;
    } ble_test_rx_param_t;
    
    ble_test_rx_param_t *param = (ble_test_rx_param_t *)buf;
    if (!param || buf_len != sizeof(ble_test_rx_param_t)) return;

    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "BLE TEST RX: ch=%d, phy=%d", param->channel, param->phy);
    
    ble_gap_test_mode_rx(param->channel, param->modulation_idx, param->slot_dur, param->phy);
    atcmd_rspinfor("BLETESTRX:OK");
    return;
}

/**
 * AT+BLETESTRX - Start RX test mode
 * AT+BLETESTRX=<ch>,<mod>,<slot>,<phy>
 */
static int atcmd_bletestrx(int type, char *params)
{
    if (type == ATCMD_PARAM) {
        atcmd_rspinfor("BLETESTRX=<ch>,<mod>,<slot>,<phy>");
        atcmd_rspinfor("<ch>: RX channel (0-39)");
        atcmd_rspinfor("<mod>: Modulation index (0=standard, 1=stable)");
        atcmd_rspinfor("<slot>: Slot duration");
        atcmd_rspinfor("<phy>: PHY (1=1M, 2=2M, 3=Coded)");
        return ATCMD_OK;
    }
    else if (type == ATCMD_EXEC) {
        typedef struct {
            uint8_t channel;
            uint8_t modulation_idx;
            uint8_t slot_dur;
            uint8_t phy;
        } ble_test_rx_param_t;
        
        static ble_test_rx_param_t test_param;
        memset(&test_param, 0, sizeof(test_param));
        
        char *token;
        
        // Channel
        token = atcmd_next_token(&params);
        if (!token) {
            atcmd_rspinfor("BLETESTRX:INVALID_PARAM");
            return ATCMD_ERROR;
        }
        test_param.channel = atoi(token);
        
        // Modulation index
        token = atcmd_next_token(&params);
        if (!token) {
            atcmd_rspinfor("BLETESTRX:INVALID_PARAM");
            return ATCMD_ERROR;
        }
        test_param.modulation_idx = atoi(token);
        
        // Slot duration
        token = atcmd_next_token(&params);
        if (!token) {
            atcmd_rspinfor("BLETESTRX:INVALID_PARAM");
            return ATCMD_ERROR;
        }
        test_param.slot_dur = atoi(token);
        
        // PHY
        token = atcmd_next_token(&params);
        if (!token) {
            atcmd_rspinfor("BLETESTRX:INVALID_PARAM");
            return ATCMD_ERROR;
        }
        test_param.phy = atoi(token);
        
        atcmd_send_event_and_register_handle(EVENT_ATCMD_BLE_TEST_RX, &test_param, sizeof(test_param), atcmd_event_ble_test_rx_handler, NULL);
        return ATCMD_OK;
    }
    else { // ATCMD_QUERY
        return ATCMD_UNKNOWN;
    }
}

void atcmd_event_ble_test_stop_handler(char *buf, int buf_len, int flags, void* handler_user_data)
{
    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "BLE TEST STOP");
    
    ble_gap_stop_test();
    atcmd_rspinfor("BLETESTSTOP:OK");
    return;
}

/**
 * AT+BLETESTSTOP - Stop test mode
 * AT+BLETESTSTOP
 */
static int atcmd_bleteststop(int type, char *params)
{
    if (type == ATCMD_PARAM) {
        atcmd_rspinfor("BLETESTSTOP - Stop BLE test mode");
        return ATCMD_OK;
    }
    else if (type == ATCMD_EXEC) {
        uint8_t dummy = 0;
        atcmd_send_event_and_register_handle(EVENT_ATCMD_BLE_TEST_STOP, &dummy, sizeof(dummy), atcmd_event_ble_test_stop_handler, NULL);
        return ATCMD_OK;
    }
    else { // ATCMD_QUERY
        return ATCMD_UNKNOWN;
    }
}

void atcmd_event_ble_set_pub_addr_handler(char *buf, int buf_len, int flags, void* handler_user_data)
{
    uint8_t *addr = (uint8_t *)buf;
    if (!addr || buf_len != GAP_BD_ADDR_LEN) return;
    
    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "BLE SET PUB ADDR: "MACSTR, MAC2STR(addr));
    bt_stack_nvs_del(NVS_ID_BD_ADDRESS);
    if(bt_stack_nvs_set(NVS_ID_BD_ADDRESS, GAP_BD_ADDR_LEN, addr) == NVDS_OK)
    {
        ble_gap_set_loc_pub_addr(addr);
        atcmd_rspinfor("SET PUB ADDR: %02x:%02x:%02x:%02x:%02x:%02x",
                    addr[5],addr[4],addr[3],
                    addr[2],addr[1],addr[0]);
    }
    // ble_gap_set_loc_pub_addr(addr);
    atcmd_rspinfor("BLESETPUBADDR:OK");
    return;
}

/**
 * AT+BLESETPUBADDR - Set local public address
 * AT+BLESETPUBADDR=<addr>
 */
static int atcmd_blesetpubaddr(int type, char *params)
{
    if (type == ATCMD_PARAM) {
        atcmd_rspinfor("BLESETPUBADDR=<addr>");
        atcmd_rspinfor("<addr>: MAC address (XX:XX:XX:XX:XX:XX)");
        return ATCMD_OK;
    }
    else if (type == ATCMD_EXEC) {
        static uint8_t pub_addr[GAP_BD_ADDR_LEN];
        memset(pub_addr, 0, GAP_BD_ADDR_LEN);
        
        char *token = atcmd_next_token(&params);
        if (!token) {
            atcmd_rspinfor("BLESETPUBADDR:INVALID_PARAM");
            return ATCMD_ERROR;
        }
        
        if (atcmd_ble_parse_mac(token, pub_addr) != 0) {
            atcmd_rspinfor("BLESETPUBADDR:INVALID_MAC");
            return ATCMD_ERROR;
        }
        // Reverse byte order (little-endian to big-endian)
        uint8_t reversed_addr[GAP_BD_ADDR_LEN];
        for (int i = 0; i < GAP_BD_ADDR_LEN; i++) {
            reversed_addr[i] = pub_addr[GAP_BD_ADDR_LEN - 1 - i];
        }
        memcpy(pub_addr, reversed_addr, GAP_BD_ADDR_LEN);
        atcmd_send_event_and_register_handle(EVENT_ATCMD_BLE_SET_PUB_ADDR, pub_addr, GAP_BD_ADDR_LEN, atcmd_event_ble_set_pub_addr_handler, NULL);
        return ATCMD_OK;
    }
    else { // ATCMD_QUERY
        return ATCMD_UNKNOWN;
    }
}

void atcmd_event_ble_set_con_exit_latency_handler(char *buf, int buf_len, int flags, void* handler_user_data)
{
    uint8_t *flag = (uint8_t *)buf;
    if (!flag || buf_len != sizeof(uint8_t)) return;

    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "BLE SET CON EXIT LATENCY: flag=%d", *flag);
    
    ble_gap_set_con_exit_latency(*flag);
    atcmd_rspinfor("BLESETCONEXITLATENCY:OK");
    return;
}

/**
 * AT+BLESETCONEXITLATENCY - Set connection exit latency
 * AT+BLESETCONEXITLATENCY=<flag>
 */
static int atcmd_blesetconexitlatency(int type, char *params)
{
    if (type == ATCMD_PARAM) {
        atcmd_rspinfor("BLESETCONEXITLATENCY=<flag>");
        atcmd_rspinfor("<flag>: 0=disable, 1=enable");
        return ATCMD_OK;
    }
    else if (type == ATCMD_EXEC) {
        uint8_t flag = 1; // Default: enable
        
        char *token = atcmd_next_token(&params);
        if (token) {
            flag = atoi(token);
        }
        
        atcmd_send_event_and_register_handle(EVENT_ATCMD_BLE_SET_CON_EXIT_LATENCY, &flag, sizeof(flag), atcmd_event_ble_set_con_exit_latency_handler, NULL);
        return ATCMD_OK;
    }
    else { // ATCMD_QUERY
        return ATCMD_UNKNOWN;
    }
}

void atcmd_ble_msg_handle(void* msg)
{
    atcmd_msg_t *at_cmd = (atcmd_msg_t *)msg;
    indicate_event_handle(at_cmd->event_id, at_cmd->data, at_cmd->data_len, 0);
}

/*
 ****************************************************************************************
 * AT Command Table
 ****************************************************************************************
 */

static const atcmd_item_t atcmd_ble_host_table[] = {
    // === Initialization ===
    {atcmd_bleinit,       "AT+BLEINIT",       "Initialize BLE stack\r\n"
                                              "AT+BLEINIT=<role>\r\n"},
    
    // === Device Information ===
    {atcmd_bleaddr,       "AT+BLEADDR",       "Get/Set BLE address\r\n"
                                              "AT+BLEADDR=<type>,<addr>\r\n"},
    
    {atcmd_blename,       "AT+BLENAME",       "Get/Set device name\r\n"
                                              "AT+BLENAME=<name>\r\n"},
    
    {atcmd_bledevinfo,    "AT+BLEDEVINFO",    "Get device information\r\n"
                                              "AT+BLEDEVINFO=<info_type>\r\n"},
    
    // === Advertising ===
    {atcmd_bleadvparam,   "AT+BLEADVPARAM",   "Set advertising parameters\r\n"
                                              "AT+BLEADVPARAM=<id>,<type>,<disc>,<flags>,<filter>,<min>,<max>\r\n"},
    
    {atcmd_bleadvdata,    "AT+BLEADVDATA",    "Set/Query advertising data\r\n"
                                              "AT+BLEADVDATA=<adv_data>\r\n"},
    
    {atcmd_blescanrspdata, "AT+BLESCANRSPDATA", "Set/Query scan response data\r\n"
                                              "AT+BLESCANRSPDATA=<scan_rsp_data>\r\n"},
    
    {atcmd_bleadvgen,     "AT+BLEADVGEN",     "Generate advertising data from UUIDs\r\n"
                                              "AT+BLEADVGEN=<adv_id>,<count>,<uuid1>,...\r\n"},
    
    {atcmd_bleadvstart,   "AT+BLEADVSTART",   "Start advertising\r\n"
                                              "AT+BLEADVSTART=<adv_id>\r\n"},
    
    {atcmd_bleadvstop,    "AT+BLEADVSTOP",    "Stop advertising\r\n"
                                              "AT+BLEADVSTOP=<adv_id>\r\n"},
    
    {atcmd_bleadvstate,   "AT+BLEADVSTATE",   "Get advertising state\r\n"
                                              "AT+BLEADVSTATE=<adv_id>\r\n"},
    
    // === Scanning ===
    {atcmd_blescanparam,  "AT+BLESCANPARAM",  "Set scan parameters\r\n"
                                              "AT+BLESCANPARAM=<id>,<type>,<phy>,<intv>,<win>\r\n"},
    
    {atcmd_blescanstart,  "AT+BLESCANSTART",  "Start scan\r\n"
                                              "AT+BLESCANSTART=<scan_id>\r\n"},
    
    {atcmd_blescanstop,   "AT+BLESCANSTOP",   "Stop scan\r\n"
                                              "AT+BLESCANSTOP=<scan_id>\r\n"},
    
    // === Connection Management ===
    {atcmd_bleconnect,    "AT+BLECONNECT",    "Connect to device\r\n"
                                              "AT+BLECONNECT=<type>,<addr>,<phy>,<min>,<max>,<lat>,<to>\r\n"},
    
    {atcmd_bleconncancel, "AT+BLECONNCANCEL", "Cancel ongoing connection\r\n"
                                              "AT+BLECONNCANCEL\r\n"},
    
    {atcmd_bleconnupdate, "AT+BLECONNUPDATE", "Update connection parameters\r\n"
                                              "AT+BLECONNUPDATE=<conidx>,<min>,<max>,<latency>,<timeout>\r\n"},
    
    {atcmd_bleconninfo,   "AT+BLECONNINFO",   "Get connection information\r\n"
                                              "AT+BLECONNINFO=<conidx>,<info_type>\r\n"},
    
    {atcmd_bledisconn,    "AT+BLEDISCONN",    "Disconnect connection\r\n"
                                              "AT+BLEDISCONN=<adv_id>,<reason>\r\n"},
    
    // === Security and Pairing ===
    {atcmd_bleauthreq,    "AT+BLEAUTHREQ",    "Request authentication\r\n"
                                              "AT+BLEAUTHREQ=<conidx>,<sec_lvl>\r\n"},
    
    {atcmd_blekeycfm,     "AT+BLEKEYCFM",     "Confirm pairing key\r\n"
                                              "AT+BLEKEYCFM=<conidx>,<accept>,<key>\r\n"},
    
    {atcmd_bledelbond,    "AT+BLEDELBOND",    "Delete bond information\r\n"
                                              "AT+BLEDELBOND=<addr_type>,<addr>\r\n"},
    
    {atcmd_blepairedlist, "AT+BLEPAIREDLIST", "Get paired devices list\r\n"
                                              "AT+BLEPAIREDLIST?\r\n"},
    
    // === Advanced Configuration ===
    {atcmd_blesetphy,     "AT+BLESETPHY",     "Set PHY mode\r\n"
                                              "AT+BLESETPHY=<conidx>,<rx_phy>,<tx_phy>,<opt>\r\n"},
    
    {atcmd_blemtuexch,    "AT+BLEMTUEXCH",    "Exchange MTU\r\n"
                                              "AT+BLEMTUEXCH=<conidx>,<user_lid>\r\n"},
    
    {atcmd_blegetmtu,     "AT+BLEGETMTU",     "Get peer MTU\r\n"
                                              "AT+BLEGETMTU=<conidx>\r\n"},
    
    {atcmd_bleexitlatency, "AT+BLEEXITLATENCY", "Exit low power latency\r\n"
                                              "AT+BLEEXITLATENCY=<type>\r\n"},
    
    {atcmd_bleentrylatency, "AT+BLEENTRYLATENCY", "Entry low power latency\r\n"
                                              "AT+BLEENTRYLATENCY=<type>\r\n"},
    
    {atcmd_bleconparamdis, "AT+BLECONPARAMDIS", "Disable connection parameter update\r\n"
                                              "AT+BLECONPARAMDIS=<flag>\r\n"},
    
    // === Whitelist and Advanced Advertising ===
    {atcmd_blewhitelist,  "AT+BLEWHITELIST",  "Set whitelist\r\n"
                                              "AT+BLEWHITELIST=<count>,<type1>,<addr1>,...\r\n"},
    
    {atcmd_blepersyncstart, "AT+BLEPERSYNCSTART", "Start periodic advertising sync\r\n"
                                              "AT+BLEPERSYNCSTART=<type>,<addr_type>,<addr>,<sid>,<rep>,<to>\r\n"},
    
    {atcmd_blepersyncstop, "AT+BLEPERSYNCSTOP", "Stop periodic advertising sync\r\n"
                                              "AT+BLEPERSYNCSTOP\r\n"},
    
    // === Test Mode ===
    {atcmd_bletesttx,     "AT+BLETESTTX",     "Start TX test mode\r\n"
                                              "AT+BLETESTTX=<ch>,<payload>,<len>,<phy>,<pwr>\r\n"},
    
    {atcmd_bletestrx,     "AT+BLETESTRX",     "Start RX test mode\r\n"
                                              "AT+BLETESTRX=<ch>,<mod>,<slot>,<phy>\r\n"},
    
    {atcmd_bleteststop,   "AT+BLETESTSTOP",   "Stop test mode\r\n"
                                              "AT+BLETESTSTOP\r\n"},
    
    // === Low Priority ===
    {atcmd_blesetpubaddr, "AT+BLESETPUBADDR", "Set local public address\r\n"
                                              "AT+BLESETPUBADDR=<addr>\r\n"},
    
    {atcmd_blesetconexitlatency, "AT+BLESETCONEXITLATENCY", "Set connection exit latency\r\n"
                                              "AT+BLESETCONEXITLATENCY=<flag>\r\n"},
};

/*
 ****************************************************************************************
 * Public Functions
 ****************************************************************************************
 */

/**
 * Register BLE Host AT commands
 */
// extern void bt_stack_if_init(uint8_t type);
void atcmd_ble_host_register(void)
{
    atcmd_entry_add_table(atcmd_ble_host_table, 
                         sizeof(atcmd_ble_host_table) / sizeof(atcmd_item_t));
    CLOGI("BLE Host AT commands registered");
}

/**
 * Display BLE Host AT command help
 */
void atcmd_ble_host_help(void)
{
    int item_len = sizeof(atcmd_ble_host_table) / sizeof(atcmd_item_t);
    CLOGI("\n=== BLE Host AT Commands ===");
    for (int i = 0; i < item_len; i++) {
        CLOGI("%s: %s", atcmd_ble_host_table[i].atcmd_entry.name, 
                        atcmd_ble_host_table[i].atcmd_entry.help);
    }
}


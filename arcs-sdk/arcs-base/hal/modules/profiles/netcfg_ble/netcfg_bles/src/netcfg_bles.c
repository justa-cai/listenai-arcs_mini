/**
****************************************************************************************
*
* @file netcfg_bles.c
*
* @brief NETCFG_BLE Service source
*
* Copyright (C) ListenAI 2020-2099
*
*
****************************************************************************************
*/

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "netcfg_ble.h"
#include "netcfg_bles.h"

#include "ble_gatt.h"
#include "ble_prf.h"


/*
 * DIS ATTRIBUTES DATABASE
 ****************************************************************************************
 */
/// netcfg_ble service database description
const ble_gatt_att16_desc_t netcfg_ble_att_db[NETCFG_BLE_IDX_NB] =
{
    /// NETCFG_BLE service Declaration
    [NETCFG_BLE_IDX_SVC]                    = {BLE_GATT_DECL_PRIMARY_SERVICE,       BLE_PROP(RD),                   0},

    /// NETCFG_BLE Data Buffer
    [NETCFG_BLE_IDX_DATA_BUFF_CHAR]         = {BLE_GATT_DECL_CHARACTERISTIC,        BLE_PROP(RD),                   0},
    [NETCFG_BLE_IDX_DATA_BUFF_VAL]          = {BLE_GATT_NETCFG_BLE_DATA_BUFF,       BLE_PROP(WR)|BLE_PROP(N),       NETCFG_BLE_DATA_MAX_LEN},
    [NETCFG_BLE_IDX_DATA_BUFF_NTF_CFG]      = {BLE_GATT_DESC_CLIENT_CHAR_CFG,       BLE_PROP(RD)|BLE_PROP(WR),      BLE_OPT(NO_OFFSET)},

    /// NETCFG_BLE Status
    [NETCFG_BLE_IDX_NETCFG_BLE_STATS_CHAR]  = {BLE_GATT_DECL_CHARACTERISTIC,        BLE_PROP(RD),                   0},
    [NETCFG_BLE_IDX_NETCFG_BLE_STATS_VAL]   = {BLE_GATT_NETCFG_BLE_STATS,           BLE_PROP(RD),                   sizeof(enum netcfg_ble_status)},
};

/*
 * LOCAL FUNCTIONS DECLARATION
 ****************************************************************************************
 */

static uint16_t netcfg_bles_set_cb(uint8_t conidx, uint8_t att_idx, uint16_t op, uint8_t *p_value);
static void netcfg_bles_cb_event_sent(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status);
static void netcfg_bles_cb_att_read_get(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset, uint16_t max_length);
static void netcfg_bles_cb_att_val_set(uint8_t conidx, uint8_t user_lid, uint16_t token,uint16_t hdl, uint16_t offset, void* p_data);

/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */
static netcfg_bles_env_t netcfg_bles_env;

/// Message callback handle from APP
const netcfg_bles_cb_t netcfg_bles_default_cb =
{
    .cb_value_set = netcfg_bles_set_cb,
};

static const ble_gatt_srv_cb_t netcfg_bles_cb =
{
    .cb_event_sent    = netcfg_bles_cb_event_sent,
    .cb_att_read_get  = netcfg_bles_cb_att_read_get,
    .cb_att_event_get = NULL,
    .cb_att_info_get  = NULL,
    .cb_att_val_set   = netcfg_bles_cb_att_val_set,
};

/*
 * LOCAL FUNCTIONS
 ****************************************************************************************
 */
static uint16_t netcfg_bles_send_notify(uint8_t conidx, uint16_t item, uint16_t status)
{
    uint8_t data[2] = {0};
    CLOGD("netcfg_bles_send_notify");
//    memcpy(data, &item, sizeof(uint16_t));
    memcpy(data, &status, sizeof(uint16_t));
    // send notify
    return ble_gatt_srv_event_send(conidx, netcfg_bles_env.user_lid, 0,
                            BLE_GATT_NOTIFY, netcfg_bles_env.start_hdl + NETCFG_BLE_IDX_DATA_BUFF_VAL, data, sizeof(data));
}

static void netcfg_bles_cb_event_sent(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
}

static uint16_t netcfg_bles_set_cb(uint8_t conidx, uint8_t att_idx, uint16_t op, uint8_t *p_value)
{
	///default call back.
}

#if NETCFG_BLE_DBG
static int netcfg_ble_dump_data(uint8_t *data, uint16_t len)
{
    uint16_t i;

    NETCFG_BLE_PRINT("Data len is %u, val is:\n", len);
    for (i = 0; i < len; i++) {
        NETCFG_BLE_PRINT("%02x ", data[i]);
        if (!((i + 1) % 6))
            NETCFG_BLE_PRINT("\n");
            }

    if ((i % 6))
        NETCFG_BLE_PRINT("\n");

    return 0;
}
#endif

uint16_t ble_netcfg_bles_send_notify_custom_data(uint8_t conidx, uint16_t length, uint8_t *value)
{
    // CLOGI("netcfg_bles_send_notify_custom_data, length: %d", length);

    if (length == 0 || value == NULL) {
        CLOGE("Invalid parameters, length: %d, value: %p", length, value);
        return 1;
    }
    
    // 按照 ls_netcfg_ble_data_frame_t 分包发送
    const uint16_t max_data_per_packet = NETCFG_BLE_PAYLOAD_LEN_MAX - sizeof(ls_netcfg_ble_cmd_t);
    const uint16_t total_length = length;
    const uint16_t total_packets = ((total_length + max_data_per_packet - 1) / max_data_per_packet);  // 计算总包数
    uint16_t offset = 0;
    while (length > 0) {
        uint16_t chunk_size = (length > max_data_per_packet) ? max_data_per_packet : length;
        ls_netcfg_ble_cmd_t cmd;
        cmd.raw_index = (offset / max_data_per_packet) + 1;
        cmd.raw_count = total_packets;
        cmd.raw_length = chunk_size;
        // send notify
        uint8_t notify_data[NETCFG_BLE_PAYLOAD_LEN_MAX] = {0};
        memcpy(notify_data, &cmd, sizeof(ls_netcfg_ble_cmd_t));
        memcpy(notify_data + sizeof(ls_netcfg_ble_cmd_t), value + offset, chunk_size);
        ble_gatt_srv_event_send(conidx, netcfg_bles_env.user_lid, 0,
                                BLE_GATT_NOTIFY, netcfg_bles_env.start_hdl + NETCFG_BLE_IDX_DATA_BUFF_VAL,
                                notify_data, sizeof(ls_netcfg_ble_cmd_t) + chunk_size);
        offset += chunk_size;
        length -= chunk_size;
    }
    return 0;
}

static uint16_t netcfg_ble_start(void)
{
    /* Notify wifi module to start config */

    return NETCFG_BLE_READY;
}
static uint16_t netcfg_bles_get_opcode(uint8_t *data, enum netcfg_ble_status stat)
{
    ls_netcfg_ble_cmd_head *phead = NULL;
    ls_netcfg_ble_cmd_t *cmd = NULL;

    cmd = (ls_netcfg_ble_cmd_t *)data;

    if (cmd->raw_index == 1) {
        phead = (ls_netcfg_ble_cmd_head *)(cmd->data);
        return phead->opcode;
    }

    if (stat == NETCFG_BLE_SSID)
        return NETCFG_BLE_OP_SSID;

    if (stat == NETCFG_BLE_PWD)
        return NETCFG_BLE_OP_PWD;

    return NETCFG_BLE_OP_MAX;
}

/* Note that index from APP always bigger than 0 */
static int netcfg_bles_asm_data(ls_netcfg_ble_cmd_t *cmd, uint8_t *dst)
{
    uint8_t index;
    int len = 0;
    int return_len = 0;

    index = cmd->raw_index;
    if (cmd->raw_index == 1) {
        ls_netcfg_ble_cmd_head *phead = (ls_netcfg_ble_cmd_head *)(cmd->data);
        int actual_data_len = cmd->raw_length - sizeof(ls_netcfg_ble_cmd_head);
#if NETCFG_BLE_DBG
        NETCFG_BLE_LOGD("phead->len: %d, actual_data_len: %d, raw_length: %d", 
                       phead->len, actual_data_len, cmd->raw_length);
#endif
        memcpy(dst, cmd->data + sizeof(ls_netcfg_ble_cmd_head), actual_data_len);
        return_len = actual_data_len;
    } else {
        size_t dst_len = strlen((char*)dst);
        size_t max_dst_size = NETCFG_BLE_SSID_MAX_LEN;
        size_t remaining = max_dst_size - dst_len - 1; // 为null终止符留空间
        size_t copy_len = (cmd->raw_length < remaining) ? cmd->raw_length : remaining;
        
        if (copy_len > 0) {
            strncat((char*)dst, (char*)cmd->data, copy_len);
        }
        return_len = strlen((char*)dst);
    }
    
    return return_len;
}

static void netcfg_bles_cb_att_val_set(uint8_t conidx, uint8_t user_lid, uint16_t token,
                                        uint16_t hdl, uint16_t offset, void* p_data)
{
    uint16_t  status      = BLE_GAP_ERR_NO_ERROR;
    uint8_t att_idx       = hdl - netcfg_bles_env.start_hdl;
    uint16_t length = ble_co_buf_data_len(p_data);
    uint8_t *buff = ble_co_buf_data(p_data);
    uint16_t opcode = NETCFG_BLE_OP_MAX;
    ls_netcfg_ble_cmd_t *cmd = NULL;
    enum netcfg_ble_status notify_stat;

    switch(att_idx) {
    case NETCFG_BLE_IDX_DATA_BUFF_NTF_CFG:
        uint16_t cfg = ble_co_read16p(buff);
        if(cfg <= BLE_PRF_CLI_START_NTF) {
            netcfg_bles_env.ntf_cfg[conidx] = cfg;
        } else {
            status = BLE_PRF_ERR_INVALID_PARAM;
        }
        break;
    case NETCFG_BLE_IDX_DATA_BUFF_VAL:
#if NETCFG_BLE_DBG
        NETCFG_BLE_LOGD("[%s] Get att op, att_idx is %d \n", __func__, att_idx);
        netcfg_ble_dump_data(buff, length);
#endif
        cmd = (ls_netcfg_ble_cmd_t *)buff;
        opcode = netcfg_bles_get_opcode(buff, netcfg_bles_env.state);
        switch (opcode) {
        case NETCFG_BLE_OP_START:
#if NETCFG_BLE_DBG
            NETCFG_BLE_LOGD("Get OP NETCFG_BLE_OP_START, netcfg_bles_env.state: %d\n", netcfg_bles_env.state);
#endif
            if (netcfg_ble_start() == NETCFG_BLE_READY) {
                memset(netcfg_bles_env.data.ssid, 0, sizeof(netcfg_bles_env.data.ssid));
                memset(netcfg_bles_env.data.pwd, 0, sizeof(netcfg_bles_env.data.pwd));
                notify_stat = netcfg_bles_env.state = NETCFG_BLE_INPROCESS;
            } else {
                notify_stat = netcfg_bles_env.state = NETCFG_BLE_ERR;
            }
            break;
        case NETCFG_BLE_OP_SSID:
#if NETCFG_BLE_DBG
            NETCFG_BLE_LOGD("Get OP NETCFG_BLE_OP_SSID\n");
#endif
            if ((netcfg_bles_env.state == NETCFG_BLE_INPROCESS) || (netcfg_bles_env.state == NETCFG_BLE_SSID)) {
                int ssid_len = netcfg_bles_asm_data(cmd, netcfg_bles_env.data.ssid);
                NETCFG_BLE_LOGD("netcfg_bles_asm_data received complete ssid: %s, ssid_len: %d", netcfg_bles_env.data.ssid, ssid_len);
                if (cmd->raw_index == cmd->raw_count) {
                    if (ssid_len < sizeof(netcfg_bles_env.data.ssid)) {
                        netcfg_bles_env.data.ssid[ssid_len] = '\0';
                    } else {
                        netcfg_bles_env.data.ssid[sizeof(netcfg_bles_env.data.ssid) - 1] = '\0';
                    }
                    NETCFG_BLE_LOGD("received complete ssid: %s", netcfg_bles_env.data.ssid);
                    netcfg_bles_env.state = NETCFG_BLE_PWD;
                    notify_stat = NETCFG_BLE_INPROCESS;
                } else {
                    NETCFG_BLE_LOGD("received incomplete ssid, wait next, cur ssid: %s", netcfg_bles_env.data.ssid);
                    notify_stat = NETCFG_BLE_INPROCESS;
                    netcfg_bles_env.state = NETCFG_BLE_SSID;
                }
            } else {
                notify_stat = netcfg_bles_env.state = NETCFG_BLE_ERR;
            }
            if ((netcfg_bles_env.p_cb != NULL) && (netcfg_bles_env.p_cb->cb_value_set != NULL)) {
                netcfg_bles_env.p_cb->cb_value_set(conidx, att_idx, opcode, NULL);
            }

            break;
        case NETCFG_BLE_OP_PWD:
#if NETCFG_BLE_DBG
            NETCFG_BLE_LOGD("Get OP NETCFG_BLE_OP_PWD\n");
#endif
            if (netcfg_bles_env.state == NETCFG_BLE_PWD) {
                int pwd_len = netcfg_bles_asm_data(cmd, netcfg_bles_env.data.pwd);
                if (cmd->raw_index == cmd->raw_count) {
                    if (pwd_len < sizeof(netcfg_bles_env.data.pwd)) {
                        netcfg_bles_env.data.pwd[pwd_len] = '\0';
                    } else {
                        netcfg_bles_env.data.pwd[sizeof(netcfg_bles_env.data.pwd) - 1] = '\0';
                    }
                    notify_stat = netcfg_bles_env.state = NETCFG_BLE_CERT_READY;
                } else {
                    notify_stat = NETCFG_BLE_INPROCESS;
                    netcfg_bles_env.state = NETCFG_BLE_PWD;
                }
            } else {
                NETCFG_BLE_LOGD("cur state: %d", netcfg_bles_env.state);
                notify_stat = netcfg_bles_env.state = NETCFG_BLE_ERR;
            }
            if ((netcfg_bles_env.p_cb != NULL) && (netcfg_bles_env.p_cb->cb_value_set != NULL)) {
                netcfg_bles_env.p_cb->cb_value_set(conidx, att_idx, opcode, NULL);
            }
            break;
        case NETCFG_BLE_OP_DONE:
#if NETCFG_BLE_DBG
            NETCFG_BLE_LOGD("Get OP NETCFG_BLE_OP_DONE\n");
#endif
            if (netcfg_bles_env.state != NETCFG_BLE_CERT_READY) {
                NETCFG_BLE_LOGD("netcfg_bles_env.state != NETCFG_BLE_CERT_READY, state: 0x%X\n", netcfg_bles_env.state);
                notify_stat = netcfg_bles_env.state = NETCFG_BLE_ERR;
            } else {
                if ((netcfg_bles_env.p_cb != NULL) && (netcfg_bles_env.p_cb->cb_value_set != NULL)) {
                    notify_stat = netcfg_bles_env.state =
                        netcfg_bles_env.p_cb->cb_value_set(conidx, att_idx, opcode, (uint8_t *)&(netcfg_bles_env.data));
                }
            }
            break;
        case NETCFG_BLE_OP_REBOOT:
            if ((netcfg_bles_env.p_cb != NULL) && (netcfg_bles_env.p_cb->cb_value_set != NULL)) {
                netcfg_bles_env.p_cb->cb_value_set(conidx, att_idx, opcode, NULL);
            }
            notify_stat = netcfg_bles_env.state = NETCFG_BLE_REBOOTING;
            break;
        case NETCFG_BLE_AUTH_INFO:
            NETCFG_BLE_LOGD("Get OP NETCFG_BLE_AUTH_INFO\n");
            if ((netcfg_bles_env.p_cb != NULL) && (netcfg_bles_env.p_cb->cb_value_set != NULL)) {
                netcfg_bles_env.p_cb->cb_value_set(conidx, att_idx, opcode, NULL);
            }
            notify_stat = netcfg_bles_env.state;
            break;
        default:
            NETCFG_BLE_LOGD("Unknown OP code 0x%x or stat error 0x%x\n", opcode, netcfg_bles_env.state);
            notify_stat = netcfg_bles_env.state = NETCFG_BLE_ERR;
            break;
        }
        break;
    defalut:
        break;
    }

    ble_gatt_srv_att_val_set_cfm(conidx, user_lid, token, status);
}

/**
 ****************************************************************************************
 * @brief This function is called when peer want to read local attribute database value.
 *
 *        @see gatt_srv_att_read_get_cfm shall be called to provide attribute value
 *
 * @param[in] conidx        Connection index
 * @param[in] user_lid      GATT user local identifier
 * @param[in] token         Procedure token that must be returned in confirmation function
 * @param[in] hdl           Attribute handle
 * @param[in] offset        Data offset
 * @param[in] max_length    Maximum data length to return
 ****************************************************************************************
 */
static void netcfg_bles_cb_att_read_get(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset, uint16_t max_length)
{
    uint16_t status      = BLE_GAP_ERR_NO_ERROR;
    uint32_t value;
    uint16_t length       = 0;
    uint8_t att_idx       = hdl - netcfg_bles_env.start_hdl;
    enum netcfg_ble_status netcfg_stat = NETCFG_BLE_READY;

    switch (att_idx) {
    case NETCFG_BLE_IDX_DATA_BUFF_NTF_CFG:
        value = (netcfg_bles_env.ntf_cfg[conidx] != 0)
                ? BLE_PRF_CLI_START_NTF : BLE_PRF_CLI_STOP_NTFIND;
        length = sizeof(uint16_t);
        status = ble_gatt_srv_att_read_get_cfm(conidx, user_lid, token, status, length, length, (uint8_t*)&value);
        NETCFG_BLE_LOGD("netcfg_bles_cb_att_read_get NETCFG_BLE_IDX_DATA_BUFF_NTF_CFG, status: %d", status);
        break;
    case NETCFG_BLE_IDX_NETCFG_BLE_STATS_VAL:
        status = ble_gatt_srv_att_read_get_cfm(conidx, user_lid, token, status,
                            sizeof(netcfg_stat), sizeof(netcfg_stat), (uint8_t*)&netcfg_stat);
        NETCFG_BLE_LOGD("NETCFG_BLE_IDX_NETCFG_BLE_STATS_VAL, status: %d", status);
        break;
    default:
        status = BLE_PRF_ERR_INVALID_PARAM;
        break;
    }
}

/*
 * GLOBAL FUNCTIONS DEFINITIONS
 ****************************************************************************************
 */
 
uint16_t netcfg_bles_get_state(void)
{
    return netcfg_bles_env.state;
}

void netcfg_bles_set_state(uint16_t state)
{
    netcfg_bles_env.state = state;
}

uint16_t netcfg_bles_init(uint8_t sec_lvl, uint8_t user_prio, netcfg_bles_cb_t *p_cb)
{
    uint16_t status = BLE_GAP_ERR_NO_ERROR;
    uint8_t user_lid = BLE_GATT_INVALID_USER_LID;
    uint16_t start_hdl = 0;

    memset(&netcfg_bles_env, 0, sizeof(netcfg_bles_env));

    if(p_cb != NULL)
    {
        netcfg_bles_env.p_cb = p_cb;
    }
    else
    {
        netcfg_bles_env.p_cb = &netcfg_bles_default_cb;
    }
    do {
        status = ble_gatt_user_register(NETCFG_BLE_DATA_MAX_LEN, user_prio, &netcfg_bles_cb, &user_lid);
        if(status != BLE_GAP_ERR_NO_ERROR)
            break;

        status = ble_gatt_db_svc16_add(user_lid, sec_lvl, BLE_GATT_NETCFG_BLE_SERVICE, NETCFG_BLE_IDX_NB,
                                   NULL, &(netcfg_ble_att_db[0]), NETCFG_BLE_IDX_NB, &start_hdl);
        if(status != BLE_GAP_ERR_NO_ERROR)
            break;

        netcfg_bles_env.start_hdl = start_hdl;
        netcfg_bles_env.user_lid = user_lid;
        netcfg_bles_env.state = NETCFG_BLE_IDLE;

    } while(0);

    if((status != BLE_GAP_ERR_NO_ERROR) && (user_lid != BLE_GATT_INVALID_USER_LID))
        ble_gatt_user_unregister(user_lid);

    return (status);
}


uint16_t ble_netcfg_bles_init(netcfg_bles_cb_t *p_cb)
{
    netcfg_bles_init(0, 0, p_cb);
}

uint16_t ble_netcfg_bles_send_notify(uint8_t conidx, uint16_t item, uint16_t status)
{   
    if(status == 0)
    {
        status = netcfg_bles_get_state();
    }
    return netcfg_bles_send_notify(conidx, item, status);
}


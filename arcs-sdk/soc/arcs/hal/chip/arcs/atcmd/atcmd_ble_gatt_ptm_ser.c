/**
****************************************************************************************
*
* @file atcmd_ble_gatt_ptm_ser.c
*
* @brief BLE Production Test Mode (PTM) GATT Service source
*
* Copyright (C) ListenAI 2020-2099
*
****************************************************************************************
*/

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "ble_gatt.h"
#include "ble_prf.h"
#include "ble_task.h"
#include "log_print.h"
#include "atcmd_ble_gatt_ptm_ser.h"
#include "atcmd.h"

/*
 * DEFINES
 ****************************************************************************************
 */

/// PTM GATT UUIDs
#define BLE_GATT_PTM_DATA_CHAR         (0xA002)

#define UUID16_TO_BT128(uuid)                                     \
    {                                                             \
        (uint8_t)((uuid) & 0xFF), (uint8_t)(((uuid) >> 8) & 0xFF),\
        0x00, 0x00, 0x10, 0x00, 0x80, 0x00,                       \
        0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB, 0x00, 0x00            \
    }

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */

/// PTM Service state
enum
{
    PTM_STATE_IDLE,
    PTM_STATE_READY,
    PTM_STATE_WAIT_REBOOT,
};

/// PTM Service Attributes Index
enum
{
    PTM_IDX_SVC,

    PTM_IDX_DATA_CHAR,
    PTM_IDX_DATA_VAL,
    PTM_IDX_DATA_CFG,

    PTM_IDX_NB,
};

typedef struct
{
    uint8_t state;
    uint8_t user_lid;
    uint16_t start_hdl;
    uint16_t ntf_cfg[BLE_CONNECTION_MAX];
    ptm_data_rx_cb_t rx_cb;
} ptm_env_t;

/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */

static const uint8_t BLE_GATT_PTM_PRIMARY_SERVICE_UUID[BLE_GATT_UUID_128_LEN] =
{
    0x12, 0xA2, 0x4D, 0x2E, 0xFE, 0x14, 0x48, 0x8E,
    0x93, 0xD2, 0x17, 0x3C, 0xFF, 0xD1, 0x00, 0x00,
};

static const ble_gatt_att_desc_t ptm_att_db[PTM_IDX_NB] =
{
    [PTM_IDX_SVC] =
    {
        .uuid = UUID16_TO_BT128(BLE_GATT_DECL_PRIMARY_SERVICE),
        .info = BLE_PROP(RD),
        .ext_info = 0,
    },

    [PTM_IDX_DATA_CHAR] =
    {
        .uuid = UUID16_TO_BT128(BLE_GATT_DECL_CHARACTERISTIC),
        .info = BLE_PROP(RD),
        .ext_info = 0,
    },

    [PTM_IDX_DATA_VAL] =
    {
        .uuid = UUID16_TO_BT128(BLE_GATT_PTM_DATA_CHAR),
        .info = BLE_PROP(WC) | BLE_PROP(N),
        .ext_info = PTM_DATA_MAX_LEN,
    },

    [PTM_IDX_DATA_CFG] =
    {
        .uuid = UUID16_TO_BT128(BLE_GATT_DESC_CLIENT_CHAR_CFG),
        .info = BLE_PROP(RD) | BLE_PROP(WR),
        .ext_info = BLE_OPT(NO_OFFSET),
    },
};

static ptm_env_t ptm_env;

/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */

static inline bool ptm_conidx_valid(uint8_t conidx)
{
    return (conidx < BLE_CONNECTION_MAX);
}

static inline uint16_t ptm_char_handle_get(void)
{
    return (ptm_env.start_hdl + PTM_IDX_DATA_VAL);
}

static void ptm_cb_event_sent(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG,
               "ptm_cb_event_sent: conidx=%d, status=%d", conidx, status);
    (void)user_lid;
    (void)dummy;
}

static void ptm_cb_att_event_get(uint8_t conidx, uint8_t user_lid, uint16_t token,
                                 uint16_t dummy, uint16_t hdl, uint16_t max_length)
{
    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG,
               "ptm_cb_att_event_get not implemented (conidx=%d, hdl=%d)", conidx, hdl);
    (void)user_lid;
    (void)token;
    (void)dummy;
    (void)max_length;
}

static void ptm_cb_att_info_get(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl)
{
    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG,
               "ptm_cb_att_info_get not implemented (conidx=%d, hdl=%d)", conidx, hdl);
    (void)user_lid;
    (void)token;
}

static void ptm_cb_att_val_set(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl,
                               uint16_t offset, void* p_data)
{
    uint16_t status = BLE_GAP_ERR_NO_ERROR;
    uint8_t att_idx = hdl - ptm_env.start_hdl;
    uint16_t length = ble_co_buf_data_len(p_data);
    uint8_t* buff = ble_co_buf_data(p_data);

    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG,
               "ptm_cb_att_val_set: conidx=%d, hdl=%d, att_idx=%d, len=%d",
               conidx, hdl, att_idx, length);

    switch(att_idx)
    {
        case PTM_IDX_DATA_CFG:
        {
            if (length < sizeof(uint16_t))
            {
                status = BLE_PRF_ERR_INVALID_PARAM;
                break;
            }

            uint16_t cfg = ble_co_read16p(buff);
            if ((cfg == BLE_PRF_CLI_STOP_NTFIND) || (cfg == BLE_PRF_CLI_START_NTF))
            {
                if (ptm_conidx_valid(conidx))
                {
                    ptm_env.ntf_cfg[conidx] = cfg;
                }
            }
            else
            {
                status = BLE_PRF_ERR_INVALID_PARAM;
            }
        } break;

        case PTM_IDX_DATA_VAL:
        {
            if (ptm_env.rx_cb != NULL)
            {
                ptm_env.rx_cb(conidx, buff, length);
            }
        } break;

        default:
        {
            status = BLE_PRF_ERR_INVALID_PARAM;
        } break;
    }

    ble_gatt_srv_att_val_set_cfm(conidx, user_lid, token, status);
    (void)offset;
}

static void ptm_cb_att_read_get(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl,
                                uint16_t offset, uint16_t max_length)
{
    uint16_t status = BLE_GAP_ERR_NO_ERROR;
    uint8_t att_idx = hdl - ptm_env.start_hdl;

    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG,
               "ptm_cb_att_read_get: conidx=%d, hdl=%d, att_idx=%d",
               conidx, hdl, att_idx);

    switch(att_idx)
    {
        case PTM_IDX_DATA_CFG:
        {
            uint16_t value = BLE_PRF_CLI_STOP_NTFIND;
            if (ptm_conidx_valid(conidx) && (ptm_env.ntf_cfg[conidx] == BLE_PRF_CLI_START_NTF))
            {
                value = BLE_PRF_CLI_START_NTF;
            }
            ble_gatt_srv_att_read_get_cfm(conidx, user_lid, token, status,
                                          sizeof(uint16_t), sizeof(uint16_t),
                                          (uint8_t*)&value);
        } break;

        default:
        {
            status = BLE_PRF_ERR_INVALID_PARAM;
            ble_gatt_srv_att_read_get_cfm(conidx, user_lid, token, status,
                                          0, 0, NULL);
        } break;
    }

    (void)offset;
    (void)max_length;
}

static const ble_gatt_srv_cb_t ptm_cb =
{
    .cb_event_sent    = ptm_cb_event_sent,
    .cb_att_read_get  = ptm_cb_att_read_get,
    .cb_att_event_get = ptm_cb_att_event_get,
    .cb_att_info_get  = ptm_cb_att_info_get,
    .cb_att_val_set   = ptm_cb_att_val_set,
};

/*
 * GLOBAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */

uint16_t ptm_init(uint8_t sec_lvl, uint8_t user_prio)
{
    uint16_t status = BLE_GAP_ERR_NO_ERROR;
    uint8_t user_lid = BLE_GATT_INVALID_USER_LID;
    uint16_t start_hdl = 0;

    memset(&ptm_env, 0, sizeof(ptm_env));

    do
    {
        status = ble_gatt_user_register(PTM_DATA_MAX_LEN, user_prio, &ptm_cb, &user_lid);
        if (status != BLE_GAP_ERR_NO_ERROR)
        {
            break;
        }

        status = ble_gatt_db_svc_add(user_lid, sec_lvl, BLE_GATT_PTM_PRIMARY_SERVICE_UUID,
                                     PTM_IDX_NB, NULL, ptm_att_db, PTM_IDX_NB, &start_hdl);
        if (status != BLE_GAP_ERR_NO_ERROR)
        {
            break;
        }

        ptm_env.start_hdl = start_hdl;
        ptm_env.user_lid = user_lid;
        ptm_env.state = PTM_STATE_READY;
        for (uint8_t idx = 0; idx < BLE_CONNECTION_MAX; idx++)
        {
            ptm_env.ntf_cfg[idx] = BLE_PRF_CLI_STOP_NTFIND;
        }
    } while(0);

    if ((status != BLE_GAP_ERR_NO_ERROR) && (user_lid != BLE_GATT_INVALID_USER_LID))
    {
        ble_gatt_user_unregister(user_lid);
    }

    AT_DBG_MSG(AT_LOG_FLAG_BLE,
               (status == BLE_GAP_ERR_NO_ERROR) ? AT_LOG_LEVEL_INFO : AT_LOG_LEVEL_ERROR,
               "ptm_init status=%d, start_hdl=0x%04X", status, ptm_env.start_hdl);

    return status;
}

uint16_t ptm_send_notify(uint8_t conidx, const uint8_t *data, uint16_t length)
{
    if (!ptm_conidx_valid(conidx) || (data == NULL) || (length == 0))
    {
        return BLE_PRF_ERR_INVALID_PARAM;
    }

    if (ptm_env.user_lid == BLE_GATT_INVALID_USER_LID)
    {
        return -1;
    }

    if (ptm_env.ntf_cfg[conidx] != BLE_PRF_CLI_START_NTF)
    {
        return BLE_PRF_ERR_REQ_DISALLOWED;
    }

    return ble_gatt_srv_event_send(conidx, ptm_env.user_lid, 0,
                                   BLE_GATT_NOTIFY, ptm_char_handle_get(),
                                   data, length);
}

void ptm_register_rx_callback(ptm_data_rx_cb_t cb)
{
    ptm_env.rx_cb = cb;
}

bool ptm_is_notify_enabled(uint8_t conidx)
{
    if (!ptm_conidx_valid(conidx))
    {
        return false;
    }
    return (ptm_env.ntf_cfg[conidx] == BLE_PRF_CLI_START_NTF);
}


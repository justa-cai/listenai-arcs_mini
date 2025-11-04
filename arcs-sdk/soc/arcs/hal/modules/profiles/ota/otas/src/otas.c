/**
****************************************************************************************
*
* @file otas.c
*
* @brief BLE OTA Service source
*
* Copyright (C) ListenAI 2020-2099
*
*
****************************************************************************************
*/

/*
 * MACROS
 ****************************************************************************************
 */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "ota.h"
#include "otas.h"

#include "ble_gatt.h"
#include "ble_prf.h"

/*
 * DEFINES
 ****************************************************************************************
 */

/// OTA BLE GATT UUIDs
#define BLE_GATT_OTA_SERVICE        (0xfc20)
#define BLE_GATT_OTA_DATA_BUFF      (0xfc21)
#define BLE_GATT_OTA_VERSION        (0xfc22)

#define min(x,y)    ((x)>(y)?(y):(x))

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */
/// OTA Service state
enum
{
    OTAS_IDEL,
    OTAS_READY,
    OTAS_WAIT_REBOOT,
};

/// OTA Service Attributes Index
enum
{
    /// service
    OTA_IDX_SVC,

    /// data buffer
    OTA_IDX_DATA_BUFF_CHAR,
    OTA_IDX_DATA_BUFF_VAL,
    OTA_IDX_DATA_BUFF_NTF_CFG,

    /// OTA Version
    OTA_IDX_OTA_VERSION_CHAR,
    OTA_IDX_OTA_VERSION_VAL,

    OTA_IDX_NB,
};


/// ota service environment variable
typedef struct otas_env
{
    /// service state
    uint8_t state;

    /// GATT user local identifier
    uint8_t user_lid;

    /// HIDS Start Handles
    uint16_t start_hdl;

    /// Notification configuration
    uint16_t ntf_cfg[BLE_CONNECTION_MAX];

    /// ota data buffer
    uint32_t *buff;

} otas_env_t;


/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */

/// ota service database description
const ble_gatt_att16_desc_t ota_att_db[OTA_IDX_NB] =
{
    /// OTA service Declaration
    [OTA_IDX_SVC]                              = {BLE_GATT_DECL_PRIMARY_SERVICE,       BLE_PROP(RD),                           0                                           },

    /// OTA Data Buffer
    [OTA_IDX_DATA_BUFF_CHAR]                   = {BLE_GATT_DECL_CHARACTERISTIC,        BLE_PROP(RD),                           0                                           },
    [OTA_IDX_DATA_BUFF_VAL]                    = {BLE_GATT_OTA_DATA_BUFF,              BLE_PROP(WR)|BLE_PROP(N),               OTA_DATA_MAX_LEN                            },
    [OTA_IDX_DATA_BUFF_NTF_CFG]                = {BLE_GATT_DESC_CLIENT_CHAR_CFG,       BLE_PROP(RD)|BLE_PROP(WR),              BLE_OPT(NO_OFFSET)                          },

    /// OTA Version
    [OTA_IDX_OTA_VERSION_CHAR]                 = {BLE_GATT_DECL_CHARACTERISTIC,        BLE_PROP(RD),                           0                                           },
    [OTA_IDX_OTA_VERSION_VAL]                  = {BLE_GATT_OTA_VERSION,                BLE_PROP(RD),                           sizeof(ls_ota_ver_t)                        },

};


/// ota service environment
static otas_env_t otas_env;

/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */
static void otas_send_notify(uint8_t conidx, uint16_t result, uint16_t index)
{
    uint8_t data[4];

    data[0] = result;
    data[1] = 0;
    data[2] = index&0xff;
    data[3] = index>>8;
    // send notify
    ble_gatt_srv_event_send(conidx, otas_env.user_lid, 0,
                            BLE_GATT_NOTIFY, otas_env.start_hdl+OTA_IDX_DATA_BUFF_VAL, data, 4);
}


static void otas_cb_event_sent(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{

}

/**
 ****************************************************************************************
 * @brief This function is called during a write procedure to modify attribute handle.
 *
 *        @see gatt_srv_att_val_set_cfm shall be called to accept or reject attribute
 *        update.
 *
 * @param[in] conidx        Connection index
 * @param[in] user_lid      GATT user local identifier
 * @param[in] token         Procedure token that must be returned in confirmation function
 * @param[in] hdl           Attribute handle
 * @param[in] offset        Value offset
 * @param[in] p_data        Pointer to buffer that contains data to write starting from offset
 ****************************************************************************************
 */
static void otas_cb_att_val_set(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset, void* p_data)
{
    uint16_t  status      = BLE_GAP_ERR_NO_ERROR;
    uint8_t att_idx       = hdl - otas_env.start_hdl;

    uint16_t length = ble_co_buf_data_len(p_data);
    uint8_t* buff = ble_co_buf_data(p_data);
    uint8_t  result = OTA_SUCCESS;
    ls_ota_cmd_t cmd;

    switch(att_idx)
    {
    case OTA_IDX_DATA_BUFF_NTF_CFG:
        {
            uint16_t cfg = ble_co_read16p(buff);
            if(cfg <= BLE_PRF_CLI_START_NTF)
            {
                otas_env.ntf_cfg[conidx] = cfg;
            }
            else
                status = BLE_PRF_ERR_INVALID_PARAM;
        }
        break;
    case OTA_IDX_DATA_BUFF_VAL:
        {
            memcpy(&cmd, buff, min(sizeof(ls_ota_cmd_t), length));

            switch(cmd.opcode)
            {
            case OTA_UPDATE_NAME:
                break;
            case OTA_UPDATE_BDADDR:
                break;
            case OTA_REBOOT:
                {
                    ble_gap_disconnect(conidx, 0x13); // REMOTE USER TERMINATED CONNECTION
                    otas_env.state = OTAS_WAIT_REBOOT;
                }
                break;
            case OTA_WRITE_DATA:
                if(otas_env.buff == NULL){
                    otas_env.buff = plf_malloc((OTA_DATA_MAX_LEN + 4));
                }
                cmd.data.data = otas_env.buff;
                memcpy(cmd.data.data, buff + ((uint8_t*)(&cmd.data.length) - (uint8_t*)(&cmd)) + sizeof(uint16_t), cmd.data.length);
                /// no break
            default:
                result = ota_process_command(&cmd);
                break;
            }
        }
        break;
    }

    ble_gatt_srv_att_val_set_cfm(conidx, user_lid, token, status);

    if(att_idx == OTA_IDX_DATA_BUFF_VAL)
    {
        if(cmd.opcode == OTA_WRITE_DATA)
            otas_send_notify(conidx, result, cmd.data.index);
        else
            otas_send_notify(conidx, result, 0);
    }
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
static void otas_cb_att_read_get(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset, uint16_t max_length)
{
    uint16_t  status      = BLE_GAP_ERR_NO_ERROR;
    uint32_t value;
    uint16_t length       = 0;
    uint8_t att_idx       = hdl - otas_env.start_hdl;

    switch(att_idx)
    {
    case OTA_IDX_DATA_BUFF_NTF_CFG:
        value = (otas_env.ntf_cfg[conidx] != 0)
                ? BLE_PRF_CLI_START_NTF : BLE_PRF_CLI_STOP_NTFIND;
        length = sizeof(uint16_t);
        status = ble_gatt_srv_att_read_get_cfm(conidx, user_lid, token, status, length, length, (uint8_t*)&value);
        break;
    case OTA_IDX_OTA_VERSION_VAL:
        {
            const ls_ota_ver_t *ver = ota_get_current_version(OTA_ZONE_ID_AP);
            if (ver) {
                status = ble_gatt_srv_att_read_get_cfm(conidx, user_lid, token, status, sizeof(ls_ota_ver_t), sizeof(ls_ota_ver_t), (uint8_t*)ver);
            } else {
                CLOGD("Get Null ver from zone[%d]", __func__, OTA_ZONE_ID_AP);
            }
        }
        break;
    default:
        {
            status = BLE_PRF_ERR_INVALID_PARAM;
        }
        break;
    }
}

/// Service callback hander from GATT
static const ble_gatt_srv_cb_t otas_cb =
{
    .cb_event_sent    = otas_cb_event_sent,
    .cb_att_read_get  = otas_cb_att_read_get,
    .cb_att_event_get = NULL,
    .cb_att_info_get  = NULL,
    .cb_att_val_set   = otas_cb_att_val_set,
};

/*
 * GLOBAL FUNCTIONS DEFINITIONS
 ****************************************************************************************
 */
uint16_t otas_init(uint8_t sec_lvl, uint8_t user_prio)
{
    uint16_t status = BLE_GAP_ERR_NO_ERROR;

    uint8_t user_lid = BLE_GATT_INVALID_USER_LID;
    uint16_t start_hdl = 0;

    memset(&otas_env, 0, sizeof(otas_env));

    do
    {
        /// register OTAS user
        status = ble_gatt_user_register(OTA_DATA_MAX_LEN, user_prio, &otas_cb, &user_lid);
        if(status != BLE_GAP_ERR_NO_ERROR) break;

        // Add OTA service
        status = ble_gatt_db_svc16_add(user_lid, sec_lvl, BLE_GATT_OTA_SERVICE, OTA_IDX_NB,
                                   NULL, &(ota_att_db[0]), OTA_IDX_NB, &start_hdl);
        if(status != BLE_GAP_ERR_NO_ERROR) break;

        otas_env.start_hdl = start_hdl;
        otas_env.user_lid = user_lid;
        otas_env.state = OTAS_IDEL;

    }while(0);

    if((status != BLE_GAP_ERR_NO_ERROR) && (user_lid != BLE_GATT_INVALID_USER_LID))
    {
        ble_gatt_user_unregister(user_lid);
    }

    return (status);
}

void otas_con_cleanup(uint8_t conidx, uint16_t reason)
{
    void (*pReset)(void);

    if(otas_env.state == OTAS_WAIT_REBOOT)
    {
        // Restart FW
        //pReset = *((uint32_t * )(0x4));
        //pReset();
        HAL_PMU_Chip_Software_Reset_Enable();
    }
    else
        otas_env.state = OTAS_IDEL;
}

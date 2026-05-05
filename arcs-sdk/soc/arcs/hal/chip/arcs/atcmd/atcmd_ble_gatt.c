/**
 ****************************************************************************************
 *
 * @file atcmd_ble_gatt.c
 *
 * @brief BLE GATT Implementation
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

#include "log_print.h"
#include "nvs.h"
#include "atcmd.h"
#include "atcmd_hash.h"
//#include "plf.h"

#include "ble_task.h"
#include "ble_drv.h"
#include "ble_plf_config.h"
#include "ble_gap.h"
#include "ble_prf.h"

// #include "lisa_log.h"

#include "hogpd_msg.h"
#include "hogpd.h"
#include "bass.h"
#include "diss.h"
#include "netcfg_bles.h"
#include "atcmd_ble_gatt_tp_ser.h"
#include "atcmd_ble_gatt_ptm_ser.h"

void atcmd_bt_stack_ble_hid_rcv(uint8_t conidx, uint16_t index, uint16_t length, uint16_t offset, uint8_t *data);
static void atcmd_bt_stack_ble_hid_send_cmp(uint32_t token, uint8_t val_id);
static void  atcmd_bt_stack_ble_hid_read_cmp(uint32_t token, uint8_t val_id);
uint8_t *atcmd_bt_stack_vbat_percent_get(void);

/// Message callback handle from APP
const hogpd_cb_t atcmd_bt_stack_ble_hogpd_msg_cb =
{
    .cb_read_cmp = atcmd_bt_stack_ble_hid_read_cmp,
    .cb_notify_cmp = atcmd_bt_stack_ble_hid_send_cmp,
    .cb_write_cmp = NULL,

    .cb_read_ind = NULL,
    .cb_notify_ind = NULL,
    .cb_write_ind = atcmd_bt_stack_ble_hid_rcv,
};

/// Message callback handle from APP
static const netcfg_bles_cb_t netcfg_app_cb =
{
    // .cb_value_set = netcfg_bles_profile_set_cb,
};

const diss_cb_t atcmd_bt_stack_ble_diss_msg_cb =
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
    // //  media
    // 0x05, 0x0C,                     // USAGE_PAGE (Consumer Devices)
    // 0x09, 0x01,                     // USAGE (Consumer Control)
    // 0xA1, 0x01,                     // COLLECTION (Application)
    // 0x85, HIDS_MEDIA_REPORT_ID,     // REPORT_ID (3)
    // 0x19, 0x00,                     // USAGE_MINIMUM (0x00)
    // 0x2A, 0x9C, 0x02,               // USAGE_MAXIMUM (0x02 0xc9)
    // 0x15, 0x00,                     // LOGICAL_MINIMUM (0x00)
    // 0x26, 0x9C, 0x02,               // LOGICAL_MAXIMUM (0x02 0xc9)
    // 0x95, 0x01,                     // REPORT_COUNT (1)
    // 0x75, 0x10,                     // REPORT_SIZE (0x10)
    // 0x81, 0x00,                     //INPUT (Data,Ary,Abs)
    // 0xC0,                           //      END_COLLECTION
    // //voice data report
    // 0x05 , 0x0C,                    //      Usage Page (Consumer Devices)
    // 0x09 , 0x01,                    //    Usage (Consumer Control)
    // 0xA1 , 0x01,                    //    Collection (Application)
    // 0x85 , HIDS_VOICE_DATA_IN_REPORT_ID,                               //    Report ID=0xFC
    // 0x95 , 0xff,                    //    REPORT_COUNT (20)
    // 0x75 , 0x08,                    //    REPORT_SIZE (8)
    // 0x15 , 0x00,                    //    LOGICAL_MINIMUM (0)
    // 0x26 , 0xFF , 0x00,             //    LOGICAL_MAXIMUM (255)
    // 0x81 , 0x00,                    //    INPUT (Data,Ary,Abs)
    // 0xC0,                           //      END_COLLECTION
    // //gde ack in
    // 0x05 , 0x0C,                    //      Usage Page (Consumer Devices)
    // 0x09 , 0x01,                    //    Usage (Consumer Control)
    // 0xA1 , 0x01,                    //    Collection (Application)
    // 0x85 , HIDS_GDE_ACK_IN_REPORT_ID,//   Report ID=0xF8
    // 0x95 , 0xff,                    //    REPORT_COUNT (20)
    // 0x75 , 0x08,                    //    REPORT_SIZE (8)
    // 0x15 , 0x00,                    //    LOGICAL_MINIMUM (0)
    // 0x26 , 0xFF , 0x00,             //    LOGICAL_MAXIMUM (255)
    // 0x81 , 0x00,                    //    INPUT (Data,Ary,Abs)
    // 0xC0,                           //      END_COLLECTION

    // //gde feedback
    // 0x05 , 0x0C,                    //      Usage Page (Consumer Devices)
    // 0x09 , 0x01,                    //    Usage (Consumer Control)
    // 0xA1 , 0x01,                    //    Collection (Application)
    // 0x85 , HIDS_GDE_FEEDBACK_IN_REPORT_ID,                             //    Report ID=0xF9
    // 0x95 , 0xff,                    //    REPORT_COUNT (1)
    // 0x75 , 0x08,                    //    REPORT_SIZE (8)
    // 0x15 , 0x00,                    //    LOGICAL_MINIMUM (0)
    // 0x26 , 0xFF , 0x00,             //    LOGICAL_MAXIMUM (255)
    // 0x81 , 0x00,                    //    INPUT (Data,Ary,Abs)
    // 0xC0,                           //      END_COLLECTION
};

void atcmd_ble_gatt_init(void)
{
    ble_bass_init();
    ble_bass_enable(0, atcmd_bt_stack_vbat_percent_get());
    // //enable net config.
    // ble_netcfg_bles_init((netcfg_bles_cb_t *)&netcfg_app_cb);
    //enable diss.
    ble_diss_init((diss_cb_t *)&atcmd_bt_stack_ble_diss_msg_cb);

    // enable hid service
    uint8_t svc_features = HOGPD_CFG_KEYBOARD | HOGPD_CFG_MOUSE | HOGPD_CFG_PROTO_MODE | HOGPD_CFG_REPORT_NTF_EN;
    uint8_t report_char_cfg = HOGPD_CFG_REPORT_IN;
    hogpd_report_map_t report_map = {sizeof(hid_report_map), 0, (uint8_t *)hid_report_map};
    ble_hogpd_init(svc_features, report_char_cfg, (hogpd_cb_t*)&atcmd_bt_stack_ble_hogpd_msg_cb, &report_map);
    ble_hogpd_enable(0);

    tps_init(0, 0);
    // ptm_init(2<<5, 0);
}

void atcmd_ble_gatt_hogpd_reinit(void)
{
    /// profile reinit
    hogpd_report_map_t report_map = {sizeof(hid_report_map), 0, (uint8_t *)hid_report_map};
    hogpd_init_report_map(1, &report_map);
}

void atcmd_bt_stack_ble_hid_rcv(uint8_t conidx, uint16_t index, uint16_t length, uint16_t offset, uint8_t *data)
{
    /// send hid data to application to handle.
    //app_hid_rcv_data(conidx, index, length, offset, data);
}

uint8_t *atcmd_bt_stack_vbat_percent_get(void)
{
    static uint8_t vbat_percent = 80;
    return &vbat_percent;
}

uint8_t atcmd_bt_stack_ble_hid_send(uint8_t conidx, uint8_t report_idx, uint8_t length, uint8_t* value)
{
    // bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();
    // uint8_t status = 0;
    // uint16_t max_send_cnt = BT_STACK_BLE_HOGPD_HID_MAX_COUNT;

    // //CLOGD("hid idx:%d, len:%d", report_idx, length);

    // /// remain 5 pkt for ctrl & cmd.
    // if(report_idx == HIDS_VOICE_DATA_INDEX)
    // {
    //     max_send_cnt = BT_STACK_BLE_HOGPD_HID_MAX_COUNT - 5;
    // }
    // else
    // {
    //     max_send_cnt = BT_STACK_BLE_HOGPD_HID_MAX_COUNT;
    // }

    // if (stack_env->bt_hid_send_cnt < max_send_cnt) 
    // {
    //     status = ble_hogpd_report_upd(conidx, report_idx, length, value);
    //     if(status)
    //     {
    //         CLOGW("hid err status:%d", status);
    //     } 
    //     stack_env->bt_hid_send_cnt++;
    // } 
    // else 
    // {
    //     status = 0x01;
    //     CLOGW("OverFlow");
    // }
    // return status;
}

static void atcmd_bt_stack_ble_hid_send_cmp(uint32_t token, uint8_t val_id)
{
    // bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();

    // if(stack_env->bt_hid_send_cnt != 0)
    // {
    //     stack_env->bt_hid_send_cnt--;
    // }

    //if(stack_env->bt_hid_send_cnt == 0)
    //{
    //    CLOGD("hid cmp:%d", stack_env->bt_hid_send_cnt);
    //}
}

static void  atcmd_bt_stack_ble_hid_read_cmp(uint32_t token, uint8_t val_id)
{
    ///to do;
}

extern void tps_send_notify(uint8_t conidx, uint16_t len, uint8_t* data);
int atcmd_ble_gatt_tps_send_notify(int type, char *params)
{
    if (type == ATCMD_PARAM) {
        atcmd_rspinfor("AT+BLETPSSENDNTF=<data>");
        atcmd_rspinfor("<data>: Hex string data to send");
        return ATCMD_OK;
    }

    else if (type == ATCMD_EXEC) {
        char *token = atcmd_next_token(&params);
        if (!token) {
            atcmd_rspinfor("AT+BLETPSSENDNTF:INVALID_PARAM");
            return ATCMD_ERROR;
        }
        int len = strlen(token);
        if (len % 2 != 0 || len > 1500) {
            atcmd_rspinfor("AT+BLETPSSENDNTF:INVALID_FORMAT");
            return ATCMD_ERROR;
        }
        uint16_t data_len = len / 2;
        uint8_t data[data_len];
        for (int i = 0; i < data_len; i++) {
            sscanf(&token[i * 2], "%2hhx", &data[i]);
        }
        tps_send_notify(0, data_len, data);
        return ATCMD_OK;
    }
    else { // ATCMD_QUERY
        atcmd_rspinfor("AT+BLETPSSENDNTF:USE_QUERY");
        return ATCMD_ERROR;
    }
}   
void tps_send_indicate(uint8_t conidx, uint16_t len, uint8_t* data);
int atcmd_ble_gatt_tps_send_indicate(int type, char *params)
{
    if (type == ATCMD_PARAM) {
        atcmd_rspinfor("AT+BLETPSSENDIND=<data>");
        atcmd_rspinfor("<data>: Hex string data to send");
        return ATCMD_OK;
    }
    else if (type == ATCMD_EXEC) {
        char *token = atcmd_next_token(&params);
        if (!token) {
            atcmd_rspinfor("AT+BLETPSSENDIND:INVALID_PARAM");
            return ATCMD_ERROR;
        }
        int len = strlen(token);
        if (len % 2 != 0 || len > 1500) {
            atcmd_rspinfor("AT+BLETPSSENDIND:INVALID_FORMAT");
            return ATCMD_ERROR;
        }
        uint16_t data_len = len / 2;
        uint8_t data[data_len];
        for (int i = 0; i < data_len; i++) {
            sscanf(&token[i * 2], "%2hhx", &data[i]);
        }
        tps_send_indicate(0, data_len, data);
        return ATCMD_OK;
    }
    else { // ATCMD_QUERY
        atcmd_rspinfor("AT+BLETPSSENDIND:USE_QUERY");
        return ATCMD_ERROR;
    }
}

/*
 ****************************************************************************************
 * AT Command Table
 ****************************************************************************************
 */

static const atcmd_item_t atcmd_ble_gatt_table[] = 
{
    {atcmd_ble_gatt_tps_send_notify, "AT+BLETPSSENDNTF", "Send TPS Notification\r\n"
                                                    "AT+BLETPSSENDNTF=<data>\r\n"
                                                    "<data>: Hex string data to send\r\n",},
    {atcmd_ble_gatt_tps_send_indicate, "AT+BLETPSSENDIND", "Send TPS Indication\r\n"
                                                      "AT+BLETPSSENDIND=<data>\r\n"
                                                      "<data>: Hex string data to send\r\n",},
};

void atcmd_ble_gatt_register(void)
{
    atcmd_entry_add_table(atcmd_ble_gatt_table, 
                         sizeof(atcmd_ble_gatt_table)/sizeof(atcmd_item_t));
    CLOGI("BLE GATT AT commands registered");
}

void atcmd_ble_gatt_help(void)
{
    int item_len = sizeof(atcmd_ble_gatt_table)/sizeof(atcmd_item_t);
    CLOGI("\n=== BLE GATT AT Commands ===");
    for(int i = 0; i < item_len; i++) {
        CLOGI("%s: %s", atcmd_ble_gatt_table[i].atcmd_entry.name, 
                        atcmd_ble_gatt_table[i].atcmd_entry.help);
    }
}


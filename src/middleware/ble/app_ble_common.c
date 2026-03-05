/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "lisa_bluetooth.h"
#include "bt_app_if.h"
#include "bt_stack_if.h"
#include "netcfg_bles.h"
#include "diss.h"

#define LOG_TAG "app_ble_common"
#include <lisa_log.h>

uint8_t manufacturer_data[18] = {
    0xab, 0x0a, 0xa1, 0xdc, 0xa8, 0x76, 0x83, 0x65, 0x73, 0x83, 0x72, 0x65, 0x82, 0x67, 0x83, 0x68, 0x00, 0x78,
};

ble_gap_cfg_t user_bt_stack_dev_cfg = {
    .addr = {{0x44, 0x55, 0x66, 0x03, 0x23, 0x20}, 0},
    .name_len = sizeof(DEVICE_NAME),
    .name = DEVICE_NAME,
    .appearance = GAP_APP_GENERIC_MEDIA_PLAYER, // hid_keyboard
    .iocap = GAP_IO_CAP_NO_INPUT_NO_OUTPUT,
    .auth = GAP_SEC_NOT_ENC,
    .pairing_mode = GAPM_PAIRING_LEGACY,
};

#if (ADV_USER_DATA)

uint8_t lisa_ble_gen_user_adv_data(uint8_t *p_data)
{
    uint8_t nb_uuid = 1;

    uint16_t uuids[1] = {HID_UUID};

    // Remaining Length
    uint8_t rem_len = LEGA_ADV_DATA_LEN - 3;

    uint8_t *p_buf = p_data;
    uint8_t length = 0;

    /// add Manufacturer specific
    *p_buf++ = sizeof(manufacturer_data) + 1;
    *p_buf++ = 0xff; // GAP_AD_TYPE_MANU_SPECIFIC_DATA;
    memcpy(p_buf, manufacturer_data, 18);
    p_buf += sizeof(manufacturer_data);
    length += (sizeof(manufacturer_data) + 2);

    // Sanity check
    assert(rem_len >= LEGA_ADV_DATA_LEN - 3);

    // Get remaining space in the Advertising Data - 2 bytes are used for name length/flag
    rem_len -= length;

    // Check if additional data can be added to the Advertising data - 2 bytes needed for type and length
    if (rem_len > 2) {
        uint8_t dev_name_length = MIN(user_bt_stack_dev_cfg.name_len, (rem_len - 2));

        // Device name length
        *p_buf = dev_name_length + 1;
        // Device name flag (check if device name is complete or not)
        *(p_buf + 1) = (dev_name_length == user_bt_stack_dev_cfg.name_len)
                           ? 0x09
                           : 0x08; // GAP_AD_TYPE_COMPLETE_NAME : GAP_AD_TYPE_SHORTENED_NAME;
        // Copy device name
        memcpy(p_buf + 2, user_bt_stack_dev_cfg.name, dev_name_length);

        // Update advertising data length
        length += (dev_name_length + 2);
    }
    return length;
}
#endif

uint8_t adv_user_data[LEGA_ADV_DATA_LEN];
const uint8_t *lisa_bt_get_adv_data(uint8_t *len)
{
    *len = lisa_ble_gen_user_adv_data(adv_user_data);
    return adv_user_data;
}
extern uint16_t netcfg_bles_profile_set_cb(uint8_t conidx, uint8_t att_idx, uint16_t op, uint8_t *p_value);

/// Message callback handle from APP
static const netcfg_bles_cb_t netcfg_app_cb = {
    .cb_value_set = netcfg_bles_profile_set_cb,
};
const diss_cb_t user_bt_stack_ble_diss_msg_cb = {
    .cb_value_get = NULL, // dis_profile_get_cb,
};

/**
 * @brief 初始化自定义服务
 *
 * 注册 GATT 用户回调并添加服务到数据库。
 * 此函数应在协议栈初始化完成后调用。
 */
void app_ble_init_cmp(void)
{
    // enable bass service
    ble_bass_init();
    ble_bass_enable(0, bt_stack_vbat_percent_get());
    // enable net config.
    ble_netcfg_bles_init((netcfg_bles_cb_t *)&netcfg_app_cb);

    // enable diss.
    ble_diss_init((diss_cb_t *)&user_bt_stack_ble_diss_msg_cb);

#if BLE_VOICE_SIMULATOR
    // enable hid service
    uint8_t svc_features = HOGPD_CFG_KEYBOARD | HOGPD_CFG_MOUSE | HOGPD_CFG_PROTO_MODE | HOGPD_CFG_REPORT_NTF_EN;
    uint8_t report_char_cfg = HOGPD_CFG_REPORT_IN;
    hogpd_report_map_t report_map = {sizeof(hid_report_map), 0, (uint8_t *)hid_report_map};
    ble_hogpd_init(svc_features, report_char_cfg, (hogpd_cb_t *)&bt_stack_ble_hogpd_msg_cb, &report_map);
    ble_hogpd_enable(0);
#endif
}

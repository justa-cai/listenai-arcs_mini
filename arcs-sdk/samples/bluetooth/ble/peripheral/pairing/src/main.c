/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief BLE 配对示例
 *
 * 本示例演示如何使用 LISA Bluetooth 组件实现 BLE 配对功能：
 * 1. 配对成功/失败/加密事件回调 (lisa_ble_register_bond_cb)
 * 2. 自定义 passkey 处理 (lisa_ble_register_key_req_cb)
 * 3. 通过 lisa_ble_key_confirm() 确认 passkey
 */

#define LOG_TAG "sample"
#include <lisa_log.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arcs_ap_base.h"
#include "spiflash.h"
#include "arcs_flash_if.h"
#include "nvs.h"
#include "lisa_bluetooth.h"
#include "ble_adv_data.h"
#include "lisa_ble_api.h"

#include "FreeRTOS.h"
#include "task.h"

#define SAMPLE_PASSKEY  888888

#if CFG_NVS
#define NVDS_FLASH_ADDRESS_OFFSET   (0xFF8000) //The last 32KB of 16B flash
#define NVDS_FLASH_SIZE      (0x8000) //32KB

struct nvs_fs arcs_nvs_fs;
FLASH_DEV arcs_flash_dev  = {
    .base_addr = CMN_FLASHC_BASE,
    .d_width = 4,
    .sclk_div = 0xFF, //divider is 1
    .run_mod = RUN_WITHOUT_INT,
    .timeout = 0x180000
};

static int arcs_nvs_init(void)
{
    struct flash_pages_info info;

    flash_if_init(&arcs_flash_dev, 0, 0);

    //init nvs module
    arcs_nvs_fs.offset = NVDS_FLASH_ADDRESS_OFFSET;
    arcs_nvs_fs.flash_device = &arcs_flash_dev;
    flash_get_page_info_by_offs(&arcs_flash_dev, arcs_nvs_fs.offset, &info);
    arcs_nvs_fs.sector_size = info.size;
    arcs_nvs_fs.sector_count = NVDS_FLASH_SIZE/info.size;

    nvds_init(&arcs_nvs_fs);

    return 0;
}
#endif

static const uint8_t user_adv_data[] = {
    BLE_AD_FLAGS(GAP_AD_TYPE_FLAGS_GENERAL | GAP_AD_TYPE_FLAGS_BREDR_NOT_SUPPORTED),
    BLE_AD_COMPLETE_NAME(9, 'A', 'R', 'C', 'S', '_', 'P', 'A', 'I', 'R'),
};

const uint8_t* lisa_bt_get_adv_data(uint8_t *len)
{
    *len = sizeof(user_adv_data);
    return user_adv_data;
}

static const uint8_t user_scan_rsp_data[] = {
    BLE_AD_MFG_DATA(4, 0xAB, 0x0A, 0x01, 0x02),
};

const uint8_t* lisa_bt_get_scan_rsp_data(uint8_t *len)
{
    *len = sizeof(user_scan_rsp_data);
    return user_scan_rsp_data;
}

static void on_ble_connected(uint8_t conidx, uint16_t conhdl, const gap_bdaddr_t *peer_addr)
{
    LISA_LOGI(LOG_TAG, "BLE Connected! conidx=%d, conhdl=0x%04X", conidx, conhdl);
    LISA_LOGI(LOG_TAG, "Peer addr: %02X:%02X:%02X:%02X:%02X:%02X (type=%d)",
              peer_addr->addr[5], peer_addr->addr[4], peer_addr->addr[3],
              peer_addr->addr[2], peer_addr->addr[1], peer_addr->addr[0],
              peer_addr->addr_type);
}

static void on_ble_disconnected(uint8_t conidx, uint16_t conhdl, uint16_t reason)
{
    LISA_LOGI(LOG_TAG, "BLE Disconnected! conidx=%d, conhdl=0x%04X, reason=0x%04X",
              conidx, conhdl, reason);

    // Restart advertising after disconnection
    lisa_ble_adv_start(0, LISA_BLE_ADV_GEN);
    LISA_LOGI(LOG_TAG, "Advertising restarted");
}

static void on_ble_bond(uint8_t conidx, uint8_t info, uint8_t value)
{
    const char *info_str = "UNKNOWN";

    switch (info) {
    case 0:  info_str = "PAIRING_SUCCEED"; break;
    case 1:  info_str = "PAIRING_FAILED"; break;
    case 2:  info_str = "TK_EXCH"; break;
    case 3:  info_str = "IRK_EXCH"; break;
    case 4:  info_str = "CSRK_EXCH"; break;
    case 5:  info_str = "LTK_EXCH"; break;
    case 10: info_str = "LINK_ENCRYPTED"; break;
    case 11: info_str = "LINK_ENCRYPT_REQ"; break;
    }

    LISA_LOGI(LOG_TAG, "Bond event! conidx=%d, info=%d (%s), value=%d",
              conidx, info, info_str, value);
}

static void on_ble_key_req(uint8_t conidx, uint8_t key_type, uint32_t passkey)
{
    LISA_LOGI(LOG_TAG, "Key request! conidx=%d, key_type=%d, passkey=%lu",
              conidx, key_type, (unsigned long)passkey);
    LISA_LOGI(LOG_TAG, "Confirming passkey: %d", SAMPLE_PASSKEY);

    lisa_ble_key_confirm(conidx, 1, SAMPLE_PASSKEY);
}

int main(int argc, char **argv)
{
    LISA_LOGI(LOG_TAG, "=== BLE Pairing Example ===");

    arcs_nvs_init();

    lisa_bluetooth_init(NULL);

    lisa_ble_register_conn_cb(on_ble_connected);
    lisa_ble_register_disc_cb(on_ble_disconnected);
    lisa_ble_register_bond_cb(on_ble_bond);
    lisa_ble_register_key_req_cb(on_ble_key_req);

    LISA_LOGI(LOG_TAG, "BLE Stack Initialized");

    // Start Advertising
    lisa_ble_adv_start(0, LISA_BLE_ADV_GEN);
    LISA_LOGI(LOG_TAG, "Advertising started");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return 0;
}

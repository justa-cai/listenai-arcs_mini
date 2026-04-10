/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief BLE 自定义服务示例
 *
 * 本示例演示如何使用 LISA Bluetooth 组件实现自定义 GATT 服务：
 * 1. 自定义广播数据
 * 2. 自定义扫描响应数据
 * 3. 自定义 GAP 配置
 * 4. BLE 连接/断连回调
 * 5. 自定义 GATT 服务（读/写/通知/安全读）
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
#include "custom_service.h"

#include "FreeRTOS.h"
#include "task.h"

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
    BLE_AD_COMPLETE_NAME(4, 'A', 'R', 'C', 'S'),
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

int main(int argc, char **argv)
{
    LISA_LOGI(LOG_TAG, "=== BLE Custom Service Example ===");

    arcs_nvs_init();

    lisa_bluetooth_init(app_ble_init_cmp);

    // Register BLE connection/disconnection callbacks
    lisa_ble_register_conn_cb(on_ble_connected);
    lisa_ble_register_disc_cb(on_ble_disconnected);

    LISA_LOGI(LOG_TAG, "BLE Stack Initialized");

    // Start Advertising
    lisa_ble_adv_start(0, LISA_BLE_ADV_GEN);
    LISA_LOGI(LOG_TAG, "Advertising started");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return 0;
}

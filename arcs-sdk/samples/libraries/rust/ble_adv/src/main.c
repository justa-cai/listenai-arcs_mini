/*
 * Copyright (c) 2026, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 *
 * NVS/flash bring-up glue for the Rust ble_adv sample. The flash-backed NVS
 * (which stores the persistent BD address) is sample-local C boilerplate (it
 * pulls in flash + nvs headers), so it stays here. Once NVS is up we hand off
 * to rust_main(), which drives the LISA Bluetooth advertising API via
 * arcs::Bluetooth. The advertising payload is supplied by Rust through the
 * weak lisa_bt_get_adv_data / lisa_bt_get_scan_rsp_data overrides.
 */
#define LOG_TAG "rust_ble_adv"
#include <lisa_log.h>

#include <stdint.h>

#include "arcs_ap_base.h"
#include "spiflash.h"
#include "flash_if.h"
#include "nvs.h"

#include "FreeRTOS.h"
#include "task.h"

extern int rust_main(void);

#if CFG_NVS
#define NVDS_FLASH_ADDRESS_OFFSET (0xFF8000) /* last 32KB of 16M flash */
#define NVDS_FLASH_SIZE           (0x8000)   /* 32KB */

struct nvs_fs arcs_nvs_fs;
FLASH_DEV arcs_flash_dev = {
    .base_addr = CMN_FLASHC_BASE,
    .d_width = 4,
    .sclk_div = 0xFF, /* divider is 1 */
    .run_mod = RUN_WITHOUT_INT,
    .timeout = 0x180000,
};

static int arcs_nvs_init(void)
{
    struct flash_pages_info info;

    flash_if_init(&arcs_flash_dev, 0, 0);

    arcs_nvs_fs.offset = NVDS_FLASH_ADDRESS_OFFSET;
    arcs_nvs_fs.flash_device = &arcs_flash_dev;
    flash_get_page_info_by_offs(&arcs_flash_dev, arcs_nvs_fs.offset, &info);
    arcs_nvs_fs.sector_size = info.size;
    arcs_nvs_fs.sector_count = NVDS_FLASH_SIZE / info.size;

    nvds_init(&arcs_nvs_fs);

    return 0;
}
#endif

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    LISA_LOGI(LOG_TAG, "=== Rust BLE advertise demo ===");

#if CFG_NVS
    arcs_nvs_init();
#endif

    int ret = rust_main();
    if (ret != 0) {
        LISA_LOGE(LOG_TAG, "rust_main returned %d", ret);
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    return 0;
}

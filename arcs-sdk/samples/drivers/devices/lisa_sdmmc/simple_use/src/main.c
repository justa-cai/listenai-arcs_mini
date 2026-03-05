/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief LISA SDMMC driver usage example
 *
 * This example demonstrates how to use the LISA SDMMC driver:
 * - Configure SDMMC pins (user responsibility)
 * - Get SDMMC device
 * - Probe SD/MMC card
 * - Query SDMMC information
 * - Read and write sectors
 */

#define LOG_TAG "sdmmc_sample"
#include <lisa_log.h>

#include <string.h>
#include <stdint.h>
#include "lisa_device.h"
#include "lisa_sdmmc.h"
#include "IOMuxManager.h"

#define SDMMC_DEVICE_NAME    "sdmmc0"
#define SECTOR_SIZE         512
/* Use an offset to avoid overwriting filesystem area, similar to driver tests */
#define TEST_SECTOR_START   2048

/* SDMMC 引脚定义 (根据硬件原理图配置) */
#define SDMMC_CLK_PAD       CSK_IOMUX_PAD_A
#define SDMMC_CLK_PIN       6
#define SDMMC_CMD_PAD       CSK_IOMUX_PAD_A
#define SDMMC_CMD_PIN       7
#define SDMMC_DAT0_PAD      CSK_IOMUX_PAD_A
#define SDMMC_DAT0_PIN      5
#define SDMMC_DAT1_PAD      CSK_IOMUX_PAD_A
#define SDMMC_DAT1_PIN      4
#define SDMMC_DAT2_PAD      CSK_IOMUX_PAD_A
#define SDMMC_DAT2_PIN      9
#define SDMMC_DAT3_PAD      CSK_IOMUX_PAD_A
#define SDMMC_DAT3_PIN      8
#define SDMMC_FUNC          CSK_IOMUX_FUNC_ALTER15

/* Aligned buffer for DMA operations */
static uint8_t buffer[SECTOR_SIZE] __attribute__((aligned(64)));

/*
    为满足不同板型示例场景，重定向sdmmc设备的pinmux配置
*/
#ifdef CONFIG_BOARD_ARCS_EVB
void lisa_sdio_pinmux()
{
    IOMuxManager_PinConfigure(SDMMC_CLK_PAD, SDMMC_CLK_PIN, SDMMC_FUNC);
    IOMuxManager_PinConfigure(SDMMC_CMD_PAD, SDMMC_CMD_PIN, SDMMC_FUNC);
    IOMuxManager_PinConfigure(SDMMC_DAT0_PAD, SDMMC_DAT0_PIN, SDMMC_FUNC);
    IOMuxManager_PinConfigure(SDMMC_DAT1_PAD, SDMMC_DAT1_PIN, SDMMC_FUNC);
    IOMuxManager_PinConfigure(SDMMC_DAT2_PAD, SDMMC_DAT2_PIN, SDMMC_FUNC);
    IOMuxManager_PinConfigure(SDMMC_DAT3_PAD, SDMMC_DAT3_PIN, SDMMC_FUNC);
}
#endif

int main(int argc, char **argv)
{
    int ret;
    uint32_t sector_count, sector_size;

    LISA_LOGI(LOG_TAG, "LISA SDMMC Driver Example");

    lisa_device_t *sdmmc = lisa_device_get(SDMMC_DEVICE_NAME);
    if (!sdmmc) {
        LISA_LOGE(LOG_TAG, "Failed to get sdmmc device");
        return -1;
    }

    ret = lisa_sdmmc_probe(sdmmc);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Disk init failed: %d", ret);
        return ret;
    }

    if (lisa_sdmmc_status(sdmmc) != LISA_SDMMC_STATUS_OK) {
        LISA_LOGE(LOG_TAG, "Disk not ready");
        return -1;
    }

    lisa_sdmmc_get_sector_count(sdmmc, &sector_count);
    lisa_sdmmc_get_sector_size(sdmmc, &sector_size);

    LISA_LOGI(LOG_TAG, "Disk Info:");
    LISA_LOGI(LOG_TAG, "  Sector count: %u", sector_count);
    LISA_LOGI(LOG_TAG, "  Sector size:  %u bytes", sector_size);
    LISA_LOGI(LOG_TAG, "  Total size:   %llu MB",
              (uint64_t)sector_count * sector_size / (1024 * 1024));

    uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04};

    ret = lisa_sdmmc_write(sdmmc, test_data, TEST_SECTOR_START, 1);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Write sector %u failed: %d", TEST_SECTOR_START, ret);
        return ret;
    }

    LISA_LOGI(LOG_TAG, "Write sector %u OK", TEST_SECTOR_START);

    ret = lisa_sdmmc_read(sdmmc, buffer, TEST_SECTOR_START, 1);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Read sector %u failed: %d", TEST_SECTOR_START, ret);
        return ret;
    }
    
    if (memcmp(buffer, test_data, sizeof(test_data)) == 0) {
        LISA_LOGI(LOG_TAG, "Read sector %u OK", TEST_SECTOR_START);
    } else {
        LISA_LOGE(LOG_TAG, "Read sector %u verify failed", TEST_SECTOR_START);
        return -1;
    }

    LISA_LOGI(LOG_TAG, "LISA SDMMC Sample OK");
    return 0;
}


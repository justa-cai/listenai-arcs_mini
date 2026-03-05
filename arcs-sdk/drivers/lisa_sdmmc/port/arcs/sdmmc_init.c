/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stdint.h>
#include "lib_sdc.h"
#include "drv_sdc.h"
#include "arcs_ap.h"
#include "esp_heap_caps.h"
#include "FreeRTOS.h"

#define LOG_TAG "sdmmc_init"
#include <lisa_log.h>

#define SD_PORT SD_0

/* 默认扫描超时时间 */
#ifndef CONFIG_LISA_SDMMC_SCAN_TIMEOUT_MS
#define CONFIG_LISA_SDMMC_SCAN_TIMEOUT_MS 3000
#endif

static SDCardInfo sd_info;

#define _DMA32  __attribute__((aligned(32)))
static _DMA32 uint8_t sd_adma_buf[512];

/**
 * @brief 配置 SDMMC 时钟和电源
 */
static void sdmmc_clock_power_init(void)
{
    /* Enable SDIO Host Clock */
    IP_AP_CFG->REG_CLK_CFG1.bit.ENA_SDIOH_CLK = 1;

    /* Release Reset signal */
    IP_SDIOH->REG_VR1.bit.LO_SD_RSTN = 1;

    /* Enable SD Clock */
    IP_SDIOH->REG_CCR_TCR_SRR.bit.SD_CLK_EN = 1;

    /* Set SD power enable and voltage (3.3V) */
    IP_SDIOH->REG_HC1_PCR_BGCR.bit.SD_BUS_POW = 1;
    IP_SDIOH->REG_HC1_PCR_BGCR.bit.SD_BUS_VOL = 7;
}

/**
 * @brief 平台电源和时钟初始化（在设备注册时调用）
 *
 * @return 0 成功, -EIO 失败
 */
int sdmmc_platform_init(void)
{
    u32 option = SDC_OPTION_ENABLE | SDC_OPTION_FIXED |
                 SDC_OPTION_SDIO_STD_FUNC | SDC_OPTION_SDIO_FORCE_3_3_V;

    /* Platform init with clock/power callback */
    gm_api_sdc_platform_init(option, 0, sdmmc_clock_power_init, (u32)&sd_info);

    /* Set ADMA buffer */
    gm_sdc_api_action(SD_PORT, GM_SDC_ACTION_SET_ADMA_BUFER, sd_adma_buf, NULL);

    LISA_LOGI(LOG_TAG, "SDMMC platform initialized (clock and power configured)");
    return 0;
}

/**
 * @brief 探测 SD/MMC 卡并配置（在 probe 时调用）
 *
 * @return 0 成功, -EIO 失败
 */
int sdmmc_hard_init(void)
{
    int ret;

    /* Card detection */
    ret = (int)gm_sdc_api_action(SD_PORT, GM_SDC_ACTION_CARD_DETECTION, NULL, NULL);
    if (ret != 0) {
        LISA_LOGD(LOG_TAG, "Card detection failed: %d", ret);
        return -EIO;
    }

    /* Set 4-bit bus width */
    u32 bus_width = 4;
    ret = gm_sdc_api_action(SD_PORT, GM_SDC_ACTION_SET_BUS_WIDTH, &bus_width, NULL);
    if (ret != 0) {
        LISA_LOGW(LOG_TAG, "Set bus width failed: %d", ret);
    }

    LISA_LOGI(LOG_TAG, "SD/MMC card detected and configured");
    return 0;
}

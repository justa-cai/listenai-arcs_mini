/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stdint.h>
#include "lib_sdc.h"
#include "drv_sdc.h"
#if defined(CONFIG_BOOT_ADB)
#include <ftsdc021.h>
#include "ClockManager.h"
#endif
#include "arcs_ap.h"
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
#if defined(CONFIG_BOOT_ADB)
    /* Match the SDIO host with the 2x flash clock path used by low-level demos. */
    HAL_CRM_SetSdio_hClkDiv(1U, 1U);
    HAL_CRM_SetSdio_hClkSrc(CRM_IpSrcFlashClk);
#endif

    /* Enable SDIO Host Clock */
    IP_AP_CFG->REG_CLK_CFG1.bit.ENA_SDIOH_CLK = 1;

    /* Release Reset signal */
    IP_SDIOH->REG_VR1.bit.LO_SD_RSTN = 1;

#if defined(CONFIG_BOOT_ADB)
    /* Match the boot recovery SD clock setup so card scan can switch modes. */
    IP_SDIOH->REG_CCR_TCR_SRR.bit.UPPER_BIT_SD_CLK_SEL = 0;
    IP_SDIOH->REG_CCR_TCR_SRR.bit.LOW_BIT_SD_CLK_SEL = 0;
#endif

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
    u32 option = SDC_OPTION_ENABLE | SDC_OPTION_FIXED | SDC_OPTION_SDIO_STD_FUNC;

#if !defined(CONFIG_BOOT_ADB)
    option |= SDC_OPTION_SDIO_FORCE_3_3_V;
#endif

    /* Platform init with clock/power callback */
    gm_api_sdc_platform_init(option, 0, sdmmc_clock_power_init, (u32)&sd_info);

#if !defined(CONFIG_BOOT_ADB)
    /* The app-side flow uses the legacy detection path and needs ADMA ready up front. */
    gm_sdc_api_action(SD_PORT, GM_SDC_ACTION_SET_ADMA_BUFER, sd_adma_buf, NULL);
#endif

    LISA_LOGI(LOG_TAG, "SDMMC platform initialized (clock and power configured)");
    return 0;
}

#if defined(CONFIG_BOOT_ADB)
static int sdmmc_scan_card(uint32_t bus_speed)
{
    int ret = ERR_SD_CARD_NOT_EXIST;

    for (uint32_t elapsed_ms = 0; elapsed_ms < CONFIG_LISA_SDMMC_SCAN_TIMEOUT_MS; elapsed_ms += 100U) {
        ret = (int)gm_sdc_api_action(SD_PORT, GM_SDC_ACTION_CARD_SCAN, &bus_speed, NULL);
        if (ret == ERR_SD_NO_ERROR) {
            return 0;
        }

        if (ret != ERR_SD_CARD_NOT_EXIST) {
            LISA_LOGW(LOG_TAG, "Card scan failed at speed=%u, ret=%d", (unsigned int)bus_speed, ret);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    return ret;
}
#endif

/**
 * @brief 探测 SD/MMC 卡并配置（在 probe 时调用）
 *
 * @return 0 成功, -EIO 失败
 */
int sdmmc_hard_init(void)
{
    int ret;
    u32 bus_width = 4;

#if defined(CONFIG_BOOT_ADB)
    const uint32_t selected_speed = UHS_SDR25_BUS_SPEED;

    ret = (int)gm_sdc_api_action(SD_PORT, GM_SDC_ACTION_INIT, NULL, NULL);
    if (ret != ERR_SD_NO_ERROR) {
        LISA_LOGE(LOG_TAG, "SD host init failed: %d", ret);
        return -EIO;
    }

    ret = (int)gm_sdc_api_action(SD_PORT, GM_SDC_ACTION_SET_ADMA_BUFER, sd_adma_buf, NULL);
    if (ret != ERR_SD_NO_ERROR) {
        LISA_LOGE(LOG_TAG, "Set ADMA buffer failed: %d", ret);
        return -EIO;
    }

    ret = (int)gm_sdc_api_action(SD_PORT, GM_SDC_ACTION_SOFT_RESET, &(u32){SDHCI_SOFTRST_ALL}, NULL);
    if (ret != ERR_SD_NO_ERROR) {
        LISA_LOGE(LOG_TAG, "SD host soft reset failed: %d", ret);
        return -EIO;
    }

    ret = sdmmc_scan_card(selected_speed);
    if (ret != 0) {
        LISA_LOGE(LOG_TAG, "SD card scan timeout, last ret=%d", ret);
        return -EIO;
    }
#else
    /* Keep the app-side flow on the proven card-detection sequence used by CI tests. */
    ret = (int)gm_sdc_api_action(SD_PORT, GM_SDC_ACTION_CARD_DETECTION, NULL, NULL);
    if (ret != 0) {
        LISA_LOGD(LOG_TAG, "Card detection failed: %d", ret);
        return -EIO;
    }

#endif

    ret = (int)gm_sdc_api_action(SD_PORT, GM_SDC_ACTION_SET_BUS_WIDTH, &bus_width, NULL);
#if defined(CONFIG_BOOT_ADB)
    if (ret != ERR_SD_NO_ERROR) {
        LISA_LOGE(LOG_TAG, "Set bus width failed: %d", ret);
        return -EIO;
    }
#else
    if (ret != 0) {
        LISA_LOGW(LOG_TAG, "Set bus width failed: %d", ret);
    }
#endif

    LISA_LOGI(LOG_TAG, "SD/MMC card detected and configured");
    return 0;
}

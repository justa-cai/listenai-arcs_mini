/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include "lib_sdc.h"
#include "drv_sdc.h"
#include "ClockManager.h"
#include "arcs_ap.h"
#include "FreeRTOS.h"

#include "sdmmc_init.h"

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
    /* Use the 2x flash clock path expected by the SDIO host controller. */
    HAL_CRM_SetSdio_hClkDiv(1U, 1U);
    HAL_CRM_SetSdio_hClkSrc(CRM_IpSrcFlashClk);

    /* Enable SDIO Host Clock */
    IP_AP_CFG->REG_CLK_CFG1.bit.ENA_SDIOH_CLK = 1;

    /* Release Reset signal */
    IP_SDIOH->REG_VR1.bit.LO_SD_RSTN = 1;

    /* Select DIV2 before enabling the SD output clock. */
    IP_SDIOH->REG_CCR_TCR_SRR.bit.UPPER_BIT_SD_CLK_SEL = 0;
    IP_SDIOH->REG_CCR_TCR_SRR.bit.LOW_BIT_SD_CLK_SEL = 0;

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
    u32 option = SDC_OPTION_ENABLE | SDC_OPTION_SDIO_STD_FUNC;

    /* App-side SD/FS flows still rely on the legacy fixed-card sequence. */
    option |= SDC_OPTION_FIXED | SDC_OPTION_SDIO_FORCE_3_3_V;

    /* Platform init with clock/power callback */
    gm_api_sdc_platform_init(option, 0, sdmmc_clock_power_init, (u32)&sd_info);

    /* The app-side flow uses the legacy detection path and needs ADMA ready up front. */
    gm_sdc_api_action(SD_PORT, GM_SDC_ACTION_SET_ADMA_BUFER, sd_adma_buf, NULL);

    LISA_LOGI(LOG_TAG, "SDMMC platform initialized (clock and power configured)");
    return 0;
}

static const struct sdmmc_runtime_ops *g_sdmmc_runtime_ops;

void sdmmc_set_runtime_ops(const struct sdmmc_runtime_ops *ops)
{
    g_sdmmc_runtime_ops = ops;
}

/* 长循环 yield 派发：驱动不反向依赖任何 wdt 实现，由 caller 通过
 * runtime_ops->yield 注入；未注册即 no-op。 */
static void sdmmc_yield(void)
{
    if (g_sdmmc_runtime_ops != NULL && g_sdmmc_runtime_ops->yield != NULL) {
        g_sdmmc_runtime_ops->yield();
    }
}

/**
 * @brief 探测 SD/MMC 卡并配置（在 probe 时调用）
 *
 * @return 0 成功, -EIO 失败
 */
int sdmmc_hard_init(void)
{
    int ret;
    u32 bus_width = 4;

    /* Keep the app-side flow on the proven card-detection sequence used by CI tests. */
    ret = (int)gm_sdc_api_action(SD_PORT, GM_SDC_ACTION_CARD_DETECTION, NULL, NULL);
    if (ret != 0) {
        LISA_LOGD(LOG_TAG, "Card detection failed: %d", ret);
        return -EIO;
    }

    ret = (int)gm_sdc_api_action(SD_PORT, GM_SDC_ACTION_SET_BUS_WIDTH, &bus_width, NULL);
   if (ret != 0) {
        LISA_LOGW(LOG_TAG, "Set bus width failed: %d", ret);
    }

    LISA_LOGI(LOG_TAG, "SD/MMC card detected and configured");
    return 0;
}

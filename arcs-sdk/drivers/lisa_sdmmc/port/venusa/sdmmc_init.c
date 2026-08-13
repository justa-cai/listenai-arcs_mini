/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdmmc_init.h"

#include <errno.h>
#include <stdint.h>

#include "board.h"
#include "cache.h"
#include "ClockManager.h"
#include "IOMuxManager.h"
#include "ftsdc021.h"
#include "lib_sdc.h"
#include "sdioh_reg.h"
#include "venusa_ap.h"

#define LOG_TAG "sdmmc_init"
#include <lisa_log.h>

#define SDMMC_PORT SD_0
#define SDMMC_DMA_ALIGN __attribute__((aligned(32)))

#ifndef CONFIG_LISA_SDMMC_VENUSA_BUS_WIDTH
#define CONFIG_LISA_SDMMC_VENUSA_BUS_WIDTH 4
#endif

#ifndef CONFIG_LISA_SDMMC_VENUSA_TARGET_SPEED
#define CONFIG_LISA_SDMMC_VENUSA_TARGET_SPEED UHS_SDR50_BUS_SPEED
#endif

#ifndef CONFIG_LISA_SDMMC_VENUSA_SDMA_BOUNDARY_ORDER
#define CONFIG_LISA_SDMMC_VENUSA_SDMA_BOUNDARY_ORDER 7
#endif

static bool s_platform_ready;
static SDMMC_DMA_ALIGN SDCardInfo s_sd_card_info;
static SDMMC_DMA_ALIGN uint8_t s_sd_adma_buffer[SDMMC_SECTOR_SIZE];

static volatile ftsdc021_reg *sdmmc_regs(void)
{
    return (volatile ftsdc021_reg *)(uintptr_t)SDC_FTSDC021_0_PA_BASE;
}

static void sdmmc_pinmux(void)
{
    lisa_sdio_pinmux();

    for (uint8_t pin = 0U; pin <= CSK_IOMUX_PAD_SDIO_MAX_PIN; pin++) {
        (void)IOMuxManager_PinConfigure(CSK_IOMUX_PAD_SDIO, pin, CSK_IOMUX_FUNC_DEFAULT);
    }

    /* CLK does not need pull-up; CMD/DAT[3:0] do. */
    for (uint8_t pin = 1U; pin <= CSK_IOMUX_PAD_SDIO_MAX_PIN; pin++) {
        (void)IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_SDIO, pin, HAL_IOMUX_PULLUP_MODE);
    }
}

static void sdmmc_clock_power_init(void)
{
    (void)HAL_CRM_SetSdiohClkDiv(1U, 1U);
    __HAL_CRM_SDIOH_CLK_ENABLE();

    sdmmc_pinmux();

    IP_SDIOH->REG_VR1.bit.LO_SD_RSTN = 0x1;
    IP_SDIOH->REG_CCR_TCR_SRR.bit.SD_CLK_EN = 0x1;
    IP_SDIOH->REG_HC1_PCR_BGCR.bit.SD_BUS_POW = 0x1;
    IP_SDIOH->REG_HC1_PCR_BGCR.bit.SD_BUS_VOL = 0x3;
}

static uint32_t sdmmc_platform_options(void)
{
    uint32_t options = SDC_OPTION_ENABLE | SDC_OPTION_FIXED;

#if CONFIG_LISA_SDMMC_VENUSA_FORCE_3_3V
    options |= SDC_OPTION_SDIO_FORCE_3_3_V;
#endif

    return options;
}

int sdmmc_platform_init(void)
{
    if (s_platform_ready) {
        return 0;
    }

    uint32_t ret = gm_api_sdc_platform_init(sdmmc_platform_options(),
                                            0U,
                                            sdmmc_clock_power_init,
                                            (uint32_t)(uintptr_t)&s_sd_card_info);
    if (ret != ERR_SD_NO_ERROR) {
        LISA_LOGE(LOG_TAG, "SDMMC platform init failed: %u", (unsigned int)ret);
        return -EIO;
    }

    ret = gm_sdc_api_action(SDMMC_PORT,
                            GM_SDC_ACTION_SET_ADMA_BUFER,
                            s_sd_adma_buffer,
                            NULL);
    if (ret != ERR_SD_NO_ERROR) {
        LISA_LOGE(LOG_TAG, "SDMMC ADMA buffer setup failed: %u", (unsigned int)ret);
        return -EIO;
    }

    s_platform_ready = true;
    return 0;
}

void sdmmc_platform_reset(void)
{
    s_platform_ready = false;
}

static bool sdmmc_signal_18v(void)
{
    return (sdmmc_regs()->HostCtrl2 & SDHCI_18V_SIGNAL) != 0U;
}

static void sdmmc_apply_transfer_tuning(void)
{
    SDCardInfo *card = SDHost[SDMMC_PORT].Card;

    if ((card != NULL) && (card->FlowSet.UseDMA == SDMA)) {
        ftsdc021_set_transfer_type(SDMMC_PORT,
                                   SDMA,
                                   CONFIG_LISA_SDMMC_VENUSA_SDMA_BOUNDARY_ORDER);
    }
}

static int sdmmc_apply_speed_tuning(void)
{
    SDCardInfo *card = SDHost[SDMMC_PORT].Card;
    const uint32_t target = CONFIG_LISA_SDMMC_VENUSA_TARGET_SPEED;

    if (card == NULL) {
        return -ENODEV;
    }

    if (target == (uint32_t)card->speed) {
        return 0;
    }

    if ((target > UHS_SDR25_BUS_SPEED) && !sdmmc_signal_18v()) {
        LISA_LOGW(LOG_TAG, "Skip UHS speed target=%u: card/host remains 3.3V",
                  (unsigned int)target);
        return 0;
    }

    if ((card->bs_mode & (1UL << target)) == 0U) {
        LISA_LOGW(LOG_TAG, "Skip unsupported SD speed target=%u, mode mask=0x%x",
                  (unsigned int)target,
                  (unsigned int)card->bs_mode);
        return 0;
    }

    if (ftsdc021_set_bus_speed_mode(SDMMC_PORT, (uint8_t)target) != 0U) {
        LISA_LOGW(LOG_TAG, "Switch SD speed target=%u failed", (unsigned int)target);
        return -EIO;
    }

    return 0;
}

int sdmmc_hard_init(void)
{
    int ret = sdmmc_platform_init();
    if (ret != 0) {
        return ret;
    }

    ret = (int)gm_sdc_api_action(SDMMC_PORT, GM_SDC_ACTION_CARD_DETECTION, NULL, NULL);
    if (ret != ERR_SD_NO_ERROR) {
        LISA_LOGE(LOG_TAG, "SD card detection failed: %d", ret);
        return -EIO;
    }

    uint32_t bus_width = CONFIG_LISA_SDMMC_VENUSA_BUS_WIDTH;
    ret = (int)gm_sdc_api_action(SDMMC_PORT,
                                 GM_SDC_ACTION_SET_BUS_WIDTH,
                                 &bus_width,
                                 NULL);
    if (ret != ERR_SD_NO_ERROR) {
        LISA_LOGW(LOG_TAG, "Set SD bus width=%u failed: %d", (unsigned int)bus_width, ret);
    }

    (void)sdmmc_apply_speed_tuning();
    sdmmc_apply_transfer_tuning();

    return 0;
}

bool sdmmc_card_exists(void)
{
    return gm_sdc_api_action(SDMMC_PORT, GM_SDC_ACTION_IS_CARD_EXIST, NULL, NULL) == ERR_SD_NO_ERROR;
}

int sdmmc_read_sectors(uint32_t sector, uint32_t count, void *buffer)
{
    int ret = (int)gm_sdc_api_sdcard_sector_read(SDMMC_PORT, sector, count, buffer);
    if (ret != ERR_SD_NO_ERROR) {
        LISA_LOGE(LOG_TAG, "SD read failed: sector=%u count=%u ret=%d",
                  (unsigned int)sector,
                  (unsigned int)count,
                  ret);
        return -EIO;
    }

    return 0;
}

int sdmmc_write_sectors(uint32_t sector, uint32_t count, const void *buffer)
{
    int ret = (int)gm_sdc_api_sdcard_sector_write(SDMMC_PORT, sector, count, (void *)buffer);
    if (ret != ERR_SD_NO_ERROR) {
        LISA_LOGE(LOG_TAG, "SD write failed: sector=%u count=%u ret=%d",
                  (unsigned int)sector,
                  (unsigned int)count,
                  ret);
        return -EIO;
    }

    return 0;
}

int sdmmc_get_sector_count(uint32_t *sector_count)
{
    if (sector_count == NULL) {
        return -EINVAL;
    }

    uint32_t value = 0U;
    int ret = (int)gm_sdc_api_action(SDMMC_PORT,
                                     GM_SDC_ACTION_GET_BLK_NUM,
                                     NULL,
                                     &value);
    if (ret != ERR_SD_NO_ERROR) {
        return -EIO;
    }

    *sector_count = value;
    return 0;
}

int sdmmc_get_erase_block_size(uint32_t *erase_block_sectors)
{
    if (erase_block_sectors == NULL) {
        return -EINVAL;
    }

    uint32_t value = 0U;
    int ret = (int)gm_sdc_api_action(SDMMC_PORT,
                                     GM_SDC_ACTION_GET_ERASE_SIZE,
                                     NULL,
                                     &value);
    if (ret != ERR_SD_NO_ERROR) {
        return -EIO;
    }

    *erase_block_sectors = (value == 0U) ? 1U : value;
    return 0;
}

void sdmmc_cache_invalidate(void *buffer, uint32_t length)
{
#if CONFIG_DCACHE_ENABLE
    if ((buffer != NULL) && (length > 0U)) {
        HAL_InvalidateDCache_by_Addr((uint32_t *)(uintptr_t)buffer, length);
    }
#else
    (void)buffer;
    (void)length;
#endif
}

void sdmmc_cache_flush(const void *buffer, uint32_t length)
{
#if CONFIG_DCACHE_ENABLE
    if ((buffer != NULL) && (length > 0U)) {
        HAL_FlushDCache_by_Addr((uint32_t *)(uintptr_t)buffer, length);
    }
#else
    (void)buffer;
    (void)length;
#endif
}

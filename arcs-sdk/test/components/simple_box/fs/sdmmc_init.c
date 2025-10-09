
#include <errno.h>
#include <stdint.h>
#include "lib_sdc.h"
#include "drv_sdc.h"
#include "IOMuxManager.h"
#include "arcs_ap.h"
#include "esp_heap_caps.h"
#include "FreeRTOS.h"

#define SD_PORT SD_0
static SDCardInfo sd_info;
static uint8_t *dma_buf = NULL;

static int sdmmc_setup(void)
{

    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 6, CSK_IOMUX_FUNC_ALTER15); // sd_clk
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 7, CSK_IOMUX_FUNC_ALTER15); // sd_cmd
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 5, CSK_IOMUX_FUNC_ALTER15); // sd_dat0
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 4, CSK_IOMUX_FUNC_ALTER15); // sd_dat1
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 9, CSK_IOMUX_FUNC_ALTER15); // sd_dat2
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 8, CSK_IOMUX_FUNC_ALTER15); // sd_dat3

    IP_AP_CFG->REG_CLK_CFG1.bit.ENA_SDIOH_CLK = 1;          // Enable SDIO Host Clock
    IP_SDIOH->REG_VR1.bit.LO_SD_RSTN = 1;                   // SD Output Low-Active Reset Signal to RSTN of eMMC
    IP_SDIOH->REG_CCR_TCR_SRR.bit.UPPER_BIT_SD_CLK_SEL = 0; // SD Clock Frequency Divider[9:8]: DIVx2
    IP_SDIOH->REG_CCR_TCR_SRR.bit.LOW_BIT_SD_CLK_SEL = 0;   // SD Clock Frequency Divider[7:0]: DIVx2
    IP_SDIOH->REG_CCR_TCR_SRR.bit.SD_CLK_EN = 1;            // SD Clock Enable
    IP_SDIOH->REG_HC1_PCR_BGCR.bit.SD_BUS_POW = 1;          // SD Bus Power Enable
    IP_SDIOH->REG_HC1_PCR_BGCR.bit.SD_BUS_VOL = 7;          // SD Bus Voltage Select: 5:1.8V 6:3.0V 7:3.3V
}

static int sdmmc_probe(uint32_t timeout_ms)
{
    u32 ret;

    ret = gm_api_sdc_platform_init(SDC_OPTION_ENABLE | SDC_OPTION_CD_INVERT, 0, NULL, (u32)&sd_info);
    if (ret != ERR_SD_NO_ERROR) {
        CLOGE("gm_api_sdc_platform_init failed:%d", ret);
        goto _ERR1;
    }

    ret = gm_sdc_api_action(SD_PORT, GM_SDC_ACTION_INIT, NULL, NULL);
    if (ret != ERR_SD_NO_ERROR) {
        CLOGE("gm_sdc_api_action GM_SDC_ACTION_INIT failed:%d", ret);
        goto _ERR1;
    }
    dma_buf = heap_caps_aligned_alloc(CACHE_LINE_SIZE(DCACHE), sd_info.FlowSet.sdma_bound_mask + 1,
                                      MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM);
    if (dma_buf == NULL) {
        CLOGE("heap_caps_aligned_alloc failed");
        goto _ERR1;
    }

    ret = gm_sdc_api_action(SD_PORT, GM_SDC_ACTION_SET_ADMA_BUFER, dma_buf, NULL);
    if (ret != ERR_SD_NO_ERROR) {
        CLOGE("gm_sdc_api_action GM_SDC_ACTION_SET_ADMA_BUFER failed:%d", ret);
        goto _ERR1;
    }

    ret = gm_sdc_api_action(SD_PORT, GM_SDC_ACTION_SOFT_RESET, &(u32){SDHCI_SOFTRST_ALL}, NULL);
    if (ret != ERR_SD_NO_ERROR) {
        CLOGE("gm_sdc_api_action GM_SDC_ACTION_SOFT_RESET failed:%d", ret);
        goto _ERR1;
    }

    for (int i = 0; i < timeout_ms; i += 10) {
        // 0:SDR12/LS 1:SDR25/HS 2:SDR50 3:SDR104 4:DDR50
        ret = gm_sdc_api_action(SD_PORT, GM_SDC_ACTION_CARD_SCAN, &(u32){UHS_SDR25_BUS_SPEED}, NULL);
        if (ERR_SD_NO_ERROR == ret) {
            break;
        }
        if (ERR_SD_CARD_NOT_EXIST != ret) {
            CLOGW("Scan card failed:%d", ret);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (ret != ERR_SD_NO_ERROR) {
        goto _ERR1;
    }

    ret = gm_sdc_api_action(SD_PORT, GM_SDC_ACTION_SET_BUS_WIDTH, &(u32){4}, NULL);
    if (ret != ERR_SD_NO_ERROR) {
        goto _ERR1;
    }

    extern u8 SDC_SD_menu(u8 ip_idx);
    SDC_SD_menu(SD_PORT);

    return 0;
_ERR1:
    if (dma_buf != NULL) {
        heap_caps_free(dma_buf);
    }

    return -EIO;
}

int sdmmc_hard_init(void)
{
    sdmmc_setup();
    return sdmmc_probe(3000);
}
#include "winbond_config.h"

#ifndef PSRAM_WINBOND_DEFAULT_DENSITY
#define PSRAM_WINBOND_DEFAULT_DENSITY    PSRAM_MEM_128Mb_DENSITY_MAP
#endif

#define PSRAM_WINBOND_AHB_RD_SEQ_ID      1U
#define PSRAM_WINBOND_AHB_WR_SEQ_ID      2U
#define PSRAM_WINBOND_MR_WR_SEQ_ID       3U
#define PSRAM_WINBOND_MR_RD_SEQ_ID       4U

#define PSRAM_WINBOND_MR0_HIGH_VALUE     0x8eU
#define PSRAM_WINBOND_MR0_LOW_VALUE      0x57U

void __psram_winbond_mr_seq_configure(void)
{
    IP_PSRAM_CTRL->REG_SEQSEL.bit.MR_WR_SEQ_ID = PSRAM_WINBOND_MR_WR_SEQ_ID;
    IP_PSRAM_CTRL->REG_SEQSEL.bit.MR_RD_SEQ_ID = PSRAM_WINBOND_MR_RD_SEQ_ID;

    IP_PSRAM_CTRL->REG_S3LUT0.all = (0x500U << 16) | 0xf60U;
    IP_PSRAM_CTRL->REG_S3LUT1.all = 0;
    IP_PSRAM_CTRL->REG_S3LUT2.all = 0;
    IP_PSRAM_CTRL->REG_S3LUT3.all = 0;

    IP_PSRAM_CTRL->REG_S4LUT0.all = (0xa02U << 16) | 0xfc0U;
    IP_PSRAM_CTRL->REG_S4LUT1.all = 0;
    IP_PSRAM_CTRL->REG_S4LUT2.all = 0;
    IP_PSRAM_CTRL->REG_S4LUT3.all = 0;
}

void __psram_winbond_ahb_seq_configure(void)
{
    IP_PSRAM_CTRL->REG_SEQSEL.bit.AHB_RD_SEQ_ID = PSRAM_WINBOND_AHB_RD_SEQ_ID;
    IP_PSRAM_CTRL->REG_SEQSEL.bit.AHB_WR_SEQ_ID = PSRAM_WINBOND_AHB_WR_SEQ_ID;

    IP_PSRAM_CTRL->REG_S1LUT0.all = (0xa05U << 16) | 0xf80U;
    IP_PSRAM_CTRL->REG_S1LUT1.all = 0;
    IP_PSRAM_CTRL->REG_S1LUT2.all = 0;
    IP_PSRAM_CTRL->REG_S1LUT3.all = 0;

    IP_PSRAM_CTRL->REG_S2LUT0.all = (0xc08U << 16) | 0xf20U;
    IP_PSRAM_CTRL->REG_S2LUT1.all = 0x800U;
    IP_PSRAM_CTRL->REG_S2LUT2.all = 0;
    IP_PSRAM_CTRL->REG_S2LUT3.all = 0;
}

void __psram_winbond_controller_die_type(void)
{
    IP_PSRAM_CTRL->REG_DEVDEF.bit.DEV_TYPE = PSRAM_DEV_TYPE_WINBOND;
}

void __psram_winbond_controller_die_page_size(uint32_t density)
{
    (void)density;

    IP_PSRAM_CTRL->REG_DEVDEF.bit.DEV_PAGE_SIZE = PSRAM_DEV_PAGE_SIZE_2K;
}

void __psram_winbond_controller_timing_configure(uint32_t clock_freq)
{
    (void)clock_freq;

    IP_PSRAM_CTRL->REG_TIMCFG.bit.TCHD_CFG = 2;
}

int32_t __psram_winbond_info_extra(uint32_t *density)
{
    if (density == NULL) {
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    *density = PSRAM_WINBOND_DEFAULT_DENSITY;
    PSRAM_LOG("Winbond density cannot be probed, use default %d bytes",
              *density);

    return CSK_DRIVER_OK;
}

void __psram_winbond_die_para_configure(uint32_t clock_freq, uint32_t density)
{
    (void)clock_freq;
    (void)density;

    IP_PSRAM_CTRL->REG_DEVDEF.bit.HYPERBUS_MRWR_BYTE_LOW_VAL =
        PSRAM_WINBOND_MR0_LOW_VALUE;
    IP_PSRAM_CTRL->REG_MR0.all = PSRAM_WINBOND_MR0_HIGH_VALUE;
}

void __psram_winbond_mr_print(void)
{
    uint32_t mr0 = IP_PSRAM_CTRL->REG_MR0.all;

    PSRAM_LOG("Winbond MR0: 0x%x", mr0);
}

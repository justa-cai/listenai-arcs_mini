#include "ClockManager.h"
#include "clock_config.h"
#include "log_print.h"
#include <stdio.h>
#include <math.h>

#define FREQ_TOLERANCE_PCT            0

static const char* check_freq(uint32_t actual, uint32_t expected)
{
    if (expected == 0) return "N/A";
    double diff = fabs((double)actual - (double)expected);
    double pct = diff / expected * 100.0;
    return (pct <= FREQ_TOLERANCE_PCT) ? "PASS" : "FAIL";
}

static void print_check(const char* name, uint32_t actual, uint32_t expected)
{
    CLOGD("%-18s : %10lu Hz | Expected %10lu Hz | %s\n",
           name, actual, expected, check_freq(actual, expected));
}

void Clock_Config_Validation(void)
{
    CLOGD("\n==========================================\n");
    CLOGD("        CLOCK CONFIG VALIDATION\n");
    CLOGD("==========================================\n");

    extern uint32_t CRM_GetSrcFreq(clock_src_name_t src);
    print_check("RC032K",  CRM_GetSrcFreq(CRM_IpSrcRC032K),  BOARD_BOOTCLOCKRUN_RC032K_CLK);
    print_check("XTAL32K", CRM_GetSrcFreq(CRM_IpSrcXTAL32K), BOARD_BOOTCLOCKRUN_XTAL32K_CLK);
    print_check("RC024M",  CRM_GetSrcFreq(CRM_IpSrcRC024M),  BOARD_BOOTCLOCKRUN_RC024M_CLK);
    print_check("XTAL24M", CRM_GetSrcFreq(CRM_IpSrcXTAL24M), BOARD_BOOTCLOCKRUN_XTAL24M_CLK);

    // ---- PLL 输出 ----
    print_check("SYSPLL_CORE", CRM_GetSrcFreq(CRM_IpSrcSyspllCore), BOARD_BOOTCLOCKRUN_SYSPLL_CORE_CLK);
    print_check("SYSPLL_PERI", CRM_GetSrcFreq(CRM_IpSrcSyspllPeri), BOARD_BOOTCLOCKRUN_SYSPLL_PERI_CLK);
    print_check("SYSPLL_FLASH",CRM_GetSrcFreq(CRM_IpSrcSyspllFlash),BOARD_BOOTCLOCKRUN_SYSPLL_FLASH_CLK);
    print_check("SYSPLL_PSRAM",CRM_GetSrcFreq(CRM_IpSrcSyspllPsram),BOARD_BOOTCLOCKRUN_SYSPLL_PSRAM_CLK);
    print_check("SYSPLL_SDIO", CRM_GetSrcFreq(CRM_IpSrcSyspllSdio), BOARD_BOOTCLOCKRUN_SYSPLL_SDIO_CLK);

    // ---- 系统核心时钟 ----
    print_check("HCLK",      CRM_GetHclkFreq(), BOARD_BOOTCLOCKRUN_SYSPLL_CORE_CLK / BOARD_BOOTCLOCKRUN_HCLK_CLK_M);
    print_check("CMN_PCLK",  CRM_GetCmn_pclkFreq(), BOARD_BOOTCLOCKRUN_SYSPLL_CORE_CLK / BOARD_BOOTCLOCKRUN_CMN_PERI_PCLK_CLK_M);
    print_check("AON_CFG_PCLK", CRM_GetAon_cfg_pclkFreq(), BOARD_BOOTCLOCKRUN_SYSPLL_CORE_CLK / BOARD_BOOTCLOCKRUN_AON_CFG_PCLK_CLK_M);

    // ---- 外设时钟 ----
    print_check("FLASH",  CRM_GetFlashFreq(), BOARD_BOOTCLOCKRUN_SYSPLL_FLASH_CLK / BOARD_BOOTCLOCKRUN_FLASH_CLK_M);
    print_check("PSRAM",  CRM_GetPsramFreq(), BOARD_BOOTCLOCKRUN_SYSPLL_PSRAM_CLK / BOARD_BOOTCLOCKRUN_PSRAM_CLK_M);
    print_check("MTIME",  CRM_GetMtimeFreq(), BOARD_BOOTCLOCKRUN_XTAL24M_CLK / BOARD_BOOTCLOCKRUN_MTIME_CLK_M);
    print_check("SPI0",   CRM_GetSpi0Freq(),  BOARD_BOOTCLOCKRUN_XTAL24M_CLK / (BOARD_BOOTCLOCKRUN_SPI0_CLK_M));
    print_check("SPI1",   CRM_GetSpi1Freq(),  BOARD_BOOTCLOCKRUN_XTAL24M_CLK / (BOARD_BOOTCLOCKRUN_SPI1_CLK_M));
    print_check("UART0",  CRM_GetUart0Freq(), BOARD_BOOTCLOCKRUN_XTAL24M_CLK / (BOARD_BOOTCLOCKRUN_UART0_CLK_M));
    print_check("UART1",  CRM_GetUart1Freq(), BOARD_BOOTCLOCKRUN_XTAL24M_CLK / (BOARD_BOOTCLOCKRUN_UART1_CLK_M));
    print_check("UART2",  CRM_GetUart2Freq(), BOARD_BOOTCLOCKRUN_XTAL24M_CLK / (BOARD_BOOTCLOCKRUN_UART2_CLK_M));
    print_check("SDIOH",  CRM_GetSdiohFreq(), BOARD_BOOTCLOCKRUN_SYSPLL_SDIO_CLK / (BOARD_BOOTCLOCKRUN_SDIOH_CLK_M));

    // ---- 模拟/视频/音频时钟 ----
    print_check("I2S0", CRM_GetI2s0Freq(), BOARD_BOOTCLOCKRUN_XTAL24M_CLK);
    print_check("I2S1", CRM_GetI2s1Freq(), BOARD_BOOTCLOCKRUN_XTAL24M_CLK);
    print_check("VIC",  CRM_GetVicFreq(),  BOARD_BOOTCLOCKRUN_XTAL24M_CLK);
    print_check("DAC",  CRM_GetDacFreq(),  BOARD_BOOTCLOCKRUN_XTAL24M_CLK);
    print_check("ADC",  CRM_GetAdcFreq(),  BOARD_BOOTCLOCKRUN_XTAL24M_CLK);

    CLOGD("------------------------------------------\n");
    CLOGD("Validation complete.\n");
    CLOGD("==========================================\n");
}

int main(){
    logInit(0, 115200);
    enable_GINT();

    Clock_Config_Validation();
    return 0;
}

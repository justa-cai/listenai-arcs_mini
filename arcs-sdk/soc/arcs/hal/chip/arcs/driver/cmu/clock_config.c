/*
 * clock_config.c
 *
 *  Created on: 2021.4.20
 *      Author: USER
 */
#include "clock_config.h"
#include "ClockManager.h"
#include "arcs_ap.h"

/**
 * @brief Initializes the boot clock.
 *
 * This function sets up the boot clock system. It configures the necessary hardware
 * and software components to ensure accurate time tracking from the moment the system
 * boots up. It may be called during the system initialization phase if you want to
 * modify clock time.
 *
 * @note This function does not take any parameters and does not return a value, you can change
 *       the macro in clock_config.h.
 *       Ensure that the necessary hardware is properly set up before calling this function.
 *
 * @warning If this function is not called, or if it fails, the system's timekeeping
 *          might be inaccurate.
 */
void BootClock_Init(){

#if defined(IC_BOARD) && (IC_BOARD == 1)
    __HAL_CRM_USB_CLK_DISABLE();

    HAL_CRM_SetRc32kCaliLength(5);
    HAL_CRM_SetRc32kCaliAutoTrigger(1);

#if BOARD_BOOTCLOCKRUN_VCO_CLK_DEF
    VCO_Init(BOARD_BOOTCLOCKRUN_VCO_CLK_N, BOARD_BOOTCLOCKRUN_VCO_CLK_FRAC_N);
#endif

// Pll clock Configure ************************************************** START
#if BOARD_BOOTCLOCKRUN_SYSPLL_CLK_DEF
    // Init SYSPLL clock
    SYSPLL_Init();
#endif
#if BOARD_BOOTCLOCKRUN_BBPLL_CLK_DEF
    // Init BBPLL clock
    BBPLL_Init();
#endif

#if BOARD_BOOTCLOCKRUN_BBPLL_CORE_CLK_DEF
    CRM_BBPLL_InitCoreSrc(BOARD_BOOTCLOCKRUN_BBPLL_CORE_CFG_PARA);
#endif

#if BOARD_BOOTCLOCKRUN_SYSPLL_CORE_CLK_DEF
    // Init CRM core clock
    CRM_InitCoreSrc(BOARD_BOOTCLOCKRUN_SYSPLL_CORE_CFG_PARA);
#endif
#if BOARD_BOOTCLOCKRUN_SYSPLL_PSRAM_CLK_DEF
    // Init CRM psram clock
    CRM_InitPsramSrc(BOARD_BOOTCLOCKRUN_SYSPLL_PSRAM_CFG_PARA);
#endif
#if BOARD_BOOTCLOCKRUN_SYSPLL_PERI_CLK_DEF
    // Init CRM peri clock
    CRM_InitPeriSrc(BOARD_BOOTCLOCKRUN_SYSPLL_PERI_CFG_PARA);
#endif
#if BOARD_BOOTCLOCKRUN_SYSPLL_FLASH_CLK_DEF
    // Init CRM flash clock
    CRM_InitFlashSrc(BOARD_BOOTCLOCKRUN_SYSPLL_FLASH_CFG_PARA);
#endif

// Pll clock Configure ************************************************** END

// Core clock Configure ************************************************** START
#if BOARD_BOOTCLOCKRUN_CMN_PERI_PCLK_CLK_DEF
    // CMN_PERI_PCLK
    HAL_CRM_SetCmn_peri_pclkClkDiv(BOARD_BOOTCLOCKRUN_CMN_PERI_PCLK_CLK_N, BOARD_BOOTCLOCKRUN_CMN_PERI_PCLK_CLK_M);
#endif

#if BOARD_BOOTCLOCKRUN_AON_CFG_PCLK_CLK_DEF
    // AON_CFG_PCLK
    HAL_CRM_SetAon_cfg_pclkClkDiv(BOARD_BOOTCLOCKRUN_AON_CFG_PCLK_CLK_N, BOARD_BOOTCLOCKRUN_AON_CFG_PCLK_CLK_M);
#endif

#if BOARD_BOOTCLOCKRUN_AP_PERI_PCLK_CLK_DEF
    // AP_PERI_PCLK
    HAL_CRM_SetAp_peri_pclkClkDiv(BOARD_BOOTCLOCKRUN_AP_PERI_PCLK_CLK_N, BOARD_BOOTCLOCKRUN_AP_PERI_PCLK_CLK_M);
#endif

#if BOARD_BOOTCLOCKRUN_HCLK_CLK_DEF
    // HCLK
    HAL_CRM_SetHclkClkDiv(BOARD_BOOTCLOCKRUN_HCLK_CLK_N, BOARD_BOOTCLOCKRUN_HCLK_CLK_M);
    HAL_CRM_SetHclkClkSrc(BOARD_BOOTCLOCKRUN_HCLK_CLK_SRC);
#endif

#if BOARD_BOOTCLOCKRUN_CPU_CLK_DEF
    // CPU
#endif

// Core clock Configure ************************************************** END

// Device clock Configure ************************************************ START
#if BOARD_BOOTCLOCKRUN_PSRAM_CLK_DEF
    // PSRAM
    HAL_CRM_SetPsramClkDiv(BOARD_BOOTCLOCKRUN_PSRAM_CLK_M);
#endif

#if BOARD_BOOTCLOCKRUN_FLASH_CLK_DEF
    // FLASH
    HAL_CRM_SetFlashClkDiv(BOARD_BOOTCLOCKRUN_FLASH_CLK_M);
    HAL_CRM_SetFlashClkSrc(BOARD_BOOTCLOCKRUN_FLASH_CLK_SRC);
#endif

#if BOARD_BOOTCLOCKRUN_MTIME_CLK_DEF
    // MTIME
    HAL_CRM_SetMtimeClkDiv(BOARD_BOOTCLOCKRUN_MTIME_CLK_M);
#endif

#if BOARD_BOOTCLOCKRUN_SPI0_CLK_DEF
    // SPI0
    HAL_CRM_SetSpi0ClkDiv(BOARD_BOOTCLOCKRUN_SPI0_CLK_N, BOARD_BOOTCLOCKRUN_SPI0_CLK_M);
    HAL_CRM_SetSpi0ClkSrc(BOARD_BOOTCLOCKRUN_SPI0_CLK_SRC);
#endif

#if BOARD_BOOTCLOCKRUN_UART0_CLK_DEF
    // UART0
    HAL_CRM_SetUart0ClkDiv(BOARD_BOOTCLOCKRUN_UART0_CLK_N, BOARD_BOOTCLOCKRUN_UART0_CLK_M);
    HAL_CRM_SetUart0ClkSrc(BOARD_BOOTCLOCKRUN_UART0_CLK_SRC);
#endif

#if BOARD_BOOTCLOCKRUN_SPI1_CLK_DEF
    // SPI1
    HAL_CRM_SetSpi1ClkDiv(BOARD_BOOTCLOCKRUN_SPI1_CLK_N, BOARD_BOOTCLOCKRUN_SPI1_CLK_M);
    HAL_CRM_SetSpi1ClkSrc(BOARD_BOOTCLOCKRUN_SPI1_CLK_SRC);
#endif

#if BOARD_BOOTCLOCKRUN_UART1_CLK_DEF
    // UART1
    HAL_CRM_SetUart1ClkDiv(BOARD_BOOTCLOCKRUN_UART1_CLK_N, BOARD_BOOTCLOCKRUN_UART1_CLK_M);
    HAL_CRM_SetUart1ClkSrc(BOARD_BOOTCLOCKRUN_UART1_CLK_SRC);
#endif

#if BOARD_BOOTCLOCKRUN_SPI2_CLK_DEF
    // SPI2
    HAL_CRM_SetSpi2ClkDiv(BOARD_BOOTCLOCKRUN_SPI2_CLK_N, BOARD_BOOTCLOCKRUN_SPI2_CLK_M);
    HAL_CRM_SetSpi2ClkSrc(BOARD_BOOTCLOCKRUN_SPI2_CLK_SRC);
#endif

#if BOARD_BOOTCLOCKRUN_UART2_CLK_DEF
    // UART2
    HAL_CRM_SetUart2ClkDiv(BOARD_BOOTCLOCKRUN_UART2_CLK_N, BOARD_BOOTCLOCKRUN_UART2_CLK_M);
    HAL_CRM_SetUart2ClkSrc(BOARD_BOOTCLOCKRUN_UART2_CLK_SRC);
#endif

#if BOARD_BOOTCLOCKRUN_GPT_T0_CLK_DEF
    // GPT_T0
    HAL_CRM_SetGpt_t0ClkDiv(BOARD_BOOTCLOCKRUN_GPT_T0_CLK_M);
#endif

#if BOARD_BOOTCLOCKRUN_GPT_S_CLK_DEF
    // GPT_S
    HAL_CRM_SetGpt_sClkDiv(BOARD_BOOTCLOCKRUN_GPT_S_CLK_M);
#endif

#if BOARD_BOOTCLOCKRUN_GPADC_CLK_DEF
    // GPADC
    HAL_CRM_SetGpadcClkDiv(BOARD_BOOTCLOCKRUN_GPADC_CLK_M);
#endif

#if BOARD_BOOTCLOCKRUN_IR_TX_CLK_DEF
    // IR_TX
    HAL_CRM_SetIr_txClkDiv(BOARD_BOOTCLOCKRUN_IR_TX_CLK_M);
#endif

#if BOARD_BOOTCLOCKRUN_RGB_CLK_DEF
    // RGB
    HAL_CRM_SetRgbClkDiv(BOARD_BOOTCLOCKRUN_RGB_CLK_M);
    HAL_CRM_SetRgbClkSrc(BOARD_BOOTCLOCKRUN_RGB_CLK_SRC);
#endif

#if BOARD_BOOTCLOCKRUN_SDIO_H_CLK_DEF
    // SDIO_H
    HAL_CRM_SetSdio_hClkDiv(BOARD_BOOTCLOCKRUN_SDIO_H_CLK_N, BOARD_BOOTCLOCKRUN_SDIO_H_CLK_M);
    HAL_CRM_SetSdio_hClkSrc(BOARD_BOOTCLOCKRUN_SDIO_H_CLK_SRC);
#endif

#if BOARD_BOOTCLOCKRUN_QSPI0_CLK_DEF
    // QSPI0
    HAL_CRM_SetQspi0ClkDiv(BOARD_BOOTCLOCKRUN_QSPI0_CLK_N, BOARD_BOOTCLOCKRUN_QSPI0_CLK_M);
    HAL_CRM_SetQspi0ClkSrc(BOARD_BOOTCLOCKRUN_QSPI0_CLK_SRC);
#endif

#if BOARD_BOOTCLOCKRUN_QSPI1_CLK_DEF
    // QSPI1
    HAL_CRM_SetQspi1ClkDiv(BOARD_BOOTCLOCKRUN_QSPI1_CLK_N, BOARD_BOOTCLOCKRUN_QSPI1_CLK_M);
    HAL_CRM_SetQspi1ClkSrc(BOARD_BOOTCLOCKRUN_QSPI1_CLK_SRC);
#endif

// Device clock Configure ************************************************ END
#else
#endif
}
#if CONFIG_PM
struct clock_cfg_reg
{
    volatile uint32_t ip_sysnodef_syspll_cfg0;
    volatile uint32_t ip_sysnodef_syspll_cfg1;
    volatile uint32_t ip_sysnodef_syspll_cfg2;
    volatile uint32_t ip_sysnodef_syspll_cfg3;
    volatile uint32_t ip_sysnodef_syspll_cfg4;
    volatile uint32_t ip_sysnodef_bbpll_cfg0;
    volatile uint32_t ip_sysnodef_bus_clk_cfg0;
    volatile uint32_t ip_sysnodef_bus_clk_cfg1;
    volatile uint32_t ip_sysctrl_peri_clk_cfg0;
    volatile uint32_t ip_ap_cfg_clk_cfg0;
};

static struct clock_cfg_reg clock_cfg_reg_info;

void BootClock_save(void)
{
    clock_cfg_reg_info.ip_sysnodef_syspll_cfg0  =  IP_SYSNODEF->REG_SYSPLL_CFG0.all;
    clock_cfg_reg_info.ip_sysnodef_syspll_cfg1  =  IP_SYSNODEF->REG_SYSPLL_CFG1.all;
    clock_cfg_reg_info.ip_sysnodef_syspll_cfg2  =  IP_SYSNODEF->REG_SYSPLL_CFG2.all;
    clock_cfg_reg_info.ip_sysnodef_syspll_cfg3  =  IP_SYSNODEF->REG_SYSPLL_CFG3.all;
    clock_cfg_reg_info.ip_sysnodef_syspll_cfg4  =  IP_SYSNODEF->REG_SYSPLL_CFG4.all;
    clock_cfg_reg_info.ip_sysnodef_bbpll_cfg0   =  IP_SYSNODEF->REG_BBPLL_CFG0.all;
    clock_cfg_reg_info.ip_sysnodef_bus_clk_cfg0 =  IP_SYSNODEF->REG_BUS_CLK_CFG0.all;
    clock_cfg_reg_info.ip_sysnodef_bus_clk_cfg1 =  IP_SYSNODEF->REG_BUS_CLK_CFG1.all;
    clock_cfg_reg_info.ip_sysctrl_peri_clk_cfg0 =  IP_SYSCTRL->REG_PERI_CLK_CFG0.all;
    clock_cfg_reg_info.ip_ap_cfg_clk_cfg0       =  IP_AP_CFG->REG_CLK_CFG0.all;
}

void BootClock_restore(void)
{
    volatile uint32_t value;
    volatile uint32_t flash_stash_fifo[6];

    IP_CMN_SYS->REG_USB_CTRL1.bit.USBPHY_OUTCLKSEL = 0x0;
    IP_SYSNODEF->REG_SYSPLL_CFG4.all = clock_cfg_reg_info.ip_sysnodef_syspll_cfg4;
    IP_SYSNODEF->REG_SYSPLL_CFG1.all = clock_cfg_reg_info.ip_sysnodef_syspll_cfg1;
    IP_SYSNODEF->REG_SYSPLL_CFG0.bit.SYSPLL_ENABLE = 0x1;
    IP_SYSNODEF->REG_BBPLL_CFG0.bit.BBPLL_ENABLE   = 0x1;
    while (!IP_SYSNODEF->REG_SYSPLL_CFG0.bit.SYSPLL_LOCK);
    IP_SYSNODEF->REG_BUS_CLK_CFG1.all = clock_cfg_reg_info.ip_sysnodef_bus_clk_cfg1 | (1<<CMN_BUSCFG_BUS_CLK_CFG1_DIV_AON_CFG_PCLK_LD_Pos) | (1<<CMN_BUSCFG_BUS_CLK_CFG1_DIV_CMN_PERI_PCLK_LD_Pos);
    value  = IP_AP_CFG->REG_CLK_CFG0.all;
    value &= ~(AP_CFG_CLK_CFG0_DIV_AP_PERI_PCLK_LD_Msk | AP_CFG_CLK_CFG0_DIV_AP_PERI_PCLK_N_Msk | AP_CFG_CLK_CFG0_DIV_AP_PERI_PCLK_M_Msk);
    value |= (clock_cfg_reg_info.ip_ap_cfg_clk_cfg0 & (AP_CFG_CLK_CFG0_DIV_AP_PERI_PCLK_N_Msk | AP_CFG_CLK_CFG0_DIV_AP_PERI_PCLK_M_Msk))
             | (1<<AP_CFG_CLK_CFG0_DIV_AP_PERI_PCLK_LD_Pos);
    IP_AP_CFG->REG_CLK_CFG0.all = value;

    /*hclk*/
    IP_SYSNODEF->REG_BUS_CLK_CFG0.all = (clock_cfg_reg_info.ip_sysnodef_bus_clk_cfg0 & (CMN_BUSCFG_BUS_CLK_CFG0_DIV_HCLK_N_Msk | CMN_BUSCFG_BUS_CLK_CFG0_DIV_HCLK_M_Msk))
                                        | (1<<CMN_BUSCFG_BUS_CLK_CFG0_DIV_HCLK_LD_Pos);
    IP_SYSNODEF->REG_BUS_CLK_CFG0.bit.SEL_HCLK = 1;

    /*flash*/
    value  = clock_cfg_reg_info.ip_sysctrl_peri_clk_cfg0;
    value |= (1<<CMN_SYSCFG_PERI_CLK_CFG0_MTIME_TOGGLE_LD_Pos) | (1<<CMN_SYSCFG_PERI_CLK_CFG0_DIV_FLASH_CLK_LD_Pos);
    IP_SYSCTRL->REG_PERI_CLK_CFG0.all = value;

    for (uint8_t i = 0; i < 6; i++)
        flash_stash_fifo[i] = *(uint32_t*)(0x30000000 + i*4);

    MInvalICache();
    MInvalDCache();
    while(!IP_SYSNODEF->REG_BBPLL_CFG0.bit.BBPLL_LOCK);
}
#endif

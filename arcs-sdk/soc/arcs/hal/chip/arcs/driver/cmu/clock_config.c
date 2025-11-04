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

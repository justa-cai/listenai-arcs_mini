/**
 * @file clock_config.c
 * @brief This file contains the implementation of boot clock initialization routines.
 *        It configures system PLLs and various peripheral clock settings based on board-specific definitions.
 *
 * @details The functions in this file are responsible for initializing the system clock tree,
 *          including core clock domains, peripheral dividers, and clock sources. The configuration
 *          is controlled by preprocessor macros defined in the board configuration header files.
 *
 * @note This file should only be included once per compilation unit due to include guards.
 */
#ifndef INCLUDE_CLOCK_CONFIG_C_
#define INCLUDE_CLOCK_CONFIG_C_

#include "clock_config.h"
#include "ClockManager.h"
#include "venusa_ap.h"

/**
 * @brief Initializes the boot clock configuration for the system.
 *
 * This function performs the following operations:
 *   1. System PLL initialization (if enabled via BOARD_BOOTCLOCKRUN_SYSPLL_CLK_DEF)
 *   2. Core domain PLL configurations (CPU, Peripherals, Flash, PSRAM)
 *   3. Core clock domain configurations (HCLK, CMN_PERI_PCLK, AON_CFG_PCLK)
 *   4. Peripheral-specific clock configurations (PSRAM, MTIME, FLASH, SPIx, UARTx, etc.)
 *   5. Specialized subsystem clock configurations (VIC, GPT, I8080, IR, GPADC, I2S, RGB, DAC, ADC)
 *
 * All configurations are conditionally compiled based on corresponding BOARD_*_DEF macros.
 * Each configuration uses parameters defined in the board's configuration header file.
 */
void BootClock_Init(){

#if BOARD_BOOTCLOCKRUN_SYSPLL_CLK_DEF
    /**
     * @brief Initializes the System PLL block
     *
     * This function sets up the main system PLL which serves as primary clock source
     * for multiple domains. Must be called before any PLL-dependent configurations.
     */
    SYSPLL_Init();
#endif

// PLL CONFIGURE*****************************************************************START
#if BOARD_BOOTCLOCKRUN_SYSPLL_CORE_CLK_DEF
    /**
     * @brief Configures System PLL for Core domain
     *
     * Sets up the PLL parameters specifically optimized for CPU core operation.
     * Uses parameters from BOARD_BOOTCLOCKRUN_SYSPLL_CORE_CFG_PARA array.
     * @param[in] BOARD_BOOTCLOCKRUN_SYSPLL_CORE_CFG_PARA  PLL configuration parameters
     */
	CRM_InitSyspllCore(BOARD_BOOTCLOCKRUN_SYSPLL_CORE_CFG_PARA);
#endif

// CORE CLOCK CONFIGURE**********************************************************START
#if BOARD_BOOTCLOCKRUN_CMN_PERI_PCLK_CLK_DEF
    /**
     * @brief Configures Common Peripheral Private Clock (CMN_PCLK)
     *
     * Controls clock frequency for shared peripheral resources.
     * @param[in] BOARD_BOOTCLOCKRUN_CMN_PERI_PCLK_CLK_N  Post-divider numerator
     * @param[in] BOARD_BOOTCLOCKRUN_CMN_PERI_PCLK_CLK_M  Post-divider denominator
     */
    // CMN_PCLK
    HAL_CRM_SetCmn_pclkClkDiv(BOARD_BOOTCLOCKRUN_CMN_PERI_PCLK_CLK_N, BOARD_BOOTCLOCKRUN_CMN_PERI_PCLK_CLK_M);
#endif

#if BOARD_BOOTCLOCKRUN_AON_CFG_PCLK_CLK_DEF
    /**
     * @brief Configures Always-On Configuration Private Clock (AON_CFG_PCLK)
     *
     * Dedicated clock domain for low-power management circuitry.
     * @param[in] BOARD_BOOTCLOCKRUN_AON_CFG_PCLK_CLK_N  Post-divider numerator
     * @param[in] BOARD_BOOTCLOCKRUN_AON_CFG_PCLK_CLK_M  Post-divider denominator
     */
    // AON_CFG_PCLK
    HAL_CRM_SetAon_cfg_pclkClkDiv(BOARD_BOOTCLOCKRUN_AON_CFG_PCLK_CLK_N, BOARD_BOOTCLOCKRUN_AON_CFG_PCLK_CLK_M);
#endif

#if BOARD_BOOTCLOCKRUN_HCLK_CLK_DEF
    /**
     * @brief Configures HCLK (High-speed Bus Clock)
     *
     * Sets the division ratio between system clock and HCLK bus.
     * @param[in] BOARD_BOOTCLOCKRUN_HCLK_CLK_N  Post-divider numerator
     * @param[in] BOARD_BOOTCLOCKRUN_HCLK_CLK_M  Post-divider denominator
     */
    // HCLK
    HAL_CRM_SetHclkClkDiv(BOARD_BOOTCLOCKRUN_HCLK_CLK_N, BOARD_BOOTCLOCKRUN_HCLK_CLK_M);
    HAL_CRM_SetSysclkClkSrc(BOARD_BOOTCLOCKRUN_HCLK_CLK_SRC);
#endif

#if BOARD_BOOTCLOCKRUN_SYSPLL_PERI_CLK_DEF
    /**
     * @brief Configures System PLL for Peripheral domain
     *
     * Optimizes PLL settings for general purpose peripherals.
     * Uses parameters from BOARD_BOOTCLOCKRUN_SYSPLL_PERI_CFG_PARA array.
     * @param[in] BOARD_BOOTCLOCKRUN_SYSPLL_PERI_CFG_PARA  PLL configuration parameters
     */
    CRM_InitSyspllPeri(BOARD_BOOTCLOCKRUN_SYSPLL_PERI_CFG_PARA);
#endif

#if BOARD_BOOTCLOCKRUN_SYSPLL_FLASH_CLK_DEF
    /**
     * @brief Configures System PLL for Flash interface
     *
     * Specialized PLL configuration for flash access timing requirements.
     * Uses parameters from BOARD_BOOTCLOCKRUN_SYSPLL_FLASH_CFG_PARA array.
     * @param[in] BOARD_BOOTCLOCKRUN_SYSPLL_FLASH_CFG_PARA  PLL configuration parameters
     */
    CRM_InitSyspllFlash(BOARD_BOOTCLOCKRUN_SYSPLL_FLASH_CFG_PARA);
#endif

#if BOARD_BOOTCLOCKRUN_SYSPLL_PSRAM_CLK_DEF
    /**
     * @brief Configures System PLL for PSRAM interface
     *
     * PLL optimization for pseudo-static RAM controller requirements.
     * Uses parameters from BOARD_BOOTCLOCKRUN_SYSPLL_PSRAM_CFG_PARA array.
     * @param[in] BOARD_BOOTCLOCKRUN_SYSPLL_PSRAM_CFG_PARA  PLL configuration parameters
     */
    CRM_InitSyspllPsram(BOARD_BOOTCLOCKRUN_SYSPLL_PSRAM_CFG_PARA);
#endif

#if BOARD_BOOTCLOCKRUN_SYSPLL_SDIO_CLK_DEF
    /**
     * @brief Configures System PLL for SDIO interface
     *
     * High-speed PLL configuration for SD card interface.
     * Uses parameters from BOARD_BOOTCLOCKRUN_SYSPLL_SDIO_CFG_PARA array.
     * @param[in] BOARD_BOOTCLOCKRUN_SYSPLL_SDIO_CFG_PARA  PLL configuration parameters
     */
    CRM_InitSyspllSdio(BOARD_BOOTCLOCKRUN_SYSPLL_SDIO_CFG_PARA);
#endif
// PLL CONFIGURE*****************************************************************END


// CORE CLOCK CONFIGURE**********************************************************END

#if !CONFIG_SKIP_EXTCLK_INIT // DON'T initialize other clocks for ATE_TEST

#if BOARD_BOOTCLOCKRUN_PSRAM_CLK_DEF
    /**
     * @brief Configures PSRAM clock settings
     *
     * Sets both division ratio and clock source selection for PSRAM controller.
     * @param[in] BOARD_BOOTCLOCKRUN_PSRAM_CLK_M  Division factor
     * @param[in] BOARD_BOOTCLOCKRUN_PSRAM_CLK_SRC  Clock source selection
     */
    // PSRAM
    HAL_CRM_SetPsramClkDiv(BOARD_BOOTCLOCKRUN_PSRAM_CLK_M);
    HAL_CRM_SetPsramClkSrc(BOARD_BOOTCLOCKRUN_PSRAM_CLK_SRC);
#endif

#if BOARD_BOOTCLOCKRUN_MTIME_CLK_DEF
    /**
     * @brief Configures Machine Time counter clock
     *
     * Sets division ratio for hardware timer used by operating system tick.
     * @param[in] BOARD_BOOTCLOCKRUN_MTIME_CLK_M  Division factor
     */
    // MTIME
    HAL_CRM_SetMtimeClkDiv(BOARD_BOOTCLOCKRUN_MTIME_CLK_M);
#endif

#if BOARD_BOOTCLOCKRUN_FLASH_CLK_DEF
    /**
     * @brief Configures Flash memory interface clock
     *
     * Sets both division ratio and clock source for flash controller.
     * @param[in] BOARD_BOOTCLOCKRUN_FLASH_CLK_M  Division factor
     * @param[in] BOARD_BOOTCLOCKRUN_FLASH_CLK_SRC  Clock source selection
     */
    // FLASH
    HAL_CRM_SetFlashClkDiv(BOARD_BOOTCLOCKRUN_FLASH_CLK_M);
    HAL_CRM_SetFlashClkSrc(BOARD_BOOTCLOCKRUN_FLASH_CLK_SRC);
#endif

#if BOARD_BOOTCLOCKRUN_SPI0_CLK_DEF
    /**
     * @brief Configures SPI0 interface clock
     *
     * Sets division ratio and clock source for first SPI controller.
     * @param[in] BOARD_BOOTCLOCKRUN_SPI0_CLK_N  Post-divider numerator
     * @param[in] BOARD_BOOTCLOCKRUN_SPI0_CLK_M  Post-divider denominator
     * @param[in] BOARD_BOOTCLOCKRUN_SPI0_CLK_SRC  Clock source selection
     */
    // SPI0
    HAL_CRM_SetSpi0ClkDiv(BOARD_BOOTCLOCKRUN_SPI0_CLK_N, BOARD_BOOTCLOCKRUN_SPI0_CLK_M);
    HAL_CRM_SetSpi0ClkSrc(BOARD_BOOTCLOCKRUN_SPI0_CLK_SRC);
#endif

#if BOARD_BOOTCLOCKRUN_UART0_CLK_DEF
    /**
     * @brief Configures UART0 interface clock
     *
     * Sets division ratio and clock source for first UART controller.
     * @param[in] BOARD_BOOTCLOCKRUN_UART0_CLK_N  Post-divider numerator
     * @param[in] BOARD_BOOTCLOCKRUN_UART0_CLK_M  Post-divider denominator
     * @param[in] BOARD_BOOTCLOCKRUN_UART0_CLK_SRC  Clock source selection
     */
    // UART0
    HAL_CRM_SetUart0ClkDiv(BOARD_BOOTCLOCKRUN_UART0_CLK_N, BOARD_BOOTCLOCKRUN_UART0_CLK_M);
    HAL_CRM_SetUart0ClkSrc(BOARD_BOOTCLOCKRUN_UART0_CLK_SRC);
#endif

#if BOARD_BOOTCLOCKRUN_SPI1_CLK_DEF
    /**
     * @brief Configures SPI1 interface clock
     *
     * Sets division ratio and clock source for second SPI controller.
     * @param[in] BOARD_BOOTCLOCKRUN_SPI1_CLK_N  Post-divider numerator
     * @param[in] BOARD_BOOTCLOCKRUN_SPI1_CLK_M  Post-divider denominator
     * @param[in] BOARD_BOOTCLOCKRUN_SPI1_CLK_SRC  Clock source selection
     */
    // SPI1
    HAL_CRM_SetSpi1ClkDiv(BOARD_BOOTCLOCKRUN_SPI1_CLK_N, BOARD_BOOTCLOCKRUN_SPI1_CLK_M);
    HAL_CRM_SetSpi1ClkSrc(BOARD_BOOTCLOCKRUN_SPI1_CLK_SRC);
#endif

#if BOARD_BOOTCLOCKRUN_UART1_CLK_DEF
    /**
     * @brief Configures UART1 interface clock
     *
     * Sets division ratio and clock source for second UART controller.
     * @param[in] BOARD_BOOTCLOCKRUN_UART1_CLK_N  Post-divider numerator
     * @param[in] BOARD_BOOTCLOCKRUN_UART1_CLK_M  Post-divider denominator
     * @param[in] BOARD_BOOTCLOCKRUN_UART1_CLK_SRC  Clock source selection
     */
    // UART1
    HAL_CRM_SetUart1ClkDiv(BOARD_BOOTCLOCKRUN_UART1_CLK_N, BOARD_BOOTCLOCKRUN_UART1_CLK_M);
    HAL_CRM_SetUart1ClkSrc(BOARD_BOOTCLOCKRUN_UART1_CLK_SRC);
#endif

#if BOARD_BOOTCLOCKRUN_SDIOH_CLK_DEF
    /**
     * @brief Configures SDIOH interface clock
     *
     * Sets division ratio and clock source for high-speed SDIO host controller.
     * @param[in] BOARD_BOOTCLOCKRUN_SDIOH_CLK_N  Post-divider numerator
     * @param[in] BOARD_BOOTCLOCKRUN_SDIOH_CLK_M  Post-divider denominator
     * @param[in] BOARD_BOOTCLOCKRUN_SDIOH_CLK_SRC  Clock source selection
     */
    // SDIOH
    HAL_CRM_SetSdiohClkDiv(BOARD_BOOTCLOCKRUN_SDIOH_CLK_N, BOARD_BOOTCLOCKRUN_SDIOH_CLK_M);
    HAL_CRM_SetSdiohClkSrc(BOARD_BOOTCLOCKRUN_SDIOH_CLK_SRC);
#endif

#if BOARD_BOOTCLOCKRUN_UART2_CLK_DEF
    /**
     * @brief Configures UART2 interface clock
     *
     * Sets division ratio and clock source for third UART controller.
     * @param[in] BOARD_BOOTCLOCKRUN_UART2_CLK_N  Post-divider numerator
     * @param[in] BOARD_BOOTCLOCKRUN_UART2_CLK_M  Post-divider denominator
     * @param[in] BOARD_BOOTCLOCKRUN_UART2_CLK_SRC  Clock source selection
     */
    // UART2
    HAL_CRM_SetUart2ClkDiv(BOARD_BOOTCLOCKRUN_UART2_CLK_N, BOARD_BOOTCLOCKRUN_UART2_CLK_M);
    HAL_CRM_SetUart2ClkSrc(BOARD_BOOTCLOCKRUN_UART2_CLK_SRC);
#endif

#if BOARD_BOOTCLOCKRUN_VIC_OUT_CLK_DEF
    /**
     * @brief Configures Video Controller Output clock
     *
     * Sets division ratio and clock source for video output timing generation.
     * @param[in] BOARD_BOOTCLOCKRUN_VIC_OUT_CLK_M  Division factor
     * @param[in] BOARD_BOOTCLOCKRUN_VIC_OUT_CLK_SRC  Clock source selection
     */
    // VIC_OUT
    HAL_CRM_SetVic_outClkDiv(BOARD_BOOTCLOCKRUN_VIC_OUT_CLK_M);
    HAL_CRM_SetVic_outClkSrc(BOARD_BOOTCLOCKRUN_VIC_OUT_CLK_SRC);
#endif

#if BOARD_BOOTCLOCKRUN_GPT_CLK_DEF
    /**
     * @brief Configures General Purpose Timer clock
     *
     * Sets division ratio for general purpose timer modules.
     * @param[in] BOARD_BOOTCLOCKRUN_GPT_CLK_M  Division factor
     */
    // GPT
    HAL_CRM_SetGptClkDiv(BOARD_BOOTCLOCKRUN_GPT_CLK_M);
#endif

#if BOARD_BOOTCLOCKRUN_I8080_CLK_DEF
    /**
     * @brief Configures Intel 8080 Bus interface clock
     *
     * Sets division ratio and clock source for legacy parallel bus interface.
     * @param[in] BOARD_BOOTCLOCKRUN_I8080_CLK_M  Division factor
     * @param[in] BOARD_BOOTCLOCKRUN_I8080_CLK_SRC  Clock source selection
     */
    // I8080
    HAL_CRM_SetI8080ClkDiv(BOARD_BOOTCLOCKRUN_I8080_CLK_M);
    HAL_CRM_SetI8080ClkSrc(BOARD_BOOTCLOCKRUN_I8080_CLK_SRC);
#endif

#if BOARD_BOOTCLOCKRUN_IR_CLK_DEF
    /**
     * @brief Configures Infrared interface clock
     *
     * Sets division ratio for IR blaster functionality.
     * @param[in] BOARD_BOOTCLOCKRUN_IR_CLK_M  Division factor
     */
    // IR
    HAL_CRM_SetIrClkDiv(BOARD_BOOTCLOCKRUN_IR_CLK_M);
#endif

#if BOARD_BOOTCLOCKRUN_GPADC_CLK_DEF
    /**
     * @brief Configures General Purpose ADC clock
     *
     * Sets division ratio for analog-to-digital converter modules.
     * @param[in] BOARD_BOOTCLOCKRUN_GPADC_CLK_M  Division factor
     */
    // GPADC
    HAL_CRM_SetGpadcClkDiv(BOARD_BOOTCLOCKRUN_GPADC_CLK_M);
#endif

#if BOARD_BOOTCLOCKRUN_I2S0_CLK_DEF
    /**
     * @brief Configures I2S Audio Codec 0 clock source
     *
     * Selects master clock source for first I2S audio interface.
     * @param[in] BOARD_BOOTCLOCKRUN_I2S0_CLK_SRC  Clock source selection
     */
    // I2S0
    HAL_CRM_SetI2s0ClkSrc(BOARD_BOOTCLOCKRUN_I2S0_CLK_SRC);
#endif

#if BOARD_BOOTCLOCKRUN_I2S1_CLK_DEF
    /**
     * @brief Configures I2S Audio Codec 1 clock source
     *
     * Selects master clock source for second I2S audio interface.
     * @param[in] BOARD_BOOTCLOCKRUN_I2S1_CLK_SRC  Clock source selection
     */
    // I2S1
    HAL_CRM_SetI2s1ClkSrc(BOARD_BOOTCLOCKRUN_I2S1_CLK_SRC);
#endif

#if BOARD_BOOTCLOCKRUN_RGB_CLK_DEF
    /**
     * @brief Configures RGB LED interface clock
     *
     * Sets division ratio and clock source for RGB lighting control.
     * @param[in] BOARD_BOOTCLOCKRUN_RGB_CLK_M  Division factor
     * @param[in] BOARD_BOOTCLOCKRUN_RGB_CLK_SRC  Clock source selection
     */
    // RGB
    HAL_CRM_SetRgbClkDiv(BOARD_BOOTCLOCKRUN_RGB_CLK_M);
    HAL_CRM_SetRgbClkSrc(BOARD_BOOTCLOCKRUN_RGB_CLK_SRC);
#endif

#if BOARD_BOOTCLOCKRUN_VIC_CLK_DEF
    /**
     * @brief Configures Video Interconnect clock source
     *
     * Selects master clock source for video processing pipeline.
     * @param[in] BOARD_BOOTCLOCKRUN_VIC_CLK_SRC  Clock source selection
     */
    // VIC
    HAL_CRM_SetVicClkSrc(BOARD_BOOTCLOCKRUN_VIC_CLK_SRC);
#endif

#if BOARD_BOOTCLOCKRUN_QSPI1_CLK_DEF
    /**
     * @brief Configures Quad SPI Flash 1 interface clock
     *
     * Sets division ratio and clock source for primary QSPI flash interface.
     * @param[in] BOARD_BOOTCLOCKRUN_QSPI1_CLK_N  Post-divider numerator
     * @param[in] BOARD_BOOTCLOCKRUN_QSPI1_CLK_M  Post-divider denominator
     * @param[in] BOARD_BOOTCLOCKRUN_QSPI1_CLK_SRC  Clock source selection
     */
    // QSPI1
    HAL_CRM_SetQspi1ClkDiv(BOARD_BOOTCLOCKRUN_QSPI1_CLK_N, BOARD_BOOTCLOCKRUN_QSPI1_CLK_M);
    HAL_CRM_SetQspi1ClkSrc(BOARD_BOOTCLOCKRUN_QSPI1_CLK_SRC);
#endif

#if BOARD_BOOTCLOCKRUN_QSPI0_CLK_DEF
    /**
     * @brief Configures Quad SPI Flash 0 interface clock
     *
     * Sets division ratio and clock source for secondary QSPI flash interface.
     * @param[in] BOARD_BOOTCLOCKRUN_QSPI0_CLK_N  Post-divider numerator
     * @param[in] BOARD_BOOTCLOCKRUN_QSPI0_CLK_M  Post-divider denominator
     * @param[in] BOARD_BOOTCLOCKRUN_QSPI0_CLK_SRC  Clock source selection
     */
    // QSPI0
    HAL_CRM_SetQspi0ClkDiv(BOARD_BOOTCLOCKRUN_QSPI0_CLK_N, BOARD_BOOTCLOCKRUN_QSPI0_CLK_M);
    HAL_CRM_SetQspi0ClkSrc(BOARD_BOOTCLOCKRUN_QSPI0_CLK_SRC);
#endif

#if BOARD_BOOTCLOCKRUN_DAC_CLK_DEF
    /**
     * @brief Configures Digital-to-Analog Converter clock source
     *
     * Selects master clock source for DAC modules.
     * @param[in] BOARD_BOOTCLOCKRUN_DAC_CLK_SRC  Clock source selection
     */
    // DAC
    HAL_CRM_SetDacClkSrc(BOARD_BOOTCLOCKRUN_DAC_CLK_SRC);
#endif

#if BOARD_BOOTCLOCKRUN_ADC_CLK_DEF
    /**
     * @brief Configures Analog-to-Digital Converter clock source
     *
     * Selects master clock source for ADC modules.
     * @param[in] BOARD_BOOTCLOCKRUN_ADC_CLK_SRC  Clock source selection
     */
    // ADC
    HAL_CRM_SetAdcClkSrc(BOARD_BOOTCLOCKRUN_ADC_CLK_SRC);
#endif

#endif //!CONFIG_SKIP_EXTCLK_INIT
}
#endif /* INCLUDE_CLOCK_CONFIG_C_ */

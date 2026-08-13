/**
 * @file ClockManager.c
 * @brief This file contains the implementation of clock management functions for the system.
 *        It includes initialization, configuration, and frequency retrieval routines for various clock domains.
 *
 * @details The functions in this file handle low-level clock control including PLL initialization,
 *          clock source selection, divider configurations, and frequency calculations for different subsystems.
 */
#ifndef INCLUDE_CLOCKMANAGER_C_
#define INCLUDE_CLOCKMANAGER_C_

#include "ClockManager.h"

#define MemBarrier()          __COMPILER_BARRIER()

/**
 * @brief Retrieves the fixed 32KHz RC oscillator frequency.
 *
 * This function returns a predefined constant value representing the fixed 32KHz RC oscillator frequency.
 * @return Predefined 32KHz RC oscillator frequency value.
 */
static uint32_t CRM_GetRC032KFreq(void){
    return BOARD_BOOTCLOCKRUN_RC032K_CLK; // Returns predefined (fixed) clock frequency.
}

/**
 * @brief Retrieves the fixed 32KHz crystal oscillator frequency.
 *
 * This function returns a predefined constant value representing the fixed 32KHz crystal oscillator frequency.
 * @return Predefined 32KHz crystal oscillator frequency value.
 */
static uint32_t CRM_GetXTAL32KFreq(void){
    return BOARD_BOOTCLOCKRUN_XTAL32K_CLK; // Returns predefined (fixed) clock frequency.
}

/**
 * @brief Retrieves the fixed 24MHz RC oscillator frequency.
 *
 * This function returns a predefined constant value representing the fixed 24MHz RC oscillator frequency.
 * @return Predefined 24MHz RC oscillator frequency value.
 */
static uint32_t CRM_GetRC024MFreq(void){
    return BOARD_BOOTCLOCKRUN_RC024M_CLK; // Returns predefined (fixed) clock frequency.
}

/**
 * @brief Retrieves the fixed 24MHz crystal oscillator frequency.
 *
 * This function returns a predefined constant value representing the fixed 24MHz crystal oscillator frequency.
 * @return Predefined 24MHz crystal oscillator frequency value.
 */
static uint32_t CRM_GetXTAL24MFreq(void){
    return BOARD_BOOTCLOCKRUN_XTAL24M_CLK; // Returns predefined (fixed) clock frequency.
}

/**
 * @brief Initializes the System PLL (Phase Locked Loop).
 *
 * This function enables the system PLL and waits for it to lock. It performs basic PLL initialization sequence.
 * @return CSK_DRIVER_OK on success, or error code if initialization fails.
 */
int32_t SYSPLL_Init() {
    if (IP_CMN_SYS->REG_XO24M_OUT_READY.bit.XO24M_OUT_CNT_READY){
        IP_CMN_BUSCFG->REG_SYSPLL_CFG0.bit.SYSPLL_REF_SEL = 0x0;
    }
#if BOARD_BOOTCLOCKRUN_SYSPLL_DIVN_FRAC
    IP_CMN_BUSCFG->REG_SYSPLL_CFG3.bit.SYSPLL_SDM_DIVN_FRAC = BOARD_BOOTCLOCKRUN_SYSPLL_DIVN_FRAC;
    IP_CMN_BUSCFG->REG_SYSPLL_CFG4.bit.SYSPLL_SDM_EN = 1;
#endif
    IP_CMN_BUSCFG->REG_SYSPLL_CFG4.bit.SYSPLL_SDM_DIVN_INTEG = BOARD_BOOTCLOCKRUN_SYSPLL_DIVN_INTEG;
    IP_CMN_BUSCFG->REG_SYSPLL_CFG0.bit.SYSPLL_ENABLE = 0x1;
    while (!IP_CMN_BUSCFG->REG_SYSPLL_CFG0.bit.SYSPLL_LOCK);
    return CSK_DRIVER_OK;
}

/**
 * @brief Determines the CORE24M core clock frequency based on selected source.
 *
 * This function selects between RC or crystal oscillator as the source for the 24MHz core clock.
 * @return Selected clock source frequency (either RC or crystal based on configuration).
 */
static uint32_t CRM_GetCORE24MFreq(void){
    uint32_t src = IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.SEL_24M_SRC;
    switch (src){
    case 0:
        return CRM_GetRC024MFreq();
    case 1:
        return CRM_GetXTAL24MFreq();
    default:
        return 0;
    }
}

/**
 * @brief Determines the CORE32K core clock frequency based on selected source.
 *
 * This function selects between RC or crystal oscillator as the source for the 32KHz core clock.
 * @return Selected clock source frequency (either RC or crystal based on configuration).
 */
static uint32_t CRM_GetCORE32KFreq(void){
    uint32_t src = IP_AON_CTRL->REG_AON_CLK_CTRL.bit.SEL_32KDIV_SRC;
    switch (src){
    case 0:
        return CRM_GetRC032KFreq();
    case 1:
        return CRM_GetXTAL32KFreq();
    default:
        return 0;
    }
}

/**
 * @brief Initializes the system SYSPLL CORE clock with specified divider.
 *
 * @param[in] div Division ratio for the core clock post-divider (must be <= 14)
 * @return CSK_DRIVER_OK on success, CSK_DRIVER_ERROR_PARAMETER if invalid divider
 *
 * This function configured the system PLL core clock post-divider with safety checks.
 */
int32_t CRM_InitSyspllCore(clock_src_syspllcore_div div){
    if (div > 14){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    IP_CMN_BUSCFG->REG_SYSPLL_CFG2.bit.SYSPLL_POSTDIV_SYSTEM_LOAD = 0x0;
    IP_CMN_BUSCFG->REG_SYSPLL_CFG1.bit.SYSPLL_POSTDIV_SYSTEM_DIV_SEL = div;
    IP_CMN_BUSCFG->REG_SYSPLL_CFG0.bit.SYSPLL_POSTDIV_SYSTEM_EN = 0x1;
    MemBarrier();
    IP_CMN_BUSCFG->REG_SYSPLL_CFG2.bit.SYSPLL_POSTDIV_SYSTEM_LOAD = 0x1;
    return CSK_DRIVER_OK;
}

/**
 * @brief Retrieves the system SYSPLL CORE clock frequency.
 *
 * @return Calculated core clock frequency after accounting for post-divider settings,
 *         or 0 if the core clock is disabled.
 */
static uint32_t CRM_GetSyspllCoreFreq(void){
    uint32_t freq = 0;
    uint32_t div = 0;
    if (!IP_CMN_BUSCFG->REG_SYSPLL_CFG0.bit.SYSPLL_POSTDIV_SYSTEM_EN){
        return 0;
    }
    div = IP_CMN_BUSCFG->REG_SYSPLL_CFG1.bit.SYSPLL_POSTDIV_SYSTEM_DIV_SEL + 3;
    freq = BOARD_BOOTCLOCKRUN_SYSPLL_CLK / div;
    return freq;
}

/**
 * @brief Initializes the system SYSPLL PERI clock with specified divider.
 *
 * @param[in] div Division ratio for the peripheral clock post-divider (must be <= 10)
 * @return CSK_DRIVER_OK on success, CSK_DRIVER_ERROR_PARAMETER if invalid divider
 *
 * This function configures the system PLL peripheral clock post-divider with range checking.
 */
int32_t CRM_InitSyspllPeri(clock_src_syspllperi_div div){
    if (div > 10){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    IP_CMN_BUSCFG->REG_SYSPLL_CFG1.bit.SYSPLL_POSTDIV_PERI_DIV_SEL = div;
    IP_CMN_BUSCFG->REG_SYSPLL_CFG0.bit.SYSPLL_POSTDIV_PERI_EN = 0x1;
    return CSK_DRIVER_OK;
}

/**
 * @brief Retrieves the system SYSPLL PERI clock frequency.
 *
 * @return Calculated peripheral clock frequency after accounting for post-divider settings,
 *         or 0 if the peripheral clock is disabled.
 */
static uint32_t CRM_GetSyspllPeriFreq(void){
    uint32_t freq = 0;
    uint32_t div = 0;
    if (!IP_CMN_BUSCFG->REG_SYSPLL_CFG0.bit.SYSPLL_POSTDIV_PERI_EN){
        return 0;
    }
    div = IP_CMN_BUSCFG->REG_SYSPLL_CFG1.bit.SYSPLL_POSTDIV_PERI_DIV_SEL + 6;
    freq = BOARD_BOOTCLOCKRUN_SYSPLL_CLK / div;
    return freq;
}

/**
 * @brief Initializes the system SYSPLL FLASH clock with specified divider.
 *
 * @param[in] div Division ratio for the flash clock post-divider (must be <= 7)
 * @return CSK_DRIVER_OK on success, CSK_DRIVER_ERROR_PARAMETER if invalid divider
 *
 * This function configures the system PLL flash clock post-divider with load/unload sequencing.
 */
int32_t CRM_InitSyspllFlash(clock_src_syspllflash_div div){
    if (div > 7){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    IP_CMN_BUSCFG->REG_SYSPLL_CFG2.bit.SYSPLL_POSTDIV_FLASH_LOAD = 0x0;
    IP_CMN_BUSCFG->REG_SYSPLL_CFG1.bit.SYSPLL_POSTDIV_FLASH_DIV_SEL = div;
    IP_CMN_BUSCFG->REG_SYSPLL_CFG0.bit.SYSPLL_POSTDIV_FLASH_EN = 0x1;
    MemBarrier();
    IP_CMN_BUSCFG->REG_SYSPLL_CFG2.bit.SYSPLL_POSTDIV_FLASH_LOAD = 0x1;

    return CSK_DRIVER_OK;
}

/**
 * @brief Retrieves the system SYSPLL FLASH clock frequency.
 *
 * @return Calculated flash clock frequency after accounting for post-divider settings,
 *         or 0 if the flash clock is disabled.
 */
static uint32_t CRM_GetSyspllFlashFreq(void){
    uint32_t freq = 0;
    uint32_t div = 0;
    if (!IP_CMN_BUSCFG->REG_SYSPLL_CFG0.bit.SYSPLL_POSTDIV_FLASH_EN){
        return 0;
    }
    div = IP_CMN_BUSCFG->REG_SYSPLL_CFG1.bit.SYSPLL_POSTDIV_FLASH_DIV_SEL + 9;
    freq = BOARD_BOOTCLOCKRUN_SYSPLL_CLK / div;
    return freq;
}

/**
 * @brief Initializes the system SYSPLL PSRAM clock with specified divider.
 *
 * @param[in] div Division ratio for the PSRAM clock post-divider (must be <= 7)
 * @return CSK_DRIVER_OK on success, CSK_DRIVER_ERROR_PARAMETER if invalid divider
 *
 * This function configures the system PLL PSRAM clock post-divider with load/unload sequencing.
 */
int32_t CRM_InitSyspllPsram(clock_src_syspllpsram_div div){
    if (div > 7){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    IP_CMN_BUSCFG->REG_SYSPLL_CFG2.bit.SYSPLL_POSTDIV_PSRAM_LOAD = 0x0;
    IP_CMN_BUSCFG->REG_SYSPLL_CFG2.bit.SYSPLL_POSTDIV_PSRAM_DIV_SEL = div;
    IP_CMN_BUSCFG->REG_SYSPLL_CFG0.bit.SYSPLL_POSTDIV_PSRAM_EN = 0x0;
//    IP_CMN_BUSCFG->REG_SYSPLL_CFG2.bit.SYSPLL_POSTDIV_PSRAM_LOAD = 0x1;

    for(volatile uint32_t i=0; i<0x10; i++){
        __NOP();
    }
    IP_CMN_BUSCFG->REG_SYSPLL_CFG0.bit.SYSPLL_POSTDIV_PSRAM_EN = 0x1;
    MemBarrier();

    return CSK_DRIVER_OK;
}

/**
 * @brief Retrieves the system SYSPLL PSRAM clock frequency.
 *
 * @return Calculated PSRAM clock frequency after accounting for post-divider settings,
 *         or 0 if the PSRAM clock is disabled.
 */
static uint32_t CRM_GetSyspllPsramFreq(void){
    uint32_t freq = 0;
    uint32_t div = 0;
    if (!IP_CMN_BUSCFG->REG_SYSPLL_CFG0.bit.SYSPLL_POSTDIV_PSRAM_EN){
        return 0;
    }
    div = IP_CMN_BUSCFG->REG_SYSPLL_CFG2.bit.SYSPLL_POSTDIV_PSRAM_DIV_SEL + 3;
    freq = BOARD_BOOTCLOCKRUN_SYSPLL_CLK / div;
    return freq;
}

/**
 * @brief Initializes the system SYSPLL SDIO clock with specified divider.
 *
 * @param[in] div Division ratio for the SDIO clock post-divider (must be <= 5)
 * @return CSK_DRIVER_OK on success, CSK_DRIVER_ERROR_PARAMETER if invalid divider
 *
 * This function configures the system PLL SDIO clock post-divider with range checking.
 */
int32_t CRM_InitSyspllSdio(clock_src_syspllsdio_div div){
    if (div > 5){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    IP_CMN_BUSCFG->REG_SYSPLL_CFG1.bit.SYSPLL_POSTDIV_SDIO_DIV_SEL = div;
    IP_CMN_BUSCFG->REG_SYSPLL_CFG0.bit.SYSPLL_POSTDIV_SDIO_EN = 0x1;
    return CSK_DRIVER_OK;
}

/**
 * @brief Retrieves the system SYSPLL SDIO clock frequency.
 *
 * @return Calculated SDIO clock frequency after accounting for post-divider settings,
 *         or 0 if the SDIO clock is disabled.
 */
static uint32_t CRM_GetSyspllSdioFreq(void){
    uint32_t freq = 0;
    uint32_t div = 0;
    if (!IP_CMN_BUSCFG->REG_SYSPLL_CFG0.bit.SYSPLL_POSTDIV_SDIO_EN){
        return 0;
    }
    div = IP_CMN_BUSCFG->REG_SYSPLL_CFG1.bit.SYSPLL_POSTDIV_SDIO_DIV_SEL + 3;
    freq = BOARD_BOOTCLOCKRUN_SYSPLL_CLK / div;
    return freq;
}

/**
 * @brief Retrieves the audio PLL clock frequency.
 *
 * @return Fixed 24MHz crystal oscillator frequency used for audio applications.
 */
uint32_t CRM_GetSyspllAudFreq(void){
    return BOARD_BOOTCLOCKRUN_XTAL24M_CLK;
}

/**
 * @brief Retrieves the USB UTMMI clock frequency.
 *
 * @return Fixed 30MHz frequency required for USB OTG interface.
 */
uint32_t CRM_GetUsbUtmiFreq(void){
    return 30000000;
}

/**
 * @brief Retrieves the VIC pixel clock frequency.
 *
 * @return Fixed 24MHz crystal oscillator frequency used for video interface.
 */
uint32_t CRM_GetVicPixelFreq(void){
    return BOARD_BOOTCLOCKRUN_XTAL24M_CLK;
}

/**
 * @brief Retrieves the frequency of a specified clock source.
 *
 * @param[in] src Clock source identifier (enumeration type)
 * @return Frequency corresponding to the specified clock source, or 0 for unsupported sources.
 *
 * This function acts as a dispatcher to various clock source frequency getters.
 */
uint32_t CRM_GetSrcFreq(clock_src_name_t src){
    switch (src){
    case CRM_IpSrcRC032K:
        return CRM_GetRC032KFreq();
    
    case CRM_IpSrcXTAL32K:
        return CRM_GetXTAL32KFreq();
    
    case CRM_IpSrcRC024M:
        return CRM_GetRC024MFreq();
    
    case CRM_IpSrcXTAL24M:
        return CRM_GetXTAL24MFreq();
    
    case CRM_IpSrcCORE32K:
        return CRM_GetCORE32KFreq();
    
    case CRM_IpSrcCORE24M:
        return CRM_GetCORE24MFreq();
    
    case CRM_IpSrcSyspllCore:
        return CRM_GetSyspllCoreFreq();
    
    case CRM_IpSrcSyspllPeri:
        return CRM_GetSyspllPeriFreq();
    
    case CRM_IpSrcSyspllFlash:
        return CRM_GetSyspllFlashFreq();
    
    case CRM_IpSrcSyspllPsram:
        return CRM_GetSyspllPsramFreq();
    
    case CRM_IpSrcSyspllSdio:
        return CRM_GetSyspllSdioFreq();
    
    case CRM_IpSrcSyspllAud:
        return CRM_GetSyspllAudFreq();
    
    case CRM_IpSrcUsbUtmi:
        return CRM_GetUsbUtmiFreq();
    
    case CRM_IpSrcVicPixel:
        return CRM_GetVicPixelFreq();
    
    default:
        return 0;
    }
}

/**
 * @brief Configures the SYSCLK clock source selection.
 *
 * @param[in] src Clock source selection (0=CORE24M, 1=RC024M, 2=SyspllCore)
 * @return CSK_DRIVER_OK on successful configuration.
 *
 * This function selects the primary system clock source by configuring the HCLK mux.
 */
int32_t HAL_CRM_SetSysclkClkSrc(clock_src_name_t src){
    if (src == CRM_IpSrcCORE24M){
        IP_CMN_BUSCFG->REG_BUS_CLK_CFG0.bit.SEL_HCLK = 0;
    }if (src == CRM_IpSrcRC024M){
        IP_CMN_BUSCFG->REG_BUS_CLK_CFG0.bit.SEL_HCLK = 1;
    }if (src == CRM_IpSrcSyspllCore){
        IP_CMN_BUSCFG->REG_BUS_CLK_CFG0.bit.SEL_HCLK = 2;
    }
    return CSK_DRIVER_OK;
}

/**
 * @brief Retrieves the SYSCLK clock frequency.
 *
 * @return Current SYSCLK frequency based on active clock source and configuration.
 *
 * This function gets the current system clock frequency by querying the active source.
 */
uint32_t CRM_GetSysclkFreq(void){
    uint32_t freq = 0;
    
    clock_src_name_t src = 0;
    HAL_CRM_GetSysclkClkConfig(&src );    
    freq = CRM_GetSrcFreq(src);

    return freq;
}

/**
 * @brief Configures the HCLK clock divider settings.
 *
 * @param[in] div_n Numerator for HCLK divider (<=15)
 * @param[in] div_m Denominator for HCLK divider (<=31)
 * @return CSK_DRIVER_OK on success, CSK_DRIVER_ERROR_PARAMETER if values out of range
 *
 * This function programs the HCLK clock divider with safety checks for both numerator and denominator.
 */
int32_t HAL_CRM_SetHclkClkDiv(
    uint32_t div_n, uint32_t div_m){
    IP_CMN_BUSCFG->REG_BUS_CLK_CFG0.bit.DIV_HCLK_LD = 0x0;
    if (div_n > 15){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    IP_CMN_BUSCFG->REG_BUS_CLK_CFG0.bit.DIV_HCLK_N = div_n;
    
    if (div_m > 31){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    IP_CMN_BUSCFG->REG_BUS_CLK_CFG0.bit.DIV_HCLK_M = div_m;
    

    IP_CMN_BUSCFG->REG_BUS_CLK_CFG0.bit.DIV_HCLK_LD = 0x1;
    return CSK_DRIVER_OK;
}

/**
 * @brief Retrieves the HCLK clock frequency.
 *
 * @return Current HCLK frequency calculated from SYSCLK and divider settings.
 *
 * This function calculates the HCLK frequency by applying the current divider settings to the SYSCLK frequency.
 */
uint32_t CRM_GetHclkFreq(void){
    uint32_t freq = 0;
    uint32_t div_n = 1;
    uint32_t div_m = 1;
    

    HAL_CRM_GetHclkClkConfig(&div_n, &div_m);    
    freq = CRM_GetSysclkFreq();

    return freq*div_n/div_m;
}

/**
 * @brief Configures the CMN_PCLK clock divider settings.
 *
 * @param[in] div_n Numerator for CMN_PCLK divider (<=15)
 * @param[in] div_m Denominator for CMN_PCLK divider (<=31)
 * @return CSK_DRIVER_OK on success, CSK_DRIVER_ERROR_PARAMETER if values out of range
 *
 * This function programs the common peripheral private clock divider with range checking.
 */
int32_t HAL_CRM_SetCmn_pclkClkDiv(
    uint32_t div_n, uint32_t div_m){
    IP_CMN_BUSCFG->REG_BUS_CLK_CFG1.bit.DIV_CMN_PERI_PCLK_LD = 0x0;
    if (div_n > 15){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    IP_CMN_BUSCFG->REG_BUS_CLK_CFG1.bit.DIV_CMN_PERI_PCLK_N = div_n;
    
    if (div_m > 31){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    IP_CMN_BUSCFG->REG_BUS_CLK_CFG1.bit.DIV_CMN_PERI_PCLK_M = div_m;
    

    IP_CMN_BUSCFG->REG_BUS_CLK_CFG1.bit.DIV_CMN_PERI_PCLK_LD = 0x1;
    return CSK_DRIVER_OK;
}

/**
 * @brief Retrieves the CMN_PCLK clock frequency.
 *
 * @return Current CMN_PCLK frequency calculated from HCLK and divider settings.
 *
 * This function calculates the common peripheral private clock frequency by applying the current divider settings to the HCLK frequency.
 */
uint32_t CRM_GetCmn_pclkFreq(void){
    uint32_t freq = 0;
    uint32_t div_n = 1;
    uint32_t div_m = 1;
    

    HAL_CRM_GetCmn_pclkClkConfig(&div_n, &div_m);    
    freq = CRM_GetHclkFreq();

    return freq*div_n/div_m;
}

/**
 * @brief Configures the AON_CFG_PCLK clock divider settings.
 *
 * @param[in] div_n Numerator for AON_CFG_PCLK divider (<=31)
 * @param[in] div_m Denominator for AON_CFG_PCLK divider (<=63)
 * @return CSK_DRIVER_OK on success, CSK_DRIVER_ERROR_PARAMETER if values out of range
 *
 * This function programs the always-on configuration private clock divider with extended range support.
 */
int32_t HAL_CRM_SetAon_cfg_pclkClkDiv(
    uint32_t div_n, uint32_t div_m){
    IP_CMN_BUSCFG->REG_BUS_CLK_CFG1.bit.DIV_AON_CFG_PCLK_LD = 0x0;
    if (div_n > 31){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    IP_CMN_BUSCFG->REG_BUS_CLK_CFG1.bit.DIV_AON_CFG_PCLK_N = div_n;
    
    if (div_m > 63){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    IP_CMN_BUSCFG->REG_BUS_CLK_CFG1.bit.DIV_AON_CFG_PCLK_M = div_m;
    

    IP_CMN_BUSCFG->REG_BUS_CLK_CFG1.bit.DIV_AON_CFG_PCLK_LD = 0x1;
    return CSK_DRIVER_OK;
}
    

// Retrieves the AON_CFG_PCLK clock frequency.
/**
 * @brief Retrieves the AON_CFG_PCLK clock frequency.
 *
 * This function returns 0 if AON_CFG_PCLK is disabled, otherwise calculates the frequency based on the source and dividers.
 *
 * @return The calculated frequency in Hz. Returns 0 if disabled.
 */
uint32_t CRM_GetAon_cfg_pclkFreq(void){
    uint32_t freq = 0;
    uint32_t div_n = 1;
    uint32_t div_m = 1;
    
    HAL_CRM_GetAon_cfg_pclkClkConfig(&div_n, &div_m);    
    freq = CRM_GetHclkFreq();

    return freq*div_n/div_m;
}

// Retrieves the CORE0 clock frequency.
/**
 * @brief Retrieves the CORE0 clock frequency.
 *
 * This function returns 0 if CORE0 is disabled, otherwise provides the base HCLK frequency directly.
 *
 * @return The core clock frequency in Hz. Returns 0 if disabled.
 */
uint32_t CRM_GetCore0Freq(void){
    uint32_t freq = 0;
    
    freq = CRM_GetHclkFreq();

    return freq;
}

// Retrieves the CORE1 clock frequency.
/**
 * @brief Retrieves the CORE1 clock frequency.
 *
 * This function returns 0 if CORE1 is disabled, otherwise provides the base HCLK frequency directly.
 *
 * @return The core clock frequency in Hz. Returns 0 if disabled.
 */
uint32_t CRM_GetCore1Freq(void){
    uint32_t freq = 0;
    
    freq = CRM_GetHclkFreq();

    return freq;
}

/**
 * @brief Configures the PSRAM clock divider.
 *
 * Sets the division factor for PSRAM clock generation. The maximum allowed value is 1023.
 *
 * @param div_m Division factor (must be <= 1023)
 * @return CSK_DRIVER_OK on success, CSK_DRIVER_ERROR_PARAMETER on invalid input
 */
int32_t HAL_CRM_SetPsramClkDiv(
    uint32_t div_m){
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG0.bit.DIV_PSRAM_CLK_LD = 0x0;
    if (div_m > 1023){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG0.bit.DIV_PSRAM_CLK_M = div_m;
    
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG0.bit.DIV_PSRAM_CLK_LD = 0x1;
    return CSK_DRIVER_OK;
}

/**
 * @brief Selects the clock source for PSRAM peripheral.
 *
 * Choose between two available clock sources for PSRAM operation.
 *
 * @param src Clock source selection:
 *           @arg CRM_IpSrcCORE24M - Use CORE24M as source
 *           @arg CRM_IpSrcSyspllPsram - Use SyspllPsram as source
 * @return CSK_DRIVER_OK on success
 */
int32_t HAL_CRM_SetPsramClkSrc(clock_src_name_t src){
    if (src == CRM_IpSrcCORE24M){
        IP_CMN_SYSCFG->REG_PERI_CLK_CFG0.bit.SEL_PSRAM_CLK = 0;
    }if (src == CRM_IpSrcSyspllPsram){
        IP_CMN_SYSCFG->REG_PERI_CLK_CFG0.bit.SEL_PSRAM_CLK = 1;
    }
    return CSK_DRIVER_OK;
}

// Retrieves the PSRAM clock frequency.
/**
 * @brief Retrieves the PSRAM clock frequency.
 *
 * Returns 0 if PSRAM clock is disabled, otherwise calculates frequency based on selected source and divider.
 *
 * @return The PSRAM clock frequency in Hz. Returns 0 if disabled.
 */
uint32_t CRM_GetPsramFreq(void){
    uint32_t freq = 0;
    uint32_t div_m = 1;
    if (!HAL_CRM_PsramClkIsEnabled()){
        return 0;
    }
    clock_src_name_t src = 0;
    HAL_CRM_GetPsramClkConfig(&src, &div_m);    
    freq = CRM_GetSrcFreq(src);

    return freq/div_m;
}

/**
 * @brief Configures the MTIME clock divider.
 *
 * Sets the division factor for MTIME clock generation. The maximum allowed value is 63.
 *
 * @param div_m Division factor (must be <= 63)
 * @return CSK_DRIVER_OK on success, CSK_DRIVER_ERROR_PARAMETER on invalid input
 */
int32_t HAL_CRM_SetMtimeClkDiv(
    uint32_t div_m){
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG0.bit.MTIME_TOGGLE_LD = 0x0;
    if (div_m > 63){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG0.bit.DIV_MTIME_TOGGLE_M = div_m;
    
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG0.bit.MTIME_TOGGLE_LD = 0x1;
    return CSK_DRIVER_OK;
}

// Retrieves the MTIME clock frequency.
/**
 * @brief Retrieves the MTIME clock frequency.
 *
 * Returns 0 if MTIME clock is disabled, otherwise calculates frequency based on CORE24M source and divider.
 *
 * @return The MTIME clock frequency in Hz. Returns 0 if disabled.
 */
uint32_t CRM_GetMtimeFreq(void){
    uint32_t freq = 0;
    uint32_t div_m = 1;
    if (!HAL_CRM_MtimeClkIsEnabled()){
        return 0;
    }
    
    HAL_CRM_GetMtimeClkConfig( &div_m);    
    freq = CRM_GetCORE24MFreq();

    return freq/div_m;
}

/**
 * @brief Configures the FLASH clock divider.
 *
 * Sets the division factor for FLASH clock generation. The maximum allowed value is 31.
 *
 * @param div_m Division factor (must be <= 31)
 * @return CSK_DRIVER_OK on success, CSK_DRIVER_ERROR_PARAMETER on invalid input
 */
int32_t HAL_CRM_SetFlashClkDiv(
    uint32_t div_m){
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG0.bit.DIV_FLASHC_CLK_LD = 0x0;
    if (div_m > 31){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG0.bit.DIV_FLASHC_CLK_M = div_m;
    
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG0.bit.DIV_FLASHC_CLK_LD = 0x1;
    return CSK_DRIVER_OK;
}

/**
 * @brief Selects the clock source for FLASH controller.
 *
 * Choose between two available clock sources for FLASH operations.
 *
 * @param src Clock source selection:
 *           @arg CRM_IpSrcCORE24M - Use CORE24M as source
 *           @arg CRM_IpSrcSyspllFlash - Use SyspllFlash as source
 * @return CSK_DRIVER_OK on success
 */
int32_t HAL_CRM_SetFlashClkSrc(clock_src_name_t src){
    if (src == CRM_IpSrcCORE24M){
        IP_CMN_SYSCFG->REG_PERI_CLK_CFG0.bit.SEL_FLASHC_CLK = 0;
    }if (src == CRM_IpSrcSyspllFlash){
        IP_CMN_SYSCFG->REG_PERI_CLK_CFG0.bit.SEL_FLASHC_CLK = 1;
    }
    return CSK_DRIVER_OK;
}

// Retrieves the FLASH clock frequency.
/**
 * @brief Retrieves the FLASH clock frequency.
 *
 * Returns 0 if FLASH clock is disabled, otherwise calculates frequency based on selected source and divider.
 *
 * @return The FLASH clock frequency in Hz. Returns 0 if disabled.
 */
uint32_t CRM_GetFlashFreq(void){
    uint32_t freq = 0;
    uint32_t div_m = 1;
    if (!HAL_CRM_FlashClkIsEnabled()){
        return 0;
    }
    clock_src_name_t src = 0;
    HAL_CRM_GetFlashClkConfig(&src, &div_m);    
    freq = CRM_GetSrcFreq(src);

    return freq/div_m;
}

/**
 * @brief Configures the SPI0 clock divider.
 *
 * Sets both N and M division factors for SPI0 clock generation. Max values: N=7, M=15.
 *
 * @param div_n Prescaler numerator (must be <= 7)
 * @param div_m Prescaler denominator (must be <= 15)
 * @return CSK_DRIVER_OK on success, CSK_DRIVER_ERROR_PARAMETER on invalid input
 */
int32_t HAL_CRM_SetSpi0ClkDiv(
    uint32_t div_n, uint32_t div_m){
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG1.bit.DIV_SPI0_CLK_LD = 0x0;
    if (div_n > 7){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG1.bit.DIV_SPI0_CLK_N = div_n;
    
    if (div_m > 15){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG1.bit.DIV_SPI0_CLK_M = div_m;
    
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG1.bit.DIV_SPI0_CLK_LD = 0x1;
    return CSK_DRIVER_OK;
}

/**
 * @brief Selects the clock source for SPI0 peripheral.
 *
 * Choose between two available clock sources for SPI0 operations.
 *
 * @param src Clock source selection:
 *           @arg CRM_IpSrcCORE24M - Use CORE24M as source
 *           @arg CRM_IpSrcSyspllPeri - Use SyspllPeri as source
 * @return CSK_DRIVER_OK on success
 */
int32_t HAL_CRM_SetSpi0ClkSrc(clock_src_name_t src){
    if (src == CRM_IpSrcCORE24M){
        IP_CMN_SYSCFG->REG_PERI_CLK_CFG1.bit.SEL_SPI0_CLK = 0;
    }if (src == CRM_IpSrcSyspllPeri){
        IP_CMN_SYSCFG->REG_PERI_CLK_CFG1.bit.SEL_SPI0_CLK = 1;
    }
    return CSK_DRIVER_OK;
}

// Retrieves the SPI0 clock frequency.
/**
 * @brief Retrieves the SPI0 clock frequency.
 *
 * Returns 0 if SPI0 clock is disabled, otherwise calculates frequency based on selected source and dividers.
 * Formula: SourceFrequency * (div_n / div_m)
 *
 * @return The SPI0 clock frequency in Hz. Returns 0 if disabled.
 */
uint32_t CRM_GetSpi0Freq(void){
    uint32_t freq = 0;
    uint32_t div_n = 1;
    uint32_t div_m = 1;
    if (!HAL_CRM_Spi0ClkIsEnabled()){
        return 0;
    }
    clock_src_name_t src = 0;
    HAL_CRM_GetSpi0ClkConfig(&src,&div_n, &div_m);    
    freq = CRM_GetSrcFreq(src);

    return freq*div_n/div_m;
}

/**
 * @brief Configures the UART0 clock divider.
 *
 * Sets both N and M division factors for UART0 clock generation. Max values: N=511, M=1023.
 *
 * @param div_n Prescaler numerator (must be <= 511)
 * @param div_m Prescaler denominator (must be <= 1023)
 * @return CSK_DRIVER_OK on success, CSK_DRIVER_ERROR_PARAMETER on invalid input
 */
int32_t HAL_CRM_SetUart0ClkDiv(
    uint32_t div_n, uint32_t div_m){
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG1.bit.DIV_UART0_CLK_LD = 0x0;
    if (div_n > 511){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG1.bit.DIV_UART0_CLK_N = div_n;
    
    if (div_m > 1023){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG1.bit.DIV_UART0_CLK_M = div_m;
    
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG1.bit.DIV_UART0_CLK_LD = 0x1;
    return CSK_DRIVER_OK;
}

/**
 * @brief Selects the clock source for UART0 peripheral.
 *
 * Choose between two available clock sources for UART0 operations.
 *
 * @param src Clock source selection:
 *           @arg CRM_IpSrcCORE24M - Use CORE24M as source
 *           @arg CRM_IpSrcSyspllPeri - Use SyspllPeri as source
 * @return CSK_DRIVER_OK on success
 */
int32_t HAL_CRM_SetUart0ClkSrc(clock_src_name_t src){
    if (src == CRM_IpSrcCORE24M){
        IP_CMN_SYSCFG->REG_PERI_CLK_CFG1.bit.SEL_UART0_CLK = 0;
    }if (src == CRM_IpSrcSyspllPeri){
        IP_CMN_SYSCFG->REG_PERI_CLK_CFG1.bit.SEL_UART0_CLK = 1;
    }
    return CSK_DRIVER_OK;
}

// Retrieves the UART0 clock frequency.
/**
 * @brief Retrieves the UART0 clock frequency.
 *
 * Returns 0 if UART0 clock is disabled, otherwise calculates frequency based on selected source and dividers.
 * Formula: SourceFrequency * (div_n / div_m)
 *
 * @return The UART0 clock frequency in Hz. Returns 0 if disabled.
 */
uint32_t CRM_GetUart0Freq(void){
    uint32_t freq = 0;
    uint32_t div_n = 1;
    uint32_t div_m = 1;
    if (!HAL_CRM_Uart0ClkIsEnabled()){
        return 0;
    }
    clock_src_name_t src = 0;
    HAL_CRM_GetUart0ClkConfig(&src,&div_n, &div_m);    
    freq = CRM_GetSrcFreq(src);

    return freq*div_n/div_m;
}

/**
 * @brief Configures the SPI1 clock divider.
 *
 * Sets both N and M division factors for SPI1 clock generation. Max values: N=7, M=15.
 *
 * @param div_n Prescaler numerator (must be <= 7)
 * @param div_m Prescaler denominator (must be <= 15)
 * @return CSK_DRIVER_OK on success, CSK_DRIVER_ERROR_PARAMETER on invalid input
 */
int32_t HAL_CRM_SetSpi1ClkDiv(
    uint32_t div_n, uint32_t div_m){
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG2.bit.DIV_SPI1_CLK_LD = 0x0;
    if (div_n > 7){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG2.bit.DIV_SPI1_CLK_N = div_n;
    
    if (div_m > 15){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG2.bit.DIV_SPI1_CLK_M = div_m;
    
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG2.bit.DIV_SPI1_CLK_LD = 0x1;
    return CSK_DRIVER_OK;
}

/**
 * @brief Selects the clock source for SPI1 peripheral.
 *
 * Choose between two available clock sources for SPI1 operations.
 *
 * @param src Clock source selection:
 *           @arg CRM_IpSrcCORE24M - Use CORE24M as source
 *           @arg CRM_IpSrcSyspllPeri - Use SyspllPeri as source
 * @return CSK_DRIVER_OK on success
 */
int32_t HAL_CRM_SetSpi1ClkSrc(clock_src_name_t src){
    if (src == CRM_IpSrcCORE24M){
        IP_CMN_SYSCFG->REG_PERI_CLK_CFG2.bit.SEL_SPI1_CLK = 0;
    }if (src == CRM_IpSrcSyspllPeri){
        IP_CMN_SYSCFG->REG_PERI_CLK_CFG2.bit.SEL_SPI1_CLK = 1;
    }
    return CSK_DRIVER_OK;
}

// Retrieves the SPI1 clock frequency.
/**
 * @brief Retrieves the SPI1 clock frequency.
 *
 * Returns 0 if SPI1 clock is disabled, otherwise calculates frequency based on selected source and dividers.
 * Formula: SourceFrequency * (div_n / div_m)
 *
 * @return The SPI1 clock frequency in Hz. Returns 0 if disabled.
 */
uint32_t CRM_GetSpi1Freq(void){
    uint32_t freq = 0;
    uint32_t div_n = 1;
    uint32_t div_m = 1;
    if (!HAL_CRM_Spi1ClkIsEnabled()){
        return 0;
    }
    clock_src_name_t src = 0;
    HAL_CRM_GetSpi1ClkConfig(&src,&div_n, &div_m);    
    freq = CRM_GetSrcFreq(src);

    return freq*div_n/div_m;
}

/**
 * @brief Configures the UART1 clock divider.
 *
 * Sets both N and M division factors for UART1 clock generation. Max values: N=511, M=1023.
 *
 * @param div_n Prescaler numerator (must be <= 511)
 * @param div_m Prescaler denominator (must be <= 1023)
 * @return CSK_DRIVER_OK on success, CSK_DRIVER_ERROR_PARAMETER on invalid input
 */
int32_t HAL_CRM_SetUart1ClkDiv(
    uint32_t div_n, uint32_t div_m){
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG2.bit.DIV_UART1_CLK_LD = 0x0;
    if (div_n > 511){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG2.bit.DIV_UART1_CLK_N = div_n;
    
    if (div_m > 1023){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG2.bit.DIV_UART1_CLK_M = div_m;
    
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG2.bit.DIV_UART1_CLK_LD = 0x1;
    return CSK_DRIVER_OK;
}

/**
 * @brief Selects the clock source for UART1 peripheral.
 *
 * Choose between two available clock sources for UART1 operations.
 *
 * @param src Clock source selection:
 *           @arg CRM_IpSrcCORE24M - Use CORE24M as source
 *           @arg CRM_IpSrcSyspllPeri - Use SyspllPeri as source
 * @return CSK_DRIVER_OK on success
 */
int32_t HAL_CRM_SetUart1ClkSrc(clock_src_name_t src){
    if (src == CRM_IpSrcCORE24M){
        IP_CMN_SYSCFG->REG_PERI_CLK_CFG2.bit.SEL_UART1_CLK = 0;
    }if (src == CRM_IpSrcSyspllPeri){
        IP_CMN_SYSCFG->REG_PERI_CLK_CFG2.bit.SEL_UART1_CLK = 1;
    }
    return CSK_DRIVER_OK;
}

// Retrieves the UART1 clock frequency.
/**
 * @brief Retrieves the UART1 clock frequency.
 *
 * Returns 0 if UART1 clock is disabled, otherwise calculates frequency based on selected source and dividers.
 * Formula: SourceFrequency * (div_n / div_m)
 *
 * @return The UART1 clock frequency in Hz. Returns 0 if disabled.
 */
uint32_t CRM_GetUart1Freq(void){
    uint32_t freq = 0;
    uint32_t div_n = 1;
    uint32_t div_m = 1;
    if (!HAL_CRM_Uart1ClkIsEnabled()){
        return 0;
    }
    clock_src_name_t src = 0;
    HAL_CRM_GetUart1ClkConfig(&src,&div_n, &div_m);    
    freq = CRM_GetSrcFreq(src);

    return freq*div_n/div_m;
}

/**
 * @brief Configures the SDIOH clock divider.
 *
 * Sets both N and M division factors for SDIOH clock generation. Max values: N=7, M=15.
 *
 * @param div_n Prescaler numerator (must be <= 7)
 * @param div_m Prescaler denominator (must be <= 15)
 * @return CSK_DRIVER_OK on success, CSK_DRIVER_ERROR_PARAMETER on invalid input
 */
int32_t HAL_CRM_SetSdiohClkDiv(
    uint32_t div_n, uint32_t div_m){
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG3.bit.DIV_SDIOH_CLK2X_LD = 0x0;
    if (div_n > 7){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG3.bit.DIV_SDIOH_CLK2X_N = div_n;
    
    if (div_m > 15){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG3.bit.DIV_SDIOH_CLK2X_M = div_m;
    
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG3.bit.DIV_SDIOH_CLK2X_LD = 0x1;
    return CSK_DRIVER_OK;
}

/**
 * @brief Selects the clock source for SDIOH peripheral.
 *
 * Choose between two available clock sources for SDIOH operations.
 *
 * @param src Clock source selection:
 *           @arg CRM_IpSrcCORE24M - Use CORE24M as source
 *           @arg CRM_IpSrcSyspllSdio - Use SyspllSdio as source
 * @return CSK_DRIVER_OK on success
 */
int32_t HAL_CRM_SetSdiohClkSrc(clock_src_name_t src){
    if (src == CRM_IpSrcCORE24M){
        IP_CMN_SYSCFG->REG_PERI_CLK_CFG3.bit.SEL_SDIOH_CLK2X = 0;
    }if (src == CRM_IpSrcSyspllSdio){
        IP_CMN_SYSCFG->REG_PERI_CLK_CFG3.bit.SEL_SDIOH_CLK2X = 1;
    }
    return CSK_DRIVER_OK;
}

// Retrieves the SDIOH clock frequency.
/**
 * @brief Retrieves the SDIOH clock frequency.
 *
 * Returns 0 if SDIOH clock is disabled, otherwise calculates frequency based on selected source and dividers.
 * Formula: SourceFrequency * (div_n / div_m)
 *
 * @return The SDIOH clock frequency in Hz. Returns 0 if disabled.
 */
uint32_t CRM_GetSdiohFreq(void){
    uint32_t freq = 0;
    uint32_t div_n = 1;
    uint32_t div_m = 1;
    if (!HAL_CRM_SdiohClkIsEnabled()){
        return 0;
    }
    clock_src_name_t src = 0;
    HAL_CRM_GetSdiohClkConfig(&src,&div_n, &div_m);    
    freq = CRM_GetSrcFreq(src);

    return freq*div_n/div_m;
}

/**
 * @brief Configures the UART2 clock divider.
 *
 * Sets both N and M division factors for UART2 clock generation. Max values: N=511, M=1023.
 *
 * @param div_n Prescaler numerator (must be <= 511)
 * @param div_m Prescaler denominator (must be <= 1023)
 * @return CSK_DRIVER_OK on success, CSK_DRIVER_ERROR_PARAMETER on invalid input
 */
int32_t HAL_CRM_SetUart2ClkDiv(
    uint32_t div_n, uint32_t div_m){
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG3.bit.DIV_UART2_CLK_LD = 0x0;
    if (div_n > 511){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG3.bit.DIV_UART2_CLK_N = div_n;
    
    if (div_m > 1023){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG3.bit.DIV_UART2_CLK_M = div_m;
    
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG3.bit.DIV_UART2_CLK_LD = 0x1;
    return CSK_DRIVER_OK;
}

/**
 * @brief Selects the clock source for UART2 peripheral.
 *
 * Choose between two available clock sources for UART2 operations.
 *
 * @param src Clock source selection:
 *           @arg CRM_IpSrcCORE24M - Use CORE24M as source
 *           @arg CRM_IpSrcSyspllPeri - Use SyspllPeri as source
 * @return CSK_DRIVER_OK on success
 */
int32_t HAL_CRM_SetUart2ClkSrc(clock_src_name_t src){
    if (src == CRM_IpSrcCORE24M){
        IP_CMN_SYSCFG->REG_PERI_CLK_CFG3.bit.SEL_UART2_CLK = 0;
    }if (src == CRM_IpSrcSyspllPeri){
        IP_CMN_SYSCFG->REG_PERI_CLK_CFG3.bit.SEL_UART2_CLK = 1;
    }
    return CSK_DRIVER_OK;
}

// Retrieves the UART2 clock frequency.
/**
 * @brief Retrieves the UART2 clock frequency.
 *
 * Returns 0 if UART2 clock is disabled, otherwise calculates frequency based on selected source and dividers.
 * Formula: SourceFrequency * (div_n / div_m)
 *
 * @return The UART2 clock frequency in Hz. Returns 0 if disabled.
 */
uint32_t CRM_GetUart2Freq(void){
    uint32_t freq = 0;
    uint32_t div_n = 1;
    uint32_t div_m = 1;
    if (!HAL_CRM_Uart2ClkIsEnabled()){
        return 0;
    }
    clock_src_name_t src = 0;
    HAL_CRM_GetUart2ClkConfig(&src,&div_n, &div_m);    
    freq = CRM_GetSrcFreq(src);

    return freq*div_n/div_m;
}

/**
 * @brief Configures the VIC_OUT clock divider.
 *
 * Sets the division factor for VIC_OUT clock generation. The maximum allowed value is 511.
 *
 * @param div_m Division factor (must be <= 511)
 * @return CSK_DRIVER_OK on success, CSK_DRIVER_ERROR_PARAMETER on invalid input
 */
int32_t HAL_CRM_SetVic_outClkDiv(
    uint32_t div_m){
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG4.bit.DIV_VIC_OUT_CLK_LD = 0x0;
    if (div_m > 511){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG4.bit.DIV_VIC_OUT_CLK_M = div_m;
    
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG4.bit.DIV_VIC_OUT_CLK_LD = 0x1;
    return CSK_DRIVER_OK;
}

/**
 * @brief Selects the clock source for VIC_OUT peripheral.
 *
 * Choose between two available clock sources for VIC_OUT operations.
 *
 * @param src Clock source selection:
 *           @arg CRM_IpSrcCORE24M - Use CORE24M as source
 *           @arg CRM_IpSrcSyspllPeri - Use SyspllPeri as source
 * @return CSK_DRIVER_OK on success
 */
int32_t HAL_CRM_SetVic_outClkSrc(clock_src_name_t src){
    if (src == CRM_IpSrcCORE24M){
        IP_CMN_SYSCFG->REG_PERI_CLK_CFG4.bit.SEL_VIC_OUT_CLK = 0;
    }if (src == CRM_IpSrcSyspllPeri){
        IP_CMN_SYSCFG->REG_PERI_CLK_CFG4.bit.SEL_VIC_OUT_CLK = 1;
    }
    return CSK_DRIVER_OK;
}

// Retrieves the VIC_OUT clock frequency.
/**
 * @brief Retrieves the VIC_OUT clock frequency.
 *
 * Returns 0 if VIC_OUT clock is disabled, otherwise calculates frequency based on selected source and divider.
 *
 * @return The VIC_OUT clock frequency in Hz. Returns 0 if disabled.
 */
uint32_t CRM_GetVic_outFreq(void){
    uint32_t freq = 0;
    uint32_t div_m = 1;
    if (!HAL_CRM_Vic_outClkIsEnabled()){
        return 0;
    }
    clock_src_name_t src = 0;
    HAL_CRM_GetVic_outClkConfig(&src, &div_m);    
    freq = CRM_GetSrcFreq(src);

    return freq/div_m;
}

/**
 * @brief Configures the GPT clock divider.
 *
 * Sets the division factor for GPT timer clock generation. The maximum allowed value is 31.
 *
 * @param div_m Division factor (must be <= 31)
 * @return CSK_DRIVER_OK on success, CSK_DRIVER_ERROR_PARAMETER on invalid input
 */
int32_t HAL_CRM_SetGptClkDiv(
    uint32_t div_m){
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG4.bit.DIV_GPT_T0_CLK_LD = 0x0;
    if (div_m > 31){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG4.bit.DIV_GPT_T0_CLK_M = div_m;
    
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG4.bit.DIV_GPT_T0_CLK_LD = 0x1;
    return CSK_DRIVER_OK;
}

// Retrieves the GPT clock frequency.
/**
 * @brief Retrieves the GPT clock frequency.
 *
 * Returns 0 if GPT clock is disabled, otherwise calculates frequency based on CORE24M source and divider.
 *
 * @return The GPT clock frequency in Hz. Returns 0 if disabled.
 */
uint32_t CRM_GetGptFreq(void){
    uint32_t freq = 0;
    uint32_t div_m = 1;
    if (!HAL_CRM_GptClkIsEnabled()){
        return 0;
    }
    
    HAL_CRM_GetGptClkConfig( &div_m);    
    freq = CRM_GetCORE24MFreq();

    return freq/div_m;
}

/**
 * @brief Configures the I8080 clock divider.
 *
 * Sets the division factor for I8080 interface clock generation. The maximum allowed value is 15.
 *
 * @param div_m Division factor (must be <= 15)
 * @return CSK_DRIVER_OK on success, CSK_DRIVER_ERROR_PARAMETER on invalid input
 */
int32_t HAL_CRM_SetI8080ClkDiv(
    uint32_t div_m){
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG5.bit.DIV_I8080_CLK_LD = 0x0;
    if (div_m > 15){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG5.bit.DIV_I8080_CLK_M = div_m;
    
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG5.bit.DIV_I8080_CLK_LD = 0x1;
    return CSK_DRIVER_OK;
}

/**
 * @brief Selects the clock source for I8080 interface.
 *
 * Choose between two available clock sources for I8080 operations.
 *
 * @param src Clock source selection:
 *           @arg CRM_IpSrcCORE24M - Use CORE24M as source
 *           @arg CRM_IpSrcSyspllPeri - Use SyspllPeri as source
 * @return CSK_DRIVER_OK on success
 */
int32_t HAL_CRM_SetI8080ClkSrc(clock_src_name_t src){
    if (src == CRM_IpSrcCORE24M){
        IP_CMN_SYSCFG->REG_PERI_CLK_CFG5.bit.SEL_I8080_CLK = 0;
    }if (src == CRM_IpSrcSyspllPeri){
        IP_CMN_SYSCFG->REG_PERI_CLK_CFG5.bit.SEL_I8080_CLK = 1;
    }
    return CSK_DRIVER_OK;
}

// Retrieves the I8080 clock frequency.
/**
 * @brief Retrieves the I8080 clock frequency.
 *
 * Returns 0 if I8080 clock is disabled, otherwise calculates frequency based on selected source and divider.
 *
 * @return The I8080 clock frequency in Hz. Returns 0 if disabled.
 */
uint32_t CRM_GetI8080Freq(void){
    uint32_t freq = 0;
    uint32_t div_m = 1;
    if (!HAL_CRM_I8080ClkIsEnabled()){
        return 0;
    }
    clock_src_name_t src = 0;
    HAL_CRM_GetI8080ClkConfig(&src, &div_m);    
    freq = CRM_GetSrcFreq(src);

    return freq/div_m;
}

/**
 * @brief Configures the IR clock divider.
 *
 * Sets the division factor for IR transmitter clock generation. The maximum allowed value is 63.
 *
 * @param div_m Division factor (must be <= 63)
 * @return CSK_DRIVER_OK on success, CSK_DRIVER_ERROR_PARAMETER on invalid input
 */
int32_t HAL_CRM_SetIrClkDiv(
    uint32_t div_m){
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG5.bit.DIV_IR_CLK_TX_LD = 0x0;
    if (div_m > 63){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG5.bit.DIV_IR_CLK_TX_M = div_m;
    
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG5.bit.DIV_IR_CLK_TX_LD = 0x1;
    return CSK_DRIVER_OK;
}

// Retrieves the IR clock frequency.
/**
 * @brief Retrieves the IR clock frequency.
 *
 * Returns 0 if IR clock is disabled, otherwise calculates frequency based on CORE24M source and divider.
 *
 * @return The IR clock frequency in Hz. Returns 0 if disabled.
 */
uint32_t CRM_GetIrFreq(void){
    uint32_t freq = 0;
    uint32_t div_m = 1;
    if (!HAL_CRM_IrClkIsEnabled()){
        return 0;
    }
    
    HAL_CRM_GetIrClkConfig( &div_m);    
    freq = CRM_GetCORE24MFreq();

    return freq/div_m;
}

/**
 * @brief Configures the GPADC clock divider.
 *
 * Sets the division factor for General Purpose ADC clock generation. The maximum allowed value is 1023.
 *
 * @param div_m Division factor (must be <= 1023)
 * @return CSK_DRIVER_OK on success, CSK_DRIVER_ERROR_PARAMETER on invalid input
 */
int32_t HAL_CRM_SetGpadcClkDiv(
    uint32_t div_m){
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG5.bit.DIV_GPADC_CLK_LD = 0x0;
    if (div_m > 1023){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG5.bit.DIV_GPADC_CLK_M = div_m;
    
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG5.bit.DIV_GPADC_CLK_LD = 0x1;
    return CSK_DRIVER_OK;
}

// Retrieves the GPADC clock frequency.
/**
 * @brief Retrieves the GPADC clock frequency.
 *
 * Returns 0 if GPADC clock is disabled, otherwise calculates frequency based on CORE24M source and divider.
 *
 * @return The GPADC clock frequency in Hz. Returns 0 if disabled.
 */
uint32_t CRM_GetGpadcFreq(void){
    uint32_t freq = 0;
    uint32_t div_m = 1;
    if (!HAL_CRM_GpadcClkIsEnabled()){
        return 0;
    }
    
    HAL_CRM_GetGpadcClkConfig( &div_m);    
    freq = CRM_GetCORE24MFreq();

    return freq/div_m;
}

/**
 * @brief Selects the clock source for I2S0 peripheral.
 *
 * Choose between two available clock sources for I2S0 audio operations.
 *
 * @param src Clock source selection:
 *           @arg CRM_IpSrcCORE24M - Use CORE24M as source
 *           @arg CRM_IpSrcSyspllAud - Use SyspllAud as source
 * @return CSK_DRIVER_OK on success
 */
int32_t HAL_CRM_SetI2s0ClkSrc(clock_src_name_t src){
    if (src == CRM_IpSrcCORE24M){
        IP_CMN_SYSCFG->REG_PERI_CLK_CFG6.bit.SEL_I2S0_MCLK = 0;
    }if (src == CRM_IpSrcSyspllAud){
        IP_CMN_SYSCFG->REG_PERI_CLK_CFG6.bit.SEL_I2S0_MCLK = 1;
    }
    return CSK_DRIVER_OK;
}

// Retrieves the I2S0 clock frequency.
/**
 * @brief Retrieves the I2S0 clock frequency.
 *
 * Returns 0 if I2S0 clock is disabled, otherwise returns the raw source frequency without division.
 *
 * @return The I2S0 master clock frequency in Hz. Returns 0 if disabled.
 */
uint32_t CRM_GetI2s0Freq(void){
    uint32_t freq = 0;
    if (!HAL_CRM_I2s0ClkIsEnabled()){
        return 0;
    }
    clock_src_name_t src = 0;
    HAL_CRM_GetI2s0ClkConfig(&src );    
    freq = CRM_GetSrcFreq(src);

    return freq;
}

/**
 * @brief Selects the clock source for I2S1 peripheral.
 *
 * Choose between two available clock sources for I2S1 audio operations.
 *
 * @param src Clock source selection:
 *           @arg CRM_IpSrcCORE24M - Use CORE24M as source
 *           @arg CRM_IpSrcSyspllAud - Use SyspllAud as source
 * @return CSK_DRIVER_OK on success
 */
int32_t HAL_CRM_SetI2s1ClkSrc(clock_src_name_t src){
    if (src == CRM_IpSrcCORE24M){
        IP_CMN_SYSCFG->REG_PERI_CLK_CFG6.bit.SEL_I2S1_MCLK = 0;
    }if (src == CRM_IpSrcSyspllAud){
        IP_CMN_SYSCFG->REG_PERI_CLK_CFG6.bit.SEL_I2S1_MCLK = 1;
    }
    return CSK_DRIVER_OK;
}

// Retrieves the I2S1 clock frequency.
/**
 * @brief Retrieves the I2S1 clock frequency.
 *
 * Returns 0 if I2S1 clock is disabled, otherwise returns the raw source frequency without division.
 *
 * @return The I2S1 master clock frequency in Hz. Returns 0 if disabled.
 */
uint32_t CRM_GetI2s1Freq(void){
    uint32_t freq = 0;
    if (!HAL_CRM_I2s1ClkIsEnabled()){
        return 0;
    }
    clock_src_name_t src = 0;
    HAL_CRM_GetI2s1ClkConfig(&src );    
    freq = CRM_GetSrcFreq(src);

    return freq;
}

/**
 * @brief Configures the RGB clock divider.
 *
 * Sets the division factor for RGB display controller clock generation. The maximum allowed value is 15.
 *
 * @param div_m Division factor (must be <= 15)
 * @return CSK_DRIVER_OK on success, CSK_DRIVER_ERROR_PARAMETER on invalid input
 */
int32_t HAL_CRM_SetRgbClkDiv(
    uint32_t div_m){
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG6.bit.DIV_RGB_CLK_LD = 0x0;
    if (div_m > 15){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG6.bit.DIV_RGB_CLK_M = div_m;
    
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG6.bit.DIV_RGB_CLK_LD = 0x1;
    return CSK_DRIVER_OK;
}

/**
 * @brief Selects the clock source for RGB display controller.
 *
 * Choose between two available clock sources for RGB operations.
 *
 * @param src Clock source selection:
 *           @arg CRM_IpSrcCORE24M - Use CORE24M as source
 *           @arg CRM_IpSrcSyspllPeri - Use SyspllPeri as source
 * @return CSK_DRIVER_OK on success
 */
int32_t HAL_CRM_SetRgbClkSrc(clock_src_name_t src){
    if (src == CRM_IpSrcCORE24M){
        IP_CMN_SYSCFG->REG_PERI_CLK_CFG6.bit.SEL_RGB_CLK = 0;
    }if (src == CRM_IpSrcSyspllPeri){
        IP_CMN_SYSCFG->REG_PERI_CLK_CFG6.bit.SEL_RGB_CLK = 1;
    }
    return CSK_DRIVER_OK;
}

// Retrieves the RGB clock frequency.
/**
 * @brief Retrieves the RGB clock frequency.
 *
 * Returns 0 if RGB clock is disabled, otherwise calculates frequency based on selected source and divider.
 *
 * @return The RGB clock frequency in Hz. Returns 0 if disabled.
 */
uint32_t CRM_GetRgbFreq(void){
    uint32_t freq = 0;
    uint32_t div_m = 1;
    if (!HAL_CRM_RgbClkIsEnabled()){
        return 0;
    }
    clock_src_name_t src = 0;
    HAL_CRM_GetRgbClkConfig(&src, &div_m);    
    freq = CRM_GetSrcFreq(src);

    return freq/div_m;
}

/**
 * @brief Selects the clock source for VIC video interface.
 *
 * Choose between two available clock sources for VIC operations.
 *
 * @param src Clock source selection:
 *           @arg CRM_IpSrcCORE24M - Use CORE24M as source
 *           @arg CRM_IpSrcSyspllAud - Use SyspllAud as source
 * @return CSK_DRIVER_OK on success
 */
int32_t HAL_CRM_SetVicClkSrc(clock_src_name_t src){
    if (src == CRM_IpSrcCORE24M){
        IP_CMN_SYSCFG->REG_PERI_CLK_CFG6.bit.SEL_VIC_CLK = 0;
    }if (src == CRM_IpSrcSyspllAud){
        IP_CMN_SYSCFG->REG_PERI_CLK_CFG6.bit.SEL_VIC_CLK = 1;
    }
    return CSK_DRIVER_OK;
}

// Retrieves the VIC clock frequency.
/**
 * @brief Retrieves the VIC clock frequency.
 *
 * Returns 0 if VIC clock is disabled, otherwise returns the raw source frequency without division.
 *
 * @return The VIC clock frequency in Hz. Returns 0 if disabled.
 */
uint32_t CRM_GetVicFreq(void){
    uint32_t freq = 0;
    if (!HAL_CRM_VicClkIsEnabled()){
        return 0;
    }
    clock_src_name_t src = 0;
    HAL_CRM_GetVicClkConfig(&src );    
    freq = CRM_GetSrcFreq(src);

    return freq;
}

/**
 * @brief Configures the QSPI1 clock divider.
 *
 * Sets both N and M division factors for QSPI1 flash interface. Max values: N=7, M=15.
 *
 * @param div_n Prescaler numerator (must be <= 7)
 * @param div_m Prescaler denominator (must be <= 15)
 * @return CSK_DRIVER_OK on success, CSK_DRIVER_ERROR_PARAMETER on invalid input
 */
int32_t HAL_CRM_SetQspi1ClkDiv(
    uint32_t div_n, uint32_t div_m){
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG6.bit.DIV_QSPI1_CLK_LD = 0x0;
    if (div_n > 7){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG6.bit.DIV_QSPI1_CLK_N = div_n;
    
    if (div_m > 15){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG6.bit.DIV_QSPI1_CLK_M = div_m;
    
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG6.bit.DIV_QSPI1_CLK_LD = 0x1;
    return CSK_DRIVER_OK;
}

/**
 * @brief Selects the clock source for QSPI1 flash interface.
 *
 * Choose between two available clock sources for QSPI1 operations.
 *
 * @param src Clock source selection:
 *           @arg CRM_IpSrcCORE24M - Use CORE24M as source
 *           @arg CRM_IpSrcSyspllPeri - Use SyspllPeri as source
 * @return CSK_DRIVER_OK on success
 */
int32_t HAL_CRM_SetQspi1ClkSrc(clock_src_name_t src){
    if (src == CRM_IpSrcCORE24M){
        IP_CMN_SYSCFG->REG_PERI_CLK_CFG6.bit.SEL_QSPI1_CLK = 0;
    }if (src == CRM_IpSrcSyspllPeri){
        IP_CMN_SYSCFG->REG_PERI_CLK_CFG6.bit.SEL_QSPI1_CLK = 1;
    }
    return CSK_DRIVER_OK;
}

// Retrieves the QSPI1 clock frequency.
/**
 * @brief Retrieves the QSPI1 clock frequency.
 *
 * Returns 0 if QSPI1 clock is disabled, otherwise calculates frequency based on selected source and dividers.
 * Formula: SourceFrequency * (div_n / div_m)
 *
 * @return The QSPI1 clock frequency in Hz. Returns 0 if disabled.
 */
uint32_t CRM_GetQspi1Freq(void){
    uint32_t freq = 0;
    uint32_t div_n = 1;
    uint32_t div_m = 1;
    if (!HAL_CRM_Qspi1ClkIsEnabled()){
        return 0;
    }
    clock_src_name_t src = 0;
    HAL_CRM_GetQspi1ClkConfig(&src,&div_n, &div_m);    
    freq = CRM_GetSrcFreq(src);

    return freq*div_n/div_m;
}

/**
 * @brief Configures the QSPI0 clock divider.
 *
 * Sets both N and M division factors for QSPI0 flash interface. Max values: N=7, M=15.
 *
 * @param div_n Prescaler numerator (must be <= 7)
 * @param div_m Prescaler denominator (must be <= 15)
 * @return CSK_DRIVER_OK on success, CSK_DRIVER_ERROR_PARAMETER on invalid input
 */
int32_t HAL_CRM_SetQspi0ClkDiv(
    uint32_t div_n, uint32_t div_m){
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG6.bit.DIV_QSPI0_CLK_LD = 0x0;
    if (div_n > 7){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG6.bit.DIV_QSPI0_CLK_N = div_n;
    
    if (div_m > 15){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG6.bit.DIV_QSPI0_CLK_M = div_m;
    
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG6.bit.DIV_QSPI0_CLK_LD = 0x1;
    return CSK_DRIVER_OK;
}

/**
 * @brief Selects the clock source for QSPI0 flash interface.
 *
 * Choose between two available clock sources for QSPI0 operations.
 *
 * @param src Clock source selection:
 *           @arg CRM_IpSrcCORE24M - Use CORE24M as source
 *           @arg CRM_IpSrcSyspllPeri - Use SyspllPeri as source
 * @return CSK_DRIVER_OK on success
 */
int32_t HAL_CRM_SetQspi0ClkSrc(clock_src_name_t src){
    if (src == CRM_IpSrcCORE24M){
        IP_CMN_SYSCFG->REG_PERI_CLK_CFG6.bit.SEL_QSPI0_CLK = 0;
    }if (src == CRM_IpSrcSyspllPeri){
        IP_CMN_SYSCFG->REG_PERI_CLK_CFG6.bit.SEL_QSPI0_CLK = 1;
    }
    return CSK_DRIVER_OK;
}

// Retrieves the QSPI0 clock frequency.
/**
 * @brief Retrieves the QSPI0 clock frequency.
 *
 * Returns 0 if QSPI0 clock is disabled, otherwise calculates frequency based on selected source and dividers.
 * Formula: SourceFrequency * (div_n / div_m)
 *
 * @return The QSPI0 clock frequency in Hz. Returns 0 if disabled.
 */
uint32_t CRM_GetQspi0Freq(void){
    uint32_t freq = 0;
    uint32_t div_n = 1;
    uint32_t div_m = 1;
    if (!HAL_CRM_Qspi0ClkIsEnabled()){
        return 0;
    }
    clock_src_name_t src = 0;
    HAL_CRM_GetQspi0ClkConfig(&src,&div_n, &div_m);    
    freq = CRM_GetSrcFreq(src);

    return freq*div_n/div_m;
}

/**
 * @brief Selects the clock source for DAC digital-to-analog converter.
 *
 * Choose between two available clock sources for DAC operations.
 *
 * @param src Clock source selection:
 *           @arg CRM_IpSrcCORE24M - Use CORE24M as source
 *           @arg CRM_IpSrcSyspllAud - Use SyspllAud as source
 * @return CSK_DRIVER_OK on success
 */
int32_t HAL_CRM_SetDacClkSrc(clock_src_name_t src){
    if (src == CRM_IpSrcCORE24M){
        IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.AUD_DAC_MCLK_SRC = 0;
    }if (src == CRM_IpSrcSyspllAud){
        IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.AUD_DAC_MCLK_SRC = 1;
    }
    return CSK_DRIVER_OK;
}

// Retrieves the DAC clock frequency.
/**
 * @brief Retrieves the DAC clock frequency.
 *
 * Returns 0 if DAC clock is disabled, otherwise returns the raw source frequency without division.
 *
 * @return The DAC master clock frequency in Hz. Returns 0 if disabled.
 */
uint32_t CRM_GetDacFreq(void){
    uint32_t freq = 0;
    
    clock_src_name_t src = 0;
    HAL_CRM_GetDacClkConfig(&src );    
    freq = CRM_GetSrcFreq(src);

    return freq;
}


/**
 * @brief      Configures the ADC clock source selection
 *
 * This function sets the selected clock source for the ADC peripheral.
 * Two possible clock sources are supported: CORE24M or SyspllAud.
 * The configuration directly manipulates the hardware register bitfield.
 *
 * @param[in] src   Specifies the desired clock source for ADC
 *                  - @arg 0: Use CORE24M as ADC clock source
 *                  - @arg 1: Use SyspllAud as ADC clock source
 *
 * @retval     CSK HAL status code (always returns CSK_DRIVER_OK on success)
 *
 * @details    Directly writes to AUD_ADC_CLK_SRC bit in REG_PERI_CLK_CFG7 register
 *              based on the input parameter value. No error checking performed
 *              on input values - caller must ensure valid parameters.
 */
int32_t HAL_CRM_SetAdcClkSrc(clock_src_name_t src){
    if (src == CRM_IpSrcCORE24M){
        IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.AUD_ADC_CLK_SRC = 0;
    }if (src == CRM_IpSrcSyspllAud){
        IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.AUD_ADC_CLK_SRC = 1;
    }
    return CSK_DRIVER_OK;
}

/**
 * @brief      Retrieves the current ADC clock frequency
 *
 * This function calculates and returns the active ADC clock frequency.
 * Returns zero if the ADC peripheral is currently disabled.
 *
 * @return     uint32_t   Calculated ADC clock frequency in Hz,
 *                       0 if ADC is disabled
 *
 * @details    Performs these steps:
 *             1. Retrieves current ADC clock configuration via HAL_CRM_GetAdcClkConfig()
 *             2. Obtains base frequency of selected clock source using CRM_GetSrcFreq()
 *             3. Returns calculated frequency (actual division factors not applied here)
 *               Note: Final ADC clock may involve additional unspecified dividers
 */
uint32_t CRM_GetAdcFreq(void){
    uint32_t freq = 0;
    
    clock_src_name_t src = 0;
    HAL_CRM_GetAdcClkConfig(&src );    
    freq = CRM_GetSrcFreq(src);

    return freq;
}

/**
 * @brief      Retrieves the current APC clock frequency
 *
 * This function calculates and returns the active APC clock frequency.
 * Returns zero if the APC peripheral is currently disabled.
 *
 * @return     uint32_t   Calculated APC clock frequency in Hz,
 *                       0 if APC is disabled
 *
 * @details    Uses common peripheral clock (cmn_pclk) as base frequency source
 *              when APC is enabled. Verifies APC clock enable status before calculation.
 */
uint32_t CRM_GetApcFreq(void){
    uint32_t freq = 0;
    if (!HAL_CRM_ApcClkIsEnabled()){
        return 0;
    }
    
    freq = CRM_GetCmn_pclkFreq();

    return freq;
}

/**
 * @brief      Retrieves the current CODEC clock frequency
 *
 * This function calculates and returns the active CODEC clock frequency.
 * Returns zero if the CODEC peripheral is currently disabled.
 *
 * @return     uint32_t   Calculated CODEC clock frequency in Hz,
 *                       0 if CODEC is disabled
 *
 * @details    Uses common peripheral clock (cmn_pclk) as base frequency source
 *              when CODEC is enabled. Verifies CODEC clock enable status before calculation.
 */
uint32_t CRM_GetCodecFreq(void){
    uint32_t freq = 0;
    if (!HAL_CRM_CodecClkIsEnabled()){
        return 0;
    }
    
    freq = CRM_GetCmn_pclkFreq();

    return freq;
}

/**
 * @brief      Retrieves the current JPEG clock frequency
 *
 * This function calculates and returns the active JPEG clock frequency.
 * Returns zero if the JPEG peripheral is currently disabled.
 *
 * @return     uint32_t   Calculated JPEG clock frequency in Hz,
 *                       0 if JPEG is disabled
 *
 * @details    Uses high-speed clock (hclk) as base frequency source
 *              when JPEG is enabled. Verifies JPEG clock enable status before calculation.
 */
uint32_t CRM_GetJpegFreq(void){
    uint32_t freq = 0;
    if (!HAL_CRM_JpegClkIsEnabled()){
        return 0;
    }
    
    freq = CRM_GetHclkFreq();

    return freq;
}

/**
 * @brief      Retrieves the current LUNA clock frequency
 *
 * This function calculates and returns the active LUNA clock frequency.
 * Returns zero if the LUNA peripheral is currently disabled.
 *
 * @return     uint32_t   Calculated LUNA clock frequency in Hz,
 *                       0 if LUNA is disabled
 *
 * @details    Uses high-speed clock (hclk) as base frequency source
 *              when LUNA is enabled. Verifies LUNA clock enable status before calculation.
 */
uint32_t CRM_GetLunaFreq(void){
    uint32_t freq = 0;
    if (!HAL_CRM_LunaClkIsEnabled()){
        return 0;
    }
    
    freq = CRM_GetHclkFreq();

    return freq;
}

/**
 * @brief      Retrieves the current GPIO1 clock frequency
 *
 * This function calculates and returns the active GPIO1 clock frequency.
 * Returns zero if the GPIO1 peripheral is currently disabled.
 *
 * @return     uint32_t   Calculated GPIO1 clock frequency in Hz,
 *                       0 if GPIO1 is disabled
 *
 * @details    Uses common peripheral clock (cmn_pclk) as base frequency source
 *              when GPIO1 is enabled. Verifies GPIO1 clock enable status before calculation.
 */
uint32_t CRM_GetGpio1Freq(void){
    uint32_t freq = 0;
    if (!HAL_CRM_Gpio1ClkIsEnabled()){
        return 0;
    }
    
    freq = CRM_GetCmn_pclkFreq();

    return freq;
}

/**
 * @brief      Retrieves the current GPIO0 clock frequency
 *
 * This function calculates and returns the active GPIO0 clock frequency.
 * Returns zero if the GPIO0 peripheral is currently disabled.
 *
 * @return     uint32_t   Calculated GPIO0 clock frequency in Hz,
 *                       0 if GPIO0 is disabled
 *
 * @details    Uses common peripheral clock (cmn_pclk) as base frequency source
 *              when GPIO0 is enabled. Verifies GPIO0 clock enable status before calculation.
 */
uint32_t CRM_GetGpio0Freq(void){
    uint32_t freq = 0;
    if (!HAL_CRM_Gpio0ClkIsEnabled()){
        return 0;
    }
    
    freq = CRM_GetCmn_pclkFreq();

    return freq;
}

/**
 * @brief      Retrieves the current GPDMA clock frequency
 *
 * This function calculates and returns the active GPDMA clock frequency.
 * Returns zero if the GPDMA peripheral is currently disabled.
 *
 * @return     uint32_t   Calculated GPDMA clock frequency in Hz,
 *                       0 if GPDMA is disabled
 *
 * @details    Uses high-speed clock (hclk) as base frequency source
 *              when GPDMA is enabled. Verifies GPDMA clock enable status before calculation.
 */
uint32_t CRM_GetGpdmaFreq(void){
    uint32_t freq = 0;
    if (!HAL_CRM_GpdmaClkIsEnabled()){
        return 0;
    }
    
    freq = CRM_GetHclkFreq();

    return freq;
}

/**
 * @brief      Retrieves the current CMNDMA clock frequency
 *
 * This function calculates and returns the active CMNDMA clock frequency.
 * Returns zero if the CMNDMA peripheral is currently disabled.
 *
 * @return     uint32_t   Calculated CMNDMA clock frequency in Hz,
 *                       0 if CMNDMA is disabled
 *
 * @details    Uses high-speed clock (hclk) as base frequency source
 *              when CMNDMA is enabled. Verifies CMNDMA clock enable status before calculation.
 */
uint32_t CRM_GetCmndmaFreq(void){
    uint32_t freq = 0;
    if (!HAL_CRM_CmndmaClkIsEnabled()){
        return 0;
    }
    
    freq = CRM_GetHclkFreq();

    return freq;
}

/**
 * @brief      Retrieves the current USB clock frequency
 *
 * This function calculates and returns the active USB clock frequency.
 * Returns zero if the USB peripheral is currently disabled.
 *
 * @return     uint32_t   Calculated USB clock frequency in Hz,
 *                       0 if USB is disabled
 *
 * @details    Uses high-speed clock (hclk) as base frequency source
 *              when USB is enabled. Verifies USB clock enable status before calculation.
 */
uint32_t CRM_GetUsbFreq(void){
    uint32_t freq = 0;
    if (!HAL_CRM_UsbClkIsEnabled()){
        return 0;
    }
    
    freq = CRM_GetHclkFreq();

    return freq;
}

/**
 * @brief      Retrieves the current I2C1 clock frequency
 *
 * This function calculates and returns the active I2C1 clock frequency.
 * Returns zero if the I2C1 peripheral is currently disabled.
 *
 * @return     uint32_t   Calculated I2C1 clock frequency in Hz,
 *                       0 if I2C1 is disabled
 *
 * @details    Uses common peripheral clock (cmn_pclk) as base frequency source
 *              when I2C1 is enabled. Verifies I2C1 clock enable status before calculation.
 */
uint32_t CRM_GetI2c1Freq(void){
    uint32_t freq = 0;
    if (!HAL_CRM_I2c1ClkIsEnabled()){
        return 0;
    }
    
    freq = CRM_GetCmn_pclkFreq();

    return freq;
}

/**
 * @brief      Retrieves the current I2C0 clock frequency
 *
 * This function calculates and returns the active I2C0 clock frequency.
 * Returns zero if the I2C0 peripheral is currently disabled.
 *
 * @return     uint32_t   Calculated I2C0 clock frequency in Hz,
 *                       0 if I2C0 is disabled
 *
 * @details    Uses common peripheral clock (cmn_pclk) as base frequency source
 *              when I2C0 is enabled. Verifies I2C0 clock enable status before calculation.
 */
uint32_t CRM_GetI2c0Freq(void){
    uint32_t freq = 0;
    if (!HAL_CRM_I2c0ClkIsEnabled()){
        return 0;
    }
    
    freq = CRM_GetCmn_pclkFreq();

    return freq;
}

#endif /* INCLUDE_CLOCKMANAGER_C_ */

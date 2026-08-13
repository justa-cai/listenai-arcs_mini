
/**
  ******************************************************************************
  * @file    ClockManager.h
  * @author  ListenAI Application Team
  * @brief   Header file of CRM HAL module
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 ListenAI.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ListenAI under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef CLOCKMANAGER_H_
#define CLOCKMANAGER_H_

#include "Driver_Common.h"
#include "venusa_ap.h"
#include "clock_config.h"

/**
 * @brief Enumerates the possible clock source names for the Clock Resource Manager (CRM).
 *        Each value represents a specific clock source used in the system.
 */
typedef enum _clock_src_name {
    CRM_IpSrcInvalide                       = 0x0U,/**< Invalid clock source. Used to indicate an uninitialized or invalid state. */
    CRM_IpSrcRC032K                         = 1U,/**< RC032K clock source. Represents the clock source for RC032K. */

    CRM_IpSrcXTAL32K                        = 2U,/**< XTAL32K clock source. Represents the clock source for XTAL32K. */

    CRM_IpSrcRC024M                         = 3U,/**< RC024M clock source. Represents the clock source for RC024M. */

    CRM_IpSrcXTAL24M                        = 4U,/**< XTAL24M clock source. Represents the clock source for XTAL24M. */

    CRM_IpSrcCORE32K                        = 5U,/**< CORE32K clock source. Represents the clock source for CORE32K. */

    CRM_IpSrcCORE24M                        = 6U,/**< CORE24M clock source. Represents the clock source for CORE24M. */

    CRM_IpSrcSyspllCore                     = 7U,/**< SyspllCore clock source. Represents the clock source for SyspllCore. */

    CRM_IpSrcSyspllPeri                     = 8U,/**< SyspllPeri clock source. Represents the clock source for SyspllPeri. */

    CRM_IpSrcSyspllFlash                    = 9U,/**< SyspllFlash clock source. Represents the clock source for SyspllFlash. */

    CRM_IpSrcSyspllPsram                    = 10U,/**< SyspllPsram clock source. Represents the clock source for SyspllPsram. */

    CRM_IpSrcSyspllSdio                     = 11U,/**< SyspllSdio clock source. Represents the clock source for SyspllSdio. */

    CRM_IpSrcSyspllAud                      = 12U,/**< SyspllAud clock source. Represents the clock source for SyspllAud. */

    CRM_IpSrcUsbUtmi                        = 13U,/**< UsbUtmi clock source. Represents the clock source for UsbUtmi. */

    CRM_IpSrcVicPixel                       = 14U,/**< VicPixel clock source. Represents the clock source for VicPixel. */

} clock_src_name_t;

/**
 * @brief Enumerates the divider options for the SYSPLL CORE clock.
 *        Each value represents a specific divider setting for the CORE clock.
 */
typedef enum _clock_src_syspll_core_div {
	CRM_IpSyspllCore_Div3_400MHz   = 0x0,  /**< SYSPLL divider /3   */

	CRM_IpSyspllCore_Div4_300MHz   = 0x1,  /**< SYSPLL divider /4   */

	CRM_IpSyspllCore_Div5_240MHz   = 0x2,  /**< SYSPLL divider /5   */

	CRM_IpSyspllCore_Div6_200MHz   = 0x3,  /**< SYSPLL divider /6   */

	CRM_IpSyspllCore_Div7_171MHz   = 0x4,  /**< SYSPLL divider /7   */

	CRM_IpSyspllCore_Div8_150MHz   = 0x5,  /**< SYSPLL divider /8   */

	CRM_IpSyspllCore_Div9_133MHz   = 0x6,  /**< SYSPLL divider /9   */

	CRM_IpSyspllCore_Div10_120MHz  = 0x7,  /**< SYSPLL divider /10  */

	CRM_IpSyspllCore_Div3_5_343MHz = 0x8,  /**< SYSPLL divider /3.5 */

	CRM_IpSyspllCore_Div4_5_266MHz = 0x9   /**< SYSPLL divider /4.5 */

} clock_src_syspllcore_div;
/**
 * @brief Enumerates the divider options for the SYSPLL PERI clock.
 *        Each value represents a specific divider setting for the PERI clock.
 */
typedef enum _clock_src_syspll_peri_div {

    CRM_IpSyspllPeri_Div6_200MHz = 0,  /**< SYSPLL PERI divider 6. */

    CRM_IpSyspllPeri_Div7_171MHz = 1,  /**< SYSPLL PERI divider 7. */

    CRM_IpSyspllPeri_Div8_150MHz = 2,  /**< SYSPLL PERI divider 8. */

    CRM_IpSyspllPeri_Div9_133MHz = 3,  /**< SYSPLL PERI divider 9. */

    CRM_IpSyspllPeri_Div10_120MHz = 4,  /**< SYSPLL PERI divider 10. */

    CRM_IpSyspllPeri_Div11_109MHz = 5,  /**< SYSPLL PERI divider 11. */

    CRM_IpSyspllPeri_Div12_100MHz = 6,  /**< SYSPLL PERI divider 12. */

    CRM_IpSyspllPeri_Div13_92MHz = 7,  /**< SYSPLL PERI divider 13. */

    CRM_IpSyspllPeri_Div14_86MHz = 8,  /**< SYSPLL PERI divider 14. */

    CRM_IpSyspllPeri_Div15_80MHz = 9,  /**< SYSPLL PERI divider 15. */

    CRM_IpSyspllPeri_Div16_75MHz = 10,  /**< SYSPLL PERI divider 16. */

} clock_src_syspllperi_div;
/**
 * @brief Enumerates the divider options for the SYSPLL FLASH clock.
 *        Each value represents a specific divider setting for the FLASH clock.
 */
typedef enum _clock_src_syspll_flash_div {

	CRM_IpSyspllFlash_Div9_133MHz   = 0x0,  /**< SYSPLL FLASH divider 9. */

	CRM_IpSyspllFlash_Div10_120MHz  = 0x1,  /**< SYSPLL FLASH divider 10. */

	CRM_IpSyspllFlash_Div11_109MHz  = 0x2,  /**< SYSPLL FLASH divider 11. */

	CRM_IpSyspllFlash_Div12_100MHz  = 0x3,  /**< SYSPLL FLASH divider 12. */

	CRM_IpSyspllFlash_Div13_92MHz   = 0x4,  /**< SYSPLL FLASH divider 13. */

	CRM_IpSyspllFlash_Div14_86MHz   = 0x5,  /**< SYSPLL FLASH divider 14. */

	CRM_IpSyspllFlash_Div15_80MHz   = 0x6,  /**< SYSPLL FLASH divider 15. */

	CRM_IpSyspllFlash_Div16_75MHz   = 0x7   /**< SYSPLL FLASH divider 16. */

} clock_src_syspllflash_div;
/**
 * @brief Enumerates the divider options for the SYSPLL PSRAM clock.
 *        Each value represents a specific divider setting for the PSRAM clock.
 */
typedef enum _clock_src_syspll_psram_div {

    CRM_IpSyspllPsram_Div3_400MHz = 0,  /**< SYSPLL PSRAM divider 3. */

    CRM_IpSyspllPsram_Div4_300MHz = 1,  /**< SYSPLL PSRAM divider 4. */

    CRM_IpSyspllPsram_Div5_240MHz = 2,  /**< SYSPLL PSRAM divider 5. */

    CRM_IpSyspllPsram_Div6_200MHz = 3,  /**< SYSPLL PSRAM divider 6. */

    CRM_IpSyspllPsram_Div7_171MHz = 4,  /**< SYSPLL PSRAM divider 7. */

    CRM_IpSyspllPsram_Div8_150MHz = 5,  /**< SYSPLL PSRAM divider 8. */

    CRM_IpSyspllPsram_Div9_133MHz = 6,  /**< SYSPLL PSRAM divider 9. */

    CRM_IpSyspllPsram_Div10_120MHz = 7,  /**< SYSPLL PSRAM divider 10. */

} clock_src_syspllpsram_div;
/**
 * @brief Enumerates the divider options for the SYSPLL SDIO clock.
 *        Each value represents a specific divider setting for the SDIO clock.
 */
typedef enum _clock_src_syspll_sdio_div {

    CRM_IpSyspllSdio_Div3_400MHz = 0,  /**< SYSPLL SDIO divider 3. */

    CRM_IpSyspllSdio_Div4_300MHz = 1,  /**< SYSPLL SDIO divider 4. */

    CRM_IpSyspllSdio_Div5_240MHz = 2,  /**< SYSPLL SDIO divider 5. */

    CRM_IpSyspllSdio_Div6_200MHz = 3,  /**< SYSPLL SDIO divider 6. */

    CRM_IpSyspllSdio_Div7_171MHz = 4,  /**< SYSPLL SDIO divider 7. */

    CRM_IpSyspllSdio_Div8_150MHz = 5,  /**< SYSPLL SDIO divider 8. */

} clock_src_syspllsdio_div;

/**
 * @brief Initializes the SYSPLL for the CORE clock with the specified divider.
 * @param div Divider value for the CORE (from clock_src_syspllcore_div).
 * @return Status code (0 for success, negative for error).
 */
int32_t CRM_SysPll_InitCore(clock_src_syspllcore_div div);

/**
 * @brief Initializes the SYSPLL for the PERI clock with the specified divider.
 * @param div Divider value for the PERI (from clock_src_syspllperi_div).
 * @return Status code (0 for success, negative for error).
 */
int32_t CRM_SysPll_InitPeri(clock_src_syspllperi_div div);

/**
 * @brief Initializes the SYSPLL for the FLASH clock with the specified divider.
 * @param div Divider value for the FLASH (from clock_src_syspllflash_div).
 * @return Status code (0 for success, negative for error).
 */
int32_t CRM_SysPll_InitFlash(clock_src_syspllflash_div div);

/**
 * @brief Initializes the SYSPLL for the PSRAM clock with the specified divider.
 * @param div Divider value for the PSRAM (from clock_src_syspllpsram_div).
 * @return Status code (0 for success, negative for error).
 */
int32_t CRM_SysPll_InitPsram(clock_src_syspllpsram_div div);

/**
 * @brief Initializes the SYSPLL for the SDIO clock with the specified divider.
 * @param div Divider value for the SDIO (from clock_src_syspllsdio_div).
 * @return Status code (0 for success, negative for error).
 */
int32_t CRM_SysPll_InitSdio(clock_src_syspllsdio_div div);

/**
 * @brief Enables the PSRAM clock by setting the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_PSRAM_CLK_ENABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG0.bit.ENA_PSRAM_CLK = 0x1; \
} while(0)

/**
 * @brief Disables the PSRAM clock by clearing the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_PSRAM_CLK_DISABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG0.bit.ENA_PSRAM_CLK = 0x0; \
} while(0)

/**
 * @brief Checks if the PSRAM clock is enabled.
 * @return 1 if enabled, 0 if disabled.
 */
static inline uint8_t HAL_CRM_PsramClkIsEnabled() {
    return IP_CMN_SYSCFG->REG_PERI_CLK_CFG0.bit.ENA_PSRAM_CLK;
}

/**
 * @brief Sets the divider for the PSRAM clock.

 * @param div_m Divider value (M) for the PSRAM clock.
 * @return Status code (0 for success, negative for error).
 */

int32_t HAL_CRM_SetPsramClkDiv(
    uint32_t div_m);

/**
  * @brief  Function to configure the PSRAM clock source
  * @param  src specifies which source that PSRAM can choose
  *         This parameter can be one of the following values:
  *         @arg 0: PSRAM using CORE24M to source clock
  *         @arg 1: PSRAM using SyspllPsram to source clock
  * @return CSK HAL status
  */
int32_t HAL_CRM_SetPsramClkSrc(clock_src_name_t src);

/**
 * @brief Retrieves the current PSRAM clock configuration.
 * @param src Pointer to store the clock source (from clock_src_name_t).
 * @param div_m Pointer to store the divider value (M).
 */
static inline void HAL_CRM_GetPsramClkConfig(clock_src_name_t *src, uint32_t *div_m) {
    uint32_t src_t = IP_CMN_SYSCFG->REG_PERI_CLK_CFG0.bit.SEL_PSRAM_CLK;
    if (src_t == 0) {
        *src = CRM_IpSrcCORE24M;
    } else {
        *src = CRM_IpSrcSyspllPsram;
    }
    *div_m = IP_CMN_SYSCFG->REG_PERI_CLK_CFG0.bit.DIV_PSRAM_CLK_M;
}

/**
 * @brief Gets the current frequency of the PSRAM clock.
 * @return Frequency in Hz.
 */
uint32_t CRM_GetPsramFreq(void);

/**
 * @brief Enables the MTIME clock by setting the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_MTIME_CLK_ENABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG0.bit.ENA_MTIME_TOGGLE = 0x1; \
} while(0)

/**
 * @brief Disables the MTIME clock by clearing the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_MTIME_CLK_DISABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG0.bit.ENA_MTIME_TOGGLE = 0x0; \
} while(0)

/**
 * @brief Checks if the MTIME clock is enabled.
 * @return 1 if enabled, 0 if disabled.
 */
static inline uint8_t HAL_CRM_MtimeClkIsEnabled(){
    return IP_CMN_SYSCFG->REG_PERI_CLK_CFG0.bit.ENA_MTIME_TOGGLE;
}

/**
 * @brief Sets the divider for the MTIME clock.

 * @param div_m Divider value (M) for the MTIME clock.
 * @return Status code (0 for success, negative for error).
 */

int32_t HAL_CRM_SetMtimeClkDiv(
    uint32_t div_m);

/**
 * @brief Retrieves the current MTIME clock configuration.

 * @param div_m Pointer to store the divider value (M).
 */
static inline void HAL_CRM_GetMtimeClkConfig( uint32_t *div_m){
    
    *div_m = IP_CMN_SYSCFG->REG_PERI_CLK_CFG0.bit.DIV_MTIME_TOGGLE_M;
}

/**
 * @brief Gets the current frequency of the MTIME clock.
 * @return Frequency in Hz.
 */
uint32_t CRM_GetMtimeFreq(void);

/**
 * @brief Enables the FLASH clock by setting the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_FLASH_CLK_ENABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG0.bit.ENA_FLASHC_CLK = 0x1; \
} while(0)

/**
 * @brief Disables the FLASH clock by clearing the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_FLASH_CLK_DISABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG0.bit.ENA_FLASHC_CLK = 0x0; \
} while(0)

/**
 * @brief Checks if the FLASH clock is enabled.
 * @return 1 if enabled, 0 if disabled.
 */
static inline uint8_t HAL_CRM_FlashClkIsEnabled(){
    return IP_CMN_SYSCFG->REG_PERI_CLK_CFG0.bit.ENA_FLASHC_CLK;
}

/**
 * @brief Sets the divider for the FLASH clock.

 * @param div_m Divider value (M) for the FLASH clock.
 * @return Status code (0 for success, negative for error).
 */

int32_t HAL_CRM_SetFlashClkDiv(
    uint32_t div_m);

/**
  * @brief  Function to configure the FLASH clock source
  * @param  src specifies which source that FLASH can choose
  *         This parameter can be one of the following values:
  *         @arg 0: FLASH using CORE24M to source clock
  *         @arg 1: FLASH using SyspllFlash to source clock
  * @return CSK HAL status
  */
int32_t HAL_CRM_SetFlashClkSrc(clock_src_name_t src);

/**
 * @brief Retrieves the current FLASH clock configuration.
 * @param src Pointer to store the clock source (from clock_src_name_t).

 * @param div_m Pointer to store the divider value (M).
 */
static inline void HAL_CRM_GetFlashClkConfig(clock_src_name_t *src, uint32_t *div_m){
    uint32_t src_t;
    src_t = IP_CMN_SYSCFG->REG_PERI_CLK_CFG0.bit.SEL_FLASHC_CLK;
    if (src_t == 0){
        *src = CRM_IpSrcCORE24M;
    }if (src_t == 1){
        *src = CRM_IpSrcSyspllFlash;
    }
    
    *div_m = IP_CMN_SYSCFG->REG_PERI_CLK_CFG0.bit.DIV_FLASHC_CLK_M;
}

/**
 * @brief Gets the current frequency of the FLASH clock.
 * @return Frequency in Hz.
 */
uint32_t CRM_GetFlashFreq(void);

/**
 * @brief Enables the SPI0 clock by setting the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_SPI0_CLK_ENABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG1.bit.ENA_SPI0_CLK = 0x1; \
} while(0)

/**
 * @brief Disables the SPI0 clock by clearing the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_SPI0_CLK_DISABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG1.bit.ENA_SPI0_CLK = 0x0; \
} while(0)

/**
 * @brief Checks if the SPI0 clock is enabled.
 * @return 1 if enabled, 0 if disabled.
 */
static inline uint8_t HAL_CRM_Spi0ClkIsEnabled(){
    return IP_CMN_SYSCFG->REG_PERI_CLK_CFG1.bit.ENA_SPI0_CLK;
}

/**
 * @brief Sets the divider for the SPI0 clock.
 * @param div_n Divider value (N) for the SPI0 clock.
 * @param div_m Divider value (M) for the SPI0 clock.
 * @return Status code (0 for success, negative for error).
 */

int32_t HAL_CRM_SetSpi0ClkDiv(
    uint32_t div_n, uint32_t div_m);

/**
  * @brief  Function to configure the SPI0 clock source
  * @param  src specifies which source that SPI0 can choose
  *         This parameter can be one of the following values:
  *         @arg 0: SPI0 using CORE24M to source clock
  *         @arg 1: SPI0 using SyspllPeri to source clock
  * @return CSK HAL status
  */
int32_t HAL_CRM_SetSpi0ClkSrc(clock_src_name_t src);

/**
 * @brief Retrieves the current SPI0 clock configuration.
 * @param src Pointer to store the clock source (from clock_src_name_t).
 * @param div_n Pointer to store the divider value (N).
 * @param div_m Pointer to store the divider value (M).
 */
static inline void HAL_CRM_GetSpi0ClkConfig(clock_src_name_t *src,uint32_t *div_n, uint32_t *div_m){
    uint32_t src_t;
    src_t = IP_CMN_SYSCFG->REG_PERI_CLK_CFG1.bit.SEL_SPI0_CLK;
    if (src_t == 0){
        *src = CRM_IpSrcCORE24M;
    }if (src_t == 1){
        *src = CRM_IpSrcSyspllPeri;
    }
    *div_n = IP_CMN_SYSCFG->REG_PERI_CLK_CFG1.bit.DIV_SPI0_CLK_N;
    *div_m = IP_CMN_SYSCFG->REG_PERI_CLK_CFG1.bit.DIV_SPI0_CLK_M;
}

/**
 * @brief Gets the current frequency of the SPI0 clock.
 * @return Frequency in Hz.
 */
uint32_t CRM_GetSpi0Freq(void);

/**
 * @brief Enables the UART0 clock by setting the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_UART0_CLK_ENABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG1.bit.ENA_UART0_CLK = 0x1; \
} while(0)

/**
 * @brief Disables the UART0 clock by clearing the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_UART0_CLK_DISABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG1.bit.ENA_UART0_CLK = 0x0; \
} while(0)

/**
 * @brief Checks if the UART0 clock is enabled.
 * @return 1 if enabled, 0 if disabled.
 */
static inline uint8_t HAL_CRM_Uart0ClkIsEnabled(){
    return IP_CMN_SYSCFG->REG_PERI_CLK_CFG1.bit.ENA_UART0_CLK;
}

/**
 * @brief Sets the divider for the UART0 clock.
 * @param div_n Divider value (N) for the UART0 clock.
 * @param div_m Divider value (M) for the UART0 clock.
 * @return Status code (0 for success, negative for error).
 */

int32_t HAL_CRM_SetUart0ClkDiv(
    uint32_t div_n, uint32_t div_m);

/**
  * @brief  Function to configure the UART0 clock source
  * @param  src specifies which source that UART0 can choose
  *         This parameter can be one of the following values:
  *         @arg 0: UART0 using CORE24M to source clock
  *         @arg 1: UART0 using SyspllPeri to source clock
  * @return CSK HAL status
  */
int32_t HAL_CRM_SetUart0ClkSrc(clock_src_name_t src);

/**
 * @brief Retrieves the current UART0 clock configuration.
 * @param src Pointer to store the clock source (from clock_src_name_t).
 * @param div_n Pointer to store the divider value (N).
 * @param div_m Pointer to store the divider value (M).
 */
static inline void HAL_CRM_GetUart0ClkConfig(clock_src_name_t *src,uint32_t *div_n, uint32_t *div_m){
    uint32_t src_t;
    src_t = IP_CMN_SYSCFG->REG_PERI_CLK_CFG1.bit.SEL_UART0_CLK;
    if (src_t == 0){
        *src = CRM_IpSrcCORE24M;
    }if (src_t == 1){
        *src = CRM_IpSrcSyspllPeri;
    }
    *div_n = IP_CMN_SYSCFG->REG_PERI_CLK_CFG1.bit.DIV_UART0_CLK_N;
    *div_m = IP_CMN_SYSCFG->REG_PERI_CLK_CFG1.bit.DIV_UART0_CLK_M;
}

/**
 * @brief Gets the current frequency of the UART0 clock.
 * @return Frequency in Hz.
 */
uint32_t CRM_GetUart0Freq(void);

/**
 * @brief Enables the SPI1 clock by setting the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_SPI1_CLK_ENABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG2.bit.ENA_SPI1_CLK = 0x1; \
} while(0)

/**
 * @brief Disables the SPI1 clock by clearing the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_SPI1_CLK_DISABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG2.bit.ENA_SPI1_CLK = 0x0; \
} while(0)

/**
 * @brief Checks if the SPI1 clock is enabled.
 * @return 1 if enabled, 0 if disabled.
 */
static inline uint8_t HAL_CRM_Spi1ClkIsEnabled(){
    return IP_CMN_SYSCFG->REG_PERI_CLK_CFG2.bit.ENA_SPI1_CLK;
}

/**
 * @brief Sets the divider for the SPI1 clock.
 * @param div_n Divider value (N) for the SPI1 clock.
 * @param div_m Divider value (M) for the SPI1 clock.
 * @return Status code (0 for success, negative for error).
 */

int32_t HAL_CRM_SetSpi1ClkDiv(
    uint32_t div_n, uint32_t div_m);

/**
  * @brief  Function to configure the SPI1 clock source
  * @param  src specifies which source that SPI1 can choose
  *         This parameter can be one of the following values:
  *         @arg 0: SPI1 using CORE24M to source clock
  *         @arg 1: SPI1 using SyspllPeri to source clock
  * @return CSK HAL status
  */
int32_t HAL_CRM_SetSpi1ClkSrc(clock_src_name_t src);

/**
 * @brief Retrieves the current SPI1 clock configuration.
 * @param src Pointer to store the clock source (from clock_src_name_t).
 * @param div_n Pointer to store the divider value (N).
 * @param div_m Pointer to store the divider value (M).
 */
static inline void HAL_CRM_GetSpi1ClkConfig(clock_src_name_t *src,uint32_t *div_n, uint32_t *div_m){
    uint32_t src_t;
    src_t = IP_CMN_SYSCFG->REG_PERI_CLK_CFG2.bit.SEL_SPI1_CLK;
    if (src_t == 0){
        *src = CRM_IpSrcCORE24M;
    }if (src_t == 1){
        *src = CRM_IpSrcSyspllPeri;
    }
    *div_n = IP_CMN_SYSCFG->REG_PERI_CLK_CFG2.bit.DIV_SPI1_CLK_N;
    *div_m = IP_CMN_SYSCFG->REG_PERI_CLK_CFG2.bit.DIV_SPI1_CLK_M;
}

/**
 * @brief Gets the current frequency of the SPI1 clock.
 * @return Frequency in Hz.
 */
uint32_t CRM_GetSpi1Freq(void);

/**
 * @brief Enables the UART1 clock by setting the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_UART1_CLK_ENABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG2.bit.ENA_UART1_CLK = 0x1; \
} while(0)

/**
 * @brief Disables the UART1 clock by clearing the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_UART1_CLK_DISABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG2.bit.ENA_UART1_CLK = 0x0; \
} while(0)

/**
 * @brief Checks if the UART1 clock is enabled.
 * @return 1 if enabled, 0 if disabled.
 */
static inline uint8_t HAL_CRM_Uart1ClkIsEnabled(){
    return IP_CMN_SYSCFG->REG_PERI_CLK_CFG2.bit.ENA_UART1_CLK;
}

/**
 * @brief Sets the divider for the UART1 clock.
 * @param div_n Divider value (N) for the UART1 clock.
 * @param div_m Divider value (M) for the UART1 clock.
 * @return Status code (0 for success, negative for error).
 */

int32_t HAL_CRM_SetUart1ClkDiv(
    uint32_t div_n, uint32_t div_m);

/**
  * @brief  Function to configure the UART1 clock source
  * @param  src specifies which source that UART1 can choose
  *         This parameter can be one of the following values:
  *         @arg 0: UART1 using CORE24M to source clock
  *         @arg 1: UART1 using SyspllPeri to source clock
  * @return CSK HAL status
  */
int32_t HAL_CRM_SetUart1ClkSrc(clock_src_name_t src);

/**
 * @brief Retrieves the current UART1 clock configuration.
 * @param src Pointer to store the clock source (from clock_src_name_t).
 * @param div_n Pointer to store the divider value (N).
 * @param div_m Pointer to store the divider value (M).
 */
static inline void HAL_CRM_GetUart1ClkConfig(clock_src_name_t *src,uint32_t *div_n, uint32_t *div_m){
    uint32_t src_t;
    src_t = IP_CMN_SYSCFG->REG_PERI_CLK_CFG2.bit.SEL_UART1_CLK;
    if (src_t == 0){
        *src = CRM_IpSrcCORE24M;
    }if (src_t == 1){
        *src = CRM_IpSrcSyspllPeri;
    }
    *div_n = IP_CMN_SYSCFG->REG_PERI_CLK_CFG2.bit.DIV_UART1_CLK_N;
    *div_m = IP_CMN_SYSCFG->REG_PERI_CLK_CFG2.bit.DIV_UART1_CLK_M;
}

/**
 * @brief Gets the current frequency of the UART1 clock.
 * @return Frequency in Hz.
 */
uint32_t CRM_GetUart1Freq(void);

/**
 * @brief Enables the SDIOH clock by setting the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_SDIOH_CLK_ENABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG3.bit.ENA_SDIOH_CLK2X = 0x1; \
} while(0)

/**
 * @brief Disables the SDIOH clock by clearing the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_SDIOH_CLK_DISABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG3.bit.ENA_SDIOH_CLK2X = 0x0; \
} while(0)

/**
 * @brief Checks if the SDIOH clock is enabled.
 * @return 1 if enabled, 0 if disabled.
 */
static inline uint8_t HAL_CRM_SdiohClkIsEnabled(){
    return IP_CMN_SYSCFG->REG_PERI_CLK_CFG3.bit.ENA_SDIOH_CLK2X;
}

/**
 * @brief Sets the divider for the SDIOH clock.
 * @param div_n Divider value (N) for the SDIOH clock.
 * @param div_m Divider value (M) for the SDIOH clock.
 * @return Status code (0 for success, negative for error).
 */

int32_t HAL_CRM_SetSdiohClkDiv(
    uint32_t div_n, uint32_t div_m);

/**
  * @brief  Function to configure the SDIOH clock source
  * @param  src specifies which source that SDIOH can choose
  *         This parameter can be one of the following values:
  *         @arg 0: SDIOH using CORE24M to source clock
  *         @arg 1: SDIOH using SyspllSdio to source clock
  * @return CSK HAL status
  */
int32_t HAL_CRM_SetSdiohClkSrc(clock_src_name_t src);

/**
 * @brief Retrieves the current SDIOH clock configuration.
 * @param src Pointer to store the clock source (from clock_src_name_t).
 * @param div_n Pointer to store the divider value (N).
 * @param div_m Pointer to store the divider value (M).
 */
static inline void HAL_CRM_GetSdiohClkConfig(clock_src_name_t *src,uint32_t *div_n, uint32_t *div_m){
    uint32_t src_t;
    src_t = IP_CMN_SYSCFG->REG_PERI_CLK_CFG3.bit.SEL_SDIOH_CLK2X;
    if (src_t == 0){
        *src = CRM_IpSrcCORE24M;
    }if (src_t == 1){
        *src = CRM_IpSrcSyspllSdio;
    }
    *div_n = IP_CMN_SYSCFG->REG_PERI_CLK_CFG3.bit.DIV_SDIOH_CLK2X_N;
    *div_m = IP_CMN_SYSCFG->REG_PERI_CLK_CFG3.bit.DIV_SDIOH_CLK2X_M;
}

/**
 * @brief Gets the current frequency of the SDIOH clock.
 * @return Frequency in Hz.
 */
uint32_t CRM_GetSdiohFreq(void);

/**
 * @brief Enables the UART2 clock by setting the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_UART2_CLK_ENABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG3.bit.ENA_UART2_CLK = 0x1; \
} while(0)

/**
 * @brief Disables the UART2 clock by clearing the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_UART2_CLK_DISABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG3.bit.ENA_UART2_CLK = 0x0; \
} while(0)

/**
 * @brief Checks if the UART2 clock is enabled.
 * @return 1 if enabled, 0 if disabled.
 */
static inline uint8_t HAL_CRM_Uart2ClkIsEnabled(){
    return IP_CMN_SYSCFG->REG_PERI_CLK_CFG3.bit.ENA_UART2_CLK;
}

/**
 * @brief Sets the divider for the UART2 clock.
 * @param div_n Divider value (N) for the UART2 clock.
 * @param div_m Divider value (M) for the UART2 clock.
 * @return Status code (0 for success, negative for error).
 */

int32_t HAL_CRM_SetUart2ClkDiv(
    uint32_t div_n, uint32_t div_m);

/**
  * @brief  Function to configure the UART2 clock source
  * @param  src specifies which source that UART2 can choose
  *         This parameter can be one of the following values:
  *         @arg 0: UART2 using CORE24M to source clock
  *         @arg 1: UART2 using SyspllPeri to source clock
  * @return CSK HAL status
  */
int32_t HAL_CRM_SetUart2ClkSrc(clock_src_name_t src);

/**
 * @brief Retrieves the current UART2 clock configuration.
 * @param src Pointer to store the clock source (from clock_src_name_t).
 * @param div_n Pointer to store the divider value (N).
 * @param div_m Pointer to store the divider value (M).
 */
static inline void HAL_CRM_GetUart2ClkConfig(clock_src_name_t *src,uint32_t *div_n, uint32_t *div_m){
    uint32_t src_t;
    src_t = IP_CMN_SYSCFG->REG_PERI_CLK_CFG3.bit.SEL_UART2_CLK;
    if (src_t == 0){
        *src = CRM_IpSrcCORE24M;
    }if (src_t == 1){
        *src = CRM_IpSrcSyspllPeri;
    }
    *div_n = IP_CMN_SYSCFG->REG_PERI_CLK_CFG3.bit.DIV_UART2_CLK_N;
    *div_m = IP_CMN_SYSCFG->REG_PERI_CLK_CFG3.bit.DIV_UART2_CLK_M;
}

/**
 * @brief Gets the current frequency of the UART2 clock.
 * @return Frequency in Hz.
 */
uint32_t CRM_GetUart2Freq(void);

/**
 * @brief Enables the VIC_OUT clock by setting the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_VIC_OUT_CLK_ENABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG4.bit.ENA_VIC_OUT_CLK = 0x1; \
} while(0)

/**
 * @brief Disables the VIC_OUT clock by clearing the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_VIC_OUT_CLK_DISABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG4.bit.ENA_VIC_OUT_CLK = 0x0; \
} while(0)

/**
 * @brief Checks if the VIC_OUT clock is enabled.
 * @return 1 if enabled, 0 if disabled.
 */
static inline uint8_t HAL_CRM_Vic_outClkIsEnabled(){
    return IP_CMN_SYSCFG->REG_PERI_CLK_CFG4.bit.ENA_VIC_OUT_CLK;
}

/**
 * @brief Sets the divider for the VIC_OUT clock.

 * @param div_m Divider value (M) for the VIC_OUT clock.
 * @return Status code (0 for success, negative for error).
 */

int32_t HAL_CRM_SetVic_outClkDiv(
    uint32_t div_m);

/**
  * @brief  Function to configure the VIC_OUT clock source
  * @param  src specifies which source that VIC_OUT can choose
  *         This parameter can be one of the following values:
  *         @arg 0: VIC_OUT using CORE24M to source clock
  *         @arg 1: VIC_OUT using SyspllPeri to source clock
  * @return CSK HAL status
  */
int32_t HAL_CRM_SetVic_outClkSrc(clock_src_name_t src);

/**
 * @brief Retrieves the current VIC_OUT clock configuration.
 * @param src Pointer to store the clock source (from clock_src_name_t).

 * @param div_m Pointer to store the divider value (M).
 */
static inline void HAL_CRM_GetVic_outClkConfig(clock_src_name_t *src, uint32_t *div_m){
    uint32_t src_t;
    src_t = IP_CMN_SYSCFG->REG_PERI_CLK_CFG4.bit.SEL_VIC_OUT_CLK;
    if (src_t == 0){
        *src = CRM_IpSrcCORE24M;
    }if (src_t == 1){
        *src = CRM_IpSrcSyspllPeri;
    }
    
    *div_m = IP_CMN_SYSCFG->REG_PERI_CLK_CFG4.bit.DIV_VIC_OUT_CLK_M;
}

/**
 * @brief Gets the current frequency of the VIC_OUT clock.
 * @return Frequency in Hz.
 */
uint32_t CRM_GetVic_outFreq(void);

/**
 * @brief Enables the GPT clock by setting the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_GPT_CLK_ENABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG4.bit.ENA_GPT_CLK = 0x1; \
} while(0)

/**
 * @brief Disables the GPT clock by clearing the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_GPT_CLK_DISABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG4.bit.ENA_GPT_CLK = 0x0; \
} while(0)

/**
 * @brief Checks if the GPT clock is enabled.
 * @return 1 if enabled, 0 if disabled.
 */
static inline uint8_t HAL_CRM_GptClkIsEnabled(){
    return IP_CMN_SYSCFG->REG_PERI_CLK_CFG4.bit.ENA_GPT_CLK;
}

/**
 * @brief Sets the divider for the GPT clock.

 * @param div_m Divider value (M) for the GPT clock.
 * @return Status code (0 for success, negative for error).
 */

int32_t HAL_CRM_SetGptClkDiv(
    uint32_t div_m);

/**
 * @brief Retrieves the current GPT clock configuration.

 * @param div_m Pointer to store the divider value (M).
 */
static inline void HAL_CRM_GetGptClkConfig( uint32_t *div_m){
    
    *div_m = IP_CMN_SYSCFG->REG_PERI_CLK_CFG4.bit.DIV_GPT_T0_CLK_M;
}

/**
 * @brief Gets the current frequency of the GPT clock.
 * @return Frequency in Hz.
 */
uint32_t CRM_GetGptFreq(void);

/**
 * @brief Enables the I8080 clock by setting the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_I8080_CLK_ENABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG5.bit.ENA_I8080_CLK = 0x1; \
} while(0)

/**
 * @brief Disables the I8080 clock by clearing the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_I8080_CLK_DISABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG5.bit.ENA_I8080_CLK = 0x0; \
} while(0)

/**
 * @brief Checks if the I8080 clock is enabled.
 * @return 1 if enabled, 0 if disabled.
 */
static inline uint8_t HAL_CRM_I8080ClkIsEnabled(){
    return IP_CMN_SYSCFG->REG_PERI_CLK_CFG5.bit.ENA_I8080_CLK;
}

/**
 * @brief Sets the divider for the I8080 clock.

 * @param div_m Divider value (M) for the I8080 clock.
 * @return Status code (0 for success, negative for error).
 */

int32_t HAL_CRM_SetI8080ClkDiv(
    uint32_t div_m);

/**
  * @brief  Function to configure the I8080 clock source
  * @param  src specifies which source that I8080 can choose
  *         This parameter can be one of the following values:
  *         @arg 0: I8080 using CORE24M to source clock
  *         @arg 1: I8080 using SyspllPeri to source clock
  * @return CSK HAL status
  */
int32_t HAL_CRM_SetI8080ClkSrc(clock_src_name_t src);

/**
 * @brief Retrieves the current I8080 clock configuration.
 * @param src Pointer to store the clock source (from clock_src_name_t).

 * @param div_m Pointer to store the divider value (M).
 */
static inline void HAL_CRM_GetI8080ClkConfig(clock_src_name_t *src, uint32_t *div_m){
    uint32_t src_t;
    src_t = IP_CMN_SYSCFG->REG_PERI_CLK_CFG5.bit.SEL_I8080_CLK;
    if (src_t == 0){
        *src = CRM_IpSrcCORE24M;
    }if (src_t == 1){
        *src = CRM_IpSrcSyspllPeri;
    }
    
    *div_m = IP_CMN_SYSCFG->REG_PERI_CLK_CFG5.bit.DIV_I8080_CLK_M;
}

/**
 * @brief Gets the current frequency of the I8080 clock.
 * @return Frequency in Hz.
 */
uint32_t CRM_GetI8080Freq(void);

/**
 * @brief Enables the IR clock by setting the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_IR_CLK_ENABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG5.bit.ENA_IR_CLK = 0x1; \
} while(0)

/**
 * @brief Disables the IR clock by clearing the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_IR_CLK_DISABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG5.bit.ENA_IR_CLK = 0x0; \
} while(0)

/**
 * @brief Checks if the IR clock is enabled.
 * @return 1 if enabled, 0 if disabled.
 */
static inline uint8_t HAL_CRM_IrClkIsEnabled(){
    return IP_CMN_SYSCFG->REG_PERI_CLK_CFG5.bit.ENA_IR_CLK;
}

/**
 * @brief Sets the divider for the IR clock.

 * @param div_m Divider value (M) for the IR clock.
 * @return Status code (0 for success, negative for error).
 */

int32_t HAL_CRM_SetIrClkDiv(
    uint32_t div_m);

/**
 * @brief Retrieves the current IR clock configuration.

 * @param div_m Pointer to store the divider value (M).
 */
static inline void HAL_CRM_GetIrClkConfig( uint32_t *div_m){
    
    *div_m = IP_CMN_SYSCFG->REG_PERI_CLK_CFG5.bit.DIV_IR_CLK_TX_M;
}

/**
 * @brief Gets the current frequency of the IR clock.
 * @return Frequency in Hz.
 */
uint32_t CRM_GetIrFreq(void);

/**
 * @brief Enables the GPADC clock by setting the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_GPADC_CLK_ENABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG5.bit.ENA_GPADC_CLK = 0x1; \
} while(0)

/**
 * @brief Disables the GPADC clock by clearing the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_GPADC_CLK_DISABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG5.bit.ENA_GPADC_CLK = 0x0; \
} while(0)

/**
 * @brief Checks if the GPADC clock is enabled.
 * @return 1 if enabled, 0 if disabled.
 */
static inline uint8_t HAL_CRM_GpadcClkIsEnabled(){
    return IP_CMN_SYSCFG->REG_PERI_CLK_CFG5.bit.ENA_GPADC_CLK;
}

/**
 * @brief Sets the divider for the GPADC clock.

 * @param div_m Divider value (M) for the GPADC clock.
 * @return Status code (0 for success, negative for error).
 */

int32_t HAL_CRM_SetGpadcClkDiv(
    uint32_t div_m);

/**
 * @brief Retrieves the current GPADC clock configuration.

 * @param div_m Pointer to store the divider value (M).
 */
static inline void HAL_CRM_GetGpadcClkConfig( uint32_t *div_m){
    
    *div_m = IP_CMN_SYSCFG->REG_PERI_CLK_CFG5.bit.DIV_GPADC_CLK_M;
}

/**
 * @brief Gets the current frequency of the GPADC clock.
 * @return Frequency in Hz.
 */
uint32_t CRM_GetGpadcFreq(void);

/**
 * @brief Enables the I2S0 clock by setting the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_I2S0_CLK_ENABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.ENA_I2S0_CLK = 0x1; \
} while(0)

/**
 * @brief Disables the I2S0 clock by clearing the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_I2S0_CLK_DISABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.ENA_I2S0_CLK = 0x0; \
} while(0)

/**
 * @brief Checks if the I2S0 clock is enabled.
 * @return 1 if enabled, 0 if disabled.
 */
static inline uint8_t HAL_CRM_I2s0ClkIsEnabled(){
    return IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.ENA_I2S0_CLK;
}

/**
  * @brief  Function to configure the I2S0 clock source
  * @param  src specifies which source that I2S0 can choose
  *         This parameter can be one of the following values:
  *         @arg 0: I2S0 using CORE24M to source clock
  *         @arg 1: I2S0 using SyspllAud to source clock
  * @return CSK HAL status
  */
int32_t HAL_CRM_SetI2s0ClkSrc(clock_src_name_t src);

/**
 * @brief Retrieves the current I2S0 clock configuration.
 * @param src Pointer to store the clock source (from clock_src_name_t).

 */
static inline void HAL_CRM_GetI2s0ClkConfig(clock_src_name_t *src ){
    uint32_t src_t;
    src_t = IP_CMN_SYSCFG->REG_PERI_CLK_CFG6.bit.SEL_I2S0_MCLK;
    if (src_t == 0){
        *src = CRM_IpSrcCORE24M;
    }if (src_t == 1){
        *src = CRM_IpSrcSyspllAud;
    }
    
}

/**
 * @brief Gets the current frequency of the I2S0 clock.
 * @return Frequency in Hz.
 */
uint32_t CRM_GetI2s0Freq(void);

/**
 * @brief Enables the I2S1 clock by setting the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_I2S1_CLK_ENABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.ENA_I2S1_CLK = 0x1; \
} while(0)

/**
 * @brief Disables the I2S1 clock by clearing the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_I2S1_CLK_DISABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.ENA_I2S1_CLK = 0x0; \
} while(0)

/**
 * @brief Checks if the I2S1 clock is enabled.
 * @return 1 if enabled, 0 if disabled.
 */
static inline uint8_t HAL_CRM_I2s1ClkIsEnabled(){
    return IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.ENA_I2S1_CLK;
}

/**
  * @brief  Function to configure the I2S1 clock source
  * @param  src specifies which source that I2S1 can choose
  *         This parameter can be one of the following values:
  *         @arg 0: I2S1 using CORE24M to source clock
  *         @arg 1: I2S1 using SyspllAud to source clock
  * @return CSK HAL status
  */
int32_t HAL_CRM_SetI2s1ClkSrc(clock_src_name_t src);

/**
 * @brief Retrieves the current I2S1 clock configuration.
 * @param src Pointer to store the clock source (from clock_src_name_t).

 */
static inline void HAL_CRM_GetI2s1ClkConfig(clock_src_name_t *src ){
    uint32_t src_t;
    src_t = IP_CMN_SYSCFG->REG_PERI_CLK_CFG6.bit.SEL_I2S1_MCLK;
    if (src_t == 0){
        *src = CRM_IpSrcCORE24M;
    }if (src_t == 1){
        *src = CRM_IpSrcSyspllAud;
    }
    
}

/**
 * @brief Gets the current frequency of the I2S1 clock.
 * @return Frequency in Hz.
 */
uint32_t CRM_GetI2s1Freq(void);

/**
 * @brief Enables the RGB clock by setting the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_RGB_CLK_ENABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG6.bit.ENA_RGB_CLK = 0x1; \
} while(0)

/**
 * @brief Disables the RGB clock by clearing the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_RGB_CLK_DISABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG6.bit.ENA_RGB_CLK = 0x0; \
} while(0)

/**
 * @brief Checks if the RGB clock is enabled.
 * @return 1 if enabled, 0 if disabled.
 */
static inline uint8_t HAL_CRM_RgbClkIsEnabled(){
    return IP_CMN_SYSCFG->REG_PERI_CLK_CFG6.bit.ENA_RGB_CLK;
}

/**
 * @brief Sets the divider for the RGB clock.

 * @param div_m Divider value (M) for the RGB clock.
 * @return Status code (0 for success, negative for error).
 */

int32_t HAL_CRM_SetRgbClkDiv(
    uint32_t div_m);

/**
  * @brief  Function to configure the RGB clock source
  * @param  src specifies which source that RGB can choose
  *         This parameter can be one of the following values:
  *         @arg 0: RGB using CORE24M to source clock
  *         @arg 1: RGB using SyspllPeri to source clock
  * @return CSK HAL status
  */
int32_t HAL_CRM_SetRgbClkSrc(clock_src_name_t src);

/**
 * @brief Retrieves the current RGB clock configuration.
 * @param src Pointer to store the clock source (from clock_src_name_t).

 * @param div_m Pointer to store the divider value (M).
 */
static inline void HAL_CRM_GetRgbClkConfig(clock_src_name_t *src, uint32_t *div_m){
    uint32_t src_t;
    src_t = IP_CMN_SYSCFG->REG_PERI_CLK_CFG6.bit.SEL_RGB_CLK;
    if (src_t == 0){
        *src = CRM_IpSrcCORE24M;
    }if (src_t == 1){
        *src = CRM_IpSrcSyspllPeri;
    }
    
    *div_m = IP_CMN_SYSCFG->REG_PERI_CLK_CFG6.bit.DIV_RGB_CLK_M;
}

/**
 * @brief Gets the current frequency of the RGB clock.
 * @return Frequency in Hz.
 */
uint32_t CRM_GetRgbFreq(void);

/**
 * @brief Enables the VIC clock by setting the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_VIC_CLK_ENABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.ENA_VIC_CLK = 0x1; \
} while(0)

/**
 * @brief Disables the VIC clock by clearing the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_VIC_CLK_DISABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.ENA_VIC_CLK = 0x0; \
} while(0)

/**
 * @brief Checks if the VIC clock is enabled.
 * @return 1 if enabled, 0 if disabled.
 */
static inline uint8_t HAL_CRM_VicClkIsEnabled(){
    return IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.ENA_VIC_CLK;
}

/**
  * @brief  Function to configure the VIC clock source
  * @param  src specifies which source that VIC can choose
  *         This parameter can be one of the following values:
  *         @arg 0: VIC using CORE24M to source clock
  *         @arg 1: VIC using SyspllAud to source clock
  * @return CSK HAL status
  */
int32_t HAL_CRM_SetVicClkSrc(clock_src_name_t src);

/**
 * @brief Retrieves the current VIC clock configuration.
 * @param src Pointer to store the clock source (from clock_src_name_t).

 */
static inline void HAL_CRM_GetVicClkConfig(clock_src_name_t *src ){
    uint32_t src_t;
    src_t = IP_CMN_SYSCFG->REG_PERI_CLK_CFG6.bit.SEL_VIC_CLK;
    if (src_t == 0){
        *src = CRM_IpSrcCORE24M;
    }if (src_t == 1){
        *src = CRM_IpSrcSyspllPeri;
    }
    
}

/**
 * @brief Gets the current frequency of the VIC clock.
 * @return Frequency in Hz.
 */
uint32_t CRM_GetVicFreq(void);

/**
 * @brief Enables the QSPI1 clock by setting the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_QSPI1_CLK_ENABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG6.bit.ENA_QSPI1_CLK = 0x1; \
} while(0)

/**
 * @brief Disables the QSPI1 clock by clearing the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_QSPI1_CLK_DISABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG6.bit.ENA_QSPI1_CLK = 0x0; \
} while(0)

/**
 * @brief Checks if the QSPI1 clock is enabled.
 * @return 1 if enabled, 0 if disabled.
 */
static inline uint8_t HAL_CRM_Qspi1ClkIsEnabled(){
    return IP_CMN_SYSCFG->REG_PERI_CLK_CFG6.bit.ENA_QSPI1_CLK;
}

/**
 * @brief Sets the divider for the QSPI1 clock.
 * @param div_n Divider value (N) for the QSPI1 clock.
 * @param div_m Divider value (M) for the QSPI1 clock.
 * @return Status code (0 for success, negative for error).
 */

int32_t HAL_CRM_SetQspi1ClkDiv(
    uint32_t div_n, uint32_t div_m);

/**
  * @brief  Function to configure the QSPI1 clock source
  * @param  src specifies which source that QSPI1 can choose
  *         This parameter can be one of the following values:
  *         @arg 0: QSPI1 using CORE24M to source clock
  *         @arg 1: QSPI1 using SyspllPeri to source clock
  * @return CSK HAL status
  */
int32_t HAL_CRM_SetQspi1ClkSrc(clock_src_name_t src);

/**
 * @brief Retrieves the current QSPI1 clock configuration.
 * @param src Pointer to store the clock source (from clock_src_name_t).
 * @param div_n Pointer to store the divider value (N).
 * @param div_m Pointer to store the divider value (M).
 */
static inline void HAL_CRM_GetQspi1ClkConfig(clock_src_name_t *src,uint32_t *div_n, uint32_t *div_m){
    uint32_t src_t;
    src_t = IP_CMN_SYSCFG->REG_PERI_CLK_CFG6.bit.SEL_QSPI1_CLK;
    if (src_t == 0){
        *src = CRM_IpSrcCORE24M;
    }if (src_t == 1){
        *src = CRM_IpSrcSyspllPeri;
    }
    *div_n = IP_CMN_SYSCFG->REG_PERI_CLK_CFG6.bit.DIV_QSPI1_CLK_N;
    *div_m = IP_CMN_SYSCFG->REG_PERI_CLK_CFG6.bit.DIV_QSPI1_CLK_M;
}

/**
 * @brief Gets the current frequency of the QSPI1 clock.
 * @return Frequency in Hz.
 */
uint32_t CRM_GetQspi1Freq(void);

/**
 * @brief Enables the QSPI0 clock by setting the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_QSPI0_CLK_ENABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG6.bit.ENA_QSPI0_CLK = 0x1; \
} while(0)

/**
 * @brief Disables the QSPI0 clock by clearing the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_QSPI0_CLK_DISABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG6.bit.ENA_QSPI0_CLK = 0x0; \
} while(0)

/**
 * @brief Checks if the QSPI0 clock is enabled.
 * @return 1 if enabled, 0 if disabled.
 */
static inline uint8_t HAL_CRM_Qspi0ClkIsEnabled(){
    return IP_CMN_SYSCFG->REG_PERI_CLK_CFG6.bit.ENA_QSPI0_CLK;
}

/**
 * @brief Sets the divider for the QSPI0 clock.
 * @param div_n Divider value (N) for the QSPI0 clock.
 * @param div_m Divider value (M) for the QSPI0 clock.
 * @return Status code (0 for success, negative for error).
 */

int32_t HAL_CRM_SetQspi0ClkDiv(
    uint32_t div_n, uint32_t div_m);

/**
  * @brief  Function to configure the QSPI0 clock source
  * @param  src specifies which source that QSPI0 can choose
  *         This parameter can be one of the following values:
  *         @arg 0: QSPI0 using CORE24M to source clock
  *         @arg 1: QSPI0 using SyspllPeri to source clock
  * @return CSK HAL status
  */
int32_t HAL_CRM_SetQspi0ClkSrc(clock_src_name_t src);

/**
 * @brief Retrieves the current QSPI0 clock configuration.
 * @param src Pointer to store the clock source (from clock_src_name_t).
 * @param div_n Pointer to store the divider value (N).
 * @param div_m Pointer to store the divider value (M).
 */
static inline void HAL_CRM_GetQspi0ClkConfig(clock_src_name_t *src,uint32_t *div_n, uint32_t *div_m){
    uint32_t src_t;
    src_t = IP_CMN_SYSCFG->REG_PERI_CLK_CFG6.bit.SEL_QSPI0_CLK;
    if (src_t == 0){
        *src = CRM_IpSrcCORE24M;
    }if (src_t == 1){
        *src = CRM_IpSrcSyspllPeri;
    }
    *div_n = IP_CMN_SYSCFG->REG_PERI_CLK_CFG6.bit.DIV_QSPI0_CLK_N;
    *div_m = IP_CMN_SYSCFG->REG_PERI_CLK_CFG6.bit.DIV_QSPI0_CLK_M;
}

/**
 * @brief Gets the current frequency of the QSPI0 clock.
 * @return Frequency in Hz.
 */
uint32_t CRM_GetQspi0Freq(void);

/**
  * @brief  Function to configure the DAC clock source
  * @param  src specifies which source that DAC can choose
  *         This parameter can be one of the following values:
  *         @arg 0: DAC using CORE24M to source clock
  *         @arg 1: DAC using SyspllAud to source clock
  * @return CSK HAL status
  */
int32_t HAL_CRM_SetDacClkSrc(clock_src_name_t src);

/**
 * @brief Retrieves the current DAC clock configuration.
 * @param src Pointer to store the clock source (from clock_src_name_t).

 */
static inline void HAL_CRM_GetDacClkConfig(clock_src_name_t *src ){
    uint32_t src_t;
    src_t = IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.AUD_DAC_MCLK_SRC;
    if (src_t == 0){
        *src = CRM_IpSrcCORE24M;
    }if (src_t == 1){
        *src = CRM_IpSrcSyspllAud;
    }
    
}

/**
 * @brief Gets the current frequency of the DAC clock.
 * @return Frequency in Hz.
 */
uint32_t CRM_GetDacFreq(void);

/**
  * @brief  Function to configure the ADC clock source
  * @param  src specifies which source that ADC can choose
  *         This parameter can be one of the following values:
  *         @arg 0: ADC using CORE24M to source clock
  *         @arg 1: ADC using SyspllAud to source clock
  * @return CSK HAL status
  */
int32_t HAL_CRM_SetAdcClkSrc(clock_src_name_t src);

/**
 * @brief Retrieves the current ADC clock configuration.
 * @param src Pointer to store the clock source (from clock_src_name_t).

 */
static inline void HAL_CRM_GetAdcClkConfig(clock_src_name_t *src ){
    uint32_t src_t;
    src_t = IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.AUD_ADC_CLK_SRC;
    if (src_t == 0){
        *src = CRM_IpSrcCORE24M;
    }if (src_t == 1){
        *src = CRM_IpSrcSyspllAud;
    }
    
}

/**
 * @brief Gets the current frequency of the ADC clock.
 * @return Frequency in Hz.
 */
uint32_t CRM_GetAdcFreq(void);

/**
 * @brief Enables the APC clock by setting the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_APC_CLK_ENABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.ENA_APC_CLK = 0x1; \
} while(0)

/**
 * @brief Disables the APC clock by clearing the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_APC_CLK_DISABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.ENA_APC_CLK = 0x0; \
} while(0)

/**
 * @brief Checks if the APC clock is enabled.
 * @return 1 if enabled, 0 if disabled.
 */
static inline uint8_t HAL_CRM_ApcClkIsEnabled(){
    return IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.ENA_APC_CLK;
}

/**
 * @brief Gets the current frequency of the APC clock.
 * @return Frequency in Hz.
 */
uint32_t CRM_GetApcFreq(void);

/**
 * @brief Enables the CODEC clock by setting the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_CODEC_CLK_ENABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.ENA_CODEC_CLK = 0x1; \
} while(0)

/**
 * @brief Disables the CODEC clock by clearing the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_CODEC_CLK_DISABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.ENA_CODEC_CLK = 0x0; \
} while(0)

/**
 * @brief Checks if the CODEC clock is enabled.
 * @return 1 if enabled, 0 if disabled.
 */
static inline uint8_t HAL_CRM_CodecClkIsEnabled(){
    return IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.ENA_CODEC_CLK;
}

/**
 * @brief Gets the current frequency of the CODEC clock.
 * @return Frequency in Hz.
 */
uint32_t CRM_GetCodecFreq(void);

/**
 * @brief Enables the JPEG clock by setting the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_JPEG_CLK_ENABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.ENA_JPEG_CLK = 0x1; \
} while(0)

/**
 * @brief Disables the JPEG clock by clearing the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_JPEG_CLK_DISABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.ENA_JPEG_CLK = 0x0; \
} while(0)

/**
 * @brief Checks if the JPEG clock is enabled.
 * @return 1 if enabled, 0 if disabled.
 */
static inline uint8_t HAL_CRM_JpegClkIsEnabled(){
    return IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.ENA_JPEG_CLK;
}

/**
 * @brief Gets the current frequency of the JPEG clock.
 * @return Frequency in Hz.
 */
uint32_t CRM_GetJpegFreq(void);

/**
 * @brief Enables the LUNA clock by setting the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_LUNA_CLK_ENABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.ENA_LUNA_CLK = 0x1; \
} while(0)

/**
 * @brief Disables the LUNA clock by clearing the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_LUNA_CLK_DISABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.ENA_LUNA_CLK = 0x0; \
} while(0)

/**
 * @brief Checks if the LUNA clock is enabled.
 * @return 1 if enabled, 0 if disabled.
 */
static inline uint8_t HAL_CRM_LunaClkIsEnabled(){
    return IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.ENA_LUNA_CLK;
}

/**
 * @brief Gets the current frequency of the LUNA clock.
 * @return Frequency in Hz.
 */
uint32_t CRM_GetLunaFreq(void);

/**
 * @brief Enables the GPIO1 clock by setting the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_GPIO1_CLK_ENABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.ENA_GPIO1_CLK = 0x1; \
} while(0)

/**
 * @brief Disables the GPIO1 clock by clearing the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_GPIO1_CLK_DISABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.ENA_GPIO1_CLK = 0x0; \
} while(0)

/**
 * @brief Checks if the GPIO1 clock is enabled.
 * @return 1 if enabled, 0 if disabled.
 */
static inline uint8_t HAL_CRM_Gpio1ClkIsEnabled(){
    return IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.ENA_GPIO1_CLK;
}

/**
 * @brief Gets the current frequency of the GPIO1 clock.
 * @return Frequency in Hz.
 */
uint32_t CRM_GetGpio1Freq(void);

/**
 * @brief Enables the GPIO0 clock by setting the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_GPIO0_CLK_ENABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.ENA_GPIO0_CLK = 0x1; \
} while(0)

/**
 * @brief Disables the GPIO0 clock by clearing the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_GPIO0_CLK_DISABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.ENA_GPIO0_CLK = 0x0; \
} while(0)

/**
 * @brief Checks if the GPIO0 clock is enabled.
 * @return 1 if enabled, 0 if disabled.
 */
static inline uint8_t HAL_CRM_Gpio0ClkIsEnabled(){
    return IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.ENA_GPIO0_CLK;
}

/**
 * @brief Gets the current frequency of the GPIO0 clock.
 * @return Frequency in Hz.
 */
uint32_t CRM_GetGpio0Freq(void);

/**
 * @brief Enables the GPDMA clock by setting the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_GPDMA_CLK_ENABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.ENA_GPDMA2D_CLK = 0x1; \
} while(0)

/**
 * @brief Disables the GPDMA clock by clearing the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_GPDMA_CLK_DISABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.ENA_GPDMA2D_CLK = 0x0; \
} while(0)

/**
 * @brief Checks if the GPDMA clock is enabled.
 * @return 1 if enabled, 0 if disabled.
 */
static inline uint8_t HAL_CRM_GpdmaClkIsEnabled(){
    return IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.ENA_GPDMA2D_CLK;
}

/**
 * @brief Gets the current frequency of the GPDMA clock.
 * @return Frequency in Hz.
 */
uint32_t CRM_GetGpdmaFreq(void);

/**
 * @brief Enables the CMNDMA clock by setting the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_CMNDMA_CLK_ENABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.ENA_CMNDMAC_CLK = 0x1; \
} while(0)

/**
 * @brief Disables the CMNDMA clock by clearing the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_CMNDMA_CLK_DISABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.ENA_CMNDMAC_CLK = 0x0; \
} while(0)

/**
 * @brief Checks if the CMNDMA clock is enabled.
 * @return 1 if enabled, 0 if disabled.
 */
static inline uint8_t HAL_CRM_CmndmaClkIsEnabled(){
    return IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.ENA_CMNDMAC_CLK;
}

/**
 * @brief Gets the current frequency of the CMNDMA clock.
 * @return Frequency in Hz.
 */
uint32_t CRM_GetCmndmaFreq(void);

/**
 * @brief Enables the USB clock by setting the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_USB_CLK_ENABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.ENA_USB_CLK = 0x1; \
} while(0)

/**
 * @brief Disables the USB clock by clearing the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_USB_CLK_DISABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.ENA_USB_CLK = 0x0; \
} while(0)

/**
 * @brief Checks if the USB clock is enabled.
 * @return 1 if enabled, 0 if disabled.
 */
static inline uint8_t HAL_CRM_UsbClkIsEnabled(){
    return IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.ENA_USB_CLK;
}

/**
 * @brief Gets the current frequency of the USB clock.
 * @return Frequency in Hz.
 */
uint32_t CRM_GetUsbFreq(void);

/**
 * @brief Enables the I2C1 clock by setting the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_I2C1_CLK_ENABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.ENA_I2C1_CLK = 0x1; \
} while(0)

/**
 * @brief Disables the I2C1 clock by clearing the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_I2C1_CLK_DISABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.ENA_I2C1_CLK = 0x0; \
} while(0)

/**
 * @brief Checks if the I2C1 clock is enabled.
 * @return 1 if enabled, 0 if disabled.
 */
static inline uint8_t HAL_CRM_I2c1ClkIsEnabled(){
    return IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.ENA_I2C1_CLK;
}

/**
 * @brief Gets the current frequency of the I2C1 clock.
 * @return Frequency in Hz.
 */
uint32_t CRM_GetI2c1Freq(void);

/**
 * @brief Enables the I2C0 clock by setting the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_I2C0_CLK_ENABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.ENA_I2C0_CLK = 0x1; \
} while(0)

/**
 * @brief Disables the I2C0 clock by clearing the enable bit in the peripheral clock configuration register.
 */
#define __HAL_CRM_I2C0_CLK_DISABLE() \
do{ \
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.ENA_I2C0_CLK = 0x0; \
} while(0)

/**
 * @brief Checks if the I2C0 clock is enabled.
 * @return 1 if enabled, 0 if disabled.
 */
static inline uint8_t HAL_CRM_I2c0ClkIsEnabled(){
    return IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.ENA_I2C0_CLK;
}

/**
 * @brief Gets the current frequency of the I2C0 clock.
 * @return Frequency in Hz.
 */
uint32_t CRM_GetI2c0Freq(void);

/**
  * @brief  Function to configure the SYSCLK clock source
  * @param  src specifies which source that SYSCLK can choose
  *         This parameter can be one of the following values:
  *         @arg 0: SYSCLK using CORE24M to source clock
  *         @arg 1: SYSCLK using RC024M to source clock
  *         @arg 2: SYSCLK using SyspllCore to source clock
  * @return CSK HAL status
  */
int32_t HAL_CRM_SetSysclkClkSrc(clock_src_name_t src);

/**
 * @brief Retrieves the current SYSCLK clock configuration.
 * @param src Pointer to store the clock source (from clock_src_name_t).

 */
static inline void HAL_CRM_GetSysclkClkConfig(clock_src_name_t *src ){
    uint32_t src_t;
    src_t = IP_CMN_BUSCFG->REG_BUS_CLK_CFG0.bit.SEL_HCLK;
    if (src_t == 0){
        *src = CRM_IpSrcCORE24M;
    }if (src_t == 1){
        *src = CRM_IpSrcRC024M;
    }if (src_t == 2){
        *src = CRM_IpSrcSyspllCore;
    }
    
}

/**
 * @brief Gets the current frequency of the SYSCLK clock.
 * @return Frequency in Hz.
 */
uint32_t CRM_GetSysclkFreq(void);

/**
 * @brief Sets the divider for the HCLK clock.
 * @param div_n Divider value (N) for the HCLK clock.
 * @param div_m Divider value (M) for the HCLK clock.
 * @return Status code (0 for success, negative for error).
 */

int32_t HAL_CRM_SetHclkClkDiv(
    uint32_t div_n, uint32_t div_m);

/**
 * @brief Retrieves the current HCLK clock configuration.

 * @param div_n Pointer to store the divider value (N).
 * @param div_m Pointer to store the divider value (M).
 */
static inline void HAL_CRM_GetHclkClkConfig(uint32_t *div_n, uint32_t *div_m){
    *div_n = IP_CMN_BUSCFG->REG_BUS_CLK_CFG0.bit.DIV_HCLK_N;
    *div_m = IP_CMN_BUSCFG->REG_BUS_CLK_CFG0.bit.DIV_HCLK_M;
}

/**
 * @brief Gets the current frequency of the HCLK clock.
 * @return Frequency in Hz.
 */
uint32_t CRM_GetHclkFreq(void);

/**
 * @brief Sets the divider for the CMN_PCLK clock.
 * @param div_n Divider value (N) for the CMN_PCLK clock.
 * @param div_m Divider value (M) for the CMN_PCLK clock.
 * @return Status code (0 for success, negative for error).
 */

int32_t HAL_CRM_SetCmn_pclkClkDiv(
    uint32_t div_n, uint32_t div_m);

/**
 * @brief Retrieves the current CMN_PCLK clock configuration.

 * @param div_n Pointer to store the divider value (N).
 * @param div_m Pointer to store the divider value (M).
 */
static inline void HAL_CRM_GetCmn_pclkClkConfig(uint32_t *div_n, uint32_t *div_m){
    *div_n = IP_CMN_BUSCFG->REG_BUS_CLK_CFG1.bit.DIV_CMN_PERI_PCLK_N;
    *div_m = IP_CMN_BUSCFG->REG_BUS_CLK_CFG1.bit.DIV_CMN_PERI_PCLK_M;
}

/**
 * @brief Gets the current frequency of the CMN_PCLK clock.
 * @return Frequency in Hz.
 */
uint32_t CRM_GetCmn_pclkFreq(void);

/**
 * @brief Sets the divider for the AON_CFG_PCLK clock.
 * @param div_n Divider value (N) for the AON_CFG_PCLK clock.
 * @param div_m Divider value (M) for the AON_CFG_PCLK clock.
 * @return Status code (0 for success, negative for error).
 */

int32_t HAL_CRM_SetAon_cfg_pclkClkDiv(
    uint32_t div_n, uint32_t div_m);

/**
 * @brief Retrieves the current AON_CFG_PCLK clock configuration.

 * @param div_n Pointer to store the divider value (N).
 * @param div_m Pointer to store the divider value (M).
 */
static inline void HAL_CRM_GetAon_cfg_pclkClkConfig(uint32_t *div_n, uint32_t *div_m){
    *div_n = IP_CMN_BUSCFG->REG_BUS_CLK_CFG1.bit.DIV_AON_CFG_PCLK_N;
    *div_m = IP_CMN_BUSCFG->REG_BUS_CLK_CFG1.bit.DIV_AON_CFG_PCLK_M;
}

/**
 * @brief Gets the current frequency of the AON_CFG_PCLK clock.
 * @return Frequency in Hz.
 */
uint32_t CRM_GetAon_cfg_pclkFreq(void);

/**
 * @brief Gets the current frequency of the CORE0 clock.
 * @return Frequency in Hz.
 */
uint32_t CRM_GetCore0Freq(void);

/**
 * @brief Gets the current frequency of the CORE1 clock.
 * @return Frequency in Hz.
 */
uint32_t CRM_GetCore1Freq(void);

int32_t SYSPLL_Init();

int32_t CRM_InitSyspllCore(clock_src_syspllcore_div div);

int32_t CRM_InitSyspllPeri(clock_src_syspllperi_div div);

int32_t CRM_InitSyspllFlash(clock_src_syspllflash_div div);

int32_t CRM_InitSyspllPsram(clock_src_syspllpsram_div div);

int32_t CRM_InitSyspllSdio(clock_src_syspllsdio_div div);

#endif /* INCLUDE_CLOCKMANAGER_H_ */

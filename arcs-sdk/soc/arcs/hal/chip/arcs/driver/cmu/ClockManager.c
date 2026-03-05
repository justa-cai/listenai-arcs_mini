/**
  ******************************************************************************
  * @file    ClockManager.c
  * @author  ListenAI Application Team
  * @brief   Clock Manager HAL module driver.
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
/* Includes ------------------------------------------------------------------*/
#include "ClockManager.h"

/* Macro ------------------------------------------------------------------*/
#define MemBarrier()          __COMPILER_BARRIER()

void VCO_Init(uint32_t n, uint32_t frac_n){    
	IP_SYSNODEF->REG_SYSPLL_CFG4.bit.SYSPLL_SDM_DIVN_INTEG = n;  // 24 * n
    if (frac_n) {
        IP_SYSNODEF->REG_SYSPLL_CFG3.bit.SYSPLL_SDM_DIVN_FRAC = frac_n; //24 * (frac_n / 2^27)
	    IP_SYSNODEF->REG_SYSPLL_CFG4.bit.SYSPLL_SDM_EN = 1;
    }
}

/**********************************PLL************************************/
/**
 * @brief Initializes the System PLL (Phase-Locked Loop).
 *
 * This function activates and configures the System PLL, which is a critical component
 * for generating the system's high-speed clock signals. The System PLL multiplies a base
 * input frequency to a higher frequency used to drive the processor and peripherals. This
 * initialization is essential for achieving the desired performance and functionality
 * of the system.
 *
 * @return Always return CSK_DRIVER_OK.
 *
 * @note The System PLL must be correctly configured and stabilized before its output can be
 *       used as a clock source for the system. This function typically involves waiting for
 *       the PLL to lock to the desired frequency, which may take some time.
 *
 * @warning Incorrect configuration of the System PLL can lead to system instability or
 *          failure to achieve the required clock speeds for operation. It is crucial to
 *          ensure that the PLL settings, including the input frequency and multiplication
 *          factors, are compatible with the system's specifications.
 */
int32_t SYSPLL_Init(){
#if (IC_BOARD == 0)
    return CSK_DRIVER_OK;
#else
    IP_SYSNODEF->REG_SYSPLL_CFG0.bit.SYSPLL_ENABLE = 0x1;
    while(!IP_SYSNODEF->REG_SYSPLL_CFG0.bit.SYSPLL_LOCK);
    return CSK_DRIVER_OK;
#endif
}
/**
 * @brief Initializes the System PLL (Phase-Locked Loop).
 *
 * This function activates and configures the System PLL, which is a critical component
 * for generating the system's high-speed clock signals. The System PLL multiplies a base
 * input frequency to a higher frequency used to drive the processor and peripherals. This
 * initialization is essential for achieving the desired performance and functionality
 * of the system.
 *
 * @return Always return CSK_DRIVER_OK.
 *
 * @note The System PLL must be correctly configured and stabilized before its output can be
 *       used as a clock source for the system. This function typically involves waiting for
 *       the PLL to lock to the desired frequency, which may take some time.
 *
 * @warning Incorrect configuration of the System PLL can lead to system instability or
 *          failure to achieve the required clock speeds for operation. It is crucial to
 *          ensure that the PLL settings, including the input frequency and multiplication
 *          factors, are compatible with the system's specifications.
 */
int32_t BBPLL_Init(){
#if (IC_BOARD == 0)
    return CSK_DRIVER_OK;
#else
    IP_SYSNODEF->REG_BBPLL_CFG0.bit.BBPLL_ENABLE = 0x1;
    while(!IP_SYSNODEF->REG_BBPLL_CFG0.bit.BBPLL_LOCK);
    return CSK_DRIVER_OK;
#endif
}

int32_t CRM_BBPLL_InitCoreSrc(bbpll_clock_src_core_div_t div){
    if (div > 4){
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    IP_SYSNODEF->REG_BBPLL_CFG0.bit.BBPLL_POSTDIV_SYSSEL = div;
    IP_SYSNODEF->REG_BBPLL_CFG0.bit.BBPLL_ADDABUF_SYSCLKEN = 0x1;
    return CSK_DRIVER_OK;
}

static uint32_t CRM_GetBBPLLCoreSrcFreq(){
    uint32_t div = IP_SYSNODEF->REG_BBPLL_CFG0.bit.BBPLL_POSTDIV_SYSSEL;
    if (IP_SYSNODEF->REG_BBPLL_CFG0.bit.BBPLL_ADDABUF_SYSCLKEN == 0){
        return 0;
    }

    switch(div){
        case BBPLL_CRM_IpCore_96Mhz:
            return BOARD_BOOTCLOCKRUN_BBPLL_CLK/10;
        case BBPLL_CRM_IpCore_160Mhz:
            return BOARD_BOOTCLOCKRUN_BBPLL_CLK/6;
        case BBPLL_CRM_IpCore_240Mhz:
            return BOARD_BOOTCLOCKRUN_BBPLL_CLK/4;
        case BBPLL_CRM_IpCore_960Mhz:
            return BOARD_BOOTCLOCKRUN_BBPLL_CLK;
        default:
            return 0;
    }
}

/**********************************SRC************************************/
/**
 * @brief Initializes the Core clock source with a specified divider.
 *
 * This function configures the Core clock source of the system by applying the specified
 * divider. It is designed to set up the core clock speed according to the system's requirements.
 * The divider parameter allows for flexible adjustment of the clock frequency, enabling
 * optimization of the system's performance and power consumption.
 *
 * @param div The divider value to apply to the Core clock source. This parameter is of type
 *            clock_src_core_div_t, which is an enumeration listing the possible divider values.
 *            The exact values and their corresponding divider ratios are dependent on the
 *            system's hardware specifications.
 *
 * @return uint32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                  can indicate issues such as an invalid clock divider.
 *
 * @note This function must be called to ensure the system's core clock is correctly configured
 *       before performing any operations that depend on the core clock. Incorrect configuration
 *       may lead to system instability or malfunction.
 *
 * @warning Care should be taken when choosing the divider value, as incorrect settings can
 *          adversely affect the system's performance and functionality. It is recommended to
 *          consult the system's hardware documentation or a system engineer when configuring
 *          the core clock source.
 */

int32_t CRM_InitCoreSrc(clock_src_core_div_t div){
    if (div > 15){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    IP_SYSNODEF->REG_SYSPLL_CFG1.bit.SYSPLL_POSTDIV_SYSTEM_DIV_SEL = div;
    return CSK_DRIVER_OK;
}

/**
 * @brief Retrieves the frequency of the Core source.
 *
 * This function returns the current frequency (in Hz) of the Core source used by the system.
 * It is a static function, intended for internal use within the module where it is declared.
 * The frequency is determined by the underlying hardware configuration and the system's
 * current state.
 *
 * @return uint32_t The frequency of the Core source in Hertz.
 *
 * @note The value returned is dependent on the system's current configuration and may vary
 *       if the configuration changes.
 *
 * @warning This function should only be called after the system's clock sources have been
 *          properly configured. Calling it before system initialization may result in
 *          undefined behavior or incorrect values.
 */
static uint32_t CRM_GetCoreSrcFreq(void){
    uint32_t div = IP_SYSNODEF->REG_SYSPLL_CFG1.bit.SYSPLL_POSTDIV_SYSTEM_DIV_SEL;

    switch(div){
    case CRM_IpCore_300MHz:
        return BOARD_BOOTCLOCKRUN_SYSPLL_CLK/4;
    case CRM_IpCore_240MHz:
        return BOARD_BOOTCLOCKRUN_SYSPLL_CLK/5;
    case CRM_IpCore_200MHz:
        return BOARD_BOOTCLOCKRUN_SYSPLL_CLK/6;
    case CRM_IpCore_150MHz:
        return BOARD_BOOTCLOCKRUN_SYSPLL_CLK/8;
    case CRM_IpCore_133MHz:
        return BOARD_BOOTCLOCKRUN_SYSPLL_CLK/9;
    case CRM_IpCore_120MHz:
        return BOARD_BOOTCLOCKRUN_SYSPLL_CLK/10;
    case CRM_IpCore_100MHz:
        return BOARD_BOOTCLOCKRUN_SYSPLL_CLK/12;
    default:
        return 0;
    }
}
/**
 * @brief Initializes the Psram clock source with a specified divider.
 *
 * This function configures the Psram clock source of the system by applying the specified
 * divider. It is designed to set up the core clock speed according to the system's requirements.
 * The divider parameter allows for flexible adjustment of the clock frequency, enabling
 * optimization of the system's performance and power consumption.
 *
 * @param div The divider value to apply to the Psram clock source. This parameter is of type
 *            clock_src_psram_div_t, which is an enumeration listing the possible divider values.
 *            The exact values and their corresponding divider ratios are dependent on the
 *            system's hardware specifications.
 *
 * @return uint32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                  can indicate issues such as an invalid clock divider.
 *
 * @note This function must be called to ensure the system's core clock is correctly configured
 *       before performing any operations that depend on the core clock. Incorrect configuration
 *       may lead to system instability or malfunction.
 *
 * @warning Care should be taken when choosing the divider value, as incorrect settings can
 *          adversely affect the system's performance and functionality. It is recommended to
 *          consult the system's hardware documentation or a system engineer when configuring
 *          the core clock source.
 */

int32_t CRM_InitPsramSrc(clock_src_psram_div_t div){
    if (div > 15){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    IP_SYSNODEF->REG_SYSPLL_CFG2.bit.SYSPLL_POSTDIV_PSRAM_DIV_SEL = div;
    return CSK_DRIVER_OK;
}

/**
 * @brief Retrieves the frequency of the Psram source.
 *
 * This function returns the current frequency (in Hz) of the Psram source used by the system.
 * It is a static function, intended for internal use within the module where it is declared.
 * The frequency is determined by the underlying hardware configuration and the system's
 * current state.
 *
 * @return uint32_t The frequency of the Psram source in Hertz.
 *
 * @note The value returned is dependent on the system's current configuration and may vary
 *       if the configuration changes.
 *
 * @warning This function should only be called after the system's clock sources have been
 *          properly configured. Calling it before system initialization may result in
 *          undefined behavior or incorrect values.
 */
static uint32_t CRM_GetPsramSrcFreq(void){
    uint32_t div = IP_SYSNODEF->REG_SYSPLL_CFG2.bit.SYSPLL_POSTDIV_PSRAM_DIV_SEL;

    switch(div){
    case CRM_IpPsram_240MHz:
        return BOARD_BOOTCLOCKRUN_SYSPLL_CLK/5;    
    case CRM_IpPsram_200MHz:
        return BOARD_BOOTCLOCKRUN_SYSPLL_CLK/6;
    case CRM_IpPsram_150MHz:
        return BOARD_BOOTCLOCKRUN_SYSPLL_CLK/8;
    case CRM_IpPsram_133MHz:
        return BOARD_BOOTCLOCKRUN_SYSPLL_CLK/9;
    case CRM_IpPsram_120MHz:
        return BOARD_BOOTCLOCKRUN_SYSPLL_CLK/10;
    case CRM_IpPsram_100MHz:
        return BOARD_BOOTCLOCKRUN_SYSPLL_CLK/12;
    default:
        return 0;
    }
}

/**
 * @brief Retrieves the frequency of the Xtal source.
 *
 * This function returns the current frequency (in Hz) of the Xtal source used by the system.
 * It is a static function, intended for internal use within the module where it is declared.
 * The frequency is determined by the underlying hardware configuration and the system's
 * current state.
 *
 * @return uint32_t The frequency of the Xtal source in Hertz.
 *
 * @note The value returned is dependent on the system's current configuration and may vary
 *       if the configuration changes.
 *
 * @warning This function should only be called after the system's clock sources have been
 *          properly configured. Calling it before system initialization may result in
 *          undefined behavior or incorrect values.
 */
static uint32_t CRM_GetXtalSrcFreq(void){
    return IC_BOARD_XTAL_FREQ;
}
/**
 * @brief Initializes the Peri clock source with a specified divider.
 *
 * This function configures the Peri clock source of the system by applying the specified
 * divider. It is designed to set up the core clock speed according to the system's requirements.
 * The divider parameter allows for flexible adjustment of the clock frequency, enabling
 * optimization of the system's performance and power consumption.
 *
 * @param div The divider value to apply to the Peri clock source. This parameter is of type
 *            clock_src_peri_div_t, which is an enumeration listing the possible divider values.
 *            The exact values and their corresponding divider ratios are dependent on the
 *            system's hardware specifications.
 *
 * @return uint32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                  can indicate issues such as an invalid clock divider.
 *
 * @note This function must be called to ensure the system's core clock is correctly configured
 *       before performing any operations that depend on the core clock. Incorrect configuration
 *       may lead to system instability or malfunction.
 *
 * @warning Care should be taken when choosing the divider value, as incorrect settings can
 *          adversely affect the system's performance and functionality. It is recommended to
 *          consult the system's hardware documentation or a system engineer when configuring
 *          the core clock source.
 */

int32_t CRM_InitPeriSrc(clock_src_peri_div_t div){
    if (div > 3){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    IP_SYSNODEF->REG_SYSPLL_CFG1.bit.SYSPLL_POSTDIV_PERI_DIV_SEL = div;
    return CSK_DRIVER_OK;
}

/**
 * @brief Retrieves the frequency of the Peri source.
 *
 * This function returns the current frequency (in Hz) of the Peri source used by the system.
 * It is a static function, intended for internal use within the module where it is declared.
 * The frequency is determined by the underlying hardware configuration and the system's
 * current state.
 *
 * @return uint32_t The frequency of the Peri source in Hertz.
 *
 * @note The value returned is dependent on the system's current configuration and may vary
 *       if the configuration changes.
 *
 * @warning This function should only be called after the system's clock sources have been
 *          properly configured. Calling it before system initialization may result in
 *          undefined behavior or incorrect values.
 */
static uint32_t CRM_GetPeriSrcFreq(void){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    return BOARD_BOOTCLOCKRUN_SYSPLL_CLK/12;
#endif
}
/**
 * @brief Initializes the Flash clock source with a specified divider.
 *
 * This function configures the Flash clock source of the system by applying the specified
 * divider. It is designed to set up the core clock speed according to the system's requirements.
 * The divider parameter allows for flexible adjustment of the clock frequency, enabling
 * optimization of the system's performance and power consumption.
 *
 * @param div The divider value to apply to the Flash clock source. This parameter is of type
 *            clock_src_flash_div_t, which is an enumeration listing the possible divider values.
 *            The exact values and their corresponding divider ratios are dependent on the
 *            system's hardware specifications.
 *
 * @return uint32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                  can indicate issues such as an invalid clock divider.
 *
 * @note This function must be called to ensure the system's core clock is correctly configured
 *       before performing any operations that depend on the core clock. Incorrect configuration
 *       may lead to system instability or malfunction.
 *
 * @warning Care should be taken when choosing the divider value, as incorrect settings can
 *          adversely affect the system's performance and functionality. It is recommended to
 *          consult the system's hardware documentation or a system engineer when configuring
 *          the core clock source.
 */

__RAMCODE__ int32_t CRM_InitFlashSrc(clock_src_flash_div_t div){
    if (div > 3){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    IP_SYSNODEF->REG_SYSPLL_CFG1.bit.SYSPLL_POSTDIV_FLASH_DIV_SEL = div;
    return CSK_DRIVER_OK;
}

/**
 * @brief Retrieves the frequency of the Flash source.
 *
 * This function returns the current frequency (in Hz) of the Flash source used by the system.
 * It is a static function, intended for internal use within the module where it is declared.
 * The frequency is determined by the underlying hardware configuration and the system's
 * current state.
 *
 * @return uint32_t The frequency of the Flash source in Hertz.
 *
 * @note The value returned is dependent on the system's current configuration and may vary
 *       if the configuration changes.
 *
 * @warning This function should only be called after the system's clock sources have been
 *          properly configured. Calling it before system initialization may result in
 *          undefined behavior or incorrect values.
 */
static uint32_t CRM_GetFlashSrcFreq(void){
    uint32_t div = IP_SYSNODEF->REG_SYSPLL_CFG1.bit.SYSPLL_POSTDIV_FLASH_DIV_SEL;

    switch(div){
    case CRM_IpFlash_200MHz:
        return BOARD_BOOTCLOCKRUN_SYSPLL_CLK/6;
    case CRM_IpFlash_150MHz:
        return BOARD_BOOTCLOCKRUN_SYSPLL_CLK/8;
    case CRM_IpFlash_120MHz:
        return BOARD_BOOTCLOCKRUN_SYSPLL_CLK/10;
    case CRM_IpFlash_100MHz:
        return BOARD_BOOTCLOCKRUN_SYSPLL_CLK/12;
    default:
        return 0;
    }
}

/**
 * @brief Retrieves the frequency of the Cmn32k source.
 *
 * This function returns the current frequency (in Hz) of the Cmn32k source used by the system.
 * It is a static function, intended for internal use within the module where it is declared.
 * The frequency is determined by the underlying hardware configuration and the system's
 * current state.
 *
 * @return uint32_t The frequency of the Cmn32k source in Hertz.
 *
 * @note The value returned is dependent on the system's current configuration and may vary
 *       if the configuration changes.
 *
 * @warning This function should only be called after the system's clock sources have been
 *          properly configured. Calling it before system initialization may result in
 *          undefined behavior or incorrect values.
 */
static uint32_t CRM_GetCmn32kSrcFreq(void){
    return IC_BOARD_CMN32K_FREQ;
}

static uint32_t CRM_GetAon32kFreq(void) {
	return IC_BOARD_CMN32K_FREQ;
}

/**
 * @brief Retrieves the frequency of the Aon32k source.
 *
 * This function returns the current frequency (in Hz) of the Aon32k source used by the system.
 * It is a static function, intended for internal use within the module where it is declared.
 * The frequency is determined by the underlying hardware configuration and the system's
 * current state.
 *
 * @return uint32_t The frequency of the Aon32k source in Hertz.
 *
 * @note The value returned is dependent on the system's current configuration and may vary
 *       if the configuration changes.
 *
 * @warning This function should only be called after the system's clock sources have been
 *          properly configured. Calling it before system initialization may result in
 *          undefined behavior or incorrect values.
 */
static uint32_t CRM_GetRc32kSrcFreq(void){
    uint32_t rc32k_freq = IC_BOARD_CMN32K_FREQ;
    if(IP_AON_CTRL->REG_BT_RC_CALI.bit.RCCAL_RESULT) {
        uint8_t rccal_length = IP_AON_CTRL->REG_BT_RC_CALI.bit.RCCAL_LENGTH;
        rc32k_freq = ((CRM_GetXtalSrcFreq()<<rccal_length) / IP_AON_CTRL->REG_BT_RC_CALI.bit.RCCAL_RESULT);
    }
    return rc32k_freq;
}

/**
 * @brief Enables or disables the automatic trigger for RC32k calibration.
 */
void HAL_CRM_SetRc32kCaliAutoTrigger(uint8_t enable){
    if (enable) {
        IP_AON_CTRL->REG_BT_RC_CALI.bit.RCCAL_AUTO_TRIG_SEL = 1;
    } else {
        IP_AON_CTRL->REG_BT_RC_CALI.bit.RCCAL_AUTO_TRIG_SEL = 0;
    }
}

/**
 * @brief Sets the RC32k calibration cycle number.
 * This function configures the length of the RC32k calibration cycle by setting the
 * RCCAL_LENGTH field in the RCCAL register. The length parameter determines how many
 * cycles(2 ^ length) the calibration process will run, with a maximum value of 8(2 ^ 8).
 */
void HAL_CRM_SetRc32kCaliLength(uint8_t length){
    if (length > 8) {
        length = 8;
    }
    IP_AON_CTRL->REG_BT_RC_CALI.bit.RCCAL_LENGTH = length;
}

/**
 * @brief Starts the RC32k calibration process.
 * This function initiates the RC32k calibration by setting the RCCAL_START bit in the
 * RCCAL register. It also clears any previous calibration done interrupt status and waits
 * for the calibration to complete by polling the RCCAL_DONE_RAWSTAT bit in the RCCAL_IRQ
 * register.
 */
void HAL_CRM_SetRc32kCaliStart(){
    IP_AON_CTRL->REG_BT_RC_CALI_IRQ.bit.RCCAL_DONE_CLR = 1;
    IP_AON_CTRL->REG_BT_RC_CALI.bit.RCCAL_START = 0x1;
    while(!IP_AON_CTRL->REG_BT_RC_CALI_IRQ.bit.RCCAL_DONE_RAWSTAT);
}

/**
 * @brief Retrieves the frequency of a specified clock source.
 *
 * This function returns the current frequency (in Hz) of a specified clock source in the system.
 * The clock source is determined by the 'src' parameter, which should be one of the values
 * defined in the 'clock_src_name_t' enumeration.
 *
 * @param src The clock source for which the frequency is requested. This parameter should be
 *            a value from the 'clock_src_name_t' enumeration, indicating the specific clock source.
 *
 * @return uint32_t The frequency of the specified clock source in Hertz. If the specified source
 *                  is invalid or the frequency cannot be determined, the function may return 0
 *                  or an error code (if defined).
 *
 * @note The accuracy and availability of the returned frequency may depend on the system's
 *       current state and the specific clock source queried.
 *
 * @warning Ensure that the clock source specified in 'src' is initialized and active before
 *          calling this function. Querying an inactive or uninitialized clock source might
 *          lead to undefined behavior or incorrect frequency values.
 */
uint32_t CRM_GetSrcFreq(clock_src_name_t src){
    switch(src){
    case CRM_IpSrcCoreClk:
        return CRM_GetCoreSrcFreq();
    case CRM_IpSrcPsramClk:
        return CRM_GetPsramSrcFreq();
    case CRM_IpSrcXtalClk:
        return CRM_GetXtalSrcFreq();
    case CRM_IpSrcPeriClk:
        return CRM_GetPeriSrcFreq();
    case CRM_IpSrcFlashClk:
        return CRM_GetFlashSrcFreq();
    case CRM_IpSrcCmn32kClk:
        return CRM_GetCmn32kSrcFreq();
    case CRM_IpSrcAon32kClk:
        return CRM_GetRc32kSrcFreq();
    case CRM_IpSrcBBPLLCoreClk:
        return CRM_GetBBPLLCoreSrcFreq();
    default:
        return 0;
    }
}

/**********************************DEVICE************************************/
/**
 * @brief Sets the clock divider for Psram.
 *
 * This function configures the clock division for Psram by setting the divider ratios
 * to the specified values. The division is defined by  and 'div_m'.
 * These parameters determine how the input clock frequency is divided to derive the
 * desired Psram clock frequency.
 * * * @param div_m The denominator part of the clock division ratio. Specifies the lower part
 *              of the division ratio, The div_m select range from [1 - 31].
 * *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                 typically indicates what went wrong during the configuration process.
 *
 * @note The exact behavior and limitations of the division ratio depend on the specific
 *       hardware capabilities and clock configuration. Ensure that the values of  and 'div_m' *       are within the valid range for your hardware.
 *
 * @warning Improper configuration of the clock divider might disrupt UART0 communication.
 *          It's important to ensure that  and 'div_m' are set to values that are
 *          compatible with the Psram specifications and the overall system clock settings.
 */
int32_t HAL_CRM_SetPsramClkDiv(uint32_t div_m){
    if (div_m == 0 || div_m >  31){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
	IP_SYSCTRL->REG_PERI_CLK_CFG0.bit.DIV_PSRAM_CLK_LD = 0x0;
	IP_SYSCTRL->REG_PERI_CLK_CFG0.bit.DIV_PSRAM_CLK_M = div_m;
	// Avoid the compiler out-of-order
    MemBarrier();
    IP_SYSCTRL->REG_PERI_CLK_CFG0.bit.DIV_PSRAM_CLK_LD = 0x1;
    return CSK_DRIVER_OK;
}
/**
 * @brief Retrieves the current operating frequency of Psram.
 *
 * This function returns the frequency (in Hz) at which Psram is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Psram clock
 * divider settings. This value is important for setting up Psram communication parameters.
 *
 * @return uint32_t The operating frequency of Psram in Hertz. If the frequency cannot be determined
 *                  or if Psram is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Psram frequency.
 *
 * @warning This function can call anywhere which get the Psram clock frequency.
 */
uint32_t CRM_GetPsramFreq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    if (!HAL_CRM_PsramClkIsEnabled()){
        return 0;
    }
    uint32_t div_m;
    HAL_CRM_GetPsramClkConfig(&div_m);
    return  CRM_GetSrcFreq(CRM_IpSrcPsramClk) / div_m;
#endif
}/**
 * @brief Sets the clock divider for Flash.
 *
 * This function configures the clock division for Flash by setting the divider ratios
 * to the specified values. The division is defined by  and 'div_m'.
 * These parameters determine how the input clock frequency is divided to derive the
 * desired Flash clock frequency.
 * * * @param div_m The denominator part of the clock division ratio. Specifies the lower part
 *              of the division ratio, The div_m select range from [1 - 31].
 * *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                 typically indicates what went wrong during the configuration process.
 *
 * @note The exact behavior and limitations of the division ratio depend on the specific
 *       hardware capabilities and clock configuration. Ensure that the values of  and 'div_m' *       are within the valid range for your hardware.
 *
 * @warning Improper configuration of the clock divider might disrupt UART0 communication.
 *          It's important to ensure that  and 'div_m' are set to values that are
 *          compatible with the Flash specifications and the overall system clock settings.
 */
__attribute__ ((section (_TMP_RAM_SEC), optimize("O0"))) int32_t HAL_CRM_SetFlashClkDiv(uint32_t div_m){
    if (div_m == 0 || div_m >  32){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
	IP_SYSCTRL->REG_PERI_CLK_CFG0.bit.DIV_FLASH_CLK_LD = 0x0;
	IP_SYSCTRL->REG_PERI_CLK_CFG0.bit.DIV_FLASH_CLK_M = div_m;
	// Avoid the compiler out-of-order
    MemBarrier();
    IP_SYSCTRL->REG_PERI_CLK_CFG0.bit.DIV_FLASH_CLK_LD = 0x1;

    // Read some dummy words from Flash then invalidate the cache to fix the clock glitch issue
    volatile uint32_t flash_stash_fifo[6] = {0};
    for(uint8_t i = 0; i < 6; i++){
    	flash_stash_fifo[i] = *(uint32_t*)(0x30000000 + i*4);
    }
    MInvalICache();
    MInvalDCache();

    return CSK_DRIVER_OK;
}
/**
 * @brief Sets the clock source for Flash.
 *
 * This function configures Flash to use a specific clock source as defined by the 'src' parameter.
 * The 'src' parameter should be one of the values defined in the 'clock_src_name_t' enumeration,
 * representing the various clock sources available in the system. This allows for flexible
 * configuration of the Flash clocking, depending on the system's requirements and the available
 * clock sources.
 *
 * @param src The desired clock source for Flash. This should be a value from the
 *            'clock_src_name_t' enumeration that specifies which clock source to use that Can choose this source -> CRM_IpSrcXtalClk, CRM_IpSrcFlashClk.
 *
 * @return uint32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                  can indicate issues such as an invalid clock source or a failure in applying
 *                  the new clock source setting.
 *
 * @note The function's ability to change the clock source may depend on the current state of Flash
 *       and the system's clock configuration. It is advisable to ensure that Flash is not actively
 *       transmitting data when changing its clock source.
 *
 * @warning Using an incorrect or unsupported clock source for Flash can lead to communication
 *          failures or system instability. Ensure that the selected clock source is compatible
 *          with Flash's operational requirements.
 */
__RAMCODE__ uint32_t HAL_CRM_SetFlashClkSrc(clock_src_name_t src){
    if (src == CRM_IpSrcXtalClk){
        IP_SYSCTRL->REG_PERI_CLK_CFG0.bit.SEL_FLASH_CLK = 0;
    }
    if (src == CRM_IpSrcFlashClk){
        IP_SYSCTRL->REG_PERI_CLK_CFG0.bit.SEL_FLASH_CLK = 1;
    }
    return CSK_DRIVER_OK;
} 
/**
 * @brief Retrieves the current operating frequency of Flash.
 *
 * This function returns the frequency (in Hz) at which Flash is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Flash clock
 * divider settings. This value is important for setting up Flash communication parameters.
 *
 * @return uint32_t The operating frequency of Flash in Hertz. If the frequency cannot be determined
 *                  or if Flash is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Flash frequency.
 *
 * @warning This function can call anywhere which get the Flash clock frequency.
 */
uint32_t CRM_GetFlashFreq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    if (!HAL_CRM_FlashClkIsEnabled()){
        return 0;
    }
    clock_src_name_t src = CRM_IpSrcInvalide;
    uint32_t div_m;
    HAL_CRM_GetFlashClkConfig(&src, &div_m);
    return  CRM_GetSrcFreq(src) / div_m;
#endif
}/**
 * @brief Sets the clock divider for Mtime.
 *
 * This function configures the clock division for Mtime by setting the divider ratios
 * to the specified values. The division is defined by  and 'div_m'.
 * These parameters determine how the input clock frequency is divided to derive the
 * desired Mtime clock frequency.
 * * * @param div_m The denominator part of the clock division ratio. Specifies the lower part
 *              of the division ratio, The div_m select range from [1 - 63].
 * *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                 typically indicates what went wrong during the configuration process.
 *
 * @note The exact behavior and limitations of the division ratio depend on the specific
 *       hardware capabilities and clock configuration. Ensure that the values of  and 'div_m' *       are within the valid range for your hardware.
 *
 * @warning Improper configuration of the clock divider might disrupt UART0 communication.
 *          It's important to ensure that  and 'div_m' are set to values that are
 *          compatible with the Mtime specifications and the overall system clock settings.
 */
int32_t HAL_CRM_SetMtimeClkDiv(uint32_t div_m){
    if (div_m == 0 || div_m >  63){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
	IP_SYSCTRL->REG_PERI_CLK_CFG0.bit.MTIME_TOGGLE_LD = 0x0;
	IP_SYSCTRL->REG_PERI_CLK_CFG0.bit.DIV_MTIME_TOGGLE_M = div_m;
	// Avoid the compiler out-of-order
    MemBarrier();
    IP_SYSCTRL->REG_PERI_CLK_CFG0.bit.MTIME_TOGGLE_LD = 0x1;
    return CSK_DRIVER_OK;
}
/**
 * @brief Retrieves the current operating frequency of Mtime.
 *
 * This function returns the frequency (in Hz) at which Mtime is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Mtime clock
 * divider settings. This value is important for setting up Mtime communication parameters.
 *
 * @return uint32_t The operating frequency of Mtime in Hertz. If the frequency cannot be determined
 *                  or if Mtime is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Mtime frequency.
 *
 * @warning This function can call anywhere which get the Mtime clock frequency.
 */
uint32_t CRM_GetMtimeFreq(){
#if (IC_BOARD == 0)
    uint32_t div_m;
    HAL_CRM_GetMtimeClkConfig(&div_m);
    return IC_BOARD_FPGA_FIX_FREQ / div_m;
#else
    if (!HAL_CRM_MtimeClkIsEnabled()){
        return 0;
    }    uint32_t div_m;
    HAL_CRM_GetMtimeClkConfig(&div_m);
    return CRM_GetSrcFreq(CRM_IpSrcXtalClk) / div_m;
#endif
}/**
 * @brief Sets the clock divider for Spi0.
 *
 * This function configures the clock division for Spi0 by setting the divider ratios
 * to the specified values. The division is defined by 'div_n' and 'div_m'.
 * These parameters determine how the input clock frequency is divided to derive the
 * desired Spi0 clock frequency.
 * * @param div_n The numerator part of the clock division ratio. Specifies the upper part
 *              of the division ratio, The div_n select range from [1 - 7].
 * * * @param div_m The denominator part of the clock division ratio. Specifies the lower part
 *              of the division ratio, The div_m select range from [1 - 15].
 * *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                 typically indicates what went wrong during the configuration process.
 *
 * @note The exact behavior and limitations of the division ratio depend on the specific
 *       hardware capabilities and clock configuration. Ensure that the values of 'div_n' and 'div_m' *       are within the valid range for your hardware.
 *
 * @warning Improper configuration of the clock divider might disrupt UART0 communication.
 *          It's important to ensure that 'div_n' and 'div_m' are set to values that are
 *          compatible with the Spi0 specifications and the overall system clock settings.
 */
int32_t HAL_CRM_SetSpi0ClkDiv(uint32_t div_n, uint32_t div_m){
if (div_n == 0 || div_n >  7){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    if (div_m == 0 || div_m >  15){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
	IP_SYSCTRL->REG_PERI_CLK_CFG1.bit.DIV_SPI0_CLK_LD = 0x0;
	IP_SYSCTRL->REG_PERI_CLK_CFG1.bit.DIV_SPI0_CLK_N = div_n;
	IP_SYSCTRL->REG_PERI_CLK_CFG1.bit.DIV_SPI0_CLK_M = div_m;
	// Avoid the compiler out-of-order
    MemBarrier();
    IP_SYSCTRL->REG_PERI_CLK_CFG1.bit.DIV_SPI0_CLK_LD = 0x1;
    return CSK_DRIVER_OK;
}/**
 * @brief Sets the clock source for Spi0.
 *
 * This function configures Spi0 to use a specific clock source as defined by the 'src' parameter.
 * The 'src' parameter should be one of the values defined in the 'clock_src_name_t' enumeration,
 * representing the various clock sources available in the system. This allows for flexible
 * configuration of the Spi0 clocking, depending on the system's requirements and the available
 * clock sources.
 *
 * @param src The desired clock source for Spi0. This should be a value from the
 *            'clock_src_name_t' enumeration that specifies which clock source to use that Can choose this source -> CRM_IpSrcXtalClk, CRM_IpSrcPeriClk.
 *
 * @return uint32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                  can indicate issues such as an invalid clock source or a failure in applying
 *                  the new clock source setting.
 *
 * @note The function's ability to change the clock source may depend on the current state of Spi0
 *       and the system's clock configuration. It is advisable to ensure that Spi0 is not actively
 *       transmitting data when changing its clock source.
 *
 * @warning Using an incorrect or unsupported clock source for Spi0 can lead to communication
 *          failures or system instability. Ensure that the selected clock source is compatible
 *          with Spi0's operational requirements.
 */
uint32_t HAL_CRM_SetSpi0ClkSrc(clock_src_name_t src){
    if (src == CRM_IpSrcXtalClk){
        IP_SYSCTRL->REG_PERI_CLK_CFG1.bit.SEL_SPI0_CLK = 0;
    }
    if (src == CRM_IpSrcPeriClk){
        IP_SYSCTRL->REG_PERI_CLK_CFG1.bit.SEL_SPI0_CLK = 1;
    }
    return CSK_DRIVER_OK;
} 
/**
 * @brief Retrieves the current operating frequency of Spi0.
 *
 * This function returns the frequency (in Hz) at which Spi0 is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Spi0 clock
 * divider settings. This value is important for setting up Spi0 communication parameters.
 *
 * @return uint32_t The operating frequency of Spi0 in Hertz. If the frequency cannot be determined
 *                  or if Spi0 is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Spi0 frequency.
 *
 * @warning This function can call anywhere which get the Spi0 clock frequency.
 */
uint32_t CRM_GetSpi0Freq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    if (!HAL_CRM_Spi0ClkIsEnabled()){
        return 0;
    }    clock_src_name_t src = CRM_IpSrcInvalide;
    uint32_t div_n;
    uint32_t div_m;
    HAL_CRM_GetSpi0ClkConfig(&src, &div_n, &div_m);
    return  CRM_GetSrcFreq(src) * div_n / div_m;
#endif
}/**
 * @brief Sets the clock divider for Uart0.
 *
 * This function configures the clock division for Uart0 by setting the divider ratios
 * to the specified values. The division is defined by 'div_n' and 'div_m'.
 * These parameters determine how the input clock frequency is divided to derive the
 * desired Uart0 clock frequency.
 * * @param div_n The numerator part of the clock division ratio. Specifies the upper part
 *              of the division ratio, The div_n select range from [1 - 511].
 * * * @param div_m The denominator part of the clock division ratio. Specifies the lower part
 *              of the division ratio, The div_m select range from [1 - 1023].
 * *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                 typically indicates what went wrong during the configuration process.
 *
 * @note The exact behavior and limitations of the division ratio depend on the specific
 *       hardware capabilities and clock configuration. Ensure that the values of 'div_n' and 'div_m' *       are within the valid range for your hardware.
 *
 * @warning Improper configuration of the clock divider might disrupt UART0 communication.
 *          It's important to ensure that 'div_n' and 'div_m' are set to values that are
 *          compatible with the Uart0 specifications and the overall system clock settings.
 */
int32_t HAL_CRM_SetUart0ClkDiv(uint32_t div_n, uint32_t div_m){
if (div_n == 0 || div_n >  511){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    if (div_m == 0 || div_m >  1023){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
	IP_SYSCTRL->REG_PERI_CLK_CFG1.bit.DIV_UART0_CLK_LD = 0x0;
	IP_SYSCTRL->REG_PERI_CLK_CFG1.bit.DIV_UART0_CLK_N = div_n;
	IP_SYSCTRL->REG_PERI_CLK_CFG1.bit.DIV_UART0_CLK_M = div_m;
	// Avoid the compiler out-of-order
    MemBarrier();
    IP_SYSCTRL->REG_PERI_CLK_CFG1.bit.DIV_UART0_CLK_LD = 0x1;
    return CSK_DRIVER_OK;
}/**
 * @brief Sets the clock source for Uart0.
 *
 * This function configures Uart0 to use a specific clock source as defined by the 'src' parameter.
 * The 'src' parameter should be one of the values defined in the 'clock_src_name_t' enumeration,
 * representing the various clock sources available in the system. This allows for flexible
 * configuration of the Uart0 clocking, depending on the system's requirements and the available
 * clock sources.
 *
 * @param src The desired clock source for Uart0. This should be a value from the
 *            'clock_src_name_t' enumeration that specifies which clock source to use that Can choose this source -> CRM_IpSrcXtalClk, CRM_IpSrcPeriClk.
 *
 * @return uint32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                  can indicate issues such as an invalid clock source or a failure in applying
 *                  the new clock source setting.
 *
 * @note The function's ability to change the clock source may depend on the current state of Uart0
 *       and the system's clock configuration. It is advisable to ensure that Uart0 is not actively
 *       transmitting data when changing its clock source.
 *
 * @warning Using an incorrect or unsupported clock source for Uart0 can lead to communication
 *          failures or system instability. Ensure that the selected clock source is compatible
 *          with Uart0's operational requirements.
 */
uint32_t HAL_CRM_SetUart0ClkSrc(clock_src_name_t src){
    if (src == CRM_IpSrcXtalClk){
        IP_SYSCTRL->REG_PERI_CLK_CFG1.bit.SEL_UART0_CLK = 0;
    }
    if (src == CRM_IpSrcPeriClk){
        IP_SYSCTRL->REG_PERI_CLK_CFG1.bit.SEL_UART0_CLK = 1;
    }
    return CSK_DRIVER_OK;
} 
/**
 * @brief Retrieves the current operating frequency of Uart0.
 *
 * This function returns the frequency (in Hz) at which Uart0 is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Uart0 clock
 * divider settings. This value is important for setting up Uart0 communication parameters.
 *
 * @return uint32_t The operating frequency of Uart0 in Hertz. If the frequency cannot be determined
 *                  or if Uart0 is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Uart0 frequency.
 *
 * @warning This function can call anywhere which get the Uart0 clock frequency.
 */
uint32_t CRM_GetUart0Freq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    if (!HAL_CRM_Uart0ClkIsEnabled()){
        return 0;
    }    clock_src_name_t src = CRM_IpSrcInvalide;
    uint32_t div_n;
    uint32_t div_m;
    HAL_CRM_GetUart0ClkConfig(&src, &div_n, &div_m);
    return  CRM_GetSrcFreq(src) * div_n / div_m;
#endif
}/**
 * @brief Sets the clock divider for Spi1.
 *
 * This function configures the clock division for Spi1 by setting the divider ratios
 * to the specified values. The division is defined by 'div_n' and 'div_m'.
 * These parameters determine how the input clock frequency is divided to derive the
 * desired Spi1 clock frequency.
 * * @param div_n The numerator part of the clock division ratio. Specifies the upper part
 *              of the division ratio, The div_n select range from [1 - 7].
 * * * @param div_m The denominator part of the clock division ratio. Specifies the lower part
 *              of the division ratio, The div_m select range from [1 - 15].
 * *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                 typically indicates what went wrong during the configuration process.
 *
 * @note The exact behavior and limitations of the division ratio depend on the specific
 *       hardware capabilities and clock configuration. Ensure that the values of 'div_n' and 'div_m' *       are within the valid range for your hardware.
 *
 * @warning Improper configuration of the clock divider might disrupt UART0 communication.
 *          It's important to ensure that 'div_n' and 'div_m' are set to values that are
 *          compatible with the Spi1 specifications and the overall system clock settings.
 */
int32_t HAL_CRM_SetSpi1ClkDiv(uint32_t div_n, uint32_t div_m){
if (div_n == 0 || div_n >  7){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    if (div_m == 0 || div_m >  15){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
	IP_SYSCTRL->REG_PERI_CLK_CFG2.bit.DIV_SPI1_CLK_LD = 0x0;
	IP_SYSCTRL->REG_PERI_CLK_CFG2.bit.DIV_SPI1_CLK_N = div_n;
	IP_SYSCTRL->REG_PERI_CLK_CFG2.bit.DIV_SPI1_CLK_M = div_m;
	// Avoid the compiler out-of-order
    MemBarrier();
    IP_SYSCTRL->REG_PERI_CLK_CFG2.bit.DIV_SPI1_CLK_LD = 0x1;
    return CSK_DRIVER_OK;
}/**
 * @brief Sets the clock source for Spi1.
 *
 * This function configures Spi1 to use a specific clock source as defined by the 'src' parameter.
 * The 'src' parameter should be one of the values defined in the 'clock_src_name_t' enumeration,
 * representing the various clock sources available in the system. This allows for flexible
 * configuration of the Spi1 clocking, depending on the system's requirements and the available
 * clock sources.
 *
 * @param src The desired clock source for Spi1. This should be a value from the
 *            'clock_src_name_t' enumeration that specifies which clock source to use that Can choose this source -> CRM_IpSrcXtalClk, CRM_IpSrcPeriClk.
 *
 * @return uint32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                  can indicate issues such as an invalid clock source or a failure in applying
 *                  the new clock source setting.
 *
 * @note The function's ability to change the clock source may depend on the current state of Spi1
 *       and the system's clock configuration. It is advisable to ensure that Spi1 is not actively
 *       transmitting data when changing its clock source.
 *
 * @warning Using an incorrect or unsupported clock source for Spi1 can lead to communication
 *          failures or system instability. Ensure that the selected clock source is compatible
 *          with Spi1's operational requirements.
 */
uint32_t HAL_CRM_SetSpi1ClkSrc(clock_src_name_t src){
    if (src == CRM_IpSrcXtalClk){
        IP_SYSCTRL->REG_PERI_CLK_CFG2.bit.SEL_SPI1_CLK = 0;
    }
    if (src == CRM_IpSrcPeriClk){
        IP_SYSCTRL->REG_PERI_CLK_CFG2.bit.SEL_SPI1_CLK = 1;
    }
    return CSK_DRIVER_OK;
} 
/**
 * @brief Retrieves the current operating frequency of Spi1.
 *
 * This function returns the frequency (in Hz) at which Spi1 is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Spi1 clock
 * divider settings. This value is important for setting up Spi1 communication parameters.
 *
 * @return uint32_t The operating frequency of Spi1 in Hertz. If the frequency cannot be determined
 *                  or if Spi1 is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Spi1 frequency.
 *
 * @warning This function can call anywhere which get the Spi1 clock frequency.
 */
uint32_t CRM_GetSpi1Freq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    if (!HAL_CRM_Spi1ClkIsEnabled()){
        return 0;
    }    clock_src_name_t src = CRM_IpSrcInvalide;
    uint32_t div_n;
    uint32_t div_m;
    HAL_CRM_GetSpi1ClkConfig(&src, &div_n, &div_m);
    return  CRM_GetSrcFreq(src) * div_n / div_m;
#endif
}/**
 * @brief Sets the clock divider for Uart1.
 *
 * This function configures the clock division for Uart1 by setting the divider ratios
 * to the specified values. The division is defined by 'div_n' and 'div_m'.
 * These parameters determine how the input clock frequency is divided to derive the
 * desired Uart1 clock frequency.
 * * @param div_n The numerator part of the clock division ratio. Specifies the upper part
 *              of the division ratio, The div_n select range from [1 - 511].
 * * * @param div_m The denominator part of the clock division ratio. Specifies the lower part
 *              of the division ratio, The div_m select range from [1 - 1023].
 * *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                 typically indicates what went wrong during the configuration process.
 *
 * @note The exact behavior and limitations of the division ratio depend on the specific
 *       hardware capabilities and clock configuration. Ensure that the values of 'div_n' and 'div_m' *       are within the valid range for your hardware.
 *
 * @warning Improper configuration of the clock divider might disrupt UART0 communication.
 *          It's important to ensure that 'div_n' and 'div_m' are set to values that are
 *          compatible with the Uart1 specifications and the overall system clock settings.
 */
int32_t HAL_CRM_SetUart1ClkDiv(uint32_t div_n, uint32_t div_m){
if (div_n == 0 || div_n >  511){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    if (div_m == 0 || div_m >  1023){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
	IP_SYSCTRL->REG_PERI_CLK_CFG2.bit.DIV_UART1_CLK_LD = 0x0;
	IP_SYSCTRL->REG_PERI_CLK_CFG2.bit.DIV_UART1_CLK_N = div_n;
	IP_SYSCTRL->REG_PERI_CLK_CFG2.bit.DIV_UART1_CLK_M = div_m;
	// Avoid the compiler out-of-order
    MemBarrier();
    IP_SYSCTRL->REG_PERI_CLK_CFG2.bit.DIV_UART1_CLK_LD = 0x1;
    return CSK_DRIVER_OK;
}/**
 * @brief Sets the clock source for Uart1.
 *
 * This function configures Uart1 to use a specific clock source as defined by the 'src' parameter.
 * The 'src' parameter should be one of the values defined in the 'clock_src_name_t' enumeration,
 * representing the various clock sources available in the system. This allows for flexible
 * configuration of the Uart1 clocking, depending on the system's requirements and the available
 * clock sources.
 *
 * @param src The desired clock source for Uart1. This should be a value from the
 *            'clock_src_name_t' enumeration that specifies which clock source to use that Can choose this source -> CRM_IpSrcXtalClk, CRM_IpSrcPeriClk.
 *
 * @return uint32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                  can indicate issues such as an invalid clock source or a failure in applying
 *                  the new clock source setting.
 *
 * @note The function's ability to change the clock source may depend on the current state of Uart1
 *       and the system's clock configuration. It is advisable to ensure that Uart1 is not actively
 *       transmitting data when changing its clock source.
 *
 * @warning Using an incorrect or unsupported clock source for Uart1 can lead to communication
 *          failures or system instability. Ensure that the selected clock source is compatible
 *          with Uart1's operational requirements.
 */
uint32_t HAL_CRM_SetUart1ClkSrc(clock_src_name_t src){
    if (src == CRM_IpSrcXtalClk){
        IP_SYSCTRL->REG_PERI_CLK_CFG2.bit.SEL_UART1_CLK = 0;
    }
    if (src == CRM_IpSrcPeriClk){
        IP_SYSCTRL->REG_PERI_CLK_CFG2.bit.SEL_UART1_CLK = 1;
    }
    return CSK_DRIVER_OK;
} 
/**
 * @brief Retrieves the current operating frequency of Uart1.
 *
 * This function returns the frequency (in Hz) at which Uart1 is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Uart1 clock
 * divider settings. This value is important for setting up Uart1 communication parameters.
 *
 * @return uint32_t The operating frequency of Uart1 in Hertz. If the frequency cannot be determined
 *                  or if Uart1 is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Uart1 frequency.
 *
 * @warning This function can call anywhere which get the Uart1 clock frequency.
 */
uint32_t CRM_GetUart1Freq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    if (!HAL_CRM_Uart1ClkIsEnabled()){
        return 0;
    }
    clock_src_name_t src = CRM_IpSrcInvalide;
    uint32_t div_n;
    uint32_t div_m;
    HAL_CRM_GetUart1ClkConfig(&src, &div_n, &div_m);
    return  CRM_GetSrcFreq(src) * div_n / div_m;
#endif
}/**
 * @brief Sets the clock divider for Spi2.
 *
 * This function configures the clock division for Spi2 by setting the divider ratios
 * to the specified values. The division is defined by 'div_n' and 'div_m'.
 * These parameters determine how the input clock frequency is divided to derive the
 * desired Spi2 clock frequency.
 * * @param div_n The numerator part of the clock division ratio. Specifies the upper part
 *              of the division ratio, The div_n select range from [1 - 7].
 * * * @param div_m The denominator part of the clock division ratio. Specifies the lower part
 *              of the division ratio, The div_m select range from [1 - 15].
 * *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                 typically indicates what went wrong during the configuration process.
 *
 * @note The exact behavior and limitations of the division ratio depend on the specific
 *       hardware capabilities and clock configuration. Ensure that the values of 'div_n' and 'div_m' *       are within the valid range for your hardware.
 *
 * @warning Improper configuration of the clock divider might disrupt UART0 communication.
 *          It's important to ensure that 'div_n' and 'div_m' are set to values that are
 *          compatible with the Spi2 specifications and the overall system clock settings.
 */
int32_t HAL_CRM_SetSpi2ClkDiv(uint32_t div_n, uint32_t div_m){
if (div_n == 0 || div_n >  7){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    if (div_m == 0 || div_m >  15){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
	IP_SYSCTRL->REG_PERI_CLK_CFG3.bit.DIV_SPI2_CLK_LD = 0x0;
	IP_SYSCTRL->REG_PERI_CLK_CFG3.bit.DIV_SPI2_CLK_N = div_n;
	IP_SYSCTRL->REG_PERI_CLK_CFG3.bit.DIV_SPI2_CLK_M = div_m;
	// Avoid the compiler out-of-order
    MemBarrier();
    IP_SYSCTRL->REG_PERI_CLK_CFG3.bit.DIV_SPI2_CLK_LD = 0x1;
    return CSK_DRIVER_OK;
}/**
 * @brief Sets the clock source for Spi2.
 *
 * This function configures Spi2 to use a specific clock source as defined by the 'src' parameter.
 * The 'src' parameter should be one of the values defined in the 'clock_src_name_t' enumeration,
 * representing the various clock sources available in the system. This allows for flexible
 * configuration of the Spi2 clocking, depending on the system's requirements and the available
 * clock sources.
 *
 * @param src The desired clock source for Spi2. This should be a value from the
 *            'clock_src_name_t' enumeration that specifies which clock source to use that Can choose this source -> CRM_IpSrcXtalClk, CRM_IpSrcPeriClk.
 *
 * @return uint32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                  can indicate issues such as an invalid clock source or a failure in applying
 *                  the new clock source setting.
 *
 * @note The function's ability to change the clock source may depend on the current state of Spi2
 *       and the system's clock configuration. It is advisable to ensure that Spi2 is not actively
 *       transmitting data when changing its clock source.
 *
 * @warning Using an incorrect or unsupported clock source for Spi2 can lead to communication
 *          failures or system instability. Ensure that the selected clock source is compatible
 *          with Spi2's operational requirements.
 */
uint32_t HAL_CRM_SetSpi2ClkSrc(clock_src_name_t src){
    if (src == CRM_IpSrcXtalClk){
        IP_SYSCTRL->REG_PERI_CLK_CFG3.bit.SEL_SPI2_CLK = 0;
    }
    if (src == CRM_IpSrcPeriClk){
        IP_SYSCTRL->REG_PERI_CLK_CFG3.bit.SEL_SPI2_CLK = 1;
    }
    return CSK_DRIVER_OK;
} 
/**
 * @brief Retrieves the current operating frequency of Spi2.
 *
 * This function returns the frequency (in Hz) at which Spi2 is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Spi2 clock
 * divider settings. This value is important for setting up Spi2 communication parameters.
 *
 * @return uint32_t The operating frequency of Spi2 in Hertz. If the frequency cannot be determined
 *                  or if Spi2 is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Spi2 frequency.
 *
 * @warning This function can call anywhere which get the Spi2 clock frequency.
 */
uint32_t CRM_GetSpi2Freq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    if (!HAL_CRM_Spi2ClkIsEnabled()){
        return 0;
    }
    clock_src_name_t src = CRM_IpSrcInvalide;
    uint32_t div_n;
    uint32_t div_m;
    HAL_CRM_GetSpi2ClkConfig(&src, &div_n, &div_m);
    return  CRM_GetSrcFreq(src) * div_n / div_m;
#endif
}/**
 * @brief Sets the clock divider for Uart2.
 *
 * This function configures the clock division for Uart2 by setting the divider ratios
 * to the specified values. The division is defined by 'div_n' and 'div_m'.
 * These parameters determine how the input clock frequency is divided to derive the
 * desired Uart2 clock frequency.
 * * @param div_n The numerator part of the clock division ratio. Specifies the upper part
 *              of the division ratio, The div_n select range from [1 - 511].
 * * * @param div_m The denominator part of the clock division ratio. Specifies the lower part
 *              of the division ratio, The div_m select range from [1 - 1023].
 * *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                 typically indicates what went wrong during the configuration process.
 *
 * @note The exact behavior and limitations of the division ratio depend on the specific
 *       hardware capabilities and clock configuration. Ensure that the values of 'div_n' and 'div_m' *       are within the valid range for your hardware.
 *
 * @warning Improper configuration of the clock divider might disrupt UART0 communication.
 *          It's important to ensure that 'div_n' and 'div_m' are set to values that are
 *          compatible with the Uart2 specifications and the overall system clock settings.
 */
int32_t HAL_CRM_SetUart2ClkDiv(uint32_t div_n, uint32_t div_m){
if (div_n == 0 || div_n >  511){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    if (div_m == 0 || div_m >  1023){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
	IP_SYSCTRL->REG_PERI_CLK_CFG3.bit.DIV_UART2_CLK_LD = 0x0;
	IP_SYSCTRL->REG_PERI_CLK_CFG3.bit.DIV_UART2_CLK_N = div_n;
	IP_SYSCTRL->REG_PERI_CLK_CFG3.bit.DIV_UART2_CLK_M = div_m;
	// Avoid the compiler out-of-order
    MemBarrier();
    IP_SYSCTRL->REG_PERI_CLK_CFG3.bit.DIV_UART2_CLK_LD = 0x1;
    return CSK_DRIVER_OK;
}/**
 * @brief Sets the clock source for Uart2.
 *
 * This function configures Uart2 to use a specific clock source as defined by the 'src' parameter.
 * The 'src' parameter should be one of the values defined in the 'clock_src_name_t' enumeration,
 * representing the various clock sources available in the system. This allows for flexible
 * configuration of the Uart2 clocking, depending on the system's requirements and the available
 * clock sources.
 *
 * @param src The desired clock source for Uart2. This should be a value from the
 *            'clock_src_name_t' enumeration that specifies which clock source to use that Can choose this source -> CRM_IpSrcXtalClk, CRM_IpSrcPeriClk.
 *
 * @return uint32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                  can indicate issues such as an invalid clock source or a failure in applying
 *                  the new clock source setting.
 *
 * @note The function's ability to change the clock source may depend on the current state of Uart2
 *       and the system's clock configuration. It is advisable to ensure that Uart2 is not actively
 *       transmitting data when changing its clock source.
 *
 * @warning Using an incorrect or unsupported clock source for Uart2 can lead to communication
 *          failures or system instability. Ensure that the selected clock source is compatible
 *          with Uart2's operational requirements.
 */
uint32_t HAL_CRM_SetUart2ClkSrc(clock_src_name_t src){
    if (src == CRM_IpSrcXtalClk){
        IP_SYSCTRL->REG_PERI_CLK_CFG3.bit.SEL_UART2_CLK = 0;
    }
    if (src == CRM_IpSrcPeriClk){
        IP_SYSCTRL->REG_PERI_CLK_CFG3.bit.SEL_UART2_CLK = 1;
    }
    return CSK_DRIVER_OK;
} 
/**
 * @brief Retrieves the current operating frequency of Uart2.
 *
 * This function returns the frequency (in Hz) at which Uart2 is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Uart2 clock
 * divider settings. This value is important for setting up Uart2 communication parameters.
 *
 * @return uint32_t The operating frequency of Uart2 in Hertz. If the frequency cannot be determined
 *                  or if Uart2 is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Uart2 frequency.
 *
 * @warning This function can call anywhere which get the Uart2 clock frequency.
 */
uint32_t CRM_GetUart2Freq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    if (!HAL_CRM_Uart2ClkIsEnabled()){
        return 0;
    }
    clock_src_name_t src = CRM_IpSrcInvalide;
    uint32_t div_n;
    uint32_t div_m;
    HAL_CRM_GetUart2ClkConfig(&src, &div_n, &div_m);
    return  CRM_GetSrcFreq(src) * div_n / div_m;
#endif
}/**
 * @brief Sets the clock divider for Gpt_t0.
 *
 * This function configures the clock division for Gpt_t0 by setting the divider ratios
 * to the specified values. The division is defined by  and 'div_m'.
 * These parameters determine how the input clock frequency is divided to derive the
 * desired Gpt_t0 clock frequency.
 * * * @param div_m The denominator part of the clock division ratio. Specifies the lower part
 *              of the division ratio, The div_m select range from [1 - 15].
 * *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                 typically indicates what went wrong during the configuration process.
 *
 * @note The exact behavior and limitations of the division ratio depend on the specific
 *       hardware capabilities and clock configuration. Ensure that the values of  and 'div_m' *       are within the valid range for your hardware.
 *
 * @warning Improper configuration of the clock divider might disrupt UART0 communication.
 *          It's important to ensure that  and 'div_m' are set to values that are
 *          compatible with the Gpt_t0 specifications and the overall system clock settings.
 */
int32_t HAL_CRM_SetGpt_t0ClkDiv(uint32_t div_m){
    if (div_m == 0 || div_m >  15){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
	IP_SYSCTRL->REG_PERI_CLK_CFG4.bit.DIV_GPT_CLK_T0_LD = 0x0;
	IP_SYSCTRL->REG_PERI_CLK_CFG4.bit.DIV_GPT_CLK_T0_M = div_m;
	// Avoid the compiler out-of-order
    MemBarrier();
    IP_SYSCTRL->REG_PERI_CLK_CFG4.bit.DIV_GPT_CLK_T0_LD = 0x1;
    return CSK_DRIVER_OK;
}
/**
 * @brief Retrieves the current operating frequency of Gpt_t0.
 *
 * This function returns the frequency (in Hz) at which Gpt_t0 is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Gpt_t0 clock
 * divider settings. This value is important for setting up Gpt_t0 communication parameters.
 *
 * @return uint32_t The operating frequency of Gpt_t0 in Hertz. If the frequency cannot be determined
 *                  or if Gpt_t0 is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Gpt_t0 frequency.
 *
 * @warning This function can call anywhere which get the Gpt_t0 clock frequency.
 */
uint32_t CRM_GetGpt_t0Freq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    if (!HAL_CRM_Gpt_t0ClkIsEnabled()){
        return 0;
    }
    uint32_t div_m;
    HAL_CRM_GetGpt_t0ClkConfig(&div_m);
    return  CRM_GetSrcFreq(CRM_IpSrcXtalClk) / div_m;
#endif
}/**
 * @brief Sets the clock divider for Gpt_s.
 *
 * This function configures the clock division for Gpt_s by setting the divider ratios
 * to the specified values. The division is defined by  and 'div_m'.
 * These parameters determine how the input clock frequency is divided to derive the
 * desired Gpt_s clock frequency.
 * * * @param div_m The denominator part of the clock division ratio. Specifies the lower part
 *              of the division ratio, The div_m select range from [1 - 15].
 * *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                 typically indicates what went wrong during the configuration process.
 *
 * @note The exact behavior and limitations of the division ratio depend on the specific
 *       hardware capabilities and clock configuration. Ensure that the values of  and 'div_m' *       are within the valid range for your hardware.
 *
 * @warning Improper configuration of the clock divider might disrupt UART0 communication.
 *          It's important to ensure that  and 'div_m' are set to values that are
 *          compatible with the Gpt_s specifications and the overall system clock settings.
 */
int32_t HAL_CRM_SetGpt_sClkDiv(uint32_t div_m){
    if (div_m == 0 || div_m >  15){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
	IP_SYSCTRL->REG_PERI_CLK_CFG4.bit.DIV_GPT_CLK_S_LD = 0x0;
	IP_SYSCTRL->REG_PERI_CLK_CFG4.bit.DIV_GPT_CLK_S_M = div_m;
	// Avoid the compiler out-of-order
    MemBarrier();
    IP_SYSCTRL->REG_PERI_CLK_CFG4.bit.DIV_GPT_CLK_S_LD = 0x1;
    return CSK_DRIVER_OK;
}
/**
 * @brief Retrieves the current operating frequency of Gpt_s.
 *
 * This function returns the frequency (in Hz) at which Gpt_s is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Gpt_s clock
 * divider settings. This value is important for setting up Gpt_s communication parameters.
 *
 * @return uint32_t The operating frequency of Gpt_s in Hertz. If the frequency cannot be determined
 *                  or if Gpt_s is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Gpt_s frequency.
 *
 * @warning This function can call anywhere which get the Gpt_s clock frequency.
 */
uint32_t CRM_GetGpt_sFreq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    if (!HAL_CRM_Gpt_sClkIsEnabled()){
        return 0;
    }
    uint32_t div_m;
    HAL_CRM_GetGpt_sClkConfig(&div_m);
    return  CRM_GetSrcFreq(CRM_IpSrcXtalClk) / div_m;
#endif
}/**
 * @brief Sets the clock divider for Gpadc.
 *
 * This function configures the clock division for Gpadc by setting the divider ratios
 * to the specified values. The division is defined by  and 'div_m'.
 * These parameters determine how the input clock frequency is divided to derive the
 * desired Gpadc clock frequency.
 * * * @param div_m The denominator part of the clock division ratio. Specifies the lower part
 *              of the division ratio, The div_m select range from [1 - 1023].
 * *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                 typically indicates what went wrong during the configuration process.
 *
 * @note The exact behavior and limitations of the division ratio depend on the specific
 *       hardware capabilities and clock configuration. Ensure that the values of  and 'div_m' *       are within the valid range for your hardware.
 *
 * @warning Improper configuration of the clock divider might disrupt UART0 communication.
 *          It's important to ensure that  and 'div_m' are set to values that are
 *          compatible with the Gpadc specifications and the overall system clock settings.
 */
int32_t HAL_CRM_SetGpadcClkDiv(uint32_t div_m){
    if (div_m == 0 || div_m >  1023){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
	IP_SYSCTRL->REG_PERI_CLK_CFG5.bit.DIV_GPADC_CLK_LD = 0x0;
	IP_SYSCTRL->REG_PERI_CLK_CFG5.bit.DIV_GPADC_CLK_M = div_m;
	// Avoid the compiler out-of-order
    MemBarrier();
    IP_SYSCTRL->REG_PERI_CLK_CFG5.bit.DIV_GPADC_CLK_LD = 0x1;
    return CSK_DRIVER_OK;
}
/**
 * @brief Retrieves the current operating frequency of Gpadc.
 *
 * This function returns the frequency (in Hz) at which Gpadc is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Gpadc clock
 * divider settings. This value is important for setting up Gpadc communication parameters.
 *
 * @return uint32_t The operating frequency of Gpadc in Hertz. If the frequency cannot be determined
 *                  or if Gpadc is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Gpadc frequency.
 *
 * @warning This function can call anywhere which get the Gpadc clock frequency.
 */
uint32_t CRM_GetGpadcFreq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    if (!HAL_CRM_GpadcClkIsEnabled()){
        return 0;
    }
    uint32_t div_m;
    HAL_CRM_GetGpadcClkConfig(&div_m);
    return  CRM_GetSrcFreq(CRM_IpSrcXtalClk) / div_m;
#endif
}/**
 * @brief Sets the clock divider for Ir_tx.
 *
 * This function configures the clock division for Ir_tx by setting the divider ratios
 * to the specified values. The division is defined by  and 'div_m'.
 * These parameters determine how the input clock frequency is divided to derive the
 * desired Ir_tx clock frequency.
 * * * @param div_m The denominator part of the clock division ratio. Specifies the lower part
 *              of the division ratio, The div_m select range from [1 - 63].
 * *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                 typically indicates what went wrong during the configuration process.
 *
 * @note The exact behavior and limitations of the division ratio depend on the specific
 *       hardware capabilities and clock configuration. Ensure that the values of  and 'div_m' *       are within the valid range for your hardware.
 *
 * @warning Improper configuration of the clock divider might disrupt UART0 communication.
 *          It's important to ensure that  and 'div_m' are set to values that are
 *          compatible with the Ir_tx specifications and the overall system clock settings.
 */
int32_t HAL_CRM_SetIr_txClkDiv(uint32_t div_m){
    if (div_m == 0 || div_m >  63){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
	IP_SYSCTRL->REG_PERI_CLK_CFG5.bit.DIV_IR_CLK_TX_LD = 0x0;
	IP_SYSCTRL->REG_PERI_CLK_CFG5.bit.DIV_IR_CLK_TX_M = div_m;
	// Avoid the compiler out-of-order
    MemBarrier();
    IP_SYSCTRL->REG_PERI_CLK_CFG5.bit.DIV_IR_CLK_TX_LD = 0x1;
    return CSK_DRIVER_OK;
}
/**
 * @brief Retrieves the current operating frequency of Ir_tx.
 *
 * This function returns the frequency (in Hz) at which Ir_tx is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Ir_tx clock
 * divider settings. This value is important for setting up Ir_tx communication parameters.
 *
 * @return uint32_t The operating frequency of Ir_tx in Hertz. If the frequency cannot be determined
 *                  or if Ir_tx is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Ir_tx frequency.
 *
 * @warning This function can call anywhere which get the Ir_tx clock frequency.
 */
uint32_t CRM_GetIr_txFreq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    uint32_t div_m;
    HAL_CRM_GetIr_txClkConfig(&div_m);
    return  CRM_GetSrcFreq(CRM_IpSrcXtalClk) / div_m;
#endif
}
/**
 * @brief Retrieves the current operating frequency of Ir.
 *
 * This function returns the frequency (in Hz) at which Ir is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Ir clock
 * divider settings. This value is important for setting up Ir communication parameters.
 *
 * @return uint32_t The operating frequency of Ir in Hertz. If the frequency cannot be determined
 *                  or if Ir is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Ir frequency.
 *
 * @warning This function can call anywhere which get the Ir clock frequency.
 */
uint32_t CRM_GetIrFreq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    if (!HAL_CRM_IrClkIsEnabled()){
        return 0;
    }
    return  CRM_GetSrcFreq(CRM_IpSrcXtalClk) / 750;
#endif
}
/**
 * @brief Retrieves the current operating frequency of Dma.
 *
 * This function returns the frequency (in Hz) at which Dma is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Dma clock
 * divider settings. This value is important for setting up Dma communication parameters.
 *
 * @return uint32_t The operating frequency of Dma in Hertz. If the frequency cannot be determined
 *                  or if Dma is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Dma frequency.
 *
 * @warning This function can call anywhere which get the Dma clock frequency.
 */
uint32_t CRM_GetDmaFreq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    if (!HAL_CRM_DmaClkIsEnabled()){
        return 0;
    }
    return  CRM_GetHclkFreq();
#endif
}
/**
 * @brief Retrieves the current operating frequency of Gpio0.
 *
 * This function returns the frequency (in Hz) at which Gpio0 is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Gpio0 clock
 * divider settings. This value is important for setting up Gpio0 communication parameters.
 *
 * @return uint32_t The operating frequency of Gpio0 in Hertz. If the frequency cannot be determined
 *                  or if Gpio0 is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Gpio0 frequency.
 *
 * @warning This function can call anywhere which get the Gpio0 clock frequency.
 */
uint32_t CRM_GetGpio0Freq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    if (!HAL_CRM_Gpio0ClkIsEnabled()){
        return 0;
    }
    return  CRM_GetSrcFreq(CRM_IpSrcXtalClk);
#endif
}
/**
 * @brief Retrieves the current operating frequency of Gpio1.
 *
 * This function returns the frequency (in Hz) at which Gpio1 is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Gpio1 clock
 * divider settings. This value is important for setting up Gpio1 communication parameters.
 *
 * @return uint32_t The operating frequency of Gpio1 in Hertz. If the frequency cannot be determined
 *                  or if Gpio1 is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Gpio1 frequency.
 *
 * @warning This function can call anywhere which get the Gpio1 clock frequency.
 */
uint32_t CRM_GetGpio1Freq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    if (!HAL_CRM_Gpio1ClkIsEnabled()){
        return 0;
    }
    return  CRM_GetSrcFreq(CRM_IpSrcXtalClk);
#endif
}
/**
 * @brief Retrieves the current operating frequency of I2c0.
 *
 * This function returns the frequency (in Hz) at which I2c0 is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the I2c0 clock
 * divider settings. This value is important for setting up I2c0 communication parameters.
 *
 * @return uint32_t The operating frequency of I2c0 in Hertz. If the frequency cannot be determined
 *                  or if I2c0 is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the I2c0 frequency.
 *
 * @warning This function can call anywhere which get the I2c0 clock frequency.
 */
uint32_t CRM_GetI2c0Freq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    if (!HAL_CRM_I2c0ClkIsEnabled()){
        return 0;
    }
    return  CRM_GetSrcFreq(CRM_IpSrcXtalClk);
#endif
}
/**
 * @brief Retrieves the current operating frequency of I2c1.
 *
 * This function returns the frequency (in Hz) at which I2c1 is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the I2c1 clock
 * divider settings. This value is important for setting up I2c1 communication parameters.
 *
 * @return uint32_t The operating frequency of I2c1 in Hertz. If the frequency cannot be determined
 *                  or if I2c1 is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the I2c1 frequency.
 *
 * @warning This function can call anywhere which get the I2c1 clock frequency.
 */
uint32_t CRM_GetI2c1Freq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    if (!HAL_CRM_I2c1ClkIsEnabled()){
        return 0;
    }
    return  CRM_GetSrcFreq(CRM_IpSrcXtalClk);
#endif
}
/**
 * @brief Retrieves the current operating frequency of Qdec.
 *
 * This function returns the frequency (in Hz) at which Qdec is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Qdec clock
 * divider settings. This value is important for setting up Qdec communication parameters.
 *
 * @return uint32_t The operating frequency of Qdec in Hertz. If the frequency cannot be determined
 *                  or if Qdec is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Qdec frequency.
 *
 * @warning This function can call anywhere which get the Qdec clock frequency.
 */
uint32_t CRM_GetQdecFreq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    if (!HAL_CRM_QdecClkIsEnabled()){
        return 0;
    }
    return  CRM_GetSrcFreq(CRM_IpSrcXtalClk);
#endif
}
/**
 * @brief Retrieves the current operating frequency of Smid.
 *
 * This function returns the frequency (in Hz) at which Smid is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Smid clock
 * divider settings. This value is important for setting up Smid communication parameters.
 *
 * @return uint32_t The operating frequency of Smid in Hertz. If the frequency cannot be determined
 *                  or if Smid is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Smid frequency.
 *
 * @warning This function can call anywhere which get the Smid clock frequency.
 */
uint32_t CRM_GetSmidFreq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    if (!HAL_CRM_SmidClkIsEnabled()){
        return 0;
    }
    return  CRM_GetSrcFreq(CRM_IpSrcXtalClk);
#endif
}
/**
 * @brief Retrieves the current operating frequency of Rfif.
 *
 * This function returns the frequency (in Hz) at which Rfif is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Rfif clock
 * divider settings. This value is important for setting up Rfif communication parameters.
 *
 * @return uint32_t The operating frequency of Rfif in Hertz. If the frequency cannot be determined
 *                  or if Rfif is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Rfif frequency.
 *
 * @warning This function can call anywhere which get the Rfif clock frequency.
 */
uint32_t CRM_GetRfifFreq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    if (!HAL_CRM_RfifClkIsEnabled()){
        return 0;
    }
    return  CRM_GetSrcFreq(CRM_IpSrcXtalClk);
#endif
}
/**
 * @brief Retrieves the current operating frequency of Trng.
 *
 * This function returns the frequency (in Hz) at which Trng is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Trng clock
 * divider settings. This value is important for setting up Trng communication parameters.
 *
 * @return uint32_t The operating frequency of Trng in Hertz. If the frequency cannot be determined
 *                  or if Trng is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Trng frequency.
 *
 * @warning This function can call anywhere which get the Trng clock frequency.
 */
uint32_t CRM_GetTrngFreq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    if (!HAL_CRM_TrngClkIsEnabled()){
        return 0;
    }
    return  CRM_GetSrcFreq(CRM_IpSrcXtalClk);
#endif
}
/**
 * @brief Retrieves the current operating frequency of Calendar.
 *
 * This function returns the frequency (in Hz) at which Calendar is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Calendar clock
 * divider settings. This value is important for setting up Calendar communication parameters.
 *
 * @return uint32_t The operating frequency of Calendar in Hertz. If the frequency cannot be determined
 *                  or if Calendar is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Calendar frequency.
 *
 * @warning This function can call anywhere which get the Calendar clock frequency.
 */
uint32_t CRM_GetCalendarFreq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    if (!HAL_CRM_CalendarClkIsEnabled()){
        return 0;
    }
    return  CRM_GetSrcFreq(CRM_IpSrcXtalClk);
#endif
}
/**
 * @brief Retrieves the current operating frequency of Usb.
 *
 * This function returns the frequency (in Hz) at which Usb is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Usb clock
 * divider settings. This value is important for setting up Usb communication parameters.
 *
 * @return uint32_t The operating frequency of Usb in Hertz. If the frequency cannot be determined
 *                  or if Usb is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Usb frequency.
 *
 * @warning This function can call anywhere which get the Usb clock frequency.
 */
uint32_t CRM_GetUsbFreq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    if (!HAL_CRM_UsbClkIsEnabled()){
        return 0;
    }
    return  CRM_GetSrcFreq(CRM_IpSrcXtalClk);
#endif
}
/**
 * @brief Retrieves the current operating frequency of Bt.
 *
 * This function returns the frequency (in Hz) at which Bt is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Bt clock
 * divider settings. This value is important for setting up Bt communication parameters.
 *
 * @return uint32_t The operating frequency of Bt in Hertz. If the frequency cannot be determined
 *                  or if Bt is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Bt frequency.
 *
 * @warning This function can call anywhere which get the Bt clock frequency.
 */
uint32_t CRM_GetBtFreq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    if (!HAL_CRM_BtClkIsEnabled()){
        return 0;
    }
    return  CRM_GetHclkFreq();
#endif
}
/**
 * @brief Retrieves the current operating frequency of Wifi.
 *
 * This function returns the frequency (in Hz) at which Wifi is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Wifi clock
 * divider settings. This value is important for setting up Wifi communication parameters.
 *
 * @return uint32_t The operating frequency of Wifi in Hertz. If the frequency cannot be determined
 *                  or if Wifi is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Wifi frequency.
 *
 * @warning This function can call anywhere which get the Wifi clock frequency.
 */
uint32_t CRM_GetWifiFreq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    if (!HAL_CRM_WifiClkIsEnabled()){
        return 0;
    }
    return  CRM_GetHclkFreq();
#endif
}
/**
 * @brief Retrieves the current operating frequency of Crypto.
 *
 * This function returns the frequency (in Hz) at which Crypto is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Crypto clock
 * divider settings. This value is important for setting up Crypto communication parameters.
 *
 * @return uint32_t The operating frequency of Crypto in Hertz. If the frequency cannot be determined
 *                  or if Crypto is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Crypto frequency.
 *
 * @warning This function can call anywhere which get the Crypto clock frequency.
 */
uint32_t CRM_GetCryptoFreq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    if (!HAL_CRM_CryptoClkIsEnabled()){
        return 0;
    }
    return  CRM_GetHclkFreq();
#endif
}
/**
 * @brief Retrieves the current operating frequency of Jpeg.
 *
 * This function returns the frequency (in Hz) at which Jpeg is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Jpeg clock
 * divider settings. This value is important for setting up Jpeg communication parameters.
 *
 * @return uint32_t The operating frequency of Jpeg in Hertz. If the frequency cannot be determined
 *                  or if Jpeg is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Jpeg frequency.
 *
 * @warning This function can call anywhere which get the Jpeg clock frequency.
 */
uint32_t CRM_GetJpegFreq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    if (!HAL_CRM_JpegClkIsEnabled()){
        return 0;
    }
    return  CRM_GetHclkFreq();
#endif
}
/**
 * @brief Retrieves the current operating frequency of Gpdma.
 *
 * This function returns the frequency (in Hz) at which Gpdma is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Gpdma clock
 * divider settings. This value is important for setting up Gpdma communication parameters.
 *
 * @return uint32_t The operating frequency of Gpdma in Hertz. If the frequency cannot be determined
 *                  or if Gpdma is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Gpdma frequency.
 *
 * @warning This function can call anywhere which get the Gpdma clock frequency.
 */
uint32_t CRM_GetGpdmaFreq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    if (!HAL_CRM_GpdmaClkIsEnabled()){
        return 0;
    }
    return  CRM_GetHclkFreq();
#endif
}/**
 * @brief Sets the clock divider for Rgb.
 *
 * This function configures the clock division for Rgb by setting the divider ratios
 * to the specified values. The division is defined by  and 'div_m'.
 * These parameters determine how the input clock frequency is divided to derive the
 * desired Rgb clock frequency.
 * * * @param div_m The denominator part of the clock division ratio. Specifies the lower part
 *              of the division ratio, The div_m select range from [1 - 7].
 * *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                 typically indicates what went wrong during the configuration process.
 *
 * @note The exact behavior and limitations of the division ratio depend on the specific
 *       hardware capabilities and clock configuration. Ensure that the values of  and 'div_m' *       are within the valid range for your hardware.
 *
 * @warning Improper configuration of the clock divider might disrupt UART0 communication.
 *          It's important to ensure that  and 'div_m' are set to values that are
 *          compatible with the Rgb specifications and the overall system clock settings.
 */
int32_t HAL_CRM_SetRgbClkDiv(uint32_t div_m){
    if (div_m == 0 || div_m >  7){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
	IP_AP_CFG->REG_CLK_CFG0.bit.DIV_RGB_CLK_LD = 0x0;
	IP_AP_CFG->REG_CLK_CFG0.bit.DIV_RGB_CLK_M = div_m;
	// Avoid the compiler out-of-order
    MemBarrier();
    IP_AP_CFG->REG_CLK_CFG0.bit.DIV_RGB_CLK_LD = 0x1;
    return CSK_DRIVER_OK;
}/**
 * @brief Sets the clock source for Rgb.
 *
 * This function configures Rgb to use a specific clock source as defined by the 'src' parameter.
 * The 'src' parameter should be one of the values defined in the 'clock_src_name_t' enumeration,
 * representing the various clock sources available in the system. This allows for flexible
 * configuration of the Rgb clocking, depending on the system's requirements and the available
 * clock sources.
 *
 * @param src The desired clock source for Rgb. This should be a value from the
 *            'clock_src_name_t' enumeration that specifies which clock source to use that Can choose this source -> CRM_IpSrcXtalClk, CRM_IpSrcPeriClk.
 *
 * @return uint32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                  can indicate issues such as an invalid clock source or a failure in applying
 *                  the new clock source setting.
 *
 * @note The function's ability to change the clock source may depend on the current state of Rgb
 *       and the system's clock configuration. It is advisable to ensure that Rgb is not actively
 *       transmitting data when changing its clock source.
 *
 * @warning Using an incorrect or unsupported clock source for Rgb can lead to communication
 *          failures or system instability. Ensure that the selected clock source is compatible
 *          with Rgb's operational requirements.
 */
uint32_t HAL_CRM_SetRgbClkSrc(clock_src_name_t src){
    if (src == CRM_IpSrcXtalClk){
        IP_AP_CFG->REG_CLK_CFG0.bit.SEL_RGB_CLK = 0;
    }
    if (src == CRM_IpSrcPeriClk){
        IP_AP_CFG->REG_CLK_CFG0.bit.SEL_RGB_CLK = 1;
    }
    return CSK_DRIVER_OK;
} 
/**
 * @brief Retrieves the current operating frequency of Rgb.
 *
 * This function returns the frequency (in Hz) at which Rgb is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Rgb clock
 * divider settings. This value is important for setting up Rgb communication parameters.
 *
 * @return uint32_t The operating frequency of Rgb in Hertz. If the frequency cannot be determined
 *                  or if Rgb is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Rgb frequency.
 *
 * @warning This function can call anywhere which get the Rgb clock frequency.
 */
uint32_t CRM_GetRgbFreq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    if (!HAL_CRM_RgbClkIsEnabled()){
        return 0;
    }
    clock_src_name_t src = CRM_IpSrcInvalide;
    uint32_t div_m;
    HAL_CRM_GetRgbClkConfig(&src, &div_m);
    return  CRM_GetSrcFreq(src) / div_m;
#endif
}
/**
 * @brief Retrieves the current operating frequency of Blender.
 *
 * This function returns the frequency (in Hz) at which Blender is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Blender clock
 * divider settings. This value is important for setting up Blender communication parameters.
 *
 * @return uint32_t The operating frequency of Blender in Hertz. If the frequency cannot be determined
 *                  or if Blender is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Blender frequency.
 *
 * @warning This function can call anywhere which get the Blender clock frequency.
 */
uint32_t CRM_GetBlenderFreq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    return  CRM_GetHclkFreq();
#endif
}
/**
 * @brief Retrieves the current operating frequency of Sdio_d.
 *
 * This function returns the frequency (in Hz) at which Sdio_d is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Sdio_d clock
 * divider settings. This value is important for setting up Sdio_d communication parameters.
 *
 * @return uint32_t The operating frequency of Sdio_d in Hertz. If the frequency cannot be determined
 *                  or if Sdio_d is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Sdio_d frequency.
 *
 * @warning This function can call anywhere which get the Sdio_d clock frequency.
 */
uint32_t CRM_GetSdio_dFreq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    return  CRM_GetHclkFreq();
#endif
}/**
 * @brief Sets the clock divider for Sdio_h.
 *
 * This function configures the clock division for Sdio_h by setting the divider ratios
 * to the specified values. The division is defined by 'div_n' and 'div_m'.
 * These parameters determine how the input clock frequency is divided to derive the
 * desired Sdio_h clock frequency.
 * * @param div_n The numerator part of the clock division ratio. Specifies the upper part
 *              of the division ratio, The div_n select range from [1 - 7].
 * * * @param div_m The denominator part of the clock division ratio. Specifies the lower part
 *              of the division ratio, The div_m select range from [1 - 15].
 * *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                 typically indicates what went wrong during the configuration process.
 *
 * @note The exact behavior and limitations of the division ratio depend on the specific
 *       hardware capabilities and clock configuration. Ensure that the values of 'div_n' and 'div_m' *       are within the valid range for your hardware.
 *
 * @warning Improper configuration of the clock divider might disrupt UART0 communication.
 *          It's important to ensure that 'div_n' and 'div_m' are set to values that are
 *          compatible with the Sdio_h specifications and the overall system clock settings.
 */
int32_t HAL_CRM_SetSdio_hClkDiv(uint32_t div_n, uint32_t div_m){
if (div_n == 0 || div_n >  7){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    if (div_m == 0 || div_m >  15){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
	IP_AP_CFG->REG_CLK_CFG1.bit.DIV_SDIOH_CLK2X_LD = 0x0;
	IP_AP_CFG->REG_CLK_CFG1.bit.DIV_SDIOH_CLK2X_N = div_n;
	IP_AP_CFG->REG_CLK_CFG1.bit.DIV_SDIOH_CLK2X_M = div_m;
	// Avoid the compiler out-of-order
    MemBarrier();
    IP_AP_CFG->REG_CLK_CFG1.bit.DIV_SDIOH_CLK2X_LD = 0x1;
    return CSK_DRIVER_OK;
}/**
 * @brief Sets the clock source for Sdio_h.
 *
 * This function configures Sdio_h to use a specific clock source as defined by the 'src' parameter.
 * The 'src' parameter should be one of the values defined in the 'clock_src_name_t' enumeration,
 * representing the various clock sources available in the system. This allows for flexible
 * configuration of the Sdio_h clocking, depending on the system's requirements and the available
 * clock sources.
 *
 * @param src The desired clock source for Sdio_h. This should be a value from the
 *            'clock_src_name_t' enumeration that specifies which clock source to use that Can choose this source -> CRM_IpSrcXtalClk, CRM_IpSrcPeriClk.
 *
 * @return uint32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                  can indicate issues such as an invalid clock source or a failure in applying
 *                  the new clock source setting.
 *
 * @note The function's ability to change the clock source may depend on the current state of Sdio_h
 *       and the system's clock configuration. It is advisable to ensure that Sdio_h is not actively
 *       transmitting data when changing its clock source.
 *
 * @warning Using an incorrect or unsupported clock source for Sdio_h can lead to communication
 *          failures or system instability. Ensure that the selected clock source is compatible
 *          with Sdio_h's operational requirements.
 */
uint32_t HAL_CRM_SetSdio_hClkSrc(clock_src_name_t src){
    if (src == CRM_IpSrcXtalClk){
        IP_AP_CFG->REG_CLK_CFG1.bit.SEL_SDIOH_CLK2X = 0;
    }
    if (src == CRM_IpSrcFlashClk){
        IP_AP_CFG->REG_CLK_CFG1.bit.SEL_SDIOH_CLK2X = 1;
    }
    return CSK_DRIVER_OK;
} 
/**
 * @brief Retrieves the current operating frequency of Sdio_h.
 *
 * This function returns the frequency (in Hz) at which Sdio_h is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Sdio_h clock
 * divider settings. This value is important for setting up Sdio_h communication parameters.
 *
 * @return uint32_t The operating frequency of Sdio_h in Hertz. If the frequency cannot be determined
 *                  or if Sdio_h is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Sdio_h frequency.
 *
 * @warning This function can call anywhere which get the Sdio_h clock frequency.
 */
uint32_t CRM_GetSdio_hFreq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    if (!HAL_CRM_Sdio_hClkIsEnabled()){
        return 0;
    }
    clock_src_name_t src = CRM_IpSrcInvalide;
    uint32_t div_n;
    uint32_t div_m;
    HAL_CRM_GetSdio_hClkConfig(&src, &div_n, &div_m);
    return  CRM_GetSrcFreq(src) * div_n / div_m;
#endif
}
/**
 * @brief Retrieves the current operating frequency of Wdt.
 *
 * This function returns the frequency (in Hz) at which Wdt is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Wdt clock
 * divider settings. This value is important for setting up Wdt communication parameters.
 *
 * @return uint32_t The operating frequency of Wdt in Hertz. If the frequency cannot be determined
 *                  or if Wdt is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Wdt frequency.
 *
 * @warning This function can call anywhere which get the Wdt clock frequency.
 */
uint32_t CRM_GetWdtFreq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    return  CRM_GetCmn32kSrcFreq();
#endif
}
/**
 * @brief Retrieves the current operating frequency of Apc.
 *
 * This function returns the frequency (in Hz) at which Apc is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Apc clock
 * divider settings. This value is important for setting up Apc communication parameters.
 *
 * @return uint32_t The operating frequency of Apc in Hertz. If the frequency cannot be determined
 *                  or if Apc is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Apc frequency.
 *
 * @warning This function can call anywhere which get the Apc clock frequency.
 */
uint32_t CRM_GetApcFreq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    if (!HAL_CRM_ApcClkIsEnabled()){
        return 0;
    }
    return  CRM_GetHclkFreq();
#endif
}
/**
 * @brief Retrieves the current operating frequency of I2s.
 *
 * This function returns the frequency (in Hz) at which I2s is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the I2s clock
 * divider settings. This value is important for setting up I2s communication parameters.
 *
 * @return uint32_t The operating frequency of I2s in Hertz. If the frequency cannot be determined
 *                  or if I2s is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the I2s frequency.
 *
 * @warning This function can call anywhere which get the I2s clock frequency.
 */
uint32_t CRM_GetI2sFreq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    if (!HAL_CRM_I2sClkIsEnabled()){
        return 0;
    }
    return  CRM_GetHclkFreq();
#endif
}
/**
 * @brief Retrieves the current operating frequency of Dac.
 *
 * This function returns the frequency (in Hz) at which Dac is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Dac clock
 * divider settings. This value is important for setting up Dac communication parameters.
 *
 * @return uint32_t The operating frequency of Dac in Hertz. If the frequency cannot be determined
 *                  or if Dac is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Dac frequency.
 *
 * @warning This function can call anywhere which get the Dac clock frequency.
 */
uint32_t CRM_GetDacFreq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    if (!HAL_CRM_DacClkIsEnabled()){
        return 0;
    }
    return  CRM_GetHclkFreq();
#endif
}
/**
 * @brief Retrieves the current operating frequency of Adc.
 *
 * This function returns the frequency (in Hz) at which Adc is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Adc clock
 * divider settings. This value is important for setting up Adc communication parameters.
 *
 * @return uint32_t The operating frequency of Adc in Hertz. If the frequency cannot be determined
 *                  or if Adc is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Adc frequency.
 *
 * @warning This function can call anywhere which get the Adc clock frequency.
 */
uint32_t CRM_GetAdcFreq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    if (!HAL_CRM_AdcClkIsEnabled()){
        return 0;
    }
    return  CRM_GetHclkFreq();
#endif
}
/**
 * @brief Retrieves the current operating frequency of Efuse.
 *
 * This function returns the frequency (in Hz) at which Efuse is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Efuse clock
 * divider settings. This value is important for setting up Efuse communication parameters.
 *
 * @return uint32_t The operating frequency of Efuse in Hertz. If the frequency cannot be determined
 *                  or if Efuse is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Efuse frequency.
 *
 * @warning This function can call anywhere which get the Efuse clock frequency.
 */
uint32_t CRM_GetEfuseFreq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    if (!HAL_CRM_EfuseClkIsEnabled()){
        return 0;
    }    return  CRM_GetCmn32kSrcFreq();
#endif
}
/**
 * @brief Retrieves the current operating frequency of Dma2d.
 *
 * This function returns the frequency (in Hz) at which Dma2d is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Dma2d clock
 * divider settings. This value is important for setting up Dma2d communication parameters.
 *
 * @return uint32_t The operating frequency of Dma2d in Hertz. If the frequency cannot be determined
 *                  or if Dma2d is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Dma2d frequency.
 *
 * @warning This function can call anywhere which get the Dma2d clock frequency.
 */
uint32_t CRM_GetDma2dFreq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    return  CRM_GetHclkFreq();
#endif
}
/**
 * @brief Retrieves the current operating frequency of Video.
 *
 * This function returns the frequency (in Hz) at which Video is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Video clock
 * divider settings. This value is important for setting up Video communication parameters.
 *
 * @return uint32_t The operating frequency of Video in Hertz. If the frequency cannot be determined
 *                  or if Video is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Video frequency.
 *
 * @warning This function can call anywhere which get the Video clock frequency.
 */
uint32_t CRM_GetVideoFreq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    if (!HAL_CRM_VideoClkIsEnabled()){
        return 0;
    }    return  CRM_GetHclkFreq();
#endif
}/**
 * @brief Sets the clock divider for Qspi0.
 *
 * This function configures the clock division for Qspi0 by setting the divider ratios
 * to the specified values. The division is defined by 'div_n' and 'div_m'.
 * These parameters determine how the input clock frequency is divided to derive the
 * desired Qspi0 clock frequency.
 * * @param div_n The numerator part of the clock division ratio. Specifies the upper part
 *              of the division ratio, The div_n select range from [1 - 7].
 * * * @param div_m The denominator part of the clock division ratio. Specifies the lower part
 *              of the division ratio, The div_m select range from [1 - 15].
 * *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                 typically indicates what went wrong during the configuration process.
 *
 * @note The exact behavior and limitations of the division ratio depend on the specific
 *       hardware capabilities and clock configuration. Ensure that the values of 'div_n' and 'div_m' *       are within the valid range for your hardware.
 *
 * @warning Improper configuration of the clock divider might disrupt UART0 communication.
 *          It's important to ensure that 'div_n' and 'div_m' are set to values that are
 *          compatible with the Qspi0 specifications and the overall system clock settings.
 */
int32_t HAL_CRM_SetQspi0ClkDiv(uint32_t div_n, uint32_t div_m){
if (div_n == 0 || div_n >  7){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    if (div_m == 0 || div_m >  15){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
	IP_AP_CFG->REG_CLK_CFG1.bit.DIV_QSPI0_CLK_LD = 0x0;
	IP_AP_CFG->REG_CLK_CFG1.bit.DIV_QSPI0_CLK_N = div_n;
	IP_AP_CFG->REG_CLK_CFG1.bit.DIV_QSPI0_CLK_M = div_m;
	// Avoid the compiler out-of-order
    MemBarrier();
    IP_AP_CFG->REG_CLK_CFG1.bit.DIV_QSPI0_CLK_LD = 0x1;
    return CSK_DRIVER_OK;
}/**
 * @brief Sets the clock source for Qspi0.
 *
 * This function configures Qspi0 to use a specific clock source as defined by the 'src' parameter.
 * The 'src' parameter should be one of the values defined in the 'clock_src_name_t' enumeration,
 * representing the various clock sources available in the system. This allows for flexible
 * configuration of the Qspi0 clocking, depending on the system's requirements and the available
 * clock sources.
 *
 * @param src The desired clock source for Qspi0. This should be a value from the
 *            'clock_src_name_t' enumeration that specifies which clock source to use that Can choose this source -> CRM_IpSrcXtalClk, CRM_IpSrcPeriClk.
 *
 * @return uint32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                  can indicate issues such as an invalid clock source or a failure in applying
 *                  the new clock source setting.
 *
 * @note The function's ability to change the clock source may depend on the current state of Qspi0
 *       and the system's clock configuration. It is advisable to ensure that Qspi0 is not actively
 *       transmitting data when changing its clock source.
 *
 * @warning Using an incorrect or unsupported clock source for Qspi0 can lead to communication
 *          failures or system instability. Ensure that the selected clock source is compatible
 *          with Qspi0's operational requirements.
 */
uint32_t HAL_CRM_SetQspi0ClkSrc(clock_src_name_t src){
    if (src == CRM_IpSrcXtalClk){
        IP_AP_CFG->REG_CLK_CFG1.bit.SEL_QSPI0_CLK = 0;
    }
    if (src == CRM_IpSrcFlashClk){
        IP_AP_CFG->REG_CLK_CFG1.bit.SEL_QSPI0_CLK = 1;
    }
    return CSK_DRIVER_OK;
} 
/**
 * @brief Retrieves the current operating frequency of Qspi0.
 *
 * This function returns the frequency (in Hz) at which Qspi0 is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Qspi0 clock
 * divider settings. This value is important for setting up Qspi0 communication parameters.
 *
 * @return uint32_t The operating frequency of Qspi0 in Hertz. If the frequency cannot be determined
 *                  or if Qspi0 is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Qspi0 frequency.
 *
 * @warning This function can call anywhere which get the Qspi0 clock frequency.
 */
uint32_t CRM_GetQspi0Freq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    if (!HAL_CRM_Qspi0ClkIsEnabled()){
        return 0;
    }    clock_src_name_t src = CRM_IpSrcInvalide;
    uint32_t div_n;
    uint32_t div_m;
    HAL_CRM_GetQspi0ClkConfig(&src, &div_n, &div_m);
    return  CRM_GetSrcFreq(src) * div_n / div_m;
#endif
}/**
 * @brief Sets the clock divider for Qspi1.
 *
 * This function configures the clock division for Qspi1 by setting the divider ratios
 * to the specified values. The division is defined by 'div_n' and 'div_m'.
 * These parameters determine how the input clock frequency is divided to derive the
 * desired Qspi1 clock frequency.
 * * @param div_n The numerator part of the clock division ratio. Specifies the upper part
 *              of the division ratio, The div_n select range from [1 - 7].
 * * * @param div_m The denominator part of the clock division ratio. Specifies the lower part
 *              of the division ratio, The div_m select range from [1 - 15].
 * *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                 typically indicates what went wrong during the configuration process.
 *
 * @note The exact behavior and limitations of the division ratio depend on the specific
 *       hardware capabilities and clock configuration. Ensure that the values of 'div_n' and 'div_m' *       are within the valid range for your hardware.
 *
 * @warning Improper configuration of the clock divider might disrupt UART0 communication.
 *          It's important to ensure that 'div_n' and 'div_m' are set to values that are
 *          compatible with the Qspi1 specifications and the overall system clock settings.
 */
int32_t HAL_CRM_SetQspi1ClkDiv(uint32_t div_n, uint32_t div_m){
if (div_n == 0 || div_n >  7){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    if (div_m == 0 || div_m >  15){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
	IP_AP_CFG->REG_CLK_CFG1.bit.DIV_QSPI1_CLK_LD = 0x0;
	IP_AP_CFG->REG_CLK_CFG1.bit.DIV_QSPI1_CLK_N = div_n;
	IP_AP_CFG->REG_CLK_CFG1.bit.DIV_QSPI1_CLK_M = div_m;
	// Avoid the compiler out-of-order
    MemBarrier();
    IP_AP_CFG->REG_CLK_CFG1.bit.DIV_QSPI1_CLK_LD = 0x1;
    return CSK_DRIVER_OK;
}/**
 * @brief Sets the clock source for Qspi1.
 *
 * This function configures Qspi1 to use a specific clock source as defined by the 'src' parameter.
 * The 'src' parameter should be one of the values defined in the 'clock_src_name_t' enumeration,
 * representing the various clock sources available in the system. This allows for flexible
 * configuration of the Qspi1 clocking, depending on the system's requirements and the available
 * clock sources.
 *
 * @param src The desired clock source for Qspi1. This should be a value from the
 *            'clock_src_name_t' enumeration that specifies which clock source to use that Can choose this source -> CRM_IpSrcXtalClk, CRM_IpSrcPeriClk.
 *
 * @return uint32_t Returns 0 on success, or a non-zero error code on failure. The error code
 *                  can indicate issues such as an invalid clock source or a failure in applying
 *                  the new clock source setting.
 *
 * @note The function's ability to change the clock source may depend on the current state of Qspi1
 *       and the system's clock configuration. It is advisable to ensure that Qspi1 is not actively
 *       transmitting data when changing its clock source.
 *
 * @warning Using an incorrect or unsupported clock source for Qspi1 can lead to communication
 *          failures or system instability. Ensure that the selected clock source is compatible
 *          with Qspi1's operational requirements.
 */
uint32_t HAL_CRM_SetQspi1ClkSrc(clock_src_name_t src){
    if (src == CRM_IpSrcXtalClk){
        IP_AP_CFG->REG_CLK_CFG1.bit.SEL_QSPI1_CLK = 0;
    }
    if (src == CRM_IpSrcFlashClk){
        IP_AP_CFG->REG_CLK_CFG1.bit.SEL_QSPI1_CLK = 1;
    }
    return CSK_DRIVER_OK;
} 
/**
 * @brief Retrieves the current operating frequency of Qspi1.
 *
 * This function returns the frequency (in Hz) at which Qspi1 is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Qspi1 clock
 * divider settings. This value is important for setting up Qspi1 communication parameters.
 *
 * @return uint32_t The operating frequency of Qspi1 in Hertz. If the frequency cannot be determined
 *                  or if Qspi1 is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Qspi1 frequency.
 *
 * @warning This function can call anywhere which get the Qspi1 clock frequency.
 */
uint32_t CRM_GetQspi1Freq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    if (!HAL_CRM_Qspi1ClkIsEnabled()){
        return 0;
    }    clock_src_name_t src = CRM_IpSrcInvalide;
    uint32_t div_n;
    uint32_t div_m;
    HAL_CRM_GetQspi1ClkConfig(&src, &div_n, &div_m);
    return  CRM_GetSrcFreq(src) * div_n / div_m;
#endif
}
/**
 * @brief Retrieves the current operating frequency of Dvp.
 *
 * This function returns the frequency (in Hz) at which Dvp is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Dvp clock
 * divider settings. This value is important for setting up Dvp communication parameters.
 *
 * @return uint32_t The operating frequency of Dvp in Hertz. If the frequency cannot be determined
 *                  or if Dvp is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Dvp frequency.
 *
 * @warning This function can call anywhere which get the Dvp clock frequency.
 */
uint32_t CRM_GetDvpFreq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    if (!HAL_CRM_DvpClkIsEnabled()){
        return 0;
    }    return  CRM_GetHclkFreq();
#endif
}
/**
 * @brief Retrieves the current operating frequency of Keysense0.
 *
 * This function returns the frequency (in Hz) at which Keysense0 is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Keysense0 clock
 * divider settings. This value is important for setting up Keysense0 communication parameters.
 *
 * @return uint32_t The operating frequency of Keysense0 in Hertz. If the frequency cannot be determined
 *                  or if Keysense0 is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Keysense0 frequency.
 *
 * @warning This function can call anywhere which get the Keysense0 clock frequency.
 */
uint32_t CRM_GetKeysense0Freq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    if (!HAL_CRM_Keysense0ClkIsEnabled()){
        return 0;
    }    return  CRM_GetCmn32kSrcFreq();
#endif
}
/**
 * @brief Retrieves the current operating frequency of Keysense1.
 *
 * This function returns the frequency (in Hz) at which Keysense1 is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Keysense1 clock
 * divider settings. This value is important for setting up Keysense1 communication parameters.
 *
 * @return uint32_t The operating frequency of Keysense1 in Hertz. If the frequency cannot be determined
 *                  or if Keysense1 is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Keysense1 frequency.
 *
 * @warning This function can call anywhere which get the Keysense1 clock frequency.
 */
uint32_t CRM_GetKeysense1Freq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    if (!HAL_CRM_Keysense1ClkIsEnabled()){
        return 0;
    }    return  CRM_GetCmn32kSrcFreq();
#endif
}
/**
 * @brief Retrieves the current operating frequency of Dualtimer.
 *
 * This function returns the frequency (in Hz) at which Dualtimer is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Dualtimer clock
 * divider settings. This value is important for setting up Dualtimer communication parameters.
 *
 * @return uint32_t The operating frequency of Dualtimer in Hertz. If the frequency cannot be determined
 *                  or if Dualtimer is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Dualtimer frequency.
 *
 * @warning This function can call anywhere which get the Dualtimer clock frequency.
 */
uint32_t CRM_GetDualtimerFreq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    return  CRM_GetCmn32kSrcFreq();
#endif
}
/**
 * @brief Retrieves the current operating frequency of Aon_timer.
 *
 * This function returns the frequency (in Hz) at which Aon_timer is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Aon_timer clock
 * divider settings. This value is important for setting up Aon_timer communication parameters.
 *
 * @return uint32_t The operating frequency of Aon_timer in Hertz. If the frequency cannot be determined
 *                  or if Aon_timer is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Aon_timer frequency.
 *
 * @warning This function can call anywhere which get the Aon_timer clock frequency.
 */
uint32_t CRM_GetAon_timerFreq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    if (!HAL_CRM_Aon_timerClkIsEnabled()){
        return 0;
    }    return  CRM_GetRc32kSrcFreq();
#endif
}
/**
 * @brief Retrieves the current operating frequency of Aon_wdt.
 *
 * This function returns the frequency (in Hz) at which Aon_wdt is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Aon_wdt clock
 * divider settings. This value is important for setting up Aon_wdt communication parameters.
 *
 * @return uint32_t The operating frequency of Aon_wdt in Hertz. If the frequency cannot be determined
 *                  or if Aon_wdt is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Aon_wdt frequency.
 *
 * @warning This function can call anywhere which get the Aon_wdt clock frequency.
 */
uint32_t CRM_GetAon_wdtFreq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    return  CRM_GetRc32kSrcFreq();
#endif
}
/**
 * @brief Retrieves the current operating frequency of Mailbox.
 *
 * This function returns the frequency (in Hz) at which Mailbox is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Mailbox clock
 * divider settings. This value is important for setting up Mailbox communication parameters.
 *
 * @return uint32_t The operating frequency of Mailbox in Hertz. If the frequency cannot be determined
 *                  or if Mailbox is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Mailbox frequency.
 *
 * @warning This function can call anywhere which get the Mailbox clock frequency.
 */
uint32_t CRM_GetMailboxFreq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    return  CRM_GetHclkFreq();
#endif
}
/**
 * @brief Retrieves the current operating frequency of Mutex.
 *
 * This function returns the frequency (in Hz) at which Mutex is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Mutex clock
 * divider settings. This value is important for setting up Mutex communication parameters.
 *
 * @return uint32_t The operating frequency of Mutex in Hertz. If the frequency cannot be determined
 *                  or if Mutex is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Mutex frequency.
 *
 * @warning This function can call anywhere which get the Mutex clock frequency.
 */
uint32_t CRM_GetMutexFreq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    return  CRM_GetHclkFreq();
#endif
}
/**
 * @brief Retrieves the current operating frequency of Luna.
 *
 * This function returns the frequency (in Hz) at which Luna is currently operating. The frequency
 * is determined by the current configuration of the system's clock sources and the Luna clock
 * divider settings. This value is important for setting up Luna communication parameters.
 *
 * @return uint32_t The operating frequency of Luna in Hertz. If the frequency cannot be determined
 *                  or if Luna is not properly configured, the function may return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's clock
 *       configuration. Changes to the clock setup may affect the Luna frequency.
 *
 * @warning This function can call anywhere which get the Luna clock frequency.
 */
uint32_t CRM_GetLunaFreq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    if (!HAL_CRM_LunaClkIsEnabled()){
        return 0;
    }    return  CRM_GetHclkFreq();
#endif
}
/**********************************CORE************************************/
/**
 * @brief Sets the clock divider for the Cmn_peri_pclk.
 *
 * This function configures the clock division for the Cmn_peri_pclk
 * using the specified divider ratios. The division is determined by 'div_n'
 * and 'div_m'. These parameters define how the input clock frequency is divided to obtain
 * the desired Cmn_peri_pclk clock frequency.
 * * @param div_n The numerator part of the clock division ratio. It specifies the upper part
 *              of the division ratio, controlling how much the input clock frequency is
 *              divided, the div_n select range from [1 - 15].
 * * * @param div_m The denominator part of the clock division ratio. It specifies the lower part
 *              of the division ratio, contributing to the final clock frequency calculation,
 *              The div_m select range from [1 - 31]
 * * @return void This function does not return a value. Any errors or status conditions may be
 *              handled internally or reported through other mechanisms, as per the hardware
 *              design and system requirements.
 *
 * @note The behavior and constraints of the division ratio are dependent on the hardware
 *       capabilities and current clock configuration. It is important to ensure that the values
 *       for 'div_n' and 'div_m' are within the acceptable range for the system's hardware.
 *
 * @warning Incorrect configuration of the clock divider can affect the operation of the Cmn_peri_pclk
 *          and connected bus. Ensure that 'div_n' and 'div_m' are set
 *          to values that are appropriate for the desired clock frequency and compatible with
 *          the peripheral's operational requirements.
 */
int32_t HAL_CRM_SetCmn_peri_pclkClkDiv(uint32_t div_n, uint32_t div_m){
    if (div_n == 0 || div_n >  15){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    if (div_m == 0 || div_m >  31){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    IP_SYSNODEF->REG_BUS_CLK_CFG1.bit.DIV_CMN_PERI_PCLK_LD = 0x0;
	IP_SYSNODEF->REG_BUS_CLK_CFG1.bit.DIV_CMN_PERI_PCLK_N = div_n;
	IP_SYSNODEF->REG_BUS_CLK_CFG1.bit.DIV_CMN_PERI_PCLK_M = div_m;
    // Avoid the compiler out-of-order
    MemBarrier();
    IP_SYSNODEF->REG_BUS_CLK_CFG1.bit.DIV_CMN_PERI_PCLK_LD = 0x1;

    return CSK_DRIVER_OK;
}
/**
 * @brief Retrieves the current operating frequency of the Cmn_peri_pclk.
 *
 * This function returns the frequency (in Hz) at which the Cmn_peri_pclk is currently operating.
 * its frequency is crucial for determining the performance and timing characteristics of
 * various system components. The frequency is determined by the current configuration of
 * the system's clock sources and any relevant clock dividers.
 *
 * @return uint32_t The operating frequency of Cmn_peri_pclk in Hertz. If the frequency cannot be
 *                  determined, or if Cmn_peri_pclk is not properly configured, the function may
 *                  return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's
 *       clock configuration. Changes in clock source or divider settings can affect the
 *       Cmn_peri_pclk frequency.
 *
 * @warning This function can call anywhere which get the Cmn_peri_pclk clock frequency.
 */
uint32_t CRM_GetCmn_peri_pclkFreq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
uint32_t div_n;uint32_t div_m;HAL_CRM_GetCmn_peri_pclkClkConfig(&div_n, &div_m);    return CRM_GetHclkFreq() * div_n / div_m;
#endif
}
/**
 * @brief Sets the clock divider for the Aon_cfg_pclk.
 *
 * This function configures the clock division for the Aon_cfg_pclk
 * using the specified divider ratios. The division is determined by 'div_n'
 * and 'div_m'. These parameters define how the input clock frequency is divided to obtain
 * the desired Aon_cfg_pclk clock frequency.
 * * @param div_n The numerator part of the clock division ratio. It specifies the upper part
 *              of the division ratio, controlling how much the input clock frequency is
 *              divided, the div_n select range from [1 - 31].
 * * * @param div_m The denominator part of the clock division ratio. It specifies the lower part
 *              of the division ratio, contributing to the final clock frequency calculation,
 *              The div_m select range from [1 - 63]
 * * @return void This function does not return a value. Any errors or status conditions may be
 *              handled internally or reported through other mechanisms, as per the hardware
 *              design and system requirements.
 *
 * @note The behavior and constraints of the division ratio are dependent on the hardware
 *       capabilities and current clock configuration. It is important to ensure that the values
 *       for 'div_n' and 'div_m' are within the acceptable range for the system's hardware.
 *
 * @warning Incorrect configuration of the clock divider can affect the operation of the Aon_cfg_pclk
 *          and connected bus. Ensure that 'div_n' and 'div_m' are set
 *          to values that are appropriate for the desired clock frequency and compatible with
 *          the peripheral's operational requirements.
 */
int32_t HAL_CRM_SetAon_cfg_pclkClkDiv(uint32_t div_n, uint32_t div_m){
    if (div_n == 0 || div_n >  31){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    if (div_m == 0 || div_m >  63){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    IP_SYSNODEF->REG_BUS_CLK_CFG1.bit.DIV_AON_CFG_PCLK_LD = 0x0;
	IP_SYSNODEF->REG_BUS_CLK_CFG1.bit.DIV_AON_CFG_PCLK_N = div_n;
	IP_SYSNODEF->REG_BUS_CLK_CFG1.bit.DIV_AON_CFG_PCLK_M = div_m;
    // Avoid the compiler out-of-order
    MemBarrier();
    IP_SYSNODEF->REG_BUS_CLK_CFG1.bit.DIV_AON_CFG_PCLK_LD = 0x1;

    return CSK_DRIVER_OK;
}
/**
 * @brief Retrieves the current operating frequency of the Aon_cfg_pclk.
 *
 * This function returns the frequency (in Hz) at which the Aon_cfg_pclk is currently operating.
 * its frequency is crucial for determining the performance and timing characteristics of
 * various system components. The frequency is determined by the current configuration of
 * the system's clock sources and any relevant clock dividers.
 *
 * @return uint32_t The operating frequency of Aon_cfg_pclk in Hertz. If the frequency cannot be
 *                  determined, or if Aon_cfg_pclk is not properly configured, the function may
 *                  return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's
 *       clock configuration. Changes in clock source or divider settings can affect the
 *       Aon_cfg_pclk frequency.
 *
 * @warning This function can call anywhere which get the Aon_cfg_pclk clock frequency.
 */
uint32_t CRM_GetAon_cfg_pclkFreq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
uint32_t div_n;uint32_t div_m;HAL_CRM_GetAon_cfg_pclkClkConfig(&div_n, &div_m);    return CRM_GetHclkFreq() * div_n / div_m;
#endif
}
/**
 * @brief Sets the clock divider for the Ap_peri_pclk.
 *
 * This function configures the clock division for the Ap_peri_pclk
 * using the specified divider ratios. The division is determined by 'div_n'
 * and 'div_m'. These parameters define how the input clock frequency is divided to obtain
 * the desired Ap_peri_pclk clock frequency.
 * * @param div_n The numerator part of the clock division ratio. It specifies the upper part
 *              of the division ratio, controlling how much the input clock frequency is
 *              divided, the div_n select range from [1 - 15].
 * * * @param div_m The denominator part of the clock division ratio. It specifies the lower part
 *              of the division ratio, contributing to the final clock frequency calculation,
 *              The div_m select range from [1 - 31]
 * * @return void This function does not return a value. Any errors or status conditions may be
 *              handled internally or reported through other mechanisms, as per the hardware
 *              design and system requirements.
 *
 * @note The behavior and constraints of the division ratio are dependent on the hardware
 *       capabilities and current clock configuration. It is important to ensure that the values
 *       for 'div_n' and 'div_m' are within the acceptable range for the system's hardware.
 *
 * @warning Incorrect configuration of the clock divider can affect the operation of the Ap_peri_pclk
 *          and connected bus. Ensure that 'div_n' and 'div_m' are set
 *          to values that are appropriate for the desired clock frequency and compatible with
 *          the peripheral's operational requirements.
 */
int32_t HAL_CRM_SetAp_peri_pclkClkDiv(uint32_t div_n, uint32_t div_m){
    if (div_n == 0 || div_n >  15){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    if (div_m == 0 || div_m >  31){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    IP_AP_CFG->REG_CLK_CFG0.bit.DIV_AP_PERI_PCLK_LD = 0x0;
	IP_AP_CFG->REG_CLK_CFG0.bit.DIV_AP_PERI_PCLK_N = div_n;
	IP_AP_CFG->REG_CLK_CFG0.bit.DIV_AP_PERI_PCLK_M = div_m;
    // Avoid the compiler out-of-order
    MemBarrier();
    IP_AP_CFG->REG_CLK_CFG0.bit.DIV_AP_PERI_PCLK_LD = 0x1;

    return CSK_DRIVER_OK;
}
/**
 * @brief Retrieves the current operating frequency of the Ap_peri_pclk.
 *
 * This function returns the frequency (in Hz) at which the Ap_peri_pclk is currently operating.
 * its frequency is crucial for determining the performance and timing characteristics of
 * various system components. The frequency is determined by the current configuration of
 * the system's clock sources and any relevant clock dividers.
 *
 * @return uint32_t The operating frequency of Ap_peri_pclk in Hertz. If the frequency cannot be
 *                  determined, or if Ap_peri_pclk is not properly configured, the function may
 *                  return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's
 *       clock configuration. Changes in clock source or divider settings can affect the
 *       Ap_peri_pclk frequency.
 *
 * @warning This function can call anywhere which get the Ap_peri_pclk clock frequency.
 */
uint32_t CRM_GetAp_peri_pclkFreq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
uint32_t div_n;uint32_t div_m;HAL_CRM_GetAp_peri_pclkClkConfig(&div_n, &div_m);    return CRM_GetHclkFreq() * div_n / div_m;
#endif
}
/**
 * @brief Sets the clock divider for the Hclk.
 *
 * This function configures the clock division for the Hclk
 * using the specified divider ratios. The division is determined by 'div_n'
 * and 'div_m'. These parameters define how the input clock frequency is divided to obtain
 * the desired Hclk clock frequency.
 * * @param div_n The numerator part of the clock division ratio. It specifies the upper part
 *              of the division ratio, controlling how much the input clock frequency is
 *              divided, the div_n select range from [1 - 15].
 * * * @param div_m The denominator part of the clock division ratio. It specifies the lower part
 *              of the division ratio, contributing to the final clock frequency calculation,
 *              The div_m select range from [1 - 31]
 * * @return void This function does not return a value. Any errors or status conditions may be
 *              handled internally or reported through other mechanisms, as per the hardware
 *              design and system requirements.
 *
 * @note The behavior and constraints of the division ratio are dependent on the hardware
 *       capabilities and current clock configuration. It is important to ensure that the values
 *       for 'div_n' and 'div_m' are within the acceptable range for the system's hardware.
 *
 * @warning Incorrect configuration of the clock divider can affect the operation of the Hclk
 *          and connected bus. Ensure that 'div_n' and 'div_m' are set
 *          to values that are appropriate for the desired clock frequency and compatible with
 *          the peripheral's operational requirements.
 */
int32_t HAL_CRM_SetHclkClkDiv(uint32_t div_n, uint32_t div_m){
    if (div_n == 0 || div_n >  15){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    if (div_m == 0 || div_m >  31){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    IP_SYSNODEF->REG_BUS_CLK_CFG0.bit.DIV_HCLK_LD = 0x0;
	IP_SYSNODEF->REG_BUS_CLK_CFG0.bit.DIV_HCLK_N = div_n;
	IP_SYSNODEF->REG_BUS_CLK_CFG0.bit.DIV_HCLK_M = div_m;
    // Avoid the compiler out-of-order
    MemBarrier();
    IP_SYSNODEF->REG_BUS_CLK_CFG0.bit.DIV_HCLK_LD = 0x1;

    return CSK_DRIVER_OK;
}
/**
 * @brief Sets the clock source for the Hclk.
 *
 * This function configures the system's Hclk to use a specific
 * clock source as defined by the 'src' parameter. The 'src' parameter should be one of
 * the values defined in the 'clock_src_name_t' enumeration, representing the various
 * clock sources available in the system. Changing the Hclk source can be crucial for
 * system performance tuning, power management, or adapting to different operational
 * modes.
 *
 * @param src The desired clock source for Hclk. It should be a value from the
 *            'clock_src_name_t' enumeration that specifies which clock source to use, can choose this source -> [CRM_IpSrcXtalClk, CRM_IpSrcCoreClk, ]
 *
 * @return uint32_t Returns 0 on success, or a non-zero error code on failure. The error
 *                  code can indicate issues such as an invalid clock source or failure
 *                  in applying the new clock source setting.
 *
 * @note The function's ability to change the clock source may depend on the current state
 *       of Hclk and the system's overall clock configuration. It is advisable to ensure
 *       that the system is in a suitable state to change the clock source without
 *       disrupting operational stability.
 *
 * @warning Using an incorrect or unsupported clock source for Hclk can lead to system
 *          instability or malfunction. Ensure that the selected clock source is compatible
 *          with the system's requirements and that any dependent subsystems are
 *          appropriately configured to handle the change in clock source.
 */
uint32_t HAL_CRM_SetHclkClkSrc(clock_src_name_t src){
    if (src == CRM_IpSrcXtalClk){
        IP_SYSNODEF->REG_BUS_CLK_CFG0.bit.SEL_HCLK = 0;
    }
    if (src == CRM_IpSrcCoreClk){
        IP_SYSNODEF->REG_BUS_CLK_CFG0.bit.SEL_HCLK = 1;
    }
    if (src == CRM_IpSrcBBPLLCoreClk){
        IP_SYSNODEF->REG_BUS_CLK_CFG0.bit.SEL_HCLK = 2;
    }
    return CSK_DRIVER_OK;
} /**
 * @brief Retrieves the current operating frequency of the Hclk.
 *
 * This function returns the frequency (in Hz) at which the Hclk is currently operating.
 * its frequency is crucial for determining the performance and timing characteristics of
 * various system components. The frequency is determined by the current configuration of
 * the system's clock sources and any relevant clock dividers.
 *
 * @return uint32_t The operating frequency of Hclk in Hertz. If the frequency cannot be
 *                  determined, or if Hclk is not properly configured, the function may
 *                  return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's
 *       clock configuration. Changes in clock source or divider settings can affect the
 *       Hclk frequency.
 *
 * @warning This function can call anywhere which get the Hclk clock frequency.
 */
uint32_t CRM_GetHclkFreq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    clock_src_name_t src = CRM_IpSrcInvalide;
    uint32_t div_n;uint32_t div_m;
    HAL_CRM_GetHclkClkConfig(&src, &div_n, &div_m);
    return CRM_GetSrcFreq(src) * div_n / div_m;
#endif
}

/**
 * @brief Retrieves the current operating frequency of the Cpu.
 *
 * This function returns the frequency (in Hz) at which the Cpu is currently operating.
 * its frequency is crucial for determining the performance and timing characteristics of
 * various system components. The frequency is determined by the current configuration of
 * the system's clock sources and any relevant clock dividers.
 *
 * @return uint32_t The operating frequency of Cpu in Hertz. If the frequency cannot be
 *                  determined, or if Cpu is not properly configured, the function may
 *                  return 0.
 *
 * @note The returned frequency value is dependent on the current state of the system's
 *       clock configuration. Changes in clock source or divider settings can affect the
 *       Cpu frequency.
 *
 * @warning This function can call anywhere which get the Cpu clock frequency.
 */
uint32_t CRM_GetCpuFreq(){
#if (IC_BOARD == 0)
    return IC_BOARD_FPGA_FIX_FREQ;
#else
    return CRM_GetHclkFreq();
#endif
}

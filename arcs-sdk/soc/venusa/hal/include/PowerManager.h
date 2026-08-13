/**
  ******************************************************************************
  * @file    PowerManager.h
  * @author  ListenAI Application Team
  * @brief   Header file of PMU HAL module.
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
#ifndef __CSK_DRIVER_PMU_H
#define __CSK_DRIVER_PMU_H


#ifdef __cplusplus
 extern "C" {
#endif


/* Includes ------------------------------------------------------------------*/
#include "Driver_Common.h"
#include "venusa_ap.h"

 /** @defgroup PMU
   * @brief PMU HAL module driver
   * @{
   */

 /* Exported types ------------------------------------------------------------*/
 /** @defgroup PMU_Exported_Types PMU Exported Types
   * @{
   */
typedef enum {
  PMU_SLEEP_NONE = 0U,
  PMU_SLEEP_TRIGGER_BY_CORE0,
  PMU_SLEEP_TRIGGER_BY_CORE1,
  PMU_SLEEP_TRIGGER_BY_CORE0_AND_CORE1
} pmu_sleep_trigger_type_t;

 /**
  * @brief PMU sleep mode enumeration.
  *
  * This enum defines the possible Deep Sleep modes that can be used with the
  * HAL_PMU_EnterDeepSleepMode function.
  */
typedef enum _pmu_sleepmode {
    PMU_SLEEPMODE_MODE1 = 0x1U,
    PMU_SLEEPMODE_MODE2,	//aon_sub module can wake up this sleep mode
    PMU_SLEEPMODE_MODE3,	//only GPIOB&KEYSENSE can wake up this sleep mode
}pmu_sleepmode_t;

typedef enum _pmu_wakeupsrc {
  PMU_WAKEUP_TIMER = 0,
  PMU_WAKEUP_IWDT,
  PMU_WAKEUP_KEY,
  PMU_WAKEUP_RTC,
  PMU_WAKEUP_GPIOB_00 = 16U,
  PMU_WAKEUP_GPIOB_01,
  PMU_WAKEUP_GPIOB_02,
  PMU_WAKEUP_GPIOB_03,
  PMU_WAKEUP_GPIOB_04,
  PMU_WAKEUP_GPIOB_05,
  PMU_WAKEUP_NONE = 0xFFU,
}pmu_wakeupsrc_t;

typedef enum _pmu_rstsrc {
    PMU_RST_POR                 = 0U,     // Bit[0]: Power-On Reset (POR) or Pad Reset
    PMU_RST_AON                 = 1U,     // Bit[1]: Always-On subsystem reset (aon_sw or WDT)
    PMU_RST_SYSRESETREQ_CORE1   = 16U,    // Bit[16]: core1_sysresetreq
    PMU_RST_SYSRESETREQ_CORE0   = 17U,    // Bit[17]: core0_sysresetreq
    PMU_RST_SW1                 = 18U,    // Bit[18]: cmn_sw1_reset
    PMU_RST_SW0                 = 19U,    // Bit[19]: cmn_sw0_reset
    PMU_RST_WDT_CORE1           = 20U,    // Bit[20]: core1_wdt_rst
    PMU_RST_WDT_CORE0           = 21U,    // Bit[21]: core0_wdt_rst
    PMU_RST_NONE = 0xFFU,
}pmu_rstsrc_t;

typedef struct
{
  uint32_t bit;
  pmu_rstsrc_t src;
} pmu_reset_src_map_t;

typedef enum _pmu_gpio_src {
    PMU_POLARITY_GPIOB_00 = 0x0U,
    PMU_POLARITY_GPIOB_01,
    PMU_POLARITY_GPIOB_02,
    PMU_POLARITY_GPIOB_03,
    PMU_POLARITY_GPIOB_04,
    PMU_POLARITY_GPIOB_05,
}pmu_gpio_src_t;

/**
  * @}
  */

/* Exported constants --------------------------------------------------------*/

/** @defgroup PMU_Exported_Constants PMU Exported Constants
  * @{
  */
#define CSK_PMU_API_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,0)
#define CSK_PMU_DRV_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,1)


/*---------------------Control mode for application---------------------------------*/
/** @defgroup PMU_HOLD_mode_entry PMU SLEEP mode entry
  * @{
  */
#define PMU_HOLDENTRY_WFI              ((uint8_t)0x01)
#define PMU_HOLDENTRY_WFE              ((uint8_t)0x02)
/**
  * @}
  */


/** @defgroup PMU_LIGHTSLEEP_mode_entry PMU STOP mode entry
  * @{
  */
#define PMU_LIGHTSLEEPENTRY_WFI               ((uint8_t)0x01)
#define PMU_LIGHTSLEEPENTRY_WFE               ((uint8_t)0x02)
/**
  * @}
  */

/** @defgroup PMU_DEEPSLEEP_mode_entry PMU STOP mode entry
  * @{
  */
#define PMU_DEEPSLEEPENTRY_WFI               ((uint8_t)0x01)
#define PMU_DEEPSLEEPENTRY_WFE               ((uint8_t)0x02)
/**
  * @}
  */

/**
  * @}
  */

/* Exported macro ------------------------------------------------------------*/
/** @defgroup PMU_Exported_Macro PMU Exported Macro
  * @{
  */


/**
  * @}
  */
/* Exported functions --------------------------------------------------------*/
/** @defgroup PMU_Exported_Functions PMU Exported Functions
  * @{
  */
/** @defgroup PMU_Exported_Functions1 PMU Exported Functions1
  * @{
  */
void HAL_PMU_PreConfigSleepTrigger(pmu_sleep_trigger_type_t sleep_trigger);

/**
 * @brief Get the system reset cause.
 *
 * This function retrieves the system reset cause from the AON status register
 * and then clears the reset cause.
 *
 * @return The reset source as defined in pmu_rstsrc_t.
 */
pmu_rstsrc_t HAL_PMU_GetSysResetCause(void);

/**
 * @brief Get the wake-up cause.
 *
 * This function checks the PMU wake-up source register and returns the first
 * set wake-up source it finds.
 *
 * @return The wake-up source as defined in pmu_wakeupsrc_t.
 *         Returns PMU_WAKEUP_NONE if no source is found.
 */
pmu_wakeupsrc_t HAL_PMU_GetWakeUpCause(void);

/**
 * @brief Clear the system reset cause.
 *
 * This function clears the system reset cause in the AON status register.
 */
void HAL_PMU_ClearSysResetCause(void);

/**
 * @brief Clear the wake-up cause.
 *
 * This function clears the wake-up cause in the PMU wake-up IRQ clear register.
 */
void HAL_PMU_ClearWakeUpCause(void);

/*
 * @brief Enable a specific wake-up source.
 *
 * This function enables a specified wake-up source in the PMU enable wake-up register.
 *
 * @param WakeUpSrc The wake-up source to enable, as defined in pmu_wakeupsrc_t.
 */
void HAL_PMU_EnableWakeUpSrc(pmu_wakeupsrc_t WakeUpSrc);

/**
 * @brief Disable a specific wake-up source.
 *
 * This function disables a specified wake-up source in the PMU enable wake-up register.
 *
 * @param WakeUpSrc The wake-up source to disable.
 */
void HAL_PMU_DisableWakeUpSrc(pmu_wakeupsrc_t WakeUpSrc);

/**
* @brief Enable IRQ for a specific wake-up source.
*
* This function enables the interrupt for a specified wake-up source in the
* PMU enable wake-up IRQ register.
*
* @param WakeUpSrc The wake-up source for which to enable the IRQ, as defined in pmu_wakeupsrc_t.
*/
void HAL_PMU_EnableWakeUpSrcIrq(pmu_wakeupsrc_t WakeUpSrc);

/**
* @brief Disable IRQ for a specific wake-up source.
*
* This function disables the interrupt for a specified wake-up source in the
* PMU enable wake-up IRQ register.
*
* @param WakeUpSrc The wake-up source for which to enable the IRQ, as defined in pmu_wakeupsrc_t.
*/
void HAL_PMU_DisableWakeUpSrcIrq(pmu_wakeupsrc_t WakeUpSrc);

/**
 * @brief Select GPIO polarity for wake-up.
 *
 * This function sets the polarity for a specific GPIO used as a wake-up source.
 * The polarity can be set to either high or low.
 *
 * @param GpioPos The position of the GPIO in the wake-up source, as defined in pmu_gpio_src_t.
 * @param polarity The polarity to be set for the specified GPIO.
 *                - 0: Set the polarity to low.
 *                - 1: Set the polarity to high.
 */
void HAL_PMU_GPIOPolaritySelect(pmu_gpio_src_t GpioPos, uint8_t polarity);

/*
 * @brief Enter Deep Sleep Mode.
 *
 * This function puts the system into a specified Deep Sleep mode. The sleep mode is selected
 * through the SleepMode parameter, and the method to enter sleep (WFI or WFE) is determined by
 * the SLEEPEntry parameter.
 *
 * @param SleepMode The deep sleep mode to enter, as defined by pmu_sleepmode_t.
 * @param SLEEPEntry Determines the method to enter sleep mode. Use PMU_HOLDENTRY_WFI for
 *        Wait For Interrupt, or other values for Wait For Event.
 */
void HAL_PMU_EnterDeepSleepMode(pmu_sleepmode_t SleepMode, uint8_t SLEEPEntry);

/*
  * @brief Set AON LDO VMEM on mode.
  *
  * This function is called to set AON LDO to VMEM on mode. includes PSRAM and sdio power enable
*/
#define __HAL_PMU_AON_LDO_VMEM_ON()	\
do { \
  IP_AON_CTRL->REG_AON_LDO_VMEM.bit.ENA_LDO_VMEM = 0x1;	\
} while(0)

/*
  * @brief Set AON LDO VMEM off mode.
  *
  * This function is called to set AON LDO to VMEM off mode. Includes PSRAM and sdio power disable
*/
#define __HAL_PMU_AON_LDO_VMEM_OFF()	\
do { \
  IP_AON_CTRL->REG_AON_LDO_VMEM.bit.ENA_LDO_VMEM = 0x0;	\
} while(0)

/*
  * @brief Set AON LDO VA on mode.
  *
  * This function is called to set AON LDO to VA on mode. Includes efuse gpioc and audio power enable
*/
#define __HAL_PMU_AON_LDO_VA_ON()	\
do { \
	IP_AON_CTRL->REG_AON_TUNE0.bit.EN_LDO_VA = 0x1;	\
} while(0)

/*
  * @brief Set AON LDO VA off mode.
  *
  * This function is called to set AON LDO to VA off mode. Includes efuse gpioc and audio power disable
*/
#define __HAL_PMU_AON_LDO_VA_OFF()	\
do { \
  IP_AON_CTRL->REG_AON_TUNE0.bit.EN_LDO_VA = 0x0;	\
} while(0)

/*
  * @brief Set AON LDO to normal off mode.
  *
  * This function is called to set AON LDO to normal off mode
*/
#define __HAL_PMU_AON_LDO_NORMAL_OFF()	\
do { \
	IP_AON_CTRL->REG_AON_FRC_CTRL0.bit.NORMON_VREF_LDOAON_FORCE= 0x1;	\
} while(0)

/*
  * @brief Set AON LDO to normal on mode.
  *
  * This function is called to set AON LDO to normal on mode
*/
#define __HAL_PMU_AON_LDO_NORMAL_ON()	\
do { \
  IP_AON_CTRL->REG_AON_FRC_CTRL0.bit.NORMON_VREF_LDOAON_FORCE= 0x0;	\
} while(0)

/**
 * @brief Clear AON WDT interrupt.
 *
 * This function is called to perform clear AON WDT interrupt
 */
#define __HAL_PMU_AON_WDT_CLEAR_IRQ()  \
do { \
	  IP_AON_WDT->REG_AON_WDT_IRQ_CLR.all = 0x1;	\
	  while(IP_AON_WDT->REG_AON_WDT_IRQ_CAUSE.bit.WDT_WAKEUP_STATUS);	\
} while(0)

/**
 * @brief Clear AON Timer interrupt.
 *
 * This function is called to perform clear AON Timer interrupt
 */
#define __HAL_PMU_AON_TIMER_CLEAR_IRQ()  \
do { \
	IP_AON_TIMER->REG_OS_TIMER_IRQ_CLR.all = 0x1;	\
	while(IP_AON_TIMER->REG_OS_TIMER_IRQ_CAUSE.bit.OSTIMER_STATUS);	\
} while(0)


/**
 * @brief Initiate software reset for CMN core 0.
 *
 * Triggers a coordinated reset sequence targeting the Common Management Network (CMN) core 0.
 * Follows required sequence of enabling reset configuration and writing magic number to reset register.
 *
 * @details Two-step process:
 *          1. Set CMNSW0_2CMN_RST_EN bit in SW_RESET_CFG0
 *          2. Write special value (0xcafe000a) to SW_RESET_CORE0 register
 */
#define __HAL_PMU_CMN_SOFTWARE_CORE0_RESET()  \
do { \
    IP_CMN_SYS->REG_SW_RESET_CFG0.bit.CMNSW0_2CMN_RST_EN = 0x1U; \
    IP_CMN_SYS->REG_SW_RESET_CORE0.all = 0xcafe000a; \
} while(0)


/**
 * @brief Initiate software reset for CMN core 1.
 *
 * Triggers a coordinated reset sequence targeting the Common Management Network (CMN) core 1.
 * Follows required sequence of enabling reset configuration and writing magic number to reset register.
 *
 * @details Two-step process:
 *          1. Set CMNSW1_2CMN_RST_EN bit in SW_RESET_CFG0
 *          2. Write special value (0xcafe000a) to SW_RESET_CORE1 register
 */
#define __HAL_PMU_CMN_SOFTWARE_CORE1_RESET()  \
do { \
    IP_CMN_SYS->REG_SW_RESET_CFG0.bit.CMNSW1_2CMN_RST_EN = 0x1U; \
    IP_CMN_SYS->REG_SW_RESET_CORE1.all = 0xcafe000a; \
} while(0)


/**
 * @brief Initiate full chip software reset via AON controller.
 *
 * This macro triggers a complete system reset by writing a specific magic number
 * to the AON software reset register. It effectively resets all components of the chip.
 *
 * @details Single-step operation:
 *          Writes 0xCAFE000A to REG_AON_SW_RESET register
 */
#define __HAL_PMU_AON_SOFTWARE_RESET_FULL_CHIP() \
do { \
    IP_AON_CTRL->REG_AON_SW_RESET.all = 0xCAFE000A; \
} while(0)

/**
 * @brief Initiate watchdog timer 0 reset.
 *
 * Triggers a system reset specifically through Watchdog Timer 0 (WDT0) expiration path.
 * Simply enables the WDT0-to-CMN reset path in the reset configuration register.
 *
 * @details Single-step operation:
 *          Sets WDT0_2CMN_RST_EN bit in SW_RESET_CFG0 register
 */
#define __HAL_PMU_WDT0_RESET_CMN_ENABLE()  \
do { \
    IP_CMN_SYS->REG_SW_RESET_CFG0.bit.WDT0_2CMN_RST_EN = 0x1U; \
} while(0)

/**
 * @brief Initiate watchdog timer 1 reset.
 *
 * Similar to HAL_PMU_WDT0_RESET but uses Watchdog Timer 1 (WDT1) as reset source.
 * Enables corresponding bit in reset configuration register.
 *
 * @details Single-step operation:
 *          Sets WDT1_2CMN_RST_EN bit in SW_RESET_CFG0 register
 */
#define __HAL_PMU_WDT1_RESET_CMN_ENABLE()  \
do { \
    IP_CMN_SYS->REG_SW_RESET_CFG0.bit.WDT1_2CMN_RST_EN = 0x1U; \
} while(0)









/** @defgroup PMU_Exported_Functions_Macro PMU Exported Functions2
 * @{
 */
/**
 * @brief Enable reset for UART0.
 *
 * This macro sets the UART0_RESET bit in REG_SW_RESET_CFG2 register to 1,
 * which triggers a reset for UART0.
*/
#define __HAL_PMU_UART0_RST_ENABLE()    \
do { \
	IP_SYSCTRL->REG_SW_RESET_CFG2.bit.UART0_RESET = 0x1; \
} while(0)

/**
 * @brief Enable reset for UART1.
 *
 * This macro sets the UART1_RESET bit in REG_SW_RESET_CFG2 register to 1,
 * which triggers a reset for UART1.
 */
#define __HAL_PMU_UART1_RST_ENABLE()    \
do { \
	IP_SYSCTRL->REG_SW_RESET_CFG2.bit.UART1_RESET = 0x1; \
} while(0)

/**
 * @brief Enable reset for UART2.
 *
 * This macro sets the UART2_RESET bit in REG_SW_RESET_CFG2 register to 1,
 * which triggers a reset for UART2.
 */
#define __HAL_PMU_UART2_RST_ENABLE()    \
do { \
	IP_SYSCTRL->REG_SW_RESET_CFG2.bit.UART2_RESET = 0x1; \
} while(0)

/**
 * @brief Enable reset for SPI0.
 *
 * This macro sets the SPI0_RESET bit in REG_SW_RESET_CFG2 register to 1,
 * which triggers a reset for SPI0.
 */
#define __HAL_PMU_SPI0_RST_ENABLE()    \
do { \
	IP_SYSCTRL->REG_SW_RESET_CFG2.bit.SPI0_RESET = 0x1; \
} while(0)

/**
 * @brief Enable reset for SPI1.
 *
 * This macro sets the SPI1_RESET bit in REG_SW_RESET_CFG2 register to 1,
 * which triggers a reset for SPI1.
 */
#define __HAL_PMU_SPI1_RST_ENABLE()    \
do { \
	IP_SYSCTRL->REG_SW_RESET_CFG2.bit.SPI1_RESET = 0x1; \
} while(0)

/**
 * @brief Enable reset for I2C0.
 *
 * This macro sets the I2C0_RESET bit in REG_SW_RESET_CFG2 register to 1,
 * which triggers a reset for I2C0.
 */
#define __HAL_PMU_I2C0_RST_ENABLE()    \
do { \
  IP_SYSCTRL->REG_SW_RESET_CFG2.bit.I2C0_RESET = 0x1; \
} while(0)

/**
 * @brief Enable reset for I2C1.
 *
 * This macro sets the I2C1_RESET bit in REG_SW_RESET_CFG2 register to 1,
 * which triggers a reset for I2C1.
 */
#define __HAL_PMU_I2C1_RST_ENABLE()    \
do { \
	IP_SYSCTRL->REG_SW_RESET_CFG2.bit.I2C1_RESET = 0x1; \
} while(0)

/**
 * @brief Enable reset for IR.
 *
 * This macro sets the IR_RESET bit in REG_SW_RESET_CFG2 register to 1,
 * which triggers a reset for IR.
 */
#define __HAL_PMU_IR_RST_ENABLE()    \
do { \
	IP_SYSCTRL->REG_SW_RESET_CFG2.bit.IR_RESET = 0x1; \
} while(0)

 /**
  * @brief Enable reset for USB.
  *
  * This macro sets the USB bit in REG_SW_RESET_CFG2 register to 1,
  * which triggers a reset for USB.
  */
#define __HAL_PMU_USBC_RESET_ENABLE()	\
do{	\
 	IP_SYSCTRL->REG_SW_RESET_CFG2.bit.USBC_RESET = 0x1;	\
} while(0)

/**
* @brief Enable reset for GPT.
*
* This macro sets the GPT_RESET bit in REG_SW_RESET_CFG2 register to 1,
* which triggers a reset for GPT.
*/
#define __HAL_PMU_GPT_RST_ENABLE()    \
do { \
	IP_SYSCTRL->REG_SW_RESET_CFG2.bit.GPT_RESET = 0x1; \
} while(0)

/**
* @brief Enable reset for GPIO1.
*
* This macro sets the GPIO1_RESET bit in REG_SW_RESET_CFG2 register to 1,
* which triggers a reset for GPIO1.
*/
#define __HAL_PMU_GPIO1_RST_ENABLE()    \
do { \
	IP_SYSCTRL->REG_SW_RESET_CFG2.bit.GPIO1_RESET = 0x1; \
} while(0)

/**
* @brief Enable reset for GPIO0.
*
* This macro sets the GPIO0_RESET bit in REG_SW_RESET_CFG2 register to 1,
* which triggers a reset for GPIO0.
*/
#define __HAL_PMU_GPIO0_RST_ENABLE()    \
do { \
	IP_SYSCTRL->REG_SW_RESET_CFG2.bit.GPIO0_RESET = 0x1; \
} while(0)

/**
 * @brief Enable reset for PSRAM.
 *
 * This macro sets the PSRAM_RESET bit in REG_SW_RESET_CFG2 register to 1,
 * which triggers a reset for PSRAM.
 */
#define __HAL_PMU_PSRAM_RST_ENABLE()    \
do { \
  	IP_SYSCTRL->REG_SW_RESET_CFG2.bit.PSRAM_CTRL_RESET = 0x1; \
} while(0)

/**
* @brief Enable reset for FLASHC.
*
* This macro sets the FLASHC_RESET bit in REG_SW_RESET_CFG2 register to 1,
* which triggers a reset for FLASHC.
*/
#define __HAL_PMU_FLASHC_RST_ENABLE()    \
do { \
	IP_SYSCTRL->REG_SW_RESET_CFG2.bit.FLASH_CTRL_RESET = 0x1; \
} while(0)

/**
 * @brief Enable reset for GPADC.
 *
 * This macro sets the GPADC_RESET bit in REG_SW_RESET_CFG2 register to 1,
 * which triggers a reset for GPADC.
 */
#define __HAL_PMU_GPADC_RST_ENABLE()    \
do { \
	IP_SYSCTRL->REG_SW_RESET_CFG2.bit.GPADC_RESET = 0x1; \
} while(0)

/**
* @brief Enable reset for DMA.
*
* This macro sets the DMA_RESET bit in REG_SW_RESET_CFG2 register to 1,
* which triggers a reset for DMA.
*/
#define __HAL_PMU_DMA_RST_ENABLE()    \
do { \
	IP_SYSCTRL->REG_SW_RESET_CFG2.bit.CMNDMA_RESET = 0x1; \
} while(0)

/**
* @brief Enable reset for GPDMA2D.
*
* This macro sets the GPDMA2D_RESET bit in REG_SW_RESET_CFG2 register to 1,
* which triggers a reset for GPDMA2D.
*/
#define __HAL_PMU_GPDMA2D_RST_ENABLE()    \
do { \
  IP_SYSCTRL->REG_SW_RESET_CFG2.bit.GPDMA2D_RESET = 0x1; \
} while(0)

/**
* @brief Enable reset for APC.
*
* This macro sets the APC_REG_RESET bit in REG_SW_RESET_CFG2 register to 1,
* which triggers a reset for APC.
*/
#define __HAL_PMU_APC_RST_ENABLE()    \
do { \
	IP_SYSCTRL->REG_SW_RESET_CFG2.bit.APC_RESET = 0x1; \
} while(0)

/**
* @brief Enable reset for CODEC.
*
* This macro sets the CODEC_REG_RESET bit in REG_SW_RESET_CFG2 register to 1,
* which triggers a reset for CODEC.
*/
#define __HAL_PMU_CODEC_RST_ENABLE()    \
do { \
	IP_SYSCTRL->REG_SW_RESET_CFG2.bit.CODEC_RESET = 0x1; \
} while(0)

/**
* @brief Enable reset for I2S1.
*
* This macro sets the I2S1_RESET bit in REG_SW_RESET_CFG2 register to 1,
* which triggers a reset for I2S1.
*/
#define __HAL_PMU_I2S1_RST_ENABLE()    \
do { \
	IP_SYSCTRL->REG_SW_RESET_CFG2.bit.I2S1_RESET = 0x1; \
} while(0)

/**
* @brief Enable reset for I2S0.
*
* This macro sets the I2S0_RESET bit in REG_SW_RESET_CFG2 register to 1,
* which triggers a reset for I2S0.
*/
#define __HAL_PMU_I2S0_RST_ENABLE()    \
do { \
	IP_SYSCTRL->REG_SW_RESET_CFG2.bit.I2S0_RESET = 0x1; \
} while(0)

/**
* @brief Enable reset for DVP.
*
* This macro sets the DVP_RESET bit in REG_SW_RESET_CFG2 register to 1,
* which triggers a reset for DVP.
*/
#define __HAL_PMU_DVP_RST_ENABLE()    \
do { \
  IP_SYSCTRL->REG_SW_RESET_CFG2.bit.DVP_RESET = 0x1; \
} while(0)

/**
* @brief Enable reset for QSPI0.
*
* This macro sets the QSPI0_RESET bit in REG_SW_RESET_CFG2 register to 1,
* which triggers a reset for QSPI0.
*/
#define __HAL_PMU_QSPI0_RST_ENABLE()    \
do { \
  IP_SYSCTRL->REG_SW_RESET_CFG2.bit.QSPI0_RESET = 0x1; \
} while(0)

/**
* @brief Enable reset for QSPI1.
*
* This macro sets the QSPI1_RESET bit in REG_SW_RESET_CFG2 register to 1,
* which triggers a reset for QSPI1.
*/
#define __HAL_PMU_QSPI1_RST_ENABLE()    \
do { \
  IP_SYSCTRL->REG_SW_RESET_CFG2.bit.QSPI1_RESET = 0x1; \
} while(0)

/**
* @brief Enable reset for JPEG.
*
* This macro sets the JPEG_RESET bit in REG_SW_RESET_CFG2 register to 1,
* which triggers a reset for JPEG.
*/
#define __HAL_PMU_JPEG_RST_ENABLE()    \
do { \
  IP_SYSCTRL->REG_SW_RESET_CFG2.bit.JPEG_RESET = 0x1; \
} while(0)

/**
* @brief Enable reset for RGB.
*
* This macro sets the RGB_RESET bit in REG_SW_RESET_CFG2 register to 1,
* which triggers a reset for RGB.
*/
#define __HAL_PMU_RGB_RST_ENABLE()    \
do { \
  IP_SYSCTRL->REG_SW_RESET_CFG2.bit.RGB_RESET = 0x1; \
} while(0)

/**
* @brief Enable reset for SDIOH.
*
* This macro sets the SDIOH_RESET bit in REG_SW_RESET_CFG2 register to 1,
* which triggers a reset for SDIOH.
*/
#define __HAL_PMU_SDIOH_RST_ENABLE()    \
do { \
  IP_SYSCTRL->REG_SW_RESET_CFG2.bit.SDIOH_RESET = 0x1; \
} while(0)

/**
* @brief Enable reset for LUNA.
*
* This macro sets the LUNA_RESET bit in REG_SW_RESET_CFG2 register to 1,
* which triggers a reset for LUNA.
*/
#define __HAL_PMU_LUNA_RST_ENABLE()    \
do { \
  IP_SYSCTRL->REG_SW_RESET_CFG2.bit.LUNA_RESET = 0x1; \
} while(0)

/**
* @brief Enable reset for I8080.
*
* This macro sets the I8080_RESET bit in REG_SW_RESET_CFG2 register to 1,
* which triggers a reset for I8080.
*/
#define __HAL_PMU_I8080_RST_ENABLE()    \
do { \
  IP_SYSCTRL->REG_SW_RESET_CFG2.bit.I8080_RESET = 0x1; \
} while(0)

/**
* @brief Enable reset for PSRAM_CTRL_PRESET.
*
* This macro sets the PSRAM_CTRL_PRESET_RESET bit in REG_SW_RESET_CFG2 register to 1,
* which triggers a reset for PSRAM_CTRL_PRESET.
*/
#define __HAL_PMU_PSRAM_CTRL_PRESET_RST_ENABLE()    \
do { \
  IP_SYSCTRL->REG_SW_RESET_CFG2.bit.PSRAM_CTRL_PRESET = 0x1; \
} while(0)

/**
* @brief Enable reset for FLASH_CTRL_PRESET.
*
* This macro sets the FLASH_CTRL_PRESET_RESET bit in REG_SW_RESET_CFG2 register to 1,
* which triggers a reset for FLASH_CTRL_PRESET.
*/
#define __HAL_PMU_FLASH_CTRL_PRESET_RST_ENABLE()    \
do { \
  IP_SYSCTRL->REG_SW_RESET_CFG2.bit.FLASH_CTRL_PRESET = 0x1; \
} while(0)

/**
* @brief Enable reset for VIC_OUT.
*
* This macro sets the VIC_OUT_RESET bit in REG_SW_RESET_CFG2 register to 1,
* which triggers a reset for VIC_OUT.
*/
#define __HAL_PMU_VIC_OUT_RST_ENABLE()    \
do { \
  IP_SYSCTRL->REG_SW_RESET_CFG2.bit.VIC_OUT_RESET = 0x1; \
} while(0)

/*
 * @brief Enable reset for Keysense.
 *
 * This macro sets the KEYSENSE1_RESET bit in REG_AON_RST_CTRL register to 1,
 * which triggers a reset for Keysense.
 */
#define __HAL_PMU_KEYSENSE_RST_ENABLE()    \
do { \
	IP_AON_CTRL->REG_AON_RST_CTRL.bit.KEYSENSE_RESET = 0x1; \
} while(0)


/*
 * @brief Enable reset for AON TIMER.
 *
 * This macro sets the AON_TIMER_RESET bit in REG_AON_RST_CTRL register to 1,
 * which triggers a reset for AON TIMER.
 */
#define __HAL_PMU_AON_TIMER_ENABLE()    \
do { \
	IP_AON_CTRL->REG_AON_RST_CTRL.bit.AON_TIMER_RESET = 0x1; \
} while(0)


/*
 * @brief Enable reset for EFUSE.
 *
 * This macro sets the EFUSE_RESET bit in REG_AON_RST_CTRL register to 1,
 * which triggers a reset for AON IOMUX.
 */
#define __HAL_PMU_EFUSE_RST_ENABLE()    \
do { \
	IP_AON_CTRL->REG_AON_RST_CTRL.bit.EFUSE_RESET = 0x1; \
} while(0)


/*
 * @brief Enable reset for CALENDAR.
 *
 * This macro sets the CALENDAR_RESET bit in REG_AON_RST_CTRL register to 1,
 * which triggers a reset for CALENDAR.
 */
#define __HAL_PMU_CALENDAR_RST_ENABLE()    \
do { \
	IP_AON_CTRL->REG_AON_RST_CTRL.bit.CALENDAR_RESET = 0x1; \
} while(0)

/*
 * @brief Enable XO24M.
 *
 * This macro enables the XO24M by setting the PW_MODE_24M_FORCE_ON bit in the
 * REG_POWER_WKUP_CTRL0 register in powermode2.
 */
#define __HAL_PMU_XO24M_ENABLE()    \
do { \
	IP_AON_CTRL->REG_POWER_WKUP_CTRL0.bit.PW_MODE_24M_FORCE_ON = 0x1; \
} while(0)

/**
 * @brief Disable XO24M.
 *
 * This macro disable the XO24M by setting the PW_MODE_24M_FORCE_ON bit in the
 * REG_POWER_WKUP_CTRL0 register in powermode2.
 */
#define __HAL_PMU_XO24M_DISABLE()    \
do { \
	IP_AON_CTRL->REG_POWER_WKUP_CTRL0.bit.PW_MODE_24M_FORCE_ON = 0x0; \
} while(0)

/**
 * @brief Enable RC32K.
 *
 * This macro enables the RC32K oscillator by resetting the PD_RCO32K_IN_PW_MODE3
 * in the REG_POWER_WKUP_CTRL0 registers in powermode3.
 */
#define __HAL_PMU_RC32K_ENABLE()    \
do { \
	IP_AON_CTRL->REG_POWER_WKUP_CTRL0.bit.PD_RCO32K_IN_PW_MODE3 = 0x1; \
} while(0)

/**
 * @brief Disable RC32K.
 *
 * This macro disalbes the RC32K oscillator by resetting the PD_RCO32K_IN_PW_MODE3
 * in the REG_POWER_WKUP_CTRL0 registers in powermode3.
 */
#define __HAL_PMU_RC32K_DISABLE()    \
do { \
	IP_AON_CTRL->REG_POWER_WKUP_CTRL0.bit.PD_RCO32K_IN_PW_MODE3 = 0x0; \
} while(0)

/** @} */ /* End of group PMU_Exported_Functions */

/** @} */ /* End of group PMU */


#ifdef __cplusplus
}
#endif

#endif /* __CSK_DRIVER_PMU_H */

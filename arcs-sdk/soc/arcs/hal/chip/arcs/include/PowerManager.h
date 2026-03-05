/**
  ******************************************************************************
  * @file    Driver_PMU.h
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
#include "arcs_ap.h"

/** @addtogroup CSK_HAL_Driver
  * @{
  */

/** @addtogroup PMU
  * @{
  */


/* Exported types ------------------------------------------------------------*/
/** @defgroup PMU_Exported_Types PMU Exported Types
  * @{
  */
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
    PMU_WAKEUP_KEY0,
    PMU_WAKEUP_KEY1,
	PMU_WAKEUP_RTC,
	PMU_WAKEUP_BT,
	PMU_WAKEUP_WIFI,
    PMU_WAKEUP_GPIOB_00 = 16U,
    PMU_WAKEUP_GPIOB_01,
    PMU_WAKEUP_GPIOB_02,
    PMU_WAKEUP_GPIOB_03,
    PMU_WAKEUP_GPIOB_04,
    PMU_WAKEUP_GPIOB_05,
    PMU_WAKEUP_GPIOB_06,
    PMU_WAKEUP_GPIOB_07,
    PMU_WAKEUP_GPIOB_08,
    PMU_WAKEUP_GPIOB_09,
    PMU_WAKEUP_NONE = 0xFFU,
}pmu_wakeupsrc_t;


/**
 * @brief CP: total 256K ram, 32K/bank, 0x2000_0000 -- 0x2003_FFFF
 * 		  WIFI SUB: total 256K ram, 32K/bank, 0x2004_0000 -- 0x2007_FFFF
 * 		  BT: 32K RAM, 0x200C_0000 -- 0x200C_8000
 */
typedef enum _pmu_rambank {
    PMU_WIFI_SUB_RAMBANK0 = 0U,
	PMU_WIFI_SUB_RAMBANK1,
	PMU_WIFI_SUB_RAMBANK2,
	PMU_WIFI_SUB_RAMBANK3,
	PMU_WIFI_SUB_RAMBANK4,
	PMU_WIFI_SUB_RAMBANK5,
	PMU_WIFI_SUB_RAMBANK6,
	PMU_WIFI_SUB_RAMBANK7,
	PMU_WIFI_KEY_RAMBANK,
	PMU_BT_RAMBANK0,
    PMU_CP_RAMBANK0,
	PMU_CP_RAMBANK1,
	PMU_RAMBANK_NONE = 0xFFU,
}pmu_rambank_t;


typedef enum _pmu_rstsrc {
    PMU_RST_POR = 0U,
    PMU_RST_AON = 1U,
	PMU_RST_CP_WDT = 16U,
	PMU_RST_CMN,
	PMU_RST_CP_SW,
	PMU_RST_AP_SW_WDT,
    PMU_RST_NONE = 0xFFU,
}pmu_rstsrc_t;


typedef enum _pmu_gpio_src {
    PMU_POLARITY_GPIOB_00 = 0x0U,
    PMU_POLARITY_GPIOB_01,
    PMU_POLARITY_GPIOB_02,
    PMU_POLARITY_GPIOB_03,
    PMU_POLARITY_GPIOB_04,
    PMU_POLARITY_GPIOB_05,
    PMU_POLARITY_GPIOB_06,
    PMU_POLARITY_GPIOB_07,
    PMU_POLARITY_GPIOB_08,
    PMU_POLARITY_GPIOB_09,
}pmu_gpio_src_t;

typedef enum _pmu_sleep_trigger {
	PMU_SLEEP_NONE = 0U,
	PMU_SLEEP_CMD_BY_AP,
	PMU_SLEEP_CMD_BY_CP,
	PMU_SLEEP_AP_AND_CP
}pmu_sleep_trigger_t;

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
/** @addtogroup PMU_Exported_Functions PMU Exported Functions
  * @{
  */

/** @addtogroup PMU_Exported_Functions_Group2 Peripheral Control functions
  * @{
  */


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


/**
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
 * @brief Enables RAM retention for the specified RAM bank.
 *
 * This function enables retention for a specific RAM bank to preserve its content
 * during low-power modes. It is designed to support CPU RAM banks (PMU_CP_RAMBANK0 to PMU_CP_RAMBANK1),
 * Wi-Fi subsystem RAM banks (PMU_WIFI_SUB_RAMBANK0 to PMU_WIFI_SUB_RAMBANK7), and the Bluetooth RAM bank (PMU_BT_RAMBANK0).
 * The function sets the appropriate bit(s) in the PMU control registers to activate retention for the selected bank.
 *
 * @param RamBank The RAM bank for which to enable retention. This should be one of the
 *                values defined in the pmu_rambank_t enumeration, which includes CPU RAM banks,
 *                Wi-Fi subsystem RAM banks, the Bluetooth RAM bank, and an option to represent 'none'.
 *
 * @note This function performs no action if an invalid RAM bank is specified. For CPU and Wi-Fi RAM banks,
 *       the function modifies the RAM_RETENTION_SEL_L bits in the RAM_RETENTION_SEL register. The PMU_RAMBANK_NONE
 *       value is considered invalid for enabling RAM retention.
 */
void HAL_PMU_EnableRamRetention(pmu_rambank_t RamBank);


/**
 * @brief Disables RAM retention for the specified RAM bank.
 *
 * This function disables retention for a specified RAM bank, allowing the RAM content to be not preserved
 * during low-power modes. It supports disabling retention for CPU RAM banks (PMU_CP_RAMBANK0 to PMU_CP_RAMBANK7),
 * Wi-Fi subsystem RAM banks (PMU_WIFI_SUB_RAMBANK0 to PMU_WIFI_SUB_RAMBANK7), and the Bluetooth RAM bank (PMU_BT_RAMBANK0).
 * The function clears the appropriate bit(s) in the PMU control registers to deactivate retention for the selected bank.
 *
 * @param RamBank The RAM bank for which to disable retention. This should be one of the
 *                values defined in the pmu_rambank_t enumeration, which includes CPU RAM banks,
 *                Wi-Fi subsystem RAM banks, the Bluetooth RAM bank, and an option to represent 'none'.
 *
 * @note This function performs no action if an invalid RAM bank is specified. For CPU and Wi-Fi RAM banks,
 *       the function modifies the RAM_RETENTION_SEL_L bits in the REG_PMU_CTRL3 register to clear the retention setting.
 *       For the Bluetooth RAM bank, it clears a specific bit (bit 28) in the REG_PMU_CTRL4 register. The PMU_RAMBANK_NONE
 *       value is considered invalid for disabling RAM retention.
 */
void HAL_PMU_DisableRamRetention(pmu_rambank_t RamBank);


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



/**
 * @brief Enter Hold Mode.
 *
 * This function puts the system into Hold mode. It chooses between Wait For Interrupt (WFI)
 * and Wait For Event (WFE) to enter sleep mode based on the SLEEPEntry parameter.
 *
 * @param SLEEPEntry Determines the method to enter sleep mode. Use PMU_HOLDENTRY_WFI for
 *        Wait For Interrupt, or other values for Wait For Event.
 */
void HAL_PMU_EnterHoldMode(uint8_t SLEEPEntry);

/**
 * @brief Configure PMU sleep trigger conditions.
 *
 * This function sets the sleep trigger conditions in the PMU by configuring
 * the appropriate bits in the power wakeup control register.
 *
 * @param sleepTrigger Specifies the trigger source for sleep mode:
 *                     - PMU_SLEEP_NONE: No sleep
 *                     - PMU_SLEEP_CMD_BY_CP: CP triggers sleep, masking AP
 *                     - PMU_SLEEP_CMD_BY_AP: AP triggers sleep, masking CP
 *                     - PMU_SLEEP_AP_AND_CP: Both AP and CP enter sleep
 */
void HAL_PMU_PreConfigSleepTrigger(pmu_sleep_trigger_t sleepTrigger);


void HAL_PMU_ConfigDeepSleepMode(pmu_sleepmode_t SleepMode, uint8_t SLEEPEntry);
/**
 * @brief Enter the chip into deep sleep mode.
 *
 * This function configures the system to enter deep sleep mode based on the specified
 * power mode, sleep entry method, and sleep target. It allows selective masking
 * of AP and/or CP cores for sleep operations.
 *
 * @param SleepMode Power mode for deep sleep (e.g., PMU_SLEEPMODE_MODE1, PMU_SLEEPMODE_MODE2).
 * @param SLEEPEntry Sleep entry method:
 *                   - PMU_DEEPSLEEPENTRY_WFI: Use WFI instruction to enter sleep.
 *                   - PMU_DEEPSLEEPENTRY_WFE: Use WFE instruction to enter sleep.
 * @param sleepTarget Target cores for sleep:
 *                    - PMU_SLEEP_NONE: No cores are masked from sleep.
 *                    - PMU_SLEEP_AP: Only mask AP core for sleep.
 *                    - PMU_SLEEP_CP: Only mask CP core for sleep.
 *                    - PMU_SLEEP_AP_AND_CP: Mask both AP and CP cores for sleep.
 *
 * @return None.
 *
 * @note Ensure that the required conditions for entering deep sleep mode are met
 *       before calling this function. The function automatically handles enabling
 *       power mode and deep sleep settings.
 *
 * @example
 * // Example usage: Enter deep sleep mode with AP masked and WFI entry.
 * HAL_PMU_EnterDeepSleepMode(PMU_SLEEPMODE_MODE2, PMU_DEEPSLEEPENTRY_WFI, PMU_SLEEP_AP);
 */
void HAL_PMU_EnterDeepSleepMode(pmu_sleepmode_t SleepMode, uint8_t SLEEPEntry);


/**
 * @brief Enable reset for UART0.
 *
 * This macro sets the UART0_RESET bit in REG_SW_RESET_CP2 register to 1,
 * which triggers a reset for UART0.
 */
#define __HAL_PMU_UART0_RST_ENABLE()    \
do { \
	IP_SYSCTRL->REG_SW_RESET_CP2.bit.UART0_RESET = 0x1; \
} while(0)


/**
 * @brief Enable reset for UART1.
 *
 * This macro sets the UART1_RESET bit in REG_SW_RESET_CP2 register to 1,
 * which triggers a reset for UART1.
 */
#define __HAL_PMU_UART1_RST_ENABLE()    \
do { \
	IP_SYSCTRL->REG_SW_RESET_CP2.bit.UART1_RESET = 0x1; \
} while(0)


/**
 * @brief Enable reset for UART2.
 *
 * This macro sets the UART2_RESET bit in REG_SW_RESET_CP2 register to 1,
 * which triggers a reset for UART2.
 */
#define __HAL_PMU_UART2_RST_ENABLE()    \
do { \
	IP_SYSCTRL->REG_SW_RESET_CP2.bit.UART2_RESET = 0x1; \
} while(0)


/**
 * @brief Enable reset for IR.
 *
 * This macro sets the IR_RESET bit in REG_SW_RESET_CP2 register to 1,
 * which triggers a reset for IR.
 */
#define __HAL_PMU_IR_RST_ENABLE()    \
do { \
	IP_SYSCTRL->REG_SW_RESET_CP2.bit.IR_RESET = 0x1; \
} while(0)


/**
 * @brief Enable reset for SPI0.
 *
 * This macro sets the SPI0_RESET bit in REG_SW_RESET_CP2 register to 1,
 * which triggers a reset for SPI0.
 */
#define __HAL_PMU_SPI0_RST_ENABLE()    \
do { \
	IP_SYSCTRL->REG_SW_RESET_CP2.bit.SPI0_RESET = 0x1; \
} while(0)


/**
 * @brief Enable reset for SPI1.
 *
 * This macro sets the SPI1_RESET bit in REG_SW_RESET_CP2 register to 1,
 * which triggers a reset for SPI1.
 */
#define __HAL_PMU_SPI1_RST_ENABLE()    \
do { \
	IP_SYSCTRL->REG_SW_RESET_CP2.bit.SPI1_RESET = 0x1; \
} while(0)


/**
 * @brief Enable reset for SPI2.
 *
 * This macro sets the SPI1_RESET bit in REG_SW_RESET_CP2 register to 1,
 * which triggers a reset for SPI2.
 */
#define __HAL_PMU_SPI2_RST_ENABLE()    \
do { \
	IP_SYSCTRL->REG_SW_RESET_CP2.bit.SPI2_RESET = 0x1; \
} while(0)


/**
 * @brief Enable reset for I2C0.
 *
 * This macro sets the I2C0_RESET bit in REG_SW_RESET_CP2 register to 1,
 * which triggers a reset for I2C0.
 */
#define __HAL_PMU_I2C0_RST_ENABLE()    \
do { \
	IP_SYSCTRL->REG_SW_RESET_CP2.bit.I2C0_RESET = 0x1; \
} while(0)


/**
 * @brief Enable reset for I2C1.
 *
 * This macro sets the I2C1_RESET bit in REG_SW_RESET_CP2 register to 1,
 * which triggers a reset for I2C1.
 */
#define __HAL_PMU_I2C1_RST_ENABLE()    \
do { \
	IP_SYSCTRL->REG_SW_RESET_CP2.bit.I2C1_RESET = 0x1; \
} while(0)


/**
 * @brief Enable reset for GPIO0.
 *
 * This macro sets the GPIO0_RESET bit in REG_SW_RESET_CP2 register to 1,
 * which triggers a reset for GPIO0.
 */
#define __HAL_PMU_GPIO0_RST_ENABLE()    \
do { \
	IP_SYSCTRL->REG_SW_RESET_CP2.bit.GPIO0_RESET = 0x1; \
} while(0)


/**
 * @brief Enable reset for GPIO1.
 *
 * This macro sets the GPIO1_RESET bit in REG_SW_RESET_CP2 register to 1,
 * which triggers a reset for GPIO1.
 */
#define __HAL_PMU_GPIO1_RST_ENABLE()    \
do { \
	IP_SYSCTRL->REG_SW_RESET_CP2.bit.GPIO1_RESET = 0x1; \
} while(0)


/**
 * @brief Enable reset for GPADC.
 *
 * This macro sets the GPADC_RESET bit in REG_SW_RESET_CP2 register to 1,
 * which triggers a reset for GPADC.
 */
#define __HAL_PMU_GPADC_RST_ENABLE()    \
do { \
	IP_SYSCTRL->REG_SW_RESET_CP2.bit.GPADC_RESET = 0x1; \
} while(0)


/**
 * @brief Enable reset for DMA.
 *
 * This macro sets the DMA_RESET bit in REG_SW_RESET_CP2 register to 1,
 * which triggers a reset for DMA.
 */
#define __HAL_PMU_DMA_RST_ENABLE()    \
do { \
	IP_SYSCTRL->REG_SW_RESET_CP2.bit.DMA_RESET = 0x1; \
} while(0)


/**
 * @brief Enable reset for FLASHC.
 *
 * This macro sets the FLASHC_RESET bit in REG_SW_RESET_CP2 register to 1,
 * which triggers a reset for FLASHC.
 */
#define __HAL_PMU_FLASHC_RST_ENABLE()    \
do { \
	IP_SYSCTRL->REG_SW_RESET_CP2.bit.FLASH_CTRL_RESET = 0x1; \
} while(0)


/**
 * @brief Enable reset for GPT.
 *
 * This macro sets the GPT_RESET bit in REG_SW_RESET_CP2 register to 1,
 * which triggers a reset for GPT.
 */
#define __HAL_PMU_GPT_RST_ENABLE()    \
do { \
	IP_SYSCTRL->REG_SW_RESET_CP2.bit.GPT_RESET = 0x1; \
} while(0)


/**
 * @brief Enable reset for I2S.
 *
 * This macro sets the I2S_RESET bit in REG_SW_RESET_CP2 register to 1,
 * which triggers a reset for I2S.
 */
#define __HAL_PMU_I2S_RST_ENABLE()    \
do { \
	IP_SYSCTRL->REG_SW_RESET_CP2.bit.TRNG_RESET = 0x1; \
} while(0)


/**
 * @brief Enable reset for CODEC.
 *
 * This macro sets the CODEC_REG_RESET bit in REG_SW_RESET_CP2 register to 1,
 * which triggers a reset for CODEC.
 */
#define __HAL_PMU_CODEC_RST_ENABLE()    \
do { \
	IP_SYSCTRL->REG_SW_RESET_CP2.bit.TRNG_RESET = 0x1; \
} while(0)


/**
 * @brief Enable reset for CODEC_ASYNC.
 *
 * This macro sets the CODEC_ASYNC_RESET bit in REG_SW_RESET_CP2 register to 1,
 * which triggers a reset for CODEC_ASYNC.
 */
#define __HAL_PMU_CODEC_ASYNC_RST_ENABLE()    \
do { \
	IP_SYSCTRL->REG_SW_RESET_CP2.bit.TRNG_RESET = 0x1; \
} while(0)


/**
 * @brief Enable reset for APC.
 *
 * This macro sets the APC_REG_RESET bit in REG_SW_RESET_CP2 register to 1,
 * which triggers a reset for APC.
 */
#define __HAL_PMU_APC_REG_RST_ENABLE()    \
do { \
	IP_SYSCTRL->REG_SW_RESET_CP2.bit.TRNG_RESET = 0x1; \
} while(0)


/**
 * @brief Enable reset for APC_CORE.
 *
 * This macro sets the APC_CORE_RESET bit in REG_SW_RESET_CP2 register to 1,
 * which triggers a reset for APC_CORE.
 */
#define __HAL_PMU_APC_CORE_RST_ENABLE()    \
do { \
	IP_SYSCTRL->REG_SW_RESET_CP2.bit.TRNG_RESET = 0x1; \
} while(0)


/**
 * @brief Enable reset for MCU_CORE.
 *
 * This macro sets the MCU_CORE_RESET bit in REG_SW_RESET_CP2 register to 1,
 * which triggers a reset for MCU_CORE.
 */
#define __HAL_PMU_MCU_CORE_RST_ENABLE()    \
do { \
	IP_SYSCTRL->REG_SW_RESET_CP2.bit.TRNG_RESET = 0x1; \
} while(0)


/**
 * @brief Enable reset for LUNA.
 *
 * This macro sets the LUNA_RESET bit in REG_SW_RESET_CP2 register to 1,
 * which triggers a reset for LUNA.
 */
#define __HAL_PMU_LUNA_RST_ENABLE()    \
do { \
	IP_SYSCTRL->REG_SW_RESET_CP2.bit.TRNG_RESET = 0x1; \
} while(0)



/**
 * @brief Enable reset for ap core and subsystem.
 *
 */
#define __HAL_PMU_AP_RST_ENABLE()    \
do { \
	IP_AP_CFG->REG_SW_RESET.bit.APSW2APSOC_RST_EN = 0x1; \
	IP_AP_CFG->REG_AP_RESET.bit.AP_RESET = 0x1;  \
} while(0)

/**
 * @brief Enable reset for cp core and subsystem.
 *
 */
#define __HAL_PMU_CP_RST_ENABLE()    \
do { \
	IP_SYSCTRL->REG_SW_RESET_CP1.bit.CMNSW2AP_RST_EN = 0x1; \
	IP_SYSCTRL->REG_SW_RESET_CP0.all = 0xCAFE000A;  \
} while(0)

/**
 * @brief Enable reset for while chip.
 *
 */
#define __HAL_PMU_WholeChip_RST_ENABLE()    \
do { \
	IP_SYSCTRL->REG_SW_RESET_CP1.bit.CMNSW2AP_RST_EN = 0x1; \
	IP_SYSCTRL->REG_SW_RESET_CP1.bit.CMNSW2CP_RST_EN = 0x1; \
	IP_SYSCTRL->REG_SW_RESET_CP1.bit.CMNSW2CMN_RST_EN = 0x1; \
	IP_SYSCTRL->REG_SW_RESET_CP0.all = 0xCAFE000A;  \
} while(0)


/**
 * @brief Enable reset for AON TIMER.
 *
 * This macro sets the AON_TIMER_RESET bit in REG_AON_RST_CTRL register to 1,
 * which triggers a reset for AON TIMER.
 */
#define __HAL_PMU_AON_TIMER_ENABLE()    \
do { \
	IP_AON_CTRL->REG_AON_RST_CTRL.bit.AON_TIMER_RESET = 0x1; \
} while(0)


/**
 * @brief Enable reset for Keysense.
 *
 * This macro sets the KEYSENSE0_RESET bit in REG_AON_RST_CTRL register to 1,
 * which triggers a reset for Keysense.
 */
#define __HAL_PMU_KEYSENSE_ENABLE()    \
do { \
	IP_AON_CTRL->REG_AON_RST_CTRL.bit.KEYSENSE0_RESET = 0x1; \
} while(0)


/**
 * @brief Enable reset for Keysense.
 *
 * This macro sets the KEYSENSE1_RESET bit in REG_AON_RST_CTRL register to 1,
 * which triggers a reset for Keysense.
 */
#define __HAL_PMU_KEYSENSE1_ENABLE()    \
do { \
	IP_AON_CTRL->REG_AON_RST_CTRL.bit.KEYSENSE1_RESET = 0x1; \
} while(0)


/**
 * @brief Enable reset for AON IOMUX.
 *
 * This macro sets the AON_IOMUX_RESET bit in REG_AON_RST_CTRL register to 1,
 * which triggers a reset for AON IOMUX.
 */
#define __HAL_PMU_AON_IOMUX_ENABLE()    \
do { \
	IP_AON_CTRL->REG_AON_RST_CTRL.bit.AON_IOMUX_RESET = 1; \
} while(0)


/**
 * @brief Enable reset for EFUSE.
 *
 * This macro sets the EFUSE_RESET bit in REG_AON_RST_CTRL register to 1,
 * which triggers a reset for AON IOMUX.
 */
#define __HAL_PMU_EFUSE_ENABLE()    \
do { \
	IP_AON_CTRL->REG_AON_RST_CTRL.bit.EFUSE_RESET = 1; \
} while(0)


/**
 * @brief Enable reset for CALENDAR.
 *
 * This macro sets the CALENDAR_RESET bit in REG_AON_RST_CTRL register to 1,
 * which triggers a reset for CALENDAR.
 */
#define __HAL_PMU_CALENDAR_ENABLE()    \
do { \
	IP_AON_CTRL->REG_AON_RST_CTRL.bit.CALENDAR_RESET = 1; \
} while(0)


/**
 * @brief Enable LDO for the core.
 *
 * This macro enables the LDO for the core by setting the EN_LDO_CORE_FORCEDATA and
 * EN_LDO_CORE_FORCE bits in the REG_AON_FRC_CTRL1 registers.
 */
#define __HAL_PMU_LDO_CORE_ENABLE()    \
do { \
	IP_AON_CTRL->REG_AON_FRC_CTRL1.bit.EN_LDO_CORE_FORCEDATA = 0x1; \
	IP_AON_CTRL->REG_AON_FRC_CTRL1.bit.EN_LDO_CORE_FORCE = 0x1; \
} while(0)


/**
 * @brief Disable LDO for the core.
 *
 * This macro disables the LDO for the core by setting the EN_LDO_CORE_FORCEDATA and
 * EN_LDO_CORE_FORCE bits in the REG_AON_FRC_CTRL1 registers.
 */
#define __HAL_PMU_LDO_CORE_DISABLE()    \
do { \
	IP_AON_CTRL->REG_AON_FRC_CTRL1.bit.EN_LDO_CORE_FORCEDATA = 0x0; \
	IP_AON_CTRL->REG_AON_FRC_CTRL1.bit.EN_LDO_CORE_FORCE = 0x1; \
} while(0)


/**
 * @brief Enable UVLO for VIO.
 *
 * This macro enables the Under-Voltage Lock-Out (UVLO) for VIO by setting the
 * EN_UVLO_FORCEDATA and EN_LDO_CORE_FORCE bits in the REG_AON_FORCE_DATA and
 * REG_AON_FRC_CTRL1 registers.
 */
#define __HAL_PMU_UVLO_VIO_ENABLE()    \
do { \
	IP_AON_CTRL->REG_AON_FRC_CTRL1.bit.EN_UVLO_FORCEDATA = 0x1; \
	IP_AON_CTRL->REG_AON_FRC_CTRL1.bit.EN_LDO_CORE_FORCE = 0x1; \
} while(0)

/**
 * @brief Enable DET_VBAT.
 *
 * This macro enables DET_VBAT by setting the EN_DET_VBAT_FORCEDATA and EN_DET_VBAT_FORCE bit in the
 * REG_AON_FRC_CTRL1 register.
 */
#define __HAL_PMU_DET_VBAT_ENABLE()    \
do { \
	IP_AON_CTRL->REG_AON_FRC_CTRL1.bit.EN_DET_VBAT_FORCEDATA = 0x1; \
	IP_AON_CTRL->REG_AON_FRC_CTRL1.bit.EN_DET_VBAT_FORCE = 0x1; \
} while(0)


/**
 * @brief Disable DET_VBAT.
 *
 * This macro enables DET_VBAT by setting the EN_DET_VBAT_FORCEDATA and EN_DET_VBAT_FORCE bit in the
 * REG_AON_FRC_CTRL1 register.
 */
#define __HAL_PMU_DET_VBAT_DISABLE()    \
do { \
	IP_AON_CTRL->REG_AON_FRC_CTRL1.bit.EN_DET_VBAT_FORCEDATA = 0x0; \
	IP_AON_CTRL->REG_AON_FRC_CTRL1.bit.EN_DET_VBAT_FORCE = 0x1; \
} while(0)


/**
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
	IP_AON_CTRL->REG_POWER_WKUP_CTRL0.bit.PD_RCO32K_IN_PW_MODE3 = 0x0; \
} while(0)

/**
 * @brief Disable RC32K.
 *
 * This macro disalbes the RC32K oscillator by resetting the PD_RCO32K_IN_PW_MODE3
 * in the REG_POWER_WKUP_CTRL0 registers in powermode3.
 */
#define __HAL_PMU_RC32K_DISABLE()    \
do { \
	IP_AON_CTRL->REG_POWER_WKUP_CTRL0.bit.PD_RCO32K_IN_PW_MODE3 = 0x1; \
} while(0)


/**
  * @}
  */


/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

#ifdef __cplusplus
}
#endif

#endif /* __CSK_DRIVER_PMU_H */

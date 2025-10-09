/**
  ******************************************************************************
  * @file    PowerManager.c
  * @author  ListenAI Application Team
  * @brief   Power Manager HAL module driver.
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
#include "PowerManager.h"


/**********************************SRC************************************/

/**
 * @brief Get the system reset cause.
 *
 * This function retrieves the system reset cause from the AON status register
 * and then clears the reset cause.
 *
 * @return The reset source as defined in pmu_rstsrc_t.
 */
pmu_rstsrc_t HAL_PMU_GetSysResetCause(void){
	uint32_t rstCause;
	rstCause = IP_AON_CTRL->REG_SYSRST_STATUS.all;
    //clear reset cause
    IP_AON_CTRL->REG_SYSRST_STATUS.all = rstCause;

    if (rstCause & (1 << PMU_RST_POR)) return PMU_RST_POR;
    if (rstCause & (1 << PMU_RST_AON)) return PMU_RST_AON;
    if (rstCause & (1 << PMU_RST_CP_WDT)) return PMU_RST_CP_WDT;
    if (rstCause & (1 << PMU_RST_CMN)) return PMU_RST_CMN;
    if (rstCause & (1 << PMU_RST_CP_SW)) return PMU_RST_CP_SW;
    if (rstCause & (1 << PMU_RST_AP_SW_WDT)) return PMU_RST_AP_SW_WDT;

    return PMU_RST_NONE;
}


/**
 * @brief Get the wake-up cause.
 *
 * This function checks the PMU wake-up source register and returns the first
 * set wake-up source it finds.
 *
 * @return The wake-up source as defined in pmu_wakeupsrc_t.
 *         Returns PMU_WAKEUP_NONE if no source is found.
 */
pmu_wakeupsrc_t HAL_PMU_GetWakeUpCause(void){
	uint32_t wakeupCause;

	wakeupCause = IP_AON_CTRL->REG_WAKEUP_ISR.all;
	//clear wakeup cause
	IP_AON_CTRL->REG_WAKEUP_ICR.all = wakeupCause;
	if(wakeupCause != 0) {
	    for (pmu_wakeupsrc_t src = PMU_WAKEUP_TIMER; src <= PMU_WAKEUP_GPIOB_09; src++) {
	        if (wakeupCause & (1 << src)) {
	            return src;
	        }
	    }
	}

	return PMU_WAKEUP_NONE;
}


/**
 * @brief Clear the system reset cause.
 *
 * This function clears the system reset cause in the REG_SYSRST_STATUS register.
 */
void HAL_PMU_ClearSysResetCause(void){
    IP_AON_CTRL->REG_SYSRST_STATUS.all = 0xFFFFFFFF;
}


/**
 * @brief Clear the wake-up cause.
 *
 * This function clears the wake-up cause in the PMU wake-up IRQ clear register.
 */
void HAL_PMU_ClearWakeUpCause(void){
    IP_AON_CTRL->REG_WAKEUP_ICR.all = 0xFFFFFFFF;
}



/**
 * @brief Enable a specific wake-up source.
 *
 * This function enables a specified wake-up source in the PMU enable wake-up register.
 *
 * @param WakeUpSrc The wake-up source to enable, as defined in pmu_wakeupsrc_t.
 */
void HAL_PMU_EnableWakeUpSrc(pmu_wakeupsrc_t WakeUpSrc)
{
    IP_AON_CTRL->REG_WAKEUP_ENABLE.all |= (1 << WakeUpSrc);
}


/**
 * @brief Disable a specific wake-up source.
 *
 * This function disables a specified wake-up source in the PMU enable wake-up register.
 *
 * @param WakeUpSrc The wake-up source to disable.
 */
void HAL_PMU_DisableWakeUpSrc(pmu_wakeupsrc_t WakeUpSrc)
{
    IP_AON_CTRL->REG_WAKEUP_ENABLE.all &= (~(1 << WakeUpSrc));

}


/**
 * @brief Enable IRQ for a specific wake-up source.
 *
 * This function enables the interrupt for a specified wake-up source in the
 * PMU enable wake-up IRQ register.
 *
 * @param WakeUpSrc The wake-up source for which to enable the IRQ, as defined in pmu_wakeupsrc_t.
 */
void HAL_PMU_EnableWakeUpSrcIrq(pmu_wakeupsrc_t WakeUpSrc)
{
    IP_AON_CTRL->REG_WAKEUP_IMR.all &= (~(1 << WakeUpSrc));
}


/**
 * @brief Disable IRQ for a specific wake-up source.
 *
 * This function disables the interrupt for a specified wake-up source in the
 * PMU enable wake-up IRQ register.
 *
 * @param WakeUpSrc The wake-up source for which to enable the IRQ, as defined in pmu_wakeupsrc_t.
 */
void HAL_PMU_DisableWakeUpSrcIrq(pmu_wakeupsrc_t WakeUpSrc)
{
    IP_AON_CTRL->REG_WAKEUP_IMR.all |= (1 << WakeUpSrc);

}


/**
 * @brief Select GPIO polarity for wake-up.
 *
 * This function sets the polarity for a specific GPIO used as a wake-up source.
 * The polarity can be set to either high or low.
 *
 * @param GpioPos The position of the GPIO in the wake-up source, as defined in pmu_gpio_src_t.
 * @param polarity The polarity to be set for the specified GPIO.
 *                - 0: Set the polarity to high active.
 *                - 1: Set the polarity to low active.
 */
void HAL_PMU_GPIOPolaritySelect(pmu_gpio_src_t GpioPos, uint8_t polarity)
{
    if(polarity){
        IP_AON_CTRL->REG_GPIO_CTRL_POL.bit.GPIO_WAKEUP_POL |= (1 << (GpioPos - PMU_WAKEUP_GPIOB_00));
    }else{
        IP_AON_CTRL->REG_GPIO_CTRL_POL.bit.GPIO_WAKEUP_POL &= (~(1 << (GpioPos - PMU_WAKEUP_GPIOB_00)));
    }
}


/**
 * @brief Enter Hold Mode.
 *
 * This function puts the system into Hold mode. It chooses between Wait For Interrupt (WFI)
 * and Wait For Event (WFE) to enter sleep mode based on the SLEEPEntry parameter.
 *
 * @param SLEEPEntry Determines the method to enter sleep mode. Use PMU_HOLDENTRY_WFI for
 *        Wait For Interrupt, or other values for Wait For Event.
 */
void HAL_PMU_EnterHoldMode(uint8_t SLEEPEntry)
{
  /* Select SLEEP mode entry -------------------------------------------------*/
  if(SLEEPEntry == PMU_HOLDENTRY_WFI)
  {
    /* Request Wait For Interrupt */
    __WFI();
  }
  else
  {
    /* Request Wait For Event */
    __disable_irq();
    __WFE();
  }
}

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
void HAL_PMU_PreConfigSleepTrigger(pmu_sleep_trigger_t sleepTrigger) {
	switch(sleepTrigger) {
	case PMU_SLEEP_NONE:
		IP_AON_CTRL->REG_POWER_WKUP_CTRL0.bit.MASK_AP_ENTER_SLEEP = 0;
		IP_AON_CTRL->REG_POWER_WKUP_CTRL0.bit.MASK_CP_ENTER_SLEEP = 0;
		break;
    case PMU_SLEEP_CMD_BY_CP:
        IP_AON_CTRL->REG_POWER_WKUP_CTRL0.bit.MASK_AP_ENTER_SLEEP = 1;
        IP_AON_CTRL->REG_POWER_WKUP_CTRL0.bit.MASK_CP_ENTER_SLEEP = 0;
        break;
    case PMU_SLEEP_CMD_BY_AP:
    	IP_AON_CTRL->REG_POWER_WKUP_CTRL0.bit.MASK_AP_ENTER_SLEEP = 0;
		IP_AON_CTRL->REG_POWER_WKUP_CTRL0.bit.MASK_CP_ENTER_SLEEP = 1;
		break;
    case PMU_SLEEP_AP_AND_CP:
        IP_AON_CTRL->REG_POWER_WKUP_CTRL0.bit.MASK_AP_ENTER_SLEEP = 1;
        IP_AON_CTRL->REG_POWER_WKUP_CTRL0.bit.MASK_CP_ENTER_SLEEP = 1;
        break;
    default:
    	return;
	}
}

void HAL_PMU_ConfigDeepSleepMode(pmu_sleepmode_t SleepMode, uint8_t SLEEPEntry)
{
    IP_AON_CTRL->REG_POWER_WKUP_CTRL0.bit.ENA_POWERMODE = SleepMode;
    IP_AON_CTRL->REG_POWER_WKUP_CTRL0.bit.ENA_DEEPSLEEP = 1;

    __set_wfi_sleepmode(WFI_DEEP_SLEEP);
}

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
void HAL_PMU_EnterDeepSleepMode(pmu_sleepmode_t SleepMode, uint8_t SLEEPEntry)
{
    IP_AON_CTRL->REG_POWER_WKUP_CTRL0.bit.ENA_POWERMODE = SleepMode;
    IP_AON_CTRL->REG_POWER_WKUP_CTRL0.bit.ENA_DEEPSLEEP = 1;

    __set_wfi_sleepmode(WFI_DEEP_SLEEP);

    /* select WFI or WFE command to enter sleep mode */
    if(SLEEPEntry == PMU_DEEPSLEEPENTRY_WFI) {
        __WFI();
    } else {
        __disable_irq();
        __WFE();
        __enable_irq();
    }
}


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
void HAL_PMU_EnableRamRetention(pmu_rambank_t RamBank)
{
    if ((RamBank >= PMU_WIFI_SUB_RAMBANK0) && (RamBank <= PMU_CP_RAMBANK1)) {
        IP_AON_CTRL->REG_RAM_RETENTION_SEL.all |= (1 << RamBank);
    }
    if (RamBank == PMU_RAMBANK_NONE) {
        IP_AON_CTRL->REG_RAM_RETENTION_SEL.all = 0;
    }
}


/**
 * @brief Disables RAM retention for the specified RAM bank.
 *
 * This function disables retention for a specified RAM bank, allowing the RAM content to be not preserved
 * during low-power modes. It supports disabling retention for CPU RAM banks (PMU_CP_RAMBANK0 to PMU_CP_RAMBANK1),
 * Wi-Fi subsystem RAM banks (PMU_WIFI_SUB_RAMBANK0 to PMU_WIFI_SUB_RAMBANK7), and the Bluetooth RAM bank (PMU_BT_RAMBANK0).
 * The function clears the appropriate bit(s) in the PMU control registers to deactivate retention for the selected bank.
 *
 * @param RamBank The RAM bank for which to disable retention. This should be one of the
 *                values defined in the pmu_rambank_t enumeration, which includes CPU RAM banks,
 *                Wi-Fi subsystem RAM banks, the Bluetooth RAM bank, and an option to represent 'none'.
 *
 * @note This function performs no action if an invalid RAM bank is specified. For CPU and Wi-Fi RAM banks,
 *       the function modifies the RAM_RETENTION_SEL_L bits in the RAM_RETENTION_SEL register to clear the retention setting.
 *       The PMU_RAMBANK_NONE value is considered invalid for disabling RAM retention.
 */
void HAL_PMU_DisableRamRetention(pmu_rambank_t RamBank)
{
    if ((RamBank >= PMU_WIFI_SUB_RAMBANK0) && (RamBank <= PMU_CP_RAMBANK1)) {
        IP_AON_CTRL->REG_RAM_RETENTION_SEL.all &= ~(1 << RamBank);
    }
    if (RamBank == PMU_RAMBANK_NONE) {
        IP_AON_CTRL->REG_RAM_RETENTION_SEL.all = 0;
    }
}





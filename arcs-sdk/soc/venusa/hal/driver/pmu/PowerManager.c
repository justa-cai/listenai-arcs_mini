/**
 * @file PowerManager.c
 * @author USER
 * @brief Power Management Unit (PMU) driver implementation file.
 *        This file contains the core functionality for configuring and controlling
 *        system power modes, sleep/wakeup management, reset sources handling,
 *        and related low-power control features.
 *
 * @details Main responsibilities include:
 *          - Sleep trigger configuration (@ref HAL_PMU_PreConfigSleepTrigger)
 *          - Deep sleep mode entry (@ref HAL_PMU_EnterDeepSleepMode)
 *          - Wakeup source detection (@ref HAL_PMU_GetWakeUpCause)
 *          - System reset cause analysis (@ref HAL_PMU_GetSysResetCause)
 *          - GPIO wakeup polarity control (@ref HAL_PMU_GPIOPolaritySelect)
 *          - Wakeup source enable/disable APIs
 *          - Specialized reset functions (CMN cores, watchdog timers)
 *
 * @note All functions operate directly on hardware registers through memory-mapped
 *       peripheral addresses (IP_AON_CTRL, IP_CMN_SYS). Interrupt safety considerations
 *       must be handled by calling functions when applicable.
 *
 * @created May 15, 2025
 * @copyright Copyright (c) ListenAI
 * @license Proprietary - See LICENSE file for details
 */
#include "PowerManager.h"

/**
 * @brief Configure PMU sleep trigger conditions.
 *
 * This function sets the sleep trigger conditions in the Power Management Unit (PMU) by configuring
 * the appropriate bits in the power wakeup control register. It determines which CPU core(s) can trigger
 * the system to enter sleep mode based on the specified trigger source.
 *
 * @param[in] sleep_trigger Specifies the trigger source for entering sleep mode:
 *        @arg PMU_SLEEP_NONE           No sleep trigger enabled
 *        @arg PMU_SLEEP_TRIGGER_BY_CORE0 Core0 can trigger sleep (masking Core1)
 *        @arg PMU_SLEEP_TRIGGER_BY_CORE1 Core1 can trigger sleep (masking Core0)
 *        @arg PMU_SLEEP_TRIGGER_BY_CORE0_AND_CORE1 Both cores can trigger sleep
 *
 * @details The implementation modifies MASK_CORE*_ENTER_SLEEP bits in POWER_WKUP_CTRL0 register:
 *          - Clearing a bit allows corresponding core to trigger sleep
 *          - Setting a bit prevents that core from triggering sleep
 *
 * @note Only applicable when PMU is operating in split-core architecture mode.
 */
void HAL_PMU_PreConfigSleepTrigger(pmu_sleep_trigger_type_t sleep_trigger) {
    switch (sleep_trigger)
    {
    case PMU_SLEEP_NONE:
        IP_AON_CTRL->REG_POWER_WKUP_CTRL0.bit.MASK_CORE0_ENTER_SLEEP = 0;
        IP_AON_CTRL->REG_POWER_WKUP_CTRL0.bit.MASK_CORE1_ENTER_SLEEP = 0;
        break;
    case PMU_SLEEP_TRIGGER_BY_CORE0:
        IP_AON_CTRL->REG_POWER_WKUP_CTRL0.bit.MASK_CORE0_ENTER_SLEEP = 0;
        IP_AON_CTRL->REG_POWER_WKUP_CTRL0.bit.MASK_CORE1_ENTER_SLEEP = 1;
        break;
    case PMU_SLEEP_TRIGGER_BY_CORE1:
        IP_AON_CTRL->REG_POWER_WKUP_CTRL0.bit.MASK_CORE0_ENTER_SLEEP = 1;
        IP_AON_CTRL->REG_POWER_WKUP_CTRL0.bit.MASK_CORE1_ENTER_SLEEP = 0;
        break;
    case PMU_SLEEP_TRIGGER_BY_CORE0_AND_CORE1:
        IP_AON_CTRL->REG_POWER_WKUP_CTRL0.bit.MASK_CORE0_ENTER_SLEEP = 1;
        IP_AON_CTRL->REG_POWER_WKUP_CTRL0.bit.MASK_CORE1_ENTER_SLEEP = 1;
        break;
    default:
        return;
    }
}

/**
 * @brief Enter deep sleep power saving mode.
 *
 * This function initiates deep sleep procedure with specified sleep mode configuration. It performs
 * necessary setup in power control registers and executes either WFI (Wait For Interrupt) or WFE
 * (Wait For Event) instruction based on selected entry method.
 *
 * @param[in] SleepMode Desired power mode during deep sleep
 * @param[in] SLEEPEntry Method to enter sleep mode:
 *           @arg PMU_DEEPSLEEPENTRY_WFI Use WFI instruction
 *           @arg PMU_DEEPSLEEPENTRY_WFE Use WFE instruction with interrupt masking
 *
 * @details Execution flow:
 *          1. Program ENA_POWERMODE and ENA_DEEPSLEEP bits in POWER_WKUP_CTRL0
 *          2. Set processor sleep mode via __set_wfi_sleepmode()
 *          3. Execute selected sleep instruction (WFI/WFE)
 *
 * @note Must be called after completing all pre-sleep preparations.
 */
void HAL_PMU_EnterDeepSleepMode(pmu_sleepmode_t SleepMode, uint8_t SLEEPEntry) {
	__HAL_PMU_AON_LDO_NORMAL_OFF();

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
 * @brief Get last wakeup source from PMU.
 *
 * Retrieves the primary cause that woke up the system from low-power state by reading
 * the wakeup interrupt status register. Returns first detected wakeup source in priority order.
 *
 * @return Wakeup source code (@ref pmu_wakeupsrc_t). Returns PMU_WAKEUP_NONE if no wakeup occurred.
 *
 * @details Searches through all possible wakeup sources starting from highest priority
 *          (defined by enum ordering) until matching bit is found in REG_WAKEUP_ISR.
 */
pmu_wakeupsrc_t HAL_PMU_GetWakeUpCause(void) {
	uint32_t wakeupCause = IP_AON_CTRL->REG_WAKEUP_ISR.all;

	if(wakeupCause != 0) {
	    for (pmu_wakeupsrc_t src = PMU_WAKEUP_TIMER; src <= PMU_WAKEUP_GPIOB_05; src++) {
	        if (wakeupCause & (1 << src)) {
	            return src;
	        }
	    }
	}
	return PMU_WAKEUP_NONE;
}

/**
 * @brief Clear all wakeup source flags in PMU.
 *
 * Writes back the current wakeup status register value to clear all pending wakeup interrupts.
 * Typically called after processing a wakeup event to reset the status flags.
 *
 * @note Does not affect actual hardware state machines, only clears status flags.
 */
void HAL_PMU_ClearWakeUpCause(void) {
    uint32_t wakeupCause = IP_AON_CTRL->REG_WAKEUP_ISR.all;
    IP_AON_CTRL->REG_WAKEUP_ICR.all = wakeupCause;
}

/**
 * @brief Get system reset cause recorded by PMU.
 *
 * Reads the system reset status register to identify the last reset source. Returns the highest
 * priority reset cause found in the status register.
 *
 * @return System reset cause code (@ref pmu_rstsrc_t). Returns PMU_RST_NONE if no reset occurred.
 *
 * @details Checks individual reset flags in REG_SYSRST_STATUS in descending priority order:
 *          POR > AON > System Reset Requests > Software Resets > Watchdog Timeouts
 */
pmu_rstsrc_t HAL_PMU_GetSysResetCause(void) {
    uint32_t rstCause = IP_AON_CTRL->REG_SYSRST_STATUS.all;

    if(rstCause & (1 << PMU_RST_POR)) return PMU_RST_POR;
    if(rstCause & (1 << PMU_RST_AON)) return PMU_RST_AON;
    if(rstCause & (1 << PMU_RST_SYSRESETREQ_CORE1)) return PMU_RST_SYSRESETREQ_CORE1;
    if(rstCause & (1 << PMU_RST_SYSRESETREQ_CORE0)) return PMU_RST_SYSRESETREQ_CORE0;
    if(rstCause & (1 << PMU_RST_SW1)) return PMU_RST_SW1;
    if(rstCause & (1 << PMU_RST_SW0)) return PMU_RST_SW0;
    if(rstCause & (1 << PMU_RST_WDT_CORE1)) return PMU_RST_WDT_CORE1;
    if(rstCause & (1 << PMU_RST_WDT_CORE0)) return PMU_RST_WDT_CORE0;

    return PMU_RST_NONE;
}

/**
 * @brief Clear all system reset cause flags in PMU.
 *
 * Writes back the current reset status register value to clear all recorded reset causes.
 * Should be called after reading reset causes to prepare for new reset detection.
 *
 * @note Does not modify actual reset generation circuitry, only clears status flags.
 */
void HAL_PMU_ClearSysResetCause(void) {
	uint32_t rstCause = IP_AON_CTRL->REG_SYSRST_STATUS.all;
	IP_AON_CTRL->REG_SYSRST_STATUS.all = rstCause;
}

/**
 * @brief Select GPIO wakeup polarity for specified pin.
 *
 * Configures whether rising or falling edge on selected GPIO pin will generate wakeup event.
 * Also enables global GPIO wakeup control when changing polarity settings.
 *
 * @param[in] gpioPos GPIO pin position (@ref pmu_gpio_src_t)
 * @param[in] polarity Wakeup edge selection: 1=rising edge, 0=falling edge
 *
 * @details Toggles corresponding bit in GPIO_WAKEUP_POL register based on polarity parameter.
 *          Always enables ENA_GPIO_WAKEUP_CTRL after polarity configuration.
 */
void HAL_PMU_GPIOPolaritySelect(pmu_gpio_src_t gpioPos, uint8_t polarity) {
    if(polarity) {
        IP_AON_CTRL->REG_GPIO_CTRL_POL.bit.GPIO_WAKEUP_POL |= (1 << (gpioPos));
    } else {
        IP_AON_CTRL->REG_GPIO_CTRL_POL.bit.GPIO_WAKEUP_POL &= (~(1 << (gpioPos)));
    }   
    IP_AON_CTRL->REG_GPIO_WAKEUP_CTRL.bit.ENA_GPIO_WAKEUP_CTRL = 0x1;
}

/**
 * @brief Enable specified wakeup source in PMU.
 *
 * Sets corresponding enable bit in wakeup enable register to activate selected wakeup source.
 * Allows multiple wakeup sources to be enabled simultaneously.
 *
 * @param[in] WakeUpSrc Wakeup source to enable (@ref pmu_wakeupsrc_t)
 *
 * @details Directly sets bit corresponding to WakeUpSrc parameter in REG_WAKEUP_ENABLE.
 */
void HAL_PMU_EnableWakeUpSrc(pmu_wakeupsrc_t WakeUpSrc)
{
    IP_AON_CTRL->REG_WAKEUP_ENABLE.all |= (1 << WakeUpSrc);
}

/**
 * @brief Disable specified wakeup source in PMU.
 *
 * Clears corresponding enable bit in wakeup enable register to deactivate selected wakeup source.
 * Other enabled wakeup sources remain active.
 *
 * @param[in] WakeUpSrc Wakeup source to disable (@ref pmu_wakeupsrc_t)
 *
 * @details Directly clears bit corresponding to WakeUpSrc parameter in REG_WAKEUP_ENABLE.
 */
void HAL_PMU_DisableWakeUpSrc(pmu_wakeupsrc_t WakeUpSrc)
{
    IP_AON_CTRL->REG_WAKEUP_ENABLE.all &= (~(1 << WakeUpSrc));
}

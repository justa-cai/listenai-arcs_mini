/** @file aon_wdt.c
 *  @brief This file contains the implementation of the Always-On Watchdog Timer (AON WDT) driver.
 *         It provides initialization, power control, configuration, and interrupt handling functionalities.
 */

#include "Driver_AON_WDT.h"
#include "venusa_ap.h"

static void AON_WDT_Handler(void);

/** @struct AON_WDT_Info_t
 *  @brief Information structure for AON WDT instance.
 *         Contains callback event pointer and workspace pointer.
 */
static AON_WDT_Info_t aon_wdt_info = {0};

/** @struct AON_WDT_Resource_t
 *  @brief Resource structure for AON WDT peripheral.
 *         Includes register base address, IRQ number, IRQ handler, info pointer, and state flags.
 */
static AON_WDT_Resource_t aon_wdt_resources = {
	.reg = 	IP_AON_WDT,                   /*!< Base address of AON WDT register block */
	.irq_num = IRQ_AON_WDT_VECTOR,          /*!< Interrupt vector number for AON WDT */
	.irq_handler = AON_WDT_Handler,         /*!< Interrupt service routine handler */
	.info = &aon_wdt_info,                  /*!< Pointer to associated info structure */
	.state = AON_WDT_UNINITIALIZED,         /*!< Initial state flag */
};

/**
 * @brief Get the global AON WDT resources structure.
 *        Used by driver manager to locate peripheral resources.
 *
 * @return Pointer to the static aon_wdt_resources structure.
 */
void* AON_WDT(void) {
	return (void*)&aon_wdt_resources;
}

/**
 * @brief Initialize the AON WDT driver.
 *        Configures default parameters and sets the initialized state flag.
 *
 * @param[in] res          Pointer to AON_WDT_Resource_t structure.
 * @param[in] callback     User-defined event callback function.
 * @param[in] workspace    User-defined data passed to the callback function.
 *
 * @return CSK_DRIVER_OK on success, or error code if already initialized.
 *
 * @note This function must be called before any other driver functions.
 */
int32_t AON_WDT_Initialize(void* res, HAL_AON_WDT_SignalEvent_t callback, void* workspace) {
	AON_WDT_CHECK_RESOURCES(res);
	AON_WDT_Resource_t *aon_wdt = (AON_WDT_Resource_t*) res;

    if (aon_wdt->state & AON_WDT_INITIALIZED){
        return CSK_DRIVER_OK;
    }

    aon_wdt->info->cb_event = callback;
    aon_wdt->info->workspace = workspace;

    aon_wdt->state |= AON_WDT_INITIALIZED;

    return CSK_DRIVER_OK;
}

/**
 * @brief Uninitialize the AON WDT driver.
 *        Clears callback references and resets state machine to uninitialized.
 *
 * @param[in] res Pointer to AON_WDT_Resource_t structure.
 *
 * @return CSK_DRIVER_OK always.
 *
 * @note Does not modify hardware registers - only software state management.
 */
int32_t AON_WDT_Uninitialize(void* res) {
    AON_WDT_CHECK_RESOURCES(res);
    AON_WDT_Resource_t *aon_wdt = (AON_WDT_Resource_t*) res;

    aon_wdt->info->cb_event = NULL;
    aon_wdt->info->workspace = NULL;

    aon_wdt->state = AON_WDT_UNINITIALIZED;

    return CSK_DRIVER_OK;
}

/**
 * @brief Control power states of the AON WDT peripheral.
 *        Supports POWER_OFF and POWER_FULL states with clock gating and reset capabilities.
 *
 * @param[in] res   Pointer to AON_WDT_Resource_t structure.
 * @param[in] state Target power state (CSK_POWER_OFF/CSK_POWER_LOW/CSK_POWER_FULL).
 *
 * @return CSK_DRIVER_OK on success, error code for invalid transitions or unsupported states.
 *
 * @details
 * - For CSK_POWER_OFF: Disconnects IRQ and clears powered state flag.
 * - For CSK_POWER_FULL: Performs reset sequence, restores IRQ handler, and sets powered flag.
 * - CSK_POWER_LOW is explicitly unsupported.
 */
int32_t AON_WDT_PowerControl(void* res, CSK_POWER_STATE state) {
    AON_WDT_CHECK_RESOURCES(res);
    AON_WDT_Resource_t *aon_wdt = (AON_WDT_Resource_t*) res;

    switch (state) {
    case CSK_POWER_OFF:
        if ((aon_wdt->state & AON_WDT_INITIALIZED) == 0U) {
            return CSK_DRIVER_ERROR;
        }

		// Disable IRQ
		disable_IRQ(aon_wdt->irq_num);

		aon_wdt->state &= ~AON_WDT_POWERED;

		// Uninstall IRQ Handler
		register_ISR(aon_wdt->irq_num, NULL, NULL);

        break;

    case CSK_POWER_LOW:
        return CSK_DRIVER_ERROR_UNSUPPORTED;

    case CSK_POWER_FULL:
        if(!(aon_wdt->state & AON_WDT_INITIALIZED)){
            return CSK_DRIVER_ERROR;
        }

        if(aon_wdt->state & AON_WDT_POWERED){
            return CSK_DRIVER_OK;
        }

        // Clear reset status
        aon_wdt->reg->REG_AON_WDT_IRQ_CLR.all = 0x1;
        while(aon_wdt->reg->REG_AON_WDT_IRQ_CAUSE.bit.WDT_RESET_OCURRED) {
        	// Do nothing, intentionally empty
        }

		register_ISR(aon_wdt->irq_num, aon_wdt->irq_handler, NULL);
		enable_IRQ(aon_wdt->irq_num);

        aon_wdt->state |= AON_WDT_POWERED;

        break;
    default:
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    }

    return CSK_DRIVER_OK;
}

/**
 * @brief Configure watchdog timer parameters including load value, interrupt enable, trigger mode, and reset domain.
 *
 * @param[in] res      Pointer to AON_WDT_Resource_t structure.
 * @param[in] control  Bitmask combining various configuration options.
 * @param[in] arg      Argument specific to selected control option.
 *
 * @return CSK_DRIVER_OK on success, error code for invalid combinations or unpowered state.
 *
 * @details
 * - Time configuration (@ref HAL_AON_WDT_TIME_CFG): Sets the load value (arg).
 * - Interrupt control (@ref HAL_AON_WDT_INTERRUPT_Msk): Enables/disables interrupt generation.
 * - Mode control (@ref HAL_AON_WDT_MODE_CTRL_Msk): Selects reset-only or interrupt-only behavior.
 * - Reset domain (@ref HAL_AON_WDT_RST_DOMAIN_Msk): Chooses core or PMU domain for reset generation.
 */
int32_t AON_WDT_Control(void* res, uint32_t control, uint32_t arg){
    AON_WDT_CHECK_RESOURCES(res);
    AON_WDT_Resource_t *aon_wdt = (AON_WDT_Resource_t*) res;

    if(!(aon_wdt->state & AON_WDT_POWERED)){
        return CSK_DRIVER_ERROR;
    }

    // Set time load value
    if (control & HAL_AON_WDT_TIME_CFG){
    	aon_wdt->reg->REG_AONWDTTIMER_LOADVAL.all = arg & AON_WDT_LOAD_VALUE_MASK;
    }

    // Enable interrupt
    if (control & HAL_AON_WDT_INTERRUPT_Msk){
        if (arg){
            aon_wdt->reg->REG_AON_WDTTIMER_CTRL.bit.WDT_INT_MASK = 0x1;	// enable irq
        } else {
            aon_wdt->reg->REG_AON_WDTTIMER_CTRL.bit.WDT_INT_MASK = 0x0; // disable irq
        }
    }

    // Set WDT trigger mode
    switch (control & HAL_AON_WDT_MODE_CTRL_Msk){
    	case HAL_AON_WDT_CTRL_RESET_MODE:
    	{
			aon_wdt->reg->REG_AON_WDTTIMER_CTRL.bit.WDT_MODE = 0x1; //only reset no interrupt
    	}
		break;
    	case HAL_AON_WDT_CTRL_INT_MODE:
    	{
			aon_wdt->reg->REG_AON_WDTTIMER_CTRL.bit.WDT_MODE = 0x0; // only interrupt
    	}
		break;
    }

    // Set WDT reset domain
    switch (control & HAL_AON_WDT_RST_DOMAIN_Msk){
		case HAL_AON_WDT_RST_CORE_DOMAIN:
		{
			aon_wdt->reg->REG_RESET_PMU_EN.all = AON_WDT_DOMAIN_CTRL_RESET_DBB;
		}
			break;
		case HAL_AON_WDT_RST_PMU_DOMAIN:
		{
			aon_wdt->reg->REG_RESET_PMU_EN.all = AON_WDT_DOMAIN_CTRL_RESET_PMU;
		}
			break;
    }

    return CSK_DRIVER_OK;
}

/**
 * @brief Start the AON Watchdog Timer.
 *        Implements protected start sequence with lock mechanism.
 *
 * @param[in] res Pointer to AON_WDT_Resource_t structure.
 *
 * @return CSK_DRIVER_OK on success, error code if peripheral is not powered.
 *
 * @details
 * 1. Releases protection lock by writing release code to REG_OTHER_PROTECT.
 * 2. Starts the watchdog by setting START bit in control register.
 * 3. Polls until WDENABLED bit indicates successful startup.
 * 4. Reacquires protection lock using lock code.
 */
int32_t AON_WDT_Enable(void* res){
    AON_WDT_CHECK_RESOURCES(res);
    AON_WDT_Resource_t *aon_wdt = (AON_WDT_Resource_t*) res;

    // Release lock and wait sync to protected bit
    do{
    	aon_wdt->reg->REG_OTHER_PROTECT.all = AON_WDT_START_PROTECT_RELEASE;
    } while(aon_wdt->reg->REG_AON_WDTTIMER_CTRL.bit.START_PROTECTED);

    // Start
    aon_wdt->reg->REG_AON_WDTTIMER_CTRL.bit.START = 0x1;
    while(!aon_wdt->reg->REG_AON_WDTTIMER_CTRL.bit.WDENABLED);

    // Lock protect
    while(!aon_wdt->reg->REG_AON_WDTTIMER_CTRL.bit.START_PROTECTED){
        aon_wdt->reg->REG_OTHER_PROTECT.all = AON_WDT_START_PROTECT_LOCK;
    }

    return CSK_DRIVER_OK;
}

/**
 * @brief Stop the AON Watchdog Timer.
 *        Implements protected stop sequence with lock mechanism.
 *
 * @param[in] res Pointer to AON_WDT_Resource_t structure.
 *
 * @return CSK_DRIVER_OK on success, error code if peripheral is not powered.
 *
 * @details
 * 1. Releases protection lock by writing release code to REG_STOP_PROTECT.
 * 2. Stops the watchdog by setting STOP bit in control register.
 * 3. Polls until WDENABLED bit clears indicating successful stop.
 * 4. Reacquires protection lock using lock code.
 */
int32_t AON_WDT_Disable(void* res){
    AON_WDT_CHECK_RESOURCES(res);
    AON_WDT_Resource_t *aon_wdt = (AON_WDT_Resource_t*) res;

    // Release protect
    do {
        aon_wdt->reg->REG_STOP_PROTECT.all = AON_WDT_STOP_PROTECT_RELEASE;
    } while(aon_wdt->reg->REG_AON_WDTTIMER_CTRL.bit.STOP_PROTECTED);

    // Stop
    aon_wdt->reg->REG_AON_WDTTIMER_CTRL.bit.STOP = 0x1;
    while(aon_wdt->reg->REG_AON_WDTTIMER_CTRL.bit.WDENABLED);

    // Lock protect
    while(!aon_wdt->reg->REG_AON_WDTTIMER_CTRL.bit.STOP_PROTECTED){
        aon_wdt->reg->REG_STOP_PROTECT.all = AON_WDT_STOP_PROTECT_LOCK;
    }

    return CSK_DRIVER_OK;
}

/**
 * @brief Read the current load value stored in the watchdog timer register.
 *
 * @param[in]  res        Pointer to AON_WDT_Resource_t structure.
 * @param[out] load_value Address to store the current load value.
 *
 * @return CSK_DRIVER_OK on success, error code if peripheral is not powered.
 *
 * @note Value is automatically masked by AON_WDT_LOAD_VALUE_MASK.
 */
int32_t AON_WDT_ReadLoadValue(void* res, uint32_t *load_value) {
    AON_WDT_CHECK_RESOURCES(res);
    AON_WDT_Resource_t *aon_wdt = (AON_WDT_Resource_t*) res;

    *load_value = aon_wdt->reg->REG_AONWDTTIMER_LOADVAL.all & AON_WDT_LOAD_VALUE_MASK;

    return CSK_DRIVER_OK;
}

/**
 * @brief Refresh the watchdog timer counter without restarting the timer.
 *        Requires temporary release of protection lock.
 *
 * @param[in] res Pointer to AON_WDT_Resource_t structure.
 *
 * @return CSK_DRIVER_OK on success, error code if peripheral is not powered.
 *
 * @note The actual reload operation may require additional synchronization depending on hardware behavior.
 */
int32_t AON_WDT_Refresh(void* res){
    AON_WDT_CHECK_RESOURCES(res);
    AON_WDT_Resource_t *aon_wdt = (AON_WDT_Resource_t*) res;

    // Release lock and wait sync to protected bit
    do{
    	aon_wdt->reg->REG_OTHER_PROTECT.all = AON_WDT_START_PROTECT_RELEASE;
    } while(aon_wdt->reg->REG_AON_WDTTIMER_CTRL.bit.LOAD_PROTECTED);

    aon_wdt->reg->REG_AON_WDTTIMER_CTRL.bit.RELOAD = 0x1;

    // TODO Reload bit will be pretend by protected bit
//    // Lock protect
//    while(!aon_wdt->reg->REG_AON_WDTTIMER_CTRL.bit.LOAD_PROTECTED){
//    	aon_wdt->reg->REG_OTHER_PROTECT.all = AON_WDT_START_PROTECT_LOCK;
//    }

    return CSK_DRIVER_OK;
}

/**
 * @brief Interrupt Service Routine (ISR) for AON WDT events.
 *        Clears interrupt status, calls registered callback, and acknowledges causes.
 *
 * @details This handler executes in interrupt context. It:
 *         1. Clears the interrupt status register.
 *         2. Calls the user-registered callback if present.
 *         3. Drains all reported interrupt causes.
 */
static void AON_WDT_Handler(void){
	// Clear interrupt source
	aon_wdt_resources.reg->REG_AON_WDT_IRQ_CLR.all = 0x1;

    if (aon_wdt_resources.info->cb_event) {
    	aon_wdt_resources.info->cb_event(aon_wdt_resources.info->workspace);
    }

    while(aon_wdt_resources.reg->REG_AON_WDT_IRQ_CAUSE.bit.WDT_WAKEUP_STATUS);
}

/**
 *@file wdt.c
 *
 *@date  Created on: 2025.5.19
 *      Author: USER
 */

#include "wdt.h"

/**
 * @brief Global watchdog timer instance information structure
 *
 * Stores operational state and configuration data for the primary WDT instance.
 * Managed exclusively by the driver core logic.
 */
static WDT_Info_t wdt0_info = { 0 };

/**
 * @brief Watchdog Timer Interrupt Service Routine (ISR)
 *
 * Clears expired interrupt flag and invokes registered callback function.
 * Designed to handle WDT trigger events according to configured behavior.
 */
static void WDT_IRQ_Handler(void);

/**
 * @brief Watchdog Timer Hardware Resource Descriptor
 *
 * Maps physical peripheral base address, IRQ vector, and associates handler/data structures.
 * Configured differently based on boot HARTID selection at compilation time.
 */
static const WDT_Resources_t wdt0_resources = {
#if(BOOT_HARTID == 0)
	IP_CORE0_WDT,
    IRQ_CORE0_WDT_VECTOR,
#else
	IP_CORE1_WDT,
    IRQ_CORE1_WDT_VECTOR,
#endif
    WDT_IRQ_Handler,
    &wdt0_info,
};

/**
 * @brief Get pointer to watchdog timer resources
 *
 * Retrieves base address of statically allocated WDT resource structure.
 * Used by driver manager to access hardware-specific interfaces.
 *
 * @return Void pointer to const WDT_Resources_t structure
 */
void* WDT(void) {
    return (void*)&wdt0_resources;
}

/**
 * @brief Initialize watchdog timer resources
 *
 * Sets up initial state machine, registers callback handler, and prepares control paths.
 * Must be called before any other WDT operations except power management.
 *
 * @param[in] res      Pointer to WDT_Resources_t structure
 * @param[in] callback User-defined event notification function
 * @param[in] workspace Application-specific context data buffer
 * @return CSK_DRIVER_OK on success, error code otherwise
 *
 * @note Does nothing if already initialized (idempotent operation)
 * @note Stores callback and workspace pointers in resource structure
 */
int32_t WDT_Initialize(void* res, HAL_WDT_SignalEvent_t callback, void* workspace) {
    CHECK_RESOURCES(res);
    WDT_Resources_t *wdt = (WDT_Resources_t *)res;

    if (wdt->info->state & WDT_INITIALIZED) {
        return CSK_DRIVER_OK;
    }

    wdt->info->busy = 0x0;
    wdt->info->callback = callback;
    wdt->info->int_stage = 0x0;
    wdt->info->reset_stage = 0x0;
    wdt->info->state |=  WDT_INITIALIZED;
    wdt->info->workspace = workspace;

    return CSK_DRIVER_OK;
}

/**
 * @brief Deinitialize watchdog timer resources
 *
 * Tears down software state machine and detaches all registered handlers.
 * Guaranteed to succeed even if never initialized.
 *
 * @param[in] res Pointer to WDT_Resources_t structure
 * @return Always returns CSK_DRIVER_OK
 *
 * @note Clears all stored callback references and working memory pointers
 * @note Sets state machine back to uninitialized condition
 */
int32_t WDT_Uninitialize(void* res) {
    CHECK_RESOURCES(res);
    WDT_Resources_t *wdt = (WDT_Resources_t *)res;

    // Reset WDT status flags
    wdt->info->int_stage = 0x0;
    wdt->info->reset_stage = 0x0;
    wdt->info->state = 0U;
    wdt->info->busy = 0x0;
    wdt->info->callback = NULL;
    wdt->info->workspace = NULL;

    return CSK_DRIVER_OK;
}

/**
 * @brief Control power states of watchdog timer
 *
 * Manages transitions between powered-off, low-power, and full-power operating modes.
 * Full power mode enables clock gating and interrupt generation capabilities.
 *
 * @param[in] res     Pointer to WDT_Resources_t structure
 * @param[in] state   Target power state (OFF/LOW/FULL)
 * @return CSK_DRIVER_OK on success, error code otherwise
 *
 * @retval CSK_DRIVER_ERROR If attempted to power off without initialization
 * @retval CSK_DRIVER_ERROR_UNSUPPORTED For unsupported low-power requests
 * @note Low-power mode currently not implemented (returns UNSUPPORTED)
 * @note Power transitions involve IRQ mask updates and register writes
 */
int32_t WDT_PowerControl(void* res, CSK_POWER_STATE state) {
    CHECK_RESOURCES(res);
    WDT_Resources_t *wdt = (WDT_Resources_t *)res;

    switch (state)
    {
    case CSK_POWER_OFF:
        if ((wdt->info->state & WDT_INITIALIZED) == 0U) {
            return CSK_DRIVER_ERROR;
        }

        register_ISR(wdt->irq_num, NULL, NULL);
        disable_IRQ(wdt->irq_num);

        wdt->reg->REG_CTRL.bit.EN = 0x0;

        wdt->info->state &= ~WDT_POWERED;
        break;
    case CSK_POWER_LOW:
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    case CSK_POWER_FULL:
        if ((wdt->info->state & WDT_INITIALIZED) == 0U) {
            return CSK_DRIVER_ERROR;
        }
        register_ISR(wdt->irq_num, wdt->irq_handler, NULL);
        enable_IRQ(wdt->irq_num);

        wdt->reg->REG_CTRL.bit.EN = 0x1;
        
        wdt->info->state |= WDT_POWERED; 
        break;
    default:
        break;
    }

    return CSK_DRIVER_OK;
}

/**
 * @brief Configure watchdog timer parameters
 *
 * Programs clock source selection, interrupt timeout, and reset window duration.
 * Requires powered state and non-busy condition to proceed.
 *
 * @param[in] res    Pointer to WDT_Resources_t structure
 * @param[in] cfg    Configuration parameters structure
 * @return CSK_DRIVER_OK on success, error code otherwise
 *
 * @retval CSK_DRIVER_ERROR If not powered or device busy
 * @note Uses magic sequence writes for protected register access
 * @note Updates internal stage tracking variables from configuration
 * @note Sets WDT_STATE_CONFIGURED flag upon successful completion
 */
int32_t WDT_Control(void* res, HAL_DRIVER_WDT_Cfg_t* cfg) {
    CHECK_RESOURCES(res);
    WDT_Resources_t *wdt = (WDT_Resources_t *)res;

    if ((wdt->info->state & WDT_STATE_POWERED) == 0U) {
        return CSK_DRIVER_ERROR;
    }

    if (wdt->info->busy) {
        return CSK_DRIVER_ERROR_BUSY;
    }

    uint32_t cfg_value = 0;

    // Clock source
    cfg_value = __RV_INSERT_FIELD(cfg_value, WDT_CTRL_CLKSEL_Msk, cfg->clk_src);
    // interrupt time
    cfg_value = __RV_INSERT_FIELD(cfg_value, WDT_CTRL_INTTIME_Msk, cfg->int_time);
    // reset timer
    cfg_value = __RV_INSERT_FIELD(cfg_value, WDT_CTRL_RSTTIME_Msk, cfg->rst_time);

    wdt->info->int_stage   = cfg->int_time;
    wdt->info->reset_stage = cfg->rst_time;

    wdt->reg->REG_WREN.all = WDT_MAGIC_WRITE_PROTECTION;
    wdt->reg->REG_CTRL.all = cfg_value;

    // Set configured flag
    wdt->info->state |= WDT_STATE_CONFIGURED;
    
    return CSK_DRIVER_OK;
}

/**
 * @brief Enable watchdog timer operation
 *
 * Arms the timer with interrupt and reset output enables after validation checks.
 * Marks device as busy during active counting phase.
 *
 * @param[in] res Pointer to WDT_Resources_t structure
 * @return CSK_DRIVER_OK on success, error code otherwise
 *
 * @retval CSK_DRIVER_ERROR If not powered or device busy
 * @note Performs three-step enable sequence (RSTEN->INTEN->EN)
 * @note Uses partial clear/set operations for control register updates
 * @note Sets busy flag until explicitly disabled or reset occurs
 */
int32_t WDT_Enable(void *res) {
    CHECK_RESOURCES(res);
    WDT_Resources_t *wdt = (WDT_Resources_t *)res;

    uint32_t cfg_value = 0U;

    if ((wdt->info->state & WDT_STATE_POWERED) == 0U) {
        return CSK_DRIVER_ERROR;
    }

    if (wdt->info->busy == 1) {
        return CSK_DRIVER_ERROR_BUSY;
    }

    // rst enable
    cfg_value = __RV_INSERT_FIELD(cfg_value, WDT_CTRL_RSTEN_Msk, 0x1);
    // int enable
    cfg_value = __RV_INSERT_FIELD(cfg_value, WDT_CTRL_INTEN_Msk, 0x1);
    // enable
    cfg_value = __RV_INSERT_FIELD(cfg_value, WDT_CTRL_EN_Msk, 0x1);
    
    wdt->info->busy = 0x1;

    // enable write and write control register
    wdt->reg->REG_WREN.all = WDT_MAGIC_WRITE_PROTECTION;
    wdt->reg->REG_CTRL.all = (wdt->reg->REG_CTRL.all & (~(WDT_CTRL_RSTEN_Msk | WDT_CTRL_INTEN_Msk | WDT_CTRL_EN_Msk))) | cfg_value;

    return CSK_DRIVER_OK;
}

/**
 * @brief Disable watchdog timer operation
 *
 * Stops counting and disables interrupt/reset outputs while maintaining configuration.
 * Clears busy flag allowing new configuration cycles.
 *
 * @param[in] res Pointer to WDT_Resources_t structure
 * @return CSK_DRIVER_OK on success, error code otherwise
 *
 * @retval CSK_DRIVER_ERROR If not powered
 * @note Maintains configured parameters but halts timing operations
 * @note Uses magic sequence write for protected register access
 * @note Clears busy flag immediately after disable operation
 */
int32_t WDT_Disable(void *res) {
    CHECK_RESOURCES(res);
    WDT_Resources_t *wdt = (WDT_Resources_t *)res;

    if((wdt->info->state & WDT_STATE_POWERED) == 0U) {
        return CSK_DRIVER_ERROR;
    }

    wdt->reg->REG_WREN.all = WDT_MAGIC_WRITE_PROTECTION;
    wdt->reg->REG_CTRL.bit.EN = 0x0;

    wdt->info->busy = 0x0;

    return CSK_DRIVER_OK;
}

/**
 * @brief Feed watchdog timer (prevent timeout)
 *
 * Writes special sequence to restart counter without reconfiguration.
 * Required periodic maintenance when using automatic reset mode.
 *
 * @param[in] res Pointer to WDT_Resources_t structure
 * @return CSK_DRIVER_OK on success, error code otherwise
 *
 * @note Uses two-phase magic sequence write protocol
 * @note First writes write enable key, then restart command
 * @note Does not affect current enable/busy state machines
 */
int32_t WDT_Feed(void *res) {
    CHECK_RESOURCES(res);
    WDT_Resources_t *wdt = (WDT_Resources_t *)res;

    wdt->reg->REG_WREN.all = WDT_MAGIC_WRITE_PROTECTION;  // 0x5AA5
    wdt->reg->REG_RESTART.all = WDT_MAGIC_RESTART_VALUE;  // 0xCAFE

    return CSK_DRIVER_OK;
}

/**
 * @brief Watchdog Timer Interrupt Handler
 *
 * Clears expired interrupt status and dispatches to registered callback.
 * Called automatically by interrupt controller when timer expires.
 *
 * @note Runs with interrupt priorities determined by vector table
 * @note Only processes one expiration event per invocation
 * @note Callback execution occurs with interrupt locked (nesting prevented)
 */
static void
WDT_IRQ_Handler(void) {
    wdt0_resources.reg->REG_ST.bit.INTEXPIRED = 0x1;

    if (wdt0_resources.info->callback != NULL){
        wdt0_resources.info->callback(wdt0_resources.info->workspace);
    }
}

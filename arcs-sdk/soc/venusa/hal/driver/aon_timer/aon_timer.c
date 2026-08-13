/** @file aon_timer.c
 *  @brief This file contains the implementation of the Always-On Timer (AON TIMER) driver.
 *         It provides initialization, power control, timer configuration, and interrupt handling functionalities.
 */

#include "Driver_AON_TIMER.h"
#include "PowerManager.h"
#include "venusa_ap.h"

static void aon_timer_irq_handler(void);

/** @struct AON_TIMER_Info_t
 *  @brief Information structure for AON TIMER instance.
 *         Contains callback event pointer, workspace pointer, run mode, and reload count.
 */
static AON_TIMER_Info_t aon_timer_info = {0};

/** @struct AON_TIMER_Resources_t
 *  @brief Resource structure for AON TIMER peripheral.
 *         Includes register base address, IRQ number, IRQ handler, info pointer, and state flags.
 */
static AON_TIMER_Resources_t aon_timer_resources = {
    .reg = IP_AON_TIMER,                   /*!< Base address of AON TIMER register block */
    .irq_num = IRQ_AON_TIMER_VECTOR,       /*!< Interrupt vector number for AON TIMER */
    .irq_handler = aon_timer_irq_handler,  /*!< Interrupt service routine handler */
    .info = &aon_timer_info,               /*!< Pointer to associated info structure */
    .state = AON_TIMER_UNINITIALIZED,      /*!< Initial state flag */
};

/**
 * @brief Initialize the AON TIMER driver.
 *        Configures default parameters and sets the initialized state flag.
 *
 * @param[in] res          Pointer to AON_TIMER_Resources_t structure.
 * @param[in] cb_event     Event callback function triggered on timer completion.
 * @param[in] workspace    User-defined data passed to the callback function.
 *
 * @return CSK_DRIVER_OK on success, or error code if already initialized.
 *
 * @note This function must be called before any other driver functions.
 */
int32_t AON_TIMER_Initialize(void* res, HAL_AON_TIMER_SignalEvent_t cb_event, void* workspace) {
    CHECK_RESOURCES(res);
    AON_TIMER_Resources_t *aon_timer = (AON_TIMER_Resources_t *)res;

    if(aon_timer->state & AON_TIMER_INITIALIZED) {
        return CSK_DRIVER_OK;
    }

    aon_timer->info->cb_event = cb_event;
    aon_timer->info->workspace = workspace;
    aon_timer->info->run_mode = HAL_AON_TIMER_MODE_Normal;
    aon_timer->info->reload_cnt = 0;

    aon_timer->state |= AON_TIMER_INITIALIZED;

    return CSK_DRIVER_OK;
}

/**
 * @brief Control power states of the AON TIMER peripheral.
 *        Supports POWER_OFF and POWER_FULL states with clock gating and reset capabilities.
 *
 * @param[in] res   Pointer to AON_TIMER_Resources_t structure.
 * @param[in] state Target power state (CSK_POWER_OFF/CSK_POWER_LOW/CSK_POWER_FULL).
 *
 * @return CSK_DRIVER_OK on success, error code for invalid transitions or unsupported states.
 *
 * @details
 * - For CSK_POWER_OFF: Disconnects IRQ and clears powered state flag.
 * - For CSK_POWER_FULL: Enables clock, performs reset, restores IRQ handler, and sets powered flag.
 * - CSK_POWER_LOW is explicitly unsupported.
 */
int32_t AON_TIMER_PowerControl(void* res, CSK_POWER_STATE state) {
    CHECK_RESOURCES(res);
    AON_TIMER_Resources_t *aon_timer = (AON_TIMER_Resources_t *)res;

    switch (state)
    {
    case CSK_POWER_OFF:
        if ((aon_timer->state & AON_TIMER_INITIALIZED) == 0U) {
            return CSK_DRIVER_ERROR;
        }

        // Disable irq
        disable_IRQ(aon_timer->irq_num);
        register_ISR(aon_timer->irq_num, NULL, NULL);
        aon_timer->state &= ~AON_TIMER_POWERED;
        break;
    case CSK_POWER_LOW:
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    case CSK_POWER_FULL:
        if ((aon_timer->state & AON_TIMER_INITIALIZED) == 0U) {
            return CSK_DRIVER_ERROR;
        }

        IP_AON_CTRL->REG_AON_CLK_CTRL.bit.ENA_AON_TIMER_CLK = 0x1;
        IP_AON_CTRL->REG_AON_RST_CTRL.bit.AON_TIMER_RESET = 0x1;

        // __HAL_PMU_AON_TIMER_ENABLE();
        // __HAL_PMU_AON_TIMER_RESET();

        register_ISR(aon_timer->irq_num, aon_timer->irq_handler, NULL);
        enable_IRQ(aon_timer->irq_num);
        
        aon_timer->state |= AON_TIMER_POWERED;
        break;
    default:
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    }

    return CSK_DRIVER_OK;
}

/**
 * @brief Configure timer operating mode, interrupt enable, and clock source selection.
 *        Affects wrap/repeat behavior, interrupt masking, and input clock derivation path.
 *
 * @param[in] res     Pointer to AON_TIMER_Resources_t structure.
 * @param[in] control Bitmask combining mode, interrupt, and clock selection settings.
 *
 * @return CSK_DRIVER_OK on success, error code for invalid combinations or unpowered state.
 *
 * @details
 * - Mode selection (@ref HAL_AON_TIMER_MODE_Msk):
 *     * HAL_AON_TIMER_MODE_Wrapping: Free-running counter with auto-reload.
 *     * HAL_AON_TIMER_MODE_Repeat: Pulse generation after terminal count.
 *     * HAL_AON_TIMER_MODE_Normal: Single-shot countdown.
 * - Interrupt control (@ref HAL_AON_TIMER_INTERRUPT_Msk):
 *     * HAL_AON_TIMER_INTERRUPT_Enabled: Unmask interrupt output.
 *     * HAL_AON_TIMER_INTERRUPT_Disabled: Block interrupt propagation.
 * - Clock selection (@ref HAL_AON_TIMER_CLK_SEL_Mask):
 *     * HAL_AON_TIMER_CLK_SEL_Rc32K: Direct 32kHz reference.
 *     * HAL_AON_TIMER_CLK_SEL_X024M_Div32K: External crystal divided by 32K.
 *     * HAL_AON_TIMER_CLK_SEL_RC24M_Div32K: Internal RC oscillator divided by 32K.
 */
int32_t AON_TIMER_Control(void* res, uint32_t control) {
    CHECK_RESOURCES(res);
    AON_TIMER_Resources_t *aon_timer = (AON_TIMER_Resources_t *)res;

    if((aon_timer->state & AON_TIMER_POWERED) == 0){
        return CSK_DRIVER_ERROR;
    }

    aon_timer->info->run_mode = control & HAL_AON_TIMER_MODE_Msk;
    switch(control & HAL_AON_TIMER_MODE_Msk) 
    {
    case HAL_AON_TIMER_MODE_Wrapping:
        aon_timer->reg->REG_OS_TIMER_CTRL.bit.WRAP_MODE = 0x1;
        aon_timer->reg->REG_OS_TIMER_CTRL.bit.REPEAT_MODE = 0x0;
        break;
    case HAL_AON_TIMER_MODE_Repeat:
        aon_timer->reg->REG_OS_TIMER_CTRL.bit.WRAP_MODE = 0x0;
        aon_timer->reg->REG_OS_TIMER_CTRL.bit.REPEAT_MODE = 0x1;
        break;
    case HAL_AON_TIMER_MODE_Normal:
        aon_timer->reg->REG_OS_TIMER_CTRL.bit.WRAP_MODE = 0x0;
        aon_timer->reg->REG_OS_TIMER_CTRL.bit.REPEAT_MODE = 0x0;
        break;
    default:
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    }

    switch(control & HAL_AON_TIMER_INTERRUPT_Msk)
    {
    case HAL_AON_TIMER_INTERRUPT_Enabled:
        aon_timer->reg->REG_OS_TIMER_IRQ_MASK.all = 0x1;
        break;
    case HAL_AON_TIMER_INTERRUPT_Disabled:
        aon_timer->reg->REG_OS_TIMER_IRQ_MASK.all = 0x0;
        break;
    default:
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    }

    switch(control & HAL_AON_TIMER_CLK_SEL_Mask)
    {
    case HAL_AON_TIMER_CLK_SEL_Rc32K:
        IP_AON_CTRL->REG_AON_CLK_CTRL.bit.AON_SLP_CLK_SEL = 0x0;
        break;
    case HAL_AON_TIMER_CLK_SEL_XO24M_Div32K:
        IP_AON_CTRL->REG_AON_CLK_CTRL.bit.AON_SLP_CLK_SEL = 0x1;
        IP_AON_CTRL->REG_AON_CLK_CTRL.bit.SEL_32KDIV_SRC  = 0x1;
        break;
    case HAL_AON_TIMER_CLK_SEL_RC24M_Div32K:
        IP_AON_CTRL->REG_AON_CLK_CTRL.bit.AON_SLP_CLK_SEL = 0x1;
        IP_AON_CTRL->REG_AON_CLK_CTRL.bit.SEL_32KDIV_SRC  = 0x0;
        break;
    default:
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    }

    return CSK_DRIVER_OK;
}

/**
 * @brief Set the timer period using direct count value loading.
 *        The loaded value determines the duration before reaching terminal count.
 *
 * @param[in] res    Pointer to AON_TIMER_Resources_t structure.
 * @param[in] count  Desired timer period expressed in clock cycles (masked by hardware limits).
 *
 * @return CSK_DRIVER_OK on success, error code if peripheral is not powered.
 *
 * @note The actual loaded value is constrained by AON_TIMER_LOAD_VALUE_MASK.
 */
int32_t AON_TIMER_SetTimerPeriodByCount(void* res, uint32_t count) {
    CHECK_RESOURCES(res);
    AON_TIMER_Resources_t *aon_timer = (AON_TIMER_Resources_t *)res;

    if((aon_timer->state & AON_TIMER_POWERED) == 0){
        return CSK_DRIVER_ERROR;
    }

    aon_timer->reg->REG_OS_TIMER_CTRL.bit.LOADVAL = count & AON_TIMER_LOAD_VALUE_MASK;
    aon_timer->reg->REG_OS_TIMER_CTRL.bit.LOADER = 0x1;

    return CSK_DRIVER_OK;
}

/**
 * @brief Start the AON TIMER counting operation.
 *        Polls until the ENABLED bit confirms successful activation.
 *
 * @param[in] res Pointer to AON_TIMER_Resources_t structure.
 *
 * @return CSK_DRIVER_OK on success, error code if peripheral is not powered.
 *
 * @warning Must ensure proper configuration via AON_TIMER_Control() before starting.
 */
int32_t AON_TIMER_StartTimer(void* res) {
    CHECK_RESOURCES(res);
    AON_TIMER_Resources_t *aon_timer = (AON_TIMER_Resources_t *)res;

    if((aon_timer->state & AON_TIMER_POWERED) == 0){
        return CSK_DRIVER_ERROR;
    }

    aon_timer->reg->REG_OS_TIMER_CTRL.bit.ENABLE = 0x1;
    while(!aon_timer->reg->REG_OS_TIMER_CTRL.bit.ENABLED);

    return CSK_DRIVER_OK;
}

/**
 * @brief Uninitialize the AON TIMER driver.
 *        Clears callback references and resets state machine to uninitialized.
 *
 * @param[in] res Pointer to AON_TIMER_Resources_t structure.
 *
 * @return CSK_DRIVER_OK always.
 *
 * @note Does not modify hardware registers - only software state management.
 */
int32_t AON_TIMER_Uninitialize(void* res) {
    CHECK_RESOURCES(res);
    AON_TIMER_Resources_t *aon_timer = (AON_TIMER_Resources_t*)res;

    aon_timer->info->cb_event = NULL;
    aon_timer->info->run_mode = HAL_AON_TIMER_MODE_Normal;
    aon_timer->info->reload_cnt = 0;

    aon_timer->state = AON_TIMER_UNINITIALIZED;

    return CSK_DRIVER_OK;
}

/**
 * @brief Read the current timer counter value.
 *        Returns the elapsed count since last load operation.
 *
 * @param[in]  res    Pointer to AON_TIMER_Resources_t structure.
 * @param[out] count Address to store the current counter value.
 *
 * @return CSK_DRIVER_OK on success, error code if peripheral is not powered.
 *
 * @note Value is automatically masked by AON_TIMER_LOAD_VALUE_MASK.
 */
int32_t AON_TIMER_ReadTimerCount(void* res, uint32_t *count) {
    CHECK_RESOURCES(res);
    AON_TIMER_Resources_t *aon_timer = (AON_TIMER_Resources_t*)res;

    *count = aon_timer->reg->REG_OS_TIMER_CURVAL.all & AON_TIMER_LOAD_VALUE_MASK;

    return CSK_DRIVER_OK;
}

/**
 * @brief Get the global AON TIMER resources structure.
 *        Used by driver manager to locate peripheral resources.
 *
 * @return Pointer to the static aon_timer_resources structure.
 */
void *AON_TIMER() {
    return (void*)&aon_timer_resources;
}

/**
 * @brief Stop the AON TIMER counting operation.
 *        Polls until the ENABLED bit confirms deactivation completion.
 *
 * @param[in] res Pointer to AON_TIMER_Resources_t structure.
 *
 * @return CSK_DRIVER_OK on success, error code if peripheral is not powered.
 *
 * @note Hardware continues running until STOP sequence completes.
 */
int32_t AON_TIMER_StopTimer(void* res) {
    CHECK_RESOURCES(res);
    AON_TIMER_Resources_t *aon_timer = (AON_TIMER_Resources_t*)res;

    if((aon_timer->state & AON_TIMER_POWERED) == 0){
        return CSK_DRIVER_ERROR;
    }

    aon_timer->reg->REG_OS_TIMER_CTRL.bit.ENABLE = 0x0;
    while(aon_timer->reg->REG_OS_TIMER_CTRL.bit.ENABLED);

    return CSK_DRIVER_OK;
}

/**
 * @brief Interrupt Service Routine (ISR) for AON TIMER events.
 *        Clears interrupt status, calls registered callback, and acknowledges causes.
 *
 * @details This handler executes in interrupt context. It:
 *         1. Clears the interrupt status register.
 *         2. Calls the user-registered callback if present.
 *         3. Drains all reported interrupt causes.
 */
static void aon_timer_irq_handler() {
    aon_timer_resources.reg->REG_OS_TIMER_IRQ_CLR.all = 0x1;

    if (aon_timer_resources.info->cb_event){
        aon_timer_resources.info->cb_event(HAL_AON_TIMER_EVENT_COMPLETE, aon_timer_resources.info->workspace);
    }

    while(aon_timer_resources.reg->REG_OS_TIMER_IRQ_CAUSE.bit.OSTIMER_STATUS);
}

#include "Driver_WDT.h"
#include "arcs_ap.h"

// WDT flags
#define WDT_FLAG_INITIALIZED                (1U << 0)
#define WDT_FLAG_POWERED                    (1U << 1)
#define WDT_FLAG_CONFIGURED                 (1U << 2)

#define WDT_MAGIC_WRITE_PROTECTION          (0x5AA5)
#define WDT_MAGIC_RESTARTING                (0xCAFE)

// WDT Information (Run-Time)
typedef struct _WDT_INFO
{
    uint8_t flags;         // WDT driver flags
    uint8_t busy;
    uint8_t int_stage;
    uint8_t reset_stage;
    HAL_WDT_SignalEvent_t callback;
    void* workspace;
} WDT_INFO;

// WDT Resources definitions
typedef struct
{
    WDT_RegDef *reg;          // Pointer to WDT peripheral
    uint32_t irq_num;
    void (*irq_handler)(void);
    WDT_INFO *info;         // Run-Time Information
}const WDT_RESOURCES;

static WDT_INFO wdt0_info = { 0 };

static void WDT_IRQ_Handler(void);
static const WDT_RESOURCES wdt0_resources = {
#if(BOOT_HARTID == 0)
	IP_AP_WDT,
#else
	IP_CP_WDT,
#endif
    IRQ_AP_WDT_VECTOR,
    WDT_IRQ_Handler,
    &wdt0_info
};

#define CHECK_RESOURCES(res)  do{\
    if(res != &wdt0_resources){\
        return CSK_DRIVER_ERROR_PARAMETER;\
    }\
}while(0)

void* WDT(void){
    return (void*)&wdt0_resources;
}

/**
 * @brief Initializes the Watchdog Timer (WDT).
 *
 * This function prepares the Watchdog Timer (WDT) for operation, setting it to a known state. It typically
 * involves setting the WDT to its default configuration.
 *
 * @param res A pointer to the resources needed by the WDT, such as base addresses or hardware descriptors.
 *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure. The error code indicates the
 *                 nature of the failure in the initialization process.
 */
int32_t
WDT_Initialize(void* res, HAL_WDT_SignalEvent_t callback, void* workspace)
{
    CHECK_RESOURCES(res);
    WDT_RESOURCES* wdt = (WDT_RESOURCES*)res;

    if (wdt->info->flags & WDT_FLAG_INITIALIZED) {
        // Driver is already initialized
        return CSK_DRIVER_OK;
    }

    wdt->info->busy = 0x0;
    wdt->info->int_stage = 0x0;
    wdt->info->reset_stage = 0x0;
    wdt->info->flags = WDT_FLAG_INITIALIZED;
    wdt->info->callback = callback;
    wdt->info->workspace = workspace;

    return CSK_DRIVER_OK;
}

/**
 * @brief Uninitializes the Watchdog Timer (WDT).
 *
 * This function disables the WDT and releases the associated resources. It should be called when the WDT
 * is no longer needed, to ensure proper cleanup of resources.
 *
 * @param res A pointer to the resources used by the WDT.
 *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure.
 */
int32_t
WDT_Uninitialize(void* res)
{
    CHECK_RESOURCES(res);
    WDT_RESOURCES* wdt = (WDT_RESOURCES*)res;

    // Reset WDT status flags
    wdt->info->int_stage = 0x0;
    wdt->info->reset_stage = 0x0;
    wdt->info->flags = 0U;
    wdt->info->busy = 0x0;
    wdt->info->callback = NULL;
    wdt->info->workspace = NULL;

    return CSK_DRIVER_OK;
}

/**
 * @brief Controls power state of the Watchdog Timer (WDT).
 *
 * This function manages the power modes of the WDT, enabling or disabling it based on the specified power state.
 *
 * @param res A pointer to the WDT resources.
 * @param state The desired power state to be set for the WDT, as defined by CSK_POWER_STATE.
 *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure.
 */
int32_t
WDT_PowerControl(void* res, CSK_POWER_STATE state)
{
    CHECK_RESOURCES(res);
    WDT_RESOURCES* wdt = (WDT_RESOURCES*)res;

    switch (state) {
    case CSK_POWER_OFF:
        if ((wdt->info->flags & WDT_FLAG_INITIALIZED) == 0U) {
            return CSK_DRIVER_ERROR;
        }
        // Disable IRQ and clear pending interrupts
        disable_IRQ(wdt->irq_num);

        // Disable peripheral interrupt
        register_ISR(wdt->irq_num, NULL, NULL);

        // Disable WDT
        wdt->reg->REG_CTRL.bit.EN = 0x0;

        // Disable peripheral clock

        wdt->info->flags &= ~WDT_FLAG_POWERED;
        break;
    case CSK_POWER_LOW:
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    case CSK_POWER_FULL:
        // check API protocol
        if ((wdt->info->flags & WDT_FLAG_INITIALIZED) == 0U) {
            return CSK_DRIVER_ERROR;
        }
        if ((wdt->info->flags & WDT_FLAG_POWERED) != 0U) {
            return CSK_DRIVER_OK;
        }

        // TODO
        // Reset peripheral
        wdt->info->flags = WDT_FLAG_POWERED | WDT_FLAG_INITIALIZED;

        register_ISR(wdt->irq_num, wdt->irq_handler, NULL);
        enable_IRQ(wdt->irq_num);

        break;
    default:
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    }
    return CSK_DRIVER_OK;
}

/**
 * @brief Configures the Watchdog Timer (WDT) with specified settings.
 *
 * This function sets the operational parameters of the WDT, including the clock source, reset time, and
 * interrupt time based on the provided configuration struct.
 *
 * @param res A pointer to the WDT resources.
 * @param cfg Configuration struct containing the new settings for the WDT.
 *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure.
 */
int32_t
WDT_Control(void* res, hal_driver_wdt_cfg_t* cfg)
{
    CHECK_RESOURCES(res);
    WDT_RESOURCES* wdt = (WDT_RESOURCES*)res;

    // check API protocol
    if ((wdt->info->flags & WDT_FLAG_POWERED) == 0U) {
        // WDT not powered
        return CSK_DRIVER_ERROR;
    }

    if (wdt->info->busy){
        return CSK_DRIVER_ERROR_BUSY;
    }

    uint32_t cfg_value = 0;

    // clock source
    cfg_value = __RV_INSERT_FIELD(cfg_value, WDT_CTRL_CLKSEL_Msk, cfg->clk_src);
    // interrupt time
    cfg_value = __RV_INSERT_FIELD(cfg_value, WDT_CTRL_INTTIME_Msk, cfg->int_time);
    // reset time
    cfg_value = __RV_INSERT_FIELD(cfg_value, WDT_CTRL_RSTTIME_Msk, cfg->rst_time);

    wdt->info->int_stage = cfg->int_time;
    wdt->info->reset_stage = cfg->rst_time;

    // enable write and write register
    wdt->reg->REG_WREN.all = WDT_MAGIC_WRITE_PROTECTION;
    wdt->reg->REG_CTRL.all = cfg_value;

    // Set configured flag
    wdt->info->flags |= WDT_FLAG_CONFIGURED;

    return CSK_DRIVER_OK;
}

/**
 * @brief Updates the clock source for the Watchdog Timer (WDT).
 *
 * This function changes the clock source of the WDT to either an external source or an APB clock as specified.
 *
 * @param res A pointer to the WDT resources.
 * @param clk_src The new clock source as defined by hal_driver_wdt_clk_src.
 *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure.
 */
int32_t WDT_ClkSrc_Update(void* res, hal_driver_wdt_clk_src clk_src){
    CHECK_RESOURCES(res);
    WDT_RESOURCES* wdt = (WDT_RESOURCES*)res;

    // check API protocol
    if ((wdt->info->flags & WDT_FLAG_POWERED) == 0U) {
        // WDT not powered
        return CSK_DRIVER_ERROR;
    }

    if (wdt->info->busy){
        return CSK_DRIVER_ERROR_BUSY;
    }

    uint32_t cfg_value = wdt->reg->REG_CTRL.all;

    cfg_value = __RV_INSERT_FIELD(cfg_value, WDT_CTRL_CLKSEL_Msk, clk_src);

    wdt->reg->REG_WREN.all = WDT_MAGIC_WRITE_PROTECTION;
    wdt->reg->REG_CTRL.all = cfg_value;

    return CSK_DRIVER_OK;
}

/**
 * @brief Updates the interrupt time for the Watchdog Timer (WDT).
 *
 * This function sets the interrupt generation time of the WDT, adjusting how long the WDT waits
 * before generating an interrupt, based on the specified setting.
 *
 * @param res A pointer to the WDT resources.
 * @param int_time The new interrupt time as defined by hal_driver_wdt_int_time.
 *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure.
 */
int32_t WDT_IntTime_Update(void* res, hal_driver_wdt_int_time int_time){
    CHECK_RESOURCES(res);
    WDT_RESOURCES* wdt = (WDT_RESOURCES*)res;

    // check API protocol
    if ((wdt->info->flags & WDT_FLAG_POWERED) == 0U) {
        // WDT not powered
        return CSK_DRIVER_ERROR;
    }

    if (wdt->info->busy){
        return CSK_DRIVER_ERROR_BUSY;
    }

    uint32_t cfg_value = wdt->reg->REG_CTRL.all;

    cfg_value = __RV_INSERT_FIELD(cfg_value, WDT_CTRL_INTTIME_Msk, int_time);

    wdt->info->int_stage = int_time;

    wdt->reg->REG_WREN.all = WDT_MAGIC_WRITE_PROTECTION;
    wdt->reg->REG_CTRL.all = cfg_value;

    return CSK_DRIVER_OK;
}

/**
 * @brief Updates the reset time for the Watchdog Timer (WDT).
 *
 * This function adjusts the reset time of the WDT, determining the delay before the system is reset
 * after a timeout.
 *
 * @param res A pointer to the WDT resources.
 * @param rst_time The new reset time as defined by hal_driver_wdt_rst_time.
 *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure.
 */
int32_t WDT_RstTime_Update(void* res, hal_driver_wdt_rst_time rst_time){
    CHECK_RESOURCES(res);
    WDT_RESOURCES* wdt = (WDT_RESOURCES*)res;

    // check API protocol
    if ((wdt->info->flags & WDT_FLAG_POWERED) == 0U) {
        // WDT not powered
        return CSK_DRIVER_ERROR;
    }

    if (wdt->info->busy){
        return CSK_DRIVER_ERROR_BUSY;
    }

    uint32_t cfg_value = wdt->reg->REG_CTRL.all;

    cfg_value = __RV_INSERT_FIELD(cfg_value, WDT_CTRL_RSTEN_Msk, rst_time);

    wdt->info->reset_stage = rst_time;

    wdt->reg->REG_WREN.all = WDT_MAGIC_WRITE_PROTECTION;
    wdt->reg->REG_CTRL.all = cfg_value;

    return CSK_DRIVER_OK;
}

/**
 * @brief Enables the Watchdog Timer (WDT).
 *
 * This function activates the WDT to start monitoring the system. The WDT begins counting down based
 * on its configured parameters and will reset the system unless periodically refreshed.
 *
 * @param res A pointer to the WDT resources.
 *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure.
 */
int32_t
WDT_Enable(void* res)
{
    CHECK_RESOURCES(res);
    WDT_RESOURCES* wdt = (WDT_RESOURCES*)res;

    uint32_t cfg_value = 0U;

    // check API protocol
    if ((wdt->info->flags & WDT_FLAG_POWERED) == 0U) {
        // WDT not powered
        return CSK_DRIVER_ERROR;
    }

    if (wdt->info->busy == 1){
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
 * @brief Refreshes the Watchdog Timer (WDT).
 *
 * This function resets the countdown of the WDT to prevent the system from resetting. It must be called
 * periodically before the countdown expires to keep the system running.
 *
 * @param res A pointer to the WDT resources.
 *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure.
 */
int32_t
WDT_Refresh(void* res)
{
    CHECK_RESOURCES(res);
    WDT_RESOURCES* wdt = (WDT_RESOURCES*)res;

    // enable write
    wdt->reg->REG_WREN.all = WDT_MAGIC_WRITE_PROTECTION;
    // write restart register
    wdt->reg->REG_RESTART.all = WDT_MAGIC_RESTARTING;

    return CSK_DRIVER_OK;
}

/**
 * @brief Disables the Watchdog Timer (WDT).
 *
 * This function stops the WDT from counting down, effectively preventing it from resetting the system.
 * It is typically used during system shutdown or when the WDT is no longer required.
 *
 * @param res A pointer to the WDT resources.
 *
 * @return int32_t Returns 0 on success, or a non-zero error code on failure.
 */
int32_t 
WDT_Disable(void* res)
{
    CHECK_RESOURCES(res);
    WDT_RESOURCES* wdt = (WDT_RESOURCES*)res;

    // check API protocol
    if ((wdt->info->flags & WDT_FLAG_POWERED) == 0U) {
        // WDT not powered
        return CSK_DRIVER_ERROR;
    }

    // enable write and write control register
    wdt->reg->REG_WREN.all = WDT_MAGIC_WRITE_PROTECTION;
    wdt->reg->REG_CTRL.bit.EN = 0x0;

    wdt->info->busy = 0x0;

    return CSK_DRIVER_OK;
}

static void
WDT_IRQ_Handler(void){
    if (wdt0_resources.info->callback != NULL){
        wdt0_resources.info->callback(wdt0_resources.info->workspace);
    }
}

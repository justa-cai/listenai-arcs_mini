/**
 * @file    dual_timer.c
 * @author  ListenAI Application Team
 * @brief   Dual Timers Hardware Abstraction Layer (HAL) Driver Implementation
 *          This file implements the driver interface for the dual timer peripheral,
 *          providing functionality including initialization, power control,
 *          timer configuration, and interrupt handling.
 *
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
#include "dual_timer_reg.h"
#include "Driver_DUAL_TIMER.h"
#include "venusa_ap.h"

/* Private typedef -----------------------------------------------------------*/
/**
 * @brief Timer State Flags Bitmask Definitions
 * @details These flags track the state of the timer driver and individual channels.
 */
#define TIMER_FLAG_INITIALIZED      (1UL << 0)    ///< Driver has been initialized
#define TIMER_FLAG_POWERED          (1UL << 1)    ///< Timer is powered on
#define TIMER_FLAG_CH_ACTIVED(CH)   (1UL << (2 + CH)) ///< Channel CH is active

/**
 * @brief Maximum Number of Supported Channels
 * @details This driver supports up to two independent timer channels.
 */
#define CSK_TIMER_MAX_CHANNEL_NUM   (2)

/**
 * @brief Interrupt Clear Value
 * @details Magic value used to clear interrupt status registers.
 */
#define CSK_TIMER_CLR_INT           (0x951202)

/**
 * @brief Dual Timer Information Structure
 * @details Stores per-channel configuration and state information.
 */
typedef struct _TIMER_INFO {
    CSK_TIMER_SignalEvent_t cb_event;     ///< Event callback function
    void* workspace;                     ///< User context data pointer
    uint32_t reload;                     ///< Reload value for auto-reload
    uint32_t cnt;                        ///< Current counter value
} DUALTIMERS_INFO;

/**
 * @brief Dual Timer Resource Container
 * @details Centralized storage for register block, IRQ info, and channel data.
 */
typedef struct _TIMER_RESOURCES {
    DUAL_TIMER_RegDef* reg;              ///< Base address of timer registers
    uint32_t irq_num;                    ///< Associated IRQ number
    void (*irq_handler)(void);           ///< IRQ handler function pointer
    DUALTIMERS_INFO info[CSK_TIMER_MAX_CHANNEL_NUM]; ///< Per-channel info array
    uint32_t flags;                      ///< State flags bitmask
} DUALTIMERS_RESOURCES;

/* Private variables ---------------------------------------------------------*/
/**
 * @brief Driver Version Number
 * @details Follows CSK driver versioning scheme (major.minor).
 */
#define CSK_TIMER_DRV_VERSION    CSK_DRIVER_VERSION_MAJOR_MINOR(1, 0)

/**
 * @brief Driver Version Information Structure
 * @details Populated with API and driver version numbers.
 */
static const CSK_DRIVER_VERSION DriverVersion = {
    CSK_TIMER_API_VERSION,
    CSK_TIMER_DRV_VERSION
};

/**
 * @brief Dual Timer Instance 0 Resources
 * @details Preconfigured resource structure for first timer instance.
 */
static void DUALTIMERS0_IRQ_Handler(void);
static void DUALTIMERS1_IRQ_Handler(void);

static DUALTIMERS_RESOURCES dualtimers0_resources = {
        IP_DUALTIMERS0,
        IRQ_CMN_TIMER0_VECTOR,
        DUALTIMERS0_IRQ_Handler,
        {
                {0},
                {0},
        },
        0,
};

/**
 * @brief Dual Timer Instance 1 Resources
 * @details Preconfigured resource structure for second timer instance.
 */
static DUALTIMERS_RESOURCES dualtimers1_resources = {
        IP_DUALTIMERS1,
        IRQ_CMN_TIMER1_VECTOR,
        DUALTIMERS1_IRQ_Handler,
        {
                {0},
                {0},
        },
        0,
};

/**
 * @brief Resource Validation Macro
 * @details Checks if provided resource pointer matches valid instances.
 * @param res Pointer to resource structure to validate
 */
#define CHECK_RESOURCES(res)  do{\
        if((res != &dualtimers0_resources) && (res != &dualtimers1_resources)){\
            return CSK_DRIVER_ERROR_PARAMETER;\
        }\
}while(0)

/* Private functions ---------------------------------------------------------*/
/**
 * @fn DUALTIMERS0
 * @brief Get Address of First Timer Instance
 * @return Pointer to dualtimers0_resources structure
 */
void* DUALTIMERS0(void){
    return (void*)&dualtimers0_resources;
}

/**
 * @fn DUALTIMERS1
 * @brief Get Address of Second Timer Instance
 * @return Pointer to dualtimers1_resources structure
 */
void* DUALTIMERS1(void){
    return (void*)&dualtimers1_resources;
}

/**
 * @fn CSK_TIMER_GetVersion
 * @brief Get Driver Version Information
 * @return DriverVersion structure containing version numbers
 */
CSK_DRIVER_VERSION CSK_TIMER_GetVersion(void){
    return DriverVersion;
}

/**
 * @fn DUALTIMERS_Initialize
 * @brief Initialize Timer Driver
 * @param[in] res Pointer to timer resources structure
 * @return CSK_DRIVER_OK on success, error code otherwise
 * @details Resets all channel states and marks driver as initialized.
 *          Called automatically during first PowerControl(FULL) call.
 */
int32_t DUALTIMERS_Initialize(void* res){
    CHECK_RESOURCES(res);
    DUALTIMERS_RESOURCES* timer = (DUALTIMERS_RESOURCES*)res;

    if(timer->flags & TIMER_FLAG_INITIALIZED){
        return CSK_DRIVER_OK;
    }

    {
        uint32_t i = 0;
        for(i = 0; i < CSK_TIMER_MAX_CHANNEL_NUM; i++){
            timer->info[i].cb_event = NULL;
            timer->info[i].cnt = 0;
            timer->info[i].reload = 0;
            timer->info[i].workspace = NULL;
        }
    }

    timer->flags = TIMER_FLAG_INITIALIZED;

    return CSK_DRIVER_OK;
}

/**
 * @fn DUALTIMERS_Uninitialize
 * @brief Deinitialize Timer Driver
 * @param[in] res Pointer to timer resources structure
 * @return CSK_DRIVER_OK on success
 * @details Clears all channel states and removes initialized flag.
 *          Should be called before permanent driver removal.
 */
int32_t DUALTIMERS_Uninitialize(void* res){
    CHECK_RESOURCES(res);
    DUALTIMERS_RESOURCES* timer = (DUALTIMERS_RESOURCES*)res;

    {
        uint32_t i = 0;
        for(i = 0; i < CSK_TIMER_MAX_CHANNEL_NUM; i++){
            timer->info[i].cb_event = NULL;
            timer->info[i].cnt = 0;
            timer->info[i].reload = 0;
            timer->info[i].workspace = NULL;
        }
    }

    timer->flags = 0;

    return CSK_DRIVER_OK;
}

/**
 * @fn DUALTIMERS_PowerControl
 * @brief Control Timer Power State
 * @param[in] res Pointer to timer resources structure
 * @param[in] state Target power state (OFF/LOW/FULL)
 * @return CSK_DRIVER_OK on success, error code otherwise
 * @details Manages power modes and associated clock gating.
 *          Supports full power down and wakeup sequences.
 */
int32_t DUALTIMERS_PowerControl(void* res, CSK_POWER_STATE state){
    CHECK_RESOURCES(res);
    DUALTIMERS_RESOURCES* timer = (DUALTIMERS_RESOURCES*)res;

    switch (state)
    {
    case CSK_POWER_OFF:
        if ((timer->flags & TIMER_FLAG_INITIALIZED) == 0U) {
            return CSK_DRIVER_ERROR;
        }

        // Disable UART IRQ
        disable_IRQ(timer->irq_num);

        timer->flags &= ~TIMER_FLAG_POWERED;

        // Uninstall IRQ Handler
        register_ISR(timer->irq_num, NULL, NULL);

        break;

    case CSK_POWER_LOW:
        return CSK_DRIVER_ERROR_UNSUPPORTED;

    case CSK_POWER_FULL:
        if ((timer->flags & TIMER_FLAG_INITIALIZED) == 0U) {
            return CSK_DRIVER_ERROR;
        }
        if ((timer->flags & TIMER_FLAG_POWERED) != 0U) {
            return CSK_DRIVER_OK;
        }

        // TODO
        // Power and clock manager
        // Reset current peripheral

        // Clear interrupt pending status
        timer->reg->REG_INTCLR0.all = CSK_TIMER_CLR_INT;
        timer->reg->REG_INTCLR1.all = CSK_TIMER_CLR_INT;

        timer->flags = TIMER_FLAG_INITIALIZED | TIMER_FLAG_POWERED;

        // Configure interrupt handle
        register_ISR(timer->irq_num, timer->irq_handler, NULL);
        enable_IRQ(timer->irq_num);

        break;
    }
    return CSK_DRIVER_OK;
}

/**
 * @fn DUALTIMERS_Control
 * @brief Configure Timer Channel Parameters
 * @param[in] res Pointer to timer resources structure
 * @param[in] control Control flags (prescaler|size|mode|interrupt)
 * @param[in] ch Target channel number (0 or 1)
 * @return CSK_DRIVER_OK on success, error code otherwise
 * @details Applies multiple configuration settings simultaneously:
 *          - Prescaler selection (divide by 1/16/256)
 *          - Counter size (16-bit or 32-bit)
 *          - Operation mode (free running/periodic/one-shot)
 *          - Interrupt enable/disable
 */
int32_t DUALTIMERS_Control(void* res, uint32_t control, uint32_t ch){
    CHECK_RESOURCES(res);

    DUALTIMERS_RESOURCES* timer = (DUALTIMERS_RESOURCES*)res;

    if ((timer->flags & TIMER_FLAG_POWERED) == 0U) {
        return CSK_DRIVER_ERROR;
    }

    switch (control & CSK_TIMER_PRESCALE_Msk){
    case CSK_TIMER_PRESCALE_Divide_1:
        if(ch == CSK_TIMER_CHANNEL_0){
            timer->reg->REG_CONTROL0.bit.TIMERPRE = 0x0;
        }else if(ch == CSK_TIMER_CHANNEL_1){
            timer->reg->REG_CONTROL1.bit.TIMERPRE = 0x0;
        }
        break;
    case CSK_TIMER_PRESCALE_Divide_16:
        if(ch == CSK_TIMER_CHANNEL_0){
            timer->reg->REG_CONTROL0.bit.TIMERPRE = 0x1;
        }else if(ch == CSK_TIMER_CHANNEL_1){
            timer->reg->REG_CONTROL1.bit.TIMERPRE = 0x1;
        }
        break;
    case CSK_TIMER_PRESCALE_Divide_256:
        if(ch == CSK_TIMER_CHANNEL_0){
            timer->reg->REG_CONTROL0.bit.TIMERPRE = 0x2;
        }else if(ch == CSK_TIMER_CHANNEL_1){
            timer->reg->REG_CONTROL1.bit.TIMERPRE = 0x2;
        }
        break;
    }

    switch (control & CSK_TIMER_SIZE_Msk){
    case CSK_TIMER_SIZE_16Bit:
        if(ch == CSK_TIMER_CHANNEL_0){
            timer->reg->REG_CONTROL0.bit.SIZE = 0x0;
        }else if(ch == CSK_TIMER_CHANNEL_1){
            timer->reg->REG_CONTROL1.bit.SIZE = 0x0;
        }
        break;
    case CSK_TIMER_SIZE_32Bit:
        if(ch == CSK_TIMER_CHANNEL_0){
            timer->reg->REG_CONTROL0.bit.SIZE = 0x1;
        }else if(ch == CSK_TIMER_CHANNEL_1){
            timer->reg->REG_CONTROL1.bit.SIZE = 0x1;
        }
        break;
    }

    switch (control & CSK_TIMER_MODE_Msk){
    case CSK_TIMER_MODE_FreeRunning:
        if(ch == CSK_TIMER_CHANNEL_0){
            timer->reg->REG_CONTROL0.bit.MODE = 0x0;
            timer->reg->REG_CONTROL0.bit.ONESHOT = 0x0;
        }else if(ch == CSK_TIMER_CHANNEL_1){
            timer->reg->REG_CONTROL1.bit.MODE = 0x0;
            timer->reg->REG_CONTROL1.bit.ONESHOT = 0x0;
        }
        break;
    case CSK_TIMER_MODE_Periodic:
        if(ch == CSK_TIMER_CHANNEL_0){
            timer->reg->REG_CONTROL0.bit.MODE = 0x1;
            timer->reg->REG_CONTROL0.bit.ONESHOT = 0x0;
        }else if(ch == CSK_TIMER_CHANNEL_1){
            timer->reg->REG_CONTROL1.bit.MODE = 0x1;
            timer->reg->REG_CONTROL1.bit.ONESHOT = 0x0;
        }
        break;
    case CSK_TIMER_MODE_OneShot:
        if(ch == CSK_TIMER_CHANNEL_0){
            timer->reg->REG_CONTROL0.bit.ONESHOT = 0x1;
        }else if(ch == CSK_TIMER_CHANNEL_1){
            timer->reg->REG_CONTROL1.bit.ONESHOT = 0x1;
        }
        break;
    }

    switch (control & CSK_TIMER_INTERRUPT_Msk){
    case CSK_TIMER_INTERRUPT_Enabled:
        if(ch == CSK_TIMER_CHANNEL_0){
            timer->reg->REG_CONTROL0.bit.INT_ENABLE = 0x1;
        }else if(ch == CSK_TIMER_CHANNEL_1){
            timer->reg->REG_CONTROL1.bit.INT_ENABLE = 0x1;
        }
        break;
    case CSK_TIMER_INTERRUPT_Disabled:
        if(ch == CSK_TIMER_CHANNEL_0){
            timer->reg->REG_CONTROL0.bit.INT_ENABLE = 0x0;
        }else if(ch == CSK_TIMER_CHANNEL_1){
            timer->reg->REG_CONTROL1.bit.INT_ENABLE = 0x0;
        }
        break;
    }

    return CSK_DRIVER_OK;
}

/**
 * @fn DUALTIMERS_SetTimerCallback
 * @brief Set Channel Completion Callback
 * @param[in] res Pointer to timer resources structure
 * @param[in] channel Target channel number (0 or 1)
 * @param[in] cb_event Callback function pointer
 * @param[in] workspace User context data pointer
 * @return CSK_DRIVER_OK on success, error code otherwise
 * @details Associates an event handler with a specific timer channel.
 *          The callback will trigger when the timer completes a cycle.
 */
int32_t DUALTIMERS_SetTimerCallback(void* res, uint32_t channel, CSK_TIMER_SignalEvent_t cb_event, void* workspace){
    CHECK_RESOURCES(res);

    DUALTIMERS_RESOURCES* timer = (DUALTIMERS_RESOURCES*)res;

    if ((timer->flags & TIMER_FLAG_POWERED) == 0U) {
        return CSK_DRIVER_ERROR;
    }

    timer->info[channel].cb_event = cb_event;
    timer->info[channel].workspace = workspace;

    return CSK_DRIVER_OK;
}

/**
 * @fn DUALTIMERS_SetTimerPeriodByCount
 * @brief Set Timer Period Using Count Value
 * @param[in] res Pointer to timer resources structure
 * @param[in] channel Target channel number (0 or 1)
 * @param[in] count Desired period count value
 * @return CSK_DRIVER_OK on success, error code otherwise
 * @details Programs both current load and background load registers.
 *          The background load updates the counter when it reaches zero.
 */
int32_t DUALTIMERS_SetTimerPeriodByCount(void* res, uint32_t channel, uint32_t count){
    CHECK_RESOURCES(res);

    DUALTIMERS_RESOURCES* timer = (DUALTIMERS_RESOURCES*)res;

    if ((timer->flags & TIMER_FLAG_POWERED) == 0U) {
        return CSK_DRIVER_ERROR;
    }

    timer->info[channel].reload = count;
    // config load and bgload register
    if(channel == CSK_TIMER_CHANNEL_0){
        timer->reg->REG_LOAD0.all = count;    // this set current load register = count
        timer->reg->REG_BGLOAD0.all = count;  // this set load register = count when load register turn to 0
    }else if(channel == CSK_TIMER_CHANNEL_1){
        timer->reg->REG_LOAD1.all = count;    // this set current load register = count
        timer->reg->REG_BGLOAD1.all = count;  // this set load register = count when load register turn to 0
    }

    return CSK_DRIVER_OK;
}

/**
 * @fn DUALTIMERS_StartTimer
 * @brief Start Timer Channel Counting
 * @param[in] res Pointer to timer resources structure
 * @param[in] channel Target channel number (0 or 1)
 * @return CSK_DRIVER_OK on success, error code otherwise
 * @details Enables the selected timer channel and marks it as active.
 *          Only one start operation per channel is allowed until stopped.
 */
int32_t DUALTIMERS_StartTimer(void* res, uint32_t channel){
    CHECK_RESOURCES(res);

    DUALTIMERS_RESOURCES* timer = (DUALTIMERS_RESOURCES*)res;

    if ((timer->flags & TIMER_FLAG_POWERED) == 0U) {
        return CSK_DRIVER_ERROR;
    }

    if ((timer->flags & TIMER_FLAG_CH_ACTIVED(channel))) {
        return CSK_DRIVER_OK;
    }

    timer->flags = timer->flags | TIMER_FLAG_CH_ACTIVED(channel);

    // config enable register
    if(channel == CSK_TIMER_CHANNEL_0){
        timer->reg->REG_CONTROL0.bit.ENABLE = 0x1;
    }else if(channel == CSK_TIMER_CHANNEL_1){
        timer->reg->REG_CONTROL1.bit.ENABLE = 0x1;
    }

    return CSK_DRIVER_OK;
}

/**
 * @fn DUALTIMERS_ReadTimerCount
 * @brief Read Current Timer Count Value
 * @param[in] res Pointer to timer resources structure
 * @param[in] channel Target channel number (0 or 1)
 * @param[out] count Pointer to store current count value
 * @return CSK_DRIVER_OK on success, error code otherwise
 * @details Returns the current counter value for the specified channel.
 *          Returns 0 if channel is not active.
 */
int32_t DUALTIMERS_ReadTimerCount(void* res, uint32_t channel, uint32_t *count){
    CHECK_RESOURCES(res);

    DUALTIMERS_RESOURCES* timer = (DUALTIMERS_RESOURCES*)res;

    if ((timer->flags & TIMER_FLAG_POWERED) == 0U) {
        return CSK_DRIVER_ERROR;
    }

    if ((timer->flags & TIMER_FLAG_CH_ACTIVED(channel)) == 0) {
        *count = 0;
        return CSK_DRIVER_OK;
    }

    // get value from reg
    if(channel == CSK_TIMER_CHANNEL_0){
        *count = timer->reg->REG_VALUE0.all;
    }else if(channel == CSK_TIMER_CHANNEL_1){
        *count = timer->reg->REG_VALUE1.all;
    }

    return CSK_DRIVER_OK;
}

/**
 * @fn DUALTIMERS_StopTimer
 * @brief Stop Timer Channel Counting
 * @param[in] res Pointer to timer resources structure
 * @param[in] channel Target channel number (0 or 1)
 * @return CSK_DRIVER_OK on success, error code otherwise
 * @details Disenables the selected timer channel and clears active flag.
 *          Allows safe restart after stopping.
 */
int32_t DUALTIMERS_StopTimer(void* res, uint32_t channel){
    CHECK_RESOURCES(res);

    DUALTIMERS_RESOURCES* timer = (DUALTIMERS_RESOURCES*)res;

    if ((timer->flags & TIMER_FLAG_POWERED) == 0U) {
        return CSK_DRIVER_ERROR;
    }

    if ((timer->flags & TIMER_FLAG_CH_ACTIVED(channel)) == 0) {
        return CSK_DRIVER_OK;
    }

    timer->flags = timer->flags & (~TIMER_FLAG_CH_ACTIVED(channel));

    // config disable register
    if(channel == CSK_TIMER_CHANNEL_0){
        timer->reg->REG_CONTROL0.bit.ENABLE = 0x0;
    }else if(channel == CSK_TIMER_CHANNEL_1){
        timer->reg->REG_CONTROL1.bit.ENABLE = 0x0;
    }

    return CSK_DRIVER_OK;
}

/**
 * @fn DUALTIMERS_IRQ_Handler
 * @brief Common Interrupt Handler for Both Channels
 * @param[in] timer Pointer to timer resources structure
 * @details Processes interrupt status for both channels, clears flags,
 *          and invokes registered callbacks if configured.
 *          Called from both channel-specific IRQ handlers.
 */
static void
DUALTIMERS_IRQ_Handler(DUALTIMERS_RESOURCES *timer){
    uint32_t ret0, ret1;
    uint32_t event = 0;
    ret0 = timer->reg->REG_MIS0.all;
    ret1 = timer->reg->REG_MIS1.all;

    if(ret0){
        // clear interrupt status
        timer->reg->REG_INTCLR0.all = CSK_TIMER_CLR_INT;
        event = CSK_TIMER_EVENT_ONESTEP_COMPLETE_CH0;
        if(timer->info[0].cb_event){
            timer->info[0].cb_event(event, timer->info[0].workspace);
        }
    }

    event = 0;

    if(ret1){
        timer->reg->REG_INTCLR1.all = CSK_TIMER_CLR_INT;
        event = CSK_TIMER_EVENT_ONESTEP_COMPLETE_CH1;
        if(timer->info[1].cb_event){
            timer->info[1].cb_event(event, timer->info[1].workspace);
        }
    }
}

/**
 * @fn DUALTIMERS0_IRQ_Handler
 * @brief Interrupt Handler for First Timer Instance
 * @details Mapped to IRQ_CMN_TIMER0_VECTOR vector.
 *          Delegates processing to common handler.
 */
static void DUALTIMERS0_IRQ_Handler(void){
    DUALTIMERS_IRQ_Handler(&dualtimers0_resources);
}

/**
 * @fn DUALTIMERS1_IRQ_Handler
 * @brief Interrupt Handler for Second Timer Instance
 * @details Mapped to IRQ_CMN_TIMER1_VECTOR vector.
 *          Delegates processing to common handler.
 */
static void DUALTIMERS1_IRQ_Handler(void){
    DUALTIMERS_IRQ_Handler(&dualtimers1_resources);
}

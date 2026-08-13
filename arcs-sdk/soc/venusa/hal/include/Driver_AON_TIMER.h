/*!
 * @file Driver_AON_TIMER.h
 * @brief Always-On Timer (AON TIMER) Driver Interface Headers
 *        This file contains the interface definitions for AON Timer hardware abstraction layer.
 *        Provides initialization, control, and monitoring functions for low-power timer peripheral.
 *        Created on: May 15, 2025
 *        Author: USER
 */

#ifndef DRIVER_AON_TIMER_H_
#define DRIVER_AON_TIMER_H_

#include "Driver_Common.h"
#include "venusa_ap.h"

/** @defgroup AON_TIMER AON Timer
  * @brief AON_TIMER HAL module driver
  * @{
  */

/* Exported constants --------------------------------------------------------*/
/** @defgroup AON_TIMER_Exported_Constants AON TIMER Exported Constants
  * @{
  */

/** @defgroup AON_TIMER_Mode AON TIMER Mode
  * @{
  */
#define HAL_AON_TIMER_MODE_Pos                  (0)    ///< Bit position for mode selection
#define HAL_AON_TIMER_MODE_Msk                  (3UL << HAL_AON_TIMER_MODE_Pos) ///< Mode selection mask
#define HAL_AON_TIMER_MODE_Wrapping             (0UL << HAL_AON_TIMER_MODE_Pos) ///< Free-running wraparound mode
#define HAL_AON_TIMER_MODE_Repeat               (1UL << HAL_AON_TIMER_MODE_Pos) ///< Auto-reload repeating mode
#define HAL_AON_TIMER_MODE_Normal               (2UL << HAL_AON_TIMER_MODE_Pos) ///< Single-cycle normal mode
/** @} */ /* End of group AON_TIMER_Mode */

/** @defgroup AON_TIMER_Interrupt AON TIMER Interrupt
  * @{
  */
#define HAL_AON_TIMER_INTERRUPT_Pos             (2)    ///< Interrupt enable bit position
#define HAL_AON_TIMER_INTERRUPT_Msk             (1UL << HAL_AON_TIMER_INTERRUPT_Pos) ///< Interrupt enable mask
#define HAL_AON_TIMER_INTERRUPT_Enabled         (0UL << HAL_AON_TIMER_INTERRUPT_Pos) ///< Interrupt disabled
#define HAL_AON_TIMER_INTERRUPT_Disabled        (1UL << HAL_AON_TIMER_INTERRUPT_Pos) ///< Interrupt enabled
/** @} */ /* End of group AON_TIMER_Interrupt */

/** @defgroup AON_TIMER_CLK_SEL AON TIMER Clock source selection register fields
  * @{
  */
#define HAL_AON_TIMER_CLK_SEL_Pos				(3)    ///< Clock source selection bit position
#define HAL_AON_TIMER_CLK_SEL_Mask				(3UL << HAL_AON_TIMER_CLK_SEL_Pos)   ///< Clock source selection mask
#define HAL_AON_TIMER_CLK_SEL_Rc32K				(0UL << HAL_AON_TIMER_CLK_SEL_Pos)   ///< 32kHz relaxation oscillator
#define HAL_AON_TIMER_CLK_SEL_XO24M_Div32K		(1UL << HAL_AON_TIMER_CLK_SEL_Pos)   ///< External 24MHz divided by 32kHz
#define HAL_AON_TIMER_CLK_SEL_RC24M_Div32K		(2UL << HAL_AON_TIMER_CLK_SEL_Pos)   ///< Internal 24MHz divided by 32kHz
/** @} */ /* End of group AON_TIMER_CLK_SEL */

/// Timer load value constraints
#define AON_TIMER_LOAD_VALUE_MASK               (0xFFFFFF) ///< Maximum allowed load value mask

/** @defgroup AON_TIMER_Events AON TIMER Events
  * @{
  */
#define HAL_AON_TIMER_EVENT_COMPLETE            (1UL << 0) ///< Timer event completion flag bit
/** @} */ /* End of group AON_TIMER_Events */

/**
  * @}
  */ /* End of group AON_TIMER_Exported_Constants */

/* Exported types ------------------------------------------------------------*/
/** @defgroup AON_TIMER_Exported_Types AON TIMER Exported Types
  * @{
  */

/**
 * @typedef HAL_AON_TIMER_SignalEvent_t
 * @brief Event callback function pointer type
 * @param[in] event Event identifier
 * @param[in] workspace User context pointer
 */
typedef void (*HAL_AON_TIMER_SignalEvent_t) (uint32_t event, void* workspace);

/**
 * @struct AON_TIMER_Info_t
 * @brief Timer instance configuration structure
 * @var cb_event Event handler callback function
 * @var workspace User-defined context data
 * @var reload_cnt Initial reload count value
 * @var run_mode Operation mode flags
 */
typedef struct {
    HAL_AON_TIMER_SignalEvent_t cb_event; ///< Event notification callback
    void *workspace;                     ///< Client application context
    uint32_t reload_cnt;                 ///< Initial counter reload value
    uint32_t run_mode;                   ///< Run mode configuration bits
} AON_TIMER_Info_t;

/**
 * @enum AON_TIMER_State_t
 * @brief Timer driver state machine
 * @var AON_TIMER_UNINITIALIZED Driver not initialized
 * @var AON_TIMER_INITIALIZED Driver successfully initialized
 * @var AON_TIMER_POWERED Peripheral powered ON
 */
typedef enum {
    AON_TIMER_UNINITIALIZED = 0,         ///< Driver uninitialized state
    AON_TIMER_INITIALIZED   = (1U << 0),///< Driver initialized successfully
    AON_TIMER_POWERED       = (1U << 1),///< Peripheral powered ON
} AON_TIMER_State_t;

/**
 * @struct AON_TIMER_Resources_t
 * @brief Timer instance resources structure
 * @var reg Register map base address
 * @var irq_num Interrupt number assignment
 * @var irq_handler Interrupt service routine
 * @var info Associated configuration data
 * @var state Current driver state
 */
typedef struct {
    AON_TIMER_RegDef* reg;               ///< Peripheral register base address
    uint32_t irq_num;                    ///< Assigned interrupt vector number
    void (*irq_handler)(void);           ///< Interrupt handler function
    AON_TIMER_Info_t* info;              ///< Configuration parameters
    AON_TIMER_State_t state;             ///< Current driver state
} AON_TIMER_Resources_t;

/**
  * @}
  */ /* End of group AON_TIMER_Exported_Types */


/* Exported macros -----------------------------------------------------------*/
/** @defgroup AON_TIMER_Exported_Macros AON TIMER Exported Macros
  * @{
  */

/**
 * @brief Resource validation macro
 * @param[in] res Resource handle to validate
 * @note Must compare against global aon_timer_resources instance
 * @return Returns error code if validation fails
 */
#define CHECK_RESOURCES(res)    do{ \
    if(res != &aon_timer_resources){ \
        return CSK_DRIVER_ERROR_PARAMETER; \
    } \
} while(0)

/**
  * @}
  */ /* End of group AON_TIMER_Exported_Macros */


/* Exported functions --------------------------------------------------------*/
/** @defgroup AON_TIMER_Exported_Functions AON TIMER Exported Functions
  * @{
  */

/**
 * @fn int32_t AON_TIMER_Initialize(void* res, HAL_AON_TIMER_SignalEvent_t cb_event, void* workspace)
 * @brief Initializes the AON Timer driver
 * @param[in] res Device resource handle
 * @param[in] cb_event Event callback function
 * @param[in] workspace User context pointer
 * @return CSK_DRIVER_SUCCESS on success, negative error code otherwise
 */
int32_t AON_TIMER_Initialize(void* res, HAL_AON_TIMER_SignalEvent_t cb_event, void* workspace);

/**
 * @fn int32_t AON_TIMER_Uninitialize(void* res)
 * @brief Deinitializes the AON Timer driver
 * @param[in] res Device resource handle
 * @return CSK_DRIVER_SUCCESS on success, negative error code otherwise
 */
int32_t AON_TIMER_Uninitialize(void* res);

/**
 * @fn int32_t AON_TIMER_PowerControl(void* res, CSK_POWER_STATE state)
 * @brief Controls power state of the AON Timer
 * @param[in] res Device resource handle
 * @param[in] state Target power state
 * @return CSK_DRIVER_SUCCESS on success, negative error code otherwise
 */
int32_t AON_TIMER_PowerControl(void* res, CSK_POWER_STATE state);

/**
 * @fn int32_t AON_TIMER_Control(void* res, uint32_t control)
 * @brief Configures timer control settings
 * @param[in] res Device resource handle
 * @param[in] control Control flags (mode, clock source, etc.)
 * @return CSK_DRIVER_SUCCESS on success, negative error code otherwise
 */
int32_t AON_TIMER_Control(void* res, uint32_t control);

/**
 * @fn int32_t AON_TIMER_SetTimerPeriodByCount(void* res, uint32_t count)
 * @brief Sets timer period using direct count value
 * @param[in] res Device resource handle
 * @param[in] count Number of clock cycles for period
 * @return CSK_DRIVER_SUCCESS on success, negative error code otherwise
 */
int32_t AON_TIMER_SetTimerPeriodByCount(void* res, uint32_t count);

/**
 * @fn int32_t AON_TIMER_StartTimer(void* res)
 * @brief Starts the AON Timer operation
 * @param[in] res Device resource handle
 * @return CSK_DRIVER_SUCCESS on success, negative error code otherwise
 */
int32_t AON_TIMER_StartTimer(void* res);

/**
 * @fn int32_t AON_TIMER_StopTimer(void* res)
 * @brief Stops the AON Timer operation
 * @param[in] res Device resource handle
 * @return CSK_DRIVER_SUCCESS on success, negative error code otherwise
 */
int32_t AON_TIMER_StopTimer(void* res);

/**
 * @fn int32_t AON_TIMER_ReadTimerCount(void* res, uint32_t *count)
 * @brief Reads current timer count value
 * @param[in] res Device resource handle
 * @param[out] count Pointer to store current count
 * @return CSK_DRIVER_SUCCESS on success, negative error code otherwise
 */
int32_t AON_TIMER_ReadTimerCount(void* res, uint32_t *count);

/**
 * @fn void* AON_TIMER(void)
 * @brief Gets default AON Timer resource handle
 * @return Pointer to default resource structure
 */
void* AON_TIMER(void);
/**
  * @}
  */ /* End of group AON_TIMER_Exported_Functions */

/** @} */ /* End of AON_TIMER group */

#endif /* DRIVER_AON_TIMER_H_ */

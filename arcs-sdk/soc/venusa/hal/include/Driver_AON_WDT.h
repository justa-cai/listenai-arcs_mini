/*!
 * @file Driver_AON_WDT.h
 * @brief Always-On Watchdog Timer (AON WDT) Driver Interface Headers
 *        This file contains the interface definitions for AON Watchdog Timer hardware abstraction layer.
 *        Provides initialization, control, and monitoring functions for low-power watchdog timer peripheral.
 *        Created on: May 27, 2025
 *        Author: USER
 */

#ifndef DRIVER_AON_WDT_H_
#define DRIVER_AON_WDT_H_

#include "Driver_Common.h"
#include "venusa_ap.h"

/** @defgroup AON_WDT
  * @brief AON_WDT HAL module driver
  * @{
  */

/* Exported constants --------------------------------------------------------*/
/** @defgroup AON_WDT_Exported_Constants AON WDT Exported Constants
  * @{
  */
/** @defgroup AON_WDT_Time AON WDT Time Configuration
  * @{
  */

#define AON_WDT_LOAD_VALUE_MASK                     (0xFFFFFF) ///< Maximum allowed load value mask

/// Status flags bit positions
#define AON_WDT_FLAG_INITIALIZED                    (1UL << 0) ///< Driver initialization complete flag
#define AON_WDT_FLAG_POWERED                        (1UL << 1) ///< Peripheral powered ON flag

/// Domain control reset codes
#define AON_WDT_DOMAIN_CTRL_RESET_PMU               0xcafee201 ///< PMU domain reset code
#define AON_WDT_DOMAIN_CTRL_RESET_DBB               0xcafee200 ///< DBB domain reset code

/// Stop protection keys
#define AON_WDT_STOP_PROTECT_LOCK                   0xDEADFACE ///< Stop protection lock key
#define AON_WDT_STOP_PROTECT_RELEASE                0xBABEBEEF ///< Stop protection release key

/// Start protection keys
#define AON_WDT_START_PROTECT_LOCK                  0xBADBEE01 ///< Start protection lock key
#define AON_WDT_START_PROTECT_RELEASE               0xBADBEE00 ///< Start protection release key

/// Time configuration register fields
#define HAL_AON_WDT_TIME_Pos                    	0          ///< Time configuration bit position
#define HAL_AON_WDT_TIME_Msk                   		(0x1UL << HAL_AON_WDT_TIME_Pos) ///< Time configuration mask
#define HAL_AON_WDT_TIME_CFG                   		(0x1UL << HAL_AON_WDT_TIME_Pos)   ///< Time configuration enable bit
/** @} */ /* End of group AON_WDT_Time */

/** @defgroup AON_WDT_Interrupt AON WDT Interrupt
  * @{
  */
#define HAL_AON_WDT_INTERRUPT_Pos               	1           ///< Interrupt enable bit position
#define HAL_AON_WDT_INTERRUPT_Msk              		(0x1UL << HAL_AON_WDT_INTERRUPT_Pos) ///< Interrupt enable mask
#define HAL_AON_WDT_INTERRUPT_EN               		(0x1UL << HAL_AON_WDT_INTERRUPT_Pos) ///< Interrupt enable bit
/** @} */ /* End of group AON_WDT_Interrupt */

/** @defgroup AON_WDT_Mode AON WDT Mode Control
  * @{
  */
#define HAL_AON_WDT_MODE_CTRL_Pos              		2           ///< Mode control bit position
#define HAL_AON_WDT_MODE_CTRL_Msk              		(0x3UL << HAL_AON_WDT_MODE_CTRL_Pos) ///< Mode control mask
#define HAL_AON_WDT_CTRL_RESET_MODE            		(0x1UL << HAL_AON_WDT_MODE_CTRL_Pos) ///< Reset mode selection
#define HAL_AON_WDT_CTRL_INT_MODE              		(0x2UL << HAL_AON_WDT_MODE_CTRL_Pos) ///< Interrupt mode selection
/** @} */ /* End of group AON_WDT_Mode */

/** @defgroup AON_WDT_Reset_Domain AON WDT Reset Domain
  * @{
  */
#define HAL_AON_WDT_RST_DOMAIN_Pos              	4           ///< Reset domain selection bit position
#define HAL_AON_WDT_RST_DOMAIN_Msk             		(0x3UL << HAL_AON_WDT_RST_DOMAIN_Pos) ///< Reset domain selection mask
#define HAL_AON_WDT_RST_CORE_DOMAIN            		(0x1UL << HAL_AON_WDT_RST_DOMAIN_Pos) ///< Core domain reset
#define HAL_AON_WDT_RST_PMU_DOMAIN             		(0x2UL << HAL_AON_WDT_RST_DOMAIN_Pos) ///< PMU domain reset
/** @} */ /* End of group AON_WDT_Reset_Domain */

/**
  * @}
  */ /* End of group AON_WDT_Exported_Constants */

/* Exported macros -----------------------------------------------------------*/
/** @defgroup AON_WDT_Exported_Macros AON WDT Exported Macros
  * @{
  */

/**
 * @brief Resource validation macro for AON WDT
 * @param[in] res Resource handle to validate
 * @note Must compare against global aon_wdt_resources instance
 * @return Returns error code if validation fails
 */
#define AON_WDT_CHECK_RESOURCES(res)  do{\
        if(res != &aon_wdt_resources){\
            return CSK_DRIVER_ERROR_PARAMETER;\
        }\
}while(0)
/**
  * @}
  */ /* End of group AON_WDT_Exported_Macros */

/* Exported types ------------------------------------------------------------*/
/** @defgroup AON_WDT_Exported_Types AON WDT Exported Types
  * @{
  */

/**
 * @typedef HAL_AON_WDT_SignalEvent_t
 * @brief Event callback function pointer type for AON WDT
 * @param[in] workspace User-defined context data pointer
 */
typedef void (*HAL_AON_WDT_SignalEvent_t) (void* workspace);

/**
 * @struct AON_WDT_Info_t
 * @brief Watchdog timer instance configuration structure
 * @var cb_event Event notification callback function
 * @var workspace User-defined context data pointer
 */
typedef struct {
    HAL_AON_WDT_SignalEvent_t cb_event; ///< Event notification callback
    void *workspace;                    ///< Client application context
} AON_WDT_Info_t;

/**
 * @enum AON_WDT_State_t
 * @brief Watchdog timer driver state machine
 * @var AON_WDT_UNINITIALIZED Driver not initialized
 * @var AON_WDT_INITIALIZED Driver successfully initialized
 * @var AON_WDT_POWERED Peripheral powered ON
 */
typedef enum {
    AON_WDT_UNINITIALIZED = 0,         ///< Driver uninitialized state
    AON_WDT_INITIALIZED   = (1U << 0),///< Driver initialized successfully
    AON_WDT_POWERED       = (1U << 1),///< Peripheral powered ON
} AON_WDT_State_t;

/**
 * @struct AON_WDT_Resource_t
 * @brief Watchdog timer instance resources structure
 * @var reg Register map base address
 * @var irq_num Assigned interrupt vector number
 * @var irq_handler Interrupt service routine function pointer
 * @var info Associated configuration data
 * @var state Current driver state
 */
typedef struct {
    AON_WDT_RegDef* reg;               ///< Peripheral register base address
    uint32_t irq_num;                  ///< Assigned interrupt vector number
    void (*irq_handler)(void);        ///< Interrupt handler function
    AON_WDT_Info_t* info;             ///< Configuration parameters
    AON_WDT_State_t state;            ///< Current driver state
} AON_WDT_Resource_t;
/**
  * @}
  */ /* End of group AON_WDT_Exported_Types */

/* Exported functions --------------------------------------------------------*/
/** @defgroup AON_WDT_Exported_Functions AON WDT Exported Functions
  * @{
  */

/**
 * @fn void* AON_WDT(void)
 * @brief Gets default AON Watchdog Timer resource handle
 * @return Pointer to default resource structure
 */
void* AON_WDT(void);

/**
 * @fn int32_t AON_WDT_Initialize(void* res, HAL_AON_WDT_SignalEvent_t callback, void* workspace)
 * @brief Initializes the AON Watchdog Timer driver
 * @param[in] res Device resource handle
 * @param[in] callback Event callback function
 * @param[in] workspace User context pointer
 * @return CSK_DRIVER_SUCCESS on success, negative error code otherwise
 */
int32_t AON_WDT_Initialize(void* res, HAL_AON_WDT_SignalEvent_t callback, void* workspace);

/**
 * @fn int32_t AON_WDT_Uninitialize(void* res)
 * @brief Deinitializes the AON Watchdog Timer driver
 * @param[in] res Device resource handle
 * @return CSK_DRIVER_SUCCESS on success, negative error code otherwise
 */
int32_t AON_WDT_Uninitialize(void* res);

/**
 * @fn int32_t AON_WDT_PowerControl(void* res, CSK_POWER_STATE state)
 * @brief Controls power state of the AON Watchdog Timer
 * @param[in] res Device resource handle
 * @param[in] state Target power state
 * @return CSK_DRIVER_SUCCESS on success, negative error code otherwise
 */
int32_t AON_WDT_PowerControl(void* res, CSK_POWER_STATE state);

/**
 * @fn int32_t AON_WDT_Control(void* res, uint32_t control, uint32_t arg)
 * @brief Configures watchdog timer control settings
 * @param[in] res Device resource handle
 * @param[in] control Control command flags
 * @param[in] arg Command argument (depends on control type)
 * @return CSK_DRIVER_SUCCESS on success, negative error code otherwise
 */
int32_t AON_WDT_Control(void* res, uint32_t control, uint32_t arg);

/**
 * @fn int32_t AON_WDT_ReadLoadValue(void* res, uint32_t *load_value)
 * @brief Reads current watchdog timer load value
 * @param[in] res Device resource handle
 * @param[out] load_value Pointer to store current load value
 * @return CSK_DRIVER_SUCCESS on success, negative error code otherwise
 */
int32_t AON_WDT_ReadLoadValue(void* res, uint32_t *load_value);

/**
 * @fn int32_t AON_WDT_Enable(void* res)
 * @brief Enables the watchdog timer functionality
 * @param[in] res Device resource handle
 * @return CSK_DRIVER_SUCCESS on success, negative error code otherwise
 */
int32_t AON_WDT_Enable(void* res);

/**
 * @fn int32_t AON_WDT_Disable(void* res)
 * @brief Disables the watchdog timer functionality
 * @param[in] res Device resource handle
 * @return CSK_DRIVER_SUCCESS on success, negative error code otherwise
 */
int32_t AON_WDT_Disable(void* res);

/**
 * @fn int32_t AON_WDT_Refresh(void* res)
 * @brief Manually refreshes the watchdog timer counter
 * @param[in] res Device resource handle
 * @return CSK_DRIVER_SUCCESS on success, negative error code otherwise
 */
int32_t AON_WDT_Refresh(void* res);
/**
  * @}
  */ /* End of group AON_WDT_Exported_Functions */

/** @} */ /* End of AON_WDT group */

#endif /* DRIVER_AON_WDT_H_ */

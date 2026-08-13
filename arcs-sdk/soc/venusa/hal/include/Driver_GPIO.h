/**
 * @file    Driver_GPIO.h
 * @author  USER
 * @brief   Header file for GPIO Driver HAL module
 * @details This file contains all necessary definitions and function declarations
 *          for the General Purpose Input/Output (GPIO) driver implementation.
 * @copyright Copyright (c) 2022.5.5. All rights reserved.
 * @license BSD 3-Clause License (see LICENSE file)
 */

#ifndef DRIVER_GPIO_H
#define DRIVER_GPIO_H

#include "Driver_Common.h"

/** @defgroup GPIO
  * @brief GPIO HAL module driver
  * @{
  */

/* Exported constants --------------------------------------------------------*/
/** @defgroup GPIO_Exported_Constants GPIO Exported Constants
  * @{
  */
/** @defgroup GPIO_API_Version GPIO API Version
  * @{
  */
/**
 * @def CSK_GPIO_API_VERSION
 * @brief API version number following major.minor format
 * @details Indicates compatibility requirements between application code
 *         and driver implementation. Must match exactly for proper operation.
 */
#define CSK_GPIO_API_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1, 0)
/** @} */ /* End of group GPIO_API_Version */

/** @defgroup GPIO_CTRL GPIO direction control macros
  * @{
  */

/**
 * @def CSK_GPIO_DIR_INPUT
 * @brief Configure GPIO pin as input
 * @details Sets data direction register bit to input mode
 */
#define CSK_GPIO_DIR_INPUT              (0x0)
/**
 * @def CSK_GPIO_DIR_OUTPUT
 * @brief Configure GPIO pin as output
 * @details Sets data direction register bit to output mode
 */
#define CSK_GPIO_DIR_OUTPUT             (0x1)

// GPIO interrupt control macros
/**
 * @def CSK_GPIO_INTR_Pos
 * @brief Position of interrupt enable field in control register
 */
#define CSK_GPIO_INTR_Pos               0
/**
 * @def CSK_GPIO_INTR_Mask
 * @brief Bit mask for interrupt enable field
 */
#define CSK_GPIO_INTR_Mask              (0x3UL << CSK_GPIO_INTR_Pos)
/**
 * @def CSK_GPIO_INTR_ENABLE
 * @brief Enable GPIO interrupt generation
 * @details Sets interrupt enable bits to active state
 */
#define CSK_GPIO_INTR_ENABLE            (2UL << CSK_GPIO_INTR_Pos)
/**
 * @def CSK_GPIO_INTR_DISABLE
 * @brief Disable GPIO interrupt generation
 * @details Clears interrupt enable bits to disable state
 */
#define CSK_GPIO_INTR_DISABLE           (1UL << CSK_GPIO_INTR_Pos)

/**
 * @def CSK_GPIO_SET_INTR_Pos
 * @brief Position of interrupt type selection field in control register
 */
#define CSK_GPIO_SET_INTR_Pos           2
/**
 * @def CSK_GPIO_SET_INTR_Mask
 * @brief Bit mask for interrupt type selection field
 */
#define CSK_GPIO_SET_INTR_Mask          (0x7UL << CSK_GPIO_SET_INTR_Pos)
/**
 * @def CSK_GPIO_SET_INTR_LOW_LEVEL
 * @brief Select low level detection for interrupt trigger
 */
#define CSK_GPIO_SET_INTR_LOW_LEVEL     (1UL << CSK_GPIO_SET_INTR_Pos)
/**
 * @def CSK_GPIO_SET_INTR_HIGH_LEVEL
 * @brief Select high level detection for interrupt trigger
 */
#define CSK_GPIO_SET_INTR_HIGH_LEVEL    (2UL << CSK_GPIO_SET_INTR_Pos)
/**
 * @def CSK_GPIO_SET_INTR_NEGATIVE_EDGE
 * @brief Select falling edge detection for interrupt trigger
 */
#define CSK_GPIO_SET_INTR_NEGATIVE_EDGE (3UL << CSK_GPIO_SET_INTR_Pos)
/**
 * @def CSK_GPIO_SET_INTR_POSITIVE_EDGE
 * @brief Select rising edge detection for interrupt trigger
 */
#define CSK_GPIO_SET_INTR_POSITIVE_EDGE (4UL << CSK_GPIO_SET_INTR_Pos)
/**
 * @def CSK_GPIO_SET_INTR_DUAL_EDGE
 * @brief Select both edges detection for interrupt trigger
 */
#define CSK_GPIO_SET_INTR_DUAL_EDGE     (5UL << CSK_GPIO_SET_INTR_Pos)

// GPIO pull configuration macros
/**
 * @def CSK_GPIO_MODE_Pos
 * @brief Position of pull-up/down configuration field in control register
 */
#define CSK_GPIO_MODE_Pos               5
/**
 * @def CSK_GPIO_MODE_Mask
 * @brief Bit mask for pull configuration field
 */
#define CSK_GPIO_MODE_Mask              (0x3UL << CSK_GPIO_MODE_Pos)
/**
 * @def CSK_GPIO_MODE_PULL_UP
 * @brief Enable internal pull-up resistor
 * @details Connects internal resistor between supply voltage and pin
 */
#define CSK_GPIO_MODE_PULL_UP           (2UL << CSK_GPIO_MODE_Pos)
/**
 * @def CSK_GPIO_MODE_PULL_DOWN
 * @brief Enable internal pull-down resistor
 * @details Connects internal resistor between ground and pin
 */
#define CSK_GPIO_MODE_PULL_DOWN         (3UL << CSK_GPIO_MODE_Pos)
/**
 * @def CSK_GPIO_MODE_PULL_NONE
 * @brief No internal pull-up or pull-down resistor
 * @details Leaves pin floating without internal termination
 */
#define CSK_GPIO_MODE_PULL_NONE         (1UL << CSK_GPIO_MODE_Pos)

// Debouncing clock source selection macros
/**
 * @def CSK_GPIO_DEBOUNCE_CLK_Pos
 * @brief Position of debounce clock source selection field
 */
#define CSK_GPIO_DEBOUNCE_CLK_Pos       7
/**
 * @def CSK_GPIO_DEBOUNCE_CLK_Mask
 * @brief Bit mask for debounce clock source selection
 */
#define CSK_GPIO_DEBOUNCE_CLK_Mask      (0x3UL << CSK_GPIO_DEBOUNCE_CLK_Pos)
/**
 * @def CSK_GPIO_DEBOUNCE_CLK_EXT
 * @brief Use external crystal oscillator for debouncing timer
 * @details Provides stable timebase independent of CPU frequency
 */
#define CSK_GPIO_DEBOUNCE_CLK_EXT       (1UL << CSK_GPIO_DEBOUNCE_CLK_Pos)
/**
 * @def CSK_GPIO_DEBOUNCE_CLK_PCLK
 * @brief Use peripheral clock (PCLK) for debouncing timer
 * @details Timebase proportional to system bus speed
 */
#define CSK_GPIO_DEBOUNCE_CLK_PCLK      (2UL << CSK_GPIO_DEBOUNCE_CLK_Pos)

// Debouncing enable/disable macros
/**
 * @def CSK_GPIO_DEBOUNCE_Pos
 * @brief Position of debounce enable field in control register
 */
#define CSK_GPIO_DEBOUNCE_Pos           9
/**
 * @def CSK_GPIO_DEBOUNCE_Mask
 * @brief Bit mask for debounce enable field
 */
#define CSK_GPIO_DEBOUNCE_Mask          (0x3UL << CSK_GPIO_DEBOUNCE_Pos)
/**
 * @def CSK_GPIO_DEBOUNCE_ENABLE
 * @brief Enable hardware debouncing functionality
 * @details Digital filtering removes mechanical switch bounce effects
 */
#define CSK_GPIO_DEBOUNCE_ENABLE        (2UL << CSK_GPIO_DEBOUNCE_Pos)
/**
 * @def CSK_GPIO_DEBOUNCE_DISABLE
 * @brief Disable hardware debouncing functionality
 * @details Directly processes raw pin transitions without filtering
 */
#define CSK_GPIO_DEBOUNCE_DISABLE       (1UL << CSK_GPIO_DEBOUNCE_Pos)

// Control register special feature macros
/**
 * @def CSK_GPIO_CONTROL_Pos
 * @brief Position of special control features field
 */
#define CSK_GPIO_CONTROL_Pos            11
/**
 * @def CSK_GPIO_CONTROL_Mask
 * @brief Bit mask for special control features field
 */
#define CSK_GPIO_CONTROL_Mask           (0x1UL << CSK_GPIO_CONTROL_Pos)
/**
 * @def CSK_GPIO_DEBOUNCE_SCALE
 * @brief Scale factor adjustment for debounce counter
 * @details Affects duration threshold for valid signal stabilization
 */
#define CSK_GPIO_DEBOUNCE_SCALE         (0x1UL << CSK_GPIO_CONTROL_Pos)

/** @} */ /* End of group GPIO_CTRL */
/**
  * @}
  */ /* End of group GPIO_Exported_Constants */

/* Exported macros -----------------------------------------------------------*/
/** @defgroup GPIO_Exported_Macros GPIO Exported Macros
  * @{
  */
#define CSK_GPIO_PIN0             (1UL << 0)  /**< Pin 0 select mask */
#define CSK_GPIO_PIN1             (1UL << 1)  /**< Pin 1 select mask */
#define CSK_GPIO_PIN2             (1UL << 2)  /**< Pin 2 select mask */
#define CSK_GPIO_PIN3             (1UL << 3)  /**< Pin 3 select mask */
#define CSK_GPIO_PIN4             (1UL << 4)  /**< Pin 4 select mask */
#define CSK_GPIO_PIN5             (1UL << 5)  /**< Pin 5 select mask */
#define CSK_GPIO_PIN6             (1UL << 6)  /**< Pin 6 select mask */
#define CSK_GPIO_PIN7             (1UL << 7)  /**< Pin 7 select mask */
#define CSK_GPIO_PIN8             (1UL << 8)  /**< Pin 8 select mask */
#define CSK_GPIO_PIN9             (1UL << 9)  /**< Pin 9 select mask */
#define CSK_GPIO_PIN10            (1UL << 10) /**< Pin 10 select mask */
#define CSK_GPIO_PIN11            (1UL << 11) /**< Pin 11 select mask */
#define CSK_GPIO_PIN12            (1UL << 12) /**< Pin 12 select mask */
#define CSK_GPIO_PIN13            (1UL << 13) /**< Pin 13 select mask */
#define CSK_GPIO_PIN14            (1UL << 14) /**< Pin 14 select mask */
#define CSK_GPIO_PIN15            (1UL << 15) /**< Pin 15 select mask */
#define CSK_GPIO_PIN16            (1UL << 16) /**< Pin 16 select mask */
#define CSK_GPIO_PIN17            (1UL << 17) /**< Pin 17 select mask */
#define CSK_GPIO_PIN18            (1UL << 18) /**< Pin 18 select mask */
#define CSK_GPIO_PIN19            (1UL << 19) /**< Pin 19 select mask */
#define CSK_GPIO_PIN20            (1UL << 20) /**< Pin 20 select mask */
#define CSK_GPIO_PIN21            (1UL << 21) /**< Pin 21 select mask */
#define CSK_GPIO_PIN22            (1UL << 22) /**< Pin 22 select mask */
#define CSK_GPIO_PIN23            (1UL << 23) /**< Pin 23 select mask */
#define CSK_GPIO_PIN24            (1UL << 24) /**< Pin 24 select mask */
#define CSK_GPIO_PIN25            (1UL << 25) /**< Pin 25 select mask */
#define CSK_GPIO_PIN26            (1UL << 26) /**< Pin 26 select mask */
#define CSK_GPIO_PIN27            (1UL << 27) /**< Pin 27 select mask */
#define CSK_GPIO_PIN28            (1UL << 28) /**< Pin 28 select mask */
#define CSK_GPIO_PIN29            (1UL << 29) /**< Pin 29 select mask */
#define CSK_GPIO_PIN30            (1UL << 30) /**< Pin 30 select mask */
#define CSK_GPIO_PIN31            (1UL << 31) /**< Pin 31 select mask */
/**
  * @}
  */ /* End of group GPIO_Exported_Macros */

/* Exported types ------------------------------------------------------------*/
/** @defgroup GPIO_Exported_Types GPIO Exported Types
  * @{
  */
// Signal event callback function type
/**
 * @typedef CSK_GPIO_SignalEvent_t
 * @brief Function pointer type for GPIO event callbacks
 * @param[in] event Event identifier indicating trigger source
 * @param[in] workspace User-defined context pointer passed during callback
 * @note Called from interrupt context - keep execution short
 */
typedef void (*CSK_GPIO_SignalEvent_t) (uint32_t event, void* workspace);

// Direction mode enumeration
/**
 * @enum _DIR_
 * @brief GPIO pin direction modes
 * @var csk_gpio_dir_input
 * @var csk_gpio_dir_output
 */
typedef enum {
    csk_gpio_dir_input = 0x0,   ///< Input mode (read operations only)
    csk_gpio_dir_output,        ///< Output mode (write operations allowed)
} _DIR_;

// Pull mode enumeration
/**
 * @enum _MODE_
 * @brief Internal pull resistor configuration
 * @var csk_gpio_mode_none_pull
 * @var csk_gpio_mode_pull_up
 * @var csk_gpio_mode_pull_down
 */
typedef enum {
    csk_gpio_mode_none_pull = 0x0,  ///< No internal pull resistors
    csk_gpio_mode_pull_up,         ///< Internal pull-up resistor enabled
    csk_gpio_mode_pull_down,       ///< Internal pull-down resistor enabled
} _MODE_;

// Interrupt mode enumeration
/**
 * @enum _INT_MODE_
 * @brief Interrupt trigger condition types
 * @var _csk_gpio_int_mode_none_
 * @var csk_gpio_int_mode_high_level
 * @var csk_gpio_int_mode_low_level
 * @var csk_gpio_int_mode_negative_level
 * @var csk_gpio_int_mode_positive_level
 * @var csk_gpio_int_mode_dual_level
 */
typedef enum {
    _csk_gpio_int_mode_none_ = 0x0,   ///< No interrupt generation
    csk_gpio_int_mode_high_level = 0x2,  ///< High level detection
    csk_gpio_int_mode_low_level = 0x3,   ///< Low level detection
    csk_gpio_int_mode_negative_level = 0x5, ///< Falling edge detection
    csk_gpio_int_mode_positive_level = 0x6, ///< Rising edge detection
    csk_gpio_int_mode_dual_level = 0x7,     ///< Both edges detection
} _INT_MODE_;

// GPIO configuration structure
/**
 * @struct _GPIO_
 * @brief Complete GPIO pin configuration parameters
 * @var dir Data direction (input/output)
 * @var mode Pull resistor configuration
 * @var int_mode Interrupt trigger condition
 */
typedef struct {
    _DIR_ dir;          ///< Data direction control
    _MODE_ mode;        ///< Pull resistor configuration
    _INT_MODE_ int_mode;///< Interrupt trigger condition
} _GPIO_;

/**
  * @}
  */ /* End of group GPIO_Exported_Types */

/* Exported functions --------------------------------------------------------*/
/** @defgroup GPIO_Exported_Functions GPIO Exported Functions
  * @{
  */

/**
 * @fn CSK_DRIVER_VERSION GPIO_GetVersion()
 * @brief Retrieves driver version information
 * @return Driver version number in CSK_DRIVER_VERSION format
 * @details Returns compiled-in driver version identifying capabilities
 */
CSK_DRIVER_VERSION GPIO_GetVersion(void);

/**
 * @fn int32_t GPIO_Initialize(void *res, CSK_GPIO_SignalEvent_t cb_event, void* workspace)
 * @brief Initializes GPIO driver resources
 * @param[out] res Pointer to resource handle allocated by this function
 * @param[in] cb_event Event callback function pointer
 * @param[in] workspace User-defined context pointer for callback routines
 * @return Negative error code on failure, non-negative status code on success
 * @details Must be called before any other GPIO functions. Allocates
 *         necessary memory resources and initializes hardware state.
 */
int32_t GPIO_Initialize(void *res, CSK_GPIO_SignalEvent_t cb_event, void* workspace);

/**
 * @fn int32_t GPIO_Uninitialize(void *res)
 * @brief Releases GPIO driver resources
 * @param[in] res Resource handle obtained from GPIO_Initialize()
 * @return Negative error code on failure, non-negative status code on success
 * @details Frees memory allocated during initialization and resets
 *         hardware to default state. Should be called when done with GPIO.
 */
int32_t GPIO_Uninitialize(void *res);

/**
 * @fn int32_t GPIO_PowerControl(void* res, CSK_POWER_STATE state)
 * @brief Controls GPIO power state
 * @param[in] res Resource handle from successful initialization
 * @param[in] state Power state to enter (ON/OFF/SLEEP etc.)
 * @return Negative error code on failure, non-negative status code on success
 * @details Manages power consumption by enabling/disabling GPIO blocks
 *         according to system power management requirements.
 */
int32_t GPIO_PowerControl(void* res, CSK_POWER_STATE state);

/**
 * @fn int32_t GPIO_Control(void* res, uint32_t control, uint32_t arg)
 * @brief Configures various GPIO control parameters
 * @param[in] res Resource handle from successful initialization
 * @param[in] control Control command identifier
 * @param[in] arg Command argument value
 * @return Negative error code on failure, non-negative status code on success
 * @details Allows fine-grained control over advanced features like debouncing,
 *         interrupt polarity, and special function registers.
 */
int32_t GPIO_Control(void* res, uint32_t control, uint32_t arg);

/**
 * @fn int32_t GPIO_PinWrite(void* res, uint32_t pin_mask, uint32_t val)
 * @brief Writes values to specified GPIO pins
 * @param[in] res Resource handle from successful initialization
 * @param[in] pin_mask Bitmask selecting target pins (see CSK_GPIO_PINx macros)
 * @param[in] val Value to write (each bit corresponds to selected pin)
 * @return Negative error code on failure, non-negative status code on success
 * @details Sets output levels on specified pins. Only effective for pins
 *         configured as outputs through GPIO_SetDir().
 */
int32_t GPIO_PinWrite(void* res, uint32_t pin_mask, uint32_t val);

/**
 * @fn int32_t GPIO_PinRead(void* res, uint32_t pin_mask)
 * @brief Reads current state of specified GPIO pins
 * @param[in] res Resource handle from successful initialization
 * @param[in] pin_mask Bitmask selecting target pins (see CSK_GPIO_PINx macros)
 * @return Bitmask representing current pin states (1=high, 0=low)
 * @details Returns logical levels present on specified pins regardless of direction setting.
 */
int32_t GPIO_PinRead(void* res, uint32_t pin_mask);

/**
 * @fn int32_t GPIO_SetDir(void* res, uint32_t pin_mask, uint32_t dir)
 * @brief Sets direction mode for specified GPIO pins
 * @param[in] res Resource handle from successful initialization
 * @param[in] pin_mask Bitmask selecting target pins (see CSK_GPIO_PINx macros)
 * @param[in] dir Direction mode (csk_gpio_dir_input or csk_gpio_dir_output)
 * @return Negative error code on failure, non-negative status code on success
 * @details Configures each specified pin as either input or output. Changes take effect immediately.
 */
int32_t GPIO_SetDir(void* res, uint32_t pin_mask, uint32_t dir);

/**
 * @fn int32_t GPIO_Status(void* res, _GPIO_** status, uint32_t* size)
 * @brief Retrieves current GPIO configuration status
 * @param[in] res Resource handle from successful initialization
 * @param[out] status Array of _GPIO_ structures receiving pin configurations
 * @param[out] size Number of elements written to status array
 * @return Negative error code on failure, non-negative status code on success
 * @details Queries current configuration of all pins and returns detailed status information.
 */
int32_t GPIO_Status(void* res, _GPIO_** status, uint32_t* size);

/**
 * @fn void* GPIOA(void)
 * @brief Get handle for GPIO Port A resources
 * @return Resource handle for GPIO Port A
 * @details Specialized interface for Port A specific functionality
 */
void* GPIOA(void);

/**
 * @fn void* GPIOB(void)
 * @brief Get handle for GPIO Port B resources
 * @return Resource handle for GPIO Port B
 * @details Specialized interface for Port B specific functionality
 */
void* GPIOB(void);
/** @} */ /* End of group GPIO_Exported_Functions */
/**
  * @}
  */ /* End of group GPIO */

#endif /* DRIVER_GPIO_H */

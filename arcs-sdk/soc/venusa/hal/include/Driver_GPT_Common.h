/**
 * @file    Driver_GPT_COMMON.h
 * @author  USER
 * @brief   Header file for General Purpose Timer (GPT) common driver interfaces
 * @details This file contains shared definitions and function declarations
 *          for the GPT peripheral driver implementation. It provides common
 *          types, event flags, configuration structures and core API functions.
 * @created Created on: April 24, 2025
 */

#ifndef DRIVER_GPT_COMMON_H_
#define DRIVER_GPT_COMMON_H_

#include "Driver_Common.h"
#include "Driver_GPT_Common.h"

/** @defgroup GPT_COMMON
  * @brief GPT_COMMON HAL module driver
  * @{
  */

/* Exported constants --------------------------------------------------------*/
/** @defgroup GPT_COMMON_Exported_Constants GPT_COMMON Exported Constants
  * @{
  */

/**
 * @def GPT_NUMBER_OF_CHANNELS
 * @brief Number of available GPT channels in the system
 * @details Defines maximum supported timer channels (fixed at 2 in this implementation)
 */
#define GPT_NUMBER_OF_CHANNELS          2

/** @defgroup GPT_COMMON_Events GPT_COMMON Event Codes
  * @{
  */
/**
 * @def CSK_GPT_EVENT_PWM_CYCLE_DONE
 * @brief PWM cycle completion event flag
 * @details Set when a complete PWM waveform generation cycle finishes
 */
#define CSK_GPT_EVENT_PWM_CYCLE_DONE                   (1UL << 0)

/**
 * @def CSK_GPT_EVENT_LEDC_COMPLETE
 * @brief LED controller operation complete event flag
 * @details Signals successful completion of LED controller operations
 */
#define CSK_GPT_EVENT_LEDC_COMPLETE                    (1UL << 1)

/**
 * @def CSK_GPT_EVENT_LEDC_TX_FIFO_UNDERFLOW
 * @brief LED controller transmit FIFO underflow event flag
 * @details Indicates FIFO buffer underflow condition during LED transmission
 */
#define CSK_GPT_EVENT_LEDC_TX_FIFO_UNDERFLOW           (1UL << 2)

/**
 * @def CSK_GPT_EVENT_BREATH_PORT0
 * @brief Breathing effect port 0 event flag
 * @details Triggered by breathing pattern generator on Port 0
 */
#define CSK_GPT_EVENT_BREATH_PORT0                     (1UL << 3)

/**
 * @def CSK_GPT_EVENT_BREATH_PORT1
 * @brief Breathing effect port 1 event flag
 * @details Triggered by breathing pattern generator on Port 1
 */
#define CSK_GPT_EVENT_BREATH_PORT1                     (1UL << 4)

/**
 * @def CSK_GPT_EVENT_BREATH_PORT2
 * @brief Breathing effect port 2 event flag
 * @details Triggered by breathing pattern generator on Port 2
 */
#define CSK_GPT_EVENT_BREATH_PORT2                     (1UL << 5)

/**
 * @def CSK_GPT_EVENT_16BITS_TIMER_COMPLETE
 * @brief 16-bit timer completion event flag
 * @details Set when 16-bit timer reaches terminal count
 */
#define CSK_GPT_EVENT_16BITS_TIMER_COMPLETE            (1UL << 6)

/**
 * @def CSK_GPT_EVENT_8BITS_TIMER0_COMPLETE
 * @brief 8-bit timer channel 0 completion event flag
 * @details Set when 8-bit timer channel 0 reaches terminal count
 */
#define CSK_GPT_EVENT_8BITS_TIMER0_COMPLETE            (1UL << 7)

/**
 * @def CSK_GPT_EVENT_8BITS_TIMER1_COMPLETE
 * @brief 8-bit timer channel 1 completion event flag
 * @details Set when 8-bit timer channel 1 reaches terminal count
 */
#define CSK_GPT_EVENT_8BITS_TIMER1_COMPLETE            (1UL << 8)
/** @} */ /* End of group GPT_COMMON_Events */
/**
  * @}
  */ /* End of group GPT_COMMON_Exported_Constants */

/* Exported types ------------------------------------------------------------*/
/** @defgroup GPT_COMMON_Exported_Types GPT_COMMON Exported Types
  * @{
  */
/**
 * @enum GPT_Channel_Num_t
 * @brief Enumeration of GPT channel numbers
 * @var GPT_CHANNEL_0 Channel 0 selection
 * @var GPT_CHANNEL_1 Channel 1 selection
 */
typedef enum {
    GPT_CHANNEL_0 = 0, ///< Select GPT Channel 0
    GPT_CHANNEL_1,     ///< Select GPT Channel 1
} GPT_Channel_Num_t;

/**
 * @enum GPT_Clk_Src_t
 * @brief Clock source selection for GPT modules
 * @var GPT_CLK_SRC_T0 Internal timer clock source T0
 * @var GPT_CLK_SRC_EXCLK External clock input
 * @var GPT_CLK_SRC_PCLK Peripheral clock line
 */
typedef enum {
    GPT_CLK_SRC_T0 = 0,      ///< Use internal T0 clock source
    GPT_CLK_SRC_EXCLK,       ///< Use external clock input
    GPT_CLK_SRC_PCLK,        ///< Use PCLK as clock source
} GPT_Clk_Src_t;

/**
 * @enum GPT_Clk_Div_t
 * @brief Clock divider settings for GPT modules
 * @var GPT_CLK_DIV_1 Division factor 1 (no division)
 * @var GPT_CLK_DIV_2 Division factor 2
 * @var GPT_CLK_DIV_4 Division factor 4
 * @var GPT_CLK_DIV_8 Division factor 8
 * @var GPT_CLK_DIV_16 Division factor 16
 * @var GPT_CLK_DIV_32 Division factor 32
 * @var GPT_CLK_DIV_64 Division factor 64
 * @var GPT_CLK_DIV_128 Division factor 128
 */
typedef enum {
    GPT_CLK_DIV_1 = 0,   ///< No clock division
    GPT_CLK_DIV_2,       ///< Divide clock by 2
    GPT_CLK_DIV_4,       ///< Divide clock by 4
    GPT_CLK_DIV_8,       ///< Divide clock by 8
    GPT_CLK_DIV_16,      ///< Divide clock by 16
    GPT_CLK_DIV_32,      ///< Divide clock by 32
    GPT_CLK_DIV_64,      ///< Divide clock by 64
    GPT_CLK_DIV_128,     ///< Divide clock by 128
} GPT_Clk_Div_t;

/**
 * @struct GPT_Config_Para_t
 * @brief General Purpose Timer configuration parameters
 * @var clk_src Clock source selection (@ref GPT_Clk_Src_t)
 * @var prediv Prescaler value (applies before main divider)
 * @var clk_div Main clock divider setting (@ref GPT_Clk_Div_t)
 */
typedef struct {
    GPT_Clk_Src_t clk_src;   ///< Clock source selection
    uint16_t prediv;         ///< Prescaler divisor value
    GPT_Clk_Div_t clk_div;   ///< Main clock divider setting
} GPT_Config_Para_t;

/**
 * @enum Hardware_Channel_Type_t
 * @brief Hardware channel usage modes
 * @var HARDWARE_CHANNEL_STAT_IDLE Channel is idle/unused
 * @var HARDWARE_CHANNEL_STAT_USED_BY_TIM16 Used by 16-bit timer
 * @var HARDWARE_CHANNEL_STAT_USED_BY_TIM8 Used by 8-bit timer
 * @var HARDWARE_CHANNEL_STAT_USED_BY_PWM Used by PWM module
 */
typedef enum {
    HARDWARE_CHANNEL_STAT_IDLE = 0,         ///< Channel currently unused
    HARDWARE_CHANNEL_STAT_USED_BY_TIM16,   ///< Allocated to 16-bit timer
    HARDWARE_CHANNEL_STAT_USED_BY_TIM8,    ///< Allocated to 8-bit timer
    HARDWARE_CHANNEL_STAT_USED_BY_PWM = 4, ///< Allocated to PWM subsystem
} Hardware_Channel_Type_t;

/**
 * @typedef CSK_GPT_SignalEvent_t
 * @brief Event callback function type for GPT signals
 * @param[in] event Event identifier bitmask
 * @param[in] param User-defined parameter pointer passed with event
 * @note Called from interrupt context - execution time should be minimal
 */
typedef void (*CSK_GPT_SignalEvent_t)(uint32_t event, void *param);

/**
 * @struct GPT_Info_t
 * @brief GPT channel status and callback information structure
 * @var channel_type Array storing current usage state of each channel
 * @var cb_event Array of registered event callback functions
 */
typedef struct {
    volatile Hardware_Channel_Type_t channel_type[GPT_NUMBER_OF_CHANNELS]; ///< Channel usage states
    CSK_GPT_SignalEvent_t cb_event[GPT_NUMBER_OF_CHANNELS];                ///< Registered callbacks
} GPT_Info_t;
/**
  * @}
  */ /* End of group GPT_COMMON_Exported_Types */

/* Exported functions --------------------------------------------------------*/
/** @defgroup GPT_COMMON_Exported_Functions GPT_COMMON Exported Functions
  * @{
  */
/**
 * @fn int32_t HAL_GPT_Initialize(void* res)
 * @brief Initializes GPT driver resources
 * @param[out] res Pointer to resource handle allocated by this function
 * @return Negative error code on failure, non-negative status code on success
 * @details Must be called before any other GPT functions. Allocates
 *         necessary memory resources and initializes hardware state.
 */
int32_t HAL_GPT_Initialize(void* res);

/**
 * @fn int32_t HAL_GPT_Uninitialize(void* res)
 * @brief Releases GPT driver resources
 * @param[in] res Resource handle obtained from HAL_GPT_Initialize()
 * @return Negative error code on failure, non-negative status code on success
 * @details Frees memory allocated during initialization and resets
 *         hardware to default state. Should be called when done with GPT.
 */
int32_t HAL_GPT_Uninitialize(void* res);

/**
 * @fn int32_t HAL_GPT_PowerControl(void* res, CSK_POWER_STATE state)
 * @brief Controls GPT power state
 * @param[in] res Resource handle from successful initialization
 * @param[in] state Power state to enter (ON/OFF/SLEEP etc.)
 * @return Negative error code on failure, non-negative status code on success
 * @details Manages power consumption by enabling/disabling GPT blocks
 *         according to system power management requirements.
 */
int32_t HAL_GPT_PowerControl(void* res, CSK_POWER_STATE state);

/**
 * @fn int32_t HAL_GPT_Control(void* res, GPT_Channel_Num_t channel, GPT_Config_Para_t* para)
 * @brief Configures GPT channel parameters
 * @param[in] res Resource handle from successful initialization
 * @param[in] channel Target channel number (@ref GPT_Channel_Num_t)
 * @param[in] para Configuration parameters (@ref GPT_Config_Para_t)
 * @return Negative error code on failure, non-negative status code on success
 * @details Applies specified clock source, prescaler and divider settings
 *         to selected GPT channel. Changes take immediate effect.
 */
int32_t HAL_GPT_Control(void* res, GPT_Channel_Num_t channel, GPT_Config_Para_t* para);

/**
 * @fn int32_t HAL_GPT_DisableChannel(void* res, GPT_Channel_Num_t channel)
 * @brief Disables specified GPT channel
 * @param[in] res Resource handle from successful initialization
 * @param[in] channel Target channel number (@ref GPT_Channel_Num_t)
 * @return Negative error code on failure, non-negative status code on success
 * @details Stops timer operation and releases channel resources.
 */
int32_t HAL_GPT_DisableChannel(void* res, GPT_Channel_Num_t channel);

/**
 * @fn int32_t HAL_GPT_RegisterChannel(void* res, GPT_Channel_Num_t channel, Hardware_Channel_Type_t type)
 * @brief Registers channel for specific peripheral use
 * @param[in] res Resource handle from successful initialization
 * @param[in] channel Target channel number (@ref GPT_Channel_Num_t)
 * @param[in] type Peripheral type that will use the channel (@ref Hardware_Channel_Type_t)
 * @return Negative error code on failure, non-negative status code on success
 * @details Reserves channel for exclusive use by specified peripheral module.
 */
int32_t HAL_GPT_RegisterChannel(void* res, GPT_Channel_Num_t channel, Hardware_Channel_Type_t type);

/**
 * @fn int32_t HAL_GPT_EnableChannel(void* res, GPT_Channel_Num_t channel)
 * @brief Enables previously configured GPT channel
 * @param[in] res Resource handle from successful initialization
 * @param[in] channel Target channel number (@ref GPT_Channel_Num_t)
 * @return Negative error code on failure, non-negative status code on success
 * @details Starts timer operation on configured channel after successful registration.
 */
int32_t HAL_GPT_EnableChannel(void* res, GPT_Channel_Num_t channel);

/**
 * @fn void* GPT0(void)
 * @brief Get handle for GPT Channel 0 resources
 * @return Resource handle for GPT Channel 0
 * @details Specialized interface for direct access to Channel 0 functionality
 */
void* GPT0(void);
/** @} */ /* End of group GPT_COMMON_Exported_Functions */
/**
  * @}
  */ /* End of group GPT_COMMON */

#endif /* DRIVER_GPT_COMMON_H_ */

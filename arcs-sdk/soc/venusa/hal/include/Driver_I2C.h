/**
 * @file    Driver_I2C.h
 * @author  USER
 * @brief   Header file for I2C (Inter-Integrated Circuit) driver definitions
 * @details This file contains all necessary definitions, configuration options,
 *          and function declarations for the I2C communication protocol stack.
 */

#ifndef __DRIVER_I2C_H
#define __DRIVER_I2C_H

#include "Driver_Common.h"

/** @defgroup I2C
  * @brief I2C HAL module driver
  * @{
  */

/* Exported constants --------------------------------------------------------*/
/** @defgroup I2C_Exported_Constants I2C Exported Constants
  * @{
  */

/** @defgroup I2C_API_Version I2C API Version
  * @{
  */
/**
 * @def CSK_I2C_API_VERSION
 * @brief API version number definition
 * @details Formatted as major.minor using CSK_DRIVER_VERSION_MAJOR_MINOR macro
 * @note Indicates compatibility requirements between application and driver versions
 */
#define CSK_I2C_API_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,01)  /* API version */
/** @} */ /* End of group I2C_API_Version */

/** @defgroup I2C_Control I2C Control Codes
  * @{
  */
/**
 * @def CSK_I2C_OWN_ADDRESS
 * @brief Set device's own slave address
 * @param[in] address New slave address value
 * @details Configures the hardware address recognized by this I2C device in slave mode
 */
#define CSK_I2C_OWN_ADDRESS             (0x01)      ///< Set Own Slave Address; arg = address

/**
 * @def CSK_I2C_BUS_SPEED
 * @brief Set I2C bus communication speed
 * @param[in] speed Desired bus speed selection (@ref CSK_I2C_BUS_SPEED_*)
 * @details Changes clock stretching characteristics based on selected speed profile
 */
#define CSK_I2C_BUS_SPEED               (0x02)      ///< Set Bus Speed; arg = speed

/**
 * @def CSK_I2C_BUS_CLEAR
 * @brief Perform bus clear sequence
 * @details Sends nine clock pulses to reset bus state without generating STOP condition
 * @note Used after bus arbitration loss or unknown bus states
 */
#define CSK_I2C_BUS_CLEAR               (0x03)      ///< Execute Bus clear: send nine clock pulses

/**
 * @def CSK_I2C_ABORT_TRANSFER
 * @brief Abort ongoing master/slave transfer operation
 * @details Terminates current transmission/reception immediately
 * @note May leave bus in intermediate state - recommend bus clear afterwards
 */
#define CSK_I2C_ABORT_TRANSFER          (0x04)      ///< Abort Master/Slave Transmit/Receive

/**
 * @def CSK_I2C_TRANSMIT_MODE
 * @brief Select transmit mode implementation
 * @param[in] arg0 Mode selection flag (0=interrupt, 1=DMA)
 * @details Controls whether data movement uses interrupt service routines or DMA engine
 */
#define CSK_I2C_TRANSMIT_MODE           (0x06)      ///< If arg0 = 0, using interrupt to transmit(default); If arg0 = 1, using DMA to transmit

/*----- I2C Bus Speed -----*/
/**
 * @def CSK_I2C_BUS_SPEED_STANDARD
 * @brief Standard mode speed specification
 * @details Max theoretical speed: 100 kHz
 */
#define CSK_I2C_BUS_SPEED_STANDARD      (0x01)      ///< Standard Speed (100kHz)

/**
 * @def CSK_I2C_BUS_SPEED_FAST
 * @brief Fast mode speed specification
 * @details Max theoretical speed: 400 kHz
 */
#define CSK_I2C_BUS_SPEED_FAST          (0x02)      ///< Fast Speed     (400kHz)

/**
 * @def CSK_I2C_BUS_SPEED_FAST_PLUS
 * @brief Fast-plus mode speed specification
 * @details Max theoretical speed: 1 MHz
 */
#define CSK_I2C_BUS_SPEED_FAST_PLUS     (0x03)      ///< Fast+ Speed    (  1MHz)

/****** I2C Address Flags *****/

/**
 * @def CSK_I2C_ADDRESS_10BIT
 * @brief 10-bit address format flag
 * @details When set, enables extended address space beyond standard 7-bit format
 */
#define CSK_I2C_ADDRESS_10BIT           0x0400      ///< 10-bit address flag

/**
 * @def CSK_I2C_ADDRESS_GC
 * @brief General Call recognition flag
 * @details When set, device responds to General Call address (0x00)
 */
#define CSK_I2C_ADDRESS_GC              0x8000      ///< General Call flag
/** @} */ /* End of group I2C_Control */

/** @defgroup I2C_Events I2C Event
  * @{
  */
/**
 * @def CSK_I2C_EVENT_TRANSFER_DONE
 * @brief Transfer completion event flag
 * @details Indicates successful completion of master/slave transfer operation
 */
#define CSK_I2C_EVENT_TRANSFER_DONE       (1UL << 0)  ///< Master/Slave Transmit/Receive finished

/**
 * @def CSK_I2C_EVENT_TRANSFER_INCOMPLETE
 * @brief Incomplete transfer event flag
 * @details Signals that transfer couldn't complete due to error conditions
 */
#define CSK_I2C_EVENT_TRANSFER_INCOMPLETE (1UL << 1)  ///< Master/Slave Transmit/Receive incomplete transfer

/**
 * @def CSK_I2C_EVENT_SLAVE_TRANSMIT
 * @brief Slave transmit request event flag
 * @details Indicates pending slave transmission operation
 */
#define CSK_I2C_EVENT_SLAVE_TRANSMIT      (1UL << 2)  ///< Slave Transmit operation requested

/**
 * @def CSK_I2C_EVENT_SLAVE_RECEIVE
 * @brief Slave receive request event flag
 * @details Indicates pending slave reception operation
 */
#define CSK_I2C_EVENT_SLAVE_RECEIVE       (1UL << 3)  ///< Slave Receive operation requested

/**
 * @def CSK_I2C_EVENT_ADDRESS_NACK
 * @brief Address NACK event flag
 * @details Indicates slave didn't acknowledge addressed device
 */
#define CSK_I2C_EVENT_ADDRESS_NACK        (1UL << 4)  ///< Address not acknowledged from Slave

/**
 * @def CSK_I2C_EVENT_GENERAL_CALL
 * @brief General Call detection event flag
 * @details Indicates receipt of General Call address (requires GC flag enabled)
 */
#define CSK_I2C_EVENT_GENERAL_CALL        (1UL << 5)  ///< General Call indication

/**
 * @def CSK_I2C_EVENT_ARBITRATION_LOST
 * @brief Bus arbitration loss event flag
 * @details Indicates master lost bus contention during multi-master situation
 */
#define CSK_I2C_EVENT_ARBITRATION_LOST    (1UL << 6)  ///< Master lost arbitration

/**
 * @def CSK_I2C_EVENT_BUS_ERROR
 * @brief Bus error detection event flag
 * @details Occurs when START/STOP conditions appear at invalid positions
 */
#define CSK_I2C_EVENT_BUS_ERROR           (1UL << 7)  ///< Bus error detected (START/STOP at illegal position)

/**
 * @def CSK_I2C_EVENT_BUS_CLEAR
 * @brief Bus clear completion event flag
 * @details Indicates successful completion of bus clear sequence
 */
#define CSK_I2C_EVENT_BUS_CLEAR           (1UL << 8)  ///< Bus clear finished

/**
 * @def CSK_I2C_EVENT_ADDRESS_ACK
 * @brief Address acknowledgment event flag
 * @details Indicates slave acknowledged addressed device
 */
#define CSK_I2C_EVENT_ADDRESS_ACK         (1UL << 9)   ///< Address acknowledged from Slave
/** @} */ /* End of group I2C_Events */
/**
  * @}
  */ /* End of group I2C_Exported_Constants */

/* Exported types ------------------------------------------------------------*/
/** @defgroup I2C_Exported_Types I2C Exported Types
  * @{
  */
/**
 * @typedef CSK_I2C_SignalEvent_t
 * @brief Event callback function pointer type
 * @param[in] event Bitmask of occurred events (@ref CSK_I2C_EVENT_*)
 * @param[in] workspace User context pointer passed during callback registration
 * @note Must be implemented by application layer for event handling
 */
typedef void
(*CSK_I2C_SignalEvent_t)(uint32_t event, void* workspace); ///< Pointer to \ref CSK_I2C_SignalEvent : Signal I2C Event.
/**
  * @}
  */ /* End of group I2C_Exported_Types */

 /* Exported functions --------------------------------------------------------*/
 /** @defgroup I2C_Exported_Functions I2C Exported Functions
   * @{
   */
/**
 * @fn CSK_DRIVER_VERSION I2C_GetVersion(void)
 * @brief Retrieves I2C driver version information
 * @return Driver version number (@ref CSK_DRIVER_VERSION)
 * @details Returns compiled-in version information for compatibility checking
 */
CSK_DRIVER_VERSION
I2C_GetVersion(void);

/**
 * @fn int32_t I2C_Initialize(void* res, CSK_I2C_SignalEvent_t cb_event, void* workspace)
 * @brief Initializes I2C peripheral with specified parameters
 * @param[out] res Opaque resource handle returned after initialization
 * @param[in] cb_event Event handler callback function (@ref CSK_I2C_SignalEvent_t)
 * @param[in] workspace User context pointer stored with resource handle
 * @return Negative error code on failure, non-negative status code on success
 * @details Allocates resources, sets default configuration, and registers event handler
 */
int32_t
I2C_Initialize(void* res, CSK_I2C_SignalEvent_t cb_event, void* workspace);

/**
 * @fn int32_t I2C_Uninitialize(void* res)
 * @brief Deinitializes and releases I2C resources
 * @param[in] res Resource handle obtained from successful initialization
 * @return Negative error code on failure, non-negative status code on success
 * @details Frees allocated resources and disables peripheral clocks
 */
int32_t
I2C_Uninitialize(void* res);

/**
 * @fn int32_t I2C_PowerControl(void* res, CSK_POWER_STATE state)
 * @brief Controls power state of I2C peripheral
 * @param[in] res Resource handle from successful initialization
 * @param[in] state Target power state (@ref CSK_POWER_STATE)
 * @return Negative error code on failure, non-negative status code on success
 * @details Manages low-power modes while maintaining necessary functionality
 */
int32_t
I2C_PowerControl(void* res, CSK_POWER_STATE state);

/**
 * @fn int32_t I2C_MasterTransmit(void* res, uint32_t addr, const uint8_t* data, uint32_t num, bool xfer_pending)
 * @brief Initiates master transmit operation
 * @param[in] res Resource handle from successful initialization
 * @param[in] addr Target slave address (7-bit or 10-bit with flags)
 * @param[in] data Pointer to transmit buffer
 * @param[in] num Number of bytes to transmit
 * @param[in] xfer_pending Stop condition behavior (false=send STOP, true=no STOP)
 * @return Negative error code on failure, non-negative status code on success
 * @details Starts master transmit sequence with specified parameters
 */
int32_t
I2C_MasterTransmit(void* res, uint32_t addr, const uint8_t* data, uint32_t num,
        bool xfer_pending);

/**
 * @fn int32_t I2C_MasterReceive(void* res, uint32_t addr, uint8_t* data, uint32_t num, bool xfer_pending)
 * @brief Initiates master receive operation
 * @param[in] res Resource handle from successful initialization
 * @param[in] addr Target slave address (7-bit or 10-bit with flags)
 * @param[out] data Pointer to receive buffer
 * @param[in] num Number of bytes to receive
 * @param[in] xfer_pending Stop condition behavior (false=send STOP, true=no STOP)
 * @return Negative error code on failure, non-negative status code on success
 * @details Starts master receive sequence with specified parameters
 */
int32_t
I2C_MasterReceive(void* res, uint32_t addr, uint8_t* data, uint32_t num,
        bool xfer_pending);

/**
 * @fn int32_t I2C_SlaveTransmit(void* res, const uint8_t* data, uint32_t num)
 * @brief Initiates slave transmit operation
 * @param[in] res Resource handle from successful initialization
 * @param[in] data Pointer to transmit buffer
 * @param[in] num Number of bytes to transmit
 * @return Negative error code on failure, non-negative status code on success
 * @details Processes outgoing data from slave transmitter queue
 */
int32_t
I2C_SlaveTransmit(void* res, const uint8_t* data, uint32_t num);

/**
 * @fn int32_t I2C_SlaveReceive(void* res, uint8_t* data, uint32_t num)
 * @brief Initiates slave receive operation
 * @param[in] res Resource handle from successful initialization
 * @param[out] data Pointer to receive buffer
 * @param[in] num Number of bytes to receive
 * @return Negative error code on failure, non-negative status code on success
 * @details Stores received data in slave receiver buffer
 */
int32_t
I2C_SlaveReceive(void* res, uint8_t* data, uint32_t num);

/**
 * @fn int32_t I2C_GetDataCount(void* res)
 * @brief Gets count of available data in receive buffer
 * @param[in] res Resource handle from successful initialization
 * @return Number of bytes available in receive buffer
 * @details Non-blocking method to check for received data
 */
int32_t
I2C_GetDataCount(void* res);

/**
 * @fn int32_t I2C_Control(void* res, uint32_t control, uint32_t arg0)
 * @brief Issues low-level control commands to I2C peripheral
 * @param[in] res Resource handle from successful initialization
 * @param[in] control Command code (@ref CSK_I2C_* control codes)
 * @param[in] arg0 Command-specific argument
 * @return Negative error code on failure, non-negative status code on success
 * @details Direct access to hardware control registers through abstracted interface
 */
int32_t
I2C_Control(void* res, uint32_t control, uint32_t arg0);

/**
 * @fn void* I2C0(void)
 * @brief Obtains instance handle for I2C controller instance 0
 * @return Opaque handle to I2C instance 0 resources
 * @details Used for multi-instance scenarios where separate controllers exist
 */
void* I2C0(void);

/**
 * @fn void* I2C1(void)
 * @brief Obtains instance handle for I2C controller instance 1
 * @return Opaque handle to I2C instance 1 resources
 * @details Used for multi-instance scenarios where separate controllers exist
 */
void* I2C1(void);
/** @} */ /* End of group I2C_Exported_Functions */
/**
  * @}
  */ /* End of group I2C */

#endif /* __DRIVER_I2C_H */

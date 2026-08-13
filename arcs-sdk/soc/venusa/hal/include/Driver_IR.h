/**
 * @file Driver_IR.h
 * @brief Infrared (IR) Driver Definitions
 * @author USER
 * @date 2020.8.11
 * @version 1.0
 * 
 * @details This header defines the interface for controlling Infrared (IR) communication.
 * It provides functions for initialization, configuration, and data transmission/reception
 * of IR signals.
 */

#ifndef DRIVER_IR_H_
#define DRIVER_IR_H_

#include "Driver_Common.h"


/** @defgroup IR IR
  * @brief IR HAL module driver
  * @{
  */

/* Exported constants --------------------------------------------------------*/
/** @defgroup IR_Exported_Constants IR Exported Constants
  * @{
  */

/** @defgroup IR_API_Version IR API Version
  * @{
  */
#define CSK_IR_API_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,0)  /**< API version */
/** @} */ /* End of group IR_API_Version */

/** @defgroup IR_Control_Codes IR Control Codes
  * @{
  */

// adjust pos value
#define CSK_IR_CONTROL_Pos                    0
#define CSK_IR_CONTROL_Msk                   (0xFUL << CSK_IR_CONTROL_Pos)

/** @defgroup IR_Misc_Controls IR Miscellaneous Controls
  * @{
  */
/*----- IR Control Codes: Miscellaneous Controls  -----*/
#define CSK_IR_CONTROL_SW_TIMEOUT_THRES      (0x1UL << CSK_IR_CONTROL_Pos)
#define CSK_IR_CONTROL_ABORT                 (0x2UL << CSK_IR_CONTROL_Pos)
#define CSK_IR_CONTROL_TX_REPEAT             (0x3UL << CSK_IR_CONTROL_Pos)
#define CSK_IR_CONTROL_CLEAR_TX_FIFO         (0x4UL << CSK_IR_CONTROL_Pos)
#define CSK_IR_CONTROL_CLEAR_RX_FIFO         (0x5UL << CSK_IR_CONTROL_Pos)
/** @} */ /* End of group IR_Misc_Controls */

/** @defgroup IR_SW_Carry IR Software Carry Configuration
  * @{
  */
#define CSK_IR_SW_CARRY_Pos                   4
#define CSK_IR_SW_CARRY_Msk                  (1UL << CSK_IR_SW_CARRY_Pos)
#define CSK_IR_SW_CARRY_CONFIG               (1UL << CSK_IR_SW_CARRY_Pos)
/** @} */ /* End of group IR_SW_Carry */

/** @defgroup IR_Decode_Control IR Decode Control
  * @{
  */
#define CSK_IR_DECODE_CONTROL_Pos             5
#define CSK_IR_DECODE_CONTROL_Msk            (7UL << CSK_IR_DECODE_CONTROL_Pos)
#define CSK_IR_DECODE_CONTROL_EN             (0x1UL << CSK_IR_DECODE_CONTROL_Pos)
/// Input polarity
#define CSK_IR_DECODE_CONTROL_INPOL          (0x2UL << CSK_IR_DECODE_CONTROL_Pos)
/// Output polarity
#define CSK_IR_DECODE_CONTROL_OUTPOL         (0x3UL << CSK_IR_DECODE_CONTROL_Pos)
/// Threshold of the carrier detected
#define CSK_IR_DECODE_CONTROL_DET_THRE       (0x4UL << CSK_IR_DECODE_CONTROL_Pos)
/// Threshold of the high limited
#define CSK_IR_DECODE_CONTROL_HIGH_THRE      (0x5UL << CSK_IR_DECODE_CONTROL_Pos)
/// Threshold of the low limited
#define CSK_IR_DECODE_CONTROL_LOW_THRE       (0x6UL << CSK_IR_DECODE_CONTROL_Pos)
/** @} */ /* End of group IR_Decode_Control */

/** @} */ /* End of group IR_Control_Codes */

/** @defgroup IR_Events IR Events
  * @{
  */
/************IR Event**************/
#define CSK_IR_EVENT_HW_SEND_COMPLETE                         (0x1UL << 0)
#define CSK_IR_EVENT_HW_RECEIVE_COMPLETE                      (0x1UL << 1)
#define CSK_IR_EVENT_HW_TX_REPEAT_COMPLETE                    (0x1UL << 2)
#define CSK_IR_EVENT_HW_RX_REPEAT_TRIGGER                     (0x1UL << 3)
#define CSK_IR_EVENT_HW_ADDRESS_VERIFY_ERROR                  (0x1UL << 4)
#define CSK_IR_EVENT_HW_COMMAND_VERIFY_ERROR                  (0x1UL << 5)
#define CSK_IR_EVENT_SW_SEND_COMPLETE                         (0x1UL << 6)
#define CSK_IR_EVENT_SW_RECEIVE_COMPLETE                      (0x1UL << 7)
#define CSK_IR_EVENT_SW_RECEIVE_TIMEOUT                       (0x1UL << 8)
/** @} */ /* End of group IR_Events */

/**
  * @}
  */ /* End of group IR_Exported_Constants */

/* Exported types ------------------------------------------------------------*/
/** @defgroup IR_Exported_Types IR Exported Types
  * @{
  */
/**
 * @brief IR Mode Enumeration
 */
typedef enum {
    IR_MODE_CUSTOM,
    IR_MODE_NEC,
    IR_MODE_TOSHIBA_9012,
    IR_MODE_PHILIPS_RC5,
} IR_MODE;
/** @} */ /* End of group IR_Exported_Types */

/* Exported macros -----------------------------------------------------------*/
/** @defgroup IR_Exported_Macros IR Exported Macros
  * @{
  */

/**
  * @}
  */ /* End of group IR_Exported_Macros */

/* Exported functions --------------------------------------------------------*/
/** @defgroup IR_Exported_Functions IR Exported Functions
  * @{
  */

/**
 * @brief Signal IR Events
 * @param[in] event IR event notification mask
 * @param[in] workspace User workspace pointer
 */
typedef void (*CSK_IR_SignalEvent_t) (uint32_t event, void* workspace);

/**
 * @brief Get IR driver version
 * @return Driver version information
 */
CSK_DRIVER_VERSION CSK_IR_GetVersion(void);

/**
 * @brief Initialize IR interface
 * @param[in] res Pointer to IR device instance
 * @param[in] cb_event Pointer to event callback function
 * @param[in] workspace User workspace pointer
 * @return Execution status
 */
int32_t  IR_Initialize(void *res, CSK_IR_SignalEvent_t cb_event, void* workspace);

/**
 * @brief De-initialize IR interface
 * @param[in] res Pointer to IR device instance
 * @return Execution status
 */
int32_t  IR_Uninitialize(void* res);

/**
 * @brief Control IR interface power
 * @param[in] res Pointer to IR device instance
 * @param[in] state Power state
 * @return Execution status
 */
int32_t  IR_PowerControl(void *res, CSK_POWER_STATE state);

/**
 * @brief Control IR interface
 * @param[in] res Pointer to IR device instance
 * @param[in] control Operation
 * @param[in] arg Argument of operation
 * @return Execution status
 */
int32_t  IR_Control(void *res, uint32_t control, uint32_t arg);

/**
 * @brief Send data in hardware mode
 * @param[in] res Pointer to IR device instance
 * @param[in] mode IR protocol mode
 * @param[in] address Address to send
 * @param[in] command Command to send
 * @return Execution status
 */
int32_t  IR_HW_Send(void *res, IR_MODE mode, uint16_t address, uint16_t command);

/**
 * @brief Receive data in hardware mode
 * @param[in] res Pointer to IR device instance
 * @param[in] mode IR protocol mode
 * @param[out] address Pointer to store received address
 * @param[out] command Pointer to store received command
 * @return Execution status
 */
int32_t  IR_HW_Receive(void *res, IR_MODE mode, uint16_t *address, uint16_t *command);

/**
 * @brief Send data in software mode
 * @param[in] res Pointer to IR device instance
 * @param[in] data Pointer to data to send
 * @param[in] num Number of data items to send
 * @return Execution status
 */
int32_t  IR_SW_Send(void *res, uint16_t *data, uint32_t num);

/**
 * @brief Receive data in software mode
 * @param[in] res Pointer to IR device instance
 * @param[out] data Pointer to buffer for received data
 * @param[in] num Number of data items to receive
 * @return Execution status
 */
int32_t  IR_SW_Receive(void *res, uint16_t *data, uint32_t num);

/**
 * @brief Get transmitted data count
 * @param[in] res Pointer to IR device instance
 * @return Number of data items transmitted
 */
uint32_t IR_GetTxCount(void *res);

/**
 * @brief Get received data count
 * @param[in] res Pointer to IR device instance
 * @return Number of data items received
 */
uint32_t IR_GetRxCount(void *res);

/** @defgroup IR_Instances IR Instances
  * @{
  */

/**
 * @brief Get IR0 device instance
 * @return IR0 device instance
 */
void* IR0(void);

/** @} */ /* End of group IR_Instances */

/** @} */ /* End of group IR_Exported_Functions */

/**
  * @}
  */ /* End of group IR */


#endif /* DRIVER_IR_H_ */

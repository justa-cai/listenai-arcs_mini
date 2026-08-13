/**
 * @file    Driver_UART.h
 * @brief   CSK UART Driver Header File
 * @details This file contains the API definitions and configuration options for the CSK UART driver.
 *          It includes control codes, status structures, event definitions, and function prototypes.
 * @copyright (c) 2022.03.01 ListenAI. All rights reserved.
 */
#ifndef DRIVER_UART_H_
#define DRIVER_UART_H_

#include "Driver_Common.h"

/** @defgroup UART
 *  @brief UART HAL module driver
 *  @{
 */
/** @defgroup UART_Exported_Constants UART Exported Constants
  * @{
  */
/** @defgroup UART_DRV_Version UART API Version
  * @{
  */
#define CSK_UART_API_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,0)  /* API version */
/** @} */ /* End of group UART_DRV_Version */

/****** UART Control Codes *****/

#define CSK_UART_CONTROL_Pos                0
#define CSK_UART_CONTROL_Msk               (0xFFUL << CSK_UART_CONTROL_Pos)

/** @defgroup UART_Control_Code UART Control Codes
 *  @{
 */
#define CSK_UART_MODE_ASYNCHRONOUS         (0x01UL << CSK_UART_CONTROL_Pos)   ///< UART (Asynchronous); arg = Baudrate
#define CSK_UART_MODE_ASYNCHRONOUS_TIMEOUT (0x02UL << CSK_UART_CONTROL_Pos)   ///< UART (Asynchronous with timeout); arg = Baudrate
#define CSK_UART_SET_DEFAULT_TX_VALUE      (0x04UL << CSK_UART_CONTROL_Pos)   ///< Set default Transmit value; arg = value
#define CSK_UART_MODE_HALF_DUPLEX          (0x08UL << CSK_UART_CONTROL_Pos)
#define CSK_UART_MODE_HALF_DUPLEX_TIMEOUT  (0x10UL << CSK_UART_CONTROL_Pos)

/*----- UART Control Codes: Mode Parameters: Data Bits -----*/
#define CSK_UART_DATA_BITS_Pos              8
#define CSK_UART_DATA_BITS_Msk             (7UL << CSK_UART_DATA_BITS_Pos)
#define CSK_UART_DATA_BITS_5               (5UL << CSK_UART_DATA_BITS_Pos)    ///< 5 Data bits
#define CSK_UART_DATA_BITS_6               (6UL << CSK_UART_DATA_BITS_Pos)    ///< 6 Data bit
#define CSK_UART_DATA_BITS_7               (7UL << CSK_UART_DATA_BITS_Pos)    ///< 7 Data bits
#define CSK_UART_DATA_BITS_8               (0UL << CSK_UART_DATA_BITS_Pos)    ///< 8 Data bits (default)

/*----- UART Control Codes: Mode Parameters: Parity -----*/
#define CSK_UART_PARITY_Pos                 12
#define CSK_UART_PARITY_Msk                (3UL << CSK_UART_PARITY_Pos)
#define CSK_UART_PARITY_NONE               (0UL << CSK_UART_PARITY_Pos)       ///< No Parity (default)
#define CSK_UART_PARITY_EVEN               (1UL << CSK_UART_PARITY_Pos)       ///< Even Parity
#define CSK_UART_PARITY_ODD                (2UL << CSK_UART_PARITY_Pos)       ///< Odd Parity

/*----- UART Control Codes: Mode Parameters: Stop Bits -----*/
#define CSK_UART_STOP_BITS_Pos              14
#define CSK_UART_STOP_BITS_Msk             (3UL << CSK_UART_STOP_BITS_Pos)
#define CSK_UART_STOP_BITS_1               (0UL << CSK_UART_STOP_BITS_Pos)    ///< 1 Stop bit (default)
#define CSK_UART_STOP_BITS_2               (1UL << CSK_UART_STOP_BITS_Pos)    ///< 2 Stop bits
#define CSK_UART_STOP_BITS_1_5             (2UL << CSK_UART_STOP_BITS_Pos)    ///< 1.5 Stop bits

/*----- UART Control Codes: Mode Parameters: Flow Control -----*/
#define CSK_UART_FLOW_CONTROL_Pos           16
#define CSK_UART_FLOW_CONTROL_Msk          (3UL << CSK_UART_FLOW_CONTROL_Pos)
#define CSK_UART_FLOW_CONTROL_NONE         (0UL << CSK_UART_FLOW_CONTROL_Pos) ///< No Flow Control (default)
#define CSK_UART_FLOW_CONTROL_RTS          (1UL << CSK_UART_FLOW_CONTROL_Pos) ///< RTS Flow Control
#define CSK_UART_FLOW_CONTROL_CTS          (2UL << CSK_UART_FLOW_CONTROL_Pos) ///< CTS Flow Control
#define CSK_UART_FLOW_CONTROL_RTS_CTS      (3UL << CSK_UART_FLOW_CONTROL_Pos) ///< RTS/CTS Flow Control

/*----- UART Control Codes: Function Parameters: Interrupt & DMA -----*/
#define CSK_UART_Function_CONTROL_Pos      18
#define CSK_UART_Function_CONTROL_Msk     (1UL << CSK_UART_Function_CONTROL_Pos)
#define CSK_UART_Function_CONTROL_Int     (1UL << CSK_UART_Function_CONTROL_Pos) ///< Use interrupt mode
#define CSK_UART_Function_CONTROL_Dma     (0UL << CSK_UART_Function_CONTROL_Pos) ///< Use DMA mode

/*----- UART Control Codes: GPIO Parameters: Custom GPIO & Default GPIO -----*/
#define CSK_UART_GPIO_CONTROL_Pos          19
#define CSK_UART_GPIO_CONTROL_Msk         (1UL << CSK_UART_GPIO_CONTROL_Pos)
#define CSK_UART_GPIO_CONTROL_CUSTOM      (1UL << CSK_UART_GPIO_CONTROL_Pos) ///< Use custom GPIO configuration
#define CSK_UART_GPIO_CONTROL_DEFAULT     (0UL << CSK_UART_GPIO_CONTROL_Pos) ///< Use default GPIO configuration

/*----- UART Control Codes: Miscellaneous Controls  -----*/
#define CSK_UART_CONTROL_TX                (0x15UL << CSK_UART_CONTROL_Pos)   ///< Transmitter control; arg: 0=disabled, 1=enabled
#define CSK_UART_CONTROL_RX                (0x16UL << CSK_UART_CONTROL_Pos)   ///< Receiver control; arg: 0=disabled, 1=enabled
#define CSK_UART_CONTROL_BREAK             (0x17UL << CSK_UART_CONTROL_Pos)   ///< Continuous Break transmission; arg: 0=disabled, 1=enabled
#define CSK_UART_ABORT_SEND                (0x18UL << CSK_UART_CONTROL_Pos)   ///< Abort ongoing send operation
#define CSK_UART_ABORT_RECEIVE             (0x19UL << CSK_UART_CONTROL_Pos)   ///< Abort ongoing receive operation
#define CSK_UART_DISABLE_TX_INT            (0x20UL << CSK_UART_CONTROL_Pos)   ///< Disable TX interrupt generation

/****** UART specific error codes *****/
#define CSK_UART_ERROR_MODE                (CSK_DRIVER_ERROR_SPECIFIC - 1)     ///< Unsupported communication mode
#define CSK_UART_ERROR_BAUDRATE            (CSK_DRIVER_ERROR_SPECIFIC - 2)     ///< Unsupported baud rate
#define CSK_UART_ERROR_DATA_BITS           (CSK_DRIVER_ERROR_SPECIFIC - 3)     ///< Unsupported data bit count
#define CSK_UART_ERROR_PARITY              (CSK_DRIVER_ERROR_SPECIFIC - 4)     ///< Unsupported parity setting
#define CSK_UART_ERROR_STOP_BITS           (CSK_DRIVER_ERROR_SPECIFIC - 5)     ///< Unsupported stop bit count
#define CSK_UART_ERROR_FLOW_CONTROL        (CSK_DRIVER_ERROR_SPECIFIC - 6)     ///< Unsupported flow control setting
/** @} */ // end of UART_Control_Code

/** @defgroup UART_EVENT UART Events
 *  @{
 */
#define CSK_UART_EVENT_SEND_COMPLETE       (1UL << 0)  ///< Send operation completed (data may still be shifting out)
#define CSK_UART_EVENT_RECEIVE_COMPLETE    (1UL << 1)  ///< Receive buffer filled completely
#define CSK_UART_EVENT_TX_OVERFLOW         (1UL << 2)  ///< Transmit buffer overflow occurred
#define CSK_UART_EVENT_TX_COMPLETE         (1UL << 3)  ///< All transmit data sent (optional feature)
#define CSK_UART_EVENT_TX_UNDERFLOW        (1UL << 4)  ///< Transmit buffer empty (slave mode only)
#define CSK_UART_EVENT_RX_OVERFLOW         (1UL << 5)  ///< Receive buffer overflow occurred
#define CSK_UART_EVENT_RX_TIMEOUT          (1UL << 6)  ///< Character receive timeout occurred
#define CSK_UART_EVENT_RX_BREAK            (1UL << 7)  ///< Break condition detected during receive
#define CSK_UART_EVENT_RX_FRAMING_ERROR    (1UL << 8)  ///< Framing error detected during receive
#define CSK_UART_EVENT_RX_PARITY_ERROR     (1UL << 9)  ///< Parity error detected during receive
#define CSK_UART_EVENT_CTS                 (1UL << 10) ///< CTS signal state changed (hardware flow control)
/** @} */ // end of UART_EVENT
/** @} */ // end of UART_Exported_Constants

/** @defgroup UART_Exported_Types UART Exported Types
  * @{
  */
/**
 * @brief UART Status Structure
 *
 * This structure provides detailed information about the current state of the UART peripheral.
 */
typedef struct _CSK_UART_STATUS {
  uint32_t tx_busy          : 1;        ///< Transmitter busy flag (1 = busy)
  uint32_t rx_busy          : 1;        ///< Receiver busy flag (1 = busy)
  uint32_t tx_underflow     : 1;        ///< Transmit data underflow detected (cleared on next send)
  uint32_t rx_overflow      : 1;        ///< Receive data overflow detected (cleared on next receive)
  uint32_t rx_break         : 1;        ///< Break condition detected during receive (cleared on next receive)
  uint32_t rx_framing_error : 1;        ///< Framing error detected during receive (cleared on next receive)
  uint32_t rx_parity_error  : 1;        ///< Parity error detected during receive (cleared on next receive)
  uint32_t reserved         : 25;       ///< Reserved bits (must be zero)
} CSK_UART_STATUS;

// Typedef for event callback function
typedef void (*CSK_UART_SignalEvent_t) (uint32_t event, void* workspace);  ///< Pointer to UART event handler function
/** @} */ // end of UART_Exported_Types

/* Exported functions --------------------------------------------------------*/
/** @defgroup UART_Exported_Functions UART Exported Functions
  * @{
  */
/**
 * @fn CSK_DRIVER_VERSION UART_GetVersion(void)
 * @brief Get driver version information
 * @return Returns driver version number as defined in Driver_Common.h
 */
CSK_DRIVER_VERSION UART_GetVersion(void);

/**
 * @fn int32_t UART_Initialize(void *res, CSK_UART_SignalEvent_t cb_event, void* workspace)
 * @brief Initialize UART device and register event callback
 * @param res Device instance pointer obtained from UARTX() macro
 * @param cb_event User event callback function
 * @param workspace User context pointer passed to callback
 * @return Standard driver return code (see Driver_Common.h)
 */
int32_t  UART_Initialize(void *res, CSK_UART_SignalEvent_t cb_event, void* workspace);

/**
 * @fn int32_t UART_Uninitialize(void *res)
 * @brief Deinitialize UART device and unregister callbacks
 * @param res Device instance pointer obtained from UARTX() macro
 * @return Standard driver return code (see Driver_Common.h)
 */
int32_t  UART_Uninitialize(void *res);

/**
 * @fn int32_t UART_PowerControl(void *res, CSK_POWER_STATE state)
 * @brief Power control for UART device
 * @param res Device instance pointer obtained from UARTX() macro
 * @param state Power state (ON/OFF/LOW_POWER) as defined in Driver_Common.h
 * @return Standard driver return code (see Driver_Common.h)
 */
int32_t  UART_PowerControl(void *res, CSK_POWER_STATE state);

/**
 * @fn int32_t UART_Control(void *res, uint32_t control, uint32_t arg)
 * @brief Configure UART parameters and operational modes
 * @param res Device instance pointer obtained from UARTX() macro
 * @param control Configuration command (see control codes above)
 * @param arg Command-specific argument
 * @return Standard driver return code (see Driver_Common.h)
 */
int32_t  UART_Control(void *res, uint32_t control, uint32_t arg);

/**
 * @fn int32_t UART_Send(void *res, const void *data, uint32_t num)
 * @brief Blocking UART data transmission
 * @param res Device instance pointer obtained from UARTX() macro
 * @param data Pointer to data buffer to transmit
 * @param num Number of bytes to transmit
 * @return Standard driver return code (see Driver_Common.h)
 */
int32_t  UART_Send(void *res, const void *data, uint32_t num);

/**
 * @fn int32_t UART_Send_IT(void *res, const void *data, uint32_t num)
 * @brief Non-blocking UART data transmission using interrupt
 * @param res Device instance pointer obtained from UARTX() macro
 * @param data Pointer to data buffer to transmit
 * @param num Number of bytes to transmit
 * @return Standard driver return code (see Driver_Common.h)
 */
int32_t  UART_Send_IT(void *res, const void *data, uint32_t num);

/**
 * @fn int32_t UART_Receive(void *res, void *data, uint32_t num)
 * @brief Blocking UART data reception
 * @param res Device instance pointer obtained from UARTX() macro
 * @param data Pointer to buffer for received data
 * @param num Number of bytes to receive
 * @return Standard driver return code (see Driver_Common.h)
 */
int32_t  UART_Receive(void *res, void *data, uint32_t num);

/**
 * @fn int32_t UART_Receive_IT(void *res, void *data, uint32_t num)
 * @brief Non-blocking UART data reception using interrupt
 * @param res Device instance pointer obtained from UARTX() macro
 * @param data Pointer to buffer for received data
 * @param num Number of bytes to receive
 * @return Standard driver return code (see Driver_Common.h)
 */
int32_t  UART_Receive_IT(void *res, void *data, uint32_t num);

/**
 * @fn uint32_t UART_GetTxCount(void *res)
 * @brief Get number of transmitted bytes since last query
 * @param res Device instance pointer obtained from UARTX() macro
 * @return Number of transmitted bytes
 */
uint32_t UART_GetTxCount(void *res);

/**
 * @fn uint32_t UART_GetRxCount(void *res)
 * @brief Get number of received bytes since last query
 * @param res Device instance pointer obtained from UARTX() macro
 * @return Number of received bytes
 */
uint32_t UART_GetRxCount(void *res);

/**
 * @fn int32_t UART_GetStatus(void *res, CSK_UART_STATUS* stat)
 * @brief Get current UART status information
 * @param res Device instance pointer obtained from UARTX() macro
 * @param stat Pointer to status structure to fill
 * @return Standard driver return code (see Driver_Common.h)
 */
int32_t  UART_GetStatus(void *res, CSK_UART_STATUS* stat);

/**
 * @brief Set DMA channel for UART TX
 * @param[in] res Pointer to UART resource structure
 * @param[in] channel DMA channel number or DMA_CHANNEL_ANY
 * @return Error code (0=success)
 */
int32_t  UART_SetDMATxChannel(void *res, uint8_t channel);

/**
 * @brief Set DMA channel for UART RX
 * @param[in] res Pointer to UART resource structure
 * @param[in] channel DMA channel number or DMA_CHANNEL_ANY
 * @return Error code (0=success)
 */
int32_t  UART_SetDMARxChannel(void *res, uint8_t channel);

/**
 * @fn void* UART0(void)
 * @brief Get UART0 device instance pointer
 * @return Device instance pointer
 */
void* UART0(void);

/**
 * @fn void* UART1(void)
 * @brief Get UART1 device instance pointer
 * @return Device instance pointer
 */
void* UART1(void);

/**
 * @fn void* UART2(void)
 * @brief Get UART2 device instance pointer
 * @return Device instance pointer
 */
void* UART2(void);
/** @} */ // end of UART_Exported_Functions
/** @} */ // end of UART group

#endif /* DRIVER_UART_H_ */

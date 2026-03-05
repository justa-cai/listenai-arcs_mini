#ifndef DRIVER_UART_H_
#define DRIVER_UART_H_


#include "Driver_Common.h"

#define CSK_UART_API_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,0)  /* API version */

/****** UART Control Codes *****/

#define CSK_UART_CONTROL_Pos                0
#define CSK_UART_CONTROL_Msk               (0xFFUL << CSK_UART_CONTROL_Pos)

/*----- UART Control Codes: Mode -----*/
#define CSK_UART_MODE_ASYNCHRONOUS         (0x01UL << CSK_UART_CONTROL_Pos)   ///< UART (Asynchronous); arg = Baudrate
#define CSK_UART_MODE_ASYNCHRONOUS_TIMEOUT (0x02UL << CSK_UART_CONTROL_Pos)   ///< UART (Asynchronous); arg = Baudrate
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
#define CSK_UART_Function_CONTROL_Int     (1UL << CSK_UART_Function_CONTROL_Pos)
#define CSK_UART_Function_CONTROL_Dma     (0UL << CSK_UART_Function_CONTROL_Pos)


/*----- UART Control Codes: GPIO Parameters: Custom GPIO & Default GPIO -----*/
#define CSK_UART_GPIO_CONTROL_Pos          19
#define CSK_UART_GPIO_CONTROL_Msk         (1UL << CSK_UART_GPIO_CONTROL_Pos)
#define CSK_UART_GPIO_CONTROL_CUSTOM      (1UL << CSK_UART_GPIO_CONTROL_Pos)
#define CSK_UART_GPIO_CONTROL_DEFAULT     (0UL << CSK_UART_GPIO_CONTROL_Pos)


/*----- UART Control Codes: Miscellaneous Controls  -----*/
#define CSK_UART_CONTROL_TX                (0x15UL << CSK_UART_CONTROL_Pos)   ///< Transmitter; arg: 0=disabled, 1=enabled
#define CSK_UART_CONTROL_RX                (0x16UL << CSK_UART_CONTROL_Pos)   ///< Receiver; arg: 0=disabled, 1=enabled
#define CSK_UART_CONTROL_BREAK             (0x17UL << CSK_UART_CONTROL_Pos)   ///< Continuous Break transmission; arg: 0=disabled, 1=enabled
#define CSK_UART_ABORT_SEND                (0x18UL << CSK_UART_CONTROL_Pos)   ///< Abort \ref CSK_UART_Send
#define CSK_UART_ABORT_RECEIVE             (0x19UL << CSK_UART_CONTROL_Pos)   ///< Abort \ref CSK_UART_Receive
#define CSK_UART_DISABLE_TX_INT            (0x20UL << CSK_UART_CONTROL_Pos)   ///< Disable TX interrupt



/****** UART specific error codes *****/
#define CSK_UART_ERROR_MODE                (CSK_DRIVER_ERROR_SPECIFIC - 1)     ///< Specified Mode not supported
#define CSK_UART_ERROR_BAUDRATE            (CSK_DRIVER_ERROR_SPECIFIC - 2)     ///< Specified baudrate not supported
#define CSK_UART_ERROR_DATA_BITS           (CSK_DRIVER_ERROR_SPECIFIC - 3)     ///< Specified number of Data bits not supported
#define CSK_UART_ERROR_PARITY              (CSK_DRIVER_ERROR_SPECIFIC - 4)     ///< Specified Parity not supported
#define CSK_UART_ERROR_STOP_BITS           (CSK_DRIVER_ERROR_SPECIFIC - 5)     ///< Specified number of Stop bits not supported
#define CSK_UART_ERROR_FLOW_CONTROL        (CSK_DRIVER_ERROR_SPECIFIC - 6)     ///< Specified Flow Control not supported


/**
\brief UART Status
*/
typedef struct _CSK_UART_STATUS {
  uint32_t tx_busy          : 1;        ///< Transmitter busy flag
  uint32_t rx_busy          : 1;        ///< Receiver busy flag
  uint32_t tx_underflow     : 1;        ///< Transmit data underflow detected (cleared on start of next send operation)
  uint32_t rx_overflow      : 1;        ///< Receive data overflow detected (cleared on start of next receive operation)
  uint32_t rx_break         : 1;        ///< Break detected on receive (cleared on start of next receive operation)
  uint32_t rx_framing_error : 1;        ///< Framing error detected on receive (cleared on start of next receive operation)
  uint32_t rx_parity_error  : 1;        ///< Parity error detected on receive (cleared on start of next receive operation)
  uint32_t reserved         : 25;
} CSK_UART_STATUS;

/****** UART Event *****/
#define CSK_UART_EVENT_SEND_COMPLETE       (1UL << 0)  ///< Send completed; however UART may still transmit data
#define CSK_UART_EVENT_RECEIVE_COMPLETE    (1UL << 1)  ///< Receive completed
#define CSK_UART_EVENT_TX_OVERFLOW         (1UL << 2)  ///< Transmit data overflow
#define CSK_UART_EVENT_TX_COMPLETE         (1UL << 3)  ///< Transmit completed (optional)
#define CSK_UART_EVENT_TX_UNDERFLOW        (1UL << 4)  ///< Transmit data not available (Synchronous Slave)
#define CSK_UART_EVENT_RX_OVERFLOW         (1UL << 5)  ///< Receive data overflow
#define CSK_UART_EVENT_RX_TIMEOUT          (1UL << 6)  ///< Receive character timeout (optional)
#define CSK_UART_EVENT_RX_BREAK            (1UL << 7)  ///< Break detected on receive
#define CSK_UART_EVENT_RX_FRAMING_ERROR    (1UL << 8)  ///< Framing error detected on receive
#define CSK_UART_EVENT_RX_PARITY_ERROR     (1UL << 9)  ///< Parity error detected on receive
#define CSK_UART_EVENT_CTS                 (1UL << 10) ///< CTS state changed (optional)


// Function documentation

typedef void (*CSK_UART_SignalEvent_t) (uint32_t event, void* workspace);  ///< Pointer to \ref CSK_UART_SignalEvent : Signal UART Event.


/*
 * Function name: CSK_UART_GetVersion
 * Function description: 获取设备和API的版本信息
 * Parameter list:
 * Return:
 *          返回版本信息，详见Driver_Common.h
 * */
CSK_DRIVER_VERSION UART_GetVersion(void);



/*
 * Function name: UART_Initialize
 * Function description: 注册回调函数和用户空间，初始化软件内部运行状态
 * Parameter list:
 *          res:       设备描述符，可以通过UARTX()函数获取该实例
 *          cb_event:  用户回调函数
 *          workspace: 用户可传入参数，程序内部不会更改该参数，会在回调函数中传出该指针
 * Return:
 *          通用返回代码，详见Driver_Common.h
 * */
int32_t  UART_Initialize(void *res, CSK_UART_SignalEvent_t cb_event, void* workspace);



/*
 * Function name: UART_Uninitialize
 * Function description: 注销回调函数和用户空间，初始化软件内部运行状态
 * Parameter list:
 *          res:       设备描述符，可以通过UARTX()函数获取该实例
 * Return:
 *          通用返回代码，详见Driver_Common.h
 * */
int32_t  UART_Uninitialize(void *res);



/*
 * Function name: UART_Initialize
 * Function description: 使能和失能UART设备，配置软件中断
 * Parameter list:
 *          res:       设备描述符，可以通过UARTX()函数获取该实例
 *          state:     具体参数详见Driver_Common.h
 * Return:
 *          通用返回代码，详见Driver_Common.h
 * */
int32_t  UART_PowerControl(void *res, CSK_POWER_STATE state);



/*
 * Function name: UART_Control
 * Function description: 配置UART设备参数，可选参数详见Driver_UART.h
 * Parameter list:
 *          res:       设备描述符，可以通过UARTX()函数获取该实例
 *          control:   配置参数
 *          arg:       可选择参数
 * Return:
 *          通用返回代码，详见Driver_Common.h
 * */
int32_t  UART_Control(void *res, uint32_t control, uint32_t arg);



/*
 * Function name: UART_Send
 * Function description: UART传输函数
 * Parameter list:
 *          res:       设备描述符，可以通过UARTX()函数获取该实例
 *          data:      需要传输的缓冲区
 *          num:       data缓冲区的大小
 * Return:
 *          通用返回代码，详见Driver_Common.h
 * */
int32_t  UART_Send(void *res, const void *data, uint32_t num);


int32_t  UART_Send_IT(void *res, const void *data, uint32_t num);

/*
 * Function name: UART_Receive
 * Function description: UART接收函数
 * Parameter list:
 *          res:       设备描述符，可以通过UARTX()函数获取该实例
 *          data:      需要接收的缓冲区
 *          num:       data缓冲区的大小
 * Return:
 *          通用返回代码，详见Driver_Common.h
 * */
int32_t  UART_Receive(void *res, void *data, uint32_t num);

int32_t  UART_Receive_IT(void *res, void *data, uint32_t num);

/*
 * Function name: UART_GetTxCount
 * Function description: 获取已经传输的大小
 * Parameter list:
 *          res:       设备描述符，可以通过UARTX()函数获取该实例
 * Return:
 *          通用返回代码，详见Driver_Common.h
 * */
uint32_t UART_GetTxCount(void *res);



/*
 * Function name: UART_GetRxCount
 * Function description: 获取已经接收的大小
 * Parameter list:
 *          res:       设备描述符，可以通过UARTX()函数获取该实例
 * Return:
 *          通用返回代码，详见Driver_Common.h
 * */
uint32_t UART_GetRxCount(void *res);



/*
 * Function name: UART_GetStatus
 * Function description: 获取UART运行状态
 * Parameter list:
 *          res:       设备描述符，可以通过UARTX()函数获取该实例
 *          stat:      运行状态指针
 * Return:
 *          通用返回代码，详见Driver_Common.h
 * */
int32_t  UART_GetStatus(void *res, CSK_UART_STATUS* stat);

/*
 * Function name: UART_SetDMATxChannel
 * Function description: Override the DMA channel assignment used for UART TX transfers.
 * Parameter list:
 *          res:         UART device descriptor (UARTX())
 *          channel:     DMA channel number (0 ~ DMA_MAX_NR_CHANNELS-1) or DMA_CHANNEL_ANY
 * Return:
 *          通用返回代码，详见Driver_Common.h
 */
int32_t  UART_SetDMATxChannel(void *res, uint8_t channel);

/*
 * Function name: UART_SetDMARxChannel
 * Function description: Override the DMA channel assignment used for UART RX transfers.
 * Parameter list:
 *          res:         UART device descriptor (UARTX())
 *          channel:     DMA channel number (0 ~ DMA_MAX_NR_CHANNELS-1) or DMA_CHANNEL_ANY
 * Return:
 *          通用返回代码，详见Driver_Common.h
 */
int32_t  UART_SetDMARxChannel(void *res, uint8_t channel);


/*
 * 获取设备实例函数
 * */
void* UART0(void);
void* UART1(void);
void* UART2(void);

#endif /* DRIVER_UART_H_ */

/**
 * @file Driver_I8080.H
 * @brief Intel 8080 Bus Controller Driver
 *
 * This driver provides an interface to control Intel 8080 bus peripherals
 * including LCD displays and other compatible devices. It supports multiple
 * transfer modes (IRQ, DMA) and various configuration options.
 *
 * @version 1.0
 * @date 2025-01-03
 */

#ifndef __INCLUDE_DRIVER_I8080_H
#define __INCLUDE_DRIVER_I8080_H

#include "Driver_Common.h"

/** @defgroup I8080
  * @brief I8080 HAL module driver
  * @{
  */
/** @defgroup I8080_Exported_Constants I8080 Exported Constants
  * @{
  */
/** @defgroup I8080_API_Version I8080 API Version
  * @{
  */
#define CSK_I8080_API_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,0)       ///< API version
/** @} */ /* End of group I8080_API_Version */

#define I8080_IRQ_TX_NUM_MAX  32                                        ///< Max TX bytes for IRQ mode
#define I8080_IRQ_RX_NUM_MAX  15                                        ///< Max RX bytes for IRQ mode
#define I8080_DMA_TX_NUM_MAX  (0xFFFFF * sizeof(uint32_t))              ///< Max DMA transfer size
/** @} */ /* End of group I8080_Exported_Constants */

/* Exported types ------------------------------------------------------------*/
/** @defgroup I8080_Exported_Types I8080 Exported Types
  * @{
  */
typedef enum {
    I8080_CONTROL_CLK_RUN_HZ            = (1<<0),   ///< Set system clock frequency (arg = Hz) max 200000000
    I8080_CONTROL_CLK_OUT_HZ            = (1<<1),   ///< Set output clock frequency (arg = Hz) max 100000000
    I8080_CONTROL_CSHT                  = (1<<2),   ///< Set CS hold time (arg = 0-15)
    I8080_CONTROL_CS2SCLK               = (1<<3),   ///< Set CS to SCLK delay (arg = 0-7)
    I8080_CONTROL_BURST_THD             = (1<<4),   ///< Set burst threshold (arg = 1/4/8/16)
    I8080_CONTROL_BIT_LSB               = (1<<5),   ///< Set LSB-first bit order
    I8080_CONTROL_BIT_MSB               = (1<<6),   ///< Set MSB-first bit order, DMA:bit31~0->bit0~31 IRQ:bit7~0->bit0~7
    I8080_CONTROL_HALFWORD_LSB          = (1<<7),   ///< Disable byte conversion for halfwords
    I8080_CONTROL_HALFWORD_MSB          = (1<<8),   ///< Enable byte swapping for halfwords, {byte3,byte2,byte1,byte0} -> {byte2,byte3,byte0,byte1}
    I8080_CONTROL_WORD_LSB              = (1<<9),   ///< (invalid) Disable byte conversion for words
    I8080_CONTROL_WORD_MSB              = (1<<10),  ///< (invalid) Enable byte swapping for words, {byte3,byte2,byte1,byte0} -> {byte0,byte1,byte2,byte3}
    I8080_CONTROL_CS_OUTPUT_LOW         = (1<<11),  ///< Force CS pin low
    I8080_CONTROL_CS_OUTPUT_HIGH        = (1<<12),  ///< Force CS pin high
    I8080_CONTROL_CS_OUTPUT_AUTO        = (1<<13),  ///< CS pin auto control
    I8080_CONTROL_RS_OUTPUT_LOW         = (1<<14),  ///< Force RS pin low
    I8080_CONTROL_RS_OUTPUT_HIGH        = (1<<15),  ///< Force RS pin high
    I8080_CONTROL_RS_OUTPUT_AUTO        = (1<<16),  ///< RS pin auto control
    I8080_CONTROL_FIFO_CLEAR            = (1<<17),  ///< Clear FIFO buffers
} I8080_emControl;

/**
 * @brief I8080 Transfer Modes
 */
typedef enum {
    I8080_TRANS_MODE_IRQ_TX_DATA,                   ///< IRQ TX data only, (0Byte < DataSize <= 32Byte)
    I8080_TRANS_MODE_IRQ_RX_DATA,                   ///< IRQ RX data only, (0Byte < DataSize <= 15Byte)
    I8080_TRANS_MODE_IRQ_TX_REG_TX_DATA,            ///< IRQ TX register + data, (0Byte < RegSize <= 4Byte), (0Byte < DataSize <= 32Byte)
    I8080_TRANS_MODE_IRQ_TX_REG_RX_DATA,            ///< IRQ TX register + RX data, (0Byte < RegSize <= 4Byte), (0Byte < DataSize <= 15Byte)
    I8080_TRANS_MODE_DMA_TX_DATA,                   ///< DMA TX data, (4Byte <= DataSize <= 4194300Byte and must align 4)
    I8080_TRANS_MODE_BUTT                           ///< Mode boundary
} I8080_emTransMode;

/**
 * @brief I8080 Interrupt Events
 */
typedef enum {
    I8080_IRQ_EVENT_TRANS_END = 0,                  ///< Transfer completed
    I8080_IRQ_EVENT_TXFIFO_OVERFLOW,                ///< TX FIFO overflow
    I8080_IRQ_EVENT_TXFIFO_UNDERFLOW,               ///< TX FIFO underflow
    I8080_IRQ_EVENT_TXFIFO_WR_FULL,                 ///< TX FIFO write full
    I8080_IRQ_EVENT_TXFIFO_RD_EMPTY,                ///< TX FIFO read empty
    I8080_IRQ_EVENT_BUTT                            ///< Event boundary
} I8080_emIrqEvent;

/**
 * @brief I8080 Status Definition
 */
typedef struct {
    uint8_t is_busy;                                 ///< Busy flag (1: busy, 0: idle)
    uint8_t tx_full;                                 ///< TX FIFO full flag
    uint8_t tx_empty;                                ///< TX FIFO empty flag
    uint8_t tx_num;                                  ///< Pending TX data count
    uint8_t rx_full;                                 ///< RX FIFO full flag
    uint8_t rx_empty;                                ///< RX FIFO empty flag
    uint8_t rx_num;                                  ///< Received data count
} I8080_StatusDef;

/**
 * @brief I8080 Event Callback Type
 * @param event Triggered event type
 * @param param User-defined parameter
 */
typedef void (*I8080_SignalEvent_t)(I8080_emIrqEvent event, uint32_t param);
/** @} */ /* End of group I8080_Exported_Types */

/* Exported functions --------------------------------------------------------*/
/** @defgroup I8080_Exported_Functions I8080 Exported Functions
  * @{
  */
/**
 * @brief Get driver version
 * @return CSK_DRIVER_VERSION structure
 */
CSK_DRIVER_VERSION I8080_GetVersion(void);


/**
 * @brief Get device handle
 * @return Pointer to device handle
 */
void* I8080(void);

/**
 * @brief Get TX buffer address
 * @return Physical address of TX buffer
 */
uint32_t I8080_TxBuf(void);

/**
 * @brief Initialize I8080 device
 * @param pDev Device handle
 * @param callback Event callback function
 * @param param User-defined parameter for callback context
 * @return Status code
 */
int32_t I8080_Initialize(void *pDev, I8080_SignalEvent_t callback, uint32_t param);


/**
 * @brief Deinitialize I8080 device
 * @param pDev Device handle
 * @return Status code
 */
int32_t I8080_Uninitialize(void *pDev);


/**
 * @brief Control I8080 device parameters
 * @param pDev Device handle
 * @param control Control command
 * @param arg Control argument
 * @return Status code
 */
int32_t I8080_Control(void *pDev, I8080_emControl control, uint32_t arg);


/**
 * @brief Execute data transfer
 * @param pDev Device handle
 * @param mode Transfer mode
 * @param pReg Register address/data
 * @param RegSize Register size
 * @param pData Data buffer
 * @param DataSize Data size
 * @return Status code
 *
 * @note IRQ_TX_DATA: (0Byte < DataSize <= 32Byte)
 * @note IRQ_RX_DATA: (0Byte < DataSize <= 15Byte)
 * @note IRQ_TX_REG_TX_DATA: (0Byte < RegSize <= 4Byte), (0Byte < DataSize <= 32Byte)
 * @note IRQ_TX_REG_RX_DATA: (0Byte < RegSize <= 4Byte), (0Byte < DataSize <= 15Byte)
 * @note DMA_TX_DATA: (4Byte <= DataSize <= 4194300Byte and must align 4)
 */
int32_t I8080_Transfer(void *pDev, I8080_emTransMode mode, uint8_t *pReg, uint8_t RegSize, uint8_t *pData, uint32_t DataSize);


/**
 * @brief Get current device status
 * @param pDev Device handle
 * @param pStatus Status output structure
 * @return Status code
 */
int32_t I8080_GetStatus(void *pDev, I8080_StatusDef *pStatus);

/** @} */ /* End of group I8080_Exported_Functions */
/**
  * @}
  */ /* End of group I8080 */

#endif /* __DRIVER_I8080_H */


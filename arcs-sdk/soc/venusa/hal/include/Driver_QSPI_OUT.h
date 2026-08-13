/**
 * @file:        Driver_QSPI_OUT.h (Serial Peripheral Interface) Driver definitions
 * @Description: This header provides driver interfaces and configuration options for the QSPI output peripheral.
 *              It includes mode settings, DMA controls, status management, and event handling.
 * @Author:       USER
 * @Date:         2025.01.02
 * @Version:      1.0
 * @Notes:        Modified based on original driver structure with added Doxygen documentation.
 */

#ifndef _DRIVER_QSPI_OUT_H
#define _DRIVER_QSPI_OUT_H

#include "Driver_Common.h"

/** @def CSK_QSPI_OUT_API_VERSION
 *  API version number following major.minor format */
#define CSK_QSPI_OUT_API_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,0)  /* API version */
/** @} */ /* End of group QSPI_OUT_DRV_Version */

/**
 * @defgroup QSPI_EVENTS SPI Event Flags
 * @{
 */
typedef enum {
    QSPI_OUT_IRQ_EVENT_TRANSFER_COMPLETE = 0,          /*!< Data transfer completed successfully */
    QSPI_OUT_IRQ_EVENT_BUTT                            /*!< End of enumeration */
} QSPI_OUT_emIrqEvent;

/**
 * @defgroup QSPI_CONTROL
 * @{
 */
typedef enum {
    QSPI_OUT_CONTROL_CLK_OUT_HZ            = (1<<0),   /*!< Set output clock frequency (arg = 200000~100000000Hz) */
    QSPI_OUT_CONTROL_BURST_THD             = (1<<1),   /*!< Set burst threshold (arg = 1/4/8) */
    QSPI_OUT_CONTROL_DMA_SIZE              = (1<<2),   /*!< Set dma send size (arg = xxx Byte) */
    QSPI_OUT_CONTROL_CPOL0_CPHA0           = (1<<3),   /*!< spi_mode: CPOL0_CPHA0 */
    QSPI_OUT_CONTROL_CPOL0_CPHA1           = (1<<4),   /*!< spi_mode: CPOL0_CPHA1 */
    QSPI_OUT_CONTROL_CPOL1_CPHA0           = (1<<5),   /*!< spi_mode: CPOL1_CPHA0 */
    QSPI_OUT_CONTROL_CPOL1_CPHA1           = (1<<6),   /*!< spi_mode: CPOL1_CPHA1*/
    QSPI_OUT_CONTROL_DATA_1LANE            = (1<<7),   /*!< data lane num=1 */
    QSPI_OUT_CONTROL_DATA_2LANE            = (1<<8),   /*!< data lane num=2 */
    QSPI_OUT_CONTROL_DATA_4LANE            = (1<<9),   /*!< data lane num=4 */
    QSPI_OUT_CONTROL_DATA_LENGTH_8         = (1<<10),  /*!< send data 8bit */
    QSPI_OUT_CONTROL_DATA_LENGTH_16        = (1<<11),  /*!< send data 16bit */
    QSPI_OUT_CONTROL_DATA_LENGTH_24        = (1<<12),  /*!< send data 24bit */
    QSPI_OUT_CONTROL_DATA_LENGTH_32        = (1<<13),  /*!< send data 32bit */
    QSPI_OUT_CONTROL_BIT_LSB               = (1<<14),  /*!< Set LSB-first bit order */
    QSPI_OUT_CONTROL_BIT_MSB               = (1<<15),  /*!< Set MSB-first bit order, DMA:bit31~0->bit0~31 IRQ:bit7~0->bit0~7 */
    QSPI_OUT_CONTROL_HALFWORD_LSB          = (1<<16),  /*!< Disable byte conversion for halfwords */
    QSPI_OUT_CONTROL_HALFWORD_MSB          = (1<<17),  /*!< Enable byte swapping for halfwords, {byte3,byte2,byte1,byte0} -> {byte2,byte3,byte0,byte1} */
    QSPI_OUT_CONTROL_FIFO_CLEAR            = (1<<18),  /*!< Clear FIFO buffers */
    QSPI_OUT_CONTROL_GET_FIFO_EMPTY        = (1<<19),  /*!< (return arg = 1:empty 0:not empty) */
} QSPI_OUT_emControl;


/**
 * @typedef QSPI_OUT_SignalEvent_t
 * @brief Function pointer type for SPI event callbacks
 *
 * @param[in] event Event mask indicating which events occurred
 * @param[in] usr_param User-defined parameter passed to callback
 */
typedef void (*QSPI_OUT_SignalEvent_t)(QSPI_OUT_emIrqEvent event, uint32_t param);
/** @} */ /* End of group QSPI_OUT_Exported_Types */

/* Exported functions --------------------------------------------------------*/
/** @defgroup QSPI_OUT_Exported_Functions QSPI_OUT Exported Functions
  * @{
  */
/**
 * @fn CSK_DRIVER_VERSION QSPI_OUT_GetVersion()
 * @brief Retrieve driver version information
 * @return Driver version number following CSK convention
 */
CSK_DRIVER_VERSION QSPI_OUT_GetVersion();

/**
 * @fn void* QSPI_OUT()
 * @brief Obtain SPI device instance pointer
 * @return Pointer to initialized SPI device instance
 */
void* QSPI_OUT();

/**
 * @fn uint32_t QSPI_OUT_Buf()
 * @brief Retrieve RGB buffer instance associated with SPI interface
 * @return Instance handle for RGB buffer management
 */
uint32_t QSPI_OUT_Buf(void);

/**
 * @fn int32_t QSPI_OUT_Initialize()
 * @brief Initialize SPI interface with event callback
 * @param[in] pDev Pointer to SPI device instance
 * @param[in] cb_event Event notification callback function
 * @param[in] param User-defined parameter for callback context
 * @return Execution status code (@ref execution_status)
 */
int32_t QSPI_OUT_Initialize(void *pDev, QSPI_OUT_SignalEvent_t callback, uint32_t param);

/**
 * @fn int32_t QSPI_OUT_Uninitialize()
 * @brief Deinitialize SPI interface and release resources
 * @param[in] pDev Pointer to SPI device instance
 * @return Execution status code (@ref execution_status)
 */
int32_t QSPI_OUT_Uninitialize(void *pDev);

/**
 * @fn int32_t QSPI_OUT_Control()
 * @brief General purpose SPI control interface
 * @param[in] pDev Pointer to SPI device instance
 * @param[in] control Control operation code
 * @param[in] arg Operation-specific argument (optional)
 * @return Combined execution status (@ref execution_status + driver-specific codes)
 */
int32_t QSPI_OUT_Control(void *pDev, QSPI_OUT_emControl control, uint32_t arg);

/**
 * @fn int32_t QSPI_OUT_Send()
 * @brief Initiate data transmission over SPI interface
 * @param[in] pDev Pointer to SPI device instance
 * @param[in] data Pointer to data buffer for transmission
 * @param[in] num Number of data items to transmit
 * @return Execution status code (@ref execution_status)
 */
int32_t QSPI_OUT_Send(void *pDev, const void *data, uint32_t num);

/** @} */ /* End of group QSPI_OUT_Exported_Functions */
/**
  * @}
  */ /* End of group QSPI_OUT */

#endif /*_DRIVER_QSPI_OUT_H */

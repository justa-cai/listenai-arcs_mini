/**
 * Project:      SPI (Serial Peripheral Interface) Driver definitions
 * @file         Driver_SPI.h
 * @author       USER
 * @copyright    Copyright (c) 2022.04.05, ListenAI
 * @brief        This file contains the SPI driver header with configuration macros, data structures, and function prototypes.
 */

#ifndef __DRIVER_SPI_H
#define __DRIVER_SPI_H

#include "Driver_Common.h"

/** @defgroup SPI
  * @brief SPI HAL module driver
  * @{
  */
/** @defgroup SPI_Exported_Constants SPI Exported Constants
  * @{
  */
/** @defgroup SPI_DRV_Version SPI API Version
  * @{
  */
/**
 * @def CSK_SPI_API_VERSION
 * @brief API version number following major.minor format
 */
#define CSK_SPI_API_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,0)  /* API version */
/** @} */ /* End of group SPI_DRV_Version */


/**
 * @def SUPPORT_8BIT_DATA_MERGE
 * @brief Data merge feature support flag (currently disabled)
 */
#define SUPPORT_8BIT_DATA_MERGE     0 // 1

/** @defgroup SPI_Control_Mode SPI Control Codes
  * @{
  */
/**
 * @def CSK_SPI_MODE_Pos
 * @brief Position of mode control bits in control register
 */
#define CSK_SPI_MODE_Pos                0
/**
 * @def CSK_SPI_MODE_Msk
 * @brief Bit mask for mode control field
 */
#define CSK_SPI_MODE_Msk                (0xFUL << CSK_SPI_MODE_Pos)     // bit[3:0]
/**
 * @def CSK_SPI_MODE_UNSET
 * @brief Keep current SPI mode unchanged
 */
#define CSK_SPI_MODE_UNSET              (0x00UL << CSK_SPI_MODE_Pos)    ///< SPI Mode is kept unchanged
/**
 * @def CSK_SPI_MODE_MASTER
 * @brief Set SPI master mode with specified bus speed
 * @arg    Bus speed in bps
 */
#define CSK_SPI_MODE_MASTER             (0x01UL << CSK_SPI_MODE_Pos)    ///< SPI Master (Output on MOSI, Input on MISO); arg = Bus Speed in bps
/**
 * @def CSK_SPI_MODE_SLAVE
 * @brief Set SPI slave mode operation
 */
#define CSK_SPI_MODE_SLAVE              (0x02UL << CSK_SPI_MODE_Pos)    ///< SPI Slave  (Output on MISO, Input on MOSI)
/** @} */ /* End of group SPI_Control_Mode */

/** @defgroup SPI_Control_Tx SPI Control Codes: TX I/O
  * @{
  */
/**
 * @def CSK_SPI_TXIO_Pos
 * @brief Position of transmit I/O control bits
 */
#define CSK_SPI_TXIO_Pos                4
/**
 * @def CSK_SPI_TXIO_Msk
 * @brief Bit mask for transmit I/O control field
 */
#define CSK_SPI_TXIO_Msk                (3UL << CSK_SPI_TXIO_Pos)       // bit[5:4]
/**
 * @def CSK_SPI_TXIO_UNSET
 * @brief Keep current transmit I/O settings unchanged
 */
#define CSK_SPI_TXIO_UNSET              (0x00UL << CSK_SPI_TXIO_Pos)    ///< SPI TX IO is kept unchanged or default (AUTO)
/**
 * @def CSK_SPI_TXIO_DMA
 * @brief Use DMA for transmit operations
 */
#define CSK_SPI_TXIO_DMA                (0x01UL << CSK_SPI_TXIO_Pos)    ///< SPI TX with DMA
/**
 * @def CSK_SPI_TXIO_PIO
 * @brief Use PIO for transmit operations
 */
#define CSK_SPI_TXIO_PIO                (0x02UL << CSK_SPI_TXIO_Pos)    ///< SPI TX with PIO
/**
 * @def CSK_SPI_TXIO_BOTH
 * @brief Enable both DMA and PIO for transmit
 */
#define CSK_SPI_TXIO_BOTH               (CSK_SPI_TXIO_DMA | CSK_SPI_TXIO_PIO)    // SPI TX with DMA & PIO
/**
 * @def CSK_SPI_TXIO_AUTO
 * @brief Preferred DMA fallback to PIO if unavailable
 */
#define CSK_SPI_TXIO_AUTO               CSK_SPI_TXIO_BOTH               ///< SPI TX: DMA preferred, PIO if DMA unavailable
/** @} */ /* End of group SPI_Control_Tx */

/** @defgroup SPI_Control_Rx SPI Control Codes: RX I/O
  * @{
  */
/**
 * @def CSK_SPI_RXIO_Pos
 * @brief Position of receive I/O control bits
 */
#define CSK_SPI_RXIO_Pos                6
/**
 * @def CSK_SPI_RXIO_Msk
 * @brief Bit mask for receive I/O control field
 */
#define CSK_SPI_RXIO_Msk                (3UL << CSK_SPI_RXIO_Pos)       // bit[7:6]
/**
 * @def CSK_SPI_RXIO_UNSET
 * @brief Keep current receive I/O settings unchanged
 */
#define CSK_SPI_RXIO_UNSET              (0x00UL << CSK_SPI_RXIO_Pos)    ///< SPI RX IO is kept unchanged or default (AUTO)
/**
 * @def CSK_SPI_RXIO_DMA
 * @brief Use DMA for receive operations
 */
#define CSK_SPI_RXIO_DMA                (0x01UL << CSK_SPI_RXIO_Pos)    ///< SPI RX with DMA
/**
 * @def CSK_SPI_RXIO_PIO
 * @brief Use PIO for receive operations
 */
#define CSK_SPI_RXIO_PIO                (0x02UL << CSK_SPI_RXIO_Pos)    ///< SPI RX with PIO
/**
 * @def CSK_SPI_RXIO_BOTH
 * @brief Enable both DMA and PIO for receive
 */
#define CSK_SPI_RXIO_BOTH               (CSK_SPI_RXIO_DMA | CSK_SPI_RXIO_PIO)    // SPI RX with DMA & PIO
/**
 * @def CSK_SPI_RXIO_AUTO
 * @brief Preferred DMA fallback to PIO if unavailable
 */
#define CSK_SPI_RXIO_AUTO               CSK_SPI_RXIO_BOTH               ///< SPI RX: DMA preferred, PIO if DMA unavailable
/** @} */ /* End of group SPI_Control_Rx */

/** @defgroup SPI_Control_Format SPI Control Codes: Frame Format
  * @{
  */
/**
 * @def CSK_SPI_FRAME_FORMAT_Pos
 * @brief Position of frame format control bits
 */
#define CSK_SPI_FRAME_FORMAT_Pos         8
/**
 * @def CSK_SPI_FRAME_FORMAT_Msk
 * @brief Bit mask for frame format control field
 */
#define CSK_SPI_FRAME_FORMAT_Msk        (0xFUL << CSK_SPI_FRAME_FORMAT_Pos) // bit[11:8]
/**
 * @def CSK_SPI_FRM_FMT_UNSET
 * @brief Keep current frame format unchanged
 */
#define CSK_SPI_FRM_FMT_UNSET           (0UL << CSK_SPI_FRAME_FORMAT_Pos)   ///< SPI Frame Format is kept unchanged or default
/** @} */ /* End of group SPI_Control_Format */

/** @defgroup SPI_Control_CPOL_CPHA SPI Control Codes
  * @{
  */
/**
 * @def CSK_SPI_CPOL0_CPHA0
 * @brief Standard SPI clock polarity 0, phase 0
 */
#define CSK_SPI_CPOL0_CPHA0             (1UL << CSK_SPI_FRAME_FORMAT_Pos)   ///< Clock Polarity 0, Clock Phase 0 (default)
/**
 * @def CSK_SPI_CPOL0_CPHA1
 * @brief Clock polarity 0, phase 1 configuration
 */
#define CSK_SPI_CPOL0_CPHA1             (2UL << CSK_SPI_FRAME_FORMAT_Pos)   ///< Clock Polarity 0, Clock Phase 1
/**
 * @def CSK_SPI_CPOL1_CPHA0
 * @brief Clock polarity 1, phase 0 configuration
 */
#define CSK_SPI_CPOL1_CPHA0             (3UL << CSK_SPI_FRAME_FORMAT_Pos)   ///< Clock Polarity 1, Clock Phase 0
/**
 * @def CSK_SPI_CPOL1_CPHA1
 * @brief Clock polarity 1, phase 1 configuration
 */
#define CSK_SPI_CPOL1_CPHA1             (4UL << CSK_SPI_FRAME_FORMAT_Pos)   ///< Clock Polarity 1, Clock Phase 1
/** @} */ /* End of group SPI_Control_CPOL_CPHA */

/** @defgroup SPI_Control_Data_Bit SPI Control Codes: Data Bits
  * @{
  */
/**
 * @def CSK_SPI_DATA_BITS_Pos
 * @brief Position of data bits control field
 */
#define CSK_SPI_DATA_BITS_Pos            12
/**
 * @def CSK_SPI_DATA_BITS_Msk
 * @brief Bit mask for data bits control field
 */
#define CSK_SPI_DATA_BITS_Msk           (0x3FUL << CSK_SPI_DATA_BITS_Pos)       // bit[17:12]
/**
 * @def CSK_SPI_DATA_BITS_UNSET
 * @brief Keep current data bit width unchanged
 */
#define CSK_SPI_DATA_BITS_UNSET         (0UL << CSK_SPI_DATA_BITS_Pos)          ///< Number of Data bits is kept unchanged
/**
 * @def CSK_SPI_DATA_BITS(n)
 * @brief Set number of data bits (1-32) or special 40 for 8bit+merge
 * @param n Number of data bits (max 32) or 40 for 8bit with DATA_MERGE
 */
#define CSK_SPI_DATA_BITS(n)            (((n) & 0x3F) << CSK_SPI_DATA_BITS_Pos) ///< Number of Data bits, generally 1 <= n <= 32, the only exception is 40(8+32), it means 8bit with DATA_MERGE

/*----- SPI Control Codes: Bit Order -----*/
/**
 * @def CSK_SPI_BIT_ORDER_Pos
 * @brief Position of bit order control bits
 */
#define CSK_SPI_BIT_ORDER_Pos            18
/**
 * @def CSK_SPI_BIT_ORDER_Msk
 * @brief Bit mask for bit order control field
 */
#define CSK_SPI_BIT_ORDER_Msk           (3UL << CSK_SPI_BIT_ORDER_Pos)      // bit[19:18]
/**
 * @def CSK_SPI_BIT_ORDER_UNSET
 * @brief Keep current bit order unchanged
 */
#define CSK_SPI_BIT_ORDER_UNSET         (0UL << CSK_SPI_BIT_ORDER_Pos)      ///< SPI Bit order is kept unchanged or default
/**
 * @def CSK_SPI_MSB_LSB
 * @brief Most significant bit first transmission
 */
#define CSK_SPI_MSB_LSB                 (1UL << CSK_SPI_BIT_ORDER_Pos)      ///< SPI Bit order from MSB to LSB (default)
/**
 * @def CSK_SPI_LSB_MSB
 * @brief Least significant bit first transmission
 */
#define CSK_SPI_LSB_MSB                 (2UL << CSK_SPI_BIT_ORDER_Pos)      ///< SPI Bit order from LSB to MSB
/** @} */ /* End of group SPI_Control_Data_Bit */

/** @defgroup SPI_Control_IO_Mode SPI Control Codes: Dual_Quad I/O Mode
  * @{
  */
/**
 * @def CSK_SPI_DQ_IO_Pos
 * @brief Position of dual/quad I/O mode control bits
 */
#define CSK_SPI_DQ_IO_Pos               20
/**
 * @def CSK_SPI_DQ_IO_Msk
 * @brief Bit mask for dual/quad I/O mode control field
 */
#define CSK_SPI_DQ_IO_Msk               (3UL << CSK_SPI_DQ_IO_Pos)          // bit[21:20]
/**
 * @def CSK_SPI_DQ_IO_UNSET
 * @brief Keep current I/O mode unchanged
 */
#define CSK_SPI_DQ_IO_UNSET             (0UL << CSK_SPI_DQ_IO_Pos)          ///< SPI Dual_Quad I/O mode is kept unchanged or default
/**
 * @def CSK_SPI_DQ_IO_SINGLE
 * @brief Single-line SPI mode (default)
 */
#define CSK_SPI_DQ_IO_SINGLE            (1UL << CSK_SPI_DQ_IO_Pos)          ///< SPI Regular/Single mode (default)
/**
 * @def CSK_SPI_DQ_IO_DUAL
 * @brief Dual-line SPI mode
 */
#define CSK_SPI_DQ_IO_DUAL              (2UL << CSK_SPI_DQ_IO_Pos)          ///< SPI Dual I/O mode
/**
 * @def CSK_SPI_DQ_IO_QUAD
 * @brief Quad-line SPI mode
 */
#define CSK_SPI_DQ_IO_QUAD              (3UL << CSK_SPI_DQ_IO_Pos)          ///< SPI Quad I/O mode
/** @} */ /* End of group SPI_Control_IO_Mode */

/** @defgroup SPI_Control_Misc SPI Control Codes: Exclusive Controls
  * @{
  */
/**
 * @def CSK_SPI_EXCL_OP_Pos
 * @brief Position of exclusive operation control bits
 */
#define CSK_SPI_EXCL_OP_Pos             28
/**
 * @def CSK_SPI_EXCL_OP_Msk
 * @brief Bit mask for exclusive operation control field
 */
#define CSK_SPI_EXCL_OP_Msk             (0xFUL << CSK_SPI_EXCL_OP_Pos)      // bit[31:28]
/**
 * @def CSK_SPI_EXCL_OP_UNSET
 * @brief No exclusive operations active
 */
#define CSK_SPI_EXCL_OP_UNSET           (0UL << CSK_SPI_EXCL_OP_Pos)        ///< NO exclusive operations
/**
 * @def CSK_SPI_SET_BUS_SPEED
 * @brief Set SPI bus speed in bps
 * @arg    New bus speed value
 */
#define CSK_SPI_SET_BUS_SPEED           (1UL << CSK_SPI_EXCL_OP_Pos)        ///< Set Bus Speed in bps; arg = value
/**
 * @def CSK_SPI_GET_BUS_SPEED
 * @brief Get current SPI bus speed in bps
 */
#define CSK_SPI_GET_BUS_SPEED           (2UL << CSK_SPI_EXCL_OP_Pos)        ///< Get Bus Speed in bps
/**
 * @def CSK_SPI_ABORT_TRANSFER
 * @brief Abort current data transfer
 * @arg    1=remain state, 0=clean state
 */
#define CSK_SPI_ABORT_TRANSFER          (3UL << CSK_SPI_EXCL_OP_Pos)        ///< Abort current data transfer, arg: 1 = remain state; 0 = clean state
/**
 * @def CSK_SPI_RESET_FIFO
 * @brief Reset RX/TX FIFOs
 * @arg    1=RX FIFO, 2=TX FIFO, 3=both
 */
#define CSK_SPI_RESET_FIFO              (4UL << CSK_SPI_EXCL_OP_Pos)        ///< Reset RX/TX FIFO, arg=1: RX FIFO, arg=2: TX FIFO, arg=3: RX & TX FIFO
/**
 * @def CSK_SPI_SET_ADV_ATTR
 * @brief Set advanced attributes via pointer
 * @arg    Pointer to SPI_ADV_ATTR structure
 */
#define CSK_SPI_SET_ADV_ATTR            (5UL << CSK_SPI_EXCL_OP_Pos)        ///< Set advanced attributes, arg = pointer to SPI_ADV_ATTR
/** @} */ /* End of group SPI_Control_Misc */

/** @defgroup SPI_Specific_Error_Code SPI specific error codes
  * @{
  */
/**
 * @def CSK_SPI_ERROR_MODE
 * @brief Unsupported SPI mode selected
 */
#define CSK_SPI_ERROR_MODE              (CSK_DRIVER_ERROR_SPECIFIC - 1)     ///< Specified Mode not supported
/**
 * @def CSK_SPI_ERROR_FRAME_FORMAT
 * @brief Unsupported frame format selected
 */
#define CSK_SPI_ERROR_FRAME_FORMAT      (CSK_DRIVER_ERROR_SPECIFIC - 2)     ///< Specified Frame Format not supported
/**
 * @def CSK_SPI_ERROR_DATA_BITS
 * @brief Unsupported data bit width selected
 */
#define CSK_SPI_ERROR_DATA_BITS         (CSK_DRIVER_ERROR_SPECIFIC - 3)     ///< Specified number of Data bits not supported
/**
 * @def CSK_SPI_ERROR_BIT_ORDER
 * @brief Unsupported bit order selected
 */
#define CSK_SPI_ERROR_BIT_ORDER         (CSK_DRIVER_ERROR_SPECIFIC - 4)     ///< Specified Bit order not supported
/**
 * @def CSK_SPI_ERROR_DQ_IO
 * @brief Unsupported dual/quad I/O mode selected
 */
#define CSK_SPI_ERROR_DQ_IO             (CSK_DRIVER_ERROR_SPECIFIC - 5)     ///< Specified Dual_Quad I/O mode not supported
/** @} */ /* End of group SPI_Specific_Error_Code */

/** @defgroup SPI_EVENT SPI event codes
  * @{
  */
/**
 * @def CSK_SPI_EVENT_TRANSFER_COMPLETE
 * @brief Data transfer completed successfully
 */
#define CSK_SPI_EVENT_TRANSFER_COMPLETE (1UL << 0)  ///< Data Transfer completed
/**
 * @def CSK_SPI_EVENT_DATA_LOST
 * @brief Data loss occurred (overrun/underrun)
 */
#define CSK_SPI_EVENT_DATA_LOST         (1UL << 1)  ///< Data lost: Receive overflow / Transmit underflow
/**
 * @def CSK_SPI_EVENT_SLV_CMD_R
 * @brief Slave mode read command received
 */
#define CSK_SPI_EVENT_SLV_CMD_R         (1UL << 3)  ///< Slave mode, receive read command
/**
 * @def CSK_SPI_EVENT_SLV_CMD_W
 * @brief Slave mode write command received
 */
#define CSK_SPI_EVENT_SLV_CMD_W         (1UL << 4)  ///< Slave mode, receive write command
/**
 * @def CSK_SPI_EVENT_SLV_CMD_S
 * @brief Slave mode status command received
 */
#define CSK_SPI_EVENT_SLV_CMD_S         (1UL << 5)  ///< Slave mode, receive read status command
/** @} */ /* End of group SPI_EVENT */
/** @} */ /* End of group SPI_Exported_Constants */

/** @defgroup SPI_Exported_Types SPI Exported Types
  * @{
  */
/**
 * @enum emIOMode
 * @brief Enumeration of possible I/O modes
 */
typedef enum _emIOMode {
   NO_IO = 0,        ///< No I/O operation
   DMA_IO = 1,       ///< Direct Memory Access mode
   PIO_IO = 2,       ///< Programmed Input/Output mode
   NUM_IO = 3        ///< Total number of I/O modes
} emIOMode;

/**
 * @struct CSK_SPI_STATUS_BIT
 * @brief Bitfield representation of SPI status register
 */
typedef struct _CSK_SPI_STATUS_BIT
{
    uint32_t busy :1;               ///< Transmitter/Receiver busy flag
    uint32_t no_endint :1;          ///< ENDINT interrupt disabled flag
    uint32_t data_ovf :1;          ///< Data overflow detected
    uint32_t data_unf :1;          ///< Data underflow detected
    uint32_t tx_mode :2;           ///< Current transmit mode (0=none, 1=DMA, 2=PIO)
    uint32_t rx_mode :2;            ///< Current receive mode (0=none, 1=DMA, 2=PIO)
    uint32_t rx_sync :1;           ///< Synchronized RX for duplex transfers
    //uint32_t dma_merge :1;      ///< Reserved for future use
    //TODO: add other status flags?
} CSK_SPI_STATUS_BIT;

/**
 * @union CSK_SPI_STATUS
 * @brief Union representing SPI status (raw value or bitfields)
 */
typedef union {
    uint32_t all;     ///< Complete status register value
    CSK_SPI_STATUS_BIT bit; ///< Bitfield representation
} CSK_SPI_STATUS;

/**
 * @struct SPI_ADV_ATTR
 * @brief Advanced SPI attributes configuration structure
 */
typedef struct {
    uint32_t flags;    // indicate which following fields are specified

    uint32_t rx_nsynca      :1; // don't sync cache for RX DMA
    uint32_t tx_nsynca      :1; // don't sync cache for TX DMA
    uint32_t rx_dmach_prio  :3; // RX DMA channel's priority
    uint32_t tx_dmach_prio  :3; // TX DMA channel's priority
    uint32_t rx_dmach_rsvd  :4; // reserved RX DMA channel #
    uint32_t tx_dmach_rsvd  :4; // reserved TX DMA channel #
    uint32_t rx_dma_bsize   :4; // RX DMA burst size, options: 1, 4, 8, 16
    uint32_t tx_dma_bsize   :4; // TX DMA burst size, options: 1, 4, 8, 16

    uint32_t rxdma_wait_odds :1; // wait for odd data when RX DMA is done
    uint32_t reserved       :7;
} SPI_ADV_ATTR;

/**
 * @typedef CSK_SPI_SignalEvent_t
 * @brief Function pointer type for SPI event callbacks
 * @param event Event mask indicating occurring events
 * @param usr_param User-defined parameter passed to callback
 */
typedef void (*CSK_SPI_SignalEvent_t)(uint32_t event, uint32_t usr_param);
/**
 * @typedef SPI_CS_SET_FUNC
 * @brief Function pointer type for SPI cs set callbacks
 * @param spi_dev SPI device index, 0, 1,...
 * @param level 0 = Low, not 0 = High
 */
typedef void (*SPI_CS_SET_FUNC)(void *spi_dev, uint8_t level);
/** @} */ /* End of group SPI_Exported_Types */

/** @defgroup SPI_Exported_Macro SPI Exported Macros
  * @{
  */
/**
 * @enum SPI Status Register Bit Masks
 */
#define SPI_STS_BUSY_MASK       (0x1 << 0)    ///< Busy flag mask
#define SPI_STS_NEND_MASK       (0x1 << 1)    ///< No ENDINT mask
#define SPI_STS_OVF_MASK        (0x1 << 2)    ///< Overflow mask
#define SPI_STS_UNF_MASK        (0x1 << 3)    ///< Underflow mask
#define SPI_STS_RXSYNC_MASK     (0x1 << 8)    ///< RX sync mask

/**
 * @def SPI_STS_TX_MODE_OFFSET
 * @brief Offset for transmit mode bits in status register
 */
#define SPI_STS_TX_MODE_OFFSET      (4)
/**
 * @def SPI_STS_RX_MODE_OFFSET
 * @brief Offset for receive mode bits in status register
 */
#define SPI_STS_RX_MODE_OFFSET      (6)
/**
 * @macro SPI_STS_TX_MODE
 * @brief Macro to extract transmit mode from status register
 * @param status Status register value
 * @return Current transmit mode
 */
#define SPI_STS_TX_MODE(status)     (((status) >> SPI_STS_TX_MODE_OFFSET) & 0x3)
/**
 * @macro SPI_STS_RX_MODE
 * @brief Macro to extract receive mode from status register
 * @param status Status register value
 * @return Current receive mode
 */
#define SPI_STS_RX_MODE(status)     (((status) >> SPI_STS_RX_MODE_OFFSET) & 0x3)

/**
 * @def SPI_ATTR_RX_NSYNCA
 * @brief Don't sync cache for RX DMA operations
 */
#define SPI_ATTR_RX_NSYNCA          (1 << 0)
/**
 * @def SPI_ATTR_TX_NSYNCA
 * @brief Don't sync cache for TX DMA operations
 */
#define SPI_ATTR_TX_NSYNCA          (1 << 1)
/**
 * @def SPI_ATTR_RX_DMACH_PRIO
 * @brief RX DMA channel priority setting
 */
#define SPI_ATTR_RX_DMACH_PRIO      (1 << 2)
/**
 * @def SPI_ATTR_TX_DMACH_PRIO
 * @brief TX DMA channel priority setting
 */
#define SPI_ATTR_TX_DMACH_PRIO      (1 << 3)
/**
 * @def SPI_ATTR_RX_DMACH_RSVD
 * @brief Reserved RX DMA channel number
 */
#define SPI_ATTR_RX_DMACH_RSVD      (1 << 4)
/**
 * @def SPI_ATTR_TX_DMACH_RSVD
 * @brief Reserved TX DMA channel number
 */
#define SPI_ATTR_TX_DMACH_RSVD      (1 << 5)
/**
 * @def SPI_ATTR_RX_DMA_BSIZE
 * @brief RX DMA burst size configuration
 */
#define SPI_ATTR_RX_DMA_BSIZE       (1 << 6)
/**
 * @def SPI_ATTR_TX_DMA_BSIZE
 * @brief TX DMA burst size configuration
 */
#define SPI_ATTR_TX_DMA_BSIZE       (1 << 7)
/**
 * @def SPI_ATTR_RXDMA_WAIT_ODDS
 * @brief Wait for odd data completion in RX DMA
 */
#define SPI_ATTR_RXDMA_WAIT_ODDS    (1 << 8)
/** @} */ /* End of group SPI_Exported_Macro */

/* Exported functions --------------------------------------------------------*/
/** @defgroup SPI_Exported_Functions SPI Exported Functions
  * @{
  */
/**
 * @fn CSK_DRIVER_VERSION SPI_GetVersion (void)
 * @brief Retrieve SPI driver version information
 * @return Driver version number
 */
CSK_DRIVER_VERSION
SPI_GetVersion();

/**
 * @fn int32_t SPI_Initialize (void *spi_dev, CSK_SPI_SignalEvent_t cb_event, uint32_t usr_param)
 * @brief Initialize SPI interface instance
 * @param spi_dev Pointer to SPI device instance
 * @param cb_event Event callback function pointer
 * @param usr_param User-defined parameter for callback
 * @return Execution status code
 */
int32_t
SPI_Initialize(void *spi_dev, CSK_SPI_SignalEvent_t cb_event, uint32_t usr_param);

/**
 * @fn int32_t SPI_Initialize_NCS (void *spi_dev, CSK_SPI_SignalEvent_t cb_event, uint32_t usr_param, SPI_CS_SET_FUNC func_set_cs)
 * @brief Initialize SPI with custom chip select control
 * @param spi_dev Device instance pointer
 * @param cb_event Event callback function
 * @param usr_param User parameter for callback
 * @param func_set_cs Chip select control function
 * @return Execution status code
 */
int32_t
SPI_Initialize_NCS(void *spi_dev, CSK_SPI_SignalEvent_t cb_event, uint32_t usr_param, SPI_CS_SET_FUNC func_set_cs);

/**
 * @fn int32_t SPI_Uninitialize (void *spi_dev)
 * @brief Deinitialize SPI interface instance
 * @param spi_dev Device instance pointer
 * @return Execution status code
 */
int32_t
SPI_Uninitialize(void *spi_dev);

/**
 * @fn int32_t SPI_PowerControl (void *spi_dev, CSK_POWER_STATE state)
 * @brief Control SPI power state
 * @param spi_dev Device instance pointer
 * @param state Target power state
 * @return Execution status code
 */
int32_t
SPI_PowerControl(void *spi_dev, CSK_POWER_STATE state);

/**
 * @fn int32_t SPI_Send (void *spi_dev, const void *data, uint32_t num)
 * @brief Send data through SPI transmitter
 * @param spi_dev Device instance pointer
 * @param data Pointer to transmit buffer
 * @param num Number of data items to send
 * @return Execution status code
 */
int32_t
SPI_Send(void *spi_dev, const void *data, uint32_t num);

/**
 * @fn int32_t SPI_Send_NEnd (void *spi_dev, const void *data, uint32_t num)
 * @brief Send data continuously without ENDINT interrupt
 * @note Experimental API - May behave differently than standard send
 * @param spi_dev Device instance pointer
 * @param data Transmit buffer pointer
 * @param num Number of data items
 * @return Execution status code
 */
int32_t
SPI_Send_NEnd(void *spi_dev, const void *data, uint32_t num);

/**
 * @fn int32_t SPI_Send_PIO_Lite (void *spi_dev, const void *data, uint32_t num, uint32_t no_endint)
 * @brief Lightweight PIO-only transmit operation
 * @note Experimental API - Limited functionality (no data merge)
 * @param spi_dev Device instance pointer
 * @param data Transmit buffer pointer
 * @param num Number of data items
 * @param no_endint Endint disable flag
 * @return Execution status code
 */
int32_t
SPI_Send_PIO_Lite(void *spi_dev, const void *data, uint32_t num, uint32_t no_endint);

/**
 * @fn int32_t SPI_Receive (void *spi_dev, void *data, uint32_t num)
 * @brief Receive data through SPI receiver
 * @param spi_dev Device instance pointer
 * @param data Receive buffer pointer
 * @param num Number of data items to receive
 * @return Execution status code
 */
int32_t
SPI_Receive(void *spi_dev, void *data, uint32_t num);

/**
 * @fn int32_t SPI_Receive_NEnd (void *spi_dev, void *data, uint32_t num)
 * @brief Continuous receive without ENDINT interrupt
 * @note Experimental API - May behave differently than standard receive
 * @param spi_dev Device instance pointer
 * @param data Receive buffer pointer
 * @param num Number of data items
 * @return Execution status code
 */
int32_t
SPI_Receive_NEnd(void *spi_dev, void *data, uint32_t num);

/**
 * @fn int32_t SPI_Receive_DMA_Lite (void *spi_dev, void *data, uint32_t num, uint32_t no_endint)
 * @brief Lightweight DMA-only receive operation
 * @note Experimental API - Requires pre-reserved DMA channels
 * @param spi_dev Device instance pointer
 * @param data Receive buffer pointer
 * @param num Number of data items
 * @param no_endint Endint disable flag
 * @return Execution status code
 */
int32_t
SPI_Receive_DMA_Lite(void *spi_dev, void *data, uint32_t num, uint32_t no_endint);

/**
 * @fn int32_t SPI_Transfer (void *spi_dev, const void *data_out, void *data_in, uint32_t num)
 * @brief Full-duplex SPI transfer (simultaneous send and receive)
 * @param spi_dev Device instance pointer
 * @param data_out Transmit buffer pointer
 * @param data_in Receive buffer pointer
 * @param num Number of data items to transfer
 * @return Execution status code
 */
int32_t
SPI_Transfer(void *spi_dev, const void *data_out, void *data_in, uint32_t num);

// SPI pin mode for Single/Dual/Quad and Bidirectional
typedef enum {
    SPI_PIN_UNIDIR_SINGLE, // Unidirectional Single SPI (MOSI, MISO separate) [default]
    SPI_PIN_BIDIR_SINGLE,  // Bidirectional Single SPI (CS + SCLK + SDIO on MOSI)
    SPI_PIN_DUAL,          // Dual SPI (2 data lines)
    SPI_PIN_QUAD,          // Quad SPI (4 data lines)
    SPI_PIN_MODE_COUNT
} SPI_PIN_MODE;

// Set SPI I/O pin mode (Single/Bidirectional/Dual/Quad)
int32_t SPI_Set_Pin_Mode(void *spi_dev, SPI_PIN_MODE pin_mode);

// Half-duplex polling: write data_out (num_out items), then read data_in (num_in items),
// with optional dummy cycles in between. Ported from vegah driver.
int32_t
SPI_TxRx_Polling(void *spi_dev, const void *data_out, uint32_t num_out,
                 void *data_in, uint32_t num_in, uint8_t interval_dummy_cnt);

/**
 * @fn int32_t SPI_Transfer_NEnd (void *spi_dev, const void *data_out, void *data_in, uint32_t num)
 * @brief Continuous full-duplex transfer without ENDINT
 * @note Experimental API - May behave differently than standard transfer
 * @param spi_dev Device instance pointer
 * @param data_out Transmit buffer pointer
 * @param data_in Receive buffer pointer
 * @param num Number of data items
 * @return Execution status code
 */
int32_t
SPI_Transfer_NEnd(void *spi_dev, const void *data_out, void *data_in, uint32_t num);

/**
 * @fn int32_t SPI_Transfer_PIO_Lite (void *spi_dev, const void *data_out, void *data_in, uint32_t num, uint32_t no_endint)
 * @brief Lightweight PIO-only full-duplex transfer
 * @note Experimental API - Limited functionality (no data merge)
 * @param spi_dev Device instance pointer
 * @param data_out Transmit buffer pointer
 * @param data_in Receive buffer pointer
 * @param num Number of data items
 * @param no_endint Endint disable flag
 * @return Execution status code
 */
int32_t
SPI_Transfer_PIO_Lite(void *spi_dev, const void *data_out, void *data_in, uint32_t num, uint32_t no_endint);

/**
 * @fn void SPI_Wait_Done (void *spi_dev)
 * @brief Block until current SPI operation completes
 * @note Should only be called from task context (not ISR)
 * @param spi_dev Device instance pointer
 */
void SPI_Wait_Done(void *spi_dev);

/**
 * @fn uint32_t SPI_Drain_RX_FIFO (void *spi_dev, uint8_t *buf, uint32_t size)
 * @brief Drain all data from RX FIFO into buffer
 * @param spi_dev Device instance pointer
 * @param buf Buffer to store drained data
 * @param size Maximum number of bytes to drain
 * @return Actual number of bytes drained
 */
uint32_t SPI_Drain_RX_FIFO(void *spi_dev, uint8_t *buf, uint32_t size);

/**
 * @fn uint32_t SPI_GetDataCount (void *spi_dev)
 * @brief Get count of transferred data items
 * @param spi_dev Device instance pointer
 * @return Number of completed data transfers
 */
uint32_t
SPI_GetDataCount(void *spi_dev);

/**
 * @fn int32_t SPI_Control (void *spi_dev, uint32_t control, uint32_t arg)
 * @brief General SPI control interface
 * @param spi_dev Device instance pointer
 * @param control Control code specifying operation
 * @param arg Operation-specific argument
 * @return Combined execution status codes
 */
int32_t
SPI_Control(void *spi_dev, uint32_t control, uint32_t arg);

/**
 * @fn int32_t SPI_GetStatus (void *spi_dev, CSK_SPI_STATUS *status)
 * @brief Retrieve current SPI status
 * @param spi_dev Device instance pointer
 * @param status Pointer to status storage structure
 * @return Execution status code
 */
int32_t
SPI_GetStatus(void *spi_dev, CSK_SPI_STATUS *status);

/**
 * @fn void SPI_Enable_Pull_CS (void *spi_dev, uint8_t enable)
 * @brief Enable/disable internal CS pull-up/down resistors
 * @param spi_dev Device instance pointer
 * @param enable 1=enable, 0=disable
 */
void SPI_Enable_Pull_CS(void *spi_dev, uint8_t enable);

/**
 * @fn void SPI_Pull_CS (void *spi_dev, uint8_t level)
 * @brief Manually drive CS line high/low
 * @param spi_dev Device instance pointer
 * @param level 1=pull high, 0=pull low
 */
void SPI_Pull_CS(void *spi_dev, uint8_t level);

//------------------------------------------------------------------------------------------
/**
 * @fn void* SPI0 ()
 * @brief Obtain handle for SPI0 device instance
 * @return Pointer to SPI0 device instance
 */
void* SPI0();

/**
 * @fn void* SPI1 ()
 * @brief Obtain handle for SPI1 device instance
 * @return Pointer to SPI1 device instance
 */
void* SPI1();

/**
 * @fn uint8_t SPI_Index (void *spi_dev)
 * @brief Get index number of SPI device instance
 * @param spi_dev Device instance pointer
 * @return Device index (0, 1, ...) or 0xFF if invalid
 */
uint8_t SPI_Index(void *spi_dev);
/** @} */ /* End of group SPI_Exported_Functions */
/**
  * @}
  */ /* End of group SPI */
// Note: On CP platform, SPI0 supports only PIO while SPI1 supports both DMA and PIO

#endif /* __DRIVER_SPI_H */

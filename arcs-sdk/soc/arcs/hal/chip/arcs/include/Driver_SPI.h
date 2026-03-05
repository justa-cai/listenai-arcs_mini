/*
 * Project:      SPI (Serial Peripheral Interface) Driver definitions
 */

#ifndef __DRIVER_SPI_H
#define __DRIVER_SPI_H

#include "Driver_Common.h"

#define CSK_SPI_API_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,0)  /* API version */
#define SUPPORT_8BIT_DATA_MERGE     0 // 1

/****** SPI Control Codes *****/

/*----- SPI Control Codes: Mode -----*/
#define CSK_SPI_MODE_Pos                0
#define CSK_SPI_MODE_Msk                (0xFUL << CSK_SPI_MODE_Pos)     // bit[3:0]
#define CSK_SPI_MODE_UNSET              (0x00UL << CSK_SPI_MODE_Pos)    ///< SPI Mode is kept unchanged
#define CSK_SPI_MODE_MASTER             (0x01UL << CSK_SPI_MODE_Pos)    ///< SPI Master (Output on MOSI, Input on MISO); arg = Bus Speed in bps
#define CSK_SPI_MODE_SLAVE              (0x02UL << CSK_SPI_MODE_Pos)    ///< SPI Slave  (Output on MISO, Input on MOSI)

/*----- SPI Control Codes: TX I/O -----*/
#define CSK_SPI_TXIO_Pos                4
#define CSK_SPI_TXIO_Msk                (3UL << CSK_SPI_TXIO_Pos)       // bit[5:4]
#define CSK_SPI_TXIO_UNSET              (0x00UL << CSK_SPI_TXIO_Pos)    ///< SPI TX IO is kept unchanged or default (AUTO)
#define CSK_SPI_TXIO_DMA                (0x01UL << CSK_SPI_TXIO_Pos)    ///< SPI TX with DMA
#define CSK_SPI_TXIO_PIO                (0x02UL << CSK_SPI_TXIO_Pos)    ///< SPI TX with PIO
#define CSK_SPI_TXIO_BOTH               (CSK_SPI_TXIO_DMA | CSK_SPI_TXIO_PIO)    // SPI TX with DMA & PIO
#define CSK_SPI_TXIO_AUTO               CSK_SPI_TXIO_BOTH               ///< SPI TX: DMA preferred, PIO if DMA unavailable
//#define CSK_SPI_TXIO_AUTO               (0x03UL << CSK_SPI_TXIO_Pos)    ///< SPI TX: DMA preferred, PIO if DMA unavailable

/*----- SPI Control Codes: RX I/O -----*/
#define CSK_SPI_RXIO_Pos                6
#define CSK_SPI_RXIO_Msk                (3UL << CSK_SPI_RXIO_Pos)       // bit[7:6]
#define CSK_SPI_RXIO_UNSET              (0x00UL << CSK_SPI_RXIO_Pos)    ///< SPI RX IO is kept unchanged or default (AUTO)
#define CSK_SPI_RXIO_DMA                (0x01UL << CSK_SPI_RXIO_Pos)    ///< SPI RX with DMA
#define CSK_SPI_RXIO_PIO                (0x02UL << CSK_SPI_RXIO_Pos)    ///< SPI RX with PIO
#define CSK_SPI_RXIO_BOTH               (CSK_SPI_RXIO_DMA | CSK_SPI_RXIO_PIO)    // SPI RX with DMA & PIO
#define CSK_SPI_RXIO_AUTO               CSK_SPI_RXIO_BOTH               ///< SPI RX: DMA preferred, PIO if DMA unavailable
//#define CSK_SPI_RXIO_AUTO               (0x03UL << CSK_SPI_RXIO_Pos)    ///< SPI RX: DMA preferred, PIO if DMA unavailable

/*----- SPI Control Codes: Frame Format -----*/
#define CSK_SPI_FRAME_FORMAT_Pos         8
#define CSK_SPI_FRAME_FORMAT_Msk        (0xFUL << CSK_SPI_FRAME_FORMAT_Pos) // bit[11:8]
#define CSK_SPI_FRM_FMT_UNSET           (0UL << CSK_SPI_FRAME_FORMAT_Pos)   ///< SPI Frame Format is kept unchanged or default
#define CSK_SPI_CPOL0_CPHA0             (1UL << CSK_SPI_FRAME_FORMAT_Pos)   ///< Clock Polarity 0, Clock Phase 0 (default)
#define CSK_SPI_CPOL0_CPHA1             (2UL << CSK_SPI_FRAME_FORMAT_Pos)   ///< Clock Polarity 0, Clock Phase 1
#define CSK_SPI_CPOL1_CPHA0             (3UL << CSK_SPI_FRAME_FORMAT_Pos)   ///< Clock Polarity 1, Clock Phase 0
#define CSK_SPI_CPOL1_CPHA1             (4UL << CSK_SPI_FRAME_FORMAT_Pos)   ///< Clock Polarity 1, Clock Phase 1
//#define CSK_SPI_TI_SSI                  (5UL << CSK_SPI_FRAME_FORMAT_Pos)   ///< Texas Instruments Frame Format
//#define CSK_SPI_MICROWIRE               (6UL << CSK_SPI_FRAME_FORMAT_Pos)   ///< National Microwire Frame Format

/*----- SPI Control Codes: Data Bits -----*/
#define CSK_SPI_DATA_BITS_Pos            12
#define CSK_SPI_DATA_BITS_Msk           (0x3FUL << CSK_SPI_DATA_BITS_Pos)       // bit[17:12]
#define CSK_SPI_DATA_BITS_UNSET         (0UL << CSK_SPI_DATA_BITS_Pos)          ///< Number of Data bits is kept unchanged
#define CSK_SPI_DATA_BITS(n)            (((n) & 0x3F) << CSK_SPI_DATA_BITS_Pos) ///< Number of Data bits, generally 1 <= n <= 32, the only
                                                                                ///< exception is 40(8+32), it means 8bit with DATA_MERGE

/*----- SPI Control Codes: Bit Order -----*/
#define CSK_SPI_BIT_ORDER_Pos            18
#define CSK_SPI_BIT_ORDER_Msk           (3UL << CSK_SPI_BIT_ORDER_Pos)      // bit[19:18]
#define CSK_SPI_BIT_ORDER_UNSET         (0UL << CSK_SPI_BIT_ORDER_Pos)      ///< SPI Bit order is kept unchanged or default
#define CSK_SPI_MSB_LSB                 (1UL << CSK_SPI_BIT_ORDER_Pos)      ///< SPI Bit order from MSB to LSB (default)
#define CSK_SPI_LSB_MSB                 (2UL << CSK_SPI_BIT_ORDER_Pos)      ///< SPI Bit order from LSB to MSB

/*----- SPI Control Codes: Dual_Quad I/O Mode  -----*/
#define CSK_SPI_DQ_IO_Pos               20
#define CSK_SPI_DQ_IO_Msk               (3UL << CSK_SPI_DQ_IO_Pos)          // bit[21:20]
#define CSK_SPI_DQ_IO_UNSET             (0UL << CSK_SPI_DQ_IO_Pos)          ///< SPI Dual_Quad I/O mode is kept unchanged or default
#define CSK_SPI_DQ_IO_SINGLE            (1UL << CSK_SPI_DQ_IO_Pos)          ///< SPI Regular/Single mode (default)
#define CSK_SPI_DQ_IO_DUAL              (2UL << CSK_SPI_DQ_IO_Pos)          ///< SPI Dual I/O mode
#define CSK_SPI_DQ_IO_QUAD              (3UL << CSK_SPI_DQ_IO_Pos)          ///< SPI Quad I/O mode

/*----- SPI Control Codes: Exclusive Controls -----*/
/*----- exclusive operations, CANNOT coexist with other Control Codes -----*/
#define CSK_SPI_EXCL_OP_Pos             28
#define CSK_SPI_EXCL_OP_Msk             (0xFUL << CSK_SPI_EXCL_OP_Pos)      // bit[31:28]
#define CSK_SPI_EXCL_OP_UNSET           (0UL << CSK_SPI_EXCL_OP_Pos)        ///< NO exclusive operations
#define CSK_SPI_SET_BUS_SPEED           (1UL << CSK_SPI_EXCL_OP_Pos)        ///< Set Bus Speed in bps; arg = value
#define CSK_SPI_GET_BUS_SPEED           (2UL << CSK_SPI_EXCL_OP_Pos)        ///< Get Bus Speed in bps
#define CSK_SPI_ABORT_TRANSFER          (3UL << CSK_SPI_EXCL_OP_Pos)        ///< Abort current data transfer, arg:
                                                                            // 1 = remain state; 0 = clean state
#define CSK_SPI_RESET_FIFO              (4UL << CSK_SPI_EXCL_OP_Pos)        ///< Reset RX/TX FIFO, arg=1: RX FIFO,
                                                                            // arg=2: TX FIFO, arg=3: RX & TX FIFO
#define CSK_SPI_SET_ADV_ATTR            (5UL << CSK_SPI_EXCL_OP_Pos)        ///< Set advanced attributes, arg = pointer to SPI_ADV_ATTR

/*----- SPI Control Codes: Miscellaneous Controls -----*/


/****** SPI specific error codes *****/
#define CSK_SPI_ERROR_MODE              (CSK_DRIVER_ERROR_SPECIFIC - 1)     ///< Specified Mode not supported
#define CSK_SPI_ERROR_FRAME_FORMAT      (CSK_DRIVER_ERROR_SPECIFIC - 2)     ///< Specified Frame Format not supported
#define CSK_SPI_ERROR_DATA_BITS         (CSK_DRIVER_ERROR_SPECIFIC - 3)     ///< Specified number of Data bits not supported
#define CSK_SPI_ERROR_BIT_ORDER         (CSK_DRIVER_ERROR_SPECIFIC - 4)     ///< Specified Bit order not supported
#define CSK_SPI_ERROR_DQ_IO             (CSK_DRIVER_ERROR_SPECIFIC - 5)     ///< Specified Dual_Quad I/O mode not supported

typedef enum _emIOMode {
   NO_IO = 0,
   DMA_IO = 1,
   PIO_IO = 2,
   NUM_IO = 3 // Total Number of IO Mode
} emIOMode;

/**
 \brief SPI Status
 */
typedef struct _CSK_SPI_STATUS_BIT
{
    uint32_t busy :1;       ///< Transmitter/Receiver busy flag
    uint32_t no_endint :1;  ///< ENDINT is not enabled when set to 1
    uint32_t data_ovf :1;  ///< Data Overflow: receiver overflow (cleared on start of transfer operation)
    uint32_t data_unf :1;  ///< Data Underflow: transmit underflow (cleared on start of transfer operation)
    uint32_t tx_mode :2;   ///< Current TX mode: 0 = NO data TX, 1 = DMA, 2 = PIO
    uint32_t rx_mode :2;   ///< Current RX mode: 0 = NO data RX, 1 = DMA, 2 = PIO
    uint32_t rx_sync :1;   ///< RX of duplex-Transfer is only for sync with TX if set to 1
    //uint32_t dma_merge :1; ///< use WORD data_width of DMA when 8bit DATA MERGE if set to 1
    //TODO: add other status flags?
} CSK_SPI_STATUS_BIT;

#define SPI_STS_BUSY_MASK       (0x1 << 0)
#define SPI_STS_NEND_MASK       (0x1 << 1)
#define SPI_STS_OVF_MASK        (0x1 << 2)
#define SPI_STS_UNF_MASK        (0x1 << 3)
#define SPI_STS_RXSYNC_MASK     (0x1 << 8)

#define SPI_STS_TX_MODE_OFFSET      (4)
#define SPI_STS_RX_MODE_OFFSET      (6)
#define SPI_STS_TX_MODE(status)     (((status) >> SPI_STS_TX_MODE_OFFSET) & 0x3)
#define SPI_STS_RX_MODE(status)     (((status) >> SPI_STS_RX_MODE_OFFSET) & 0x3)

// SPI status
typedef union {
    uint32_t all;
    CSK_SPI_STATUS_BIT bit;
} CSK_SPI_STATUS;

//----------------------------------------
// SPI advanced attributes
#define SPI_ATTR_RX_NSYNCA          (1 << 0)    // don't sync cache for RX DMA
#define SPI_ATTR_TX_NSYNCA          (1 << 1)    // don't sync cache for RX DMA
#define SPI_ATTR_RX_DMACH_PRIO      (1 << 2)    // RX DMA channel's priority
#define SPI_ATTR_TX_DMACH_PRIO      (1 << 3)    // TX DMA channel's priority
#define SPI_ATTR_RX_DMACH_RSVD      (1 << 4)    // reserve RX DMA channel
#define SPI_ATTR_TX_DMACH_RSVD      (1 << 5)    // reserve TX DMA channel
#define SPI_ATTR_RX_DMA_BSIZE       (1 << 6)    // RX DMA burst size
#define SPI_ATTR_TX_DMA_BSIZE       (1 << 7)    // TX DMA burst size
#define SPI_ATTR_RXDMA_WAIT_ODDS    (1 << 8)    // wait for odd data when RX DMA is done

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

//------------------------------------------------------------------------------------------
///****** SPI Event *****/
#define CSK_SPI_EVENT_TRANSFER_COMPLETE     (1UL << 0)  ///< Data Transfer completed
#define CSK_SPI_EVENT_DATA_LOST             (1UL << 1)  ///< Data lost: Receive overflow / Transmit underflow
#define CSK_SPI_EVENT_SLV_CMD_R             (1UL << 3)  ///< Slave mode, receive read command
#define CSK_SPI_EVENT_SLV_CMD_W             (1UL << 4)  ///< Slave mode, receive write command
#define CSK_SPI_EVENT_SLV_CMD_S             (1UL << 5)  ///< Slave mode, receive read status command
#define CSK_SPI_EVENT_DMA_BLOCK_COMPLETE    (1UL << 6)


/**
 \fn          void CSK_SPI_SignalEvent_t (uint32_t event, uint32_t usr_param)
 \brief       Signal SPI Events.
 \param[in]   event        SPI event notification mask
 \param[in]   usr_param    user parameter
 \return      none
*/
typedef void
(*CSK_SPI_SignalEvent_t)(uint32_t event, uint32_t usr_param);

//------------------------------------------------------------------------------------------
/**
 \fn          CSK_DRIVER_VERSION SPI_GetVersion (void)
 \brief       Get driver version.
 \return      \ref CSK_DRIVER_VERSION
*/
CSK_DRIVER_VERSION
SPI_GetVersion();

/**
 \fn          int32_t SPI_Initialize (void *spi_dev, CSK_SPI_SignalEvent_t cb_event, uint32_t usr_param)
 \brief       Initialize SPI Interface.
 \param[in]   spi_dev  Pointer to SPI device instance
 \param[in]   cb_event  Pointer to \ref CSK_SPI_SignalEvent_t
 \param[in]   usr_param  User-defined value, acts as last parameter of cb_event
 \return      \ref execution_status
*/
int32_t
SPI_Initialize(void *spi_dev, CSK_SPI_SignalEvent_t cb_event, uint32_t usr_param);

//spi_idx   SPI device index, 0, 1,...
//level     0 = Low, not 0 = High
//typedef void (*SPI_CS_SET_FUNC)(uint8_t spi_idx, uint8_t level);
typedef void (*SPI_CS_SET_FUNC)(void *spi_dev, uint8_t level);
int32_t
SPI_Initialize_NCS(void *spi_dev, CSK_SPI_SignalEvent_t cb_event, uint32_t usr_param, SPI_CS_SET_FUNC func_set_cs);

/**
 \fn          int32_t SPI_Uninitialize (void)
 \brief       De-initialize SPI Interface.
 \param[in]   spi_dev  Pointer to SPI device instance
 \return      \ref execution_status
*/
int32_t
SPI_Uninitialize(void *spi_dev);

/**
 \fn          int32_t SPI_PowerControl (void *spi_dev, CSK_POWER_STATE state)
 \brief       Control SPI Interface Power.
 \param[in]   spi_dev  Pointer to SPI device instance
 \param[in]   state  Power state
 \return      \ref execution_status
*/
int32_t
SPI_PowerControl(void *spi_dev, CSK_POWER_STATE state);

/**
 \fn          int32_t SPI_Send (void *spi_dev, const void *data, uint32_t num)
 \brief       Start sending data to SPI transmitter.
 \param[in]   spi_dev  Pointer to SPI device instance
 \param[in]   data  Pointer to buffer with data to send to SPI transmitter
 \param[in]   num   Number of data items to send
 \return      \ref execution_status
*/
int32_t
SPI_Send(void *spi_dev, const void *data, uint32_t num);

// Experimental API: SPI_Send_NEnd
// Sending data continuously without triggering ENDINT interrupt (when CS signal is raised up)
int32_t
SPI_Send_NEnd(void *spi_dev, const void *data, uint32_t num);

// Experimental API: SPI_Send_PIO_Lite
// simplified, stand-alone PIO Send operation (with the least configuration)
//NOTE: 8BIT_DATA_MERGE is NOT supported.
int32_t
SPI_Send_PIO_Lite(void *spi_dev, const void *data, uint32_t num, uint32_t no_endint);

/**
 \fn          int32_t SPI_Receive (void *spi_dev, void *data, uint32_t num)
 \brief       Start receiving data from SPI receiver.
 \param[in]   spi_dev  Pointer to SPI device instance
 \param[out]  data  Pointer to buffer for data to receive from SPI receiver
 \param[in]   num   Number of data items to receive
 \return      \ref execution_status
*/
int32_t
SPI_Receive(void *spi_dev, void *data, uint32_t num);

// Experimental API: SPI_Receive_NEnd
// Receiving data continuously without triggering ENDINT interrupt (when CS signal is raised up)
int32_t
SPI_Receive_NEnd(void *spi_dev, void *data, uint32_t num);

// Experimental API: SPI_Receive_DMA_Lite
// simplified, stand-alone DMA Receive operation (with the least configuration)
//NOTE: 8BIT_DATA_MERGE is NOT supported and DMA channel should be reserved in advance!
int32_t
SPI_Receive_DMA_Lite(void *spi_dev, void *data, uint32_t num, uint32_t no_endint);

// Experimental API: SPI_Receive_PIO_Lite
// simplified, stand-alone PIO Receive operation (with the least configuration)
//NOTE: 8BIT_DATA_MERGE is NOT supported. Actually the API is impractical, only for integrity.
//int32_t
//SPI_Receive_PIO_Lite(void *spi_dev, void *data, uint32_t num, uint32_t no_endint);

/**
 \fn          int32_t SPI_Transfer (void *spi_dev, const void *data_out, void *data_in, uint32_t num)
 \brief       Start sending/receiving data to/from SPI transmitter/receiver.
 \param[in]   spi_dev  Pointer to SPI device instance
 \param[in]   data_out  Pointer to buffer with data to send to SPI transmitter
 \param[out]  data_in   Pointer to buffer for data to receive from SPI receiver
 \param[in]   num       Number of data items to transfer
 \return      \ref execution_status
*/
int32_t
SPI_Transfer(void *spi_dev, const void *data_out, void *data_in, uint32_t num);

// Experimental API: SPI_Transfer_NEnd
// Duplex-Transfering data continuously without triggering ENDINT interrupt (when CS signal is raised up)
int32_t
SPI_Transfer_NEnd(void *spi_dev, const void *data_out, void *data_in, uint32_t num);

// Experimental API: SPI_Transfer_PIO_Lite
// simplified, stand-alone PIO Duplex-Transfer operation (with the least configuration)
//NOTE: 8BIT_DATA_MERGE is NOT supported.
int32_t
SPI_Transfer_PIO_Lite(void *spi_dev, const void *data_out, void *data_in, uint32_t num, uint32_t no_endint);

// Experimental API: SPI_Wait_Done
// Wait for SPI operation (Send/Receive/Transfer) done...
// NOTE: This API function can be called in TASK context ONLY!!
//      And it may cause dead lock when called in ISR context...
void SPI_Wait_Done(void *spi_dev);

// Experimental API: SPI_Drain_RX_FIFO
// Drain out all the data in the RX FIFO
// buf  The buffer to hold data read out from RX FIFO
// len  The size of buffer in byte
// return the length (in bytes) of actually read data.
uint32_t SPI_Drain_RX_FIFO(void *spi_dev, uint8_t *buf, uint32_t size);

/**
 \fn          uint32_t SPI_GetDataCount (void *spi_dev)
 \brief       Get transferred data count.
 \param[in]   spi_dev  Pointer to SPI device instance
 \return      number of data items transferred
*/
uint32_t
SPI_GetDataCount(void *spi_dev);

/**
 \fn          int32_t SPI_Control (void *spi_dev, uint32_t control, uint32_t arg)
 \brief       Control SPI Interface.
 \param[in]   spi_dev  Pointer to SPI device instance
 \param[in]   control  Operation
 \param[in]   arg      Argument of operation (optional)
 \return      common \ref execution_status and driver specific \ref spi_execution_status
*/
int32_t
SPI_Control(void *spi_dev, uint32_t control, uint32_t arg);

/**
 \fn          CSK_SPI_STATUS SPI_GetStatus (void *spi_dev)
 \brief       Get SPI status.
 \param[in]   spi_dev  Pointer to SPI device instance
 \param[out]  status  Pointer to CSK_SPI_STATUS buffer
 \return      \ref execution_status
 */
int32_t
SPI_GetStatus(void *spi_dev, CSK_SPI_STATUS *status);

// Enable (enable = 1) or disable (enable = 0) pulling up/down CS signal from SPI internal
void SPI_Enable_Pull_CS(void *spi_dev, uint8_t enable);

// pull up (level = 1) or down (level = 0) CS signal from SPI internal
void SPI_Pull_CS(void *spi_dev, uint8_t level);

//------------------------------------------------------------------------------------------
/**
 \fn          void* SPI0()
 \brief       Get SPI0 device instance
 \return      SPI0 device instance
 */
void* SPI0();

/**
 \fn          void* SPI1()
 \brief       Get SPI1 device instance
 \return      SPI1 device instance
 */
void* SPI1();

/**
 \fn          void* SPI2()
 \brief       Get SPI2 device instance
 \return      SPI2 device instance
 */
void* SPI2();

/**
 \fn          uint16_t SPI_Index(void *spi_dev)
 \brief       Get SPI device index
 \return      SPI device index, 0, 1,..., and 0xFF if not existing.
 */
uint8_t SPI_Index(void *spi_dev);


/**​
 * @brief Start continuous SPI data reception in Ping-Pong mode without triggering ENDINT interrupt.
 * @details This function initializes and starts a Ping-Pong mode DMA transfer for SPI reception.
 *  It uses two buffers (data0 and data1) to continuously receive data without interruption.
 *  When CS signal is raised, the transfer continues without generating ENDINT interrupt.
 * @param[in] spi_dev Pointer to the SPI device instance
 * @param[in] data0 Pointer to the first receive buffer
 * @param[in] data1 Pointer to the second receive buffer (can be NULL)
 * @param[in] num Number of data elements to receive in each buffer (byte)
 * @return Execution status
 * @retval CSK_DRIVER_OK Operation successful
 * @retval CSK_DRIVER_ERROR_PARAMETER Invalid parameter provided
 * @retval CSK_DRIVER_ERROR SPI device not configured or DMA error
 */
int32_t SPI_Receive_PiPo_Start(void *spi_dev, void *data0, void *data1, uint32_t num);

/**
 * @brief Cancel the circular Ping-Pong reception operation.
 * @details This function breaks the circular chain of Ping-Pong reception but does not
 *  immediately stop the ongoing transfer. The current buffer transfer will complete.
 * @param[in] spi_dev Pointer to the SPI device instance
 * @return Execution status
 * @retval CSK_DRIVER_OK Operation successful
 * @retval CSK_DRIVER_ERROR_PARAMETER Invalid parameter provided
 * @retval CSK_DRIVER_OK If channel is already idle or not using DMA
 */
int32_t SPI_Receive_PiPo_Stop(void *spi_dev);

/**
 * @brief Get the current buffer address and size for Ping-Pong reception.
 * @details This function retrieves information about the current active buffer in the
 *  Ping-Pong reception operation, including its address and size.
 * @param[in] spi_dev Pointer to the SPI device instance
 * @param[out] addr Pointer to store the current buffer address
 * @param[out] num Pointer to store the size of byte
 * @return Number of transferred blocks if successful, error code otherwise
 * @retval >=0 Number of transferred blocks
 * @retval CSK_DRIVER_ERROR_PARAMETER Invalid parameter provided
 * @retval CSK_DRIVER_OK If channel is idle or not using DMA
 */
int32_t SPI_Receive_PiPo_Get_Addr(void *spi_dev, void *addr, uint32_t *num);


//
//NOTES:
// for SPI TX/RX IO on platform CP,
// only PIO is supported on SPI0, and both DMA and PIO are supported on SPI1!
//

#endif /* __DRIVER_SPI_H */

/*
 * Project:      QSPI_LCD (Serial Peripheral Interface) Driver definitions
 */

#ifndef __INCLUDE_DRIVER_QSPI_LCD_H
#define __INCLUDE_DRIVER_QSPI_LCD_H

#include "Driver_Common.h"

#define CSK_QSPI_LCD_API_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,0)  /* API version */
#define CSK_QSPI_LCD_SUPPORT_8BIT_DATA_MERGE     0 // 1

/*----- QSPI Transfer mode -----*/
typedef enum {
    CSK_QSPI_LCD_LANE_NUM_SINGLE = 0,
    CSK_QSPI_LCD_LANE_NUM_DUAL,
    CSK_QSPI_LCD_LANE_NUM_QUAD,
    CSK_QSPI_LCD_LANE_NUM_BUTT,
} em_CSK_QSPI_LCD_LANE_NUM;

/*----- QSPI LCD mode -----*/
typedef enum {
    CSK_QSPI_LCD_MODE_NORMAL = 0,
    CSK_QSPI_LCD_MODE_888_TO_666,
    CSK_QSPI_LCD_MODE_ARGB_TO_666,
    CSK_QSPI_LCD_MODE_ARGB_TO_888,
    CSK_QSPI_LCD_MODE_BUTT,
} em_CSK_QSPI_LCD_Mode;

/*----- QSPI DMA mode -----*/
#define CSK_QSPI_LCD_DMA_TX                  (0x0)
#define CSK_QSPI_LCD_DMA_RX                  (0x1)
#define CSK_QSPI_LCD_DMA_BOTH                (0x2)

/*----- SPI Control Codes: Mode -----*/
#define CSK_QSPI_LCD_MODE_Pos                0
#define CSK_QSPI_LCD_MODE_Msk                (0xFUL << CSK_QSPI_LCD_MODE_Pos)     // bit[3:0]
#define CSK_QSPI_LCD_MODE_UNSET              (0x00UL << CSK_QSPI_LCD_MODE_Pos)    ///< SPI Mode is kept unchanged
#define CSK_QSPI_LCD_MODE_MASTER             (0x01UL << CSK_QSPI_LCD_MODE_Pos)    ///< SPI Master (Output on MOSI, Input on MISO); arg = Bus Speed in bps
#define CSK_QSPI_LCD_MODE_SLAVE              (0x02UL << CSK_QSPI_LCD_MODE_Pos)    ///< SPI Slave  (Output on MISO, Input on MOSI)

/*----- SPI Control Codes: TX I/O -----*/
#define CSK_QSPI_LCD_TXIO_Pos                4
#define CSK_QSPI_LCD_TXIO_Msk                (3UL << CSK_QSPI_LCD_TXIO_Pos)       // bit[5:4]
#define CSK_QSPI_LCD_TXIO_UNSET              (0x00UL << CSK_QSPI_LCD_TXIO_Pos)    ///< SPI TX IO is kept unchanged or default (AUTO)
#define CSK_QSPI_LCD_TXIO_DMA                (0x01UL << CSK_QSPI_LCD_TXIO_Pos)    ///< SPI TX with DMA
#define CSK_QSPI_LCD_TXIO_PIO                (0x02UL << CSK_QSPI_LCD_TXIO_Pos)    ///< SPI TX with PIO
#define CSK_QSPI_LCD_TXIO_BOTH               (CSK_QSPI_LCD_TXIO_DMA | CSK_QSPI_LCD_TXIO_PIO)    // SPI TX with DMA & PIO
#define CSK_QSPI_LCD_TXIO_AUTO               CSK_QSPI_LCD_TXIO_BOTH               ///< SPI TX: DMA preferred, PIO if DMA unavailable
//#define CSK_QSPI_LCD_TXIO_AUTO               (0x03UL << CSK_QSPI_LCD_TXIO_Pos)    ///< SPI TX: DMA preferred, PIO if DMA unavailable

/*----- SPI Control Codes: RX I/O -----*/
#define CSK_QSPI_LCD_RXIO_Pos                6
#define CSK_QSPI_LCD_RXIO_Msk                (3UL << CSK_QSPI_LCD_RXIO_Pos)       // bit[7:6]
#define CSK_QSPI_LCD_RXIO_UNSET              (0x00UL << CSK_QSPI_LCD_RXIO_Pos)    ///< SPI RX IO is kept unchanged or default (AUTO)
#define CSK_QSPI_LCD_RXIO_DMA                (0x01UL << CSK_QSPI_LCD_RXIO_Pos)    ///< SPI RX with DMA
#define CSK_QSPI_LCD_RXIO_PIO                (0x02UL << CSK_QSPI_LCD_RXIO_Pos)    ///< SPI RX with PIO
#define CSK_QSPI_LCD_RXIO_BOTH               (CSK_QSPI_LCD_RXIO_DMA | CSK_QSPI_LCD_RXIO_PIO)    // SPI RX with DMA & PIO
#define CSK_QSPI_LCD_RXIO_AUTO               CSK_QSPI_LCD_RXIO_BOTH               ///< SPI RX: DMA preferred, PIO if DMA unavailable
//#define CSK_QSPI_LCD_RXIO_AUTO               (0x03UL << CSK_QSPI_LCD_RXIO_Pos)    ///< SPI RX: DMA preferred, PIO if DMA unavailable

/*----- SPI Control Codes: Mode Parameters: Frame Format -----*/
#define CSK_QSPI_LCD_FRAME_FORMAT_Pos         8
#define CSK_QSPI_LCD_FRAME_FORMAT_Msk        (0xFUL << CSK_QSPI_LCD_FRAME_FORMAT_Pos) // bit[11:8]
#define CSK_QSPI_LCD_FRM_FMT_UNSET           (0UL << CSK_QSPI_LCD_FRAME_FORMAT_Pos)   ///< SPI Frame Format is kept unchanged or default
#define CSK_QSPI_LCD_CPOL0_CPHA0             (1UL << CSK_QSPI_LCD_FRAME_FORMAT_Pos)   ///< Clock Polarity 0, Clock Phase 0 (default)
#define CSK_QSPI_LCD_CPOL0_CPHA1             (2UL << CSK_QSPI_LCD_FRAME_FORMAT_Pos)   ///< Clock Polarity 0, Clock Phase 1
#define CSK_QSPI_LCD_CPOL1_CPHA0             (3UL << CSK_QSPI_LCD_FRAME_FORMAT_Pos)   ///< Clock Polarity 1, Clock Phase 0
#define CSK_QSPI_LCD_CPOL1_CPHA1             (4UL << CSK_QSPI_LCD_FRAME_FORMAT_Pos)   ///< Clock Polarity 1, Clock Phase 1
//#define CSK_QSPI_LCD_TI_SSI                  (5UL << CSK_QSPI_LCD_FRAME_FORMAT_Pos)   ///< Texas Instruments Frame Format
//#define CSK_QSPI_LCD_MICROWIRE               (6UL << CSK_QSPI_LCD_FRAME_FORMAT_Pos)   ///< National Microwire Frame Format

/*----- SPI Control Codes: Mode Parameters: Data Bits -----*/
#define CSK_QSPI_LCD_DATA_BITS_Pos            12
#define CSK_QSPI_LCD_DATA_BITS_Msk           (0x3FUL << CSK_QSPI_LCD_DATA_BITS_Pos)       // bit[17:12]
#define CSK_QSPI_LCD_DATA_BITS_UNSET         (0UL << CSK_QSPI_LCD_DATA_BITS_Pos)          ///< Number of Data bits is kept unchanged
#define CSK_QSPI_LCD_DATA_BITS(n)            (((n) & 0x3F) << CSK_QSPI_LCD_DATA_BITS_Pos) ///< Number of Data bits, generally 1 <= n <= 32, the only
                                                                                ///< exception is 40(8+32), it means 8bit with DATA_MERGE

/*----- SPI Control Codes: Mode Parameters: Bit Order -----*/
#define CSK_QSPI_LCD_BIT_ORDER_Pos            18
#define CSK_QSPI_LCD_BIT_ORDER_Msk           (3UL << CSK_QSPI_LCD_BIT_ORDER_Pos)      // bit[19:18]
#define CSK_QSPI_LCD_BIT_ORDER_UNSET         (0UL << CSK_QSPI_LCD_BIT_ORDER_Pos)      ///< SPI Bit order is kept unchanged or default
#define CSK_QSPI_LCD_MSB_LSB                 (1UL << CSK_QSPI_LCD_BIT_ORDER_Pos)      ///< SPI Bit order from MSB to LSB (default)
#define CSK_QSPI_LCD_LSB_MSB                 (2UL << CSK_QSPI_LCD_BIT_ORDER_Pos)      ///< SPI Bit order from LSB to MSB

/*----- SPI Control Codes: Transfer Controls -----*/
/*----- command phase enable/disable -----*/
#define CSK_QSPI_LCD_CMD_PHASE_Pos             20
#define CSK_QSPI_LCD_CMD_PHASE_Msk             (0x1UL << CSK_QSPI_LCD_CMD_PHASE_Pos)     // bit[20:20]
#define CSK_QSPI_LCD_CMD_PHASE_DISABLE         (0x0UL << CSK_QSPI_LCD_CMD_PHASE_Pos)
#define CSK_QSPI_LCD_CMD_PHASE_ENABLE          (0x1UL << CSK_QSPI_LCD_CMD_PHASE_Pos)

/*----- Address phase enable/disable, Address length -----*/
#define CSK_QSPI_LCD_ADDR_PHASE_Pos            21
#define CSK_QSPI_LCD_ADDR_PHASE_Msk            (0x1UL << CSK_QSPI_LCD_ADDR_PHASE_Pos)      // bit[21:21]
#define CSK_QSPI_LCD_ADDR_PHASE_DISABLE        (0x0UL << CSK_QSPI_LCD_ADDR_PHASE_Pos)
#define CSK_QSPI_LCD_ADDR_PHASE_ENABLE         (0x1UL << CSK_QSPI_LCD_ADDR_PHASE_Pos)
#define CSK_QSPI_LCD_ADDR_PHASE_FMT_Pos        22                                     // bit[23:22]
#define CSK_QSPI_LCD_ADDR_PHASE_FMT_Msk        (0x3UL << CSK_QSPI_LCD_ADDR_PHASE_FMT_Pos)
#define CSK_QSPI_LCD_ADDR_PHASE_1BYTES         (0x0UL << CSK_QSPI_LCD_ADDR_PHASE_FMT_Pos)
#define CSK_QSPI_LCD_ADDR_PHASE_2BYTES         (0x1UL << CSK_QSPI_LCD_ADDR_PHASE_FMT_Pos)
#define CSK_QSPI_LCD_ADDR_PHASE_3BYTES         (0x2UL << CSK_QSPI_LCD_ADDR_PHASE_FMT_Pos)
#define CSK_QSPI_LCD_ADDR_PHASE_4BYTES         (0x3UL << CSK_QSPI_LCD_ADDR_PHASE_FMT_Pos)
#define CSK_QSPI_LCD_REG_ADDR_PHASE_FMT_Pos    16


/*----- SPI Control Codes: Exclusive Controls -----*/
/*----- exclusive operations, CANNOT coexist with other Control Codes -----*/
#define CSK_QSPI_LCD_EXCL_OP_Pos             24
#define CSK_QSPI_LCD_EXCL_OP_Msk             (0xFUL << CSK_QSPI_LCD_EXCL_OP_Pos)      // bit[27:24]
#define CSK_QSPI_LCD_EXCL_OP_UNSET           (0UL << CSK_QSPI_LCD_EXCL_OP_Pos)        ///< NO exclusive operations
#define CSK_QSPI_LCD_SET_BUS_SPEED           (1UL << CSK_QSPI_LCD_EXCL_OP_Pos)        ///< Set Bus Speed in bps; arg = value
#define CSK_QSPI_LCD_GET_BUS_SPEED           (2UL << CSK_QSPI_LCD_EXCL_OP_Pos)        ///< Get Bus Speed in bps
#define CSK_QSPI_LCD_ABORT_TRANSFER          (3UL << CSK_QSPI_LCD_EXCL_OP_Pos)        ///< Abort current data transfer, arg:
                                                                            // 1 = remain state; 0 = clean state
#define CSK_QSPI_LCD_RESET_FIFO              (4UL << CSK_QSPI_LCD_EXCL_OP_Pos)        ///< Reset RX/TX FIFO, arg=1: RX FIFO,
                                                                            // arg=2: TX FIFO, arg=3: RX & TX FIFO
#define CSK_QSPI_LCD_SET_ADV_ATTR            (5UL << CSK_QSPI_LCD_EXCL_OP_Pos)        ///< Set advanced attributes, arg = pointer to SPI_ADV_ATTR

/*----- SPI Control Codes: DMA enable/disable/size -----*/
#define CSK_QSPI_LCD_DMA_Pos                 28
#define CSK_QSPI_LCD_DMA_Msk                 (0x3UL << CSK_QSPI_LCD_DMA_Pos)
#define CSK_QSPI_LCD_DMA_UNSET               (0x0UL << CSK_QSPI_LCD_DMA_Pos)
#define CSK_QSPI_LCD_DMA_ENABLE              (0x1UL << CSK_QSPI_LCD_DMA_Pos)
#define CSK_QSPI_LCD_DMA_DISABLE             (0x2UL << CSK_QSPI_LCD_DMA_Pos)
#define CSK_QSPI_LCD_DMA_SIZE                (0x3UL << CSK_QSPI_LCD_DMA_Pos)


/****** SPI specific error codes *****/
#define CSK_QSPI_LCD_ERROR_MODE              (CSK_DRIVER_ERROR_SPECIFIC - 1)     ///< Specified Mode not supported
#define CSK_QSPI_LCD_ERROR_FRAME_FORMAT      (CSK_DRIVER_ERROR_SPECIFIC - 2)     ///< Specified Frame Format not supported
#define CSK_QSPI_LCD_ERROR_DATA_BITS         (CSK_DRIVER_ERROR_SPECIFIC - 3)     ///< Specified number of Data bits not supported
#define CSK_QSPI_LCD_ERROR_BIT_ORDER         (CSK_DRIVER_ERROR_SPECIFIC - 4)     ///< Specified Bit order not supported

typedef enum _em_CSK_QSPI_LCD_IOMode {
    CSK_QSPI_LCD_NO_IO = 0,
    CSK_QSPI_LCD_DMA_IO = 1,
    CSK_QSPI_LCD_PIO_IO = 2,
    CSK_QSPI_LCD_NUM_IO = 3 // Total Number of IO Mode
} em_CSK_QSPI_LCD_IOMode;

/**
 \brief SPI Status
 */
typedef struct _CSK_QSPI_LCD_STATUS_BIT
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
} CSK_QSPI_LCD_STATUS_BIT;

#define CSK_QSPI_LCD_STS_BUSY_MASK       (0x1 << 0)
#define CSK_QSPI_LCD_STS_NEND_MASK       (0x1 << 1)
#define CSK_QSPI_LCD_STS_OVF_MASK        (0x1 << 2)
#define CSK_QSPI_LCD_STS_UNF_MASK        (0x1 << 3)
#define CSK_QSPI_LCD_STS_RXSYNC_MASK     (0x1 << 8)

#define CSK_QSPI_LCD_STS_TX_MODE_OFFSET      (4)
#define CSK_QSPI_LCD_STS_RX_MODE_OFFSET      (6)
#define CSK_QSPI_LCD_STS_TX_MODE(status)     (((status) >> CSK_QSPI_LCD_STS_TX_MODE_OFFSET) & 0x3)
#define CSK_QSPI_LCD_STS_RX_MODE(status)     (((status) >> CSK_QSPI_LCD_STS_RX_MODE_OFFSET) & 0x3)

// SPI status
typedef union {
    uint32_t all;
    CSK_QSPI_LCD_STATUS_BIT bit;
} CSK_QSPI_LCD_STATUS;

//----------------------------------------
// SPI advanced attributes
#define CSK_QSPI_LCD_ATTR_RX_NSYNCA          (1 << 0)    // don't sync cache for RX DMA
#define CSK_QSPI_LCD_ATTR_TX_NSYNCA          (1 << 1)    // don't sync cache for RX DMA
#define CSK_QSPI_LCD_ATTR_RX_DMACH_PRIO      (1 << 2)    // RX DMA channel's priority
#define CSK_QSPI_LCD_ATTR_TX_DMACH_PRIO      (1 << 3)    // TX DMA channel's priority
#define CSK_QSPI_LCD_ATTR_RX_DMACH_RSVD      (1 << 4)    // reserve RX DMA channel
#define CSK_QSPI_LCD_ATTR_TX_DMACH_RSVD      (1 << 5)    // reserve TX DMA channel
#define CSK_QSPI_LCD_ATTR_RX_DMA_BSIZE       (1 << 6)    // RX DMA burst size
#define CSK_QSPI_LCD_ATTR_TX_DMA_BSIZE       (1 << 7)    // TX DMA burst size
#define CSK_QSPI_LCD_ATTR_RXDMA_WAIT_ODDS    (1 << 8)    // wait for odd data when RX DMA is done


//------------------------------------------------------------------------------------------
///****** SPI Event *****/
#define CSK_QSPI_LCD_EVENT_TRANSFER_COMPLETE (1UL << 0)  ///< Data Transfer completed
#define CSK_QSPI_LCD_EVENT_DATA_LOST         (1UL << 1)  ///< Data lost: Receive overflow / Transmit underflow
#define CSK_QSPI_LCD_EVENT_SLV_CMD_R         (1UL << 3)  ///< Slave mode, receive read command
#define CSK_QSPI_LCD_EVENT_SLV_CMD_W         (1UL << 4)  ///< Slave mode, receive write command
#define CSK_QSPI_LCD_EVENT_SLV_CMD_S         (1UL << 5)  ///< Slave mode, receive read status command


/**
 \fn          void QSPI_LCD_SignalEvent_t (uint32_t event, uint32_t usr_param)
 \brief       Signal SPI Events.
 \param[in]   event        SPI event notification mask
 \param[in]   usr_param    user parameter
 \return      none
*/
typedef void
(*QSPI_LCD_SignalEvent_t)(uint32_t event, uint32_t usr_param);

//------------------------------------------------------------------------------------------
/**
 \fn          CSK_DRIVER_VERSION SPI_GetVersion (void)
 \brief       Get driver version.
 \return      \ref CSK_DRIVER_VERSION
*/
CSK_DRIVER_VERSION
QSPI_LCD_GetVersion();


/**
 \fn          void* QSPI_LCD()
 \brief       Get QSPI_LCD device instance
 \return      QSPI_LCD device instance
 */
void* QSPI_LCD();


/**
  * @brief  Return RGB BUF instance.
  *
  * @return Instance of RGB
  */
uint32_t QSPI_LCD_Buf(void);


/**
 \fn          int32_t SPI_Initialize (void *spi_dev, CSK_QSPI_LCD_SignalEvent_t cb_event, uint32_t usr_param)
 \brief       Initialize SPI Interface.
 \param[in]   spi_dev  Pointer to SPI device instance
 \param[in]   cb_event  Pointer to \ref CSK_QSPI_LCD_SignalEvent_t
 \param[in]   usr_param  User-defined value, acts as last parameter of cb_event
 \return      \ref execution_status
*/
int32_t
QSPI_LCD_Initialize(void *spi_dev, QSPI_LCD_SignalEvent_t cb_event, uint32_t usr_param);


/**
 \fn          int32_t SPI_Uninitialize (void)
 \brief       De-initialize SPI Interface.
 \param[in]   spi_dev  Pointer to SPI device instance
 \return      \ref execution_status
*/
int32_t
QSPI_LCD_Uninitialize(void *spi_dev);


/**
 \fn          int32_t SPI_Send (void *spi_dev, const void *data, uint32_t num)
 \brief       Start sending data to SPI transmitter.
 \param[in]   spi_dev  Pointer to SPI device instance
 \param[in]   data  Pointer to buffer with data to send to SPI transmitter
 \param[in]   num   Number of data items to send
 \return      \ref execution_status
*/
int32_t
QSPI_LCD_Send(void *spi_dev, const void *data, uint32_t num);


// Experimental API: SPI_Wait_Done
// Wait for SPI operation (Send/Receive/Transfer) done...
// NOTE: This API function can be called in TASK context ONLY!!
//      And it may cause dead lock when called in ISR context...
void QSPI_LCD_Wait_Done(void *spi_dev);


/**
 \fn          int32_t SPI_PowerControl (void *spi_dev, CSK_POWER_STATE state)
 \brief       Control SPI Interface Power.
 \param[in]   spi_dev  Pointer to SPI device instance
 \param[in]   state  Power state
 \return      \ref execution_status
*/
int32_t
QSPI_LCD_PowerControl(void *spi_dev, CSK_POWER_STATE state);


/**
 \fn          int32_t QSPI_LCD_Control (void *spi_dev, uint32_t control, uint32_t arg)
 \brief       Control QSPI Interface.
 \param[in]   spi_dev  Pointer to SPI device instance
 \param[in]   control  Operation
 \param[in]   arg      Argument of operation (optional)
 \return      common \ref execution_status and driver specific \ref spi_execution_status
*/
int32_t
QSPI_LCD_Control(void *spi_dev, uint32_t control, uint32_t arg);


/**
 \fn          uint32_t QSPI_LCD_SetLaneNum (void *spi_dev, em_CSK_QSPI_LCD_LANE_NUM lane_num)
 \brief       Set QSPI transfer single or dual or quad.
 \param[in]   spi_dev  Pointer to SPI device instance
 \param[in]   CSK_QSPI_LCD_LANE_NUM_QUAD/ CSK_QSPI_LCD_LANE_NUM_DUAL/ CSK_QSPI_LCD_LANE_NUM_QUAD
 \return      execution_status
*/
uint32_t
QSPI_LCD_SetLaneNum(void *spi_dev, em_CSK_QSPI_LCD_LANE_NUM lane_num);


/**
 \fn          uint32_t QSPI_LCD_SetDatLength(void *spi_dev, uint8_t length)
 \brief       Set QSPI transfer data length.
 \param[in]   spi_dev  Pointer to SPI device instance
 \param[in]   format   8/16/24/32
 \return      execution_status
*/
uint32_t
QSPI_LCD_SetDatLength(void *spi_dev, uint8_t length);


/**
 \fn          uint32_t QSPI_LCD_SetLcdMode(void *spi_dev, uint8_t mode)
 \brief       Set QSPI lcd mode.
 \param[in]   spi_dev  Pointer to SPI device instance
 \param[in]   mode   CSK_QSPI_LCD_MODE_NORMAL/ CSK_QSPI_LCD_MODE_888_TO_666/ CSK_QSPI_LCD_MODE_ARGB_TO_666/ CSK_QSPI_LCD_MODE_ARGB_TO_888
 \return      execution_status
*/
uint32_t
QSPI_LCD_SetLcdMode(void *spi_dev, em_CSK_QSPI_LCD_Mode mode);


/**
 \fn          CSK_QSPI_LCD_STATUS QSPI_LCD_GetStatus (void *spi_dev)
 \brief       Get QSPI status.
 \param[in]   spi_dev  Pointer to SPI device instance
 \param[out]  status  Pointer to CSK_QSPI_LCD_STATUS buffer
 \return      \ref execution_status
 */
int32_t
QSPI_LCD_GetStatus(void *spi_dev, CSK_QSPI_LCD_STATUS *status);


/**
 \fn          QSPI_LCD_SetDMASize(void *spi_dev, uint32_t num)
 \brief       Get DMA size
 \param[in]   spi_dev  Pointer to SPI device instance
 \param[in]   num:  DMA TX num
 \return      \ref execution_status
 */
int32_t
QSPI_LCD_SetDMASize(void *spi_dev, uint32_t num);


/**
 \fn          QSPI_LCD_DMAEnable(void *spi_dev)
 \brief       DMA enable
 \param[in]   spi_dev  Pointer to SPI device instance
 \return      \ref execution_status
 */
int32_t
QSPI_LCD_DMAEnable(void *spi_dev);


/**
 \fn          QSPI_LCD_DMADisable(void *spi_dev)
 \brief       DMA disable
 \param[in]   spi_dev  Pointer to SPI device instance
 \return      \ref execution_status
 */
int32_t
QSPI_LCD_DMADisable(void *spi_dev);


#endif /* __DRIVER_QSPI_LCD_H */

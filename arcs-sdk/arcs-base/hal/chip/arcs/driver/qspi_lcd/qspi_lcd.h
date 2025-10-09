/*
 * qspi_lcd.h
 *
 *  Created on: Oct 09, 2023
 *
 */

#ifndef __DRIVER_QSPI_LCD_INTERNAL_H
#define __DRIVER_QSPI_LCD_INTERNAL_H

#include "dma.h"
#include "arcs_ap.h"

#include "Driver_QSPI_LCD.h"

/* SPI transfer format register */
#define DATA_BITS(data_bits)          ((data_bits) - 1)

/* Max RD/WR transfer count each time, limited by SPI transfer control register */
#define MAX_TRANCNT                   8192

/* SPI transfer control register */
// RD/WR transfer count
#define RD_TRANCNT(num)               ((num) - 1)
#define WR_TRANCNT(num)               ((num) - 1)

/* SPI Slave Data Count Register (0x64) */
#define SLV_RDCNT(spi)               ((spi)->reg->REG_SLVDATACNT.bit.RCNT)
#define SLV_WRCNT(spi)               ((spi)->reg->REG_SLVDATACNT.bit.WCNT)
#define CLR_SLV_RDCNT(spi)           ((spi)->reg->REG_SLVDATACNT.bit.RCNT_CLR = 1) // bit[15] = 1 to clear RCnt
#define CLR_SLV_WRCNT(spi)           ((spi)->reg->REG_SLVDATACNT.bit.WCNT_CLR = 1) // bit[31] = 1 to clear WCnt
#define CLR_SLV_RDWRCNT(spi)         ((spi)->reg->REG_SLVDATACNT.all |= ((1 << 15) | (1 << 31))) // bit[15/31] = 1 to clear RCnt

// SPI transfer mode
#define SPI_TRANSMODE_WRnRD           (0x0)
#define SPI_TRANSMODE_WRONLY          (0x1)
#define SPI_TRANSMODE_RDONLY          (0x2)
#define SPI_TRANSMODE_WR_RD           (0x3)
#define SPI_TRANSMODE_RD_WR           (0x4)
#define SPI_TRANSMODE_WR_DMY_RD       (0x5)
#define SPI_TRANSMODE_RD_DMY_WR       (0x6)
#define SPI_TRANSMODE_NONEDATA        (0x7)
#define SPI_TRANSMODE_DMY_WR          (0x8)
#define SPI_TRANSMODE_DMY_RD          (0x9)

#define SPI_TRANSMODE_CMD_EN          (0x1 << 30)
#define SPI_TRANSMODE_ADDR_EN         (0x1 << 29)

/* SPI control register */
#define SPI_SPIRST                    (1UL << 0)
#define SPI_RXFIFORST                 (1UL << 1)
#define SPI_TXFIFORST                 (1UL << 2)
#define SPI_RXDMAEN                   (1UL << 3)
#define SPI_TXDMAEN                   (1UL << 4)

/* SPI interrupt status register */
/* SPI interrupt enable register */
#define SPI_INT_MASK                  (0x3F)
#define SPI_RXFIFOORINT               (1UL << 0)
#define SPI_TXFIFOURINT               (1UL << 1)
#define SPI_RXFIFOINT                 (1UL << 2)
#define SPI_TXFIFOINT                 (1UL << 3)
#define SPI_ENDINT                    (1UL << 4)
#define SPI_SLVCMD                    (1UL << 5)

/* SPI Slave Status Register */
#define SLVST_READY         (1UL) //Ready for data transaction
#define SLVST_OVER_RUN      (1UL) //Data overrun, W1C
#define SLVST_UNDER_RUN     (1UL) //Data underrun, W1C

// SPI flags (internal use)
#define SPI_FLAG_INITIALIZED          (1 << 7)     // SPI initialized
#define SPI_FLAG_POWERED              (1 << 6)     // SPI powered on
#define SPI_FLAG_CONFIGURED           (1 << 5)     // SPI configured

#define SPI_FLAG_8BIT_MERGE           (1 << 1)     // DATA_MERGE when Data_bits is 8
#define SPI_FLAG_NO_CS                (1 << 0)     //bit[0] = 1 indicates No CS pin connected

#define CSK_QSPI_LCD          ((QSPI_LCD_RegDef *)  QSPI_LCD_BASE)

#define CSK_QSPI_LCD_BUF      (QSPI_LCD_BASE + 0x2C)

//SPI TX/RX FIFO depth on ARCS
#define CSK_SPI_TXFIFO_DEPTH    16 //8 // words
#define CSK_SPI_RXFIFO_DEPTH    16 //8 // words

//current entries in SPI TX/RX FIFO
#define SPI_TXFIFO_ENTRIES(spi)     ((spi)->reg->REG_STATUS.bit.TXNUM)
#define SPI_RXFIFO_ENTRIES(spi)     ((spi)->reg->REG_STATUS.bit.RXNUM)

#define SPI_TXFIFO_FULL(spi)     ((spi)->reg->REG_STATUS.bit.TXFULL)
#define SPI_TXFIFO_EMPTY(spi)    ((spi)->reg->REG_STATUS.bit.TXEMPTY)
#define SPI_RXFIFO_FULL(spi)     ((spi)->reg->REG_STATUS.bit.RXFULL)
#define SPI_RXFIFO_EMPTY(spi)    ((spi)->reg->REG_STATUS.bit.RXEMPTY)

// SPI transfer operation
#define QSPI_LCD_SEND                      0x1
#define QSPI_LCD_RECEIVE                   0x2
#define QSPI_LCD_TRANSFER                  0x3

// SPI transfer information (Run-time)
typedef struct _SPI_TRANSFER_INFO
{
    union {
    void *rx_buf;         // pointer to in data buffer
    uint32_t rx_data;     // rx data when DMA_CH_CTLL_DST_FIX
    };
    const void *tx_buf;   // pointer to out data buffer
    uint32_t rx_cnt;      // number of data received (see TRANSFMT register bit[12:8])
    uint32_t tx_cnt;      // number of data sent (see TRANSFMT register bit[12:8])
    uint32_t req_rx_cnt;  // number of data requested to receive (see TRANSFMT register bit[12:8])
    uint32_t req_tx_cnt;  // number of data requested to send (see TRANSFMT register bit[12:8])
    uint8_t  dma_tx_done; // whether dma send is done, 1: done, 0: not done
    uint8_t  dma_rx_done; // whether dma receive is done, 1: done, 0: not done
    uint8_t  cur_op;      // current operation, send, receive or both(transfer)?
    uint8_t  rsvd;
} SPI_TRANSFER_INFO;

// SPI TX/RX IO & SPI Mode
typedef struct _SPI_TXRX_MODE {
    uint8_t mode :4;    // SPI Mode, Master or Slave?
    uint8_t tx_dma:1;   // support TX DMA?
    uint8_t tx_pio:1;   // support TX PIO?
    uint8_t rx_dma:1;   // support RX DMA?
    uint8_t rx_pio:1;   // support RX PIO?
} SPI_TXRX_MODE;

#define TX_DMA(txrx_mode)         (((txrx_mode) & CSK_QSPI_LCD_TXIO_DMA) == CSK_QSPI_LCD_TXIO_DMA)
#define TX_PIO(txrx_mode)         (((txrx_mode) & CSK_QSPI_LCD_TXIO_PIO) == CSK_QSPI_LCD_TXIO_PIO)
#define TX_DMA_ONLY(txrx_mode)    (((txrx_mode) & CSK_QSPI_LCD_TXIO_BOTH) == CSK_QSPI_LCD_TXIO_DMA)

#define RX_DMA(txrx_mode)         (((txrx_mode) & CSK_QSPI_LCD_RXIO_DMA) == CSK_QSPI_LCD_RXIO_DMA)
#define RX_PIO(txrx_mode)         (((txrx_mode) & CSK_QSPI_LCD_RXIO_PIO) == CSK_QSPI_LCD_RXIO_PIO)
#define RX_DMA_ONLY(txrx_mode)    (((txrx_mode) & CSK_QSPI_LCD_RXIO_BOTH) == CSK_QSPI_LCD_RXIO_DMA)


//spi_idx   SPI device index, 0, 1,...
//level     0 = Low, not 0 = High
typedef void (*QSPI_LCD_CS_SET_FUNC)(uint8_t spi_idx, uint8_t level);

// SPI information (Run-time)
typedef struct _SPI_INFO
{
    QSPI_LCD_SignalEvent_t cb_event;    // event callback
    uint32_t usr_param; // user parameter of event callback
    QSPI_LCD_CS_SET_FUNC cb_set_cs; // CS signal lower/raise callback

    uint8_t flags;            // SPI driver flags
    uint8_t command;          // SPI command
    uint8_t txrx_mode;        // SPI TX/RX & mode
    uint8_t data_bits;        // length of each data unit in bits (see DataLen of TRANSFMT)
    uint8_t dma_width_shift : 5;  // shift of dma width, 0: byte, 1: halfword, 2: word

    uint8_t tx_bsize_shift;   // burst size shift for TX DMA
    uint8_t rx_bsize_shift;   // burst size shift for RX DMA

    volatile CSK_QSPI_LCD_STATUS status;        // SPI status flags
    SPI_TRANSFER_INFO xfer;   // SPI transfer information

} SPI_INFO;

// SPI DMA
#define SPI_DMA_FLAG_CH_RSVD     0x1
typedef struct _SPI_DMA
{
    uint8_t reqsel;     // DMA request selection
    uint8_t channel;    // DMA channel (default or reserved)
    uint8_t ch_prio;    // DMA channel's priority
    uint8_t flag;       // channel is reserved if bit[0] = 1
    DMA_SignalEvent_t cb_event; // DMA event callback
    uint32_t usr_param; // user parameter of DMA callback
} SPI_DMA;

// SPI device structure definitions
typedef struct
{
    QSPI_LCD_RegDef *reg;  // pointer to SPI peripheral
    uint16_t txfifo_depth;  // TX FIFO depth, in words
    uint16_t rxfifo_depth;  // RX FIFO depth, in words
    uint16_t irq_num;      // SPI IRQ number
    uint16_t spi_idx;       // SPI index, can be 0, 1, ...
    //void (*irq_handler)(void);
    ISR irq_handler;

    SPI_DMA dma_tx;      // SPI TX DMA

    SPI_INFO *info;        // SPI run-time information
} SPI_DEV;

extern uint32_t eval_freq_divNM(uint16_t N_max, uint16_t M_max, uint32_t freq_in,
                        uint32_t freq_out_req, int32_t *freq_out_p);

#endif /* __DRIVER_QSPI_LCD_INTERNAL_H */

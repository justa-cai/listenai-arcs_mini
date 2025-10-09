/*
 * spi.h
 *
 *
 */

#ifndef __SPI_ARCS_H
#define __SPI_ARCS_H

#include "Driver_SPI.h"
#include "dma.h"
#include "arcs_ap.h"


/* SPI transfer format register */
#define SPI_CPHA                      (1UL << 0)
#define SPI_CPOL                      (1UL << 1)
#define SPI_SLAVE                     (1UL << 2)
#define SPI_LSB                       (1UL << 3)
#define SPI_MERGE                     (1UL << 7)
#define DATA_BITS_MASK                (0x1F << 8)
#define DATA_BITS(data_bits)          (((data_bits) - 1) << 8)

/* Max RD/WR transfer count each time, limited by SPI transfer control register */
//#define MAX_TRANCNT                   512 // no limit on arcs (32bits)!!

/* SPI transfer control register */
// RD/WR transfer count
//#define RD_TRANCNT(num)               (((num) - 1) << 0)    // bit[8:0] @ 0x20 TRANSCTRL, Reserved
//#define WR_TRANCNT(num)               (((num) - 1) << 12)   // bit[20:12] @ 0x20 TRANSCTRL, Reserved
#define RD_TRANCNT(num)                 ((num) - 1)     // bit[31:0] @ 0x1C RD_LEN [NEW]
#define WR_TRANCNT(num)                 ((num) - 1)     // bit[31:0] @ 0x18 WR_LEN [NEW]

#define DQ_IO_MASK                      (0x3 << 22)     // bit[23:22] @ 0x20 TRANSCTRL
#define DQ_IO_SINGLE                    (0x0 << 22)     //0x0: Regular (Single) mode
#define DQ_IO_DUAL                      (0x1 << 22)     //0x1: Dual I/O mode
#define DQ_IO_QUAD                      (0x2 << 22)     //0x2: Quad I/O mode

/* SPI Slave Data Count Register (0x64) */
// RD/WR transferred count, 10 bits in new version (originally 9 bits)
//#define SLV_RDCNT(spi)               ((spi)->reg->SLVDATACNT & 0x1FF)
//#define SLV_WRCNT(spi)               (((spi)->reg->SLVDATACNT >> 16) & 0x1FF)
#define SLV_RDCNT(spi)               ((spi)->reg->SLVDATACNT & 0x3FF)
#define SLV_WRCNT(spi)               (((spi)->reg->SLVDATACNT >> 16) & 0x3FF)
#define CLR_SLV_RDCNT(spi)           (spi)->reg->SLVDATACNT |= (1 << 15) // bit[15] = 1 to clear RCnt
#define CLR_SLV_WRCNT(spi)           (spi)->reg->SLVDATACNT |= (1 << 31) // bit[31] = 1 to clear WCnt
#define CLR_SLV_RDWRCNT(spi)         (spi)->reg->SLVDATACNT |= ((1 << 15) | (1 << 31)) // bit[15/31] = 1 to clear RCnt

// SPI transfer mode
#define SPI_TRANSMODE_WRnRD           (0x0 << 24)
#define SPI_TRANSMODE_WRONLY          (0x1 << 24)
#define SPI_TRANSMODE_RDONLY          (0x2 << 24)
#define SPI_TRANSMODE_WR_RD           (0x3 << 24)
#define SPI_TRANSMODE_RD_WR           (0x4 << 24)
#define SPI_TRANSMODE_WR_DMY_RD       (0x5 << 24)
#define SPI_TRANSMODE_RD_DMY_WR       (0x6 << 24)
#define SPI_TRANSMODE_NONEDATA        (0x7 << 24)
#define SPI_TRANSMODE_DMY_WR          (0x8 << 24)
#define SPI_TRANSMODE_DMY_RD          (0x9 << 24)

#define SPI_TRANSMODE_CMD_EN          (0x1 << 30)
#define SPI_TRANSMODE_ADDR_EN         (0x1 << 29)

/* SPI control register */
#define SPIRST                        (1UL << 0)
#define RXFIFORST                     (1UL << 1)
#define TXFIFORST                     (1UL << 2)
#define RXDMAEN                       (1UL << 3)
#define TXDMAEN                       (1UL << 4)
#define CS_REG_CFG_EN                 (1UL << 21) //[NEW]:
#define CS_FROM_REG                   (1UL << 22) //[NEW]:

#define THRES_MASK                    (0x1fUL)
#define RXTHRES_OFFSET                (8)
#define TXTHRES_OFFSET                (16)
#define RXTHRES_MASK                  (THRES_MASK << RXTHRES_OFFSET)
#define TXTHRES_MASK                  (THRES_MASK << TXTHRES_OFFSET)
#define RXTHRES(num)                  (((num) & THRES_MASK) << RXTHRES_OFFSET)
#define TXTHRES(num)                  (((num) & THRES_MASK) << TXTHRES_OFFSET)

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
#define SLVST_READY         (1UL << 16) //Ready for data transaction
#define SLVST_OVER_RUN      (1UL << 17) //Data overrun, W1C
#define SLVST_UNDER_RUN     (1UL << 18) //Data underrun, W1C

// SPI flags (internal use)
#define SPI_FLAG_INITIALIZED          (1 << 7)     // SPI initialized
#define SPI_FLAG_POWERED              (1 << 6)     // SPI powered on
#define SPI_FLAG_CONFIGURED           (1 << 5)     // SPI configured

#define SPI_FLAG_8BIT_MERGE           (1 << 1)     // DATA_MERGE when Data_bits is 8
#define SPI_FLAG_NO_CS                (1 << 0)     //bit[0] = 1 indicates No CS pin connected

/*****************************************************************************
 * SPI Registers Map on ARCS
 ****************************************************************************/
typedef struct {
    __I  unsigned int IDREV;                /* 0x00 ID and revision register */
         unsigned int RESERVED0[3];         /* 0x04 ~ 0x0c Reserved */
    __IO unsigned int TRANSFMT;             /* 0x10 SPI transfer format register */
    __IO unsigned int DIRECTIO;             /* 0x14 SPI direct IO control register */
         //unsigned int RESERVED1[2];         /* 0x18 ~ 0x1c Reserved */
    __IO unsigned int WR_LEN;               /* 0x18 SPI write length register */
    __IO unsigned int RD_LEN;               /* 0x1C SPI read length register */
    __IO unsigned int TRANSCTRL;            /* 0x20 SPI transfer control register */
    __IO unsigned int CMD;                  /* 0x24 SPI command register */
    __IO unsigned int ADDR;                 /* 0x28 SPI address register */
    __IO unsigned int DATA;                 /* 0x2c SPI data register */
    __IO unsigned int CTRL;                 /* 0x30 SPI control register */
    __I  unsigned int STATUS;               /* 0x34 SPI status register */
    __IO unsigned int INTREN;               /* 0x38 SPI interrupt enable register */
    __O  unsigned int INTRST;               /* 0x3c SPI interrupt status register */
    __IO unsigned int TIMING;               /* 0x40 SPI interface timing register */
         unsigned int RESERVED2[3];         /* 0x44 ~ 0x4c Reserved */
    __IO unsigned int MEMCTRL;              /* 0x50 SPI memory access control register */
         unsigned int RESERVED3[3];         /* 0x54 ~ 0x5c Reserved */
    __IO unsigned int SLVST;                /* 0x60 SPI slave status register */
    __IO unsigned int SLVDATACNT;           /* 0x64 SPI slave data count register */
         unsigned int RESERVED4[5];         /* 0x68 ~ 0x78 Reserved */
    __I  unsigned int CONFIG;               /* 0x7c Configuration register */
} CSK_SPI_RegDef;

#define CSK_SPI0             ((CSK_SPI_RegDef *)  SPI0_BASE)
#define CSK_SPI1             ((CSK_SPI_RegDef *)  SPI1_BASE)
#define CSK_SPI2             ((CSK_SPI_RegDef *)  SPI2_BASE)

//SPI TX/RX FIFO depth on ARCS
#define CSK_SPI_TXFIFO_DEPTH    16 //8 // words
#define CSK_SPI_RXFIFO_DEPTH    16 //8 // words

//current entries in SPI TX/RX FIFO
#define SPI_TXFIFO_ENTRIES(spi)     (((spi)->reg->STATUS >> 16) & 0x1f)
#define SPI_RXFIFO_ENTRIES(spi)     (((spi)->reg->STATUS >> 8) & 0x1f)

#define SPI_TXFIFO_FULL(spi)     (((spi)->reg->STATUS >> 23) & 0x1)
#define SPI_TXFIFO_EMPTY(spi)    (((spi)->reg->STATUS >> 22) & 0x1)
#define SPI_RXFIFO_FULL(spi)     (((spi)->reg->STATUS >> 15) & 0x1)
#define SPI_RXFIFO_EMPTY(spi)    (((spi)->reg->STATUS >> 14) & 0x1)


/*****************************************************************************
 * SPI clock in cmn_syscfg_reg.h.h
 ****************************************************************************/

#define SPI_CLK_CFG_MASK 0xFF800000

struct SYSCFG_REG_SPI_CLK_CFG_32BITS
{
    volatile uint32_t RESV_0_22                    : 22; // bit 0~21
    volatile uint32_t SEL_SPI_CLK                  : 1;  // bit 22
    volatile uint32_t ENA_SPI_CLK                  : 1;  // bit 23
    volatile uint32_t DIV_SPI_CLK_M                : 4;  // bit 24~27
    volatile uint32_t DIV_SPI_CLK_N                : 3;  // bit 28~30
    volatile uint32_t DIV_SPI_CLK_LD               : 1;  // bit 31
};

typedef union {
    volatile uint32_t                               all;
    struct SYSCFG_REG_SPI_CLK_CFG_32BITS            bit;
} SYSCFG_SPI_CLK_REG32;

#define SPI_CLK_SEL_MASK          (0x1 << 22) // bit[22]
#define SPI_CLK_SEL_XTAL          (0x0 << 22) // default
#define SPI_CLK_SEL_SYSPLL        (0x1 << 22)
#define SPI_CLK_ENA_MASK          (0x1 << 23) // bit[23]
#define SPI_CLK_DISABLE           (0x0 << 23)
#define SPI_CLK_ENABLE            (0x1 << 23)
#define SPI_CLK_M_MASK            (0xF << 24) // bit[27:24]
#define SPI_CLK_M(val)            (((val) & 0xF) << 24)
#define SPI_CLK_M_MAX             0xF
#define SPI_CLK_N_MASK            (0x7 << 28) // bit[30:28]
#define SPI_CLK_N(val)            (((val) & 0x7) << 28)
#define SPI_CLK_N_MAX             7
#define SPI_CLK_LD_MASK           (0x1 << 31) // bit[31]
#define SPI_CLK_LD                (0x1 << 31)

#define CSK_SPI0_CLK             ((SYSCFG_SPI_CLK_REG32 *)(CMN_SYS_BASE + 0x14))
#define CSK_SPI1_CLK             ((SYSCFG_SPI_CLK_REG32 *)(CMN_SYS_BASE + 0x18))
#define CSK_SPI2_CLK             ((SYSCFG_SPI_CLK_REG32 *)(CMN_SYS_BASE + 0x1C))
#define CSK_SPI_RESET_ADDR       (CMN_SYS_BASE + 0x0C)
#define CSK_SPI_RESET(i)         *(volatile uint32_t*)CSK_SPI_RESET_ADDR |= 1 << (3 + i)


// SPI transfer operation
#define SPI_SEND                      0x1
#define SPI_RECEIVE                   0x2
#define SPI_TRANSFER                  0x3

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

#define TX_DMA(txrx_mode)         (((txrx_mode) & CSK_SPI_TXIO_DMA) == CSK_SPI_TXIO_DMA)
#define TX_PIO(txrx_mode)         (((txrx_mode) & CSK_SPI_TXIO_PIO) == CSK_SPI_TXIO_PIO)
#define TX_DMA_ONLY(txrx_mode)    (((txrx_mode) & CSK_SPI_TXIO_BOTH) == CSK_SPI_TXIO_DMA)

#define RX_DMA(txrx_mode)         (((txrx_mode) & CSK_SPI_RXIO_DMA) == CSK_SPI_RXIO_DMA)
#define RX_PIO(txrx_mode)         (((txrx_mode) & CSK_SPI_RXIO_PIO) == CSK_SPI_RXIO_PIO)
#define RX_DMA_ONLY(txrx_mode)    (((txrx_mode) & CSK_SPI_RXIO_BOTH) == CSK_SPI_RXIO_DMA)


// SPI information (Run-time)
typedef struct _SPI_INFO
{
    CSK_SPI_SignalEvent_t cb_event;    // event callback
    uint32_t usr_param; // user parameter of event callback
    SPI_CS_SET_FUNC cb_set_cs; // CS signal lower/raise callback

    uint8_t flags;            // SPI driver flags
    uint8_t txrx_mode;        // SPI TX/RX & mode
    uint8_t data_bits;        // length of each data unit in bits (see DataLen of TRANSFMT)
    uint8_t dma_width_shift : 5;  // shift of dma width, 0: byte, 1: halfword, 2: word
    uint8_t tx_dma_no_syncache : 1;  // don't sync cache for TX DMA
    uint8_t rx_dma_no_syncache : 1;  // don't sync cache for RX DMA
    uint8_t rxdma_wait_odds : 1;  // wait for odd data when RX DMA is done

    uint8_t tx_bsize_shift;   // burst size shift for TX DMA
    uint8_t rx_bsize_shift;   // burst size shift for RX DMA

    uint8_t tx_dyn_dma_ch;    // dynamically allocated DMA channel for transmit
    uint8_t rx_dyn_dma_ch;    // dynamically allocated DMA channel for receive

    uint32_t rx_dma_control;  // RX DMA Control register value, for fast call.
    uint32_t rx_dma_config_low;  // RX DMA Config. register's low 32bit value, for fast call.
    uint32_t rx_dma_config_high;  // RX DMA Config. register's high 32bit value, for fast call.
    volatile CSK_SPI_STATUS status;        // SPI status flags
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
    SPI_RegDef *reg0;
    CSK_SPI_RegDef *reg;  // pointer to SPI peripheral
    SYSCFG_SPI_CLK_REG32 *ext_clk; // pointer to external SPI clock config.
    uint16_t txfifo_depth;  // TX FIFO depth, in words
    uint16_t rxfifo_depth;  // RX FIFO depth, in words
    uint16_t irq_num;      // SPI IRQ number
    uint16_t spi_idx;       // SPI index, can be 0, 1, ...
    //void (*irq_handler)(void);
    ISR irq_handler;

    SPI_DMA dma_tx;      // SPI TX DMA
    SPI_DMA dma_rx;      // SPI RX DMA

    SPI_INFO *info;        // SPI run-time information
} SPI_DEV;

extern uint32_t eval_freq_divNM(uint16_t N_max, uint16_t M_max, uint32_t freq_in,
                        uint32_t freq_out_req, int32_t *freq_out_p);

extern uint32_t eval_freq_divNM2(uint16_t N_max, uint16_t M_max, uint32_t khz_in,
                        uint32_t khz_out_max, int32_t *khz_out_p);

#endif /* __SPI_ARCS_H */

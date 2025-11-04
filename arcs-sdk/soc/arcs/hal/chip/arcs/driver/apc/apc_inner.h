/*
 * apc_inner.h
 *
 *
 */

#ifndef __APC_INNER_ARCS_H
#define __APC_INNER_ARCS_H

#include <stdint.h>
#include "arcs_ap.h"
#include "apc.h"
#include "apc_reg.h" // APC Register Map

//====================== Register-related Macros & Functions =====================

// channel FIFO depth (RX = IN, TX = OUT)
#define APC_IN_FIFO_DEPTH_DEF   16
#define APC_OUT_FIFO_DEPTH_DEF  16
#define APC_RX_FIFO_DEPTH_DEF   APC_IN_FIFO_DEPTH_DEF
#define APC_TX_FIFO_DEPTH_DEF   APC_OUT_FIFO_DEPTH_DEF

// channel FIFO data register address
#define APC_RX_CH0L_DATA_ADDR   (APC_BASE + 0x104)
#define APC_RX_CH0R_DATA_ADDR   (APC_BASE + 0x108)
#define APC_RX_CH1L_DATA_ADDR   (APC_BASE + 0x10C)
#define APC_RX_CH1R_DATA_ADDR   (APC_BASE + 0x110)

#define APC_TX_CH0L_DATA_ADDR   (APC_BASE + 0x0F4)
#define APC_TX_CH0R_DATA_ADDR   (APC_BASE + 0x0F8)
#define APC_TX_CH1L_DATA_ADDR   (APC_BASE + 0x0FC)
#define APC_TX_CH1R_DATA_ADDR   (APC_BASE + 0x100)

// channel FIFO CFG register address
#define APC_RX_CH0_CFG_ADDR     (APC_BASE + 0x014)
#define APC_RX_CH1_CFG_ADDR     (APC_BASE + 0x018)
#define APC_TX_CH0_CFG_ADDR     (APC_BASE + 0x00C)
#define APC_TX_CH1_CFG_ADDR     (APC_BASE + 0x010)

// EQ coefficient base address
#define APC_TX_CH0_EQCOEF_BASE      (APC_BASE + 0x1C)

//-------------------------------------------------------------------------------
// APC_CFG Register

#define APC_ENABLE_BIT                 APC_APC_CFG_APC_ENABLE_Msk           // 0x1
#define APC_RX_PATH_RESET_BIT          APC_APC_CFG_APC_RX_PATH_RESET_Msk    // 0x2
#define APC_TX_PATH_RESET_BIT          APC_APC_CFG_APC_TX_PATH_RESET_Msk    // 0x4
#define APC_AUTO_CLK_GATING_BIT        APC_APC_CFG_AUTO_CLK_GATING_Msk      // 0x8

//-------------------------------------------------------------------------------
// APC INTR-related Registers

#define APC_INTR_FIFO_EMPTY         (0x1 << 0)
#define APC_INTR_FIFO_FULL          (0x1 << 1)
#define APC_INTR_FIFO_UNDERFLOW     (0x1 << 2)
#define APC_INTR_FIFO_OVERFLOW      (0x1 << 3)
#define APC_INTR_FIFO_DMA_REQ       (0x1 << 4)

#define APC_INTR_FIFO_UNDERRUN      APC_INTR_FIFO_UNDERFLOW
#define APC_INTR_FIFO_OVERRUN       APC_INTR_FIFO_OVERFLOW
#define APC_INTR_READY_TO_XFER      APC_INTR_FIFO_DMA_REQ

#define APC_FIFO_INTR_CNT         5
#define APC_FIFO_INTR_MASK      ( APC_INTR_FIFO_EMPTY | APC_INTR_FIFO_FULL | \
                                  APC_INTR_FIFO_UNDERFLOW | APC_INTR_FIFO_OVERFLOW | \
                                  APC_INTR_FIFO_DMA_REQ )
#define APC_FIFO_INTR_ALL       APC_FIFO_INTR_MASK
#define APC_CH_INTR_MASK        APC_FIFO_INTR_MASK

// Following bits are located on APC_INTR_TX_MSK/CLR/ISR/IRSR registers, bit[10:11]
#define APC_INTR_TX_FIFO_VLD(dch)   (0x1 << ((dch - APC_DCH_IN_MAX) * 11 - 1)) // dch: 2 for TX0, 3 for TX1
#define APC_INTR_I2S0_ERR           (0x1 << 23)
#define APC_INTR_I2S1_ERR           (0x1 << 22)

//-------------------------------------------------------------------------------
// CP DMA (DW DMAC) handshake ID definitions for APC channels
#if (ARCS_VER == ARCS_B0_SOC)
#define DMA_HSID_APC_IN_CH0     DMA_HSID_APC_RX0
#define DMA_HSID_APC_IN_CH1     DMA_HSID_APC_RX1
#define DMA_HSID_APC_IN_CH2     DMA_HSID1_APC_RX2 // ap_dma_hs_sel_x = 1
#define DMA_HSID_APC_IN_CH3     DMA_HSID1_APC_RX3 // ap_dma_hs_sel_x = 1
#define DMA_HSID_APC_OUT_CH0    DMA_HSID_APC_TX0
#define DMA_HSID_APC_OUT_CH1    DMA_HSID_APC_TX1
#define DMA_HSID_APC_OUT_CH2    DMA_HSID1_APC_TX2 // ap_dma_hs_sel_x = 1
#define DMA_HSID_APC_OUT_CH3    DMA_HSID1_APC_TX3 // ap_dma_hs_sel_x = 1

#define DMA_HSSEL_APC_IN_CH2    1
#define DMA_HSSEL_APC_IN_CH3    1
#define DMA_HSSEL_APC_OUT_CH2   1
#define DMA_HSSEL_APC_OUT_CH3   1

#elif (ARCS_VER > ARCS_B0_SOC)
#define DMA_HSID_APC_IN_CH0     11
#define DMA_HSID_APC_IN_CH1     12
#define DMA_HSID_APC_IN_CH2     13
#define DMA_HSID_APC_IN_CH3     15
#define DMA_HSID_APC_OUT_CH0    8
#define DMA_HSID_APC_OUT_CH1    9
#define DMA_HSID_APC_OUT_CH2    10
#define DMA_HSID_APC_OUT_CH3    14

#define DMA_HSSEL_APC_IN_CH2    0
#define DMA_HSSEL_APC_IN_CH3    0
#define DMA_HSSEL_APC_OUT_CH2   0
#define DMA_HSSEL_APC_OUT_CH3   0

#endif // ARCS_VER

//-------------------------------------------------------------------------------
// APC_TX/RX_CHx_CFG Register (common part)

// APC TX/RX channel SRC/DST
#define RX_CH0_SRC_ADC01    0
#define RX_CH0_SRC_I2S0     1
#if (ARCS_VER < ARCS_D0_SOC)
#define RX_CH0_SRC_TX_CH0_LPBK  2
#define RX_CH0_SRC_ECHO0        RX_CH0_SRC_TX_CH0_LPBK
#else // D0
#define RX_CH0_SRC_TX_CH1_LPBK  2
#define RX_CH0_SRC_ECHO1        RX_CH0_SRC_TX_CH1_LPBK
#endif

//#define RX_CH1_SRC_ADC23    0
#define RX_CH1_SRC_I2S1         0
#if (ARCS_VER < ARCS_D0_SOC)
#define RX_CH1_SRC_TX_CH1_LPBK  1
#define RX_CH1_SRC_ECHO1        RX_CH1_SRC_TX_CH1_LPBK
#else // D0
#define RX_CH1_SRC_TX_CH0_LPBK  1
#define RX_CH1_SRC_ECHO0        RX_CH1_SRC_TX_CH0_LPBK
#endif

#define TX_CH0_DST_DAC      0
#define TX_CH0_DST_I2S0     1

// channel enable
#define APC_CH_DIS      0
#define APC_CH_EN       1

// dual_channel stereo mode
#define APC_DCH_MONO_MODE           0
#define APC_DCH_STEREO_MODE         1
#define APC_DCH_MIXED_MODE          APC_DCH_STEREO_MODE
#define APC_DCH_SEPARATE_MODE       APC_DCH_MONO_MODE


#if (ARCS_VER >= ARCS_D0_SOC) // full mask

//dual_channel configuration register bits (common part)
typedef struct _APC_DCH_CFG_REG_BITS {

    volatile uint32_t CH_L_EN                   : 1; // bit 0~0
    volatile uint32_t CH_L_MODE                 : 2; // bit 1~2
    volatile uint32_t CH_L_FIFO_FLUSH           : 1; // bit 3~3
    volatile uint32_t CH_L_FIFO_CNT             : 5; // bit 4~8
    volatile uint32_t RESV_9_15                 : 7; // bit 9~15

    volatile uint32_t CH_R_EN                   : 1; // bit 16~16
    volatile uint32_t CH_R_MODE                 : 2; // bit 17~18
    volatile uint32_t CH_R_FIFO_FLUSH           : 1; // bit 19~19
    volatile uint32_t CH_R_FIFO_CNT             : 5; // bit 20~24

    volatile uint32_t DCH_STEREO_MODE           : 1; // bit 25~25
    volatile uint32_t DCH_DMA_THD_SEL           : 2; // bit 26~27
    volatile uint32_t RESV_28_31                : 4; // bit 28~31

} APC_DCH_CFG_REG_BITS;

//dual_channel configuration register (common part)
typedef union _APC_DCH_CFG_REG {
    volatile uint32_t                     all;
    struct _APC_DCH_CFG_REG_BITS          bit;
} APC_DCH_CFG_REG;

#else // arcs_b0 / arcs_c0

// APC channel configuration register bits (common part)
typedef struct _APC_CH_CFG_REG_BITS {
    volatile uint32_t CH_EN                   : 1; // bit 0~0
    volatile uint32_t CH_MODE                 : 2; // bit 1~2
    volatile uint32_t CH_FIFO_FLUSH           : 1; // bit 3~3
    volatile uint32_t CH_FIFO_CNT             : 5; // bit 4~8
    volatile uint32_t RESERVED                : 23; // bit 9~31
} APC_CH_CFG_REG_BITS;

// APC channel configuration register (common part)
typedef union _APC_CH_CFG_REG {
    volatile uint32_t                    all;
    struct _APC_CH_CFG_REG_BITS          bit;
} APC_CH_CFG_REG;

#endif // ARCS_VER


/*
// APC channel information (fixed by IC design)
typedef struct _APC_CH_FIXED
{
    uint16_t ch_idx     : 7; // APC channel index: IN 0~3, IN/Echo 4~5, OUT 6~7
    uint16_t ch_dir     : 1; // APC channel direction: 0=OUT, 1=IN
    uint16_t dma_hsid   : 8; // DMA request/handshaking ID of APC channel

    uint16_t fifo_depth; // FIFO depth (in WORD) of APC channel
    uint32_t data_addr; // address of data register to access FIFO

    //TODO: Add more fields for APC channel
} APC_CH_FIXED;
*/

// APC channel CFG bits
#define CFG_CH_EN_POS       0
#define CFG_CH_MODE_POS     1
#define CFG_FIFO_FLU_POS    3
#define CFG_FIFO_CNT_POS    4
#define CFG_CH_EN_MASK      (0x1) // 1b
#define CFG_CH_MODE_MASK    (0x3) // 2b
#define CFG_FIFO_FLU_MASK   (0x1) // 1b
#define CFG_FIFO_CNT_MASK   (0x1F) // 5b

#define APC_CH_CFG_MASK     0x1FF // 9b

// APC channel information (fixed by IC design)
typedef struct _APC_CH_FIXED
{
    uint32_t ch_idx         : 4; // APC channel index: IN 0~3, OUT 4~7
    uint32_t ch_dir         : 1; // APC channel direction: 0=OUT, 1=IN

    // APC channel CFG bits position @ APC_TX/RX_CHx_CFG, CFG bits includes 9 bits:
    // fifo_cnt(5b), fifo_flush(1b), mode(2b), en(1b)
    uint32_t cfg_bits_pos   : 5;

    // APC channel Interrupt-related bits position @ APC_INTR_TX/RX_MSK/CLR/IRSR/ISR
    // INTR bits includes 5 bits: fifo_emp, fifo_ful, fifo_unflow, fifo_ovflow, dma_req
    uint32_t intr_bits_pos  : 5;

    uint32_t dma_hsid       : 5; // DMA request/handshaking ID of APC channel
    uint32_t dma_hssel      : 2; // DMA request/handshaking group of APC channel

    uint32_t fifo_depth     : 10; // FIFO depth (in WORD) of APC channel
    uint32_t data_addr; // address of data register to access FIFO

    //TODO: Add more fields for APC channel
} APC_CH_FIXED;

// APC dual_channel CFG bits
#define CFG_DCH_STMODE_POS      0
#define CFG_DCH_DMA_THD_POS     1
#define CFG_DCH_STMODE_MASK0     (0x1) // 1b
#define CFG_DCH_DMA_THD_MASK0    (0x3) // 2b
#define APC_DCH_CFG_MASK        0x7 // 3b

// APC dual_channel information (fixed by IC design)
typedef struct _APC_DCH_FIXED
{
    // APC dual_channel CFG bits position @ APC_TX/RX_CHx_CFG,
    // CFG bits includes 3 bits: stereo_mode(1b), dma_thd_sel(2b)
    uint8_t dcfg_bits_pos : 5;
    uint8_t reserved : 3;

    //TODO: Add more fields for APC channel
} APC_DCH_FIXED;

// APC dual_channel information (Run-time)
typedef struct _APC_DCH_INFO
{
    CSK_APC_SignalEvent_t cb_event;    // event callback
    uint32_t usr_param; // user parameter of event callback

    uint8_t intf_type; // see definitions of APC_INTF_TYPE
    uint8_t intf_idx; // instance index of intf_type, i.e. I2S 0, 1, 2...

    // L/R channel select - left, right, or both?
    uint8_t ch_sel : 2; // 01b = left, 10b = right, 11b = both
    // stereo(1) indicates that L/R channel data are interlaced, as LRLRLR...
    // mix_mode should be stereo(1) in I2S TDM mode according to IP design
    uint8_t mix_mode : 1; // 0 = mono (separate), 1 = stereo (mixed)
    uint8_t mix_target : 1; // if mixed, data path is via left (0) or right (1) channel
    // channel mode indicates sample bit width and decides data sequence when accessed
    // i.e. 1. mix_mode is mono (L/R channel data are accessed respectively)
    //  0: 16-bit {L1, L0}, 1: 24-bit{8'd0, L0}, 2: 32-bit {L0}, 3: 24-bit{L0, 8'd0}
    // 2. mix_mode is stereo (L/R channel data are accessed mixed)
    //  data sequence is always as L,R,L,R,L,R...
    uint8_t ch_mode : 2; // 0 = 16-bit, 1 = 24-bit LOW, 2 = 32-bit, 3 = 24-bit HIGH
    uint8_t reserved : 1;
    uint8_t setup_done : 1; // channel setup has been done?

    // option values: 16, 24, 32, it decides channel mode setting!
    uint8_t samp_bits; // length of each sample data in bits

    uint8_t dma_width_bits; // bits of dma width, 0: byte, 1: halfword, 2: word
    uint8_t dma_bsize_bits; // bits of dma burst size
    uint8_t dma_ch_lr[2]; // dynamically allocated DMA channel for APC channel read/write

#if SUPPORT_APC_PIO
    uint32_t *sambuf_lr[2]; // samples transfer buffer for TX or RX
    uint32_t samcnt_lr[2]; // samples count requested to TX or RX
#endif

#define RT_FLAG_NSYNCA   (0x1 << 0)
#define RT_FLAG_USE_PIO  (0x1 << 1)
    uint8_t rt_flag_lr[2]; //bit[0]=1 means NOT_SYNC_CACHE

    uint8_t busy_lr[2]; // DMA transfer is ongoing (busy) or not?
    uint32_t samps_lr[2]; // count of samples actually transfered

    // DMA Scatter-Gather settings, generally used in multiple channels read operations
    // xxx_lr[0] for Left channel or whole dual_channel, xxx_lr[1] for Right channel
    uint32_t scat_gath_lr[2]; // including SG Interval & Count
    uint8_t sg_bytes_lr[2];  // bytes of 'desired' (NOT original) sample data (e.g. 32bit, 24bit(high)=>16bit)
    uint8_t sg_offset_lr[2]; // buffer offset of sample data

//    APC_DCH_CFG_REG * cfg_reg_p; // dual_channel configuration register
    uint32_t *cfg_reg_p; // dual_channel configuration register pointer
    const APC_CH_FIXED * fixed_lr[2]; // L, R channels' fixed configuration

    //TODO: Add more fields for APC channel
} APC_DCH_INFO;

// APC device structure definitions
typedef struct
{
    CSK_APC_RegDef *reg;  // pointer to APC peripheral
    //uint32_t irq_num; // APC IRQ number
    //ISR irq_handler; // APC Interrupt handler

    APC_DCH_INFO dch_array[APC_DCH_COUNT]; // APC dual_channel array
} APC_DEV;


//-------------------------------------------------------------------------------
#define APC_RX_INT_STATUS()                     (CSK_APC->REG_APC_INTR_RX_ISR.all)
#define APC_TX_INT_STATUS()                     (CSK_APC->REG_APC_INTR_TX_ISR.all)
#define APC_RX_INT_RAW_STATUS()                 (CSK_APC->REG_APC_INTR_RX_IRSR.all)
#define APC_TX_INT_RAW_STATUS()                 (CSK_APC->REG_APC_INTR_TX_IRSR.all)
//#define APC_CH_INT_STATUS(rx_sta, tx_sta, ch)   ((ch >= APC_CH_IN_COUNT ? \
//                                                 tx_sta >> ((ch - APC_CH_IN_COUNT) * APC_FIFO_INTR_CNT) : \
//                                                 rx_sta >> (ch * APC_FIFO_INTR_CNT)) & APC_FIFO_INTR_MASK)
#define IS_SET_READY_TO_XFER(ch_sta)            (ch_sta & APC_INTR_READY_TO_XFER)
#define IS_SET_RX_OVERRUN(ch_sta)               (ch_sta & APC_INTR_FIFO_OVERRUN)
#define IS_SET_TX_UNDERRUN(ch_sta)              (ch_sta & APC_INTR_FIFO_UNDERRUN)
#define IS_SET_RX_FULL(ch_sta)                  (ch_sta & APC_INTR_FIFO_FULL)
#define IS_SET_TX_EMPTY(ch_sta)                 (ch_sta & APC_INTR_FIFO_EMPTY)

#define IS_SET_TX_FIFO_VLD(tx_sta, dch)         (tx_sta & APC_INTR_TX_FIFO_VLD(dch))
#define IS_SET_I2S0_ERR(tx_sta)                  (tx_sta & APC_INTR_I2S0_ERR)
#define IS_SET_I2S1_ERR(tx_sta)                  (tx_sta & APC_INTR_I2S1_ERR)

#endif /* __APC_INNER_ARCS_H */

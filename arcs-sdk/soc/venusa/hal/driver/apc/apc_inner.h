/*
 * apc_inner.h
 *
 */

#ifndef __APC_INNER_VENUSA_H
#define __APC_INNER_VENUSA_H

#include <stdint.h>
#include "apc.h"
#include "apc_reg.h" // APC Register Map

//====================== Register-related Macros & Functions =====================

// channel FIFO depth (RX = IN, TX = OUT)
#define APC_IN_FIFO_DEPTH_DEF   16
#define APC_OUT_FIFO_DEPTH_DEF  16
#define APC_RX_FIFO_DEPTH_DEF   APC_IN_FIFO_DEPTH_DEF
#define APC_TX_FIFO_DEPTH_DEF   APC_OUT_FIFO_DEPTH_DEF

// channel FIFO data register address
#define APC_RX_CH0L_DATA_ADDR   (APC_BASE + 0x48)
#define APC_RX_CH0R_DATA_ADDR   (APC_BASE + 0x4C)
#define APC_RX_CH1L_DATA_ADDR   (APC_BASE + 0x50)
#define APC_RX_CH1R_DATA_ADDR   (APC_BASE + 0x54)
#define APC_RX_CH2L_DATA_ADDR   (APC_BASE + 0x58)
#define APC_RX_CH2R_DATA_ADDR   (APC_BASE + 0x5C)

#define APC_TX_CH0L_DATA_ADDR   (APC_BASE + 0x34)
//#define APC_TX_CH0R_DATA_ADDR   (APC_BASE + 0x38)
#define APC_TX_CH1L_DATA_ADDR   (APC_BASE + 0x38)
#define APC_TX_CH1R_DATA_ADDR   (APC_BASE + 0x3C)
#define APC_TX_CH2L_DATA_ADDR   (APC_BASE + 0x40)
#define APC_TX_CH2R_DATA_ADDR   (APC_BASE + 0x44)

// channel CFG register address
#define APC_RX_CH0_CFG_ADDR     (APC_BASE + 0x18)
#define APC_RX_CH1_CFG_ADDR     (APC_BASE + 0x1C)
#define APC_RX_CH2_CFG_ADDR     (APC_BASE + 0x20)
#define APC_TX_CH0_CFG_ADDR     (APC_BASE + 0x0C)
#define APC_TX_CH1_CFG_ADDR     (APC_BASE + 0x10)
#define APC_TX_CH2_CFG_ADDR     (APC_BASE + 0x14)

// AP interrupt mask/clear/raw status/status register base (via apc_0)
#define APC_INTR_AP_MSK_BASE    (APC_BASE + 0x060)
#define APC_INTR_AP_CLR_BASE    (APC_BASE + 0x078)
#define APC_INTR_AP_IRSR_BASE   (APC_BASE + 0x090)
#define APC_INTR_AP_ISR_BASE    (APC_BASE + 0x0A8)

// CP interrupt mask/clear/raw status/status register base (via apc_1)
#define APC_INTR_CP_MSK_BASE    (APC_BASE + 0x0C0)
#define APC_INTR_CP_CLR_BASE    (APC_BASE + 0x0D8)
#define APC_INTR_CP_IRSR_BASE   (APC_BASE + 0x0F0)
#define APC_INTR_CP_ISR_BASE    (APC_BASE + 0x108)

// I2S Timeout configuration
#define APC_I2S_TIMEOUT_CFG_BASE        (APC_BASE + 0x120)
#define APC_I2S_TIMEOUT_CFG_ADDR(idx)   (APC_BASE + 0x120 + ((idx) << 2))
#define APC_I2S_TIMEOUT_CFG_MASK        0xFFF //12bit
#define APC_I2S_TIMEOUT_CFG_MAX         APC_I2S_TIMEOUT_CFG_MASK
 // LRCK sampling frequency is fixed at 2.4MHz
#define APC_I2S_TIMEOUT_CFG_FREQ        2400000 // 2.4MHz
#define APC_I2S_TIMEOUT_CFG_DEF         2400    // 1ms time-span when reset
#define APC_I2S_TIMEOUT_CFG_VAL         300 //APC_I2S_TIMEOUT_CFG_DEF //1 //


// EQ coefficient base address //FIXME: EQ is NOT supported on VENUSA?
//#define APC_TX_CH0_EQCOEF_BASE      (APC_BASE + 0x1C)

//-------------------------------------------------------------------------------

// APC_CFG Register
#define APC_ENABLE_BIT                 APC_APC_CFG_APC_ENABLE_Msk           // 0x1
#define APC_AUTO_CLK_GATING_BIT        APC_APC_CFG_AUTO_CLK_GATING_Msk      // 0x2

// APC_RX_PATH_RESET & APC_TX_PATH_RESET Register
#define APC_RX_PATH_RESET_BIT          APC_APC_RX_PATH_RESET_APC_RX_PATH_RESET_Msk    // 0x1
#define APC_TX_PATH_RESET_BIT          APC_APC_TX_PATH_RESET_APC_TX_PATH_RESET_Msk    // 0x1

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

// Following bits are located on APC_INTR_TX/_RX_MSK/CLR/ISR/IRSR registers
//#define APC_INTR_TX_FIFO_VLD(dch)   (0x1 << ((dch - APC_DCH_IN_MAX) * 11 - 1)) // dch: 2 for TX0, 3 for TX1
#define APC_INTR_TX_FIFO_VLD    (0x1 << 10) // on TX (TX0 & TX1 & TX2)
//#define APC_INTR_I2S0_ERR       (0x1 << 23)
//#define APC_INTR_I2S1_ERR       (0x1 << 22)
#define APC_INTR_I2S_TIMEOUT    (0x1 << 10) // on RX1 (for I2S0) & RX2 (for I2S1)
#define APC_INTR_I2S_ERR        (0x1 << 11) // on RX1 (for I2S0) & RX2 (for I2S1)
#define APC_INTR_I2S_ERR_LRCK   (0x1 << 12) // on ISR of RX1 (for I2S0) & RX2 (for I2S1)
//-------------------------------------------------------------------------------
/*
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
*/

// CMNDMA handshake ID definitions for APC channels
#define CMNDMA_HSID_APC_IN0_L        HAL_CMN_DMA_SEL1_HSID_12_APC_DMA_RX_0
#define CMNDMA_HSID_APC_IN0_R        HAL_CMN_DMA_SEL1_HSID_13_APC_DMA_RX_1
#define CMNDMA_HSID_APC_IN1_LR       HAL_CMN_DMA_SEL1_HSID_14_APC_DMA_RX_2
#define CMNDMA_HSID_APC_IN2_LR       HAL_CMN_DMA_SEL1_HSID_15_APC_DMA_RX_3
#define CMNDMA_HSID_APC_OUT0_L       HAL_CMN_DMA_SEL1_HSID_8_APC_DMA_TX_0
#define CMNDMA_HSID_APC_OUT1_LR      HAL_CMN_DMA_SEL1_HSID_9_APC_DMA_TX_1
#define CMNDMA_HSID_APC_OUT2_LR      HAL_CMN_DMA_SEL1_HSID_10_APC_DMA_TX_2
#define CMNDMA_HSSEL_APC             0x1

// GPDMA2D handshake ID definitions for APC channels
#define GPDMA2D_HSID_APC_IN0_L       HAL_GP_DMA_SEL0_HSID12_APC_DMA_RX_0
#define GPDMA2D_HSID_APC_IN0_R       HAL_GP_DMA_SEL0_HSID13_APC_DMA_RX_1
#define GPDMA2D_HSID_APC_IN1_LR      HAL_GP_DMA_SEL0_HSID14_APC_DMA_RX_2
#define GPDMA2D_HSID_APC_IN2_LR      HAL_GP_DMA_SEL0_HSID15_APC_DMA_RX_3
#define GPDMA2D_HSID_APC_OUT0_L      HAL_GP_DMA_SEL0_HSID8_APC_DMA_TX_0
#define GPDMA2D_HSID_APC_OUT1_LR     HAL_GP_DMA_SEL0_HSID9_APC_DMA_TX_1
#define GPDMA2D_HSID_APC_OUT2_LR     HAL_GP_DMA_SEL0_HSID10_APC_DMA_TX_2
#define GPDMA2D_HSSEL_APC            0x0

//-------------------------------------------------------------------------------
// APC_TX/RX_CHx_CFG Register (common part)

// APC TX/RX channel SRC/DST
#define RX_CH1_SRC_I2S0             0
#define RX_CH1_SRC_TX_CH0L_LPBK     1
#define RX_CH1_SRC_TX_CH2LR_LPBK    2
#define RX_CH1_SRC_ECHO_DAC         RX_CH1_SRC_TX_CH0L_LPBK
#define RX_CH1_SRC_ECHO_I2S1OUT     RX_CH1_SRC_TX_CH2LR_LPBK

// channel enable
#define APC_CH_DIS      0
#define APC_CH_EN       1

// dual_channel stereo mode
#define APC_DCH_MONO_MODE           0
#define APC_DCH_STEREO_MODE         1
#define APC_DCH_MIXED_MODE          APC_DCH_STEREO_MODE
#define APC_DCH_SEPARATE_MODE       APC_DCH_MONO_MODE

//dual_channel configuration register bits (common part)
typedef struct _APC_DCH_CFG_REG_BITS {

    volatile uint32_t CH_L_EN                   : 1; // bit 0~0
    volatile uint32_t CH_L_MODE                 : 2; // bit 1~2
    volatile uint32_t CH_L_FIFO_FLUSH           : 1; // bit 3~3
    volatile uint32_t CH_L_FIFO_CNT             : 5; // bit 4~8
    volatile uint32_t RESV_9_15                 : 7; // bit 9~15

    //NOTE: NO RIGHT CH FOR TX0!!
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
    uint32_t ch_idx         : 4; // APC channel index: IN 0~5, OUT 6~11 (NO 7)
    uint32_t ch_dir         : 1; // APC channel direction: 0=OUT, 1=IN

    // APC channel CFG bits position @ APC_TX/RX_CHx_CFG, CFG bits includes 9 bits:
    // fifo_cnt(5b), fifo_flush(1b), mode(2b), en(1b)
    uint32_t cfg_bits_pos   : 5;

    // APC channel Interrupt-related bits position @ APC_INTR_TX/RX_MSK/CLR/IRSR/ISR
    // INTR bits includes 5 bits: fifo_emp, fifo_ful, fifo_unflow, fifo_ovflow, dma_req
    uint32_t intr_bits_pos  : 5;

    uint32_t dma_hsid       : 4; // Common DMA request/handshaking ID of APC channel
    uint32_t dma_hssel      : 2; // Common DMA request/handshaking group of APC channel
    uint32_t gpdma_hsid     : 4; // GPDMA2D request/handshaking ID of APC channel
    uint32_t gpdma_hssel    : 2; // GPDMA2D request/handshaking group of APC channel

    uint32_t fifo_depth     : 5; // FIFO depth (in WORD) of APC channel
    uint32_t data_addr; // address of data register to access FIFO

    //TODO: Add more fields for APC channel
} APC_CH_FIXED;


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

#define RT_FLAG_NSYNCA      (0x1 << 0)
#define RT_FLAG_USE_PIO     (0x1 << 1)
#define RT_FLAG_USE_GPDMA   (0x1 << 2) // prerequisite: USE_PIO = 0
    uint8_t rt_flag_lr[2]; //bit[0]=1 means NOT_SYNC_CACHE

    uint8_t busy_lr[2]; // DMA transfer is ongoing (busy) or not?
    uint32_t samps_lr[2]; // count of samples actually transfered

    // DMA Scatter-Gather settings, generally used in multiple channels read operations
    // xxx_lr[0] for Left channel or whole dual_channel, xxx_lr[1] for Right channel
    uint32_t scat_gath_lr[2]; // including SG Interval & Count
    uint8_t sg_bytes_lr[2];  // bytes of 'desired' (NOT original) sample data (e.g. 32bit, 24bit(high)=>16bit)
    uint8_t sg_offset_lr[2]; // buffer offset of sample data

    //APC_DCH_CFG_REG * cfg_reg_p; // dual_channel configuration register
    volatile uint32_t *cfg_reg_p; // dual_channel configuration register pointer
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
//#define APC_RX_INT_STATUS()                     (CSK_APC->REG_APC_INTR_RX_ISR.all)
//#define APC_TX_INT_STATUS()                     (CSK_APC->REG_APC_INTR_TX_ISR.all)
//#define APC_RX_INT_RAW_STATUS()                 (CSK_APC->REG_APC_INTR_RX_IRSR.all)
//#define APC_TX_INT_RAW_STATUS()                 (CSK_APC->REG_APC_INTR_TX_IRSR.all)
//#define APC_CH_INT_STATUS(rx_sta, tx_sta, ch)   ((ch >= APC_CH_IN_COUNT ? \
//                                                 tx_sta >> ((ch - APC_CH_IN_COUNT) * APC_FIFO_INTR_CNT) : \
//                                                 rx_sta >> (ch * APC_FIFO_INTR_CNT)) & APC_FIFO_INTR_MASK)
#define IS_SET_READY_TO_XFER(ch_sta)            (ch_sta & APC_INTR_READY_TO_XFER)
#define IS_SET_RX_OVERRUN(ch_sta)               (ch_sta & APC_INTR_FIFO_OVERRUN)
#define IS_SET_TX_UNDERRUN(ch_sta)              (ch_sta & APC_INTR_FIFO_UNDERRUN)
#define IS_SET_RX_FULL(ch_sta)                  (ch_sta & APC_INTR_FIFO_FULL)
#define IS_SET_TX_EMPTY(ch_sta)                 (ch_sta & APC_INTR_FIFO_EMPTY)

#define IS_SET_TX_FIFO_VLD(tx_sta, dch)         (tx_sta & APC_INTR_TX_FIFO_VLD(dch))
//#define IS_SET_I2S0_ERR(tx_sta)                  (tx_sta & APC_INTR_I2S0_ERR)
//#define IS_SET_I2S1_ERR(tx_sta)                  (tx_sta & APC_INTR_I2S1_ERR)
#define IS_SET_I2S_INTR(tx_sta)                 (tx_sta & (APC_INTR_I2S_TIMEOUT | APC_INTR_I2S_ERR))
#define IS_SET_I2S_TIMEOUT(tx_sta)              (tx_sta & APC_INTR_I2S_TIMEOUT)
#define IS_SET_I2S_ERR(tx_sta)                  (tx_sta & APC_INTR_I2S_ERR)
#define IS_SET_I2S_ERR_LRCK(tx_sta)             (tx_sta & APC_INTR_I2S_ERR_LRCK)

#endif /* __APC_INNER_VENUSA_H */
